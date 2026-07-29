#include "ppu.h"

#include "cpu.h"
#include "external.h"
#include "video.h"
#include "ppu_render_state.h"
#include "title_data.h"

uint8_t nametable[NAMETABLE_SIZE];
Palette palette;
uint8_t oam[OAM_SIZE];

uint16_t ppu_v;
uint8_t ppu_w;
uint8_t ppu_f;

uint8_t ppu_ctrl;
uint8_t ppu_mask;
uint8_t ppu_status;
uint8_t oam_addr;
uint8_t ppu_scroll_x;
uint8_t ppu_scroll_y;

uint16_t vram_addr;
uint8_t vram_internal_buffer;
uint8_t oam_dma;

static uint8_t status_phase;

#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
uint8_t neogeo_ppu_batch_test_enabled = 1u;
uint32_t neogeo_ppu_batched_run_count;
uint32_t neogeo_ppu_rejected_run_count;
#endif

uint32_t neogeo_ppu_hud_generation;
uint32_t neogeo_ppu_palette_generation;
uint8_t neogeo_ppu_background_full_dirty_columns
    [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
    [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES];
uint8_t neogeo_ppu_background_hud_dirty_columns
    [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
    [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES];
uint32_t neogeo_ppu_hud_dirty_rows[3];
uint8_t neogeo_ppu_hud_dirty_tracking_valid;

static uint16_t normalize_ppu_address(uint16_t addr) {
    return (uint16_t)(addr & 0x3fffu);
}

static uint16_t nametable_index(uint16_t addr) {
    /*
     * SMB uses vertical mirroring: $2000/$2800 share one physical page and
     * $2400/$2c00 share the other.  $3000-$3eff mirrors $2000-$2eff.
     */
    addr = normalize_ppu_address(addr);
    if (addr >= 0x3000u && addr < 0x3f00u) {
        addr = (uint16_t)(addr - 0x1000u);
    }
    return (uint16_t)((addr - 0x2000u) & 0x07ffu);
}

static uint8_t palette_index(uint16_t addr) {
    uint8_t index = (uint8_t)((addr - 0x3f00u) & 0x1fu);

    if (index >= 0x10u && (index & 0x03u) == 0u) {
        index = (uint8_t)(index - 0x10u);
    }
    return index;
}

static void mark_background_column(
    uint8_t dirty_columns
        [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
        [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES],
    uint8_t column
) {
    uint8_t byte_index = (uint8_t)(column >> 3);
    uint8_t bit = (uint8_t)((uint8_t)1u << (column & 7u));

    dirty_columns[0][byte_index] |= bit;
    dirty_columns[1][byte_index] |= bit;
}

static void mark_nametable_changed(uint16_t index) {
    uint16_t table_offset = (uint16_t)(index & 0x03ffu);
    uint8_t table_column_base = (index & 0x0400u) ? 32u : 0u;

    if (table_offset < 960u) {
        uint8_t column =
            (uint8_t)(table_column_base + (table_offset & 31u));
        uint8_t row = (uint8_t)(table_offset >> 5);

        if (row >= 1u && row <= 28u) {
            mark_background_column(
                neogeo_ppu_background_full_dirty_columns,
                column
            );
        }
        if (row >= 4u && row <= 29u) {
            mark_background_column(
                neogeo_ppu_background_hud_dirty_columns,
                column
            );
        }
        if (table_column_base == 0u && row >= 1u && row <= 3u) {
            neogeo_ppu_hud_dirty_rows[row - 1u] |=
                UINT32_C(1) << column;
            ++neogeo_ppu_hud_generation;
        }
    } else {
        uint8_t attribute =
            (uint8_t)(table_offset - 960u);
        uint8_t first_column =
            (uint8_t)(table_column_base + (attribute & 7u) * 4u);
        uint8_t i;

        for (i = 0; i < 4u; ++i) {
            uint8_t column = (uint8_t)(first_column + i);

            mark_background_column(
                neogeo_ppu_background_full_dirty_columns,
                column
            );
            if ((attribute >> 3) != 0u) {
                mark_background_column(
                    neogeo_ppu_background_hud_dirty_columns,
                    column
                );
            }
        }
        if (table_column_base == 0u && (attribute >> 3) == 0u) {
            uint32_t dirty_columns = UINT32_C(0x0f) << first_column;

            neogeo_ppu_hud_dirty_rows[0] |= dirty_columns;
            neogeo_ppu_hud_dirty_rows[1] |= dirty_columns;
            neogeo_ppu_hud_dirty_rows[2] |= dirty_columns;
            ++neogeo_ppu_hud_generation;
        }
    }
}

void ppu_init(uint8_t *chr) {
    (void)chr;

    memset(nametable, 0, sizeof(nametable));
    memset(&palette, 0, sizeof(palette));
    memset(oam, 0xff, sizeof(oam));

    ppu_v = 0;
    ppu_w = 0;
    ppu_f = 0;
    ppu_ctrl = 0;
    ppu_mask = 0;
    ppu_status = 0;
    oam_addr = 0;
    ppu_scroll_x = 0;
    ppu_scroll_y = 0;
    vram_addr = 0;
    vram_internal_buffer = 0;
    oam_dma = 0;
    status_phase = 0;
    neogeo_ppu_hud_generation = 0;
    neogeo_ppu_palette_generation = 0;
    memset(
        neogeo_ppu_background_full_dirty_columns,
        0,
        sizeof(neogeo_ppu_background_full_dirty_columns)
    );
    memset(
        neogeo_ppu_background_hud_dirty_columns,
        0,
        sizeof(neogeo_ppu_background_hud_dirty_columns)
    );
    neogeo_ppu_hud_dirty_rows[0] = 0u;
    neogeo_ppu_hud_dirty_rows[1] = 0u;
    neogeo_ppu_hud_dirty_rows[2] = 0u;
    neogeo_ppu_hud_dirty_tracking_valid = 0u;
#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
    neogeo_ppu_batched_run_count = 0u;
    neogeo_ppu_rejected_run_count = 0u;
#endif
}

uint8_t ppu_read_register(uint16_t addr) {
    switch (addr) {
        case 0x2002u:
            /*
             * The translated SMB code only uses these flags for timing loops.
             * Alternating the VBlank/sprite-zero bits preserves the baseline
             * port's deterministic behavior without emulating scanline time.
             */
            status_phase ^= 1u;
            ppu_w = 0;
            return status_phase ? 0u : 0xc0u;

        case 0x2004u:
            return oam[oam_addr];

        case 0x2007u: {
            uint8_t value = vram_internal_buffer;
            uint16_t increment = (ppu_ctrl & 0x04u) ? 32u : 1u;

            vram_internal_buffer = ppu_read(vram_addr);
            vram_addr = normalize_ppu_address(
                (uint16_t)(vram_addr + increment));
            return value;
        }

        default:
            return 0;
    }
}

void ppu_transfer_oam(uint16_t start_addr) {
    uint16_t i;

    for (i = 0; i < OAM_SIZE; ++i) {
        oam[i] = ram[(start_addr + i) & (RAM_SIZE - 1u)];
    }
}

void ppu_write_scroll(uint8_t value) {
    if (ppu_w == 0u) {
        ppu_scroll_x = value;
        ppu_w = 1u;
    } else {
        ppu_scroll_y = value;
        ppu_w = 0u;
    }
}

void ppu_write_address(uint8_t value) {
    if (ppu_w != 0u) {
        vram_addr = normalize_ppu_address(
            (uint16_t)((vram_addr & 0xff00u) | value));
        ppu_w = 0u;
    } else {
        vram_addr = normalize_ppu_address(
            (uint16_t)(((uint16_t)value << 8) | (vram_addr & 0x00ffu)));
        ppu_w = 1u;
    }
}

void ppu_write_data(uint8_t value) {
    uint16_t increment = (ppu_ctrl & 0x04u) ? 32u : 1u;

    ppu_write(vram_addr, value);
    vram_addr = normalize_ppu_address((uint16_t)(vram_addr + increment));
}

uint8_t ppu_write_buffer_run(
    uint16_t source_pointer,
    uint8_t length,
    uint8_t repeat
) {
    uint16_t count = length;
    uint16_t increment = (ppu_ctrl & 0x04u) ? 32u : 1u;
    uint32_t source_last;
    uint32_t destination_last;
    uint16_t destination;
    uint16_t run_index;

#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
    if (neogeo_ppu_batch_test_enabled == 0u) {
        ++neogeo_ppu_rejected_run_count;
        return 0u;
    }
#endif

    /*
     * Only direct page-$03 RAM is admitted.  This excludes every CPU alias,
     * device register, ROM read, and 16-bit source wrap from the shortcut.
     * Length zero means 256 writes in the original DEX loop and deliberately
     * remains on that loop.
     */
    if (
        count == 0u ||
        source_pointer < 0x0300u ||
        source_pointer > 0x03ffu
    ) {
        goto rejected;
    }

    source_last = (uint32_t)source_pointer + 3u;
    if (repeat == 0u) {
        source_last += (uint32_t)count - 1u;
    }
    if (source_last > 0x03ffu) {
        goto rejected;
    }

    /*
     * Pattern-table and palette behavior stays in ppu_write().  Preflight the
     * complete run before touching state so an unsupported record can fall
     * back byte-for-byte, including writes that would cross into $3f00.
     */
    destination = normalize_ppu_address(vram_addr);
    destination_last =
        (uint32_t)destination + ((uint32_t)count - 1u) * increment;
    if (
        destination < 0x2000u ||
        destination >= 0x3f00u ||
        destination_last >= 0x3f00u
    ) {
        goto rejected;
    }

    for (run_index = 0u; run_index < count; ++run_index) {
        uint16_t physical_index = nametable_index(destination);
        uint16_t source_index = (uint16_t)(
            source_pointer + 3u + (repeat != 0u ? 0u : run_index)
        );
        uint8_t value = ram[source_index];

        if (nametable[physical_index] != value) {
            nametable[physical_index] = value;
            mark_nametable_changed(physical_index);
        }
        destination = (uint16_t)(destination + increment);
    }
    vram_addr = normalize_ppu_address(destination);

#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
    ++neogeo_ppu_batched_run_count;
#endif
    return 1u;

rejected:
#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
    ++neogeo_ppu_rejected_run_count;
#endif
    return 0u;
}

uint8_t ppu_read(uint16_t addr) {
    addr = normalize_ppu_address(addr);

    if (addr < 0x2000u) {
        /*
         * Pattern pixels live in the cartridge C/S ROMs and do not need an
         * 8 KiB CPU-side copy. The title-screen setup is the one exception:
         * it reads a 314-byte nametable payload hidden at the end of CHR.
         */
        if (
            addr >= TITLE_SCREEN_CHR_OFFSET &&
            addr < TITLE_SCREEN_CHR_OFFSET + TITLE_SCREEN_CHR_SIZE
        ) {
            return neogeo_title_screen_data[
                addr - TITLE_SCREEN_CHR_OFFSET
            ];
        }
        return 0;
    }
    if (addr < 0x3f00u) {
        return nametable[nametable_index(addr)];
    }
    return palette.u8[palette_index(addr)];
}

void ppu_write(uint16_t addr, uint8_t value) {
    addr = normalize_ppu_address(addr);

    if (addr >= 0x2000u && addr < 0x3f00u) {
        uint16_t index = nametable_index(addr);

        if (nametable[index] != value) {
            nametable[index] = value;
            mark_nametable_changed(index);
        }
    } else if (addr >= 0x3f00u) {
        uint8_t index = palette_index(addr);
        uint8_t color = (uint8_t)(value & 0x3fu);

        if (palette.u8[index] != color) {
            palette.u8[index] = color;
            ++neogeo_ppu_palette_generation;
        }
    }
}

void ppu_render(void) {
    oam_addr = 0;
    neogeo_video_render();
    ppu_f ^= 1u;
}
