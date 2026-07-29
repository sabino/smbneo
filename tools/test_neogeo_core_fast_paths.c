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

static const uint8_t offscreen_cases[] = {
    0x00u, 0x04u, 0x08u, 0x20u,
    0x40u, 0x80u, 0xfcu, 0xffu,
};

static unsigned int direct_comparisons;

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

int main(void) {
    compare_direct_and_translated();
    compare_every_read_byte();
    assert_fallback_preserves_entry();
    printf(
        "Neo Geo core fast-path differential tests: OK (%u cases)\n",
        direct_comparisons
    );
    return 0;
}
