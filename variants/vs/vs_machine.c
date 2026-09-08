#include "vs_machine.h"

static unsigned machine_palette_index(uint16_t a) {
    unsigned i = a & 31;
    if ((i & 0x13) == 0x10) i &= 15;
    return i;
}
uint8_t vs_machine_vram(const VsMachine *m, uint16_t a) {
    a &= 0x3fff;
    if (a < 0x2000) return vs_bus_chr_read(&m->bus, a);
    if (a < 0x3f00) return m->nametable[a & 0x7ff];
    return m->palette[machine_palette_index(a)];
}
static uint8_t machine_ppu_read(void *ctx, uint16_t a) {
    VsMachine *m = ctx;
    if (a == 0x2002) {
        /* Frame-driven port, like the home backend: release polling loops.
         * Not a cycle-accurate PPU or evidence of scanline timing parity. */
        m->toggle = 0; m->status_phase ^= 1;
        return m->status_phase ? 0 : 0xc0;
    }
    if (a == 0x2004) return m->oam[m->oam_address];
    if (a == 0x2007) {
        uint8_t v = m->buffer;
        m->buffer = vs_machine_vram(m, m->address);
        if ((m->address & 0x3fff) >= 0x3f00) {
            v = m->buffer;
            m->buffer = vs_machine_vram(m, m->address - 0x1000);
        }
        m->address = (m->address + ((m->ctrl & 4) ? 32 : 1)) & 0x3fff;
        return v;
    }
    return 0;
}
static void machine_ppu_write(void *ctx, uint16_t a, uint8_t v) {
    VsMachine *m = ctx;
    switch (a) {
    case 0x2000: m->ctrl = v; break;
    case 0x2001: m->mask = v; break;
    case 0x2003: m->oam_address = v; break;
    case 0x2004: m->oam[m->oam_address++] = v; break;
    case 0x2005:
        if (!m->toggle) m->scroll_x = v; else m->scroll_y = v;
        m->toggle ^= 1; break;
    case 0x2006:
        if (!m->toggle) m->address_latch = (uint16_t)(v & 0x3f) << 8;
        else { m->address_latch |= v; m->address = m->address_latch; }
        m->toggle ^= 1; break;
    case 0x2007:
        if (m->address >= 0x3f00) m->palette[machine_palette_index(m->address)] = v & 63;
        else if (m->address >= 0x2000) m->nametable[m->address & 0x7ff] = v;
        if (m->address >= 0x2000 && m->video_write)
            m->video_write(m->video_context, m->address, v);
        m->address = (m->address + ((m->ctrl & 4) ? 32 : 1)) & 0x3fff;
        break;
    }
}
static void machine_apu_write(void *ctx, uint16_t a, uint8_t v) {
    VsMachine *m = ctx;
    if (a >= 0x4000 && a <= 0x4017) { m->apu[a - 0x4000] = v; ++m->apu_writes; }
}
static void machine_oam_dma(void *ctx, uint16_t a) {
    VsMachine *m = ctx;
#if defined(SMB_VS) && !defined(VS_REFERENCE_DMA)
    if (a < 0x2000) {
        /* DMA starts on a page boundary, so every RAM mirror supplies one
         * contiguous 256-byte page without I/O side effects. OAM wraps at 256
         * independently of CPU RAM; its address register does not advance. */
        const uint8_t *source = m->bus.ram + (a & 0x7ff);
        unsigned first = 256u - m->oam_address;
        memcpy(m->oam + m->oam_address, source, first);
        if (m->oam_address) memcpy(m->oam, source + first, m->oam_address);
        return;
    }
#endif
    for (unsigned i = 0; i < 256; ++i)
        m->oam[(uint8_t)(m->oam_address + i)] = vs_bus_read(&m->bus, (uint16_t)(a + i));
}
void vs_machine_init(VsMachine *m, const uint8_t *prg, const uint8_t *chr) {
    vs_machine_init_dips(m, prg, chr, 0x10);
}
void vs_machine_init_dips(VsMachine *m, const uint8_t *prg, const uint8_t *chr, uint8_t dips) {
    memset(m, 0, sizeof(*m)); vs_bus_init(&m->bus, prg, chr);
    m->bus.dips = dips;
    m->bus.context = m; m->bus.ppu_read = machine_ppu_read; m->bus.ppu_write = machine_ppu_write;
    m->bus.apu_write = machine_apu_write; m->bus.oam_dma = machine_oam_dma;
    vs_cpu_init(&m->cpu, &m->bus);
    vs_program_run(&m->cpu, 200000);
}
int vs_machine_frame(VsMachine *m, uint8_t p1, uint8_t p2, uint8_t coin) {
    vs_bus_inputs(&m->bus, p1, p2, m->bus.dips, coin);
    if (m->cpu.fault || m->cpu.in_nmi) return 0;
    if (m->ctrl & 0x80) vs_cpu_nmi(&m->cpu);
    vs_program_run(&m->cpu, 200000);
    ++m->frames;
    return !m->cpu.fault && !m->cpu.in_nmi;
}

static uint8_t pattern(const VsMachine *m, unsigned tile, unsigned x, unsigned y) {
    uint16_t a = (uint16_t)(tile * 16 + y);
    unsigned shift = 7 - x;
    return ((vs_bus_chr_read(&m->bus, a) >> shift) & 1)
         | (((vs_bus_chr_read(&m->bus, a + 8) >> shift) & 1) << 1);
}
void vs_machine_render(const VsMachine *m, const uint8_t *rgb_palette, uint8_t *rgb) {
    for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 256; ++x) {
        unsigned sx = x, sy = y, nt = 0;
        uint8_t bg = 0, color = m->palette[0];
        if (y >= 32) { sx += m->scroll_x + ((m->ctrl & 1) ? 256 : 0); sy += m->scroll_y; }
        nt = (sx / 256) & 1; sx %= 256; sy %= 240;
        if ((m->mask & 8) && (x >= 8 || (m->mask & 2))) {
            unsigned offset = nt * 1024 + (sy / 8) * 32 + sx / 8;
            unsigned tile = m->nametable[offset] + ((m->ctrl & 16) ? 256 : 0);
            uint8_t attr = m->nametable[nt * 1024 + 960 + (sy / 32) * 8 + sx / 32];
            unsigned group = (attr >> (((sy / 16) & 1) * 4 + ((sx / 16) & 1) * 2)) & 3;
            bg = pattern(m, tile, sx % 8, sy % 8);
            if (bg) color = m->palette[group * 4 + bg];
        }
        if ((m->mask & 16) && (x >= 8 || (m->mask & 4))) {
            for (unsigned i = 0; i < 64; ++i) {
                unsigned top = m->oam[i * 4] + 1, left = m->oam[i * 4 + 3];
                unsigned height = (m->ctrl & 32) ? 16 : 8;
                if (x < left || x >= left + 8 || y < top || y >= top + height) continue;
                uint8_t attr = m->oam[i * 4 + 2];
                unsigned px = x - left, py = y - top, tile = m->oam[i * 4 + 1];
                if (attr & 64) px = 7 - px;
                if (attr & 128) py = height - 1 - py;
                if (height == 16) tile = (tile & 1) * 256 + (tile & 254) + py / 8;
                else tile += (m->ctrl & 8) ? 256 : 0;
                uint8_t obj = pattern(m, tile, px, py % 8);
                if (!obj) continue;
                if (!(attr & 32) || !bg) color = m->palette[16 + (attr & 3) * 4 + obj];
                break;
            }
        }
        if (m->mask & 1) color &= 0x30;
        memcpy(rgb + (y * 256 + x) * 3, rgb_palette + (color & 63) * 3, 3);
    }
}
