#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "data.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RUNTIME_OBJECT_COUNT = 9,
    BLOCK_ADDER_COUNT = 28,
    BLOCK_BANK_COUNT = 2,
    BLOCK_COLUMN_COUNT = 16,
    BLOCK_ROW_COUNT = 16,
    RAM_CHECK_INTERVAL = 4096
};

struct reference_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t ram[RAM_SIZE];
};

struct case_context {
    const char *suite;
    uint64_t ordinal;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t page;
    uint8_t x_position;
    uint8_t y_position;
};

struct block_address {
    uint8_t bank;
    uint8_t column;
    uint8_t row;
    uint16_t address;
};

static uint64_t case_count;
static bool logical_address_coverage
    [BLOCK_BANK_COUNT][BLOCK_COLUMN_COUNT][BLOCK_ROW_COUNT];

static uint8_t reference_read_byte(
    const struct reference_cpu *cpu,
    uint16_t address
) {
    if (address < 0x2000u) {
        return cpu->ram[address & (RAM_SIZE - 1u)];
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

static void reference_ldy(struct reference_cpu *cpu, uint8_t value) {
    cpu->y = value;
    cpu->nz = value;
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum =
        (uint16_t)cpu->a + value + (cpu->carry ? 1u : 0u);

    cpu->carry = sum > 0xffu;
    reference_lda(cpu, (uint8_t)sum);
}

static void reference_sbc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t difference = (uint16_t)(
        (uint16_t)cpu->a - value - (cpu->carry ? 0u : 1u)
    );

    cpu->carry = difference <= 0xffu;
    reference_lda(cpu, (uint8_t)difference);
}

static void reference_and(struct reference_cpu *cpu, uint8_t value) {
    reference_lda(cpu, (uint8_t)(cpu->a & value));
}

static void reference_ora(struct reference_cpu *cpu, uint8_t value) {
    reference_lda(cpu, (uint8_t)(cpu->a | value));
}

static void reference_lsr(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 1u) != 0u;
    reference_lda(cpu, (uint8_t)(cpu->a >> 1));
}

static void reference_ror(struct reference_cpu *cpu) {
    const bool old_carry = cpu->carry;

    cpu->carry = (cpu->a & 1u) != 0u;
    reference_lda(
        cpu,
        (uint8_t)((cpu->a >> 1) | (old_carry ? 0x80u : 0u))
    );
}

static void reference_pha(struct reference_cpu *cpu) {
    cpu->ram[0x100u + cpu->sp] = cpu->a;
    cpu->sp = (uint8_t)(cpu->sp - 1u);
}

static void reference_pla(struct reference_cpu *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1u);
    reference_lda(cpu, cpu->ram[0x100u + cpu->sp]);
}

static void reference_lda_zp(struct reference_cpu *cpu, uint8_t address) {
    reference_lda(cpu, cpu->ram[address]);
}

static void reference_lda_zpx(struct reference_cpu *cpu, uint8_t address) {
    reference_lda(cpu, cpu->ram[(uint8_t)(address + cpu->x)]);
}

static void reference_lda_absy(
    struct reference_cpu *cpu,
    uint16_t address
) {
    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(address + cpu->y))
    );
}

static void reference_lda_indy(
    struct reference_cpu *cpu,
    uint8_t address
) {
    const uint8_t high_address = (uint8_t)(address + 1u);
    const uint16_t base = (uint16_t)(
        cpu->ram[address] | ((uint16_t)cpu->ram[high_address] << 8)
    );

    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(base + cpu->y))
    );
}

/* Literal oracle for src/smb.asm:GetBlockBufferAddr. */
static void reference_get_block_buffer_addr(struct reference_cpu *cpu) {
    reference_pha(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_ldy(cpu, cpu->a);
    reference_lda_absy(cpu, BlockBufferAddr + 2u);
    cpu->ram[0x07] = cpu->a;
    reference_pla(cpu);
    reference_and(cpu, 0x0fu);
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read_byte(cpu, (uint16_t)(BlockBufferAddr + cpu->y))
    );
    cpu->ram[0x06] = cpu->a;
}

/*
 * Literal 6502-state oracle for src/smb.asm:BlockBufferCollision. It follows
 * the instruction sequence instead of sharing the specialized C's formulas,
 * so accumulator, index, stack, carry, NZ, scratch-RAM, and alias differences
 * remain observable.
 */
static void reference_block_buffer_collision(struct reference_cpu *cpu) {
    reference_pha(cpu);
    cpu->ram[0x04] = cpu->y;
    reference_lda_absy(cpu, BlockBuffer_X_Adder);
    cpu->carry = false;
    reference_adc(
        cpu,
        cpu->ram[(uint8_t)(SprObject_X_Position + cpu->x)]
    );
    cpu->ram[0x05] = cpu->a;
    reference_lda_zpx(cpu, SprObject_PageLoc);
    reference_adc(cpu, 0x00u);
    reference_and(cpu, 0x01u);
    reference_lsr(cpu);
    reference_ora(cpu, cpu->ram[0x05]);
    reference_ror(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_get_block_buffer_addr(cpu);
    reference_ldy(cpu, cpu->ram[0x04]);
    reference_lda_zpx(cpu, SprObject_Y_Position);
    cpu->carry = false;
    reference_adc(
        cpu,
        reference_read_byte(cpu, (uint16_t)(BlockBuffer_Y_Adder + cpu->y))
    );
    reference_and(cpu, 0xf0u);
    cpu->carry = true;
    reference_sbc(cpu, 0x20u);
    cpu->ram[0x02] = cpu->a;
    reference_ldy(cpu, cpu->a);
    reference_lda_indy(cpu, 0x06u);
    cpu->ram[0x03] = cpu->a;
    reference_ldy(cpu, cpu->ram[0x04]);
    reference_pla(cpu);
    if (cpu->nz == 0u) {
        reference_lda_zpx(cpu, SprObject_Y_Position);
    } else {
        reference_lda_zpx(cpu, SprObject_X_Position);
    }
    reference_and(cpu, 0x0fu);
    cpu->ram[0x04] = cpu->a;
    reference_lda_zp(cpu, 0x03u);
}

static void set_ram_pair(
    struct reference_cpu *reference,
    uint16_t address,
    uint8_t value
) {
    reference->ram[address & (RAM_SIZE - 1u)] = value;
    ram[address & (RAM_SIZE - 1u)] = value;
}

static void initialize_ram(struct reference_cpu *reference) {
    uint16_t address;

    for (address = 0; address < RAM_SIZE; address++) {
        set_ram_pair(
            reference,
            address,
            (uint8_t)(address * 73u + (address >> 3) * 19u + 0x5au)
        );
    }
}

static void set_registers(
    struct reference_cpu *reference,
    uint8_t input_a,
    uint8_t input_x,
    uint8_t input_y,
    uint8_t input_sp,
    bool input_carry,
    uint8_t input_nz
) {
    reference->a = input_a;
    reference->x = input_x;
    reference->y = input_y;
    reference->sp = input_sp;
    reference->carry = input_carry;
    reference->nz = input_nz;
    a = input_a;
    x = input_x;
    y = input_y;
    sp = input_sp;
    carry_flag = input_carry;
    nz_value = input_nz;
}

static void configure_runtime_ram(
    struct reference_cpu *reference,
    uint8_t object,
    uint8_t page,
    uint8_t x_position,
    uint8_t y_position,
    uint8_t scratch_seed
) {
    uint8_t address;

    for (address = 0x02u; address <= 0x07u; address++) {
        set_ram_pair(
            reference,
            address,
            (uint8_t)(scratch_seed + address * 37u)
        );
    }
    set_ram_pair(
        reference,
        (uint8_t)(SprObject_PageLoc + object),
        page
    );
    set_ram_pair(
        reference,
        (uint8_t)(SprObject_X_Position + object),
        x_position
    );
    set_ram_pair(
        reference,
        (uint8_t)(SprObject_Y_Position + object),
        y_position
    );
}

/* Used only to arrange test inputs and record address coverage, not as oracle. */
static struct block_address derive_block_address(
    const struct reference_cpu *reference,
    uint8_t object,
    uint8_t adder_index
) {
    const uint16_t x_sum = (uint16_t)(
        data[BlockBuffer_X_Adder - 0x8000u + adder_index] +
        reference->ram[(uint8_t)(SprObject_X_Position + object)]
    );
    const uint8_t adjusted_x = (uint8_t)x_sum;
    const uint16_t page_sum = (uint16_t)(
        reference->ram[(uint8_t)(SprObject_PageLoc + object)] +
        (x_sum > 0xffu ? 1u : 0u)
    );
    const uint8_t block_column = (uint8_t)(
        (((uint8_t)page_sum & 1u) << 4) | (adjusted_x >> 4)
    );
    const uint8_t bank = (uint8_t)(block_column >> 4);
    const uint8_t column = (uint8_t)(block_column & 0x0fu);
    const uint16_t y_sum = (uint16_t)(
        reference->ram[(uint8_t)(SprObject_Y_Position + object)] +
        data[BlockBuffer_Y_Adder - 0x8000u + adder_index]
    );
    const uint8_t adjusted_y =
        (uint8_t)((y_sum & 0xf0u) - 0x20u);
    const uint8_t low = (uint8_t)(
        data[BlockBufferAddr - 0x8000u + bank] + column
    );
    const uint8_t high =
        data[BlockBufferAddr + 2u - 0x8000u + bank];
    const struct block_address result = {
        bank,
        column,
        (uint8_t)(adjusted_y >> 4),
        (uint16_t)(((uint16_t)high << 8) | low) + adjusted_y
    };

    return result;
}

static int report_mismatch(
    const struct case_context *context,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu64 " a=%02x x=%02x y=%02x sp=%02x "
        "carry=%u nz=%02x page=%02x xpos=%02x ypos=%02x %s: "
        "expected %02x, got %02x\n",
        context->suite,
        context->ordinal,
        (unsigned int)context->a,
        (unsigned int)context->x,
        (unsigned int)context->y,
        (unsigned int)context->sp,
        context->carry ? 1u : 0u,
        (unsigned int)context->nz,
        (unsigned int)context->page,
        (unsigned int)context->x_position,
        (unsigned int)context->y_position,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int compare_full_ram(
    const struct reference_cpu *reference,
    const struct case_context *context
) {
    uint16_t address;

    if (memcmp(reference->ram, ram, RAM_SIZE) == 0) {
        return 0;
    }
    for (address = 0; address < RAM_SIZE; address++) {
        if (reference->ram[address] != ram[address]) {
            char field[32];

            (void)snprintf(
                field,
                sizeof(field),
                "ram[$%04x]",
                (unsigned int)address
            );
            return report_mismatch(
                context,
                field,
                reference->ram[address],
                ram[address]
            );
        }
    }
    return 0;
}

static int run_case(
    struct reference_cpu *reference,
    const char *suite,
    uint64_t ordinal
) {
    const uint8_t original_sp = reference->sp;
    const uint16_t changed_addresses[] = {
        0x02u,
        0x03u,
        0x04u,
        0x05u,
        0x06u,
        0x07u,
        (uint16_t)(0x100u + original_sp),
        (uint16_t)(0x100u + (uint8_t)(original_sp - 1u))
    };
    const struct case_context context = {
        suite,
        ordinal,
        reference->a,
        reference->x,
        reference->y,
        reference->sp,
        reference->carry,
        reference->nz,
        reference->ram[(uint8_t)(SprObject_PageLoc + reference->x)],
        reference->ram[(uint8_t)(SprObject_X_Position + reference->x)],
        reference->ram[(uint8_t)(SprObject_Y_Position + reference->x)]
    };
    size_t index;

    reference_block_buffer_collision(reference);
    BlockBufferCollision();

#define COMPARE_CPU_BYTE(field_name, expected_value, actual_value) \
    do { \
        if ((uint8_t)(expected_value) != (uint8_t)(actual_value)) { \
            return report_mismatch( \
                &context, field_name, (uint8_t)(expected_value), \
                (uint8_t)(actual_value) \
            ); \
        } \
    } while (0)

    COMPARE_CPU_BYTE("a", reference->a, a);
    COMPARE_CPU_BYTE("x", reference->x, x);
    COMPARE_CPU_BYTE("y", reference->y, y);
    COMPARE_CPU_BYTE("sp", reference->sp, sp);
    COMPARE_CPU_BYTE("carry", reference->carry, carry_flag);
    COMPARE_CPU_BYTE("nz", reference->nz, nz_value);

    for (
        index = 0;
        index < sizeof(changed_addresses) / sizeof(changed_addresses[0]);
        index++
    ) {
        const uint16_t address = changed_addresses[index];

        if (reference->ram[address] != ram[address]) {
            char field[32];

            (void)snprintf(
                field,
                sizeof(field),
                "ram[$%04x]",
                (unsigned int)address
            );
            return report_mismatch(
                &context,
                field,
                reference->ram[address],
                ram[address]
            );
        }
    }

    case_count++;
    if ((case_count % RAM_CHECK_INTERVAL) == 0u) {
        return compare_full_ram(reference, &context);
    }
    return 0;

#undef COMPARE_CPU_BYTE
}

static int test_runtime_horizontal_matrix(struct reference_cpu *reference) {
    uint64_t ordinal = 0;
    unsigned int object;
    unsigned int adder_index;
    unsigned int page;
    unsigned int x_position;

    for (object = 0; object < RUNTIME_OBJECT_COUNT; object++) {
        for (adder_index = 0; adder_index < BLOCK_ADDER_COUNT; adder_index++) {
            for (page = 0; page <= UINT8_MAX; page++) {
                for (x_position = 0; x_position <= UINT8_MAX; x_position++) {
                    const uint8_t y_position = (uint8_t)(
                        page * 17u + x_position * 29u +
                        object * 43u + adder_index * 61u
                    );
                    const uint8_t input_a =
                        ((page ^ x_position ^ object ^ adder_index) & 1u) == 0u
                        ? 0u
                        : 0xa5u;
                    struct block_address block;

                    configure_runtime_ram(
                        reference,
                        (uint8_t)object,
                        (uint8_t)page,
                        (uint8_t)x_position,
                        y_position,
                        (uint8_t)ordinal
                    );
                    block = derive_block_address(
                        reference,
                        (uint8_t)object,
                        (uint8_t)adder_index
                    );
                    set_ram_pair(
                        reference,
                        block.address,
                        (uint8_t)(page ^ x_position ^ ordinal)
                    );
                    logical_address_coverage
                        [block.bank][block.column][block.row] = true;
                    set_registers(
                        reference,
                        input_a,
                        (uint8_t)object,
                        (uint8_t)adder_index,
                        (uint8_t)(page + x_position + adder_index),
                        (ordinal & 1u) != 0u,
                        (uint8_t)(ordinal * 101u)
                    );
                    if (run_case(reference, "runtime-horizontal", ordinal)) {
                        return 1;
                    }
                    ordinal++;
                }
            }
        }
    }
    return 0;
}

static int test_runtime_vertical_matrix(struct reference_cpu *reference) {
    uint64_t ordinal = 0;
    unsigned int object;
    unsigned int adder_index;
    unsigned int y_position;
    unsigned int a_mode;

    for (object = 0; object < RUNTIME_OBJECT_COUNT; object++) {
        for (adder_index = 0; adder_index < BLOCK_ADDER_COUNT; adder_index++) {
            for (y_position = 0; y_position <= UINT8_MAX; y_position++) {
                for (a_mode = 0; a_mode < 2; a_mode++) {
                    const uint8_t page = (uint8_t)(
                        y_position * 31u + object * 47u + adder_index
                    );
                    const uint8_t x_position = (uint8_t)(
                        y_position * 67u + object + adder_index * 11u
                    );
                    const uint8_t input_a = a_mode == 0u
                        ? 0u
                        : (uint8_t)(1u + (y_position % 255u));
                    struct block_address block;

                    configure_runtime_ram(
                        reference,
                        (uint8_t)object,
                        page,
                        x_position,
                        (uint8_t)y_position,
                        (uint8_t)(ordinal * 13u)
                    );
                    block = derive_block_address(
                        reference,
                        (uint8_t)object,
                        (uint8_t)adder_index
                    );
                    set_ram_pair(reference, block.address, (uint8_t)y_position);
                    logical_address_coverage
                        [block.bank][block.column][block.row] = true;
                    set_registers(
                        reference,
                        input_a,
                        (uint8_t)object,
                        (uint8_t)adder_index,
                        (uint8_t)(ordinal * 17u),
                        (ordinal & 2u) != 0u,
                        (uint8_t)(ordinal * 89u)
                    );
                    if (run_case(reference, "runtime-vertical", ordinal)) {
                        return 1;
                    }
                    ordinal++;
                }
            }
        }
    }
    return 0;
}

static int test_logical_address_metatile_matrix(
    struct reference_cpu *reference
) {
    uint64_t ordinal = 0;
    unsigned int bank;
    unsigned int column;
    unsigned int row;
    unsigned int metatile;
    const uint8_t object = 0u;
    const uint8_t adder_index = 0u;
    const uint8_t x_adder =
        data[BlockBuffer_X_Adder - 0x8000u + adder_index];
    const uint8_t y_adder =
        data[BlockBuffer_Y_Adder - 0x8000u + adder_index];

    for (bank = 0; bank < BLOCK_BANK_COUNT; bank++) {
        for (column = 0; column < BLOCK_COLUMN_COUNT; column++) {
            for (row = 0; row < BLOCK_ROW_COUNT; row++) {
                const uint8_t x_position =
                    (uint8_t)((column << 4) - x_adder);
                const bool x_carry =
                    (uint16_t)x_position + x_adder > 0xffu;
                const uint8_t page =
                    (uint8_t)((bank - (x_carry ? 1u : 0u)) & 1u);
                const uint8_t desired_y =
                    (uint8_t)((row << 4) + 0x20u);
                const uint8_t y_position = (uint8_t)(desired_y - y_adder);

                for (metatile = 0; metatile <= UINT8_MAX; metatile++) {
                    struct block_address block;

                    configure_runtime_ram(
                        reference,
                        object,
                        page,
                        x_position,
                        y_position,
                        (uint8_t)(ordinal * 23u)
                    );
                    block = derive_block_address(
                        reference,
                        object,
                        adder_index
                    );
                    if (
                        block.bank != bank || block.column != column ||
                        block.row != row
                    ) {
                        fprintf(
                            stderr,
                            "logical-address setup failed: wanted %u/%u/%u, "
                            "derived %u/%u/%u\n",
                            bank,
                            column,
                            row,
                            (unsigned int)block.bank,
                            (unsigned int)block.column,
                            (unsigned int)block.row
                        );
                        return 1;
                    }
                    set_ram_pair(reference, block.address, (uint8_t)metatile);
                    logical_address_coverage[bank][column][row] = true;
                    set_registers(
                        reference,
                        (metatile & 1u) == 0u ? 0u : (uint8_t)metatile,
                        object,
                        adder_index,
                        (uint8_t)(ordinal * 31u),
                        (ordinal & 1u) != 0u,
                        (uint8_t)(ordinal * 79u)
                    );
                    if (run_case(reference, "address-metatile", ordinal)) {
                        return 1;
                    }
                    ordinal++;
                }
            }
        }
    }
    return 0;
}

static int test_accumulator_stack_matrix(struct reference_cpu *reference) {
    uint64_t ordinal = 0;
    unsigned int input_a;
    unsigned int input_sp;

    for (input_a = 0; input_a <= UINT8_MAX; input_a++) {
        for (input_sp = 0; input_sp <= UINT8_MAX; input_sp++) {
            const uint8_t object = (uint8_t)((input_a + input_sp) % 9u);
            const uint8_t adder_index =
                (uint8_t)((input_a * 3u + input_sp) % 28u);
            struct block_address block;

            configure_runtime_ram(
                reference,
                object,
                (uint8_t)(input_a * 41u + input_sp),
                (uint8_t)(input_a + input_sp * 59u),
                (uint8_t)(input_a * 71u + input_sp * 7u),
                (uint8_t)(input_a ^ input_sp)
            );
            block = derive_block_address(reference, object, adder_index);
            set_ram_pair(
                reference,
                block.address,
                (uint8_t)(input_a * 13u + input_sp * 17u)
            );
            set_registers(
                reference,
                (uint8_t)input_a,
                object,
                adder_index,
                (uint8_t)input_sp,
                (ordinal & 1u) != 0u,
                (uint8_t)(ordinal * 53u)
            );
            if (run_case(reference, "accumulator-stack", ordinal)) {
                return 1;
            }
            ordinal++;
        }
    }
    return 0;
}

static int test_incoming_flag_matrix(struct reference_cpu *reference) {
    uint64_t ordinal = 0;
    unsigned int input_carry;
    unsigned int input_nz;

    for (input_carry = 0; input_carry < 2; input_carry++) {
        for (input_nz = 0; input_nz <= UINT8_MAX; input_nz++) {
            const uint8_t object = (uint8_t)(input_nz % 9u);
            const uint8_t adder_index = (uint8_t)(input_nz % 28u);
            struct block_address block;

            configure_runtime_ram(
                reference,
                object,
                (uint8_t)(input_nz * 19u),
                (uint8_t)(input_nz * 43u),
                (uint8_t)(input_nz * 97u),
                (uint8_t)(input_nz * 11u)
            );
            block = derive_block_address(reference, object, adder_index);
            set_ram_pair(reference, block.address, (uint8_t)(input_nz ^ 0xa5u));
            set_registers(
                reference,
                (input_nz & 1u) == 0u ? 0u : 0x5au,
                object,
                adder_index,
                (uint8_t)(input_nz + input_carry * 127u),
                input_carry != 0u,
                (uint8_t)input_nz
            );
            if (run_case(reference, "incoming-flags", ordinal)) {
                return 1;
            }
            ordinal++;
        }
    }
    return 0;
}

static int test_zero_page_alias_matrix(struct reference_cpu *reference) {
    uint64_t ordinal = 0;
    unsigned int input_x;
    unsigned int input_y;

    for (input_x = 0; input_x <= UINT8_MAX; input_x++) {
        for (input_y = 0; input_y <= UINT8_MAX; input_y++) {
            unsigned int address;

            /*
             * For each X, varying Y cycles every zero-page byte through all
             * 256 values. This exposes source/scratch aliases and operation
             * ordering for all wrapped zpx addresses, including non-runtime X.
             */
            for (address = 0; address <= UINT8_MAX; address++) {
                set_ram_pair(
                    reference,
                    (uint8_t)address,
                    (uint8_t)(address * 73u + input_x * 29u + input_y)
                );
            }
            set_registers(
                reference,
                ((input_x ^ input_y) & 1u) == 0u
                    ? 0u
                    : (uint8_t)(input_x + input_y + 1u),
                (uint8_t)input_x,
                (uint8_t)input_y,
                (uint8_t)(input_x * 17u + input_y * 31u),
                (ordinal & 1u) != 0u,
                (uint8_t)(input_x * 101u + input_y * 47u)
            );
            if (run_case(reference, "zero-page-alias", ordinal)) {
                return 1;
            }
            ordinal++;
        }
    }
    return 0;
}

static int verify_logical_address_coverage(void) {
    unsigned int bank;
    unsigned int column;
    unsigned int row;

    for (bank = 0; bank < BLOCK_BANK_COUNT; bank++) {
        for (column = 0; column < BLOCK_COLUMN_COUNT; column++) {
            for (row = 0; row < BLOCK_ROW_COUNT; row++) {
                if (!logical_address_coverage[bank][column][row]) {
                    fprintf(
                        stderr,
                        "missing logical block address %u/%u/%u\n",
                        bank,
                        column,
                        row
                    );
                    return 1;
                }
            }
        }
    }
    return 0;
}

int main(void) {
    struct reference_cpu reference = {0};
    struct case_context final_context = {
        "final-ram-check", 0, 0, 0, 0, 0, false, 0, 0, 0, 0
    };

    initialize_ram(&reference);
    if (test_runtime_horizontal_matrix(&reference)) {
        return 1;
    }
    if (test_runtime_vertical_matrix(&reference)) {
        return 1;
    }
    if (test_logical_address_metatile_matrix(&reference)) {
        return 1;
    }
    if (test_accumulator_stack_matrix(&reference)) {
        return 1;
    }
    if (test_incoming_flag_matrix(&reference)) {
        return 1;
    }
    if (test_zero_page_alias_matrix(&reference)) {
        return 1;
    }
    if (verify_logical_address_coverage()) {
        return 1;
    }
    final_context.ordinal = case_count;
    if (compare_full_ram(&reference, &final_context)) {
        return 1;
    }

    printf(
        "BlockBufferCollision differential: %" PRIu64
        " cases passed (all register domains and 512 logical cells x 256 "
        "metatiles)\n",
        case_count
    );
    return 0;
}
