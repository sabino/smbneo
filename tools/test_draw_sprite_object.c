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
    SCRATCH_BYTES = 6,
    OAM_ROW_BYTES = 8,
    OAM_GUARD_BYTES = 10,
    FULL_RAM_COMPARE_INTERVAL = 256
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

struct draw_input {
    uint8_t first_tile;
    uint8_t second_tile;
    uint8_t row_y;
    uint8_t flip;
    uint8_t attributes;
    uint8_t row_x;
    uint8_t register_x;
    uint8_t register_y;
};

struct coverage {
    uint64_t cases;
    uint64_t flipped;
    uint64_t unflipped;
    uint64_t y_carry;
    uint64_t y_no_carry;
    uint64_t x_zero;
    uint64_t x_negative;
    uint64_t x_positive;
    uint64_t row_x_wrap;
    uint64_t row_y_wrap;
};

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    cpu->nz = value;
}

static void reference_lsr(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 0x01u) != 0u;
    cpu->a >>= 1;
    cpu->nz = cpu->a;
}

static void reference_ora(struct reference_cpu *cpu, uint8_t value) {
    cpu->a |= value;
    cpu->nz = cpu->a;
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t result = (uint16_t)cpu->a + value +
        (cpu->carry ? 1u : 0u);

    cpu->a = (uint8_t)result;
    cpu->carry = result > UINT8_MAX;
    cpu->nz = cpu->a;
}

static void reference_tya(struct reference_cpu *cpu) {
    reference_lda(cpu, cpu->y);
}

static void reference_tay(struct reference_cpu *cpu) {
    cpu->y = cpu->a;
    cpu->nz = cpu->y;
}

static void reference_inx(struct reference_cpu *cpu) {
    cpu->x = (uint8_t)(cpu->x + 1u);
    cpu->nz = cpu->x;
}

/*
 * Literal 6502-state oracle for src/smb.asm:DrawSpriteObject. Keep this
 * instruction-shaped and independent of the optimized C so the regression
 * observes accumulator, index, carry, NZ, scratch RAM, and OAM differences.
 */
static void reference_draw_sprite_object(struct reference_cpu *cpu) {
    reference_lda(cpu, cpu->ram[0x03]);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lda(cpu, cpu->ram[0x00]);
    if (!cpu->carry) {
        goto no_horizontal_flip;
    }
    cpu->ram[Sprite_Tilenumber + 4u + cpu->y] = cpu->a;
    reference_lda(cpu, cpu->ram[0x01]);
    cpu->ram[Sprite_Tilenumber + cpu->y] = cpu->a;
    reference_lda(cpu, 0x40);
    goto set_horizontal_flip_attribute;

no_horizontal_flip:
    cpu->ram[Sprite_Tilenumber + cpu->y] = cpu->a;
    reference_lda(cpu, cpu->ram[0x01]);
    cpu->ram[Sprite_Tilenumber + 4u + cpu->y] = cpu->a;
    reference_lda(cpu, 0x00);

set_horizontal_flip_attribute:
    reference_ora(cpu, cpu->ram[0x04]);
    cpu->ram[Sprite_Attributes + cpu->y] = cpu->a;
    cpu->ram[Sprite_Attributes + 4u + cpu->y] = cpu->a;
    reference_lda(cpu, cpu->ram[0x02]);
    cpu->ram[Sprite_Y_Position + cpu->y] = cpu->a;
    cpu->ram[Sprite_Y_Position + 4u + cpu->y] = cpu->a;
    reference_lda(cpu, cpu->ram[0x05]);
    cpu->ram[Sprite_X_Position + cpu->y] = cpu->a;
    cpu->carry = false;
    reference_adc(cpu, 0x08);
    cpu->ram[Sprite_X_Position + 4u + cpu->y] = cpu->a;
    reference_lda(cpu, cpu->ram[0x02]);
    cpu->carry = false;
    reference_adc(cpu, 0x08);
    cpu->ram[0x02] = cpu->a;
    reference_tya(cpu);
    cpu->carry = false;
    reference_adc(cpu, 0x08);
    reference_tay(cpu);
    reference_inx(cpu);
    reference_inx(cpu);
}

static void set_ram_pair(
    struct reference_cpu *reference,
    uint16_t address,
    uint8_t value
) {
    reference->ram[address] = value;
    ram[address] = value;
}

static int report_mismatch(
    const char *suite,
    uint64_t ordinal,
    const struct draw_input *input,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu64 " tiles=%02x/%02x scratch-yx=%02x/%02x "
        "flip=%02x attr=%02x registers-xy=%02x/%02x %s: expected "
        "0x%02x, got 0x%02x\n",
        suite,
        ordinal,
        (unsigned int)input->first_tile,
        (unsigned int)input->second_tile,
        (unsigned int)input->row_y,
        (unsigned int)input->row_x,
        (unsigned int)input->flip,
        (unsigned int)input->attributes,
        (unsigned int)input->register_x,
        (unsigned int)input->register_y,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int compare_full_ram(
    const struct reference_cpu *reference,
    const char *suite,
    uint64_t ordinal,
    const struct draw_input *input
) {
    uint16_t address;

    if (memcmp(reference->ram, ram, RAM_SIZE) == 0) {
        return 0;
    }
    for (address = 0; address < RAM_SIZE; address++) {
        if (reference->ram[address] != ram[address]) {
            char field[32];

            (void)snprintf(field, sizeof(field), "ram[$%04x]", address);
            return report_mismatch(
                suite,
                ordinal,
                input,
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
    struct coverage *coverage,
    const char *suite,
    const struct draw_input *input
) {
    const uint64_t ordinal = coverage->cases;
    const uint8_t poison = (uint8_t)(
        ordinal * UINT64_C(73) + input->first_tile * 29u +
        input->second_tile * 17u + input->flip * 7u + input->register_y
    );
    const uint16_t oam_guard_start = (uint16_t)(
        Sprite_Y_Position + input->register_y - 1u
    );
    uint16_t offset;

    reference->a = poison;
    reference->x = input->register_x;
    reference->y = input->register_y;
    reference->sp = (uint8_t)(poison ^ 0xa5u);
    reference->carry = (poison & 0x01u) != 0u;
    reference->nz = (uint8_t)(poison ^ 0x5au);
    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    set_ram_pair(reference, 0x00, input->first_tile);
    set_ram_pair(reference, 0x01, input->second_tile);
    set_ram_pair(reference, 0x02, input->row_y);
    set_ram_pair(reference, 0x03, input->flip);
    set_ram_pair(reference, 0x04, input->attributes);
    set_ram_pair(reference, 0x05, input->row_x);
    for (offset = 0; offset < OAM_GUARD_BYTES; offset++) {
        set_ram_pair(
            reference,
            (uint16_t)(oam_guard_start + offset),
            (uint8_t)(poison + offset * 31u)
        );
    }

    reference_draw_sprite_object(reference);
    DrawSpriteObject();

#define COMPARE_BYTE(field_name, expected_value, actual_value) \
    do { \
        if ((uint8_t)(expected_value) != (uint8_t)(actual_value)) { \
            return report_mismatch( \
                suite, ordinal, input, field_name, \
                (uint8_t)(expected_value), (uint8_t)(actual_value) \
            ); \
        } \
    } while (0)

    COMPARE_BYTE("a", reference->a, a);
    COMPARE_BYTE("x", reference->x, x);
    COMPARE_BYTE("y", reference->y, y);
    COMPARE_BYTE("sp", reference->sp, sp);
    COMPARE_BYTE("carry", reference->carry, carry_flag);
    COMPARE_BYTE("nz", reference->nz, nz_value);
    for (offset = 0; offset < SCRATCH_BYTES; offset++) {
        char field[24];

        (void)snprintf(field, sizeof(field), "scratch-$%02x", offset);
        COMPARE_BYTE(field, reference->ram[offset], ram[offset]);
    }
    for (offset = 0; offset < OAM_GUARD_BYTES; offset++) {
        const uint16_t address = (uint16_t)(oam_guard_start + offset);
        char field[32];

        (void)snprintf(field, sizeof(field), "oam-guard-$%04x", address);
        COMPARE_BYTE(field, reference->ram[address], ram[address]);
    }

#undef COMPARE_BYTE

    coverage->cases++;
    if ((input->flip & 0x02u) != 0u) {
        coverage->flipped++;
    } else {
        coverage->unflipped++;
    }
    if (reference->carry) {
        coverage->y_carry++;
    } else {
        coverage->y_no_carry++;
    }
    if (reference->nz == 0u) {
        coverage->x_zero++;
    } else if ((reference->nz & 0x80u) != 0u) {
        coverage->x_negative++;
    } else {
        coverage->x_positive++;
    }
    if (input->row_x >= 0xf8u) {
        coverage->row_x_wrap++;
    }
    if (input->row_y >= 0xf8u) {
        coverage->row_y_wrap++;
    }

    if ((coverage->cases % FULL_RAM_COMPARE_INTERVAL) == 0u) {
        return compare_full_ram(reference, suite, ordinal, input);
    }
    return 0;
}

static int finish_suite(
    const struct reference_cpu *reference,
    const struct coverage *coverage,
    const char *suite,
    const struct draw_input *last_input
) {
    return compare_full_ram(
        reference,
        suite,
        coverage->cases - 1u,
        last_input
    );
}

static int run_tile_flip_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct draw_input input = {
        .row_y = 0xf8,
        .attributes = 0x95,
        .row_x = 0xfc,
        .register_x = 0xfe,
        .register_y = 0xf7
    };
    uint32_t flip;
    uint32_t first_tile;
    uint32_t second_tile;

    for (flip = 0; flip <= UINT8_MAX; flip++) {
        input.flip = (uint8_t)flip;
        for (first_tile = 0; first_tile <= UINT8_MAX; first_tile++) {
            input.first_tile = (uint8_t)first_tile;
            for (second_tile = 0; second_tile <= UINT8_MAX; second_tile++) {
                input.second_tile = (uint8_t)second_tile;
                if (run_case(
                    reference,
                    coverage,
                    "tile-flip",
                    &input
                ) != 0) {
                    return 1;
                }
            }
        }
    }
    return finish_suite(reference, coverage, "tile-flip", &input);
}

static int run_attribute_flip_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct draw_input input = {
        .first_tile = 0x00,
        .second_tile = 0xff,
        .row_y = 0xf7,
        .row_x = 0xf8,
        .register_x = 0xfd,
        .register_y = 0xf8
    };
    uint32_t flip;
    uint32_t attributes;

    for (flip = 0; flip <= UINT8_MAX; flip++) {
        input.flip = (uint8_t)flip;
        for (attributes = 0; attributes <= UINT8_MAX; attributes++) {
            input.attributes = (uint8_t)attributes;
            if (run_case(
                reference,
                coverage,
                "attribute-flip",
                &input
            ) != 0) {
                return 1;
            }
        }
    }
    return finish_suite(reference, coverage, "attribute-flip", &input);
}

static int run_coordinate_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct draw_input input = {
        .first_tile = 0x5a,
        .second_tile = 0xa5,
        .flip = 0x02,
        .attributes = 0x40,
        .register_x = 0xff,
        .register_y = 0xff
    };
    uint32_t row_y;
    uint32_t row_x;

    for (row_y = 0; row_y <= UINT8_MAX; row_y++) {
        input.row_y = (uint8_t)row_y;
        for (row_x = 0; row_x <= UINT8_MAX; row_x++) {
            input.row_x = (uint8_t)row_x;
            if (run_case(
                reference,
                coverage,
                "coordinates",
                &input
            ) != 0) {
                return 1;
            }
        }
    }
    return finish_suite(reference, coverage, "coordinates", &input);
}

static int run_register_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct draw_input input = {
        .first_tile = 0x81,
        .second_tile = 0x7e,
        .row_y = 0x00,
        .attributes = 0xff,
        .row_x = 0xff
    };
    uint32_t register_y;
    uint32_t register_x;

    for (register_y = 0; register_y <= UINT8_MAX; register_y++) {
        input.register_y = (uint8_t)register_y;
        input.flip = (uint8_t)(register_y ^ 0xa5u);
        for (register_x = 0; register_x <= UINT8_MAX; register_x++) {
            input.register_x = (uint8_t)register_x;
            if (run_case(
                reference,
                coverage,
                "registers",
                &input
            ) != 0) {
                return 1;
            }
        }
    }
    return finish_suite(reference, coverage, "registers", &input);
}

int main(void) {
    struct reference_cpu reference;
    struct coverage coverage = {0};

    memset(&reference, 0, sizeof(reference));
    memset(ram, 0, sizeof(ram));

    if (run_tile_flip_exhaustion(&reference, &coverage) != 0 ||
        run_attribute_flip_exhaustion(&reference, &coverage) != 0 ||
        run_coordinate_exhaustion(&reference, &coverage) != 0 ||
        run_register_exhaustion(&reference, &coverage) != 0) {
        return 1;
    }
    if (coverage.flipped == 0u || coverage.unflipped == 0u ||
        coverage.y_carry == 0u || coverage.y_no_carry == 0u ||
        coverage.x_zero == 0u || coverage.x_negative == 0u ||
        coverage.x_positive == 0u || coverage.row_x_wrap == 0u ||
        coverage.row_y_wrap == 0u) {
        fputs("DrawSpriteObject differential coverage is incomplete\n", stderr);
        return 1;
    }

    printf(
        "DrawSpriteObject differential regression: PASS (%" PRIu64
        " cases; flip=%" PRIu64 "/%" PRIu64
        ", y-carry=%" PRIu64 "/%" PRIu64
        ", x-nz=zero:%" PRIu64 "/negative:%" PRIu64
        "/positive:%" PRIu64 ", coordinate-wrap=%" PRIu64
        "/%" PRIu64 ")\n",
        coverage.cases,
        coverage.unflipped,
        coverage.flipped,
        coverage.y_no_carry,
        coverage.y_carry,
        coverage.x_zero,
        coverage.x_negative,
        coverage.x_positive,
        coverage.row_y_wrap,
        coverage.row_x_wrap
    );
    return 0;
}
