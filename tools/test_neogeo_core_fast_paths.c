#include "code.h"
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
    uint8_t enemy_id;
    uint8_t enemy_state;
} EnemyCase;

static const EnemyCase enemy_cases[] = {
    { Goomba, 0u },
    { PiranhaPlant, 0u },
    { Lakitu, 0u },
    { Spiny, 0u },
    { Spiny, 5u },
};

static const EnemyCase move_enemy_cases[] = {
    { Goomba, 0u },
    { Spiny, 0u },
    { Spiny, 5u },
};

static const uint8_t offscreen_cases[] = {
    0x00u, 0x04u, 0x08u, 0x20u,
    0x40u, 0x80u, 0xfcu, 0xffu,
};

static unsigned int direct_comparisons;
static unsigned int move_direct_comparisons;
static unsigned int move_fallback_checks;

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

static void prepare_case(
    uint8_t slot,
    const EnemyCase *enemy,
    uint8_t frame,
    uint8_t direction,
    uint8_t attributes,
    uint8_t offscreen
) {
    memset(ram, 0, RAM_SIZE);
    a = (uint8_t)(0x80u | frame);
    x = slot;
    y = (uint8_t)(0x40u | slot);
    sp = 0xd0u;
    carry_flag = (frame & 1u) != 0u;
    nz_value = (uint8_t)(frame + 1u);

    ram[ObjectOffset] = slot;
    ram[FrameCounter] = frame;
    ram[Enemy_ID + slot] = enemy->enemy_id;
    ram[Enemy_State + slot] = enemy->enemy_state;
    ram[Enemy_MovingDir + slot] = direction;
    ram[Enemy_SprAttrib + slot] = attributes;
    ram[Enemy_Y_Position + slot] = (uint8_t)(0xe8u + frame);
    ram[Enemy_Y_HighPos + slot] = (frame & 4u) != 0u ? 2u : 1u;
    ram[Enemy_Rel_XPos] = (uint8_t)(0xf8u + frame);
    ram[Enemy_SprDataOffset + slot] = (uint8_t)(slot * 24u);
    ram[EnemyIntervalTimer + slot] = (frame & 2u) != 0u ? 5u : 0u;
    ram[FrenzyEnemyTimer] = frame < 8u ? frame : (uint8_t)(frame + 8u);
    ram[EnemyFrameTimer + slot] = (frame & 2u) != 0u ? 1u : 0u;
    ram[PiranhaPlant_Y_Speed + slot] =
        (frame & 1u) != 0u ? 1u : 0xffu;
    ram[Enemy_OffscreenBits] = offscreen;
    ram[Enemy_Flag + slot] = 1u;
}

static void compare_prepared_case(void) {
    MachineState entry;
    MachineState expected;
    MachineState actual;

    save_machine(&entry);
    EnemyGfxHandler();
    save_machine(&expected);

    load_machine(&entry);
    assert(smb_core_fast_enemy_gfx_handler());
    save_machine(&actual);
    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static void compare_direct_and_translated(void) {
    size_t case_index;
    size_t offscreen_index;
    uint8_t slot;
    uint8_t frame;
    uint8_t direction;
    uint8_t attributes;

    for (case_index = 0;
         case_index < sizeof(enemy_cases) / sizeof(enemy_cases[0]);
         ++case_index) {
        for (slot = 0; slot < 6u; ++slot) {
            for (frame = 0; frame < 16u; ++frame) {
                for (direction = 0; direction < 4u; ++direction) {
                    for (attributes = 0; attributes <= 0x20u;
                         attributes = (uint8_t)(attributes + 0x20u)) {
                        for (offscreen_index = 0;
                             offscreen_index <
                                sizeof(offscreen_cases) /
                                    sizeof(offscreen_cases[0]);
                             ++offscreen_index) {
                            prepare_case(
                                slot,
                                &enemy_cases[case_index],
                                frame,
                                direction,
                                attributes,
                                offscreen_cases[offscreen_index]
                            );
                            compare_prepared_case();
                        }
                    }
                }
            }
        }
    }
    assert(direct_comparisons == 30720u);
}

static void compare_every_read_byte(void) {
    size_t case_index;
    uint16_t value;

    for (case_index = 0;
         case_index < sizeof(enemy_cases) / sizeof(enemy_cases[0]);
         ++case_index) {
        const EnemyCase *enemy = &enemy_cases[case_index];

        for (value = 0; value < 256u; ++value) {
            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[FrameCounter] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[Enemy_MovingDir + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[Enemy_SprAttrib + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[Enemy_Y_Position + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[Enemy_Rel_XPos] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[EnemyIntervalTimer + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[FrenzyEnemyTimer] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[EnemyFrameTimer + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[PiranhaPlant_Y_Speed + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[Enemy_OffscreenBits] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0x80u);
            ram[Enemy_Y_HighPos + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0x80u);
            ram[Enemy_Y_HighPos + 2u] = 2u;
            ram[Enemy_Flag + 2u] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            ram[VerticalFlipFlag] = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            a = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            y = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            sp = (uint8_t)value;
            compare_prepared_case();

            prepare_case(2u, enemy, 0u, 1u, 0u, 0u);
            nz_value = (uint8_t)value;
            carry_flag = (value & 1u) != 0u;
            compare_prepared_case();
        }

        for (value = 0; value <= 232u; value += 4u) {
            prepare_case(2u, enemy, 0u, 1u, 0u, 0xffu);
            ram[Enemy_SprDataOffset + 2u] = (uint8_t)value;
            compare_prepared_case();
        }
    }

    /* Cross the two Piranha early-return predicates in both directions. */
    for (value = 0; value < 256u; ++value) {
        prepare_case(2u, &enemy_cases[1], 0u, 1u, 0u, 0u);
        ram[PiranhaPlant_Y_Speed + 2u] = (uint8_t)value;
        ram[EnemyFrameTimer + 2u] = 1u;
        compare_prepared_case();

        prepare_case(2u, &enemy_cases[1], 0u, 1u, 0u, 0u);
        ram[PiranhaPlant_Y_Speed + 2u] = 1u;
        ram[EnemyFrameTimer + 2u] = (uint8_t)value;
        compare_prepared_case();
    }
}

static void assert_fallback_preserves_entry(void) {
    static const EnemyCase unsupported_cases[] = {
        { BuzzyBeetle, 0u },
        { Goomba, 1u },
        { PiranhaPlant, 1u },
        { Lakitu, 5u },
        { Spiny, 1u },
        { 0xffu, 0u },
    };
    MachineState entry;
    MachineState actual;
    size_t index;
    uint16_t sprite_offset;

    for (index = 0;
         index < sizeof(unsupported_cases) / sizeof(unsupported_cases[0]);
         ++index) {
        prepare_case(2u, &unsupported_cases[index], 7u, 2u, 0x20u, 0xffu);
        save_machine(&entry);
        assert(!smb_core_fast_enemy_gfx_handler());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
    }

    prepare_case(2u, &enemy_cases[0], 7u, 2u, 0u, 0u);
    ram[BowserGfxFlag] = 1u;
    save_machine(&entry);
    assert(!smb_core_fast_enemy_gfx_handler());
    save_machine(&actual);
    assert_machine_equal(&entry, &actual);

    prepare_case(2u, &enemy_cases[0], 7u, 2u, 0u, 0u);
    ram[TimerControl] = 1u;
    save_machine(&entry);
    assert(!smb_core_fast_enemy_gfx_handler());
    save_machine(&actual);
    assert_machine_equal(&entry, &actual);

    prepare_case(2u, &enemy_cases[0], 7u, 2u, 0u, 0u);
    ram[ObjectOffset] = 3u;
    save_machine(&entry);
    assert(!smb_core_fast_enemy_gfx_handler());
    save_machine(&actual);
    assert_machine_equal(&entry, &actual);

    for (sprite_offset = 0; sprite_offset < 256u; ++sprite_offset) {
        if (sprite_offset <= 232u && (sprite_offset & 3u) == 0u) {
            continue;
        }
        prepare_case(2u, &enemy_cases[0], 7u, 2u, 0u, 0u);
        ram[Enemy_SprDataOffset + 2u] = (uint8_t)sprite_offset;
        save_machine(&entry);
        assert(!smb_core_fast_enemy_gfx_handler());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
    }
}

static void fill_move_pattern(uint32_t seed) {
    uint32_t state = seed | 1u;
    uint16_t address;

    for (address = 0; address < RAM_SIZE; ++address) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        ram[address] = (uint8_t)(state >> 24);
    }
    a = (uint8_t)(seed >> 24);
    x = (uint8_t)(seed >> 16);
    y = (uint8_t)(seed >> 8);
    sp = (uint8_t)seed;
    carry_flag = (seed & 1u) != 0u;
    nz_value = (uint8_t)(seed ^ (seed >> 8));
}

static void prepare_move_case(
    uint8_t slot,
    const EnemyCase *enemy,
    uint32_t seed
) {
    fill_move_pattern(seed);
    x = slot;
    ram[ObjectOffset] = slot;
    ram[TimerControl] = 0u;
    ram[Enemy_ID + slot] = enemy->enemy_id;
    ram[Enemy_State + slot] = enemy->enemy_state;
}

static void compare_prepared_move_case(void) {
    MachineState entry;
    MachineState expected;
    MachineState actual;

    save_machine(&entry);
    MoveNormalEnemy();
    save_machine(&expected);

    load_machine(&entry);
    assert(smb_core_fast_move_normal_enemy());
    save_machine(&actual);
    assert_machine_equal(&expected, &actual);
    ++move_direct_comparisons;
}

static void compare_move_boundary_cross_product(void) {
    static const uint8_t speed_values[] = {
        0x00u, 0x01u, 0x07u, 0x08u, 0x0fu, 0x10u,
        0x7fu, 0x80u, 0xefu, 0xf0u, 0xf8u, 0xffu,
    };
    static const uint8_t force_values[] = {
        0x00u, 0x01u, 0x7fu, 0x80u, 0xffu,
    };
    static const uint8_t position_values[] = {
        0x00u, 0x01u, 0x7fu, 0xfeu, 0xffu,
    };
    static const uint8_t page_values[] = {
        0x00u, 0x01u, 0xfeu, 0xffu,
    };
    size_t case_index;
    size_t speed_index;
    size_t force_index;
    size_t position_index;
    size_t page_index;
    uint8_t slot;

    for (case_index = 0;
         case_index < sizeof(move_enemy_cases) / sizeof(move_enemy_cases[0]);
         ++case_index) {
        for (slot = 0; slot < 6u; ++slot) {
            for (speed_index = 0;
                 speed_index < sizeof(speed_values) / sizeof(speed_values[0]);
                 ++speed_index) {
                for (force_index = 0;
                     force_index <
                        sizeof(force_values) / sizeof(force_values[0]);
                     ++force_index) {
                    for (position_index = 0;
                         position_index <
                            sizeof(position_values) /
                                sizeof(position_values[0]);
                         ++position_index) {
                        for (page_index = 0;
                             page_index <
                                sizeof(page_values) / sizeof(page_values[0]);
                             ++page_index) {
                            prepare_move_case(
                                slot,
                                &move_enemy_cases[case_index],
                                (uint32_t)(
                                    UINT32_C(0x9e3779b9) *
                                    (move_direct_comparisons + 1u)
                                )
                            );
                            ram[Enemy_X_Speed + slot] =
                                speed_values[speed_index];
                            ram[Enemy_X_MoveForce + slot] =
                                force_values[force_index];
                            ram[Enemy_X_Position + slot] =
                                position_values[position_index];
                            ram[Enemy_PageLoc + slot] =
                                page_values[page_index];
                            compare_prepared_move_case();
                        }
                    }
                }
            }
        }
    }
}

static void compare_move_every_read_byte(void) {
    size_t case_index;
    uint16_t value;

    for (case_index = 0;
         case_index < sizeof(move_enemy_cases) / sizeof(move_enemy_cases[0]);
         ++case_index) {
        const EnemyCase *enemy = &move_enemy_cases[case_index];

        for (value = 0; value < 256u; ++value) {
#define COMPARE_MOVE_RAM_BYTE(address) \
            do { \
                prepare_move_case(2u, enemy, UINT32_C(0x6d2b79f5) + value); \
                ram[(address)] = (uint8_t)value; \
                compare_prepared_move_case(); \
            } while (0)

            COMPARE_MOVE_RAM_BYTE(Enemy_X_Speed + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_X_MoveForce + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_X_Position + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_PageLoc + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_YMF_Dummy + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_Y_MoveForce + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_Y_Speed + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_Y_Position + 2u);
            COMPARE_MOVE_RAM_BYTE(Enemy_Y_HighPos + 2u);
            COMPARE_MOVE_RAM_BYTE(0x0u);
            COMPARE_MOVE_RAM_BYTE(0x1u);
            COMPARE_MOVE_RAM_BYTE(0x2u);
            COMPARE_MOVE_RAM_BYTE(0x7u);

#undef COMPARE_MOVE_RAM_BYTE

            prepare_move_case(2u, enemy, UINT32_C(0xa511e9b3) + value);
            a = (uint8_t)value;
            compare_prepared_move_case();

            prepare_move_case(2u, enemy, UINT32_C(0x63d83595) + value);
            y = (uint8_t)value;
            compare_prepared_move_case();

            prepare_move_case(2u, enemy, UINT32_C(0x243f6a88) + value);
            sp = (uint8_t)value;
            compare_prepared_move_case();

            prepare_move_case(2u, enemy, UINT32_C(0xb7e15162) + value);
            nz_value = (uint8_t)value;
            carry_flag = false;
            compare_prepared_move_case();

            prepare_move_case(2u, enemy, UINT32_C(0x8aed2a6b) + value);
            nz_value = (uint8_t)value;
            carry_flag = true;
            compare_prepared_move_case();
        }
    }
}

static void compare_move_patterned_full_ram(void) {
    size_t case_index;
    uint16_t seed;
    uint8_t slot;

    for (case_index = 0;
         case_index < sizeof(move_enemy_cases) / sizeof(move_enemy_cases[0]);
         ++case_index) {
        for (slot = 0; slot < 6u; ++slot) {
            for (seed = 0; seed < 512u; ++seed) {
                prepare_move_case(
                    slot,
                    &move_enemy_cases[case_index],
                    UINT32_C(0x85ebca6b) * (seed + 1u) + slot
                );
                compare_prepared_move_case();
            }
        }
    }
}

static bool move_case_is_supported(uint8_t enemy_id, uint8_t enemy_state) {
    return
        (enemy_id == Goomba && enemy_state == 0u) ||
        (enemy_id == Spiny &&
            (enemy_state == 0u || enemy_state == 5u));
}

static void assert_move_fallback_preserves_entry(void) {
    static const EnemyCase base_enemy = { Goomba, 0u };
    MachineState entry;
    MachineState actual;
    uint16_t enemy_id;
    uint16_t enemy_state;
    uint16_t value;

    for (enemy_id = 0; enemy_id < 256u; ++enemy_id) {
        for (enemy_state = 0; enemy_state < 256u; ++enemy_state) {
            if (move_case_is_supported(
                    (uint8_t)enemy_id,
                    (uint8_t)enemy_state)) {
                continue;
            }
            prepare_move_case(
                2u,
                &base_enemy,
                ((uint32_t)enemy_id << 24) |
                    ((uint32_t)enemy_state << 8) | UINT32_C(0x5a)
            );
            ram[Enemy_ID + 2u] = (uint8_t)enemy_id;
            ram[Enemy_State + 2u] = (uint8_t)enemy_state;
            save_machine(&entry);
            assert(!smb_core_fast_move_normal_enemy());
            save_machine(&actual);
            assert_machine_equal(&entry, &actual);
            ++move_fallback_checks;
        }
    }

    for (value = 0; value < 256u; ++value) {
        prepare_move_case(2u, &base_enemy, UINT32_C(0xc2b2ae35) + value);
        ram[ObjectOffset] = (uint8_t)value;
        if (value == 2u) {
            ram[ObjectOffset] = 3u;
        }
        save_machine(&entry);
        assert(!smb_core_fast_move_normal_enemy());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
        ++move_fallback_checks;

        prepare_move_case(2u, &base_enemy, UINT32_C(0x27d4eb2f) + value);
        ram[TimerControl] = (uint8_t)(value == 0u ? 1u : value);
        save_machine(&entry);
        assert(!smb_core_fast_move_normal_enemy());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
        ++move_fallback_checks;

        prepare_move_case(2u, &base_enemy, UINT32_C(0x165667b1) + value);
        x = (uint8_t)value;
        if (x < 6u) {
            x = 6u;
        }
        save_machine(&entry);
        assert(!smb_core_fast_move_normal_enemy());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
        ++move_fallback_checks;
    }
}

int main(void) {
    compare_direct_and_translated();
    compare_every_read_byte();
    assert_fallback_preserves_entry();
    compare_move_boundary_cross_product();
    compare_move_every_read_byte();
    compare_move_patterned_full_ram();
    assert_move_fallback_preserves_entry();
    printf(
        "Neo Geo core fast-path differential tests: OK "
        "(%u graphics, %u movement, %u untouched fallbacks)\n",
        direct_comparisons,
        move_direct_comparisons,
        move_fallback_checks
    );
    return 0;
}
