#include "vs_native_platform.h"
#include "vs_native_fast_video.h"
#include <string.h>

static unsigned palette_index(uint16_t address) {
    unsigned index = address & 31u;
    if ((index & 0x13u) == 0x10u) index &= 15u;
    return index;
}

uint8_t vs_native_platform_vram(const VsNativePlatform *platform,
                                uint16_t address) {
    address &= 0x3fffu;
    if (address < 0x2000u) return vs_bus_chr_read(&platform->bus, address);
    if (address < 0x3f00u) return platform->nametable[address & 0x7ffu];
    return platform->palette[palette_index(address)];
}

static uint8_t ppu_read(void *context, uint16_t address) {
    VsNativePlatform *platform = context;
    if (address == 0x2002u) {
        /* Frame-driven status, matching VsMachine's host platform seam. */
        platform->toggle = 0;
        platform->status_phase ^= 1u;
        return platform->status_phase ? 0u : 0xc0u;
    }
    if (address == 0x2004u) return platform->oam[platform->oam_address];
    if (address == 0x2007u) {
        uint8_t value = platform->buffer;
        platform->buffer = vs_native_platform_vram(platform, platform->address);
        if ((platform->address & 0x3fffu) >= 0x3f00u) {
            value = platform->buffer;
            platform->buffer = vs_native_platform_vram(platform,
                                                       (uint16_t)(platform->address - 0x1000u));
        }
        platform->address = (uint16_t)((platform->address +
            ((platform->ctrl & 4u) != 0u ? 32u : 1u)) & 0x3fffu);
        return value;
    }
    return 0;
}

static void ppu_write(void *context, uint16_t address, uint8_t value) {
    VsNativePlatform *platform = context;
    switch (address) {
    case 0x2000: platform->ctrl = value; break;
    case 0x2001: platform->mask = value; break;
    case 0x2003: platform->oam_address = value; break;
    case 0x2004: platform->oam[platform->oam_address++] = value; break;
    case 0x2005:
        if (!platform->toggle) platform->scroll_x = value;
        else platform->scroll_y = value;
        platform->toggle ^= 1u;
        break;
    case 0x2006:
        if (!platform->toggle) platform->address_latch = (uint16_t)(value & 0x3fu) << 8;
        else {
            platform->address_latch |= value;
            platform->address = platform->address_latch;
        }
        platform->toggle ^= 1u;
        break;
    case 0x2007:
        if (platform->address >= 0x3f00u)
            platform->palette[palette_index(platform->address)] = value & 63u;
        else if (platform->address >= 0x2000u)
            platform->nametable[platform->address & 0x7ffu] = value;
        if (platform->address >= 0x2000u && platform->video_write)
            platform->video_write(platform->video_context, platform->address, value);
        platform->address = (uint16_t)((platform->address +
            ((platform->ctrl & 4u) != 0u ? 32u : 1u)) & 0x3fffu);
        break;
    default: break;
    }
}

static void apu_write(void *context, uint16_t address, uint8_t value) {
    VsNativePlatform *platform = context;
    if (address >= 0x4000u && address <= 0x4017u) {
        platform->apu[address - 0x4000u] = value;
        ++platform->apu_writes;
    }
}

static void ppu_run(void *context, const uint8_t *source,
                    uint8_t count, uint8_t repeat) {
    VsNativePlatform *platform = context;
    uint16_t destination = platform->address;
    const uint16_t increment = (platform->ctrl & 4u) != 0u ? 32u : 1u;
    const unsigned source_step = repeat != 0u ? 0u : 1u;

    if (count != 0u && destination >= 0x2000u && destination < 0x3f00u &&
        (uint32_t)destination + (uint32_t)(count - 1u) * increment < 0x3f00u &&
        platform->video_run && platform->video_run(platform->video_context,
                                                     destination, source, count,
                                                     increment, repeat)) {
        for (unsigned i = 0; i < count; ++i) {
            platform->nametable[destination & 0x7ffu] = *source;
            source += source_step;
            destination = (uint16_t)(destination + increment);
        }
        platform->address = destination & 0x3fffu;
        return;
    }
    for (unsigned i = 0; i < count; ++i) {
        ppu_write(platform, 0x2007u, *source);
        source += source_step;
    }
}

static void oam_dma(void *context, uint16_t address) {
    VsNativePlatform *platform = context;
    if (address < 0x2000u) {
        const uint8_t *source = platform->bus.ram + (address & 0x7ffu);
        const unsigned first = 256u - platform->oam_address;
        memcpy(platform->oam + platform->oam_address, source, first);
        if (platform->oam_address != 0u)
            memcpy(platform->oam, source + first, platform->oam_address);
        return;
    }
    for (unsigned i = 0; i < 256u; ++i)
        platform->oam[(uint8_t)(platform->oam_address + i)] =
            vs_bus_read(&platform->bus, (uint16_t)(address + i));
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, used, externally_visible))
#elif defined(__clang__)
__attribute__((noinline, used))
#endif
void vs_native_platform_init(VsNativePlatform *platform,
                             const uint8_t *prg, const uint8_t *chr,
                             uint8_t dips) {
    memset(platform, 0, sizeof(*platform));
    vs_bus_init(&platform->bus, prg, chr);
    platform->bus.dips = dips;
    platform->bus.context = platform;
    platform->bus.ppu_read = ppu_read;
    platform->bus.ppu_write = ppu_write;
    platform->bus.ppu_run = ppu_run;
    platform->bus.apu_write = apu_write;
    platform->bus.oam_dma = oam_dma;
}

void vs_native_platform_inputs(VsNativePlatform *platform, uint8_t p1,
                               uint8_t p2, uint8_t coin_service) {
    vs_bus_inputs(&platform->bus, p1, p2, platform->bus.dips, coin_service);
}

void vs_native_platform_set_video_hooks(VsNativePlatform *platform,
                                        void *context,
                                        void (*write)(void *, uint16_t, uint8_t),
                                        uint8_t (*run)(void *, uint16_t,
                                                       const uint8_t *, uint8_t,
                                                       uint16_t, uint8_t)) {
    platform->video_context = context;
    platform->video_write = write;
    platform->video_run = run;
}

static bool fast_ppu_data_run(void *context, uint16_t destination,
                              const uint8_t *source, uint8_t count,
                              uint16_t increment, bool repeat) {
    VsNativePlatform *platform = (VsNativePlatform *)context;
    const uint16_t expected_increment =
        platform != NULL && (platform->ctrl & 4u) != 0u ? 32u : 1u;

    /* Every declined case is checked before entering the bus callback, so the
     * display-list helper can safely fall back to ordered data-port writes. */
    if (platform == NULL || source == NULL || count == 0u ||
        platform->bus.ppu_run == NULL ||
        destination != (platform->address & 0x3fffu) ||
        increment != expected_increment)
        return false;

    platform->bus.ppu_run(platform->bus.context, source, count,
                          repeat ? 1u : 0u);
    return true;
}

void vs_native_platform_fast_video_hooks(
    VsNativePlatform *platform, struct VsNativeFastVideoHooks *hooks) {
    if (hooks == NULL)
        return;
    hooks->opaque = platform;
    hooks->ppu_data_run = platform != NULL ? fast_ppu_data_run : NULL;
}

VsBus *vs_native_platform_bus(VsNativePlatform *platform) {
    return &platform->bus;
}

const VsBus *vs_native_platform_const_bus(const VsNativePlatform *platform) {
    return &platform->bus;
}

static uint8_t pattern(const VsNativePlatform *platform, unsigned tile,
                       unsigned x, unsigned y) {
    uint16_t address = (uint16_t)(tile * 16u + y);
    const unsigned shift = 7u - x;
    return (uint8_t)(((vs_bus_chr_read(&platform->bus, address) >> shift) & 1u) |
        (((vs_bus_chr_read(&platform->bus, (uint16_t)(address + 8u)) >> shift) & 1u) << 1));
}

void vs_native_platform_render(const VsNativePlatform *platform,
                               const uint8_t *rgb_palette, uint8_t *rgb) {
    for (unsigned y = 0; y < 240u; ++y) for (unsigned x = 0; x < 256u; ++x) {
        unsigned sx = x, sy = y, nt = 0;
        uint8_t bg = 0, color = platform->palette[0];
        if (y >= 32u) {
            sx += platform->scroll_x + ((platform->ctrl & 1u) != 0u ? 256u : 0u);
            sy += platform->scroll_y;
        }
        nt = (sx / 256u) & 1u; sx %= 256u; sy %= 240u;
        if ((platform->mask & 8u) && (x >= 8u || (platform->mask & 2u))) {
            const unsigned offset = nt * 1024u + (sy / 8u) * 32u + sx / 8u;
            const unsigned tile = platform->nametable[offset] +
                                  ((platform->ctrl & 16u) != 0u ? 256u : 0u);
            const uint8_t attr = platform->nametable[nt * 1024u + 960u +
                                                       (sy / 32u) * 8u + sx / 32u];
            const unsigned group = (attr >> (((sy / 16u) & 1u) * 4u +
                                               ((sx / 16u) & 1u) * 2u)) & 3u;
            bg = pattern(platform, tile, sx % 8u, sy % 8u);
            if (bg) color = platform->palette[group * 4u + bg];
        }
        if ((platform->mask & 16u) && (x >= 8u || (platform->mask & 4u))) {
            for (unsigned i = 0; i < 64u; ++i) {
                unsigned top = platform->oam[i * 4u] + 1u;
                unsigned left = platform->oam[i * 4u + 3u];
                const unsigned height = (platform->ctrl & 32u) != 0u ? 16u : 8u;
                if (x < left || x >= left + 8u || y < top || y >= top + height) continue;
                uint8_t attr = platform->oam[i * 4u + 2u];
                unsigned px = x - left, py = y - top;
                unsigned tile = platform->oam[i * 4u + 1u];
                if (attr & 64u) px = 7u - px;
                if (attr & 128u) py = height - 1u - py;
                if (height == 16u)
                    tile = (tile & 1u) * 256u + (tile & 254u) + py / 8u;
                else if (platform->ctrl & 8u)
                    tile += 256u;
                const uint8_t object = pattern(platform, tile, px, py % 8u);
                if (!object) continue;
                if (!(attr & 32u) || !bg)
                    color = platform->palette[16u + (attr & 3u) * 4u + object];
                break;
            }
        }
        if (platform->mask & 1u) color &= 0x30u;
        memcpy(rgb + (y * 256u + x) * 3u,
               rgb_palette + (color & 63u) * 3u, 3u);
    }
}
