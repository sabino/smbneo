#include "code.h"
#include "constants.h"
#include "cpu.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct reference_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t ram[RAM_SIZE];
};

static uint64_t case_count;

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    cpu->nz = value;
}

static void reference_ldx(struct reference_cpu *cpu, uint8_t value) {
    cpu->x = value;
    cpu->nz = value;
}

static void reference_ldy(struct reference_cpu *cpu, uint8_t value) {
    cpu->y = value;
    cpu->nz = value;
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum = (uint16_t)cpu->a + value + cpu->carry;

    cpu->carry = sum > 0xffu;
    cpu->a = (uint8_t)sum;
    cpu->nz = cpu->a;
}

static void reference_sbc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t subtrahend = (uint16_t)value +
        (cpu->carry ? 0u : 1u);
    const uint16_t difference = (uint16_t)cpu->a - subtrahend;

    cpu->carry = (uint16_t)cpu->a >= subtrahend;
    cpu->a = (uint8_t)difference;
    cpu->nz = cpu->a;
}

/*
 * Literal instruction-shaped oracle for RelativeEnemyPosition and the two
 * helpers it folds. Keep every RAM access in source order so zero-page index
 * wrapping and aliases with scratch byte $00 remain part of the contract.
 */
static void reference_relative_enemy_position(struct reference_cpu *cpu) {
    reference_lda(cpu, 0x01u);
    reference_ldy(cpu, 0x01u);
    cpu->ram[0x00u] = cpu->x;
    cpu->carry = false;
    reference_adc(cpu, cpu->ram[0x00u]);
    reference_ldx(cpu, cpu->a);
    reference_lda(
        cpu,
        cpu->ram[(uint8_t)(SprObject_Y_Position + cpu->x)]
    );
    cpu->ram[SprObject_Rel_YPos + cpu->y] = cpu->a;
    reference_lda(
        cpu,
        cpu->ram[(uint8_t)(SprObject_X_Position + cpu->x)]
    );
    cpu->carry = true;
    reference_sbc(cpu, cpu->ram[ScreenLeft_X_Pos]);
    cpu->ram[SprObject_Rel_XPos + cpu->y] = cpu->a;
    reference_ldx(cpu, cpu->ram[ObjectOffset]);
}

static uint8_t pattern_byte(uint32_t seed, unsigned int address) {
    uint32_t value = seed ^ (uint32_t)address * UINT32_C(0x45d9f3b);

    value ^= value >> 16;
    value *= UINT32_C(0x45d9f3b);
    value ^= value >> 16;
    return (uint8_t)value;
}

static int report_scalar(
    const char *suite,
    uint32_t ordinal,
    const char *field,
    unsigned int expected,
    unsigned int actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu32 " %s: expected 0x%02x, got 0x%02x\n",
        suite,
        ordinal,
        field,
        expected,
        actual
    );
    return 1;
}

static int run_case(
    const char *suite,
    uint32_t ordinal,
    uint8_t initial_a,
    uint8_t initial_x,
    uint8_t initial_y,
    uint8_t initial_sp,
    bool initial_carry,
    uint8_t initial_nz,
    uint8_t source_y,
    uint8_t source_x,
    uint8_t screen_x,
    uint8_t restored_x
) {
    struct reference_cpu reference;
    unsigned int address;
    const uint8_t object_index = (uint8_t)(initial_x + 1u);
    const uint8_t source_y_address =
        (uint8_t)(SprObject_Y_Position + object_index);
    const uint8_t source_x_address =
        (uint8_t)(SprObject_X_Position + object_index);

    for (address = 0; address < RAM_SIZE; ++address) {
        reference.ram[address] = pattern_byte(
            ordinal * UINT32_C(0x9e3779b9),
            address
        );
    }
    reference.a = initial_a;
    reference.x = initial_x;
    reference.y = initial_y;
    reference.sp = initial_sp;
    reference.carry = initial_carry;
    reference.nz = initial_nz;

    /* Apply inputs to one shared image; deliberate aliases keep last-write
     * behavior identical for the oracle and generated implementation. */
    reference.ram[source_y_address] = source_y;
    reference.ram[source_x_address] = source_x;
    reference.ram[ScreenLeft_X_Pos] = screen_x;
    reference.ram[ObjectOffset] = restored_x;

    memcpy(ram, reference.ram, sizeof(ram));
    a = initial_a;
    x = initial_x;
    y = initial_y;
    sp = initial_sp;
    carry_flag = initial_carry;
    nz_value = initial_nz;

    reference_relative_enemy_position(&reference);
    RelativeEnemyPosition();
    ++case_count;

    if (a != reference.a) {
        return report_scalar(suite, ordinal, "A", reference.a, a);
    }
    if (x != reference.x) {
        return report_scalar(suite, ordinal, "X", reference.x, x);
    }
    if (y != reference.y) {
        return report_scalar(suite, ordinal, "Y", reference.y, y);
    }
    if (sp != reference.sp) {
        return report_scalar(suite, ordinal, "SP", reference.sp, sp);
    }
    if (carry_flag != reference.carry) {
        return report_scalar(
            suite,
            ordinal,
            "carry",
            reference.carry,
            carry_flag
        );
    }
    if (nz_value != reference.nz) {
        return report_scalar(suite, ordinal, "NZ", reference.nz, nz_value);
    }
    if (memcmp(ram, reference.ram, sizeof(ram)) != 0) {
        for (address = 0; address < RAM_SIZE; ++address) {
            if (ram[address] != reference.ram[address]) {
                fprintf(
                    stderr,
                    "%s case=%" PRIu32 " RAM[%04x]: expected 0x%02x, "
                    "got 0x%02x\n",
                    suite,
                    ordinal,
                    address,
                    reference.ram[address],
                    ram[address]
                );
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_index_y_pairs(void) {
    unsigned int initial_x;
    unsigned int source_y;

    for (initial_x = 0; initial_x < 256u; ++initial_x) {
        for (source_y = 0; source_y < 256u; ++source_y) {
            const uint32_t ordinal = (uint32_t)(initial_x << 8) | source_y;

            if (run_case(
                    "index-y-pairs",
                    ordinal,
                    (uint8_t)(initial_x ^ source_y),
                    (uint8_t)initial_x,
                    (uint8_t)(source_y + 3u),
                    (uint8_t)(initial_x + source_y),
                    (source_y & 1u) != 0u,
                    (uint8_t)(initial_x - source_y),
                    (uint8_t)source_y,
                    (uint8_t)(initial_x * 17u + source_y * 29u),
                    (uint8_t)(source_y * 7u),
                    (uint8_t)(initial_x * 11u)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_x_screen_pairs(void) {
    unsigned int source_x;
    unsigned int screen_x;

    for (source_x = 0; source_x < 256u; ++source_x) {
        for (screen_x = 0; screen_x < 256u; ++screen_x) {
            const uint32_t ordinal =
                UINT32_C(0x10000) + (uint32_t)(source_x << 8) + screen_x;

            if (run_case(
                    "x-screen-pairs",
                    ordinal,
                    (uint8_t)screen_x,
                    (uint8_t)(1u + ((source_x + screen_x) % 6u)),
                    (uint8_t)(source_x - screen_x),
                    (uint8_t)(source_x + screen_x * 3u),
                    (screen_x & 0x80u) != 0u,
                    (uint8_t)(source_x ^ screen_x),
                    (uint8_t)(source_x * 13u),
                    (uint8_t)source_x,
                    (uint8_t)screen_x,
                    (uint8_t)(screen_x * 19u)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_index_restore_pairs(void) {
    unsigned int initial_x;
    unsigned int restored_x;

    for (initial_x = 0; initial_x < 256u; ++initial_x) {
        for (restored_x = 0; restored_x < 256u; ++restored_x) {
            const uint32_t ordinal =
                UINT32_C(0x20000) + (uint32_t)(initial_x << 8) + restored_x;

            if (run_case(
                    "index-restore-pairs",
                    ordinal,
                    (uint8_t)restored_x,
                    (uint8_t)initial_x,
                    (uint8_t)(initial_x * 3u),
                    (uint8_t)(restored_x * 5u),
                    (initial_x & 0x80u) != 0u,
                    (uint8_t)(initial_x + restored_x),
                    (uint8_t)(restored_x * 7u),
                    (uint8_t)(initial_x * 11u),
                    (uint8_t)(restored_x * 13u),
                    (uint8_t)restored_x
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_index_stack_pairs(void) {
    unsigned int initial_x;
    unsigned int initial_sp;

    for (initial_x = 0; initial_x < 256u; ++initial_x) {
        for (initial_sp = 0; initial_sp < 256u; ++initial_sp) {
            const uint32_t ordinal =
                UINT32_C(0x30000) + (uint32_t)(initial_x << 8) + initial_sp;

            if (run_case(
                    "index-stack-pairs",
                    ordinal,
                    (uint8_t)initial_sp,
                    (uint8_t)initial_x,
                    (uint8_t)(initial_x + initial_sp),
                    (uint8_t)initial_sp,
                    (initial_sp & 1u) != 0u,
                    (uint8_t)(initial_x ^ initial_sp),
                    (uint8_t)(initial_sp * 7u),
                    (uint8_t)(initial_x * 13u),
                    (uint8_t)(initial_sp * 17u),
                    (uint8_t)(initial_x * 19u)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    if (test_all_index_y_pairs() != 0 ||
        test_all_x_screen_pairs() != 0 ||
        test_all_index_restore_pairs() != 0 ||
        test_all_index_stack_pairs() != 0) {
        return 1;
    }

    printf(
        "RelativeEnemyPosition differential regression: PASS (%" PRIu64
        " cases; exhaustive X/Y, X/screen, X/restore, and X/SP pairs "
        "including zero-page aliases)\n",
        case_count
    );
    return 0;
}
