#ifndef SMBNEO_VS_CPU_H
#define SMBNEO_VS_CPU_H
#include "vs_bus.h"
#include <string.h>

/* State for statically translated C, not a runtime opcode interpreter. */
typedef struct {
    VsBus *bus;
    uint16_t pc;
    uint8_t a, x, y, s, p;
    uint8_t fault, idle, in_nmi, yielded;
    uint32_t instructions;
    unsigned fuel;
} VsCpu;
enum { VS_C = 1, VS_Z = 2, VS_I = 4, VS_D = 8, VS_B = 16,
       VS_U = 32, VS_V = 64, VS_N = 128 };

static inline uint8_t vs_rd(VsCpu *c, uint16_t a) {
    VsBus *bus = c->bus;

    if (a < 0x2000u)
        return bus->ram[a & 0x07ffu];
    if (a >= 0x6000u && a < 0x8000u)
        return bus->extra_ram[a & 0x07ffu];
    if (a >= 0x8000u && bus->prg)
        return bus->prg[a - 0x8000u];
    return vs_bus_read(bus, a);
}
static inline void vs_wr(VsCpu *c, uint16_t a, uint8_t v) {
    VsBus *bus = c->bus;

    if (a < 0x2000u) {
        bus->ram[a & 0x07ffu] = v;
        return;
    }
    if (a >= 0x6000u && a < 0x8000u) {
        bus->extra_ram[a & 0x07ffu] = v;
        return;
    }
    vs_bus_write(bus, a, v);
}
static inline uint8_t vs_nz(VsCpu *c, uint8_t v) {
    c->p = (c->p & ~(VS_N | VS_Z)) | (v & VS_N) | (v == 0 ? VS_Z : 0);
    return v;
}
/* Only flags proven live by the static translator need materializing. The
 * full helpers above/below remain the instruction-accurate host reference. */
static inline uint8_t vs_nz_live(VsCpu *c, uint8_t v, unsigned live) {
    unsigned mask = live & (VS_N | VS_Z);
    c->p = (c->p & ~mask) | (((v & VS_N) | (v == 0 ? VS_Z : 0)) & mask);
    return v;
}
static inline void vs_push(VsCpu *c, uint8_t v) { c->bus->ram[0x100 | c->s--] = v; }
static inline uint8_t vs_pop(VsCpu *c) { return c->bus->ram[0x100 | ++c->s]; }
static inline uint16_t vs_word(VsCpu *c, uint16_t a) {
    uint16_t lo = vs_rd(c, a);
    return lo | ((uint16_t)vs_rd(c, (uint16_t)(a + 1)) << 8);
}
static inline uint16_t vs_zpword(VsCpu *c, uint8_t a) {
    return c->bus->ram[a] | ((uint16_t)c->bus->ram[(uint8_t)(a + 1)] << 8);
}
static inline void vs_adc(VsCpu *c, uint8_t v) {
    unsigned sum = c->a + v + (c->p & VS_C);
    unsigned overflow = (~(c->a ^ v) & (c->a ^ sum) & 0x80) ? VS_V : 0;
    c->p = (c->p & ~(VS_C | VS_V)) | (sum > 255 ? VS_C : 0) | overflow;
    c->a = vs_nz(c, (uint8_t)sum); /* RP2A03 ignores decimal mode. */
}
static inline void vs_cmp(VsCpu *c, uint8_t a, uint8_t b) {
    c->p = (c->p & ~VS_C) | (a >= b ? VS_C : 0);
    (void)vs_nz(c, (uint8_t)(a - b));
}
static inline void vs_adc_live(VsCpu *c, uint8_t v, unsigned live) {
    unsigned sum = c->a + v + (c->p & VS_C);
    unsigned mask = live & (VS_C | VS_V | VS_N | VS_Z);
    unsigned flags = (sum > 255 ? VS_C : 0)
        | ((~(c->a ^ v) & (c->a ^ sum) & 0x80) ? VS_V : 0)
        | (sum & VS_N) | ((uint8_t)sum == 0 ? VS_Z : 0);
    c->p = (c->p & ~mask) | (flags & mask);
    c->a = (uint8_t)sum;
}
static inline void vs_cmp_live(VsCpu *c, uint8_t a, uint8_t b, unsigned live) {
    unsigned mask = live & (VS_C | VS_N | VS_Z);
    unsigned flags = (a >= b ? VS_C : 0) | ((a - b) & VS_N) | (a == b ? VS_Z : 0);
    c->p = (c->p & ~mask) | (flags & mask);
}
static inline void vs_cpu_init(VsCpu *c, VsBus *bus) {
    memset(c, 0, sizeof(*c)); c->bus = bus; c->s = 0xfd; c->p = VS_U | VS_I;
    c->pc = vs_word(c, 0xfffc);
}
static inline void vs_cpu_nmi(VsCpu *c) {
    vs_push(c, (uint8_t)(c->pc >> 8)); vs_push(c, (uint8_t)c->pc);
    vs_push(c, (c->p | VS_U) & ~VS_B); c->p |= VS_I;
    c->pc = vs_word(c, 0xfffa); c->idle = 0; c->in_nmi = 1;
}
/* Generated from verified linked disassembly. Returns on budget, idle or RTI.
 * Host budget counts instructions; target budget counts routine/page/loop
 * boundaries. Native C call chains are bounded and unwind to resumable state.
 * Neither is the game's timing source: exactly one NMI drives each game tick. */
void vs_program_run(VsCpu *c, unsigned budget);
#endif
