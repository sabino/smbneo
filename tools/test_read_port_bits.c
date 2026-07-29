#include "code.h"
#include "constants.h"
#include "cpu.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    SAFE_X_COUNT = 5
};

static const uint8_t safe_x_values[SAFE_X_COUNT] = {
    0u, 1u, 2u, 17u, 181u
};

struct reference_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t ram[RAM_SIZE];
    uint8_t controller1_state;
    bool controller1_strobe;
    uint8_t controller1_btn_index;
    uint8_t controller2_state;
    bool controller2_strobe;
    uint8_t controller2_btn_index;
};

struct input_case {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t controller1_state;
    bool controller1_strobe;
    uint8_t controller1_btn_index;
    uint8_t controller2_state;
    bool controller2_strobe;
    uint8_t controller2_btn_index;
    uint8_t joypad_mask0;
    uint8_t joypad_mask1;
    uint8_t ram_salt;
};

struct coverage {
    uint64_t port_cases;
    uint64_t joypad_cases;
    uint64_t filtered;
    uint64_t mask_updated;
    uint64_t strobe_clear;
    uint64_t strobe_set;
    uint64_t index_overrun;
    uint64_t player2_port;
};

static void reference_update_nz(struct reference_cpu *cpu, uint8_t value) {
    cpu->nz = value;
}

static void reference_lda(struct reference_cpu *cpu, uint8_t value) {
    cpu->a = value;
    reference_update_nz(cpu, value);
}

static void reference_ldy(struct reference_cpu *cpu, uint8_t value) {
    cpu->y = value;
    reference_update_nz(cpu, value);
}

static void reference_tax(struct reference_cpu *cpu) {
    cpu->x = cpu->a;
    reference_update_nz(cpu, cpu->x);
}

static void reference_inx(struct reference_cpu *cpu) {
    cpu->x = (uint8_t)(cpu->x + 1u);
    reference_update_nz(cpu, cpu->x);
}

static void reference_dey(struct reference_cpu *cpu) {
    cpu->y = (uint8_t)(cpu->y - 1u);
    reference_update_nz(cpu, cpu->y);
}

static void reference_lsr(struct reference_cpu *cpu) {
    cpu->carry = (cpu->a & 1u) != 0u;
    cpu->a >>= 1;
    reference_update_nz(cpu, cpu->a);
}

static void reference_ora(struct reference_cpu *cpu, uint8_t value) {
    cpu->a |= value;
    reference_update_nz(cpu, cpu->a);
}

static void reference_and(struct reference_cpu *cpu, uint8_t value) {
    cpu->a &= value;
    reference_update_nz(cpu, cpu->a);
}

static void reference_rol(struct reference_cpu *cpu) {
    const bool next_carry = (cpu->a & 0x80u) != 0u;

    cpu->a = (uint8_t)((cpu->a << 1) | (cpu->carry ? 1u : 0u));
    cpu->carry = next_carry;
    reference_update_nz(cpu, cpu->a);
}

static void reference_pha(struct reference_cpu *cpu) {
    cpu->ram[0x100u | cpu->sp] = cpu->a;
    cpu->sp = (uint8_t)(cpu->sp - 1u);
}

static void reference_pla(struct reference_cpu *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1u);
    reference_lda(cpu, cpu->ram[0x100u | cpu->sp]);
}

static void reference_write_joypad1(
    struct reference_cpu *cpu,
    uint8_t value
) {
    cpu->controller1_strobe = (value & 1u) != 0u;
    if (cpu->controller1_strobe) {
        cpu->controller1_btn_index = 0u;
    }
}

static uint8_t reference_read_byte(
    struct reference_cpu *cpu,
    uint16_t address
) {
    if (address == JOYPAD_PORT) {
        uint8_t state;

        if (cpu->controller1_btn_index > 7u) {
            return 1u;
        }
        state = (uint8_t)(
            (cpu->controller1_state >> cpu->controller1_btn_index) & 1u
        );
        if (!cpu->controller1_strobe &&
            cpu->controller1_btn_index < 8u) {
            cpu->controller1_btn_index++;
        }
        return state;
    }

    /*
     * The current core exposes $4016 only. ReadJoypads still invokes x=1 for
     * the second NES port, which therefore returns neutral zeroes and must not
     * mutate the controller-two bookkeeping.
     */
    return 0u;
}

/* Literal instruction-shaped oracle for src/smb.asm:ReadPortBits. */
static bool reference_read_port_bits(struct reference_cpu *cpu) {
    bool filtered;

    reference_ldy(cpu, 0x08u);
port_loop:
    reference_pha(cpu);
    reference_lda(
        cpu,
        reference_read_byte(cpu, (uint16_t)(JOYPAD_PORT + cpu->x))
    );
    cpu->ram[0x00u] = cpu->a;
    reference_lsr(cpu);
    reference_ora(cpu, cpu->ram[0x00u]);
    reference_lsr(cpu);
    reference_pla(cpu);
    reference_rol(cpu);
    reference_dey(cpu);
    if (cpu->nz != 0u) {
        goto port_loop;
    }

    cpu->ram[SavedJoypadBits + cpu->x] = cpu->a;
    reference_pha(cpu);
    reference_and(cpu, 0x30u);
    reference_and(cpu, cpu->ram[JoypadBitMask + cpu->x]);
    filtered = cpu->nz != 0u;
    if (filtered) {
        reference_pla(cpu);
        reference_and(cpu, 0xcfu);
        cpu->ram[SavedJoypadBits + cpu->x] = cpu->a;
        return true;
    }
    reference_pla(cpu);
    cpu->ram[JoypadBitMask + cpu->x] = cpu->a;
    return false;
}

/* Literal instruction-shaped oracle for src/smb.asm:ReadJoypads. */
static bool reference_read_joypads(struct reference_cpu *cpu) {
    bool first_filtered;

    reference_lda(cpu, 0x01u);
    reference_write_joypad1(cpu, cpu->a);
    reference_lsr(cpu);
    reference_tax(cpu);
    reference_write_joypad1(cpu, cpu->a);
    first_filtered = reference_read_port_bits(cpu);
    reference_inx(cpu);
    (void)reference_read_port_bits(cpu);
    return first_filtered;
}

static struct input_case base_input(uint8_t selector) {
    const struct input_case input = {
        .a = (uint8_t)(selector * 29u + 7u),
        .x = 0u,
        .y = (uint8_t)(selector ^ 0x5au),
        .sp = (uint8_t)(selector * 13u + 0x61u),
        .carry = (selector & 1u) != 0u,
        .nz = (uint8_t)(selector * 43u + 3u),
        .controller1_state = (uint8_t)(selector ^ 0xa5u),
        .controller1_strobe = (selector & 2u) != 0u,
        .controller1_btn_index = (uint8_t)(selector * 17u),
        .controller2_state = (uint8_t)(selector ^ 0x3cu),
        .controller2_strobe = (selector & 4u) != 0u,
        .controller2_btn_index = (uint8_t)(selector * 31u),
        .joypad_mask0 = (uint8_t)(selector * 11u),
        .joypad_mask1 = (uint8_t)(selector * 19u),
        .ram_salt = (uint8_t)(selector * 23u + 1u)
    };

    return input;
}

static void initialize_case(
    struct reference_cpu *reference,
    const struct input_case *input
) {
    uint16_t address;

    for (address = 0u; address < RAM_SIZE; address++) {
        const uint8_t value = (uint8_t)(
            input->ram_salt + address * 37u + (address >> 8) * 19u
        );

        reference->ram[address] = value;
        ram[address] = value;
    }
    reference->ram[JoypadBitMask] = input->joypad_mask0;
    reference->ram[JoypadBitMask + 1u] = input->joypad_mask1;
    ram[JoypadBitMask] = input->joypad_mask0;
    ram[JoypadBitMask + 1u] = input->joypad_mask1;
    if (input->x <= 181u) {
        reference->ram[JoypadBitMask + input->x] = input->joypad_mask0;
        ram[JoypadBitMask + input->x] = input->joypad_mask0;
    }

    reference->a = input->a;
    reference->x = input->x;
    reference->y = input->y;
    reference->sp = input->sp;
    reference->carry = input->carry;
    reference->nz = input->nz;
    reference->controller1_state = input->controller1_state;
    reference->controller1_strobe = input->controller1_strobe;
    reference->controller1_btn_index = input->controller1_btn_index;
    reference->controller2_state = input->controller2_state;
    reference->controller2_strobe = input->controller2_strobe;
    reference->controller2_btn_index = input->controller2_btn_index;

    a = reference->a;
    x = reference->x;
    y = reference->y;
    sp = reference->sp;
    carry_flag = reference->carry;
    nz_value = reference->nz;
    controller1_state = reference->controller1_state;
    controller1_strobe = reference->controller1_strobe;
    controller1_btn_index = reference->controller1_btn_index;
    controller2_state = reference->controller2_state;
    controller2_strobe = reference->controller2_strobe;
    controller2_btn_index = reference->controller2_btn_index;
}

static int report_mismatch(
    const char *suite,
    uint64_t ordinal,
    const struct input_case *input,
    const char *field,
    uint8_t expected,
    uint8_t actual
) {
    fprintf(
        stderr,
        "%s case=%" PRIu64 " a/x/y/sp=%02x/%02x/%02x/%02x "
        "c/nz=%u/%02x p1=%02x/%u/%02x p2=%02x/%u/%02x "
        "mask=%02x/%02x %s: expected 0x%02x, got 0x%02x\n",
        suite,
        ordinal,
        (unsigned int)input->a,
        (unsigned int)input->x,
        (unsigned int)input->y,
        (unsigned int)input->sp,
        input->carry ? 1u : 0u,
        (unsigned int)input->nz,
        (unsigned int)input->controller1_state,
        input->controller1_strobe ? 1u : 0u,
        (unsigned int)input->controller1_btn_index,
        (unsigned int)input->controller2_state,
        input->controller2_strobe ? 1u : 0u,
        (unsigned int)input->controller2_btn_index,
        (unsigned int)input->joypad_mask0,
        (unsigned int)input->joypad_mask1,
        field,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int compare_case(
    const struct reference_cpu *reference,
    const struct input_case *input,
    const char *suite,
    uint64_t ordinal
) {
    uint16_t address;

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
    COMPARE_BYTE(
        "controller1-state",
        reference->controller1_state,
        controller1_state
    );
    COMPARE_BYTE(
        "controller1-strobe",
        reference->controller1_strobe,
        controller1_strobe
    );
    COMPARE_BYTE(
        "controller1-index",
        reference->controller1_btn_index,
        controller1_btn_index
    );
    COMPARE_BYTE(
        "controller2-state",
        reference->controller2_state,
        controller2_state
    );
    COMPARE_BYTE(
        "controller2-strobe",
        reference->controller2_strobe,
        controller2_strobe
    );
    COMPARE_BYTE(
        "controller2-index",
        reference->controller2_btn_index,
        controller2_btn_index
    );
    if (memcmp(reference->ram, ram, RAM_SIZE) != 0) {
        for (address = 0u; address < RAM_SIZE; address++) {
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

    return 0;
}

static int run_port_case(
    struct reference_cpu *reference,
    struct coverage *coverage,
    const char *suite,
    const struct input_case *input
) {
    const uint64_t ordinal = coverage->port_cases;
    const bool filtered = (
        reference_read_port_bits(reference)
    );

    ReadPortBits();
    if (compare_case(reference, input, suite, ordinal) != 0) {
        return 1;
    }

    coverage->port_cases++;
    if (filtered) {
        coverage->filtered++;
    } else {
        coverage->mask_updated++;
    }
    if (input->controller1_strobe) {
        coverage->strobe_set++;
    } else {
        coverage->strobe_clear++;
    }
    if (input->controller1_btn_index > 7u) {
        coverage->index_overrun++;
    }
    if (input->x == 1u) {
        coverage->player2_port++;
    }
    return 0;
}

static int run_joypad_case(
    struct reference_cpu *reference,
    struct coverage *coverage,
    const char *suite,
    const struct input_case *input
) {
    const uint64_t ordinal = coverage->joypad_cases;
    const bool filtered = reference_read_joypads(reference);

    ReadJoypads();
    if (compare_case(reference, input, suite, ordinal) != 0) {
        return 1;
    }

    coverage->joypad_cases++;
    if (filtered) {
        coverage->filtered++;
    } else {
        coverage->mask_updated++;
    }
    return 0;
}

static int run_state_mask_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t state;
    uint32_t mask;

    for (state = 0u; state <= UINT8_MAX; state++) {
        for (mask = 0u; mask <= UINT8_MAX; mask++) {
            struct input_case input = base_input((uint8_t)(state + mask));

            input.x = 0u;
            input.controller1_state = (uint8_t)state;
            input.controller1_strobe = false;
            input.controller1_btn_index = 0u;
            input.joypad_mask0 = (uint8_t)mask;
            initialize_case(reference, &input);
            if (run_port_case(
                    reference,
                    coverage,
                    "state-mask",
                    &input
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_state_index_strobe_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t strobe;
    uint32_t index;
    uint32_t state;

    for (strobe = 0u; strobe <= 1u; strobe++) {
        for (index = 0u; index <= UINT8_MAX; index++) {
            for (state = 0u; state <= UINT8_MAX; state++) {
                struct input_case input = base_input(
                    (uint8_t)(state + index * 3u + strobe * 97u)
                );

                input.x = 0u;
                input.controller1_state = (uint8_t)state;
                input.controller1_strobe = strobe != 0u;
                input.controller1_btn_index = (uint8_t)index;
                input.joypad_mask0 = (uint8_t)(state ^ index ^ (strobe * 0x30u));
                initialize_case(reference, &input);
                if (run_port_case(
                        reference,
                        coverage,
                        "state-index-strobe",
                        &input
                    ) != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int run_player2_preservation_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t index;
    uint32_t state;

    for (index = 0u; index <= UINT8_MAX; index++) {
        for (state = 0u; state <= UINT8_MAX; state++) {
            struct input_case input = base_input((uint8_t)(state + index));

            input.x = 1u;
            input.controller2_state = (uint8_t)state;
            input.controller2_strobe = ((state + index) & 1u) != 0u;
            input.controller2_btn_index = (uint8_t)index;
            input.joypad_mask1 = (uint8_t)(state ^ index);
            initialize_case(reference, &input);
            if (run_port_case(
                    reference,
                    coverage,
                    "player2-neutral",
                    &input
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_register_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t selector;
    uint32_t x_index;

    for (selector = 0u; selector <= UINT8_MAX; selector++) {
        struct input_case input = base_input((uint8_t)selector);

        input.x = 0u;
        input.controller1_strobe = false;
        input.controller1_btn_index = 0u;
        input.a = (uint8_t)selector;
        initialize_case(reference, &input);
        if (run_port_case(reference, coverage, "initial-a", &input) != 0) {
            return 1;
        }

        input = base_input((uint8_t)selector);
        input.x = 0u;
        input.controller1_strobe = false;
        input.controller1_btn_index = 0u;
        input.sp = (uint8_t)selector;
        initialize_case(reference, &input);
        if (run_port_case(reference, coverage, "initial-sp", &input) != 0) {
            return 1;
        }

        input = base_input((uint8_t)selector);
        input.x = 0u;
        input.controller1_strobe = false;
        input.controller1_btn_index = 0u;
        input.y = (uint8_t)selector;
        initialize_case(reference, &input);
        if (run_port_case(reference, coverage, "initial-y", &input) != 0) {
            return 1;
        }

        input = base_input((uint8_t)selector);
        input.x = 0u;
        input.controller1_strobe = false;
        input.controller1_btn_index = 0u;
        input.nz = (uint8_t)selector;
        input.carry = (selector & 1u) != 0u;
        initialize_case(reference, &input);
        if (run_port_case(reference, coverage, "initial-flags", &input) != 0) {
            return 1;
        }

        for (x_index = 0u; x_index < SAFE_X_COUNT; x_index++) {
            input = base_input((uint8_t)selector);
            input.x = safe_x_values[x_index];
            input.controller1_strobe = false;
            input.controller1_btn_index = 0u;
            initialize_case(reference, &input);
            if (run_port_case(
                    reference,
                    coverage,
                    "safe-x",
                    &input
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int run_read_joypads_exhaustion(
    struct reference_cpu *reference,
    struct coverage *coverage
) {
    uint32_t state;
    uint32_t mask;

    for (state = 0u; state <= UINT8_MAX; state++) {
        for (mask = 0u; mask <= UINT8_MAX; mask++) {
            struct input_case input = base_input((uint8_t)(state * 7u + mask));

            input.controller1_state = (uint8_t)state;
            input.controller1_btn_index = (uint8_t)(state ^ mask);
            input.controller1_strobe = ((state + mask) & 1u) != 0u;
            input.joypad_mask0 = (uint8_t)mask;
            input.joypad_mask1 = (uint8_t)(mask ^ 0xa5u);
            initialize_case(reference, &input);
            if (run_joypad_case(
                    reference,
                    coverage,
                    "read-joypads-state-mask",
                    &input
                ) != 0) {
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
    if (run_state_mask_exhaustion(&reference, &coverage) != 0 ||
        run_state_index_strobe_exhaustion(&reference, &coverage) != 0 ||
        run_player2_preservation_exhaustion(&reference, &coverage) != 0 ||
        run_register_exhaustion(&reference, &coverage) != 0 ||
        run_read_joypads_exhaustion(&reference, &coverage) != 0) {
        return 1;
    }
    if (coverage.filtered == 0u || coverage.mask_updated == 0u ||
        coverage.strobe_clear == 0u || coverage.strobe_set == 0u ||
        coverage.index_overrun == 0u || coverage.player2_port == 0u) {
        fputs("ReadPortBits differential coverage incomplete\n", stderr);
        return 1;
    }

    printf(
        "ReadJoypads/ReadPortBits differential regression: PASS "
        "(port=%" PRIu64 ", whole-read=%" PRIu64
        "; debounce filter/update=%" PRIu64 "/%" PRIu64
        ", strobe clear/set=%" PRIu64 "/%" PRIu64
        ", index-overrun=%" PRIu64 ", player2=%" PRIu64 ")\n",
        coverage.port_cases,
        coverage.joypad_cases,
        coverage.filtered,
        coverage.mask_updated,
        coverage.strobe_clear,
        coverage.strobe_set,
        coverage.index_overrun,
        coverage.player2_port
    );
    return 0;
}
