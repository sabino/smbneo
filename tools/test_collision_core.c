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
    BOX_SHAPE_COUNT = 12,
    SECOND_BOX_INDEX = 0,
    FIRST_BOX_INDEX = 4
};

static const uint8_t box_shapes[BOX_SHAPE_COUNT][4] = {
    {0x02, 0x08, 0x0e, 0x20},
    {0x03, 0x14, 0x0d, 0x20},
    {0x02, 0x14, 0x0e, 0x20},
    {0x02, 0x09, 0x0e, 0x15},
    {0x00, 0x00, 0x18, 0x06},
    {0x00, 0x00, 0x20, 0x0d},
    {0x00, 0x00, 0x30, 0x0d},
    {0x00, 0x00, 0x08, 0x08},
    {0x06, 0x04, 0x0a, 0x08},
    {0x03, 0x0e, 0x0d, 0x14},
    {0x00, 0x02, 0x10, 0x15},
    {0x04, 0x04, 0x0c, 0x1c}
};

static uint64_t exit_counts[3];

struct reference_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t ram[RAM_SIZE];
};

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    cpu->nz = value;
}

static void reference_ldy(struct reference_cpu *cpu, uint8_t value) {
    cpu->y = value;
    cpu->nz = value;
}

static void reference_cmp(struct reference_cpu *cpu, uint8_t value) {
    cpu->carry = cpu->a >= value;
    cpu->nz = (uint8_t)(cpu->a - value);
}

/*
 * Literal 6502-state oracle for src/smb.asm:SprObjectCollisionCore. Keeping
 * this instruction-shaped and independent of the optimized C makes the test
 * sensitive to accumulator, index, carry, NZ, and scratch-RAM differences.
 */
static void reference_collision_core(struct reference_cpu *cpu) {
    cpu->ram[0x06] = cpu->y;
    reference_lda(cpu, 0x01);
    cpu->ram[0x07] = cpu->a;

collision_core_loop:
    reference_lda(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->y]);
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (cpu->carry) {
        goto first_box_greater;
    }
    reference_cmp(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->x]);
    if (!cpu->carry) {
        goto second_box_vertical_check;
    }
    if (cpu->nz == 0u) {
        goto collision_found;
    }
    reference_lda(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->y]);
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->y]);
    if (!cpu->carry) {
        goto collision_found;
    }
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (cpu->carry) {
        goto collision_found;
    }
    reference_ldy(cpu, cpu->ram[0x06]);
    return;

second_box_vertical_check:
    reference_lda(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->x]);
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (!cpu->carry) {
        goto collision_found;
    }
    reference_lda(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->y]);
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (cpu->carry) {
        goto collision_found;
    }
    reference_ldy(cpu, cpu->ram[0x06]);
    return;

first_box_greater:
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (cpu->nz == 0u) {
        goto collision_found;
    }
    reference_cmp(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->x]);
    if (!cpu->carry || cpu->nz == 0u) {
        goto collision_found;
    }
    reference_cmp(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->y]);
    if (!cpu->carry || cpu->nz == 0u) {
        goto no_collision_found;
    }
    reference_lda(cpu, cpu->ram[BoundingBox_LR_Corner + cpu->y]);
    reference_cmp(cpu, cpu->ram[BoundingBox_UL_Corner + cpu->x]);
    if (cpu->carry) {
        goto collision_found;
    }

no_collision_found:
    cpu->carry = false;
    reference_ldy(cpu, cpu->ram[0x06]);
    return;

collision_found:
    cpu->x = (uint8_t)(cpu->x + 1u);
    cpu->nz = cpu->x;
    cpu->y = (uint8_t)(cpu->y + 1u);
    cpu->nz = cpu->y;
    cpu->ram[0x07] = (uint8_t)(cpu->ram[0x07] - 1u);
    cpu->nz = cpu->ram[0x07];
    if ((cpu->nz & 0x80u) == 0u) {
        goto collision_core_loop;
    }
    cpu->carry = true;
    reference_ldy(cpu, cpu->ram[0x06]);
}

static void set_ram_pair(
    struct reference_cpu *reference,
    uint16_t address,
    uint8_t value
) {
    reference->ram[address] = value;
    ram[address] = value;
}

static void set_box_pair(
    struct reference_cpu *reference,
    uint8_t index,
    uint8_t left,
    uint8_t top,
    uint8_t right,
    uint8_t bottom
) {
    const uint8_t vertical_index = (uint8_t)(index + 1u);

    set_ram_pair(reference, BoundingBox_UL_Corner + index, left);
    set_ram_pair(reference, BoundingBox_UL_Corner + vertical_index, top);
    set_ram_pair(reference, BoundingBox_LR_Corner + index, right);
    set_ram_pair(reference, BoundingBox_LR_Corner + vertical_index, bottom);
}

static int report_byte_mismatch(
    const char *suite,
    unsigned int first_shape,
    unsigned int second_shape,
    unsigned int first_position,
    unsigned int second_position,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s shapes=%u/%u positions=%u/%u %s: expected 0x%02x, "
        "got 0x%02x\n",
        suite,
        first_shape,
        second_shape,
        first_position,
        second_position,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int run_case(
    struct reference_cpu *reference,
    const char *suite,
    unsigned int first_shape,
    unsigned int second_shape,
    unsigned int first_position,
    unsigned int second_position,
    uint8_t second_index,
    uint8_t first_index
) {
    static const char *const ram_names[] = {
        "scratch-$06",
        "scratch-$07",
        "second-ul-x",
        "second-ul-y",
        "second-lr-x",
        "second-lr-y",
        "first-ul-x",
        "first-ul-y",
        "first-lr-x",
        "first-lr-y"
    };
    const uint16_t addresses[] = {
        0x06,
        0x07,
        BoundingBox_UL_Corner + second_index,
        BoundingBox_UL_Corner + (uint8_t)(second_index + 1u),
        BoundingBox_LR_Corner + second_index,
        BoundingBox_LR_Corner + (uint8_t)(second_index + 1u),
        BoundingBox_UL_Corner + first_index,
        BoundingBox_UL_Corner + (uint8_t)(first_index + 1u),
        BoundingBox_LR_Corner + first_index,
        BoundingBox_LR_Corner + (uint8_t)(first_index + 1u)
    };
    const uint8_t poison = (uint8_t)(
        first_shape * 29u + second_shape * 17u +
        first_position * 7u + second_position
    );
    uint8_t exit_axis_count;
    size_t index;

    reference->a = poison;
    reference->x = second_index;
    reference->y = first_index;
    reference->sp = (uint8_t)(poison ^ 0xa5u);
    reference->carry = (poison & 1u) != 0u;
    reference->nz = (uint8_t)(poison ^ 0x5au);
    set_ram_pair(reference, 0x06, (uint8_t)(poison ^ 0x3cu));
    set_ram_pair(reference, 0x07, (uint8_t)(poison ^ 0xc3u));

    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    reference_collision_core(reference);
    SprObjectCollisionCore();

#define COMPARE_CPU_BYTE(field_name, expected_value, actual_value) \
    do { \
        if ((expected_value) != (actual_value)) { \
            return report_byte_mismatch( \
                suite, first_shape, second_shape, first_position, \
                second_position, field_name, (expected_value), \
                (actual_value) \
            ); \
        } \
    } while (0)

    COMPARE_CPU_BYTE("a", reference->a, a);
    COMPARE_CPU_BYTE("x", reference->x, x);
    COMPARE_CPU_BYTE("y", reference->y, y);
    COMPARE_CPU_BYTE("sp", reference->sp, sp);
    COMPARE_CPU_BYTE(
        "carry",
        (uint8_t)reference->carry,
        (uint8_t)carry_flag
    );
    COMPARE_CPU_BYTE("nz", reference->nz, nz_value);

    for (index = 0; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        COMPARE_CPU_BYTE(
            ram_names[index],
            reference->ram[addresses[index]],
            ram[addresses[index]]
        );
    }

    exit_axis_count = (uint8_t)(reference->x - second_index);
    if (exit_axis_count > 2u) {
        return report_byte_mismatch(
            suite,
            first_shape,
            second_shape,
            first_position,
            second_position,
            "reference exit-axis count",
            2,
            exit_axis_count
        );
    }
    exit_counts[exit_axis_count]++;

#undef COMPARE_CPU_BYTE

    return 0;
}

static uint8_t add_shape_offset(uint32_t position, uint8_t offset) {
    return (uint8_t)(position + offset);
}

static int verify_shape_oracle(void) {
    unsigned int shape;
    unsigned int coordinate;

    for (shape = 0; shape < BOX_SHAPE_COUNT; shape++) {
        for (coordinate = 0; coordinate < 4; coordinate++) {
            const uint16_t address = (uint16_t)(
                BoundBoxCtrlData + shape * 4u + coordinate
            );
            const uint8_t generated = data[address - UINT16_C(0x8000)];

            if (generated != box_shapes[shape][coordinate]) {
                fprintf(
                    stderr,
                    "BoundBoxCtrlData shape=%u coordinate=%u: expected "
                    "0x%02x, got 0x%02x\n",
                    shape,
                    coordinate,
                    (unsigned int)box_shapes[shape][coordinate],
                    (unsigned int)generated
                );
                return 1;
            }
        }
    }
    return 0;
}

static int run_horizontal_exhaustion(
    struct reference_cpu *reference,
    uint64_t *case_count
) {
    unsigned int first_shape;
    unsigned int second_shape;
    uint32_t first_position;
    uint32_t second_position;

    for (first_shape = 0; first_shape < BOX_SHAPE_COUNT; first_shape++) {
        for (second_shape = 0; second_shape < BOX_SHAPE_COUNT; second_shape++) {
            for (first_position = 0; first_position <= UINT8_MAX; first_position++) {
                for (
                    second_position = 0;
                    second_position <= UINT8_MAX;
                    second_position++
                ) {
                    set_box_pair(
                        reference,
                        SECOND_BOX_INDEX,
                        add_shape_offset(
                            second_position,
                            box_shapes[second_shape][0]
                        ),
                        0x20,
                        add_shape_offset(
                            second_position,
                            box_shapes[second_shape][2]
                        ),
                        0x30
                    );
                    set_box_pair(
                        reference,
                        FIRST_BOX_INDEX,
                        add_shape_offset(
                            first_position,
                            box_shapes[first_shape][0]
                        ),
                        0x24,
                        add_shape_offset(
                            first_position,
                            box_shapes[first_shape][2]
                        ),
                        0x34
                    );
                    if (run_case(
                        reference,
                        "horizontal",
                        first_shape,
                        second_shape,
                        first_position,
                        second_position,
                        SECOND_BOX_INDEX,
                        FIRST_BOX_INDEX
                    ) != 0) {
                        return 1;
                    }
                    (*case_count)++;
                }
            }
        }
    }
    return 0;
}

static int run_vertical_exhaustion(
    struct reference_cpu *reference,
    uint64_t *case_count
) {
    unsigned int first_shape;
    unsigned int second_shape;
    uint32_t first_position;
    uint32_t second_position;

    for (first_shape = 0; first_shape < BOX_SHAPE_COUNT; first_shape++) {
        for (second_shape = 0; second_shape < BOX_SHAPE_COUNT; second_shape++) {
            for (first_position = 0; first_position <= UINT8_MAX; first_position++) {
                for (
                    second_position = 0;
                    second_position <= UINT8_MAX;
                    second_position++
                ) {
                    set_box_pair(
                        reference,
                        SECOND_BOX_INDEX,
                        0x20,
                        add_shape_offset(
                            second_position,
                            box_shapes[second_shape][1]
                        ),
                        0x30,
                        add_shape_offset(
                            second_position,
                            box_shapes[second_shape][3]
                        )
                    );
                    set_box_pair(
                        reference,
                        FIRST_BOX_INDEX,
                        0x24,
                        add_shape_offset(
                            first_position,
                            box_shapes[first_shape][1]
                        ),
                        0x34,
                        add_shape_offset(
                            first_position,
                            box_shapes[first_shape][3]
                        )
                    );
                    if (run_case(
                        reference,
                        "vertical",
                        first_shape,
                        second_shape,
                        first_position,
                        second_position,
                        SECOND_BOX_INDEX,
                        FIRST_BOX_INDEX
                    ) != 0) {
                        return 1;
                    }
                    (*case_count)++;
                }
            }
        }
    }
    return 0;
}

static int run_explicit_edges(
    struct reference_cpu *reference,
    uint64_t *case_count
) {
    static const uint8_t boxes[][8] = {
        {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00},
        {0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00},
        {0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff},
        {0xf8, 0xf8, 0x08, 0x08, 0xfc, 0xfc, 0x04, 0x04}
    };
    size_t edge;

    for (edge = 0; edge < sizeof(boxes) / sizeof(boxes[0]); edge++) {
        set_box_pair(
            reference,
            SECOND_BOX_INDEX,
            boxes[edge][0],
            boxes[edge][1],
            boxes[edge][2],
            boxes[edge][3]
        );
        set_box_pair(
            reference,
            FIRST_BOX_INDEX,
            boxes[edge][4],
            boxes[edge][5],
            boxes[edge][6],
            boxes[edge][7]
        );
        if (run_case(
            reference,
            "explicit-edge",
            0,
            0,
            (unsigned int)edge,
            0,
            SECOND_BOX_INDEX,
            FIRST_BOX_INDEX
        ) != 0) {
            return 1;
        }
        (*case_count)++;
    }

    /* Exercise INX/INY wrapping between horizontal and vertical checks. */
    set_box_pair(reference, 0xff, 0xf8, 0x10, 0x08, 0x30);
    set_box_pair(reference, 0xfb, 0xfc, 0x14, 0x04, 0x34);
    if (run_case(
        reference,
        "index-wrap",
        0,
        0,
        0xff,
        0xfb,
        0xff,
        0xfb
    ) != 0) {
        return 1;
    }
    (*case_count)++;

    return 0;
}

int main(void) {
    struct reference_cpu reference;
    uint64_t case_count = 0;

    memset(&reference, 0, sizeof(reference));
    memset(ram, 0, sizeof(ram));

    if (verify_shape_oracle() != 0) {
        return 1;
    }
    if (run_horizontal_exhaustion(&reference, &case_count) != 0) {
        return 1;
    }
    if (run_vertical_exhaustion(&reference, &case_count) != 0) {
        return 1;
    }
    if (run_explicit_edges(&reference, &case_count) != 0) {
        return 1;
    }
    if (exit_counts[0] == 0u || exit_counts[1] == 0u || exit_counts[2] == 0u) {
        fprintf(
            stderr,
            "collision exit coverage incomplete: first-miss=%" PRIu64
            " second-miss=%" PRIu64 " hit=%" PRIu64 "\n",
            exit_counts[0],
            exit_counts[1],
            exit_counts[2]
        );
        return 1;
    }

    printf(
        "SprObjectCollisionCore differential regression: PASS "
        "(%" PRIu64 " cases; first-miss=%" PRIu64
        ", second-miss=%" PRIu64 ", hit=%" PRIu64 ")\n",
        case_count,
        exit_counts[0],
        exit_counts[1],
        exit_counts[2]
    );
    return 0;
}
