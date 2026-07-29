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
    uint8_t y_position;
    uint8_t count;
} ObservedYRun;

static const uint8_t selected_ids[] = { PiranhaPlant, Lakitu };

/* Exact state-zero entries observed in the W1 stress window. */
static const ObservedYRun observed_w1_piranha[] = {
    { 0x78u, 52u }, { 0x79u, 2u }, { 0x7au, 2u }, { 0x7bu, 2u },
    { 0x7cu, 2u }, { 0x7du, 2u }, { 0x7eu, 2u }, { 0x7fu, 2u },
    { 0x80u, 2u }, { 0x81u, 2u }, { 0x82u, 2u }, { 0x83u, 2u },
    { 0x84u, 2u }, { 0x85u, 2u }, { 0x86u, 2u }, { 0x87u, 2u },
    { 0x88u, 66u }, { 0x89u, 6u }, { 0x8au, 6u }, { 0x8bu, 6u },
    { 0x8cu, 6u }, { 0x8du, 6u }, { 0x8eu, 6u }, { 0x8fu, 6u },
    { 0x90u, 5u }, { 0x91u, 4u }, { 0x92u, 4u }, { 0x93u, 4u },
    { 0x94u, 4u }, { 0x95u, 4u }, { 0x96u, 2u }, { 0x97u, 2u },
};

/* Exact state-zero entries observed in the W4 stress window. */
static const ObservedYRun observed_w4_piranha[] = {
    { 0x78u, 64u }, { 0x79u, 6u }, { 0x7au, 6u }, { 0x7bu, 6u },
    { 0x7cu, 6u }, { 0x7du, 6u }, { 0x7eu, 6u }, { 0x7fu, 6u },
    { 0x80u, 6u }, { 0x81u, 6u }, { 0x82u, 4u }, { 0x83u, 4u },
    { 0x84u, 4u }, { 0x85u, 4u }, { 0x86u, 4u }, { 0x87u, 4u },
    { 0x88u, 4u }, { 0x89u, 4u }, { 0x8au, 4u }, { 0x8bu, 4u },
    { 0x8cu, 2u },
};

static const ObservedYRun observed_w4_lakitu[] = {
    { 0x28u, 120u },
};

static unsigned int direct_comparisons;
static unsigned int direct_carry_clear;
static unsigned int direct_carry_set;
static unsigned int fallback_checks;
static unsigned int observed_entries;

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

static void reference_update_nz(MachineState *state, uint8_t value) {
    state->nz = value;
}

static void reference_lda_zpx(MachineState *state, uint8_t base) {
    state->a = state->ram[(uint8_t)(base + state->x)];
    reference_update_nz(state, state->a);
}

static void reference_ldy_zpx(MachineState *state, uint8_t base) {
    state->y = state->ram[(uint8_t)(base + state->x)];
    reference_update_nz(state, state->y);
}

static void reference_and_imm(MachineState *state, uint8_t value) {
    state->a &= value;
    reference_update_nz(state, state->a);
}

static void reference_adc_imm(MachineState *state, uint8_t value) {
    uint16_t sum = (uint16_t)state->a + value + (state->carry ? 1u : 0u);

    state->carry = sum > 0xffu;
    state->a = (uint8_t)sum;
    reference_update_nz(state, state->a);
}

static void reference_cmp_imm(MachineState *state, uint8_t value) {
    state->carry = state->a >= value;
    reference_update_nz(state, (uint8_t)(state->a - value));
}

static void reference_cpy_imm(MachineState *state, uint8_t value) {
    state->carry = state->y >= value;
    reference_update_nz(state, (uint8_t)(state->y - value));
}

/*
 * Literal source-path oracle for the two table-selected policies. It follows
 * each original instruction and branch through the folded SubtEnemyYPos leaf
 * and the final CPY #$07 return, rather than restating the optimized formula.
 */
static bool reference_selected_policy(MachineState *state) {
    uint8_t enemy_id;

    reference_lda_zpx(state, Enemy_State);
    reference_and_imm(state, 0x20u);
    assert(state->nz == 0u);

    reference_lda_zpx(state, Enemy_Y_Position);
    state->carry = false;
    reference_adc_imm(state, 0x3eu);
    reference_cmp_imm(state, 0x44u);
    if (!state->carry) {
        return true;
    }

    reference_ldy_zpx(state, Enemy_ID);
    enemy_id = state->y;
    assert(enemy_id == PiranhaPlant || enemy_id == Lakitu);
    reference_cpy_imm(state, Spiny);
    assert(state->nz != 0u);
    reference_cpy_imm(state, GreenParatroopaJump);
    assert(state->nz != 0u);
    reference_cpy_imm(state, HammerBro);
    assert(state->nz != 0u);
    reference_cpy_imm(state, Spiny);
    assert(state->nz != 0u);
    reference_cpy_imm(state, PowerUpObject);
    assert(state->nz != 0u);
    reference_cpy_imm(state, 0x07u);
    assert(state->carry);
    return true;
}

static void prepare_selected_entry(
    MachineState *entry,
    uint8_t slot,
    uint8_t enemy_id,
    uint8_t y_position,
    uint32_t seed
) {
    fill_pattern(entry, seed);
    entry->x = slot;
    entry->ram[(uint8_t)(Enemy_State + slot)] = 0u;
    entry->ram[(uint8_t)(Enemy_ID + slot)] = enemy_id;
    entry->ram[(uint8_t)(Enemy_Y_Position + slot)] = y_position;
}

static void compare_selected_entry(const MachineState *entry) {
    MachineState translated;
    MachineState literal = *entry;
    MachineState direct;
    bool expected_carry;

    assert(reference_selected_policy(&literal));
    expected_carry = literal.carry;

    load_machine(entry);
    EnemyToBGCollisionDet();
    save_machine(&translated);
    assert_machine_equal(&literal, &translated);

    load_machine(entry);
    assert(smb_core_fast_enemy_to_bg_collision_det());
    save_machine(&direct);
    assert_machine_equal(&translated, &direct);

    ++direct_comparisons;
    if (expected_carry) {
        ++direct_carry_set;
    } else {
        ++direct_carry_clear;
    }
}

static void compare_all_slots_and_vertical_positions(void) {
    MachineState entry;
    size_t id_index;
    uint16_t slot;
    uint16_t y_position;

    for (id_index = 0;
         id_index < sizeof(selected_ids) / sizeof(selected_ids[0]);
         ++id_index) {
        for (slot = 0; slot < 256u; ++slot) {
            for (y_position = 0; y_position < 256u; ++y_position) {
                prepare_selected_entry(
                    &entry,
                    (uint8_t)slot,
                    selected_ids[id_index],
                    (uint8_t)y_position,
                    ((uint32_t)selected_ids[id_index] << 24) |
                        ((uint32_t)slot << 16) |
                        ((uint32_t)y_position << 8) |
                        (uint32_t)y_position
                );
                /* This also exhausts every SP value for every wrapped X. */
                entry.sp = (uint8_t)y_position;
                entry.y = (uint8_t)(y_position * 29u + slot);
                compare_selected_entry(&entry);
            }
        }
    }

    assert(direct_comparisons == 131072u);
    assert(direct_carry_clear == 34816u);
    assert(direct_carry_set == 96256u);
}

static bool y_position_clears_compare_carry(uint8_t y_position) {
    uint8_t adjusted = (uint8_t)(y_position + 0x3eu);

    return adjusted < 0x44u;
}

static void compare_every_preserved_y_on_clear_return(void) {
    MachineState entry;
    size_t id_index;
    uint16_t y_position;
    uint16_t incoming_y;
    unsigned int comparisons_before = direct_comparisons;

    for (id_index = 0;
         id_index < sizeof(selected_ids) / sizeof(selected_ids[0]);
         ++id_index) {
        for (y_position = 0; y_position < 256u; ++y_position) {
            if (!y_position_clears_compare_carry((uint8_t)y_position)) {
                continue;
            }
            for (incoming_y = 0; incoming_y < 256u; ++incoming_y) {
                prepare_selected_entry(
                    &entry,
                    0xfeu,
                    selected_ids[id_index],
                    (uint8_t)y_position,
                    ((uint32_t)y_position << 24) |
                        ((uint32_t)incoming_y << 8) | UINT32_C(0xa5)
                );
                entry.y = (uint8_t)incoming_y;
                compare_selected_entry(&entry);
            }
        }
    }

    assert(direct_comparisons - comparisons_before == 34816u);
    assert(direct_carry_clear == 69632u);
}

static void assert_fallback_entry_untouched(const MachineState *entry) {
    MachineState actual;

    load_machine(entry);
    assert(!smb_core_fast_enemy_to_bg_collision_det());
    save_machine(&actual);
    assert_machine_equal(entry, &actual);
    ++fallback_checks;
}

static void assert_every_other_policy_falls_back(void) {
    MachineState entry;
    uint16_t slot;
    uint16_t enemy_id;
    uint16_t enemy_state;
    size_t id_index;

    for (slot = 0; slot < 256u; ++slot) {
        for (enemy_id = 0; enemy_id < 256u; ++enemy_id) {
            if (enemy_id == PiranhaPlant || enemy_id == Lakitu) {
                continue;
            }
            fill_pattern(
                &entry,
                ((uint32_t)slot << 24) |
                    ((uint32_t)enemy_id << 8) | UINT32_C(0x5a)
            );
            entry.x = (uint8_t)slot;
            entry.ram[(uint8_t)(Enemy_State + slot)] = 0u;
            entry.ram[(uint8_t)(Enemy_ID + slot)] = (uint8_t)enemy_id;
            assert_fallback_entry_untouched(&entry);
        }
    }

    for (id_index = 0;
         id_index < sizeof(selected_ids) / sizeof(selected_ids[0]);
         ++id_index) {
        for (slot = 0; slot < 256u; ++slot) {
            for (enemy_state = 1u; enemy_state < 256u; ++enemy_state) {
                fill_pattern(
                    &entry,
                    ((uint32_t)selected_ids[id_index] << 24) |
                        ((uint32_t)slot << 16) |
                        ((uint32_t)enemy_state << 8) | UINT32_C(0x3c)
                );
                entry.x = (uint8_t)slot;
                entry.ram[(uint8_t)(Enemy_ID + slot)] =
                    selected_ids[id_index];
                entry.ram[(uint8_t)(Enemy_State + slot)] =
                    (uint8_t)enemy_state;
                assert_fallback_entry_untouched(&entry);
            }
        }
    }

    assert(fallback_checks == 195584u);
}

static void replay_observed_runs(
    uint8_t enemy_id,
    const ObservedYRun *runs,
    size_t run_count,
    unsigned int expected_count,
    uint32_t seed
) {
    MachineState entry;
    unsigned int before = observed_entries;
    size_t run_index;
    uint16_t repetition;

    for (run_index = 0; run_index < run_count; ++run_index) {
        for (repetition = 0; repetition < runs[run_index].count; ++repetition) {
            uint8_t slot = (uint8_t)((observed_entries * 5u) & 0xffu);

            prepare_selected_entry(
                &entry,
                slot,
                enemy_id,
                runs[run_index].y_position,
                seed + observed_entries
            );
            compare_selected_entry(&entry);
            assert(carry_flag);
            ++observed_entries;
        }
    }
    assert(observed_entries - before == expected_count);
}

static void replay_all_observed_entries(void) {
    replay_observed_runs(
        PiranhaPlant,
        observed_w1_piranha,
        sizeof(observed_w1_piranha) / sizeof(observed_w1_piranha[0]),
        219u,
        UINT32_C(0x34240000)
    );
    replay_observed_runs(
        PiranhaPlant,
        observed_w4_piranha,
        sizeof(observed_w4_piranha) / sizeof(observed_w4_piranha[0]),
        160u,
        UINT32_C(0x25691000)
    );
    replay_observed_runs(
        Lakitu,
        observed_w4_lakitu,
        sizeof(observed_w4_lakitu) / sizeof(observed_w4_lakitu[0]),
        120u,
        UINT32_C(0x25800000)
    );
    assert(observed_entries == 499u);
}

int main(void) {
    compare_all_slots_and_vertical_positions();
    compare_every_preserved_y_on_clear_return();
    assert_every_other_policy_falls_back();
    replay_all_observed_entries();
    printf(
        "Enemy background collision policy: OK "
        "(%u direct: %u carry-clear, %u carry-set; "
        "%u observed; %u untouched fallbacks)\n",
        direct_comparisons,
        direct_carry_clear,
        direct_carry_set,
        observed_entries,
        fallback_checks
    );
    return 0;
}
