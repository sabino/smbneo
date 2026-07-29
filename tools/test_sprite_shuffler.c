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

struct coverage {
    uint64_t cases;
    uint64_t adjusted_offsets;
    uint64_t first_add_carries;
    uint64_t wrapped_adjustments;
    uint64_t invalid_table_indices;
};

static void reference_load(uint8_t *reg, uint8_t value, uint8_t *nz) {
    *reg = value;
    *nz = value;
}

static void reference_compare(
    struct reference_cpu *cpu,
    uint8_t lhs,
    uint8_t rhs
) {
    cpu->carry = lhs >= rhs;
    cpu->nz = (uint8_t)(lhs - rhs);
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum = (uint16_t)cpu->a + value + (cpu->carry ? 1u : 0u);

    cpu->carry = sum > 0xffu;
    cpu->a = (uint8_t)sum;
    cpu->nz = cpu->a;
}

/* Literal state oracle for src/smb.asm:SpriteShuffler. */
static void reference_sprite_shuffler(
    struct reference_cpu *cpu,
    struct coverage *coverage
) {
    reference_load(&cpu->y, cpu->ram[AreaType], &cpu->nz);
    reference_load(&cpu->a, 0x28u, &cpu->nz);
    cpu->ram[0x0] = cpu->a;
    reference_load(&cpu->x, 0x0eu, &cpu->nz);

shuffle_loop:
    reference_load(
        &cpu->a,
        cpu->ram[SprDataOffset + cpu->x],
        &cpu->nz
    );
    reference_compare(cpu, cpu->a, cpu->ram[0x0]);
    if (cpu->carry) {
        reference_load(
            &cpu->y,
            cpu->ram[SprShuffleAmtOffset],
            &cpu->nz
        );
        cpu->carry = false;
        reference_adc(cpu, cpu->ram[SprShuffleAmt + cpu->y]);
        coverage->adjusted_offsets++;
        if (cpu->carry) {
            coverage->first_add_carries++;
            cpu->carry = false;
            reference_adc(cpu, cpu->ram[0x0]);
            if (cpu->carry) {
                coverage->wrapped_adjustments++;
            }
        }
        cpu->ram[SprDataOffset + cpu->x] = cpu->a;
    }
    cpu->x = (uint8_t)(cpu->x - 1u);
    cpu->nz = cpu->x;
    if ((cpu->nz & 0x80u) == 0u) {
        goto shuffle_loop;
    }

    reference_load(
        &cpu->x,
        cpu->ram[SprShuffleAmtOffset],
        &cpu->nz
    );
    cpu->x = (uint8_t)(cpu->x + 1u);
    cpu->nz = cpu->x;
    reference_compare(cpu, cpu->x, 0x03u);
    if (cpu->nz == 0u) {
        reference_load(&cpu->x, 0x00u, &cpu->nz);
    }
    cpu->ram[SprShuffleAmtOffset] = cpu->x;
    reference_load(&cpu->x, 0x08u, &cpu->nz);
    reference_load(&cpu->y, 0x02u, &cpu->nz);

set_misc_offset:
    reference_load(
        &cpu->a,
        cpu->ram[SprDataOffset + 5u + cpu->y],
        &cpu->nz
    );
    cpu->ram[Misc_SprDataOffset - 2u + cpu->x] = cpu->a;
    cpu->carry = false;
    reference_adc(cpu, 0x08u);
    cpu->ram[Misc_SprDataOffset - 1u + cpu->x] = cpu->a;
    cpu->carry = false;
    reference_adc(cpu, 0x08u);
    cpu->ram[Misc_SprDataOffset + cpu->x] = cpu->a;
    cpu->x = (uint8_t)(cpu->x - 1u);
    cpu->nz = cpu->x;
    cpu->x = (uint8_t)(cpu->x - 1u);
    cpu->nz = cpu->x;
    cpu->x = (uint8_t)(cpu->x - 1u);
    cpu->nz = cpu->x;
    cpu->y = (uint8_t)(cpu->y - 1u);
    cpu->nz = cpu->y;
    if ((cpu->nz & 0x80u) == 0u) {
        goto set_misc_offset;
    }
}

static void initialize_ram(
    struct reference_cpu *reference,
    uint8_t salt,
    uint8_t shuffle_index,
    uint8_t shuffle_amount,
    int varied_slot,
    uint8_t varied_value
) {
    uint16_t address;
    uint8_t slot;

    for (address = 0; address < RAM_SIZE; address++) {
        const uint8_t value = (uint8_t)(
            salt + address * 37u + (address >> 8) * 19u
        );

        reference->ram[address] = value;
        ram[address] = value;
    }
    for (slot = 0u; slot < 15u; slot++) {
        const uint8_t value = (uint8_t)(salt * 13u + slot * 29u);

        reference->ram[SprDataOffset + slot] = value;
        ram[SprDataOffset + slot] = value;
    }
    if (varied_slot >= 0) {
        reference->ram[SprDataOffset + (uint8_t)varied_slot] = varied_value;
        ram[SprDataOffset + (uint8_t)varied_slot] = varied_value;
    }
    reference->ram[SprShuffleAmtOffset] = shuffle_index;
    ram[SprShuffleAmtOffset] = shuffle_index;
    reference->ram[SprShuffleAmt + shuffle_index] = shuffle_amount;
    ram[SprShuffleAmt + shuffle_index] = shuffle_amount;
}

static int report_mismatch(
    uint64_t ordinal,
    uint8_t shuffle_index,
    uint8_t shuffle_amount,
    int varied_slot,
    uint8_t varied_value,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "SpriteShuffler case=%" PRIu64
        " index=%02x amount=%02x slot=%d value=%02x %s: "
        "expected %02x, got %02x\n",
        ordinal,
        (unsigned int)shuffle_index,
        (unsigned int)shuffle_amount,
        varied_slot,
        (unsigned int)varied_value,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int run_case(
    struct reference_cpu *reference,
    struct coverage *coverage,
    uint8_t salt,
    uint8_t shuffle_index,
    uint8_t shuffle_amount,
    int varied_slot,
    uint8_t varied_value
) {
    const uint64_t ordinal = coverage->cases;
    uint16_t address;

    initialize_ram(
        reference,
        salt,
        shuffle_index,
        shuffle_amount,
        varied_slot,
        varied_value
    );
    reference->a = (uint8_t)(salt + 0x11u);
    reference->x = (uint8_t)(salt + 0x33u);
    reference->y = (uint8_t)(salt + 0x55u);
    reference->sp = (uint8_t)(salt ^ 0xa5u);
    reference->carry = (salt & 1u) != 0u;
    reference->nz = (uint8_t)(salt + 0x77u);
    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    if (shuffle_index > 2u) {
        coverage->invalid_table_indices++;
    }
    reference_sprite_shuffler(reference, coverage);
    SpriteShuffler();

#define COMPARE_BYTE(field_name, expected_value, actual_value) \
    do { \
        if ((uint8_t)(expected_value) != (uint8_t)(actual_value)) { \
            return report_mismatch( \
                ordinal, shuffle_index, shuffle_amount, varied_slot, \
                varied_value, field_name, (uint8_t)(expected_value), \
                (uint8_t)(actual_value) \
            ); \
        } \
    } while (0)

    COMPARE_BYTE("a", reference->a, a);
    COMPARE_BYTE("x", reference->x, x);
    COMPARE_BYTE("y", reference->y, y);
    COMPARE_BYTE("sp", reference->sp, sp);
    COMPARE_BYTE("carry", reference->carry, carry_flag);
    COMPARE_BYTE("nz", reference->nz, nz_value);
    if (memcmp(reference->ram, ram, RAM_SIZE) != 0) {
        for (address = 0; address < RAM_SIZE; address++) {
            if (reference->ram[address] != ram[address]) {
                char field[32];

                (void)snprintf(field, sizeof(field), "ram[$%04x]", address);
                return report_mismatch(
                    ordinal,
                    shuffle_index,
                    shuffle_amount,
                    varied_slot,
                    varied_value,
                    field,
                    reference->ram[address],
                    ram[address]
                );
            }
        }
    }

#undef COMPARE_BYTE

    coverage->cases++;
    return 0;
}

int main(void) {
    struct reference_cpu reference;
    struct coverage coverage = {0};
    unsigned int shuffle_index;
    unsigned int shuffle_amount;
    unsigned int varied_value;
    int varied_slot;

    /* Exhaust all arithmetic pairs for each valid three-entry table index. */
    for (shuffle_index = 0u; shuffle_index < 3u; shuffle_index++) {
        for (shuffle_amount = 0u; shuffle_amount < 256u; shuffle_amount++) {
            for (varied_value = 0u; varied_value < 256u; varied_value++) {
                if (run_case(
                        &reference,
                        &coverage,
                        (uint8_t)(shuffle_index * 53u + shuffle_amount),
                        (uint8_t)shuffle_index,
                        (uint8_t)shuffle_amount,
                        (int)((shuffle_amount + varied_value) % 15u),
                        (uint8_t)varied_value
                    ) != 0) {
                    return 1;
                }
            }
        }
    }

    /* Exercise every RAM-selecting index, including aliasing fallback states. */
    for (shuffle_index = 3u; shuffle_index < 256u; shuffle_index++) {
        for (varied_value = 0u; varied_value < 256u; varied_value++) {
            if (run_case(
                    &reference,
                    &coverage,
                    (uint8_t)(shuffle_index ^ varied_value),
                    (uint8_t)shuffle_index,
                    (uint8_t)(varied_value * 17u),
                    (int)(varied_value % 15u),
                    (uint8_t)varied_value
                ) != 0) {
                return 1;
            }
        }
    }

    /* Give every one of the fifteen transformed slots boundary coverage. */
    for (varied_slot = 0; varied_slot < 15; varied_slot++) {
        static const uint8_t boundary_amounts[] = {
            0x00u, 0x01u, 0x27u, 0x28u, 0x7fu, 0xd8u, 0xffu
        };
        size_t boundary_index;

        for (boundary_index = 0u;
             boundary_index < sizeof(boundary_amounts);
             boundary_index++) {
            for (varied_value = 0u; varied_value < 256u; varied_value++) {
                if (run_case(
                        &reference,
                        &coverage,
                        (uint8_t)(varied_slot * 11 + boundary_index),
                        (uint8_t)(boundary_index % 3u),
                        boundary_amounts[boundary_index],
                        varied_slot,
                        (uint8_t)varied_value
                    ) != 0) {
                    return 1;
                }
            }
        }
    }

    if (coverage.adjusted_offsets == 0u ||
        coverage.first_add_carries == 0u ||
        coverage.wrapped_adjustments == 0u ||
        coverage.invalid_table_indices == 0u) {
        fputs("SpriteShuffler differential coverage incomplete\n", stderr);
        return 1;
    }
    printf(
        "SpriteShuffler differential regression: PASS (%" PRIu64
        " cases, %" PRIu64 " adjusted, %" PRIu64
        " first-carry, %" PRIu64 " wrapped, %" PRIu64
        " invalid-index)\n",
        coverage.cases,
        coverage.adjusted_offsets,
        coverage.first_add_carries,
        coverage.wrapped_adjustments,
        coverage.invalid_table_indices
    );
    return 0;
}
