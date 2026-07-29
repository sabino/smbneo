#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "data.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum axis {
    AXIS_X,
    AXIS_Y
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

struct offscreen_input {
    enum axis axis;
    uint8_t object_index;
    uint8_t object_pixel;
    uint8_t object_high;
    uint8_t edge_pixel[2];
    uint8_t edge_high[2];
    uint32_t tag;
};

struct coverage {
    uint64_t cases;
    uint64_t x_cases;
    uint64_t y_cases;
    uint64_t scratch_divides;
    bool x_bits[UINT8_MAX + 1u];
    bool y_bits[UINT8_MAX + 1u];
    bool final_side[UINT8_MAX + 1u];
};

static uint8_t reference_read_byte(
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

static void reference_sbc(struct reference_cpu *cpu, uint8_t value) {
    const int16_t difference = (int16_t)cpu->a - value -
        (cpu->carry ? 0 : 1);

    cpu->carry = difference >= 0;
    cpu->a = (uint8_t)difference;
    cpu->nz = cpu->a;
}

static void reference_cmp(struct reference_cpu *cpu, uint8_t value) {
    cpu->carry = cpu->a >= value;
    cpu->nz = (uint8_t)(cpu->a - value);
}

static void reference_lsr(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 1u) != 0u;
    cpu->a >>= 1;
    cpu->nz = cpu->a;
}

static void reference_and(struct reference_cpu *cpu, uint8_t value) {
    cpu->a &= value;
    cpu->nz = cpu->a;
}

static void reference_cpy(struct reference_cpu *cpu, uint8_t value) {
    cpu->carry = cpu->y >= value;
    cpu->nz = (uint8_t)(cpu->y - value);
}

static void reference_adc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t sum = (uint16_t)cpu->a + value +
        (cpu->carry ? 1u : 0u);

    cpu->carry = sum > UINT8_MAX;
    cpu->a = (uint8_t)sum;
    cpu->nz = cpu->a;
}

static void reference_tax(struct reference_cpu *cpu) {
    reference_ldx(cpu, cpu->a);
}

static void reference_dey(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y - 1u);
    cpu->nz = cpu->y;
}

/* Literal instruction-shaped oracle for src/smb.asm:DividePDiff. */
static void reference_divide_pdiff(struct reference_cpu *cpu) {
    cpu->ram[0x05] = cpu->a;
    reference_lda(cpu, cpu->ram[0x07]);
    reference_cmp(cpu, cpu->ram[0x06]);
    if (cpu->carry) {
        return;
    }
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_lsr(cpu);
    reference_and(cpu, 0x07);
    reference_cpy(cpu, 0x01);
    if (!cpu->carry) {
        reference_adc(cpu, cpu->ram[0x05]);
    }
    reference_tax(cpu);
}

/* Literal instruction-shaped oracle for src/smb.asm:GetXOffscreenBits. */
static void reference_get_x_offscreen_bits(struct reference_cpu *cpu) {
    cpu->ram[0x04] = cpu->x;
    reference_ldy(cpu, 0x01);

x_offscreen_loop:
    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(ScreenEdge_X_Pos + cpu->y))
    );
    cpu->carry = true;
    reference_sbc(
        cpu,
        reference_read_byte(
            cpu,
            (uint8_t)(SprObject_X_Position + cpu->x)
        )
    );
    cpu->ram[0x07] = cpu->a;
    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(ScreenEdge_PageLoc + cpu->y))
    );
    reference_sbc(
        cpu,
        reference_read_byte(cpu, (uint8_t)(SprObject_PageLoc + cpu->x))
    );
    reference_ldx(
        cpu,
        reference_read_byte(
            cpu,
            (uint16_t)(DefaultXOnscreenOfs + cpu->y)
        )
    );
    reference_cmp(cpu, 0x00);
    if ((cpu->nz & 0x80u) == 0u) {
        reference_ldx(
            cpu,
            reference_read_byte(
                cpu,
                (uint16_t)(DefaultXOnscreenOfs + 1u + cpu->y)
            )
        );
        reference_cmp(cpu, 0x01);
        if ((cpu->nz & 0x80u) != 0u) {
            reference_lda(cpu, 0x38);
            cpu->ram[0x06] = cpu->a;
            reference_lda(cpu, 0x08);
            reference_divide_pdiff(cpu);
        }
    }
    reference_lda(
        cpu,
        reference_read_byte(
            cpu,
            (uint16_t)(XOffscreenBitsData + cpu->x)
        )
    );
    reference_ldx(cpu, cpu->ram[0x04]);
    reference_cmp(cpu, 0x00);
    if (cpu->nz == 0u) {
        reference_dey(cpu);
        if ((cpu->nz & 0x80u) == 0u) {
            goto x_offscreen_loop;
        }
    }
}

/* Literal instruction-shaped oracle for src/smb.asm:GetYOffscreenBits. */
static void reference_get_y_offscreen_bits(struct reference_cpu *cpu) {
    cpu->ram[0x04] = cpu->x;
    reference_ldy(cpu, 0x01);

y_offscreen_loop:
    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(HighPosUnitData + cpu->y))
    );
    cpu->carry = true;
    reference_sbc(
        cpu,
        reference_read_byte(
            cpu,
            (uint8_t)(SprObject_Y_Position + cpu->x)
        )
    );
    cpu->ram[0x07] = cpu->a;
    reference_lda(cpu, 0x01);
    reference_sbc(
        cpu,
        reference_read_byte(
            cpu,
            (uint8_t)(SprObject_Y_HighPos + cpu->x)
        )
    );
    reference_ldx(
        cpu,
        reference_read_byte(
            cpu,
            (uint16_t)(DefaultYOnscreenOfs + cpu->y)
        )
    );
    reference_cmp(cpu, 0x00);
    if ((cpu->nz & 0x80u) == 0u) {
        reference_ldx(
            cpu,
            reference_read_byte(
                cpu,
                (uint16_t)(DefaultYOnscreenOfs + 1u + cpu->y)
            )
        );
        reference_cmp(cpu, 0x01);
        if ((cpu->nz & 0x80u) != 0u) {
            reference_lda(cpu, 0x20);
            cpu->ram[0x06] = cpu->a;
            reference_lda(cpu, 0x04);
            reference_divide_pdiff(cpu);
        }
    }
    reference_lda(
        cpu,
        reference_read_byte(
            cpu,
            (uint16_t)(YOffscreenBitsData + cpu->x)
        )
    );
    reference_ldx(cpu, cpu->ram[0x04]);
    reference_cmp(cpu, 0x00);
    if (cpu->nz == 0u) {
        reference_dey(cpu);
        if ((cpu->nz & 0x80u) == 0u) {
            goto y_offscreen_loop;
        }
    }
}

static int report_mismatch(
    const struct offscreen_input *input,
    uint64_t ordinal,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%c-offscreen case=%" PRIu64 " tag=%08" PRIx32
        " index=%02x object=%02x:%02x %s: expected %02x, got %02x\n",
        input->axis == AXIS_X ? 'X' : 'Y',
        ordinal,
        input->tag,
        (unsigned int)input->object_index,
        (unsigned int)input->object_high,
        (unsigned int)input->object_pixel,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static void set_ram_pair(
    struct reference_cpu *reference,
    uint16_t address,
    uint8_t value
) {
    reference->ram[address] = value;
    ram[address] = value;
}

static int run_case(
    struct reference_cpu *reference,
    struct coverage *coverage,
    const struct offscreen_input *input
) {
    const uint64_t ordinal = coverage->cases;
    uint16_t address;
    const uint8_t initial_scratch_5 = (uint8_t)(0xa5u ^ input->tag);
    const uint8_t initial_scratch_6 = (uint8_t)(0x5au ^ (input->tag >> 8));

    for (address = 0u; address < RAM_SIZE; address++) {
        const uint8_t poison = (uint8_t)(
            address * 37u + ordinal * UINT64_C(73) + input->tag * 11u
        );

        reference->ram[address] = poison;
        ram[address] = poison;
    }

    if (input->axis == AXIS_X) {
        set_ram_pair(
            reference,
            (uint8_t)(SprObject_X_Position + input->object_index),
            input->object_pixel
        );
        set_ram_pair(
            reference,
            (uint8_t)(SprObject_PageLoc + input->object_index),
            input->object_high
        );
        set_ram_pair(reference, ScreenEdge_X_Pos, input->edge_pixel[0]);
        set_ram_pair(reference, ScreenEdge_X_Pos + 1u, input->edge_pixel[1]);
        set_ram_pair(reference, ScreenEdge_PageLoc, input->edge_high[0]);
        set_ram_pair(reference, ScreenEdge_PageLoc + 1u, input->edge_high[1]);
    } else {
        set_ram_pair(
            reference,
            (uint8_t)(SprObject_Y_Position + input->object_index),
            input->object_pixel
        );
        set_ram_pair(
            reference,
            (uint8_t)(SprObject_Y_HighPos + input->object_index),
            input->object_high
        );
    }
    set_ram_pair(reference, 0x05, initial_scratch_5);
    set_ram_pair(reference, 0x06, initial_scratch_6);

    reference->a = (uint8_t)(0x3cu + input->tag);
    reference->x = input->object_index;
    reference->y = (uint8_t)(0xc3u - input->tag);
    reference->sp = (uint8_t)(0xfdu - input->tag);
    reference->carry = (input->tag & 1u) != 0u;
    reference->nz = (uint8_t)(0x81u ^ input->tag);
    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    if (input->axis == AXIS_X) {
        reference_get_x_offscreen_bits(reference);
        GetXOffscreenBits();
        coverage->x_cases++;
        coverage->x_bits[a] = true;
    } else {
        reference_get_y_offscreen_bits(reference);
        GetYOffscreenBits();
        coverage->y_cases++;
        coverage->y_bits[a] = true;
    }
    coverage->cases++;
    coverage->final_side[y] = true;
    if (ram[0x05] != initial_scratch_5 || ram[0x06] != initial_scratch_6) {
        coverage->scratch_divides++;
    }

#define CHECK_FIELD(name, expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            return report_mismatch( \
                input, ordinal, name, (uint8_t)(expected), (uint8_t)(actual) \
            ); \
        } \
    } while (0)

    CHECK_FIELD("A", reference->a, a);
    CHECK_FIELD("X", reference->x, x);
    CHECK_FIELD("Y", reference->y, y);
    CHECK_FIELD("SP", reference->sp, sp);
    CHECK_FIELD("carry", reference->carry, carry_flag);
    CHECK_FIELD("NZ", reference->nz, nz_value);
    CHECK_FIELD("ram[$04]", reference->ram[0x04], ram[0x04]);
    CHECK_FIELD("ram[$05]", reference->ram[0x05], ram[0x05]);
    CHECK_FIELD("ram[$06]", reference->ram[0x06], ram[0x06]);
    CHECK_FIELD("ram[$07]", reference->ram[0x07], ram[0x07]);
#undef CHECK_FIELD

    if ((ordinal & 0x3fu) == 0u &&
        memcmp(reference->ram, ram, RAM_SIZE) != 0) {
        for (address = 0u; address < RAM_SIZE; address++) {
            if (reference->ram[address] != ram[address]) {
                char field[32];

                (void)snprintf(field, sizeof(field), "ram[$%04x]", address);
                return report_mismatch(
                    input,
                    ordinal,
                    field,
                    reference->ram[address],
                    ram[address]
                );
            }
        }
    }
    return 0;
}

static void split_coordinate(
    uint16_t coordinate,
    uint8_t *pixel,
    uint8_t *high
) {
    *pixel = (uint8_t)coordinate;
    *high = (uint8_t)(coordinate >> 8);
}

static int run_x_exhaustive(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t difference;

    for (difference = 0u; difference <= UINT16_MAX; difference++) {
        struct offscreen_input input = {
            .axis = AXIS_X,
            .object_index = 0u,
            .object_pixel = 0u,
            .object_high = 0u,
            .edge_pixel = { 0u, (uint8_t)difference },
            .edge_high = { 0u, (uint8_t)(difference >> 8) },
            .tag = difference
        };

        if (run_case(reference, coverage, &input) != 0) {
            return 1;
        }
    }

    for (difference = 0u; difference <= UINT16_MAX; difference++) {
        const uint16_t object = 0x3456u;
        const uint16_t right = (uint16_t)(object + 0x0100u);
        const uint16_t left = (uint16_t)(object + difference);
        struct offscreen_input input = {
            .axis = AXIS_X,
            .object_index = 0x5au,
            .object_pixel = (uint8_t)object,
            .object_high = (uint8_t)(object >> 8),
            .tag = 0x10000u + difference
        };

        split_coordinate(left, &input.edge_pixel[0], &input.edge_high[0]);
        split_coordinate(right, &input.edge_pixel[1], &input.edge_high[1]);
        if (run_case(reference, coverage, &input) != 0) {
            return 1;
        }
    }
    return 0;
}

static int run_index_boundaries(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    static const uint16_t x_differences[] = {
        0x0000u, 0x0001u, 0x0007u, 0x0008u, 0x000fu,
        0x0010u, 0x0017u, 0x0018u, 0x001fu, 0x0020u,
        0x0027u, 0x0028u, 0x002fu, 0x0030u, 0x0037u,
        0x0038u, 0x0039u, 0x00ffu, 0x0100u, 0x7fffu,
        0x8000u, 0xfffeu, 0xffffu
    };
    static const uint16_t y_coordinates[] = {
        0x0000u, 0x0001u, 0x00d8u, 0x00dfu, 0x00e0u,
        0x00e1u, 0x00e8u, 0x00f0u, 0x00f8u, 0x00ffu,
        0x0100u, 0x0101u, 0x0107u, 0x0108u, 0x0110u,
        0x0118u, 0x0120u, 0x017fu, 0x0180u, 0x01ffu,
        0x0200u, 0x7fffu, 0x8000u, 0xffffu
    };
    uint16_t index;
    size_t boundary;

    for (index = 0u; index <= UINT8_MAX; index++) {
        for (boundary = 0u;
             boundary < sizeof(x_differences) / sizeof(x_differences[0]);
             boundary++) {
            const uint16_t object = (uint16_t)(0xff31u + index * 257u);
            const uint16_t right = (uint16_t)(object + x_differences[boundary]);
            const uint16_t left = (uint16_t)(object + 0x0100u);
            struct offscreen_input input = {
                .axis = AXIS_X,
                .object_index = (uint8_t)index,
                .object_pixel = (uint8_t)object,
                .object_high = (uint8_t)(object >> 8),
                .tag = 0x20000u + index * 0x100u + (uint32_t)boundary
            };

            split_coordinate(left, &input.edge_pixel[0], &input.edge_high[0]);
            split_coordinate(right, &input.edge_pixel[1], &input.edge_high[1]);
            if (run_case(reference, coverage, &input) != 0) {
                return 1;
            }
        }
        for (boundary = 0u;
             boundary < sizeof(y_coordinates) / sizeof(y_coordinates[0]);
             boundary++) {
            const uint16_t object = y_coordinates[boundary];
            const struct offscreen_input input = {
                .axis = AXIS_Y,
                .object_index = (uint8_t)index,
                .object_pixel = (uint8_t)object,
                .object_high = (uint8_t)(object >> 8),
                .tag = 0x30000u + index * 0x100u + (uint32_t)boundary
            };

            if (run_case(reference, coverage, &input) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_y_exhaustive(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t object;

    for (object = 0u; object <= UINT16_MAX; object++) {
        const struct offscreen_input input = {
            .axis = AXIS_Y,
            .object_index = 0xa5u,
            .object_pixel = (uint8_t)object,
            .object_high = (uint8_t)(object >> 8),
            .tag = 0x40000u + object
        };

        if (run_case(reference, coverage, &input) != 0) {
            return 1;
        }
    }
    return 0;
}

static int require_coverage(const struct coverage *coverage) {
    static const uint8_t expected_x_bits[] = {
        0x00u, 0x01u, 0x03u, 0x07u, 0x0fu, 0x1fu, 0x3fu, 0x7fu,
        0x80u, 0xc0u, 0xe0u, 0xf0u, 0xf8u, 0xfcu, 0xfeu, 0xffu
    };
    static const uint8_t expected_y_bits[] = {
        0x00u, 0x01u, 0x03u, 0x07u, 0x08u, 0x0cu, 0x0eu, 0x0fu
    };
    size_t i;

    if (coverage->x_cases < UINT16_MAX + 1u ||
        coverage->y_cases < UINT16_MAX + 1u ||
        coverage->scratch_divides == 0u ||
        !coverage->final_side[0x01] ||
        !coverage->final_side[0x00] ||
        !coverage->final_side[0xff]) {
        fputs("offscreen differential coverage incomplete\n", stderr);
        return 1;
    }
    for (i = 0u; i < sizeof(expected_x_bits); i++) {
        if (!coverage->x_bits[expected_x_bits[i]]) {
            fprintf(stderr, "missing X offscreen bits $%02x\n", expected_x_bits[i]);
            return 1;
        }
    }
    for (i = 0u; i < sizeof(expected_y_bits); i++) {
        if (!coverage->y_bits[expected_y_bits[i]]) {
            fprintf(stderr, "missing Y offscreen bits $%02x\n", expected_y_bits[i]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    struct reference_cpu reference;
    struct coverage coverage;

    memset(&reference, 0, sizeof(reference));
    memset(&coverage, 0, sizeof(coverage));
    if (run_x_exhaustive(&reference, &coverage) != 0 ||
        run_y_exhaustive(&reference, &coverage) != 0 ||
        run_index_boundaries(&reference, &coverage) != 0 ||
        require_coverage(&coverage) != 0) {
        return 1;
    }

    printf(
        "GetX/GetYOffscreenBits differential regression: PASS "
        "(%" PRIu64 " cases; X=%" PRIu64 ", Y=%" PRIu64
        ", DividePDiff scratch=%" PRIu64 ")\n",
        coverage.cases,
        coverage.x_cases,
        coverage.y_cases,
        coverage.scratch_divides
    );
    return 0;
}
