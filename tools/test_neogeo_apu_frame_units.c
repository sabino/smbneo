#include "apu_frame_units.h"

#include <stdint.h>
#include <stdio.h>

static void reference_envelope_4(
    uint8_t control,
    uint8_t *decay,
    uint8_t *divider
) {
    uint8_t tick;

    for (tick = 0; tick < 4u; ++tick) {
        if ((control & 0x10u) != 0u) {
            continue;
        }
        if (*divider != 0u) {
            --*divider;
            continue;
        }
        *divider = (uint8_t)(control & 0x0fu);
        if (*decay != 0u) {
            --*decay;
        } else if ((control & 0x20u) != 0u) {
            *decay = 0x0fu;
        }
    }
}

static void reference_triangle_linear_4(
    uint8_t control,
    uint8_t *counter,
    uint8_t *reload
) {
    uint8_t tick;

    for (tick = 0; tick < 4u; ++tick) {
        if (*reload != 0u) {
            *counter = (uint8_t)(control & 0x7fu);
        } else if (*counter != 0u) {
            --*counter;
        }
        if ((control & 0x80u) == 0u) {
            *reload = 0u;
        }
    }
}

static void reference_length_2(uint8_t *counter, uint8_t halted) {
    uint8_t tick;

    for (tick = 0; tick < 2u; ++tick) {
        if (halted == 0u && *counter != 0u) {
            --*counter;
        }
    }
}


static int test_envelopes(void) {
    uint16_t control;
    uint16_t decay;
    uint16_t divider;

    for (control = 0; control <= UINT8_MAX; ++control) {
        for (decay = 0; decay <= UINT8_MAX; ++decay) {
            for (divider = 0; divider <= UINT8_MAX; ++divider) {
                uint8_t expected_decay = (uint8_t)decay;
                uint8_t expected_divider = (uint8_t)divider;
                uint8_t actual_decay = (uint8_t)decay;
                uint8_t actual_divider = (uint8_t)divider;

                reference_envelope_4(
                    (uint8_t)control,
                    &expected_decay,
                    &expected_divider
                );
                neogeo_apu_clock_envelope_4(
                    (uint8_t)control,
                    &actual_decay,
                    &actual_divider
                );
                if (
                    expected_decay != actual_decay ||
                    expected_divider != actual_divider
                ) {
                    fprintf(
                        stderr,
                        "envelope mismatch control=%u decay=%u divider=%u\n",
                        control,
                        decay,
                        divider
                    );
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int test_triangle_linear(void) {
    uint16_t control;
    uint16_t counter;
    uint16_t reload;

    for (control = 0; control <= UINT8_MAX; ++control) {
        for (counter = 0; counter <= UINT8_MAX; ++counter) {
            for (reload = 0; reload <= UINT8_MAX; ++reload) {
                uint8_t expected_counter = (uint8_t)counter;
                uint8_t expected_reload = (uint8_t)reload;
                uint8_t actual_counter = (uint8_t)counter;
                uint8_t actual_reload = (uint8_t)reload;

                reference_triangle_linear_4(
                    (uint8_t)control,
                    &expected_counter,
                    &expected_reload
                );
                neogeo_apu_clock_triangle_linear_4(
                    (uint8_t)control,
                    &actual_counter,
                    &actual_reload
                );
                if (
                    expected_counter != actual_counter ||
                    expected_reload != actual_reload
                ) {
                    fprintf(
                        stderr,
                        "linear mismatch control=%u counter=%u reload=%u\n",
                        control,
                        counter,
                        reload
                    );
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int test_lengths(void) {
    uint16_t counter;
    uint16_t halted;

    for (counter = 0; counter <= UINT8_MAX; ++counter) {
        for (halted = 0; halted <= UINT8_MAX; ++halted) {
            uint8_t expected = (uint8_t)counter;
            uint8_t actual = (uint8_t)counter;

            reference_length_2(&expected, (uint8_t)halted);
            neogeo_apu_clock_length_2(&actual, (uint8_t)halted);
            if (expected != actual) {
                fprintf(
                    stderr,
                    "length mismatch counter=%u halted=%u\n",
                    counter,
                    halted
                );
                return 0;
            }
        }
    }
    return 1;
}

int main(void) {
    if (
        !test_envelopes() ||
        !test_triangle_linear() ||
        !test_lengths()
    ) {
        return 1;
    }
    puts("Neo Geo APU frame-unit exhaustive differential: OK");
    return 0;
}
