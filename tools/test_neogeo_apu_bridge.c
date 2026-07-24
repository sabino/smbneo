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

int main(void) {
    NeogeoApuBridge bridge;
    WriteCapture capture;
    uint16_t expected_period;

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
