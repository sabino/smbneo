#include "vs_machine.h"
#include "ppu.h"
#include "video.h"
#include "apu.h"
#include "input_policy.h"
#include <ngdevkit/neogeo.h>
#include <ngdevkit/asm/bios-ram.h>

extern const uint8_t vs_prg[32768], vs_chr[16384];
static VsMachine machine;
uint16_t neogeo_vs_pattern_bank;
uint8_t neogeo_vs_split;
static volatile uint8_t bss_restore_guard[32] = {0xa5};
volatile uint16_t vs_debug_stage, vs_debug_pc, vs_debug_fault, vs_debug_ctrl;
volatile uint32_t vs_debug_frames, vs_debug_steps;
volatile uint16_t vs_debug_logic_vblanks, vs_debug_render_vblanks;

static void debug_state(void) {
    vs_debug_pc = machine.cpu.pc; vs_debug_fault = machine.cpu.fault;
    vs_debug_ctrl = machine.ctrl; vs_debug_frames = machine.frames;
    vs_debug_steps = machine.cpu.instructions;
}

static void sound_write(void *ctx, uint16_t addr, uint8_t value) {
    (void)ctx; apu_write(addr, value);
}
static void video_write(void *ctx, uint16_t addr, uint8_t value) {
    (void)ctx;
    /* Shared PPU RAM/dirty tracking only; physical VRAM remains VBlank-safe. */
    ppu_write(addr, value);
}
static uint8_t controls(uint8_t raw) {
    uint8_t v = 0;
    if (!(raw & CNT_RIGHT)) v |= 128;
    if (!(raw & CNT_LEFT)) v |= 64;
    if (!(raw & CNT_DOWN)) v |= 32;
    if (!(raw & CNT_UP)) v |= 16;
    /* A/B jump, C/D run: same physical layout as the home port. */
    v |= neogeo_input_map_action_buttons(raw);
    return neogeo_input_normalize_directions(v);
}
static void present(void) {
    uint16_t bank = machine.bus.chr_bank * 512u;
    if (bank != neogeo_vs_pattern_bank) {
        neogeo_vs_pattern_bank = bank;
        neogeo_video_benchmark_invalidate();
    }
    memcpy(oam, machine.oam, 256);
    ppu_ctrl = machine.ctrl; ppu_mask = machine.mask;
    ppu_scroll_x = machine.scroll_x; ppu_scroll_y = machine.scroll_y;
    neogeo_vs_split = machine.bus.ram[0x722]; /* canonical nmi_sprite_overlap */
    ppu_render();
}
int main(void) {
    /* VS owns its complete attract/credit/start state machine. Keep the BIOS
     * in game mode even on the VS title, otherwise an MVS coin restarts main
     * through USER request 3 and erases the VS credit that just arrived.
     * This is volatile BIOS work RAM, not backup RAM or memory-card storage. */
    *(volatile uint8_t *)BIOS_USER_MODE = 2;
    (void)bss_restore_guard[0];
    vs_debug_stage = 1;
    neogeo_video_init(); ppu_init(0); apu_init(0);
    vs_debug_stage = 2;
    vs_machine_init(&machine, vs_prg, vs_chr);
    vs_debug_stage = 3; debug_state();
    for (unsigned i = 0; i < 2048; ++i) ppu_write((uint16_t)(0x2000 + i), machine.nametable[i]);
    for (unsigned i = 0; i < 32; ++i) {
        /* Skip duplicate aliases: copying slot 16 would overwrite slot zero. */
        if ((i & 0x13) != 0x10) ppu_write((uint16_t)(0x3f00 + i), machine.palette[i]);
    }
    machine.video_write = video_write;
    machine.bus.apu_write = sound_write;
    /* Forward boot APU state, then preserve every subsequent register write. */
    for (unsigned i = 0; i < 24; ++i)
        if (i != 20 && i != 22) apu_write((uint16_t)(0x4000 + i), machine.apu[i]);
    for (;;) {
        uint16_t began = neogeo_video_current_vblank();
        vs_debug_stage = 5;
        uint8_t p1 = controls(*REG_P1CNT), p2 = controls(*REG_P2CNT);
        uint8_t system = *REG_STATUS_B, coin = 0;
        if (!(system & CNT_START1)) p1 |= 4;
        if (!(system & CNT_START2)) p2 |= 4;
        /* Coin-pin bits are only defined for MVS; AES exposes no coin slots. */
        if (*(volatile uint8_t *)BIOS_MVS_FLAG & 0x80u) {
            uint8_t physical_coins = *REG_STATUS_A;
            if (!(physical_coins & 1)) coin |= 0x20;
            if (!(physical_coins & 2)) coin |= 0x40;
        }
        /* AES has no coin slots: P1 Start + either run button inserts a coin. */
        if ((p1 & 6) == 6) { coin |= 0x20; p1 &= (uint8_t)~4; }
        if (vs_machine_frame(&machine, p1, p2, coin)) apu_step_frame();
        vs_debug_logic_vblanks = (uint16_t)(neogeo_video_current_vblank() - began);
        vs_debug_stage = 4; debug_state();
        began = neogeo_video_current_vblank();
        present();
        vs_debug_render_vblanks = (uint16_t)(neogeo_video_current_vblank() - began);
    }
}
