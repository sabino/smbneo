#ifndef SMB_NEOGEO_APU_BRIDGE_H
#define SMB_NEOGEO_APU_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    /*
     * The bridge emits nine SSG registers and nine ADPCM-B registers.
     * REGISTER_LIMIT is the exclusive upper bound used by the coalescer;
     * REGISTER_COUNT is the number of registers in the explicit write order.
     */
    NEOGEO_APU_YM_REGISTER_LIMIT = 0x1c,
    NEOGEO_APU_YM_REGISTER_COUNT = 18
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
    uint8_t pulse_length[2];
    NeogeoApuEnvelope pulse_envelope[2];

    uint8_t triangle_control;
    uint16_t triangle_timer;
    uint16_t triangle_delta_n;
    uint8_t triangle_length;
    uint8_t triangle_linear_counter;
    uint8_t triangle_linear_reload;

    uint8_t noise_control;
    uint8_t noise_period;
    uint8_t noise_mode;
    uint8_t noise_length;
    NeogeoApuEnvelope noise_envelope;

    uint8_t master_enable;
    uint8_t sent_registers[NEOGEO_APU_YM_REGISTER_LIMIT];
    uint32_t sent_valid;
} NeogeoApuBridge;

void neogeo_apu_bridge_init(NeogeoApuBridge *bridge);
void neogeo_apu_bridge_write(
    NeogeoApuBridge *bridge,
    uint16_t address,
    uint8_t value
);

/*
 * Emit only changed YM2610 SSG/ADPCM-B registers, then advance the NES-style
 * envelopes, length/linear counters, and pulse sweep units by one source-style
 * 60 Hz hardware interval. Ordinary game frames call this once; the target may
 * add bounded display-period catch-up steps. A null writer advances state
 * without acknowledging registers, which is useful while the Z80 starts.
 */
bool neogeo_apu_bridge_step(
    NeogeoApuBridge *bridge,
    NeogeoApuYmWrite writer,
    void *context
);

/* Force a complete register snapshot after a Z80 reset or transport fault. */
void neogeo_apu_bridge_invalidate(NeogeoApuBridge *bridge);

#endif
