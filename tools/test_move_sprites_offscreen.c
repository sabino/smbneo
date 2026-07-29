#include "code.h"
#include "constants.h"
#include "cpu.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TERMINATING_Y_COUNT = 64,
    OAM_BYTE_COUNT = 256
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

struct loop_input {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t ram_salt;
    bool uniform_oam;
    uint8_t oam_value;
};

struct coverage {
    uint64_t cases;
    uint64_t loop_iterations;
    uint64_t carry_clear;
    uint64_t carry_set;
    uint64_t start_zero;
    uint64_t start_four;
    uint64_t start_last;
};

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    cpu->nz = value;
}

static void reference_iny(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y + 1u);
    cpu->nz = cpu->y;
}

/* Literal 6502 oracle for src/smb.asm:MoveSpritesOffscreenSkip. */
static unsigned int reference_move_sprites_offscreen(
    struct reference_cpu *cpu
) {
    unsigned int iterations = 0;

    reference_lda(cpu, 0xf8);
sprite_init_loop:
    cpu->ram[Sprite_Y_Position + cpu->y] = cpu->a;
    reference_iny(cpu);
    reference_iny(cpu);
    reference_iny(cpu);
    reference_iny(cpu);
    iterations++;
    if (cpu->nz != 0u) {
        goto sprite_init_loop;
    }
    return iterations;
}

static int report_mismatch(
    const char *suite,
    uint64_t ordinal,
    const struct loop_input *input,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu64 " initial-a/x/y/sp=%02x/%02x/%02x/%02x "
        "carry=%u nz=%02x %s: expected 0x%02x, got 0x%02x\n",
        suite,
        ordinal,
        (unsigned int)input->a,
        (unsigned int)input->x,
        (unsigned int)input->y,
        (unsigned int)input->sp,
        input->carry ? 1u : 0u,
        (unsigned int)input->nz,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static void initialize_case_ram(
    struct reference_cpu *reference,
    const struct loop_input *input
) {
    uint16_t address;

    for (address = 0; address < RAM_SIZE; address++) {
        const uint8_t value = (uint8_t)(
            input->ram_salt + address * 37u + (address >> 8) * 19u
        );

        reference->ram[address] = value;
        ram[address] = value;
    }
    if (input->uniform_oam) {
        for (address = 0; address < OAM_BYTE_COUNT; address++) {
            reference->ram[Sprite_Y_Position + address] = input->oam_value;
            ram[Sprite_Y_Position + address] = input->oam_value;
        }
    }
}

static int run_case(
    struct reference_cpu *reference,
    struct coverage *coverage,
    const char *suite,
    const struct loop_input *input
) {
    const uint64_t ordinal = coverage->cases;
    const unsigned int expected_iterations = input->y == 0u
        ? 64u
        : (256u - input->y) / 4u;
    unsigned int iterations;
    uint16_t address;

    initialize_case_ram(reference, input);
    reference->a = input->a;
    reference->x = input->x;
    reference->y = input->y;
    reference->sp = input->sp;
    reference->carry = input->carry;
    reference->nz = input->nz;
    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    iterations = reference_move_sprites_offscreen(reference);
    MoveSpritesOffscreenSkip();

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
    if (iterations != expected_iterations) {
        return report_mismatch(
            suite,
            ordinal,
            input,
            "oracle-iterations",
            (uint8_t)expected_iterations,
            (uint8_t)iterations
        );
    }
    if (memcmp(reference->ram, ram, RAM_SIZE) != 0) {
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
    }

#undef COMPARE_BYTE

    coverage->cases++;
    coverage->loop_iterations += iterations;
    if (input->carry) {
        coverage->carry_set++;
    } else {
        coverage->carry_clear++;
    }
    if (input->y == 0u) {
        coverage->start_zero++;
    } else if (input->y == 4u) {
        coverage->start_four++;
    } else if (input->y == 0xfcu) {
        coverage->start_last++;
    }
    return 0;
}

static struct loop_input base_input(uint8_t start_y, uint8_t selector) {
    const struct loop_input input = {
        .a = (uint8_t)(selector ^ 0x96u),
        .x = (uint8_t)(selector * 17u + 3u),
        .y = start_y,
        .sp = (uint8_t)(selector ^ 0xa5u),
        .carry = (selector & 0x01u) != 0u,
        .nz = (uint8_t)(selector * 29u + 7u),
        .ram_salt = (uint8_t)(selector * 43u + start_y),
        .uniform_oam = false,
        .oam_value = 0
    };

    return input;
}

static int run_register_and_ram_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t selector;
    uint32_t start_index;

    for (selector = 0; selector <= UINT8_MAX; selector++) {
        for (start_index = 0; start_index < TERMINATING_Y_COUNT; start_index++) {
            const uint8_t start_y = (uint8_t)(start_index * 4u);
            struct loop_input input = base_input(start_y, (uint8_t)selector);

            input.a = (uint8_t)selector;
            if (run_case(reference, coverage, "initial-a", &input) != 0) {
                return 1;
            }
            input = base_input(start_y, (uint8_t)selector);
            input.x = (uint8_t)selector;
            if (run_case(reference, coverage, "initial-x", &input) != 0) {
                return 1;
            }
            input = base_input(start_y, (uint8_t)selector);
            input.sp = (uint8_t)selector;
            if (run_case(reference, coverage, "initial-sp", &input) != 0) {
                return 1;
            }
            input = base_input(start_y, (uint8_t)selector);
            input.nz = (uint8_t)selector;
            if (run_case(reference, coverage, "initial-nz", &input) != 0) {
                return 1;
            }
            input = base_input(start_y, (uint8_t)selector);
            input.uniform_oam = true;
            input.oam_value = (uint8_t)selector;
            if (run_case(reference, coverage, "initial-oam", &input) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_carry_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t carry;
    uint32_t start_index;

    for (carry = 0; carry <= 1u; carry++) {
        for (start_index = 0; start_index < TERMINATING_Y_COUNT; start_index++) {
            struct loop_input input = base_input(
                (uint8_t)(start_index * 4u),
                (uint8_t)(start_index * 13u + carry)
            );

            input.carry = carry != 0u;
            if (run_case(reference, coverage, "initial-carry", &input) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    struct reference_cpu reference;
    struct coverage coverage = {0};

    memset(&reference, 0, sizeof(reference));
    memset(ram, 0, sizeof(ram));
    if (run_register_and_ram_exhaustion(&reference, &coverage) != 0 ||
        run_carry_exhaustion(&reference, &coverage) != 0) {
        return 1;
    }
    if (coverage.carry_clear == 0u || coverage.carry_set == 0u ||
        coverage.start_zero == 0u || coverage.start_four == 0u ||
        coverage.start_last == 0u) {
        fputs("MoveSpritesOffscreenSkip differential coverage incomplete\n", stderr);
        return 1;
    }

    printf(
        "MoveSpritesOffscreenSkip differential regression: PASS (%" PRIu64
        " cases, %" PRIu64 " literal iterations; start-y "
        "00/04/fc=%" PRIu64 "/%" PRIu64 "/%" PRIu64
        ", carry=%" PRIu64 "/%" PRIu64 ")\n",
        coverage.cases,
        coverage.loop_iterations,
        coverage.start_zero,
        coverage.start_four,
        coverage.start_last,
        coverage.carry_clear,
        coverage.carry_set
    );
    return 0;
}
