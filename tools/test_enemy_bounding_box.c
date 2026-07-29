#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "instructions.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct machine_state {
    uint8_t ram[RAM_SIZE];
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
};

struct observed_mask_count {
    uint8_t mask;
    uint16_t count;
};

static const struct observed_mask_count world_1_2_masks[] = {
    { 0x00u, 361u }, { 0x01u, 15u }, { 0x03u, 13u },
    { 0x07u, 13u }, { 0x08u, 9u }, { 0x0cu, 9u },
    { 0x0eu, 6u }, { 0x0fu, 137u },
};

static const struct observed_mask_count world_4_1_masks[] = {
    { 0x00u, 299u }, { 0x01u, 3u }, { 0x03u, 3u },
    { 0x07u, 4u }, { 0x08u, 6u }, { 0x0cu, 6u },
    { 0x0eu, 5u }, { 0x0fu, 53u },
};

static uint64_t case_count;
static uint64_t normal_case_count;
static uint64_t fallback_case_count;
static uint64_t geometry_case_count;
static uint64_t offscreen_case_count;
static uint64_t observed_case_count;

static uint8_t pattern_byte(uint32_t seed, uint16_t address) {
    uint32_t value = seed ^ (uint32_t)address * 0x45d9f3bu;

    value ^= value >> 16;
    value *= 0x45d9f3bu;
    value ^= value >> 16;
    return (uint8_t)value;
}

static void save_machine(struct machine_state *state) {
    memcpy(state->ram, ram, sizeof(state->ram));
    state->a = a;
    state->x = x;
    state->y = y;
    state->sp = sp;
    state->carry = carry_flag;
    state->nz = nz_value;
}

static void load_machine(const struct machine_state *state) {
    memcpy(ram, state->ram, sizeof(state->ram));
    a = state->a;
    x = state->x;
    y = state->y;
    sp = state->sp;
    carry_flag = state->carry;
    nz_value = state->nz;
}

static void prepare_machine(
    struct machine_state *state,
    uint32_t tag,
    uint8_t object_offset,
    uint8_t enemy_x,
    uint8_t screen_x,
    uint8_t enemy_page,
    uint8_t screen_page,
    uint8_t offscreen_bits,
    uint8_t relative_x,
    uint8_t relative_y,
    uint8_t control,
    uint8_t entry_sp
) {
    uint16_t address;
    const uint8_t object_index = (uint8_t)(object_offset + 1u);

    for (address = 0u; address < RAM_SIZE; ++address) {
        state->ram[address] = pattern_byte(tag, address);
    }
    state->a = (uint8_t)(tag ^ 0xa5u);
    state->x = object_offset;
    state->y = (uint8_t)(tag >> 8);
    state->sp = entry_sp;
    state->carry = (tag & 1u) != 0u;
    state->nz = (uint8_t)(tag >> 16);

    state->ram[ObjectOffset] = object_offset;
    state->ram[(uint8_t)(Enemy_X_Position + object_offset)] = enemy_x;
    state->ram[(uint8_t)(Enemy_PageLoc + object_offset)] = enemy_page;
    state->ram[ScreenLeft_X_Pos] = screen_x;
    state->ram[ScreenLeft_PageLoc] = screen_page;
    state->ram[Enemy_OffscreenBits] = offscreen_bits;
    state->ram[SprObject_Rel_XPos + 1u] = relative_x;
    state->ram[SprObject_Rel_YPos + 1u] = relative_y;
    state->ram[SprObj_BoundBoxCtrl + object_index] = control;
}

/* Literal source-shaped call chain retained by the generated generic helper. */
static void reference_get_enemy_bound_box(void) {
    ldy_imm(0x48u);
    ram[0x00u] = y;
    ldy_imm(0x44u);
    GetMaskedOffScrBits();
}

static int compare_machine(
    const struct machine_state *expected,
    const struct machine_state *actual,
    const char *suite,
    uint64_t ordinal
) {
    uint16_t address;

    for (address = 0u; address < RAM_SIZE; ++address) {
        if (expected->ram[address] != actual->ram[address]) {
            fprintf(
                stderr,
                "%s case=%" PRIu64 " RAM[$%04x]: expected $%02x, got $%02x\n",
                suite,
                ordinal,
                address,
                expected->ram[address],
                actual->ram[address]
            );
            return 1;
        }
    }
    if (
        expected->a != actual->a || expected->x != actual->x ||
        expected->y != actual->y || expected->sp != actual->sp ||
        expected->carry != actual->carry || expected->nz != actual->nz
    ) {
        fprintf(
            stderr,
            "%s case=%" PRIu64 " CPU: expected "
            "%02x,%02x,%02x,%02x,%u,%02x; got "
            "%02x,%02x,%02x,%02x,%u,%02x\n",
            suite,
            ordinal,
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
        return 1;
    }
    return 0;
}

static int run_case(
    const char *suite,
    uint32_t tag,
    uint8_t object_offset,
    uint8_t enemy_x,
    uint8_t screen_x,
    uint8_t enemy_page,
    uint8_t screen_page,
    uint8_t offscreen_bits,
    uint8_t relative_x,
    uint8_t relative_y,
    uint8_t control,
    uint8_t entry_sp
) {
    struct machine_state entry;
    struct machine_state expected;
    struct machine_state actual;
    const uint64_t ordinal = case_count;

    prepare_machine(
        &entry,
        tag,
        object_offset,
        enemy_x,
        screen_x,
        enemy_page,
        screen_page,
        offscreen_bits,
        relative_x,
        relative_y,
        control,
        entry_sp
    );
    load_machine(&entry);
    reference_get_enemy_bound_box();
    save_machine(&expected);

    load_machine(&entry);
    GetEnemyBoundBox();
    save_machine(&actual);
    if (compare_machine(&expected, &actual, suite, ordinal) != 0) {
        return 1;
    }

    if (object_offset <= 5u) {
        ++normal_case_count;
    } else {
        ++fallback_case_count;
    }
    if (expected.ram[EnemyOffscrBitsMasked + object_offset] == 0u) {
        ++geometry_case_count;
    } else {
        ++offscreen_case_count;
    }
    ++case_count;
    return 0;
}

static int run_pixel_pairs(void) {
    uint16_t enemy_x;
    uint16_t screen_x;

    for (enemy_x = 0u; enemy_x <= UINT8_MAX; ++enemy_x) {
        for (screen_x = 0u; screen_x <= UINT8_MAX; ++screen_x) {
            const uint32_t tag = (uint32_t)(enemy_x << 8) | screen_x;

            if (run_case(
                    "pixel-pairs",
                    tag,
                    (uint8_t)(tag % 6u),
                    (uint8_t)enemy_x,
                    (uint8_t)screen_x,
                    0x42u,
                    0x42u,
                    (uint8_t)(enemy_x ^ screen_x),
                    (uint8_t)(enemy_x + screen_x),
                    (uint8_t)(enemy_x - screen_x),
                    (uint8_t)(tag % 12u),
                    (uint8_t)tag
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_page_pairs(void) {
    uint16_t enemy_page;
    uint16_t screen_page;

    for (enemy_page = 0u; enemy_page <= UINT8_MAX; ++enemy_page) {
        for (screen_page = 0u; screen_page <= UINT8_MAX; ++screen_page) {
            const uint32_t pair = (uint32_t)(enemy_page << 8) | screen_page;
            const bool pixel_borrow = (pair & 1u) != 0u;

            if (run_case(
                    "page-pairs",
                    0x10000u + pair,
                    (uint8_t)(pair % 6u),
                    pixel_borrow ? 0x00u : 0xffu,
                    pixel_borrow ? 0xffu : 0x00u,
                    (uint8_t)enemy_page,
                    (uint8_t)screen_page,
                    (uint8_t)(enemy_page + 3u * screen_page),
                    (uint8_t)(enemy_page ^ screen_page),
                    (uint8_t)(enemy_page - screen_page),
                    (uint8_t)(pair % 12u),
                    (uint8_t)(pair >> 3)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_coordinate_pairs(void) {
    uint16_t relative_x;
    uint16_t relative_y;

    for (relative_x = 0u; relative_x <= UINT8_MAX; ++relative_x) {
        for (relative_y = 0u; relative_y <= UINT8_MAX; ++relative_y) {
            const uint32_t pair =
                (uint32_t)(relative_x << 8) | relative_y;

            if (run_case(
                    "coordinate-pairs",
                    0x20000u + pair,
                    (uint8_t)(pair % 6u),
                    0x80u,
                    0x20u,
                    0x33u,
                    0x33u,
                    0x00u,
                    (uint8_t)relative_x,
                    (uint8_t)relative_y,
                    (uint8_t)(pair % 12u),
                    (uint8_t)(relative_x + relative_y)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_stack_control_pairs(void) {
    uint16_t entry_sp;
    uint16_t control;

    for (entry_sp = 0u; entry_sp <= UINT8_MAX; ++entry_sp) {
        for (control = 0u; control <= UINT8_MAX; ++control) {
            const uint32_t pair = (uint32_t)(entry_sp << 8) | control;

            if (run_case(
                    "stack-control-pairs",
                    0x30000u + pair,
                    (uint8_t)(pair % 6u),
                    0x81u,
                    0x80u,
                    0x44u,
                    0x44u,
                    0x00u,
                    (uint8_t)(entry_sp * 3u),
                    (uint8_t)(control * 5u),
                    (uint8_t)control,
                    (uint8_t)entry_sp
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_mask_classes(void) {
    uint16_t mask;
    uint8_t position_class;
    uint8_t slot;

    for (mask = 0u; mask <= UINT8_MAX; ++mask) {
        for (position_class = 0u; position_class < 3u; ++position_class) {
            for (slot = 0u; slot < 6u; ++slot) {
                static const uint8_t enemy_x[] = { 0x20u, 0x80u, 0x81u };
                static const uint8_t screen_x[] = { 0x80u, 0x80u, 0x80u };
                static const uint8_t enemy_page[] = { 0x20u, 0x21u, 0x21u };
                static const uint8_t screen_page[] = { 0x21u, 0x21u, 0x21u };
                const uint32_t tag = 0x40000u + mask * 18u +
                    position_class * 6u + slot;

                if (run_case(
                        "mask-classes",
                        tag,
                        slot,
                        enemy_x[position_class],
                        screen_x[position_class],
                        enemy_page[position_class],
                        screen_page[position_class],
                        (uint8_t)mask,
                        (uint8_t)tag,
                        (uint8_t)(tag >> 8),
                        (uint8_t)(tag % 12u),
                        (uint8_t)(tag >> 4)
                    ) != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int run_fallback_aliases(void) {
    uint16_t object_offset;

    for (object_offset = 6u; object_offset <= UINT8_MAX; ++object_offset) {
        const uint32_t tag = 0x50000u + object_offset;

        if (run_case(
                "fallback-aliases",
                tag,
                (uint8_t)object_offset,
                (uint8_t)(object_offset * 17u),
                (uint8_t)(object_offset * 29u),
                (uint8_t)(object_offset * 37u),
                (uint8_t)(object_offset * 43u),
                (uint8_t)(object_offset * 53u),
                (uint8_t)(object_offset * 61u),
                (uint8_t)(object_offset * 67u),
                (uint8_t)(object_offset * 71u),
                (uint8_t)(object_offset * 73u)
            ) != 0) {
            return 1;
        }
    }
    return 0;
}

static int replay_observed(
    const struct observed_mask_count *counts,
    size_t count,
    uint32_t tag_base
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        uint16_t occurrence;

        for (occurrence = 0u; occurrence < counts[index].count;
             ++occurrence) {
            const uint32_t tag = tag_base + (uint32_t)observed_case_count;
            const bool left_mask = (observed_case_count & 1u) != 0u;

            if (run_case(
                    "observed-mask-replay",
                    tag,
                    (uint8_t)(observed_case_count % 6u),
                    left_mask ? 0x20u : 0x81u,
                    0x80u,
                    left_mask ? 0x20u : 0x21u,
                    0x21u,
                    counts[index].mask,
                    (uint8_t)observed_case_count,
                    (uint8_t)(observed_case_count * 3u),
                    (uint8_t)(observed_case_count % 12u),
                    (uint8_t)(observed_case_count * 5u)
                ) != 0) {
                return 1;
            }
            ++observed_case_count;
        }
    }
    return 0;
}

int main(void) {
    if (
        run_pixel_pairs() != 0 || run_page_pairs() != 0 ||
        run_coordinate_pairs() != 0 || run_stack_control_pairs() != 0 ||
        run_mask_classes() != 0 || run_fallback_aliases() != 0 ||
        replay_observed(
            world_1_2_masks,
            sizeof(world_1_2_masks) / sizeof(world_1_2_masks[0]),
            0x60000u
        ) != 0 ||
        replay_observed(
            world_4_1_masks,
            sizeof(world_4_1_masks) / sizeof(world_4_1_masks[0]),
            0x70000u
        ) != 0
    ) {
        return 1;
    }
    if (observed_case_count != 942u) {
        fprintf(
            stderr,
            "expected 942 observed calls, got %" PRIu64 "\n",
            observed_case_count
        );
        return 1;
    }

    printf(
        "GetEnemyBoundBox differential regression: PASS "
        "(%" PRIu64 " cases: %" PRIu64 " normal, %" PRIu64
        " alias fallback, %" PRIu64 " geometry, %" PRIu64
        " offscreen; 942 observed calls)\n",
        case_count,
        normal_case_count,
        fallback_case_count,
        geometry_case_count,
        offscreen_case_count
    );
    return 0;
}
