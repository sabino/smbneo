#ifndef SMB_NEOGEO_APU_FRAME_UNITS_H
#define SMB_NEOGEO_APU_FRAME_UNITS_H

#include <stdint.h>

/* High nibble: decay events; low nibble: final divider. */
static const uint8_t neogeo_apu_envelope_4_transition[64] = {
    0x40, 0x30, 0x20, 0x10, 0x20, 0x21, 0x10, 0x11,
    0x22, 0x10, 0x11, 0x12, 0x10, 0x11, 0x12, 0x13,
    0x11, 0x12, 0x13, 0x14, 0x12, 0x13, 0x14, 0x15,
    0x13, 0x14, 0x15, 0x16, 0x14, 0x15, 0x16, 0x17,
    0x15, 0x16, 0x17, 0x18, 0x16, 0x17, 0x18, 0x19,
    0x17, 0x18, 0x19, 0x1a, 0x18, 0x19, 0x1a, 0x1b,
    0x19, 0x1a, 0x1b, 0x1c, 0x1a, 0x1b, 0x1c, 0x1d,
    0x1b, 0x1c, 0x1d, 0x1e, 0x1c, 0x1d, 0x1e, 0x1f,
};

/*
 * SMBNeo advances four source quarter-frame clocks per target game frame.
 * Collapse the tiny fixed loop into the same divider/event arithmetic so the
 * MC68000 does not execute three separately inlined envelope state machines
 * four times per frame.
 */
static inline void neogeo_apu_clock_envelope_4(
    uint8_t control,
    uint8_t *decay,
    uint8_t *divider
) {
    uint8_t events;
    uint8_t period;
    uint8_t transition;

    if ((control & 0x10u) != 0u) {
        return;
    }
    if (*divider >= 4u) {
        *divider = (uint8_t)(*divider - 4u);
        return;
    }

    period = (uint8_t)(control & 0x0fu);
    transition = neogeo_apu_envelope_4_transition[
        (uint8_t)((period << 2) | *divider)
    ];
    events = (uint8_t)(transition >> 4);
    *divider = (uint8_t)(transition & 0x0fu);

    if (*decay >= events) {
        *decay = (uint8_t)(*decay - events);
    } else if ((control & 0x20u) != 0u) {
        *decay = (uint8_t)(16u - (uint8_t)(events - *decay));
    } else {
        *decay = 0u;
    }
}

static inline void neogeo_apu_clock_triangle_linear_4(
    uint8_t control,
    uint8_t *counter,
    uint8_t *reload
) {
    uint8_t reload_value = (uint8_t)(control & 0x7fu);

    if ((control & 0x80u) != 0u) {
        if (*reload != 0u) {
            *counter = reload_value;
        } else {
            *counter = *counter > 4u
                ? (uint8_t)(*counter - 4u)
                : 0u;
        }
        return;
    }

    if (*reload != 0u) {
        *counter = reload_value > 3u
            ? (uint8_t)(reload_value - 3u)
            : 0u;
    } else {
        *counter = *counter > 4u
            ? (uint8_t)(*counter - 4u)
            : 0u;
    }
    *reload = 0u;
}

static inline void neogeo_apu_clock_length_2(
    uint8_t *counter,
    uint8_t halted
) {
    if (halted == 0u) {
        *counter = *counter > 2u
            ? (uint8_t)(*counter - 2u)
            : 0u;
    }
}

#endif
