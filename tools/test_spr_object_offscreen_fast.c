#include "constants.h"
#include "core_fast_paths.h"
#include "cpu.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t ram[RAM_SIZE];
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
} MachineState;

typedef struct {
    uint8_t mask;
    uint16_t count;
} ObservedMaskCount;

static const ObservedMaskCount world_1_2_masks[] = {
    { 0x00u, 361u }, { 0x01u, 15u }, { 0x03u, 13u },
    { 0x07u, 13u }, { 0x08u, 9u }, { 0x0cu, 9u },
    { 0x0eu, 6u }, { 0x0fu, 137u },
};

static const ObservedMaskCount world_4_1_masks[] = {
    { 0x00u, 299u }, { 0x01u, 3u }, { 0x03u, 3u },
    { 0x07u, 4u }, { 0x08u, 6u }, { 0x0cu, 6u },
    { 0x0eu, 5u }, { 0x0fu, 53u },
};

static uint64_t direct_cases;
static uint64_t known_safe_cases;
static uint64_t fallback_cases;
static uint64_t observed_cases;
static bool observed_masks[UINT8_MAX + 1u];
static bool observed_actions[4];

static void save_machine(MachineState *state) {
    memcpy(state->ram, ram, RAM_SIZE);
    state->a = a;
    state->x = x;
    state->y = y;
    state->sp = sp;
    state->carry = carry_flag;
    state->nz = nz_value;
}

static void load_machine(const MachineState *state) {
    memcpy(ram, state->ram, RAM_SIZE);
    a = state->a;
    x = state->x;
    y = state->y;
    sp = state->sp;
    carry_flag = state->carry;
    nz_value = state->nz;
}

static void prepare_machine(
    MachineState *state,
    uint8_t slot,
    uint8_t mask,
    uint8_t sprite_offset,
    uint8_t entry_y,
    uint8_t entry_sp,
    uint32_t tag
) {
    uint16_t address;

    for (address = 0u; address < RAM_SIZE; ++address) {
        state->ram[address] = (uint8_t)(
            address * 29u + (address >> 8) * 7u + tag * 17u
        );
    }
    state->a = (uint8_t)(0xa5u ^ tag);
    state->x = (uint8_t)(0x5au + tag);
    state->y = entry_y;
    state->sp = entry_sp;
    state->carry = (tag & 1u) != 0u;
    state->nz = (uint8_t)(0x3cu - tag);
    state->ram[ObjectOffset] = slot;
    state->ram[Enemy_OffscreenBits] = mask;
    if (slot < 6u) {
        state->ram[Enemy_SprDataOffset + slot] = sprite_offset;
    }
}

static void reference_lda(MachineState *state, uint8_t value) {
    state->a = value;
    state->nz = value;
}

static void reference_ldx(MachineState *state, uint8_t value) {
    state->x = value;
    state->nz = value;
}

static void reference_lsr(MachineState *state) {
    state->carry = (state->a & 1u) != 0u;
    state->a >>= 1;
    state->nz = state->a;
}

static void reference_pha(MachineState *state) {
    state->ram[0x100u + state->sp] = state->a;
    state->sp = (uint8_t)(state->sp - 1u);
}

static void reference_pla(MachineState *state) {
    state->sp = (uint8_t)(state->sp + 1u);
    reference_lda(state, state->ram[0x100u + state->sp]);
}

static void reference_adc(MachineState *state, uint8_t value) {
    const uint16_t sum = (uint16_t)state->a + value +
        (state->carry ? 1u : 0u);

    state->a = (uint8_t)sum;
    state->carry = sum > UINT8_MAX;
    state->nz = state->a;
}

static void reference_tay(MachineState *state) {
    state->y = state->a;
    state->nz = state->y;
}

/* Literal instruction-shaped oracle for MoveESprColOffscreen and its leaf. */
static void reference_move_enemy_column(MachineState *state) {
    state->carry = false;
    reference_adc(state, state->ram[Enemy_SprDataOffset + state->x]);
    reference_tay(state);
    reference_lda(state, 0xf8u);
    state->ram[Sprite_Y_Position + state->y] = state->a;
    state->ram[Sprite_Y_Position + state->y + 8u] = state->a;
    state->ram[Sprite_Y_Position + state->y + 16u] = state->a;
}

/* Literal instruction-shaped oracle for the accepted SprObjectOffscrChk path. */
static void reference_spr_object_offscreen(MachineState *state) {
    reference_ldx(state, state->ram[ObjectOffset]);
    reference_lda(state, state->ram[Enemy_OffscreenBits]);
    reference_lsr(state);
    reference_lsr(state);
    reference_lsr(state);
    reference_pha(state);
    if (state->carry) {
        reference_lda(state, 0x04u);
        reference_move_enemy_column(state);
    }
    reference_pla(state);
    reference_lsr(state);
    reference_pha(state);
    if (state->carry) {
        reference_lda(state, 0x00u);
        reference_move_enemy_column(state);
    }
    reference_pla(state);
    reference_lsr(state);
    reference_lsr(state);
    reference_pha(state);
    assert(!state->carry);
    reference_pla(state);
    reference_lsr(state);
    reference_pha(state);
    assert(!state->carry);
    reference_pla(state);
    reference_lsr(state);
    assert(!state->carry);
}

static int machine_equal(
    const MachineState *expected,
    const MachineState *actual,
    const char *group,
    uint64_t ordinal
) {
    uint16_t address;

    for (address = 0u; address < RAM_SIZE; ++address) {
        if (expected->ram[address] != actual->ram[address]) {
            fprintf(
                stderr,
                "%s case %llu RAM $%04x: expected $%02x, got $%02x\n",
                group,
                (unsigned long long)ordinal,
                address,
                expected->ram[address],
                actual->ram[address]
            );
            return 0;
        }
    }
    if (
        expected->a != actual->a || expected->x != actual->x ||
        expected->y != actual->y || expected->sp != actual->sp ||
        expected->carry != actual->carry || expected->nz != actual->nz
    ) {
        fprintf(
            stderr,
            "%s case %llu CPU mismatch: "
            "expected %02x,%02x,%02x,%02x,%u,%02x; "
            "got %02x,%02x,%02x,%02x,%u,%02x\n",
            group,
            (unsigned long long)ordinal,
            expected->a,
            expected->x,
            expected->y,
            expected->sp,
            expected->carry ? 1u : 0u,
            expected->nz,
            actual->a,
            actual->x,
            actual->y,
            actual->sp,
            actual->carry ? 1u : 0u,
            actual->nz
        );
        return 0;
    }
    return 1;
}

static int compare_direct_case(
    uint8_t slot,
    uint8_t mask,
    uint8_t sprite_offset,
    uint8_t entry_y,
    uint8_t entry_sp,
    uint32_t tag,
    const char *group
) {
    MachineState entry;
    MachineState expected;
    MachineState actual;
    const uint64_t ordinal = direct_cases;

    prepare_machine(
        &entry,
        slot,
        mask,
        sprite_offset,
        entry_y,
        entry_sp,
        tag
    );
    expected = entry;
    reference_spr_object_offscreen(&expected);
    load_machine(&entry);
    if (!smb_core_fast_spr_object_offscr_chk()) {
        fprintf(stderr, "%s case %llu unexpectedly fell back\n",
                group, (unsigned long long)ordinal);
        return 1;
    }
    save_machine(&actual);
    if (!machine_equal(&expected, &actual, group, ordinal)) {
        return 1;
    }

    load_machine(&entry);
    if (!smb_core_fast_spr_object_offscr_known_safe(slot, sprite_offset)) {
        fprintf(stderr, "%s known-safe case %llu unexpectedly fell back\n",
                group, (unsigned long long)ordinal);
        return 1;
    }
    save_machine(&actual);
    if (!machine_equal(&expected, &actual, group, ordinal)) {
        return 1;
    }
    ++known_safe_cases;
    ++direct_cases;
    return 0;
}

static int compare_known_safe_high_mask_untouched(
    uint8_t slot,
    uint8_t mask,
    uint8_t sprite_offset,
    uint32_t tag
) {
    MachineState entry;
    MachineState actual;

    prepare_machine(
        &entry,
        slot,
        mask,
        sprite_offset,
        (uint8_t)(tag * 11u),
        (uint8_t)(tag * 19u),
        tag
    );
    load_machine(&entry);
    if (smb_core_fast_spr_object_offscr_known_safe(slot, sprite_offset)) {
        fprintf(stderr, "high-mask known-safe case unexpectedly completed\n");
        return 1;
    }
    save_machine(&actual);
    return machine_equal(
        &entry,
        &actual,
        "high-row-mask-known-safe",
        mask
    ) ? 0 : 1;
}

static int compare_fallback_case(
    uint8_t slot,
    uint8_t mask,
    uint8_t sprite_offset,
    uint32_t tag,
    const char *group
) {
    MachineState entry;
    MachineState actual;
    const uint64_t ordinal = fallback_cases;

    prepare_machine(
        &entry,
        slot,
        mask,
        sprite_offset,
        (uint8_t)(tag * 11u),
        (uint8_t)(tag * 19u),
        tag
    );
    load_machine(&entry);
    if (smb_core_fast_spr_object_offscr_chk()) {
        fprintf(stderr, "%s case %llu unexpectedly used direct path\n",
                group, (unsigned long long)ordinal);
        return 1;
    }
    save_machine(&actual);
    if (!machine_equal(&entry, &actual, group, ordinal)) {
        return 1;
    }
    ++fallback_cases;
    return 0;
}

static int run_factorized_exhaustive(void) {
    uint16_t mask;
    uint16_t sprite_offset;
    uint16_t entry_y;
    uint16_t value;

    /* Exhaust the interacting mask, OAM-offset and incoming-Y dimensions. */
    for (mask = 0u; mask < 0x20u; ++mask) {
        for (sprite_offset = 0u; sprite_offset <= 232u;
             sprite_offset += 4u) {
            for (entry_y = 0u; entry_y <= UINT8_MAX; ++entry_y) {
                if (compare_direct_case(
                        (uint8_t)((mask + sprite_offset + entry_y) % 6u),
                        (uint8_t)mask,
                        (uint8_t)sprite_offset,
                        (uint8_t)entry_y,
                        0x9du,
                        (uint32_t)(mask * 257u + sprite_offset + entry_y),
                        "mask-offset-y") != 0) {
                    return 1;
                }
            }
        }
    }

    /* Exhaust stack aliasing and the final reused stack-page byte. */
    for (mask = 0u; mask < 0x20u; ++mask) {
        for (value = 0u; value <= UINT8_MAX; ++value) {
            if (compare_direct_case(
                    (uint8_t)(value % 6u),
                    (uint8_t)mask,
                    (uint8_t)((value % 59u) * 4u),
                    (uint8_t)(value ^ mask),
                    (uint8_t)value,
                    0x100000u + mask * 256u + value,
                    "mask-stack") != 0) {
                return 1;
            }
        }
    }

    /* Cross every accepted slot with every action and safe OAM boundary. */
    for (mask = 0u; mask < 0x20u; ++mask) {
        uint8_t slot;

        for (slot = 0u; slot < 6u; ++slot) {
            for (sprite_offset = 0u; sprite_offset <= 232u;
                 sprite_offset += 4u) {
                if (compare_direct_case(
                        slot,
                        (uint8_t)mask,
                        (uint8_t)sprite_offset,
                        (uint8_t)(slot * 37u),
                        (uint8_t)(mask * 7u),
                        0x200000u + mask * 0x1000u +
                            slot * 0x100u + sprite_offset,
                        "mask-slot-offset") != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int run_fallback_untouched(void) {
    uint16_t mask;
    uint16_t slot;
    uint16_t sprite_offset;

    for (mask = 0u; mask <= UINT8_MAX; ++mask) {
        if ((mask & 0xe0u) == 0u) {
            continue;
        }
        if (compare_fallback_case(
                (uint8_t)(mask % 6u),
                (uint8_t)mask,
                (uint8_t)((mask % 59u) * 4u),
                0x300000u + mask,
                "high-row-mask") != 0) {
            return 1;
        }
        if (compare_known_safe_high_mask_untouched(
                (uint8_t)(mask % 6u),
                (uint8_t)mask,
                (uint8_t)((mask % 59u) * 4u),
                0x330000u + mask) != 0) {
            return 1;
        }
    }
    for (slot = 6u; slot <= UINT8_MAX; ++slot) {
        if (compare_fallback_case(
                (uint8_t)slot,
                (uint8_t)(slot & 0x1fu),
                0u,
                0x310000u + slot,
                "unsupported-slot") != 0) {
            return 1;
        }
    }
    for (sprite_offset = 0u; sprite_offset <= UINT8_MAX; ++sprite_offset) {
        if (sprite_offset <= 232u && (sprite_offset & 3u) == 0u) {
            continue;
        }
        if (compare_fallback_case(
                (uint8_t)(sprite_offset % 6u),
                (uint8_t)(sprite_offset & 0x1fu),
                (uint8_t)sprite_offset,
                0x320000u + sprite_offset,
                "unsafe-oam-offset") != 0) {
            return 1;
        }
    }
    return 0;
}

static int replay_observed_counts(
    const ObservedMaskCount *counts,
    size_t count,
    uint32_t tag_base
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        uint16_t occurrence;

        observed_masks[counts[index].mask] = true;
        observed_actions[(counts[index].mask >> 2) & 3u] = true;
        for (occurrence = 0u; occurrence < counts[index].count; ++occurrence) {
            const uint32_t tag = tag_base + (uint32_t)observed_cases;

            if (compare_direct_case(
                    (uint8_t)(observed_cases % 6u),
                    counts[index].mask,
                    (uint8_t)((observed_cases % 59u) * 4u),
                    (uint8_t)observed_cases,
                    (uint8_t)(observed_cases * 3u),
                    tag,
                    "observed-replay-mask") != 0) {
                return 1;
            }
            ++observed_cases;
        }
    }
    return 0;
}

static int require_observed_coverage(void) {
    static const uint8_t masks[] = {
        0x00u, 0x01u, 0x03u, 0x07u,
        0x08u, 0x0cu, 0x0eu, 0x0fu,
    };
    size_t index;

    if (observed_cases != 942u) {
        fprintf(stderr, "expected 942 observed calls, got %llu\n",
                (unsigned long long)observed_cases);
        return 1;
    }
    for (index = 0u; index < sizeof(masks); ++index) {
        if (!observed_masks[masks[index]]) {
            fprintf(stderr, "observed mask $%02x was not covered\n", masks[index]);
            return 1;
        }
    }
    for (index = 0u; index < 4u; ++index) {
        if (!observed_actions[index]) {
            fprintf(stderr, "column action %zu was not covered\n", index);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    if (
        run_factorized_exhaustive() != 0 ||
        run_fallback_untouched() != 0 ||
        replay_observed_counts(
            world_1_2_masks,
            sizeof(world_1_2_masks) / sizeof(world_1_2_masks[0]),
            0x400000u
        ) != 0 ||
        replay_observed_counts(
            world_4_1_masks,
            sizeof(world_4_1_masks) / sizeof(world_4_1_masks[0]),
            0x500000u
        ) != 0 ||
        require_observed_coverage() != 0
    ) {
        return 1;
    }
    printf(
        "SprObjectOffscrChk fast-path regression: PASS "
        "(%llu literal direct, %llu known-safe direct, "
        "%llu untouched fallback, "
        "%llu observed calls; 8 masks, 4 actions)\n",
        (unsigned long long)direct_cases,
        (unsigned long long)known_safe_cases,
        (unsigned long long)fallback_cases,
        (unsigned long long)observed_cases
    );
    return 0;
}
