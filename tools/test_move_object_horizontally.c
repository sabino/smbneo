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

static uint8_t reference_zpx(
    const struct reference_cpu *cpu,
    uint8_t base
) {
    return cpu->ram[(uint8_t)(base + cpu->x)];
}

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
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

static void reference_lsr(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 0x01u) != 0u;
    cpu->a = (uint8_t)(cpu->a >> 1);
    cpu->nz = cpu->a;
}

static void reference_cmp(struct reference_cpu *cpu, uint8_t value) {
    cpu->carry = cpu->a >= value;
    cpu->nz = (uint8_t)(cpu->a - value);
}

static void reference_ora(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = (uint8_t)(cpu->a | value);
    cpu->nz = cpu->a;
}

static void reference_dey(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y - 1u);
    cpu->nz = cpu->y;
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum = (uint16_t)cpu->a + value + cpu->carry;
    cpu->carry = sum > 0xffu;
    cpu->a = (uint8_t)sum;
    cpu->nz = cpu->a;
}

static void reference_rol(struct reference_cpu *cpu) {
    const bool old_carry = cpu->carry;
    cpu->carry = (cpu->a & 0x80u) != 0u;
    cpu->a = (uint8_t)((cpu->a << 1) | (old_carry ? 1u : 0u));
    cpu->nz = cpu->a;
}

static void reference_ror(struct reference_cpu *cpu) {
    const bool old_carry = cpu->carry;
    cpu->carry = (cpu->a & 0x01u) != 0u;
    cpu->a = (uint8_t)((cpu->a >> 1) | (old_carry ? 0x80u : 0u));
    cpu->nz = cpu->a;
}

static void reference_pha(struct reference_cpu *cpu) {
    cpu->ram[0x100u | cpu->sp] = cpu->a;
    cpu->sp = (uint8_t)(cpu->sp - 1u);
}

static void reference_pla(struct reference_cpu *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1u);
    reference_lda(cpu, cpu->ram[0x100u | cpu->sp]);
}

/*
 * Literal instruction-shaped oracle for src/smb.asm:MoveObjectHorizontally.
 * Keeping the repeated speed read, zero-page wrapping, scratch accesses, and
 * real stack traffic makes unsupported alias states part of the contract.
 */
static void reference_move_object_horizontally(struct reference_cpu *cpu) {
    reference_lda(cpu, reference_zpx(cpu, SprObject_X_Speed));
    reference_asl(cpu);
    reference_asl(cpu);
    reference_asl(cpu);
    reference_asl(cpu);
    cpu->ram[0x01] = cpu->a;
    reference_lda(cpu, reference_zpx(cpu, SprObject_X_Speed));
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_cmp(cpu, 0x08u);
    if (cpu->carry) {
        reference_ora(cpu, 0xf0u);
    }
    cpu->ram[0x00] = cpu->a;
    reference_ldy(cpu, 0x00u);
    reference_cmp(cpu, 0x00u);
    if ((cpu->nz & 0x80u) != 0u) {
        reference_dey(cpu);
    }
    cpu->ram[0x02] = cpu->y;
    reference_lda(cpu, cpu->ram[SprObject_X_MoveForce + cpu->x]);
    cpu->carry = false;
    reference_adc(cpu, cpu->ram[0x01]);
    cpu->ram[SprObject_X_MoveForce + cpu->x] = cpu->a;
    reference_lda(cpu, 0x00u);
    reference_rol(cpu);
    reference_pha(cpu);
    reference_ror(cpu);
    reference_lda(cpu, reference_zpx(cpu, SprObject_X_Position));
    reference_adc(cpu, cpu->ram[0x00]);
    cpu->ram[(uint8_t)(SprObject_X_Position + cpu->x)] = cpu->a;
    reference_lda(cpu, reference_zpx(cpu, SprObject_PageLoc));
    reference_adc(cpu, cpu->ram[0x02]);
    cpu->ram[(uint8_t)(SprObject_PageLoc + cpu->x)] = cpu->a;
    reference_pla(cpu);
    cpu->carry = false;
    reference_adc(cpu, cpu->ram[0x00]);
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
    uint8_t object_offset,
    uint8_t initial_y,
    uint8_t initial_sp,
    bool initial_carry,
    uint8_t initial_nz,
    uint8_t speed,
    uint8_t move_force,
    uint8_t position,
    uint8_t page
) {
    struct reference_cpu reference;
    unsigned int address;
    const uint8_t speed_address =
        (uint8_t)(SprObject_X_Speed + object_offset);
    const uint8_t position_address =
        (uint8_t)(SprObject_X_Position + object_offset);
    const uint8_t page_address =
        (uint8_t)(SprObject_PageLoc + object_offset);

    for (address = 0; address < RAM_SIZE; ++address) {
        reference.ram[address] = pattern_byte(ordinal * 0x9e3779b9u, address);
    }
    reference.a = initial_a;
    reference.x = object_offset;
    reference.y = initial_y;
    reference.sp = initial_sp;
    reference.carry = initial_carry;
    reference.nz = initial_nz;

    /* One shared image deliberately retains zero-page/scratch aliasing. */
    reference.ram[SprObject_X_MoveForce + object_offset] = move_force;
    reference.ram[position_address] = position;
    reference.ram[page_address] = page;
    reference.ram[speed_address] = speed;
    memcpy(ram, reference.ram, sizeof(ram));
    a = initial_a;
    x = object_offset;
    y = initial_y;
    sp = initial_sp;
    carry_flag = initial_carry;
    nz_value = initial_nz;

    reference_move_object_horizontally(&reference);
    MoveObjectHorizontally();
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

static int test_all_index_speed_pairs(void) {
    unsigned int object_offset;
    unsigned int speed;

    for (object_offset = 0; object_offset < 256u; ++object_offset) {
        for (speed = 0; speed < 256u; ++speed) {
            const uint32_t ordinal =
                (uint32_t)(object_offset << 8) | speed;
            if (run_case(
                    "index-speed-pairs",
                    ordinal,
                    (uint8_t)(object_offset ^ speed),
                    (uint8_t)object_offset,
                    (uint8_t)(object_offset + speed * 3u),
                    (uint8_t)(object_offset * 5u + speed),
                    (ordinal & 1u) != 0u,
                    (uint8_t)(object_offset * 13u + speed * 7u),
                    (uint8_t)speed,
                    (uint8_t)(object_offset * 17u + speed * 29u),
                    (uint8_t)(object_offset * 31u - speed),
                    (uint8_t)(speed * 43u + object_offset)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_speed_force_pairs(void) {
    unsigned int speed;
    unsigned int move_force;

    for (speed = 0; speed < 256u; ++speed) {
        for (move_force = 0; move_force < 256u; ++move_force) {
            const uint32_t ordinal =
                0x10000u + (uint32_t)(speed << 8) + move_force;
            if (run_case(
                    "speed-force-pairs",
                    ordinal,
                    (uint8_t)move_force,
                    (uint8_t)((speed + move_force) % 7u),
                    (uint8_t)(speed - move_force),
                    (uint8_t)(speed + move_force * 3u),
                    (move_force & 0x80u) != 0u,
                    (uint8_t)(speed ^ move_force),
                    (uint8_t)speed,
                    (uint8_t)move_force,
                    (uint8_t)(move_force + 0xf8u),
                    (uint8_t)(speed + 0xffu)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_position_page_pairs(void) {
    unsigned int position;
    unsigned int page;

    for (position = 0; position < 256u; ++position) {
        for (page = 0; page < 256u; ++page) {
            const uint32_t ordinal =
                0x20000u + (uint32_t)(position << 8) + page;
            if (run_case(
                    "position-page-pairs",
                    ordinal,
                    (uint8_t)(position + page),
                    (uint8_t)(1u + ((position + page) % 6u)),
                    (uint8_t)(position * 5u + page),
                    (uint8_t)(position - page),
                    (page & 1u) != 0u,
                    (uint8_t)(position ^ page),
                    (uint8_t)(position * 13u + page * 19u),
                    (uint8_t)(position + page),
                    (uint8_t)position,
                    (uint8_t)page
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_all_index_stack_pairs(void) {
    unsigned int object_offset;
    unsigned int stack_pointer;

    for (object_offset = 0; object_offset < 256u; ++object_offset) {
        for (stack_pointer = 0; stack_pointer < 256u; ++stack_pointer) {
            const uint32_t ordinal =
                0x30000u + (uint32_t)(object_offset << 8) + stack_pointer;
            if (run_case(
                    "index-stack-pairs",
                    ordinal,
                    (uint8_t)stack_pointer,
                    (uint8_t)object_offset,
                    (uint8_t)(object_offset + stack_pointer),
                    (uint8_t)stack_pointer,
                    (object_offset & 0x80u) != 0u,
                    (uint8_t)(object_offset - stack_pointer),
                    (uint8_t)(object_offset * 11u + stack_pointer * 23u),
                    (uint8_t)(stack_pointer + 0xf0u),
                    (uint8_t)(object_offset + 0xf8u),
                    (uint8_t)(stack_pointer + 0xffu)
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    if (test_all_index_speed_pairs() != 0 ||
        test_all_speed_force_pairs() != 0 ||
        test_all_position_page_pairs() != 0 ||
        test_all_index_stack_pairs() != 0) {
        return 1;
    }

    printf(
        "MoveObjectHorizontally differential regression: PASS (%" PRIu64
        " cases; exhaustive X/speed, speed/force, position/page, and X/SP "
        "pairs including zero-page aliases)\n",
        case_count
    );
    return 0;
}
