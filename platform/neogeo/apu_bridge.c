#include "apu_bridge.h"

#include <string.h>

enum {
    NES_APU_PULSE1_CONTROL = 0x4000,
    NES_APU_PULSE1_SWEEP = 0x4001,
    NES_APU_PULSE1_TIMER_LOW = 0x4002,
    NES_APU_PULSE1_TIMER_HIGH = 0x4003,
    NES_APU_PULSE2_CONTROL = 0x4004,
    NES_APU_PULSE2_SWEEP = 0x4005,
    NES_APU_PULSE2_TIMER_LOW = 0x4006,
    NES_APU_PULSE2_TIMER_HIGH = 0x4007,
    NES_APU_TRIANGLE_CONTROL = 0x4008,
    NES_APU_TRIANGLE_TIMER_LOW = 0x400a,
    NES_APU_TRIANGLE_TIMER_HIGH = 0x400b,
    NES_APU_NOISE_CONTROL = 0x400c,
    NES_APU_NOISE_PERIOD = 0x400e,
    NES_APU_NOISE_LENGTH = 0x400f,
    NES_APU_MASTER_ENABLE = 0x4015,

    YM_SSG_MIXER = 7,
    YM_SSG_VOLUME_A = 8,
    YM_SSG_VOLUME_B = 9,
    YM_SSG_VOLUME_C = 10,
    YM_ADPCM_B_CONTROL = 0x10,
    YM_ADPCM_B_PAN = 0x11,
    YM_ADPCM_B_START_LOW = 0x12,
    YM_ADPCM_B_START_HIGH = 0x13,
    YM_ADPCM_B_STOP_LOW = 0x14,
    YM_ADPCM_B_STOP_HIGH = 0x15,
    YM_ADPCM_B_DELTA_LOW = 0x19,
    YM_ADPCM_B_DELTA_HIGH = 0x1a,
    YM_ADPCM_B_VOLUME = 0x1b,

    SSG_ALL_CHANNELS_DISABLED = 0x3f,
    SSG_TONE_A_DISABLED = 0x01,
    SSG_TONE_B_DISABLED = 0x02,
    SSG_NOISE_C_DISABLED = 0x20,

    NES_ENVELOPE_TICKS_PER_FRAME = 4,
    NES_LINEAR_TICKS_PER_FRAME = 4,
    NES_LENGTH_TICKS_PER_FRAME = 2,
    NES_SWEEP_TICKS_PER_FRAME = 2,
    NES_SWEEP_ENABLED = 0x80,
    NES_SWEEP_PERIOD_MASK = 0x70,
    NES_SWEEP_NEGATE = 0x08,
    NES_SWEEP_SHIFT_MASK = 0x07,
    NES_PULSE_TIMER_MAX = 0x07ff,

    /*
     * The V1 generator emits a 64 KiB ADPCM-B loop at offset zero. YM2610
     * sample addresses count inclusive 256-byte blocks.
     */
    ADPCM_B_TRIANGLE_STOP_BLOCK = 0x00ff,
    ADPCM_B_TRIANGLE_VOLUME = 0x70,
    ADPCM_B_TRIANGLE_START_REPEAT = 0x90,
    ADPCM_B_TRIANGLE_RESET = 0x01
};

static const uint8_t length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

/*
 * The target SSG tone period is 8MHz/(64*frequency). Combining that with
 * the NES pulse and triangle timer equations gives approximately 1.117
 * and 2.235 target period units per source timer unit. These 8.8 factors
 * avoid division and all floating-point support on the MC68000.
 */
static uint16_t scale_period(uint16_t timer, uint16_t factor_8_8) {
    uint32_t scaled =
        ((uint32_t)(timer + 1u) * factor_8_8 + 128u) >> 8;

    if (scaled == 0u) {
        return 1u;
    }
    if (scaled > 0x0fffu) {
        return 0x0fffu;
    }
    return (uint16_t)scaled;
}

/*
 * V1 contains a 64-sample triangle period. Matching the source triangle
 * frequency therefore requires this ADPCM-B nibble rate:
 *
 *   delta-n = round(2 * 1,789,773 * 65,536 * 144
 *                   / (8,000,000 * (timer + 1)))
 *           = round(4,222,604.28 / (timer + 1)).
 *
 * This division runs only when a triangle timer byte is written, never in
 * the 60 Hz register-emission path. Timers whose quotient exceeds the
 * 16-bit YM register are clamped.
 */
static uint16_t triangle_delta_n(uint16_t timer) {
    uint16_t divisor = (uint16_t)(timer + 1u);
    uint32_t delta;

    if (divisor < 65u) {
        return 0xffffu;
    }
    delta = (4222604u + (uint32_t)(divisor / 2u)) / divisor;
    if (delta > 0xffffu) {
        return 0xffffu;
    }
    return (uint16_t)delta;
}

static uint8_t envelope_volume(
    uint8_t control,
    const NeogeoApuEnvelope *envelope
) {
    if ((control & 0x10u) != 0u) {
        return (uint8_t)(control & 0x0fu);
    }
    return envelope->decay;
}

static void restart_envelope(
    NeogeoApuEnvelope *envelope,
    uint8_t control
) {
    envelope->decay = 0x0fu;
    envelope->divider = (uint8_t)(control & 0x0fu);
}

static void clock_envelope(
    NeogeoApuEnvelope *envelope,
    uint8_t control
) {
    if ((control & 0x10u) != 0u) {
        return;
    }
    if (envelope->divider != 0u) {
        --envelope->divider;
        return;
    }

    envelope->divider = (uint8_t)(control & 0x0fu);
    if (envelope->decay != 0u) {
        --envelope->decay;
    } else if ((control & 0x20u) != 0u) {
        envelope->decay = 0x0fu;
    }
}

static void clock_frame_envelopes(NeogeoApuBridge *bridge) {
    uint8_t tick;

    for (tick = 0; tick < NES_ENVELOPE_TICKS_PER_FRAME; ++tick) {
        clock_envelope(
            &bridge->pulse_envelope[0],
            bridge->pulse_control[0]
        );
        clock_envelope(
            &bridge->pulse_envelope[1],
            bridge->pulse_control[1]
        );
        clock_envelope(
            &bridge->noise_envelope,
            bridge->noise_control
        );
    }
}

static void clock_triangle_linear_counter(
    NeogeoApuBridge *bridge
) {
    uint8_t tick;

    for (tick = 0; tick < NES_LINEAR_TICKS_PER_FRAME; ++tick) {
        if (bridge->triangle_linear_reload != 0u) {
            bridge->triangle_linear_counter =
                (uint8_t)(bridge->triangle_control & 0x7fu);
        } else if (bridge->triangle_linear_counter != 0u) {
            --bridge->triangle_linear_counter;
        }

        if ((bridge->triangle_control & 0x80u) == 0u) {
            bridge->triangle_linear_reload = 0;
        }
    }
}

static void clock_length_counter(
    uint8_t *counter,
    uint8_t halted
) {
    if (halted == 0u && *counter != 0u) {
        --*counter;
    }
}

static void clock_frame_length_counters(NeogeoApuBridge *bridge) {
    uint8_t tick;

    for (tick = 0; tick < NES_LENGTH_TICKS_PER_FRAME; ++tick) {
        clock_length_counter(
            &bridge->pulse_length[0],
            (uint8_t)(bridge->pulse_control[0] & 0x20u)
        );
        clock_length_counter(
            &bridge->pulse_length[1],
            (uint8_t)(bridge->pulse_control[1] & 0x20u)
        );
        clock_length_counter(
            &bridge->triangle_length,
            (uint8_t)(bridge->triangle_control & 0x80u)
        );
        clock_length_counter(
            &bridge->noise_length,
            (uint8_t)(bridge->noise_control & 0x20u)
        );
    }
}

static uint16_t pulse_sweep_target_period(
    const NeogeoApuBridge *bridge,
    uint8_t channel,
    uint8_t *positive_overflow
) {
    uint8_t control = bridge->pulse_sweep_control[channel];
    uint8_t shift = (uint8_t)(control & NES_SWEEP_SHIFT_MASK);
    uint16_t timer = bridge->pulse_timer[channel];
    uint16_t change = (uint16_t)(timer >> shift);

    *positive_overflow = 0;
    if ((control & NES_SWEEP_NEGATE) != 0u) {
        uint16_t subtraction = (uint16_t)(
            change + (channel == 0u ? 1u : 0u)
        );

        return subtraction > timer
            ? 0u
            : (uint16_t)(timer - subtraction);
    }

    timer = (uint16_t)(timer + change);
    if (timer > NES_PULSE_TIMER_MAX) {
        *positive_overflow = 1;
    }
    return timer;
}

static void refresh_pulse_sweep_mute(
    NeogeoApuBridge *bridge,
    uint8_t channel
) {
    uint8_t positive_overflow;

    (void)pulse_sweep_target_period(
        bridge,
        channel,
        &positive_overflow
    );
    bridge->pulse_sweep_mute[channel] = (uint8_t)(
        bridge->pulse_timer[channel] < 8u ||
        positive_overflow != 0u
    );
}

static void clock_pulse_sweep(
    NeogeoApuBridge *bridge,
    uint8_t channel
) {
    uint8_t control = bridge->pulse_sweep_control[channel];
    uint8_t divider_was_zero =
        bridge->pulse_sweep_divider[channel] == 0u;
    uint8_t positive_overflow;
    uint16_t target = pulse_sweep_target_period(
        bridge,
        channel,
        &positive_overflow
    );

    bridge->pulse_sweep_mute[channel] = (uint8_t)(
        bridge->pulse_timer[channel] < 8u ||
        positive_overflow != 0u
    );
    if (
        divider_was_zero != 0u &&
        (control & NES_SWEEP_ENABLED) != 0u &&
        (control & NES_SWEEP_SHIFT_MASK) != 0u &&
        bridge->pulse_sweep_mute[channel] == 0u
    ) {
        bridge->pulse_timer[channel] = target;
        refresh_pulse_sweep_mute(bridge, channel);
    }

    /*
     * The target update uses the divider's pre-clock state. Reloading is a
     * separate, subsequent action, matching the source sweep-unit ordering.
     */
    if (
        divider_was_zero != 0u ||
        bridge->pulse_sweep_reload[channel] != 0u
    ) {
        bridge->pulse_sweep_divider[channel] = (uint8_t)(
            (control & NES_SWEEP_PERIOD_MASK) >> 4
        );
        bridge->pulse_sweep_reload[channel] = 0;
    } else {
        --bridge->pulse_sweep_divider[channel];
    }
}

static void clock_frame_sweeps(NeogeoApuBridge *bridge) {
    uint8_t tick;

    for (tick = 0; tick < NES_SWEEP_TICKS_PER_FRAME; ++tick) {
        clock_pulse_sweep(bridge, 0);
        clock_pulse_sweep(bridge, 1);
    }
}

static void clock_frame_units(NeogeoApuBridge *bridge) {
    clock_frame_envelopes(bridge);
    clock_triangle_linear_counter(bridge);
    clock_frame_length_counters(bridge);
    clock_frame_sweeps(bridge);
}

static uint8_t pulse_is_audible(
    const NeogeoApuBridge *bridge,
    uint8_t channel,
    uint8_t volume
) {
    uint8_t enable_mask = (uint8_t)(1u << channel);

    return (uint8_t)(
        (bridge->master_enable & enable_mask) != 0u &&
        bridge->pulse_length[channel] != 0u &&
        bridge->pulse_timer[channel] >= 8u &&
        bridge->pulse_sweep_mute[channel] == 0u &&
        volume != 0u
    );
}

static uint8_t triangle_is_audible(const NeogeoApuBridge *bridge) {
    uint8_t linear_active = (uint8_t)(
        bridge->triangle_linear_counter != 0u ||
        (
            bridge->triangle_linear_reload != 0u &&
            (bridge->triangle_control & 0x7fu) != 0u
        )
    );

    return (uint8_t)(
        (bridge->master_enable & 0x04u) != 0u &&
        bridge->triangle_length != 0u &&
        linear_active != 0u &&
        bridge->triangle_timer > 2u
    );
}

static uint8_t noise_is_audible(
    const NeogeoApuBridge *bridge,
    uint8_t volume
) {
    return (uint8_t)(
        (bridge->master_enable & 0x08u) != 0u &&
        bridge->noise_length != 0u &&
        volume != 0u
    );
}

static void build_ym_registers(
    const NeogeoApuBridge *bridge,
    uint8_t registers[NEOGEO_APU_YM_REGISTER_LIMIT]
) {
    static const uint8_t noise_periods[16] = {
        1, 1, 1, 2, 4, 7, 9, 11,
        14, 18, 27, 31, 31, 31, 31, 31
    };
    uint16_t period;
    uint8_t pulse_volume[2];
    uint8_t noise_volume;
    uint8_t mixer = SSG_ALL_CHANNELS_DISABLED;
    uint8_t channel;

    memset(registers, 0, NEOGEO_APU_YM_REGISTER_LIMIT);

    for (channel = 0; channel < 2u; ++channel) {
        uint8_t base = (uint8_t)(channel * 2u);

        period = scale_period(bridge->pulse_timer[channel], 286u);
        registers[base] = (uint8_t)period;
        registers[base + 1u] = (uint8_t)(period >> 8);
        pulse_volume[channel] = envelope_volume(
            bridge->pulse_control[channel],
            &bridge->pulse_envelope[channel]
        );
    }

    registers[6] = noise_periods[bridge->noise_period & 0x0fu];

    noise_volume = envelope_volume(
        bridge->noise_control,
        &bridge->noise_envelope
    );

    if (pulse_is_audible(bridge, 0, pulse_volume[0]) != 0u) {
        mixer &= (uint8_t)~SSG_TONE_A_DISABLED;
    } else {
        pulse_volume[0] = 0;
    }
    if (pulse_is_audible(bridge, 1, pulse_volume[1]) != 0u) {
        mixer &= (uint8_t)~SSG_TONE_B_DISABLED;
    } else {
        pulse_volume[1] = 0;
    }
    if (noise_is_audible(bridge, noise_volume) != 0u) {
        mixer &= (uint8_t)~SSG_NOISE_C_DISABLED;
    } else {
        noise_volume = 0;
    }

    registers[YM_SSG_MIXER] = mixer;
    registers[YM_SSG_VOLUME_A] = pulse_volume[0];
    registers[YM_SSG_VOLUME_B] = pulse_volume[1];
    registers[YM_SSG_VOLUME_C] = noise_volume;

    /*
     * ADPCM-B is an independent fourth voice. The sample is kept looping
     * through a note and pitch changes update delta-n without retriggering
     * the predictor; control changes only on audible edges.
     */
    registers[YM_ADPCM_B_CONTROL] =
        triangle_is_audible(bridge) != 0u
            ? ADPCM_B_TRIANGLE_START_REPEAT
            : ADPCM_B_TRIANGLE_RESET;
    registers[YM_ADPCM_B_PAN] = 0xc0u;
    registers[YM_ADPCM_B_START_LOW] = 0;
    registers[YM_ADPCM_B_START_HIGH] = 0;
    registers[YM_ADPCM_B_STOP_LOW] =
        (uint8_t)ADPCM_B_TRIANGLE_STOP_BLOCK;
    registers[YM_ADPCM_B_STOP_HIGH] =
        (uint8_t)(ADPCM_B_TRIANGLE_STOP_BLOCK >> 8);
    registers[YM_ADPCM_B_DELTA_LOW] =
        (uint8_t)bridge->triangle_delta_n;
    registers[YM_ADPCM_B_DELTA_HIGH] =
        (uint8_t)(bridge->triangle_delta_n >> 8);
    registers[YM_ADPCM_B_VOLUME] = ADPCM_B_TRIANGLE_VOLUME;
}

void neogeo_apu_bridge_init(NeogeoApuBridge *bridge) {
    memset(bridge, 0, sizeof(*bridge));
    bridge->triangle_delta_n = triangle_delta_n(0);
    refresh_pulse_sweep_mute(bridge, 0);
    refresh_pulse_sweep_mute(bridge, 1);
}

void neogeo_apu_bridge_write(
    NeogeoApuBridge *bridge,
    uint16_t address,
    uint8_t value
) {
    uint8_t channel;

    switch (address) {
        case NES_APU_PULSE1_CONTROL:
        case NES_APU_PULSE2_CONTROL:
            channel =
                address == NES_APU_PULSE1_CONTROL ? 0u : 1u;
            bridge->pulse_control[channel] = value;
            break;

        case NES_APU_PULSE1_SWEEP:
        case NES_APU_PULSE2_SWEEP:
            channel =
                address == NES_APU_PULSE1_SWEEP ? 0u : 1u;
            bridge->pulse_sweep_control[channel] = value;
            bridge->pulse_sweep_reload[channel] = 1;
            refresh_pulse_sweep_mute(bridge, channel);
            break;

        case NES_APU_PULSE1_TIMER_LOW:
        case NES_APU_PULSE2_TIMER_LOW:
            channel =
                address == NES_APU_PULSE1_TIMER_LOW ? 0u : 1u;
            bridge->pulse_timer[channel] =
                (uint16_t)(
                    (bridge->pulse_timer[channel] & 0x0700u) | value
                );
            refresh_pulse_sweep_mute(bridge, channel);
            break;

        case NES_APU_PULSE1_TIMER_HIGH:
        case NES_APU_PULSE2_TIMER_HIGH:
            channel =
                address == NES_APU_PULSE1_TIMER_HIGH ? 0u : 1u;
            bridge->pulse_timer[channel] =
                (uint16_t)(
                    (bridge->pulse_timer[channel] & 0x00ffu) |
                    (((uint16_t)value & 0x07u) << 8)
                );
            if (
                (
                    bridge->master_enable &
                    (uint8_t)(1u << channel)
                ) != 0u
            ) {
                bridge->pulse_length[channel] =
                    length_table[value >> 3];
            }
            restart_envelope(
                &bridge->pulse_envelope[channel],
                bridge->pulse_control[channel]
            );
            refresh_pulse_sweep_mute(bridge, channel);
            break;

        case NES_APU_TRIANGLE_CONTROL:
            bridge->triangle_control = value;
            break;

        case NES_APU_TRIANGLE_TIMER_LOW:
            bridge->triangle_timer =
                (uint16_t)((bridge->triangle_timer & 0x0700u) | value);
            bridge->triangle_delta_n =
                triangle_delta_n(bridge->triangle_timer);
            break;

        case NES_APU_TRIANGLE_TIMER_HIGH:
            bridge->triangle_timer =
                (uint16_t)(
                    (bridge->triangle_timer & 0x00ffu) |
                    (((uint16_t)value & 0x07u) << 8)
                );
            bridge->triangle_delta_n =
                triangle_delta_n(bridge->triangle_timer);
            if ((bridge->master_enable & 0x04u) != 0u) {
                bridge->triangle_length = length_table[value >> 3];
            }
            bridge->triangle_linear_reload = 1;
            break;

        case NES_APU_NOISE_CONTROL:
            bridge->noise_control = value;
            break;

        case NES_APU_NOISE_PERIOD:
            bridge->noise_period = (uint8_t)(value & 0x0fu);
            bridge->noise_mode = (uint8_t)(value >> 7);
            break;

        case NES_APU_NOISE_LENGTH:
            if ((bridge->master_enable & 0x08u) != 0u) {
                bridge->noise_length = length_table[value >> 3];
            }
            restart_envelope(
                &bridge->noise_envelope,
                bridge->noise_control
            );
            break;

        case NES_APU_MASTER_ENABLE:
            if ((value & 0x01u) == 0u) {
                bridge->pulse_length[0] = 0;
            }
            if ((value & 0x02u) == 0u) {
                bridge->pulse_length[1] = 0;
            }
            if ((value & 0x04u) == 0u) {
                bridge->triangle_length = 0;
            }
            if ((value & 0x08u) == 0u) {
                bridge->noise_length = 0;
            }
            bridge->master_enable = (uint8_t)(value & 0x0fu);
            break;

        default:
            break;
    }
}

bool neogeo_apu_bridge_step(
    NeogeoApuBridge *bridge,
    NeogeoApuYmWrite writer,
    void *context
) {
    static const uint8_t write_order[NEOGEO_APU_YM_REGISTER_COUNT] = {
        /*
         * Yamaha recommends coarse-before-fine SSG tone updates. ADPCM-B
         * configuration precedes control so a first start or transport
         * recovery cannot run against stale sample boundaries or volume.
         * ngdevkit's documented delta-n sequence is low byte, then high.
         */
        1, 0, 3, 2, 6, 7, 8, 9, 10,
        YM_ADPCM_B_PAN,
        YM_ADPCM_B_START_LOW, YM_ADPCM_B_START_HIGH,
        YM_ADPCM_B_STOP_LOW, YM_ADPCM_B_STOP_HIGH,
        YM_ADPCM_B_DELTA_LOW, YM_ADPCM_B_DELTA_HIGH,
        YM_ADPCM_B_VOLUME,
        YM_ADPCM_B_CONTROL
    };
    uint8_t registers[NEOGEO_APU_YM_REGISTER_LIMIT];
    uint8_t order_index;

    build_ym_registers(bridge, registers);
    if (writer != NULL) {
        for (
            order_index = 0;
            order_index < NEOGEO_APU_YM_REGISTER_COUNT;
            ++order_index
        ) {
            uint8_t reg = write_order[order_index];
            uint32_t mask = (uint32_t)(1ul << reg);

            if (
                (bridge->sent_valid & mask) != 0u &&
                bridge->sent_registers[reg] == registers[reg]
            ) {
                continue;
            }
            if (!writer(context, reg, registers[reg])) {
                clock_frame_units(bridge);
                return false;
            }
            bridge->sent_registers[reg] = registers[reg];
            bridge->sent_valid |= mask;
        }
    }

    clock_frame_units(bridge);
    return true;
}

void neogeo_apu_bridge_invalidate(NeogeoApuBridge *bridge) {
    bridge->sent_valid = 0;
}
