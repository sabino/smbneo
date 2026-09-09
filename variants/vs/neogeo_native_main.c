/* Native direct-C Neo Geo entrypoint.
 *
 * This is an alternate target to neogeo_main.c.  It intentionally owns no
 * VsMachine/VsCpu state: generated SMBNeo code talks to VsNativePlatform via
 * its read8/write8 ABI, while the existing Neo Geo video/audio sinks remain
 * the hardware boundary.
 */
#include "vs_native_platform.h"
#include "vs_native_fast_video.h"
#include "smbneo_native.h"
#include "ppu.h"
#include "ppu_render_state.h"
#include "video.h"
#include "apu.h"
#include "audio_cadence.h"
#include "input_policy.h"
#include <ngdevkit/neogeo.h>
#include <ngdevkit/asm/bios-ram.h>
#include <string.h>

extern const uint8_t vs_prg[32768], vs_chr[16384];
/* Public state anchor used by the native ELF architecture checker. */
VsNativePlatform platform;
static SmbNeoNativeContext native;
static VsNativeFastVideoHooks native_video_hooks;
static uint32_t native_frames;
uint16_t neogeo_vs_pattern_bank;
uint8_t neogeo_vs_split;
static volatile uint8_t bss_restore_guard[32] = {0xa5};

volatile uint16_t vs_debug_stage, vs_debug_pc, vs_debug_fault, vs_debug_ctrl;
volatile uint32_t vs_debug_frames, vs_debug_steps;
volatile uint16_t vs_debug_logic_vblanks, vs_debug_render_vblanks;
volatile uint8_t vs_debug_native_unsupported;
volatile uint16_t vs_debug_native_last_symbol;
volatile uint32_t vs_debug_native_calls;

static uint8_t native_read8(void *context, uint16_t address) {
    VsNativePlatform *p = context;
    return vs_bus_read(&p->bus, address);
}

static void native_write8(void *context, uint16_t address, uint8_t value) {
    VsNativePlatform *p = context;
    vs_bus_write(&p->bus, address, value);
}

static void sound_write(void *context, uint16_t address, uint8_t value) {
    VsNativePlatform *p = context;
    if (address >= 0x4000u && address <= 0x4017u) {
        p->apu[address - 0x4000u] = value;
        ++p->apu_writes;
    }
    apu_write(address, value);
}

static void video_write(void *context, uint16_t address, uint8_t value) {
    (void)context;
    /* Platform storage is updated by its PPU callback before this sink is
     * invoked; the Neo Geo sink only records the hardware-visible write. */
    ppu_write(address, value);
}

static uint8_t video_run(void *context, uint16_t address,
                         const uint8_t *source, uint8_t count,
                         uint16_t increment, uint8_t repeat) {
    (void)context;
    return neogeo_ppu_write_nametable_run(address, source, count,
                                          increment, repeat);
}

static uint8_t controls(uint8_t raw) {
    uint8_t value = 0;
    if (!(raw & CNT_RIGHT)) value |= 128u;
    if (!(raw & CNT_LEFT)) value |= 64u;
    if (!(raw & CNT_DOWN)) value |= 32u;
    if (!(raw & CNT_UP)) value |= 16u;
    /* A/B jump, C/D run: the established Neo Geo control policy. */
    value |= neogeo_input_map_action_buttons(raw);
    return neogeo_input_normalize_directions(value);
}

static void debug_state(void) {
    vs_debug_native_unsupported = native.unsupported;
    vs_debug_native_last_symbol = native.last_symbol;
    vs_debug_native_calls = native.calls;

    /* Preserve the legacy monitor symbol names while replacing their meaning
     * with direct-C diagnostics: last generated symbol, native error bits,
     * and native call count. */
    vs_debug_pc = native.last_symbol;
    vs_debug_fault = native.unsupported;
    vs_debug_steps = native.calls;
    vs_debug_ctrl = platform.ctrl;
    vs_debug_frames = native_frames;
}

static void present(void) {
    uint16_t bank = (uint16_t)(platform.bus.chr_bank * 512u);
    if (bank != neogeo_vs_pattern_bank) {
        neogeo_vs_pattern_bank = bank;
        neogeo_video_benchmark_invalidate();
    }
    memcpy(oam, platform.oam, 256);
    ppu_ctrl = platform.ctrl;
    ppu_mask = platform.mask;
    ppu_scroll_x = platform.scroll_x;
    ppu_scroll_y = platform.scroll_y;
    neogeo_vs_split = platform.bus.ram[0x722];
    ppu_render();
}

int main(void) {
    uint16_t audio_vblank;
    /* Match the established BIOS/MVS startup contract.  This is volatile BIOS
     * work RAM only; the native path never writes backup RAM or memory cards. */
    *(volatile uint8_t *)BIOS_USER_MODE = 2;
    (void)bss_restore_guard[0];
    vs_debug_stage = 1;
    neogeo_video_init();
    ppu_init(0);
    apu_init(0);
    vs_debug_stage = 2;

    vs_native_platform_init(&platform, vs_prg, vs_chr, 0x10);
    memset(&native, 0, sizeof(native));
    native.opaque = &platform;
    native.read8 = native_read8;
    native.write8 = native_write8;
    vs_native_platform_fast_video_hooks(&platform, &native_video_hooks);
    native.fast_video_hooks = &native_video_hooks;
    native.ram = platform.bus.ram;
    native.extra_ram = platform.bus.extra_ram;
    native.prg = platform.bus.prg;
    smbneo_native_boot(&native);
    vs_debug_stage = 3;
    debug_state();

    /* Boot uses the platform's storage-only callbacks. Install hardware
     * sinks only after boot so initialization writes are forwarded exactly
     * once below, matching the proven translated startup ordering. */
    vs_native_platform_set_video_hooks(&platform, 0, video_write, video_run);
    platform.bus.apu_write = sound_write;

    for (unsigned i = 0; i < 2048u; ++i)
        ppu_write((uint16_t)(0x2000u + i), platform.nametable[i]);
    for (unsigned i = 0; i < 32u; ++i) {
        /* Palette slots 0x10/0x14/0x18/0x1c alias the universal entries. */
        if ((i & 0x13u) != 0x10u)
            ppu_write((uint16_t)(0x3f00u + i), platform.palette[i]);
    }
    for (unsigned i = 0; i < 24u; ++i)
        if (i != 20u && i != 22u)
            apu_write((uint16_t)(0x4000u + i), platform.apu[i]);
    audio_vblank = neogeo_video_current_vblank();

    for (;;) {
        uint16_t game_frame_vblank;
        uint16_t began = neogeo_video_current_vblank();

        audio_vblank = neogeo_audio_prepare_game_frame(
            audio_vblank, &game_frame_vblank
        );
        vs_debug_stage = 5;
        uint8_t p1 = controls(*REG_P1CNT), p2 = controls(*REG_P2CNT);
        uint8_t system = *REG_STATUS_B, coin = 0;
        if (!(system & CNT_START1)) p1 |= 4u;
        if (!(system & CNT_START2)) p2 |= 4u;

        /* Coin-pin bits are defined on MVS; AES has no physical coin slots. */
        if (*(volatile uint8_t *)BIOS_MVS_FLAG & 0x80u) {
            uint8_t physical_coins = *REG_STATUS_A;
            if (!(physical_coins & 1u)) coin |= 0x20u;
            if (!(physical_coins & 2u)) coin |= 0x40u;
        }
        /* AES fallback: P1 Start + either run button inserts a coin. */
        if ((p1 & 6u) == 6u) {
            coin |= 0x20u;
            p1 &= (uint8_t)~4u;
        }

        vs_native_platform_inputs(&platform, p1, p2, coin);
        native.suspended = 0;
        smbneo_native_frame(&native);
        if (!native.unsupported) {
            apu_step_frame();
            audio_vblank = game_frame_vblank;
        }
        ++native_frames;
        vs_debug_logic_vblanks = (uint16_t)(neogeo_video_current_vblank() - began);
        vs_debug_stage = 4;
        debug_state();
        began = neogeo_video_current_vblank();
        present();
        vs_debug_render_vblanks = (uint16_t)(neogeo_video_current_vblank() - began);
    }
}
