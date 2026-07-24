#ifndef SMB_NEOGEO_APU_BRIDGE_H
#define SMB_NEOGEO_APU_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    NEOGEO_APU_YM_REGISTER_COUNT = 11
};

typedef bool (*NeogeoApuYmWrite)(
    void *context,
    uint8_t reg,
    uint8_t value
);

typedef struct {
    uint8_t control;
    uint8_t decay;
    uint8_t divider;
} NeogeoApuEnvelope;

typedef struct {
    uint8_t pulse_control[2];
    uint8_t pulse_sweep_control[2];
    uint8_t pulse_sweep_divider[2];
    uint8_t pulse_sweep_reload[2];
    uint8_t pulse_sweep_mute[2];
    uint16_t pulse_timer[2];
    uint8_t pulse_active[2];
    NeogeoApuEnvelope pulse_envelope[2];

    uint8_t triangle_control;
    uint16_t triangle_timer;
    uint8_t triangle_active;

    uint8_t noise_control;
    uint8_t noise_period;
    uint8_t noise_active;
    NeogeoApuEnvelope noise_envelope;

    uint8_t master_enable;
    uint8_t sent_registers[NEOGEO_APU_YM_REGISTER_COUNT];
    uint16_t sent_valid;
} NeogeoApuBridge;

void neogeo_apu_bridge_init(NeogeoApuBridge *bridge);
void neogeo_apu_bridge_write(
    NeogeoApuBridge *bridge,
    uint16_t address,
    uint8_t value
);

/*
 * Emit only changed YM2610 SSG registers, then advance the three NES-style
 * volume envelopes and two pulse sweep units by one 60 Hz game frame. A null
 * writer advances state without acknowledging any registers, which is useful
 * while the Z80 starts.
 */
bool neogeo_apu_bridge_step(
    NeogeoApuBridge *bridge,
    NeogeoApuYmWrite writer,
    void *context
);

/* Force a complete register snapshot after a Z80 reset or transport fault. */
void neogeo_apu_bridge_invalidate(NeogeoApuBridge *bridge);

#endif
