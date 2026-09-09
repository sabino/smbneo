#ifndef SMBNEO_VS_NATIVE_PLATFORM_H
#define SMBNEO_VS_NATIVE_PLATFORM_H

/* CPU-free VS platform state.  This owns the bus-visible PPU/APU/OAM storage
 * and callbacks, but contains no 6502 execution state or persistence layer. */
#include "vs_bus.h"
#include <stdint.h>

typedef struct VsNativePlatform VsNativePlatform;
struct VsNativeFastVideoHooks;

struct VsNativePlatform {
    VsBus bus;
    uint8_t nametable[2048];
    uint8_t oam[256];
    uint8_t palette[32];
    uint8_t apu[24];
    uint16_t address;
    uint16_t address_latch;
    uint8_t ctrl;
    uint8_t mask;
    uint8_t toggle;
    uint8_t buffer;
    uint8_t oam_address;
    uint8_t status_phase;
    uint8_t scroll_x;
    uint8_t scroll_y;
    uint32_t apu_writes;
    void *video_context;
    void (*video_write)(void *, uint16_t, uint8_t);
    uint8_t (*video_run)(void *, uint16_t, const uint8_t *, uint8_t,
                         uint16_t, uint8_t);
};

void vs_native_platform_init(VsNativePlatform *platform,
                             const uint8_t *prg, const uint8_t *chr,
                             uint8_t dips);
void vs_native_platform_inputs(VsNativePlatform *platform, uint8_t p1,
                               uint8_t p2, uint8_t coin_service);
void vs_native_platform_set_video_hooks(VsNativePlatform *platform,
                                        void *context,
                                        void (*write)(void *, uint16_t, uint8_t),
                                        uint8_t (*run)(void *, uint16_t,
                                                       const uint8_t *, uint8_t,
                                                       uint16_t, uint8_t));

/* Initialize a caller-owned, per-platform display-list bridge.  An accepted
 * bridge call performs the complete VsBus PPU run (storage, address advance
 * and renderer notification); it is not a renderer-only shortcut. */
void vs_native_platform_fast_video_hooks(
    VsNativePlatform *platform, struct VsNativeFastVideoHooks *hooks);

VsBus *vs_native_platform_bus(VsNativePlatform *platform);
const VsBus *vs_native_platform_const_bus(const VsNativePlatform *platform);
uint8_t vs_native_platform_vram(const VsNativePlatform *platform,
                                uint16_t address);
void vs_native_platform_render(const VsNativePlatform *platform,
                               const uint8_t *rgb_palette, uint8_t *rgb);

#endif
