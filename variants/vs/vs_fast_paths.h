#ifndef SMBNEO_VS_FAST_PATHS_H
#define SMBNEO_VS_FAST_PATHS_H
#include "vs_cpu.h"

/* Recognized RAM-only stride-four loop. Other Y residues never terminate in
 * the source loop, so leave them to the bounded instruction implementation. */
static inline int vs_fast_stride_fill(VsCpu *c, unsigned base, uint8_t fill) {
    if (c->y & 3u) return 0;
    for (unsigned i = c->y; i < 256; i += 4)
        c->bus->ram[(base + i) & 0x7ff] = fill;
    c->a = fill;
    c->y = vs_nz(c, 0);
    return 1;
}

/* Exact semantic sprite-row operation. This edits emulated OAM in ordinary
 * RAM, never physical Neo Geo VRAM. The translator validates the entire source
 * instruction body before selecting this helper; unknown bodies fall back. */
static inline void vs_fast_render_pair(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    unsigned offset = c->y;
    unsigned next = offset + 8;
    uint8_t flip = ram[3] & 2u;
    uint8_t attr = ram[4] | (flip ? 0x40u : 0);
    ram[0x201 + offset] = ram[flip ? 1 : 0];
    ram[0x205 + offset] = ram[flip ? 0 : 1];
    ram[0x202 + offset] = ram[0x206 + offset] = attr;
    ram[0x200 + offset] = ram[0x204 + offset] = ram[2];
    ram[0x203 + offset] = ram[5];
    ram[0x207 + offset] = (uint8_t)(ram[5] + 8);
    ram[2] = (uint8_t)(ram[2] + 8);
    c->a = c->y = (uint8_t)next;
    c->p = (c->p & ~(VS_C | VS_V)) | (next > 255 ? VS_C : 0)
         | ((offset >= 120 && offset < 128) ? VS_V : 0);
    c->x = vs_nz(c, (uint8_t)(c->x + 2));
}

static inline void vs_fast_table_jump(VsCpu *c) {
    uint8_t shifted = (uint8_t)(c->a << 1);
    c->p = (c->p & ~VS_C) | (c->a >> 7);
    c->a = c->y = vs_nz(c, shifted);
    c->a = vs_nz(c, vs_pop(c)); c->bus->ram[4] = c->a;
    c->a = vs_nz(c, vs_pop(c)); c->bus->ram[5] = c->a;
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    c->a = vs_nz(c, vs_rd(c, (uint16_t)(vs_zpword(c, 4) + c->y)));
    c->bus->ram[6] = c->a;
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    c->a = vs_nz(c, vs_rd(c, (uint16_t)(vs_zpword(c, 4) + c->y)));
    c->bus->ram[7] = c->a;
    c->pc = vs_zpword(c, 6);
}

static inline void vs_fast_nmi_oam_shuffle(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    c->y = vs_nz(c, ram[1870]);
    c->a = vs_nz(c, 40); ram[0] = c->a;
    c->x = vs_nz(c, 14);
    for (;;) {
        unsigned slot = c->x;
        c->a = vs_nz(c, ram[1764 + slot]);
        vs_cmp(c, c->a, ram[0]);
        if (c->p & VS_C) {
            c->y = vs_nz(c, ram[1760]);
            c->p &= ~VS_C;
            vs_adc(c, ram[1761 + c->y]);
            if (c->p & VS_C) {
                c->p &= ~VS_C;
                vs_adc(c, ram[0]);
            }
            ram[1764 + slot] = c->a;
        }
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        if (c->p & VS_N) break;
    }
    c->x = vs_nz(c, ram[1760]);
    c->x = vs_nz(c, (uint8_t)(c->x + 1));
    vs_cmp(c, c->x, 3);
    if (c->p & VS_Z) c->x = vs_nz(c, 0);
    ram[1760] = c->x;
    c->x = vs_nz(c, 8); c->y = vs_nz(c, 2);
    for (;;) {
        unsigned source = 1769 + c->y, target = 1777 + c->x;
        c->a = vs_nz(c, ram[source]); ram[target] = c->a;
        c->p &= ~VS_C; vs_adc(c, 8); ram[target + 1] = c->a;
        c->p &= ~VS_C; vs_adc(c, 8); ram[target + 2] = c->a;
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
        if (c->p & VS_N) break;
    }
}
#endif
