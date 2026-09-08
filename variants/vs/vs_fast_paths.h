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
#endif
