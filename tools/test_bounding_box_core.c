#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "data.h"

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

static uint8_t reference_read(
    const struct reference_cpu *cpu,
    uint16_t address
) {
    if (address < 0x2000u) {
        return cpu->ram[address & 0x07ffu];
    }
    if (address >= 0x8000u) {
        return data[address - 0x8000u];
    }
    return 0u;
}

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

static void reference_asl(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 0x80u) != 0u;
    cpu->a = (uint8_t)(cpu->a << 1);
    cpu->nz = cpu->a;
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum = (uint16_t)cpu->a + value + cpu->carry;
    cpu->carry = sum > 0xffu;
    cpu->a = (uint8_t)sum;
    cpu->nz = cpu->a;
}

/*
 * Literal instruction-shaped oracle for src/smb.asm:BoundingBoxCore. It
 * deliberately retains the 6502 stack write and every final register/flag
 * effect instead of sharing the optimized geometry expressions.
 */
static void reference_bounding_box_core(struct reference_cpu *cpu) {
    cpu->ram[0x00] = cpu->x;
    reference_lda(
        cpu,
        reference_read(cpu, (uint16_t)(SprObject_Rel_YPos + cpu->y))
    );
    cpu->ram[0x02] = cpu->a;
    reference_lda(
        cpu,
        reference_read(cpu, (uint16_t)(SprObject_Rel_XPos + cpu->y))
    );
    cpu->ram[0x01] = cpu->a;
    cpu->a = cpu->x;
    cpu->nz = cpu->a;
    reference_asl(cpu);
    reference_asl(cpu);
    cpu->ram[0x100u | cpu->sp] = cpu->a;
    cpu->sp = (uint8_t)(cpu->sp - 1u);
    reference_ldy(cpu, cpu->a);
    reference_lda(
        cpu,
        reference_read(cpu, (uint16_t)(SprObj_BoundBoxCtrl + cpu->x))
    );
    reference_asl(cpu);
    reference_asl(cpu);
    reference_ldx(cpu, cpu->a);
    reference_lda(cpu, reference_read(cpu, 0x01u));
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read(cpu, (uint16_t)(BoundBoxCtrlData + cpu->x))
    );
    cpu->ram[(uint16_t)(BoundingBox_UL_Corner + cpu->y)] = cpu->a;
    reference_lda(cpu, reference_read(cpu, 0x01u));
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read(cpu, (uint16_t)(BoundBoxCtrlData + 2u + cpu->x))
    );
    cpu->ram[(uint16_t)(BoundingBox_LR_Corner + cpu->y)] = cpu->a;
    cpu->x = (uint8_t)(cpu->x + 1u);
    cpu->nz = cpu->x;
    cpu->y = (uint8_t)(cpu->y + 1u);
    cpu->nz = cpu->y;
    reference_lda(cpu, reference_read(cpu, 0x02u));
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read(cpu, (uint16_t)(BoundBoxCtrlData + cpu->x))
    );
    cpu->ram[(uint16_t)(BoundingBox_UL_Corner + cpu->y)] = cpu->a;
    reference_lda(cpu, reference_read(cpu, 0x02u));
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read(cpu, (uint16_t)(BoundBoxCtrlData + 2u + cpu->x))
    );
    cpu->ram[(uint16_t)(BoundingBox_LR_Corner + cpu->y)] = cpu->a;
    cpu->sp = (uint8_t)(cpu->sp + 1u);
    reference_lda(cpu, reference_read(cpu, (uint16_t)(0x100u | cpu->sp)));
    reference_ldy(cpu, cpu->a);
    reference_ldx(cpu, reference_read(cpu, 0x00u));
}

static uint8_t pattern_byte(uint32_t seed, unsigned int address) {
    uint32_t value = seed ^ (uint32_t)address * 0x45d9f3bu;
    value ^= value >> 16;
    value *= 0x45d9f3bu;
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
    uint8_t relative_x,
    uint8_t relative_y,
    uint8_t control
) {
    struct reference_cpu reference;
    unsigned int address;

    for (address = 0; address < RAM_SIZE; ++address) {
        reference.ram[address] = pattern_byte(ordinal * 0x9e3779b9u, address);
    }
    reference.a = initial_a;
    reference.x = initial_x;
    reference.y = initial_y;
    reference.sp = initial_sp;
    reference.carry = initial_carry;
    reference.nz = initial_nz;

    /* Exercise address aliasing by applying every input to one shared image. */
    reference.ram[SprObject_Rel_YPos + initial_y] = relative_y;
    reference.ram[SprObject_Rel_XPos + initial_y] = relative_x;
    reference.ram[SprObj_BoundBoxCtrl + initial_x] = control;
    memcpy(ram, reference.ram, sizeof(ram));
    a = initial_a;
    x = initial_x;
    y = initial_y;
    sp = initial_sp;
    carry_flag = initial_carry;
    nz_value = initial_nz;

    reference_bounding_box_core(&reference);
    BoundingBoxCore();
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

static int test_all_index_pairs(void) {
    unsigned int x_value;
    unsigned int y_value;

    for (x_value = 0; x_value < 256u; ++x_value) {
        for (y_value = 0; y_value < 256u; ++y_value) {
            const uint32_t ordinal = (uint32_t)(x_value << 8) | y_value;
            if (run_case(
                    "index-pairs",
                    ordinal,
                    (uint8_t)(x_value ^ y_value),
                    (uint8_t)x_value,
                    (uint8_t)y_value,
                    (uint8_t)(x_value + 3u * y_value),
                    (ordinal & 1u) != 0u,
                    (uint8_t)(x_value * 13u + y_value * 7u),
                    (uint8_t)(x_value * 31u + y_value),
                    (uint8_t)(y_value * 29u - x_value),
                    (uint8_t)(x_value * 17u + y_value * 43u)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_stack_control_pairs(void) {
    unsigned int stack_value;
    unsigned int control;

    for (stack_value = 0; stack_value < 256u; ++stack_value) {
        for (control = 0; control < 256u; ++control) {
            const uint32_t ordinal =
                0x10000u + (uint32_t)(stack_value << 8) + control;
            if (run_case(
                    "stack-control-pairs",
                    ordinal,
                    (uint8_t)control,
                    (uint8_t)(stack_value + control),
                    (uint8_t)(stack_value * 5u + control * 3u),
                    (uint8_t)stack_value,
                    (control & 0x80u) != 0u,
                    (uint8_t)(control - stack_value),
                    (uint8_t)(control + 0xf8u),
                    (uint8_t)(stack_value + 0xe8u),
                    (uint8_t)control
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_coordinate_pairs(void) {
    static const uint8_t boundary_indices[] = {
        0x00, 0x01, 0x3f, 0x40, 0x7f, 0x80, 0xbf, 0xc0, 0xfe, 0xff
    };
    unsigned int relative_x;
    unsigned int relative_y;

    for (relative_x = 0; relative_x < 256u; ++relative_x) {
        for (relative_y = 0; relative_y < 256u; ++relative_y) {
            const uint32_t ordinal =
                0x20000u + (uint32_t)(relative_x << 8) + relative_y;
            const uint8_t x_index = boundary_indices[
                (relative_x + relative_y) %
                (sizeof(boundary_indices) / sizeof(boundary_indices[0]))
            ];
            const uint8_t y_index = boundary_indices[
                (3u * relative_x + 7u * relative_y) %
                (sizeof(boundary_indices) / sizeof(boundary_indices[0]))
            ];
            if (run_case(
                    "coordinate-pairs",
                    ordinal,
                    (uint8_t)(relative_x + relative_y),
                    x_index,
                    y_index,
                    (uint8_t)(relative_x - relative_y),
                    (relative_y & 1u) != 0u,
                    (uint8_t)(relative_x ^ relative_y),
                    (uint8_t)relative_x,
                    (uint8_t)relative_y,
                    (uint8_t)(relative_x * 11u + relative_y * 19u)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    if (test_all_index_pairs() != 0 ||
        test_all_stack_control_pairs() != 0 ||
        test_all_coordinate_pairs() != 0) {
        return 1;
    }

    printf(
        "BoundingBoxCore differential regression: PASS (%" PRIu64
        " cases; exhaustive X/Y, SP/control, and coordinate pairs)\n",
        case_count
    );
    return 0;
}
