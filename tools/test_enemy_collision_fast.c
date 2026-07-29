#include "code.h"
#include "constants.h"
#include "core_fast_paths.h"
#include "cpu.h"
#include "enemy_collision_captured_states.h"

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

static unsigned direct_comparisons;
static unsigned fallback_checks;
static unsigned captured_comparisons;

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

static void assert_machine_equal(
    const MachineState *expected,
    const MachineState *actual
) {
    assert(memcmp(expected->ram, actual->ram, RAM_SIZE) == 0);
    assert(expected->a == actual->a);
    assert(expected->x == actual->x);
    assert(expected->y == actual->y);
    assert(expected->sp == actual->sp);
    assert(expected->carry == actual->carry);
    assert(expected->nz == actual->nz);
}

static void fill_pattern(MachineState *state, uint32_t seed) {
    uint32_t value = seed | 1u;
    uint16_t address;

    for (address = 0; address < RAM_SIZE; ++address) {
        value = value * UINT32_C(1664525) + UINT32_C(1013904223);
        state->ram[address] = (uint8_t)(value >> 24);
    }
    state->a = (uint8_t)(seed >> 24);
    state->x = (uint8_t)(seed >> 16);
    state->y = (uint8_t)(seed >> 8);
    state->sp = (uint8_t)seed;
    state->carry = (seed & 1u) != 0u;
    state->nz = (uint8_t)(seed ^ (seed >> 8));
}

static void set_box(
    MachineState *state,
    uint8_t slot,
    uint8_t left,
    uint8_t top,
    uint8_t right,
    uint8_t bottom
) {
    const uint8_t offset = (uint8_t)(slot * 4u + 4u);

    state->ram[BoundingBox_UL_Corner + offset] = left;
    state->ram[BoundingBox_UL_Corner + offset + 1u] = top;
    state->ram[BoundingBox_LR_Corner + offset] = right;
    state->ram[BoundingBox_LR_Corner + offset + 1u] = bottom;
}

static void prepare_canonical(
    MachineState *state,
    uint8_t slot,
    uint8_t active_mask,
    bool vertical_separation,
    uint32_t seed
) {
    uint8_t candidate;

    fill_pattern(state, seed);
    state->x = slot;
    state->ram[ObjectOffset] = slot;
    state->ram[FrameCounter] |= 1u;
    state->ram[AreaType] = 1u;
    state->ram[Enemy_ID + slot] = Goomba;
    state->ram[Enemy_Flag + slot] = 1u;
    state->ram[EnemyOffscrBitsMasked + slot] = 0u;
    state->ram[Enemy_OffscreenBits] &= 0x0eu;
    set_box(state, slot, 0x70u, 0x70u, 0x7fu, 0x7fu);

    for (candidate = 0; candidate < slot; ++candidate) {
        const bool active = (active_mask & (1u << candidate)) != 0u;

        state->ram[Enemy_Flag + candidate] = active ? 1u : 0u;
        state->ram[Enemy_ID + candidate] = Goomba;
        state->ram[EnemyOffscrBitsMasked + candidate] = 0u;
        if (vertical_separation) {
            set_box(
                state,
                candidate,
                0x74u,
                (uint8_t)(0x20u + candidate * 8u),
                0x78u,
                (uint8_t)(0x27u + candidate * 8u)
            );
        } else {
            set_box(
                state,
                candidate,
                (uint8_t)(0x10u + candidate * 8u),
                0x74u,
                (uint8_t)(0x17u + candidate * 8u),
                0x78u
            );
        }
    }
}

static void compare_direct(const MachineState *entry) {
    MachineState expected;
    MachineState actual;

    load_machine(entry);
    EnemiesCollision();
    save_machine(&expected);

    load_machine(entry);
    assert(smb_core_fast_enemies_collision());
    save_machine(&actual);
    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static void assert_fallback_unchanged(const MachineState *entry) {
    MachineState actual;

    load_machine(entry);
    assert(!smb_core_fast_enemies_collision());
    save_machine(&actual);
    assert_machine_equal(entry, &actual);
    ++fallback_checks;
}

static void compare_dispatch(const MachineState *entry) {
    MachineState expected;
    MachineState actual;

    load_machine(entry);
    if (!smb_core_fast_enemies_collision()) {
        save_machine(&actual);
        assert_machine_equal(entry, &actual);
        ++fallback_checks;
        return;
    }
    save_machine(&actual);

    load_machine(entry);
    EnemiesCollision();
    save_machine(&expected);
    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static void compare_exhaustive_canonical_cases(void) {
    static const uint8_t stack_values[] = { 0x00u, 0x01u, 0xfeu, 0xffu };
    static const uint8_t collision_values[] = { 0x00u, 0x55u, 0xaau, 0xffu };
    MachineState entry;
    uint8_t slot;
    uint8_t active_mask;
    uint8_t vertical;
    size_t stack_index;
    size_t collision_index;

    for (slot = 1u; slot < 6u; ++slot) {
        for (active_mask = 0u; active_mask < (1u << slot); ++active_mask) {
            for (vertical = 0u; vertical < 2u; ++vertical) {
                for (stack_index = 0;
                     stack_index < sizeof(stack_values);
                     ++stack_index) {
                    for (collision_index = 0;
                         collision_index < sizeof(collision_values);
                         ++collision_index) {
                        uint8_t candidate;

                        prepare_canonical(
                            &entry,
                            slot,
                            active_mask,
                            vertical != 0u,
                            UINT32_C(0x9e3779b9) ^
                                ((uint32_t)slot << 24) ^
                                ((uint32_t)active_mask << 12) ^
                                ((uint32_t)vertical << 8) ^
                                (uint32_t)stack_index * 17u ^
                                (uint32_t)collision_index
                        );
                        entry.sp = stack_values[stack_index];
                        for (candidate = 0; candidate < slot; ++candidate) {
                            entry.ram[Enemy_CollisionBits + candidate] =
                                collision_values[collision_index];
                        }
                        compare_direct(&entry);
                    }
                }
            }
        }
    }
}

static void compare_source_ignored_lower_ids(void) {
    static const uint8_t ignored_ids[] = {
        PiranhaPlant,
        Lakitu,
        BowserFlame,
        0xffu,
    };
    MachineState entry;
    size_t id_index;

    for (id_index = 0;
         id_index < sizeof(ignored_ids) / sizeof(ignored_ids[0]);
         ++id_index) {
        prepare_canonical(&entry, 3u, 0x07u, false, (uint32_t)id_index + 7u);
        entry.ram[Enemy_ID] = ignored_ids[id_index];
        compare_direct(&entry);
    }
}

static void compare_boundary_separations(void) {
    MachineState entry;

    prepare_canonical(&entry, 1u, 1u, false, 0x1001u);
    set_box(&entry, 1u, 0x20u, 0x20u, 0x2fu, 0x2fu);
    set_box(&entry, 0u, 0x30u, 0x20u, 0x3fu, 0x2fu);
    compare_direct(&entry);

    prepare_canonical(&entry, 1u, 1u, true, 0x1002u);
    set_box(&entry, 1u, 0x20u, 0x20u, 0x2fu, 0x2fu);
    set_box(&entry, 0u, 0x20u, 0x30u, 0x2fu, 0x3fu);
    compare_direct(&entry);
}

static void compare_cheap_source_exits(void) {
    static const uint8_t even_frames[] = { 0x00u, 0x02u, 0x80u, 0xfeu };
    static const uint8_t excluded_ids[] = {
        PiranhaPlant,
        Lakitu,
        BowserFlame,
        0xffu,
    };
    MachineState entry;
    size_t index;

    for (index = 0; index < sizeof(even_frames); ++index) {
        prepare_canonical(&entry, 2u, 0x03u, false, 0x1800u + index);
        entry.ram[FrameCounter] = even_frames[index];
        compare_direct(&entry);
    }

    prepare_canonical(&entry, 2u, 0x03u, false, 0x1810u);
    entry.ram[AreaType] = 0u;
    compare_direct(&entry);

    for (index = 0;
         index < sizeof(excluded_ids) / sizeof(excluded_ids[0]);
         ++index) {
        prepare_canonical(&entry, 2u, 0x03u, false, 0x1820u + index);
        entry.ram[Enemy_ID + 2u] = excluded_ids[index];
        compare_direct(&entry);
    }

    /* The source CPY determines carry before the shared offscreen exit. */
    prepare_canonical(&entry, 2u, 0x03u, false, 0x1830u);
    entry.ram[Enemy_ID + 2u] = Goomba;
    entry.ram[EnemyOffscrBitsMasked + 2u] = 1u;
    compare_direct(&entry);

    prepare_canonical(&entry, 2u, 0x03u, false, 0x1831u);
    entry.ram[Enemy_ID + 2u] = 0x0eu;
    entry.ram[EnemyOffscrBitsMasked + 2u] = 1u;
    compare_direct(&entry);

    /* DEX-negative exits use ObjectOffset for the final X/Y registers. */
    prepare_canonical(&entry, 1u, 1u, false, 0x1840u);
    entry.x = 0u;
    entry.ram[ObjectOffset] = 4u;
    compare_direct(&entry);

    prepare_canonical(&entry, 1u, 1u, false, 0x1841u);
    entry.x = 0x81u;
    entry.ram[ObjectOffset] = 4u;
    entry.ram[Enemy_ID + 0x81u] = Goomba;
    entry.ram[EnemyOffscrBitsMasked + 0x81u] = 0u;
    compare_direct(&entry);
}

static void compare_full_index_alias_space(void) {
    MachineState entry;
    unsigned slot;
    unsigned object_offset;

    /*
     * Enemy_ID is ZP,X while EnemyOffscrBitsMasked is ABS,X. Exercise every
     * X/ObjectOffset combination so an addressing-mode wrap cannot make the
     * helper accept a state that the generated routine handles differently.
     */
    for (slot = 0u; slot <= UINT8_MAX; ++slot) {
        for (object_offset = 0u;
             object_offset <= UINT8_MAX;
             ++object_offset) {
            fill_pattern(
                &entry,
                UINT32_C(0x51ed270b) ^
                    (uint32_t)(slot * 257u + object_offset)
            );
            entry.x = (uint8_t)slot;
            entry.ram[ObjectOffset] = (uint8_t)object_offset;
            entry.ram[FrameCounter] |= 1u;
            entry.ram[AreaType] = 1u;
            entry.ram[(uint8_t)(Enemy_ID + slot)] = Goomba;
            entry.ram[EnemyOffscrBitsMasked + slot] = 0u;
            compare_dispatch(&entry);
        }
    }
}

static void verify_fallbacks_do_not_mutate(void) {
    MachineState entry;

    prepare_canonical(&entry, 2u, 0x03u, false, 0x2002u);
    entry.ram[ObjectOffset] = 1u;
    assert_fallback_unchanged(&entry);

    prepare_canonical(&entry, 5u, 0x1fu, false, 0x2004u);
    entry.x = 6u;
    entry.ram[ObjectOffset] = 6u;
    entry.ram[Enemy_ID + 6u] = Goomba;
    entry.ram[EnemyOffscrBitsMasked + 6u] = 0u;
    assert_fallback_unchanged(&entry);

    prepare_canonical(&entry, 2u, 0x03u, false, 0x2007u);
    entry.ram[EnemyOffscrBitsMasked] = 1u;
    assert_fallback_unchanged(&entry);

    /* Equal edges count as collision in the translated inclusive boxes. */
    prepare_canonical(&entry, 1u, 1u, false, 0x2008u);
    set_box(&entry, 1u, 0x20u, 0x20u, 0x30u, 0x30u);
    set_box(&entry, 0u, 0x30u, 0x20u, 0x40u, 0x30u);
    assert_fallback_unchanged(&entry);

    prepare_canonical(&entry, 1u, 1u, false, 0x2009u);
    set_box(&entry, 0u, 0xf0u, 0x20u, 0x10u, 0x30u);
    assert_fallback_unchanged(&entry);
}

static void load_captured_state(
    MachineState *state,
    const EnemyCollisionCapturedState *capture
) {
    uint16_t source = 0u;
    uint16_t destination = 0u;

    memset(state, 0, sizeof(*state));
    while (source < capture->ram_rle_size) {
        const uint8_t count = capture->ram_rle[source++];
        const uint8_t value = capture->ram_rle[source++];

        assert(count != 0u);
        assert((uint32_t)destination + count <= RAM_SIZE);
        memset(&state->ram[destination], value, count);
        destination = (uint16_t)(destination + count);
    }
    assert(source == capture->ram_rle_size);
    assert(destination == RAM_SIZE);
    state->a = capture->a;
    state->x = capture->x;
    state->y = capture->y;
    state->sp = capture->sp;
    state->carry = capture->carry != 0u;
    state->nz = capture->nz;
}

static void compare_captured_windows(void) {
    static const EnemyCollisionCapturedState *const captures[] = {
        &enemy_collision_w1_capture,
        &enemy_collision_w4_capture,
    };
    MachineState entry;
    size_t index;

    for (index = 0; index < sizeof(captures) / sizeof(captures[0]); ++index) {
        load_captured_state(&entry, captures[index]);
        compare_direct(&entry);
        ++captured_comparisons;
    }
}

int main(void) {
    compare_exhaustive_canonical_cases();
    compare_source_ignored_lower_ids();
    compare_boundary_separations();
    compare_cheap_source_exits();
    compare_full_index_alias_space();
    verify_fallbacks_do_not_mutate();
    compare_captured_windows();

    printf(
        "enemy collision fast-path tests passed: direct=%u captured=%u "
        "fallback=%u\n",
        direct_comparisons,
        captured_comparisons,
        fallback_checks
    );
    return 0;
}
