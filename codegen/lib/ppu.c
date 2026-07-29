#include "ppu.h"
#include "cpu.h"
#include "constants.h"
#include "external.h"

#define SPRITES_PALETTES_OFFSET 0x11
#define BYTES_PER_PALETTE 4
#define TILES_PER_ROW 32
#define TILES_PER_COLUMN 30

uint8_t chr_rom[8192];
uint8_t nametable[2048];
Palette palette;

uint8_t oam[256];

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

#define FRAME_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 3)

uint8_t frame[FRAME_BUFFER_SIZE];
bool opaque_bg_mask[SCREEN_WIDTH * SCREEN_HEIGHT];

// 64 RGB colors generated from the pinned visual-reference profile.
static const uint8_t COLOR_PALETTE[] = {
#include "nes_palette_rgb.inc"
};

typedef struct {
    bool is_ready;
    uint8_t pixels[8 * 8 * 3];
    bool opaque_mask[8 * 8];
    uint32_t palette_mask; // mask of palette indices used in this tile
    uint32_t palette; // palette value, masked by palette_mask
    uint8_t universal_color;
} Tile;

Tile bg_cached_tiles[256][4]; // 256 tiles, 4 palettes

void clear_frame(void) {
    memset(frame, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
}

void clear_bg_mask(void) {
    memset(opaque_bg_mask, false, SCREEN_WIDTH * SCREEN_HEIGHT);
}

void ppu_init(uint8_t* chr) {
    // copy CHR ROM
    memcpy(chr_rom, chr, 8192);
    clear_frame();

    // init tiles
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 4; j++) {
            bg_cached_tiles[i][j].is_ready = false;
        }
    }
}

bool status_clear = false;

uint8_t ppu_read_register(uint16_t addr) {
    switch (addr) {
        case 0x2002: {
            // in SMB, the status register is only used to:
            // 1. clear the write flip flop (ppu_w)
            // 2. detect vblank
            // 3. detect sprite 0 hit

            // items 2 and 3 are used for timing purposes
            // since we're rendering the entire frame at once, we can ignore them
            // however, we can't always return the same value
            // as some loops wait for a flag to be set or cleared

            status_clear = !status_clear;

            ppu_w = 0;
            // vblank and sprite 0 hit
            return status_clear ? 0 : 0b11000000;
        }
        case 0x2004: {
            return oam[oam_addr];
        }
        case 0x2007: {
            uint8_t value = vram_internal_buffer;
            vram_internal_buffer = ppu_read(vram_addr);
            uint16_t increment = ppu_ctrl & 0b100 ? 32 : 1;
            vram_addr += increment;
            return value;
        }
        default: return 0;
    }
}

void ppu_transfer_oam(uint16_t start_addr) {
    memcpy(oam, ram + start_addr, 256);
}

void ppu_write_scroll(uint8_t value) {
    if (!ppu_w) {
        ppu_scroll_x = value;
        ppu_w = 1;
    } else {
        ppu_scroll_y = value;
        ppu_w = 0;
    }
}

void ppu_write_address(uint8_t value) {
    if (ppu_w) {
        // low byte
        vram_addr = (vram_addr & 0xff00) | value;
        ppu_w = 0;
    } else {
        // high byte
        vram_addr = ((((uint16_t)value) << 8) & 0xff00) | (vram_addr & 0xff);
        ppu_w = 1;
    }
}

void ppu_write_data(uint8_t value) {
    ppu_write(vram_addr, value);
    uint16_t increment = ppu_ctrl & 0b100 ? 32 : 1;
    vram_addr += increment;
}

uint8_t ppu_write_buffer_run(
    uint16_t source_pointer,
    uint8_t length,
    uint8_t repeat
) {
    (void)source_pointer;
    (void)length;
    (void)repeat;
    return 0u;
}

// https://www.nesdev.org/wiki/PPU_memory_map
uint8_t ppu_read(uint16_t addr) {
    if (addr < 0x2000) {
        return chr_rom[addr];
    }

    if (addr < 0x3f00) {
        return nametable[addr - 0x2000];
    }

    if (addr == 0x3f10 || addr == 0x3f14 || addr == 0x3f18 || addr == 0x3f1c) {
        return palette.u8[addr - 0x3f10];
    }

    if (addr < 0x4000) {
        return palette.u8[(addr - 0x3f00) & 31];
    }

    return 0;
}

void ppu_write(uint16_t addr, uint8_t value) {
    if (addr >= 0x2000 && addr < 0x3f00) {
        nametable[addr - 0x2000] = value;
    } else if (addr == 0x3f10 || addr == 0x3f14 || addr == 0x3f18 || addr == 0x3f1c) {
        palette.u8[addr - 0x3f10] = value;
    } else if (addr < 0x4000) {
        palette.u8[(addr - 0x3f00) & 31] = value;
    }
}

void set_pixel(size_t x, size_t y, uint8_t palette_color) {
    size_t index = (y * SCREEN_WIDTH + x) * 3;

    if (index < SCREEN_WIDTH * SCREEN_HEIGHT * 3) {
        size_t palette_offset = palette_color * 3;
        uint8_t r = COLOR_PALETTE[palette_offset];
        uint8_t g = COLOR_PALETTE[palette_offset + 1];
        uint8_t b = COLOR_PALETTE[palette_offset + 2];

        frame[index] = r;
        frame[index + 1] = g;
        frame[index + 2] = b;
    }
}

size_t get_background_palette_index(size_t tile_col, size_t tile_row, size_t nametable_offset) {
    size_t attr_table_index = (tile_row / 4) * 8 + (tile_col / 4);
    // the attribute table is stored after the nametable (960 bytes)
    size_t attr_table_byte = nametable[nametable_offset + 960 + attr_table_index];
    size_t block_x = (tile_col & 3) / 2;
    size_t block_y = (tile_row & 3) / 2;
    size_t shift = block_y * 4 + block_x * 2;

    return ((attr_table_byte >> shift) & 0b11) * BYTES_PER_PALETTE;
}

Tile* get_cached_background_tile(size_t n, size_t bank_offset, size_t palette_offset) {
    size_t palette_idx = palette_offset / 4;
    Tile *tile = &bg_cached_tiles[n][palette_idx];

    if (
        tile->is_ready &&
        tile->universal_color == palette.u8[0] &&
        tile->palette == (palette.u32[palette_idx] & tile->palette_mask)
    ) {
        return tile;
    }

    uint32_t palette_mask = 0;

    for (size_t tile_y = 0; tile_y < 8; tile_y++) {
        uint8_t plane1 = chr_rom[bank_offset + n * 16 + tile_y];
        uint8_t plane2 = chr_rom[bank_offset + n * 16 + tile_y + 8];

        for (int tile_x = 7; tile_x >= 0; tile_x--) {
            uint8_t bit0 = plane1 & 1;
            uint8_t bit1 = plane2 & 1;
            uint8_t color_index = (uint8_t)((bit1 << 1) | bit0);

            plane1 >>= 1;
            plane2 >>= 1;

            uint8_t palette_idx;
            bool is_universal_bg_color = color_index == 0;
            palette_mask |= (uint32_t)(0xff << (color_index * 8));

            if (is_universal_bg_color) {
                palette_idx = palette.u8[0];
            } else {
                palette_idx = palette.u8[palette_offset + color_index];
            }

            size_t palette_offset = palette_idx * 3;

            uint8_t r = COLOR_PALETTE[palette_offset];
            uint8_t g = COLOR_PALETTE[palette_offset + 1];
            uint8_t b = COLOR_PALETTE[palette_offset + 2];

            size_t tile_offset = tile_y * 8 + (size_t)tile_x;
            size_t tile_offset_times_3 = tile_offset * 3;

            tile->pixels[tile_offset_times_3] = r;
            tile->pixels[tile_offset_times_3 + 1] = g;
            tile->pixels[tile_offset_times_3 + 2] = b;
            tile->opaque_mask[tile_offset] = !is_universal_bg_color;
        }
    }

    tile->palette_mask = palette_mask;
    tile->palette = palette.u32[palette_idx] & palette_mask;
    tile->universal_color = palette.u8[0];
    tile->is_ready = true;

    return tile;
}

void draw_background_tile(
    size_t n,
    size_t x,
    size_t y,
    size_t bank_offset,
    size_t palette_idx,
    int shift_x,
    int min_x,
    int max_x
) {
    if ((int)(x + 7) < min_x || (int)x >= (max_x + 7)) {
        return;
    }

    Tile *tile = get_cached_background_tile(n, bank_offset, palette_idx);

    // copy the tile, row by row
    for (size_t tile_y = 0; tile_y < 8; tile_y++) {
        int screen_x = (int)x + shift_x;
        size_t screen_y = y + tile_y;
        
        if ((int)x >= min_x && (int)(x + 7) < max_x) {
            size_t tile_offset = tile_y * 8;
            size_t frame_offset = screen_y * SCREEN_WIDTH + (size_t)screen_x;
            
            memcpy(&frame[frame_offset * 3], &tile->pixels[tile_offset * 3], 8 * 3);
            memcpy(&opaque_bg_mask[frame_offset], &tile->opaque_mask[tile_offset], 8);
        } else {
            // clipping
            for (size_t tile_x = 0; tile_x < 8; tile_x++) {
                size_t tile_offset = tile_y * 8 + tile_x;
                int nametable_x = (int)x + (int)tile_x;

                if (nametable_x >= min_x && nametable_x < max_x) {
                    size_t screen_x = (size_t)(shift_x + nametable_x);
                    size_t frame_offset = screen_y * SCREEN_WIDTH + screen_x;

                    memcpy(&frame[frame_offset * 3], &tile->pixels[tile_offset * 3], 3);
                    opaque_bg_mask[frame_offset] = tile->opaque_mask[tile_offset];
                }
            }
        }
    }
}

inline bool show_status_bar() {
    return ram[Sprite0HitDetectFlag];
}

void render_status_bar(size_t bank_offset) {
    for (size_t tile_y = 0; tile_y < 4; ++tile_y) {
        for (size_t tile_x = 0; tile_x < TILES_PER_ROW; ++tile_x) {
            size_t i = tile_y * TILES_PER_ROW + tile_x;

            uint8_t tile = nametable[i]; 
            size_t palette_index = get_background_palette_index(tile_x, tile_y, 0);

            size_t screen_x = tile_x * 8;
            size_t screen_y = tile_y * 8;

            draw_background_tile(tile, screen_x, screen_y, bank_offset, palette_index, 0, 0, SCREEN_WIDTH);
        }
    }
}

void render_nametable(size_t nametable_offset, size_t bank_offset, int shift_x, int min_x, int max_x, size_t min_tile_y) {
    bool draw_leftmost_tile = ppu_mask & 0b10;

    for (size_t tile_y = min_tile_y; tile_y < TILES_PER_COLUMN; ++tile_y) {
        for (size_t tile_x = 0; tile_x < TILES_PER_ROW; ++tile_x) {
            if (!draw_leftmost_tile && tile_x == 0) {
                continue;
            }

            size_t i = tile_y * TILES_PER_ROW + tile_x;
            uint8_t tile = nametable[nametable_offset + i];
            size_t palette_index = get_background_palette_index(tile_x, tile_y, nametable_offset);

            size_t screen_x = tile_x << 3;
            size_t screen_y = tile_y << 3;

            draw_background_tile(tile, screen_x, screen_y, bank_offset, palette_index, shift_x, min_x, max_x);
        }
    }
}

void render_background(void) {
    // https://austinmorlan.com/posts/nes_rendering_overview/
    size_t bank_offset = ppu_ctrl & 0b10000 ? 0x1000 : 0;
    size_t nametable1_offset, nametable2_offset;
    bool status_bar_visible = show_status_bar();
    size_t min_tile_y = status_bar_visible ? 4 : 0;

    // vertical mirroring
    if (ppu_ctrl & 0b1) {
        nametable1_offset = 0x400;
        nametable2_offset = 0x000;
    } else {
        nametable1_offset = 0x000;
        nametable2_offset = 0x400;
    }

    render_nametable(nametable1_offset, bank_offset, -((int)ppu_scroll_x), ppu_scroll_x, SCREEN_WIDTH, min_tile_y);
    render_nametable(nametable2_offset, bank_offset, SCREEN_WIDTH - (int)ppu_scroll_x, 0, ppu_scroll_x, min_tile_y);

    if (status_bar_visible) {
        render_status_bar(bank_offset);
    }
}

void draw_sprite_tile(
    size_t n,
    size_t x,
    size_t y,
    size_t bank_offset,
    size_t palette_idx,
    bool flip_x,
    bool flip_y,
    bool behind_bg
) {
    for (size_t tile_y = 0; tile_y < 8; tile_y++) {
        uint8_t plane1 = chr_rom[bank_offset + n * 16 + tile_y];
        uint8_t plane2 = chr_rom[bank_offset + n * 16 + tile_y + 8];

        for (size_t tile_x = 0; tile_x < 8; tile_x++) {
            uint8_t bit0 = plane1 & 1;
            uint8_t bit1 = plane2 & 1;
            uint8_t color_index = (uint8_t)((bit1 << 1) | bit0);

            plane1 >>= 1;
            plane2 >>= 1;

            if (color_index != 0) {
                uint8_t palette_offset = palette.u8[palette_idx + color_index - 1];
                uint8_t flipped_x = (uint8_t)(flip_x ? tile_x : 7 - tile_x);
                uint8_t flipped_y = (uint8_t)(flip_y ? 7 - tile_y : tile_y);
                size_t screen_x = x + flipped_x;
                size_t screen_y = y + flipped_y;

                bool is_hidden = behind_bg && opaque_bg_mask[screen_y * SCREEN_WIDTH + screen_x];

                if (!is_hidden && screen_x < SCREEN_WIDTH) {
                    set_pixel(screen_x, screen_y, palette_offset);
                }
            }
        }
    }
}

void render_sprites(void) {
    // https://www.nesdev.org/wiki/PPU_OAM
    size_t bank_offset = ppu_ctrl & 0b1000 ? 0x1000 : 0;
    bool draw_leftmost_tile = ppu_mask & 0b100;

    // sprites with lower OAM indices are drawn in front
    for (int i = 252; i >= 0; i -= 4) {
        uint8_t x = oam[i + 3];
        uint8_t y = oam[i] + 1;

        if (!draw_leftmost_tile && x == 0) {
            continue;
        }

        uint8_t tile = oam[i + 1];
        uint8_t attr = oam[i + 2];

        bool flip_x = attr & 0b01000000;
        bool flip_y = attr & 0b10000000;
        bool behind_bg = attr & 0b00100000;

        uint8_t palette_index = SPRITES_PALETTES_OFFSET + (attr & 0b11) * BYTES_PER_PALETTE;
        draw_sprite_tile(tile, x, y, bank_offset, palette_index, flip_x, flip_y, behind_bg);
    }
}

void ppu_render(void) {
    oam_addr = 0; // reset OAM address

    bool render_bg = ppu_mask & 0b00001000;
    bool render_sp = ppu_mask & 0b00010000;

    clear_bg_mask();

    if (render_bg) {
        render_background();
    } else {
        clear_frame();
    }

    if (render_sp) {
        render_sprites();
    }
}
