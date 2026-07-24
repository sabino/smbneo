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

    SSG_ALL_CHANNELS_DISABLED = 0x3f,
    SSG_TONE_A_DISABLED = 0x01,
    SSG_TONE_B_DISABLED = 0x02,
    SSG_TONE_C_DISABLED = 0x04,
    SSG_NOISE_C_DISABLED = 0x20,

    NES_ENVELOPE_TICKS_PER_FRAME = 4,
    NES_SWEEP_TICKS_PER_FRAME = 2,
    NES_SWEEP_ENABLED = 0x80,
    NES_SWEEP_PERIOD_MASK = 0x70,
    NES_SWEEP_NEGATE = 0x08,
    NES_SWEEP_SHIFT_MASK = 0x07,
    NES_PULSE_TIMER_MAX = 0x07ff
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
        bridge->pulse_active[channel] != 0u &&
        bridge->pulse_timer[channel] >= 8u &&
        bridge->pulse_sweep_mute[channel] == 0u &&
        volume != 0u
    );
}

static uint8_t triangle_is_audible(const NeogeoApuBridge *bridge) {
    return (uint8_t)(
        (bridge->master_enable & 0x04u) != 0u &&
        bridge->triangle_active != 0u &&
        (bridge->triangle_control & 0x7fu) != 0u
    );
}

static uint8_t noise_is_audible(
    const NeogeoApuBridge *bridge,
    uint8_t volume
) {
    return (uint8_t)(
        (bridge->master_enable & 0x08u) != 0u &&
        bridge->noise_active != 0u &&
        volume != 0u
    );
}

static void build_ym_registers(
    const NeogeoApuBridge *bridge,
    uint8_t registers[NEOGEO_APU_YM_REGISTER_COUNT]
) {
    static const uint8_t noise_periods[16] = {
        1, 1, 1, 2, 4, 7, 9, 11,
        14, 18, 27, 31, 31, 31, 31, 31
    };
    uint16_t period;
    uint8_t pulse_volume[2];
    uint8_t triangle_volume;
    uint8_t noise_volume;
    uint8_t mixer = SSG_ALL_CHANNELS_DISABLED;
    uint8_t channel;

    memset(registers, 0, NEOGEO_APU_YM_REGISTER_COUNT);

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

    period = scale_period(bridge->triangle_timer, 572u);
    registers[4] = (uint8_t)period;
    registers[5] = (uint8_t)(period >> 8);
    registers[6] = noise_periods[bridge->noise_period & 0x0fu];

    noise_volume = envelope_volume(
        bridge->noise_control,
        &bridge->noise_envelope
    );
    triangle_volume = triangle_is_audible(bridge) != 0u ? 12u : 0u;

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
    if (triangle_volume != 0u) {
        mixer &= (uint8_t)~SSG_TONE_C_DISABLED;
    }
    if (noise_is_audible(bridge, noise_volume) != 0u) {
        mixer &= (uint8_t)~SSG_NOISE_C_DISABLED;
    } else {
        noise_volume = 0;
    }

    registers[YM_SSG_MIXER] = mixer;
    registers[YM_SSG_VOLUME_A] = pulse_volume[0];
    registers[YM_SSG_VOLUME_B] = pulse_volume[1];
    registers[YM_SSG_VOLUME_C] =
        triangle_volume > noise_volume ? triangle_volume : noise_volume;
}

void neogeo_apu_bridge_init(NeogeoApuBridge *bridge) {
    memset(bridge, 0, sizeof(*bridge));
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
            bridge->pulse_active[channel] = 1;
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
            break;

        case NES_APU_TRIANGLE_TIMER_HIGH:
            bridge->triangle_timer =
                (uint16_t)(
                    (bridge->triangle_timer & 0x00ffu) |
                    (((uint16_t)value & 0x07u) << 8)
                );
            bridge->triangle_active = 1;
            break;

        case NES_APU_NOISE_CONTROL:
            bridge->noise_control = value;
            break;

        case NES_APU_NOISE_PERIOD:
            bridge->noise_period = (uint8_t)(value & 0x0fu);
            break;

        case NES_APU_NOISE_LENGTH:
            bridge->noise_active = 1;
            restart_envelope(
                &bridge->noise_envelope,
                bridge->noise_control
            );
            break;

        case NES_APU_MASTER_ENABLE:
            if ((value & 0x01u) == 0u) {
                bridge->pulse_active[0] = 0;
            }
            if ((value & 0x02u) == 0u) {
                bridge->pulse_active[1] = 0;
            }
            if ((value & 0x04u) == 0u) {
                bridge->triangle_active = 0;
            }
            if ((value & 0x08u) == 0u) {
                bridge->noise_active = 0;
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
         * Yamaha recommends coarse-before-fine tone updates. The remaining
         * SSG registers are independent and retain their numeric order.
         */
        1, 0, 3, 2, 5, 4, 6, 7, 8, 9, 10
    };
    uint8_t registers[NEOGEO_APU_YM_REGISTER_COUNT];
    uint8_t order_index;

    build_ym_registers(bridge, registers);
    if (writer != NULL) {
        for (
            order_index = 0;
            order_index < NEOGEO_APU_YM_REGISTER_COUNT;
            ++order_index
        ) {
            uint8_t reg = write_order[order_index];
            uint16_t mask = (uint16_t)(1u << reg);

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
