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
    SCORE_DIGITS = 6,
    FULL_RAM_COMPARE_INTERVAL = 1024,
    RANDOM_CASES = 131072
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

struct score_input {
    uint8_t player[SCORE_DIGITS];
    uint8_t top[SCORE_DIGITS];
    uint8_t initial_a;
    uint8_t initial_x;
    uint8_t initial_y;
    uint8_t initial_sp;
    bool initial_carry;
    uint8_t initial_nz;
};

struct coverage {
    uint64_t cases;
    uint64_t copied;
    uint64_t rejected;
    uint64_t equal_common_slot;
    uint64_t wrapped_player_address;
    bool x_seen[UINT8_MAX + 1u];
};

static uint16_t reference_ram_address(uint16_t address) {
    return (uint16_t)(address & (RAM_SIZE - 1u));
}

static uint8_t reference_read(
    const struct reference_cpu *cpu,
    uint16_t address
) {
    return cpu->ram[reference_ram_address(address)];
}

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    cpu->nz = value;
}

static void reference_sbc(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t difference = (uint16_t)cpu->a - (uint16_t)value -
        (cpu->carry ? 0u : 1u);

    cpu->a = (uint8_t)difference;
    cpu->carry = difference <= UINT8_MAX;
    cpu->nz = cpu->a;
}

static void reference_dex(struct reference_cpu *cpu) {
    cpu->x = (uint8_t)(cpu->x - 1u);
    cpu->nz = cpu->x;
}

static void reference_dey(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y - 1u);
    cpu->nz = cpu->y;
}

static void reference_inx(struct reference_cpu *cpu) {
    cpu->x = (uint8_t)(cpu->x + 1u);
    cpu->nz = cpu->x;
}

static void reference_iny(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y + 1u);
    cpu->nz = cpu->y;
}

static void reference_cpy(struct reference_cpu *cpu, uint8_t value) {
    const uint16_t difference = (uint16_t)cpu->y - value;

    cpu->carry = cpu->y >= value;
    cpu->nz = (uint8_t)difference;
}

/*
 * Literal instruction-shaped oracle for src/smb.asm:TopScoreCheck. This
 * intentionally retains the low-to-high SBC borrow chain and sequential copy
 * so wrapped X addresses and RAM aliasing are tested independently of the
 * optimized semantic C.
 */
static void reference_top_score_check(struct reference_cpu *cpu) {
    cpu->y = 5u;
    cpu->nz = cpu->y;
    cpu->carry = true;

    do {
        reference_lda(
            cpu,
            reference_read(cpu, (uint16_t)(PlayerScoreDisplay + cpu->x))
        );
        reference_sbc(cpu, cpu->ram[TopScoreDisplay + cpu->y]);
        reference_dex(cpu);
        reference_dey(cpu);
    } while ((cpu->nz & 0x80u) == 0u);

    if (!cpu->carry) {
        return;
    }

    reference_inx(cpu);
    reference_iny(cpu);
    do {
        reference_lda(
            cpu,
            reference_read(cpu, (uint16_t)(PlayerScoreDisplay + cpu->x))
        );
        cpu->ram[TopScoreDisplay + cpu->y] = cpu->a;
        reference_inx(cpu);
        reference_iny(cpu);
        reference_cpy(cpu, 6u);
    } while (!cpu->carry);
}

static void set_ram_pair(
    struct reference_cpu *reference,
    uint16_t address,
    uint8_t value
) {
    reference->ram[address] = value;
    ram[address] = value;
}

static uint16_t player_digit_address(uint8_t initial_x, uint8_t digit) {
    const uint8_t register_x = (uint8_t)(initial_x - 5u + digit);

    return reference_ram_address((uint16_t)(PlayerScoreDisplay + register_x));
}

static bool input_scores_equal(const struct score_input *input) {
    return memcmp(input->player, input->top, SCORE_DIGITS) == 0;
}

static int report_mismatch(
    const char *suite,
    uint64_t ordinal,
    const struct score_input *input,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu64 " x=%02x a/y/sp=%02x/%02x/%02x "
        "carry/nz=%u/%02x %s: expected 0x%02x, got 0x%02x\n",
        suite,
        ordinal,
        (unsigned int)input->initial_x,
        (unsigned int)input->initial_a,
        (unsigned int)input->initial_y,
        (unsigned int)input->initial_sp,
        input->initial_carry ? 1u : 0u,
        (unsigned int)input->initial_nz,
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
    const struct score_input *input
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
    const struct score_input *input
) {
    const uint64_t ordinal = coverage->cases;
    uint8_t digit;
    bool copied;

    for (digit = 0; digit < SCORE_DIGITS; digit++) {
        set_ram_pair(
            reference,
            (uint16_t)(TopScoreDisplay + digit),
            input->top[digit]
        );
    }
    for (digit = 0; digit < SCORE_DIGITS; digit++) {
        set_ram_pair(
            reference,
            player_digit_address(input->initial_x, digit),
            input->player[digit]
        );
    }

    reference->a = input->initial_a;
    reference->x = input->initial_x;
    reference->y = input->initial_y;
    reference->sp = input->initial_sp;
    reference->carry = input->initial_carry;
    reference->nz = input->initial_nz;
    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;

    reference_top_score_check(reference);
    TopScoreCheck();
    copied = reference->y == 6u;

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
    for (digit = 0; digit < SCORE_DIGITS; digit++) {
        char field[32];

        (void)snprintf(field, sizeof(field), "top-score[%u]", digit);
        COMPARE_BYTE(
            field,
            reference->ram[TopScoreDisplay + digit],
            ram[TopScoreDisplay + digit]
        );
    }

#undef COMPARE_BYTE

    coverage->cases++;
    coverage->x_seen[input->initial_x] = true;
    if (copied) {
        coverage->copied++;
    } else {
        coverage->rejected++;
    }
    if ((input->initial_x == 5u || input->initial_x == 11u) &&
        input_scores_equal(input)) {
        coverage->equal_common_slot++;
    }
    if (input->initial_x > 34u) {
        coverage->wrapped_player_address++;
    }
    if ((coverage->cases % FULL_RAM_COMPARE_INTERVAL) == 0u) {
        return compare_full_ram(reference, suite, ordinal, input);
    }
    return 0;
}

static void set_boundary_pattern(struct score_input *input, uint8_t salt) {
    static const uint8_t boundaries[SCORE_DIGITS] = {
        0x00, 0x01, 0x7f, 0x80, 0xfe, 0xff
    };
    uint8_t digit;

    for (digit = 0; digit < SCORE_DIGITS; digit++) {
        const uint8_t value = boundaries[(digit + salt) % SCORE_DIGITS];

        input->player[digit] = value;
        input->top[digit] = value;
    }
}

static int run_byte_pair_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct score_input input = {0};
    uint32_t slot;
    uint32_t digit;
    uint32_t player_value;
    uint32_t top_value;

    for (slot = 0; slot < 2u; slot++) {
        input.initial_x = slot == 0u ? 5u : 11u;
        input.initial_sp = (uint8_t)(0xa5u + slot);
        for (digit = 0; digit < SCORE_DIGITS; digit++) {
            set_boundary_pattern(&input, (uint8_t)(digit + slot));
            for (player_value = 0; player_value <= UINT8_MAX; player_value++) {
                input.player[digit] = (uint8_t)player_value;
                for (top_value = 0; top_value <= UINT8_MAX; top_value++) {
                    input.top[digit] = (uint8_t)top_value;
                    input.initial_a = (uint8_t)(player_value ^ top_value);
                    input.initial_y = (uint8_t)(player_value + top_value);
                    input.initial_carry = (top_value & 1u) != 0u;
                    input.initial_nz = (uint8_t)(player_value - top_value);
                    if (run_case(
                        reference,
                        coverage,
                        "byte-pairs",
                        &input
                    ) != 0) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int run_register_and_x_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct score_input input = {0};
    uint32_t value;
    uint32_t pattern;

    for (pattern = 0; pattern < SCORE_DIGITS; pattern++) {
        set_boundary_pattern(&input, (uint8_t)pattern);
        input.player[pattern] ^= (uint8_t)(0x81u + pattern);
        for (value = 0; value <= UINT8_MAX; value++) {
            input.initial_x = (uint8_t)value;
            input.initial_a = (uint8_t)(value + pattern);
            input.initial_y = (uint8_t)(value ^ 0x80u);
            input.initial_sp = (uint8_t)(value - pattern);
            input.initial_carry = ((value + pattern) & 1u) != 0u;
            input.initial_nz = (uint8_t)(value ^ 0xffu);
            if (run_case(reference, coverage, "x-domain", &input) != 0) {
                return 1;
            }
        }
    }

    set_boundary_pattern(&input, 0u);
    for (value = 0; value <= UINT8_MAX; value++) {
        input.initial_x = (value & 1u) == 0u ? 5u : 11u;
        input.initial_a = (uint8_t)value;
        input.initial_y = (uint8_t)value;
        input.initial_sp = (uint8_t)value;
        input.initial_carry = (value & 1u) != 0u;
        input.initial_nz = (uint8_t)value;
        if (run_case(reference, coverage, "register-domains", &input) != 0) {
            return 1;
        }
    }
    return 0;
}

static uint32_t next_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static int run_deterministic_random(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    struct score_input input = {0};
    uint32_t state = UINT32_C(0x534d424e);
    uint32_t case_index;
    uint8_t digit;

    for (case_index = 0; case_index < RANDOM_CASES; case_index++) {
        for (digit = 0; digit < SCORE_DIGITS; digit++) {
            input.player[digit] = (uint8_t)(next_random(&state) >> 24);
            input.top[digit] = (uint8_t)(next_random(&state) >> 24);
        }
        input.initial_a = (uint8_t)(next_random(&state) >> 24);
        input.initial_x = (uint8_t)(next_random(&state) >> 24);
        input.initial_y = (uint8_t)(next_random(&state) >> 24);
        input.initial_sp = (uint8_t)(next_random(&state) >> 24);
        input.initial_carry = (next_random(&state) & 1u) != 0u;
        input.initial_nz = (uint8_t)(next_random(&state) >> 24);
        if (run_case(reference, coverage, "deterministic-random", &input) != 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    struct reference_cpu reference;
    struct coverage coverage = {0};
    struct score_input last_input = {0};
    uint16_t address;

    for (address = 0; address < RAM_SIZE; address++) {
        const uint8_t value = (uint8_t)(address * 73u + 0x5au);

        reference.ram[address] = value;
        ram[address] = value;
    }

    if (run_byte_pair_exhaustion(&reference, &coverage) != 0 ||
        run_register_and_x_exhaustion(&reference, &coverage) != 0 ||
        run_deterministic_random(&reference, &coverage) != 0 ||
        compare_full_ram(
            &reference,
            "final",
            coverage.cases,
            &last_input
        ) != 0) {
        return 1;
    }

    for (address = 0; address <= UINT8_MAX; address++) {
        if (!coverage.x_seen[address]) {
            fputs("TopScoreCheck X-domain coverage is incomplete\n", stderr);
            return 1;
        }
    }
    if (coverage.copied == 0u || coverage.rejected == 0u ||
        coverage.equal_common_slot == 0u ||
        coverage.wrapped_player_address == 0u) {
        fputs("TopScoreCheck differential coverage is incomplete\n", stderr);
        return 1;
    }

    printf(
        "TopScoreCheck differential regression: PASS (%" PRIu64
        " cases; copied=%" PRIu64 ", rejected=%" PRIu64
        ", common-equal=%" PRIu64 ", wrapped=%" PRIu64
        ")\n",
        coverage.cases,
        coverage.copied,
        coverage.rejected,
        coverage.equal_common_slot,
        coverage.wrapped_player_address
    );
    return 0;
}
