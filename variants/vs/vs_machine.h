#ifndef SMBNEO_VS_MACHINE_H
#define SMBNEO_VS_MACHINE_H
#include "vs_cpu.h"

/* Portable video/I/O adapter, shared by the diagnostic host and target port. */
typedef struct {
    VsBus bus;
    VsCpu cpu;
    uint8_t nametable[2048], oam[256], palette[32], apu[24];
    uint16_t address, address_latch;
    uint8_t ctrl, mask, toggle, buffer, oam_address, status_phase;
    uint8_t scroll_x, scroll_y;
    uint32_t apu_writes, frames;
    void *video_context;
    void (*video_write)(void *, uint16_t, uint8_t);
} VsMachine;
void vs_machine_init(VsMachine *m, const uint8_t *prg, const uint8_t *chr);
void vs_machine_init_dips(VsMachine *m, const uint8_t *prg, const uint8_t *chr, uint8_t dips);
int vs_machine_frame(VsMachine *m, uint8_t p1, uint8_t p2, uint8_t coin);
uint8_t vs_machine_vram(const VsMachine *m, uint16_t address);
void vs_machine_render(const VsMachine *m, const uint8_t *rgb_palette, uint8_t *rgb);
#endif
