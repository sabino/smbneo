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
    uint16_t address;

    for (address = 0u; address < RAM_SIZE; ++address) {
        if (expected->ram[address] != actual->ram[address]) {
            fprintf(
                stderr,
                "comparison %u RAM %04x: expected %02x actual %02x\n",
                direct_comparisons,
                address,
                expected->ram[address],
                actual->ram[address]
            );
            break;
        }
    }
    assert(memcmp(expected->ram, actual->ram, RAM_SIZE) == 0);
    assert(expected->a == actual->a);
    assert(expected->x == actual->x);
    assert(expected->y == actual->y);
    assert(expected->sp == actual->sp);
    assert(expected->carry == actual->carry);
    assert(expected->nz == actual->nz);
}

static void prepare_case(uint8_t seed) {
    uint16_t address;
    uint8_t index;

    for (address = 0u; address < RAM_SIZE; ++address) {
        ram[address] = (uint8_t)(
            (address * 37u) ^ (address >> 3) ^ (seed * 53u)
        );
    }

    a = (uint8_t)(seed * 11u + 3u);
    x = (uint8_t)(seed * 13u + 5u);
    y = (uint8_t)(seed * 17u + 7u);
    sp = (uint8_t)(seed * 19u + 9u);
    carry_flag = (seed & 1u) != 0u;
    nz_value = (uint8_t)(seed * 23u + 1u);

    ram[VRAM_Buffer2_Offset] = 0u;
    ram[CurrentColumnPos] = seed;
    ram[AreaParserTaskNum] = (uint8_t)(seed ^ 5u);
    ram[CurrentNTAddr_Low] = (uint8_t)(0x7bu + seed);
    ram[CurrentNTAddr_High] = (uint8_t)(0x20u | (seed & 7u));
    for (index = 0u; index < 13u; ++index) {
        ram[MetatileBuffer + index] = (uint8_t)(
            seed * 29u + index * 47u
        );
    }
    for (index = 0u; index < 7u; ++index) {
        ram[AttributeBuffer + index] = (uint8_t)(
            seed * 31u + index * 41u
        );
    }
}

static void compare_prepared_case(void) {
    MachineState entry;
    MachineState expected;
    MachineState actual;

    save_machine(&entry);
    RenderAreaGraphics();
    save_machine(&expected);

    load_machine(&entry);
    assert(smb_core_fast_render_area_graphics());
    save_machine(&actual);

    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static void compare_cross_product(void) {
    static const uint8_t low_values[] = {
        0x00u, 0x1eu, 0x1fu, 0x20u, 0x7fu, 0xfeu, 0xffu,
    };
    static const uint8_t high_values[] = {0x00u, 0x20u, 0x24u, 0xffu};
    uint8_t seed;
    uint8_t column;
    uint8_t task;
    size_t low_index;
    size_t high_index;

    for (seed = 0u; seed < 16u; ++seed) {
        for (column = 0u; column < 2u; ++column) {
            for (task = 0u; task < 2u; ++task) {
                for (low_index = 0u;
                     low_index < sizeof(low_values) / sizeof(low_values[0]);
                     ++low_index) {
                    for (high_index = 0u;
                         high_index <
                            sizeof(high_values) / sizeof(high_values[0]);
                         ++high_index) {
                        prepare_case(seed);
                        ram[CurrentColumnPos] = column;
                        ram[AreaParserTaskNum] = task;
                        ram[CurrentNTAddr_Low] = low_values[low_index];
                        ram[CurrentNTAddr_High] = high_values[high_index];
                        compare_prepared_case();
                    }
                }
            }
        }
    }
}

static void compare_every_read_byte(void) {
    uint16_t value;
    uint8_t index;

    for (value = 0u; value < 256u; ++value) {
        prepare_case(3u);
        ram[CurrentColumnPos] = (uint8_t)value;
        compare_prepared_case();

        prepare_case(5u);
        ram[AreaParserTaskNum] = (uint8_t)value;
        compare_prepared_case();

        prepare_case(7u);
        ram[CurrentNTAddr_Low] = (uint8_t)value;
        compare_prepared_case();

        prepare_case(11u);
        ram[CurrentNTAddr_High] = (uint8_t)value;
        compare_prepared_case();

        for (index = 0u; index < 13u; ++index) {
            prepare_case((uint8_t)(13u + index));
            ram[MetatileBuffer + index] = (uint8_t)value;
            compare_prepared_case();
        }

        for (index = 0u; index < 7u; ++index) {
            prepare_case((uint8_t)(31u + index));
            ram[AttributeBuffer + index] = (uint8_t)value;
            compare_prepared_case();
        }
    }
}

static void compare_every_initial_machine_byte(void) {
    uint16_t value;
    uint8_t scratch;

    for (value = 0u; value < 256u; ++value) {
        prepare_case(41u);
        a = (uint8_t)value;
        compare_prepared_case();

        prepare_case(43u);
        x = (uint8_t)value;
        compare_prepared_case();

        prepare_case(47u);
        y = (uint8_t)value;
        compare_prepared_case();

        prepare_case(53u);
        sp = (uint8_t)value;
        compare_prepared_case();

        prepare_case(59u);
        nz_value = (uint8_t)value;
        compare_prepared_case();

        for (scratch = 0u; scratch < 8u; ++scratch) {
            prepare_case((uint8_t)(61u + scratch));
            ram[scratch] = (uint8_t)value;
            compare_prepared_case();
        }
    }

    prepare_case(71u);
    carry_flag = false;
    compare_prepared_case();
    prepare_case(73u);
    carry_flag = true;
    compare_prepared_case();
}

static void verify_noncanonical_offsets_fall_back_untouched(void) {
    MachineState expected;
    MachineState actual;
    uint16_t offset;

    for (offset = 1u; offset < 256u; ++offset) {
        prepare_case((uint8_t)(offset * 17u));
        ram[VRAM_Buffer2_Offset] = (uint8_t)offset;
        save_machine(&expected);
        assert(!smb_core_fast_render_area_graphics());
        save_machine(&actual);
        assert_machine_equal(&expected, &actual);
    }
}

int main(void) {
    compare_cross_product();
    compare_every_read_byte();
    compare_every_initial_machine_byte();
    verify_noncanonical_offsets_fall_back_untouched();

    assert(direct_comparisons == 11266u);
    printf(
        "render area graphics fast path: %u exact machine comparisons, "
        "255 untouched fallbacks\n",
        direct_comparisons
    );
    return 0;
}
