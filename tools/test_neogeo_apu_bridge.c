#include "apu_bridge.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint8_t regs[64];
    uint8_t values[64];
    size_t count;
    size_t fail_at;
} WriteCapture;

static bool capture_write(
    void *context,
    uint8_t reg,
    uint8_t value
) {
    WriteCapture *capture = context;

    if (capture->count == capture->fail_at) {
        return false;
    }
    assert(capture->count < 64u);
    capture->regs[capture->count] = reg;
    capture->values[capture->count] = value;
    ++capture->count;
    return true;
}

static uint8_t captured_value(
    const WriteCapture *capture,
    uint8_t reg
) {
    size_t index;

    for (index = capture->count; index != 0u; --index) {
        if (capture->regs[index - 1u] == reg) {
            return capture->values[index - 1u];
        }
    }
    assert(!"register was not captured");
    return 0;
}

static void reset_capture(WriteCapture *capture) {
    capture->count = 0;
    capture->fail_at = (size_t)-1;
}

static void write_pulse_timer(
    NeogeoApuBridge *bridge,
    uint8_t channel,
    uint16_t timer
) {
    uint16_t base = channel == 0u ? 0x4000u : 0x4004u;

    neogeo_apu_bridge_write(
        bridge,
        (uint16_t)(base + 2u),
        (uint8_t)timer
    );
    neogeo_apu_bridge_write(
        bridge,
        (uint16_t)(base + 3u),
        (uint8_t)(timer >> 8)
    );
}

static void test_pulse_sweep_targets_and_cadence(void) {
    NeogeoApuBridge bridge;

    /* Period-zero positive sweep clocks twice per 60 Hz bridge step. */
    neogeo_apu_bridge_init(&bridge);
    write_pulse_timer(&bridge, 0, 100u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0x81u);
    assert(bridge.pulse_sweep_mute[0] == 0u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 225u);

    /* A zero shift continuously checks mute but never updates the timer. */
    neogeo_apu_bridge_init(&bridge);
    write_pulse_timer(&bridge, 0, 100u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0x80u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 100u);
    assert(bridge.pulse_sweep_reload[0] == 0u);

    /*
     * Negative pulse 1 uses one's-complement subtraction, while pulse 2
     * uses two's-complement subtraction. Two clocks retain the one-unit gap.
     */
    neogeo_apu_bridge_init(&bridge);
    write_pulse_timer(&bridge, 0, 100u);
    write_pulse_timer(&bridge, 1, 100u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0x89u);
    neogeo_apu_bridge_write(&bridge, 0x4005u, 0x89u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 24u);
    assert(bridge.pulse_timer[1] == 25u);

    /*
     * Divider period two updates on the first zero, then every third
     * half-frame clock. A write reloads after, rather than before, the
     * current divider's update decision.
     */
    neogeo_apu_bridge_init(&bridge);
    write_pulse_timer(&bridge, 0, 100u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0xa1u);
    assert(bridge.pulse_sweep_reload[0] == 1u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 150u);
    assert(bridge.pulse_sweep_divider[0] == 1u);
    assert(bridge.pulse_sweep_reload[0] == 0u);

    neogeo_apu_bridge_write(&bridge, 0x4001u, 0xa1u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 150u);
    assert(bridge.pulse_sweep_divider[0] == 1u);
    assert(bridge.pulse_sweep_reload[0] == 0u);

    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 225u);
    assert(bridge.pulse_sweep_divider[0] == 2u);
}

static void test_pulse_sweep_mute_and_coalescing(void) {
    NeogeoApuBridge bridge;
    WriteCapture capture;

    /*
     * A valid positive target can become an overflow after one update.
     * The continuous mute calculation must stop the second half-frame clock.
     */
    neogeo_apu_bridge_init(&bridge);
    write_pulse_timer(&bridge, 0, 0x0550u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0x81u);
    assert(bridge.pulse_sweep_mute[0] == 0u);
    assert(neogeo_apu_bridge_step(&bridge, NULL, NULL));
    assert(bridge.pulse_timer[0] == 0x07f8u);
    assert(bridge.pulse_sweep_mute[0] == 1u);

    /* Positive overflow and timer periods below eight mute immediately. */
    neogeo_apu_bridge_init(&bridge);
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0x01u);
    neogeo_apu_bridge_write(&bridge, 0x4000u, 0x1fu);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0x87u);
    write_pulse_timer(&bridge, 0, 0x07ffu);
    assert(bridge.pulse_sweep_mute[0] == 1u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 7) == 0x3fu);
    assert(captured_value(&capture, 8) == 0u);

    write_pulse_timer(&bridge, 0, 0x0700u);
    assert(bridge.pulse_sweep_mute[0] == 0u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 7) == 0x3eu);
    assert(captured_value(&capture, 8) == 0x0fu);
    assert(bridge.pulse_sweep_mute[0] == 0u);

    write_pulse_timer(&bridge, 0, 7u);
    assert(bridge.pulse_sweep_mute[0] == 1u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 7) == 0x3fu);
    write_pulse_timer(&bridge, 0, 8u);
    assert(bridge.pulse_sweep_mute[0] == 0u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 7) == 0x3eu);

    /*
     * Sweep changes still use the existing changed-register coalescer. This
     * timer range changes only SSG A's fine period byte between emissions.
     */
    neogeo_apu_bridge_init(&bridge);
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0x01u);
    neogeo_apu_bridge_write(&bridge, 0x4000u, 0x1fu);
    write_pulse_timer(&bridge, 0, 100u);
    neogeo_apu_bridge_write(&bridge, 0x4001u, 0xa1u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == NEOGEO_APU_YM_REGISTER_COUNT);
    assert(bridge.pulse_timer[0] == 150u);

    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == 1u);
    assert(capture.regs[0] == 0u);
    assert(bridge.pulse_timer[0] == 225u);
}

int main(void) {
    NeogeoApuBridge bridge;
    WriteCapture capture;
    uint16_t expected_period;

    test_pulse_sweep_targets_and_cadence();
    test_pulse_sweep_mute_and_coalescing();

    reset_capture(&capture);
    neogeo_apu_bridge_init(&bridge);

    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == NEOGEO_APU_YM_REGISTER_COUNT);
    assert(captured_value(&capture, 7) == 0x3fu);
    assert(captured_value(&capture, 8) == 0u);
    assert(captured_value(&capture, 9) == 0u);
    assert(captured_value(&capture, 10) == 0u);

    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == 0u);

    /* Constant-volume pulse 1 maps to SSG A with an integer-scaled period. */
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0x01u);
    neogeo_apu_bridge_write(&bridge, 0x4000u, 0x1fu);
    neogeo_apu_bridge_write(&bridge, 0x4002u, 0xffu);
    neogeo_apu_bridge_write(&bridge, 0x4003u, 0x02u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    expected_period = (uint16_t)((768u * 286u + 128u) >> 8);
    assert(captured_value(&capture, 0) == (uint8_t)expected_period);
    assert(captured_value(&capture, 1) == (uint8_t)(expected_period >> 8));
    assert(captured_value(&capture, 7) == 0x3eu);
    assert(captured_value(&capture, 8) == 0x0fu);

    /* A4 is approximately pulse timer 253 and target SSG period 284. */
    neogeo_apu_bridge_write(&bridge, 0x4002u, 253u);
    neogeo_apu_bridge_write(&bridge, 0x4003u, 0);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    expected_period = (uint16_t)((254u * 286u + 128u) >> 8);
    assert(expected_period >= 283u && expected_period <= 285u);
    assert(captured_value(&capture, 0) == (uint8_t)expected_period);
    assert(captured_value(&capture, 1) == (uint8_t)(expected_period >> 8));

    /* Master disable silences and clears the emulated channel length. */
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 7) == 0x3fu);
    assert(captured_value(&capture, 8) == 0u);

    /* Re-enabling alone must not revive a channel without a timer-high load. */
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0x01u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == 0u);

    /*
     * Hardware-envelope mode starts at 15 on a timer-high write and decays
     * using four quarter-frame clocks per 60 Hz game frame.
     */
    neogeo_apu_bridge_write(&bridge, 0x4000u, 0x02u);
    neogeo_apu_bridge_write(&bridge, 0x4003u, 0x02u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 8) == 0x0fu);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 8) == 0x0eu);

    /* Triangle and noise share SSG C's tone/noise mixer without floats. */
    neogeo_apu_bridge_write(&bridge, 0x4015u, 0x0cu);
    neogeo_apu_bridge_write(&bridge, 0x4008u, 0x1fu);
    neogeo_apu_bridge_write(&bridge, 0x400au, 0x40u);
    neogeo_apu_bridge_write(&bridge, 0x400bu, 0x01u);
    neogeo_apu_bridge_write(&bridge, 0x400cu, 0x1au);
    neogeo_apu_bridge_write(&bridge, 0x400eu, 0x08u);
    neogeo_apu_bridge_write(&bridge, 0x400fu, 0);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    expected_period = (uint16_t)((321u * 572u + 128u) >> 8);
    assert(captured_value(&capture, 4) == (uint8_t)expected_period);
    assert(captured_value(&capture, 5) == (uint8_t)(expected_period >> 8));
    assert(captured_value(&capture, 6) == 14u);
    assert(captured_value(&capture, 7) == 0x1bu);
    assert(captured_value(&capture, 10) == 12u);

    neogeo_apu_bridge_write(&bridge, 0x400eu, 0x03u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 6) == 2u);
    neogeo_apu_bridge_write(&bridge, 0x400eu, 0x0au);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 6) == 27u);
    neogeo_apu_bridge_write(&bridge, 0x400eu, 0x0cu);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(captured_value(&capture, 6) == 31u);

    /* A failed transport write remains dirty and is retried next frame. */
    neogeo_apu_bridge_invalidate(&bridge);
    reset_capture(&capture);
    capture.fail_at = 2;
    assert(!neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == 2u);
    reset_capture(&capture);
    assert(neogeo_apu_bridge_step(&bridge, capture_write, &capture));
    assert(capture.count == NEOGEO_APU_YM_REGISTER_COUNT - 2u);
    assert(capture.regs[0] == 3u);

    puts("Neo Geo APU bridge tests: OK");
    return 0;
}
