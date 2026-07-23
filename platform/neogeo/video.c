#include "video.h"

#include "constants.h"
#include "cpu.h"
#include "external.h"
#include "ppu.h"

#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define NEO_SCREEN_WIDTH 320u
#define NEO_SCREEN_HEIGHT 224u
#define NES_CROP_TOP 8
#define NES_CONTENT_X ((NEO_SCREEN_WIDTH - SCREEN_WIDTH) / 2u)

#define FIX_VISIBLE_Y 2u
#define FIX_CONTENT_X (NES_CONTENT_X / 8u)
#define FIX_CONTENT_COLUMNS 32u
#define FIX_HUD_ROWS 3u
#define FIX_BLANK_TILE 512u
#define FIX_SOLID_TILE 513u
#define FIX_BORDER_PALETTE 15u

/* C-ROM tiles 0..255 are reserved for the BIOS/ngdevkit eyecatcher. */
#define CROM_BLANK_TILE 256u
#define CROM_NES_TILE_BASE 257u

#define NEO_ZOOM_8X8 0x077fu
#define NEO_BG_PALETTE_BASE 16u
#define NEO_SPRITE_PALETTE_BASE 20u

#define OAM_SPRITES 64u
#define BACKGROUND_STRIPS 33u
#define BEHIND_SPRITE_OFFSET 0u
#define BACKGROUND_OFFSET (BEHIND_SPRITE_OFFSET + OAM_SPRITES)
#define FRONT_SPRITE_OFFSET (BACKGROUND_OFFSET + BACKGROUND_STRIPS)
#define SPRITES_PER_SET (FRONT_SPRITE_OFFSET + OAM_SPRITES)
#define SPRITE_SET_COUNT 2u
#define FIRST_GAME_SPRITE 1u
#define LAST_GAME_SPRITE \
    (FIRST_GAME_SPRITE + SPRITES_PER_SET * SPRITE_SET_COUNT - 1u)

#if LAST_GAME_SPRITE > 380u
#error Neo Geo renderer exceeds the 381 displayable sprite slots
#endif

#define NEO_ATTR_HORIZONTAL_FLIP 0x0001u
#define NEO_ATTR_VERTICAL_FLIP 0x0002u

#define NES_RIGHT 0x80u
#define NES_LEFT 0x40u
#define NES_DOWN 0x20u
#define NES_UP 0x10u
#define NES_START 0x08u
#define NES_SELECT 0x04u
#define NES_B 0x02u
#define NES_A 0x01u

#define NEO_BACKDROP_COLOR (*(volatile uint16_t *)0x401ffe)

/*
 * The baseline port's 64-color RGB table converted once to the Neo Geo's
 * packed 15-bit palette format.  Keeping this in ROM costs 128 bytes and
 * avoids an RGB framebuffer or per-frame color conversion.
 */
static const uint16_t nes_palette_to_neogeo[64] = {
    0x8888u, 0x203au, 0x801bu, 0x0409u, 0x1a05u, 0x1c02u, 0x4b00u, 0x4810u,
    0x6520u, 0x0140u, 0x2040u, 0x1042u, 0x0046u, 0x8000u, 0x0000u, 0x0000u,
    0x0cccu, 0x107fu, 0x125fu, 0x183fu, 0x6e2bu, 0x6f25u, 0x4f20u, 0x0d30u,
    0x0c60u, 0x0380u, 0x2080u, 0x2085u, 0x309cu, 0x8222u, 0xf000u, 0xf000u,
    0x7fffu, 0x50dfu, 0x56afu, 0x1d8fu, 0x4f4fu, 0x5f68u, 0x6f83u, 0x6f91u,
    0x6fb2u, 0x59e0u, 0x42f3u, 0x40fau, 0x30ffu, 0x7555u, 0x7000u, 0x7000u,
    0x7fffu, 0x3affu, 0x3befu, 0xfdaeu, 0x7fafu, 0x6fabu, 0x4fdbu, 0x6feau,
    0x5ff9u, 0x2de9u, 0x3aeau, 0x9afdu, 0x79ffu, 0x7dddu, 0x8111u, 0x8111u,
};

/*
 * Only SCB3 needs staging.  SCB1, SCB2, and SCB4 can be updated while their
 * set is hidden; 322 bytes buys a clean VBlank reveal without a framebuffer.
 */
static uint16_t next_scb3[SPRITES_PER_SET];
static uint8_t scanline_sprite_count[NEO_SCREEN_HEIGHT];
static uint8_t visible_set;
static uint8_t hud_was_visible;

static uint16_t sprite_set_base(uint8_t set) {
    return (uint16_t)(FIRST_GAME_SPRITE + (uint16_t)set * SPRITES_PER_SET);
}

static uint16_t sprite_y_word(int16_t y, uint16_t height_tiles) {
    uint16_t hardware_y = (uint16_t)((496 - (int32_t)y) & 0x01ff);
    return (uint16_t)((hardware_y << 7) | (height_tiles & 0x003fu));
}

static uint16_t sprite_x_word(uint16_t x) {
    return (uint16_t)(x << 7);
}

static uint16_t neogeo_color(uint8_t nes_color) {
    return nes_palette_to_neogeo[nes_color & 0x3fu];
}

static uint8_t background_palette_index(
    uint16_t nametable_offset,
    uint8_t tile_x,
    uint8_t tile_y
) {
    uint16_t attribute_index =
        (uint16_t)((tile_y >> 2) * 8u + (tile_x >> 2));
    uint8_t attribute =
        nametable[nametable_offset + 960u + attribute_index];
    uint8_t shift =
        (uint8_t)(((tile_y & 3u) >> 1) * 4u + ((tile_x & 3u) >> 1) * 2u);

    return (uint8_t)((attribute >> shift) & 3u);
}

static void write_sprite_control(uint16_t sprite, uint16_t x) {
    /*
     * A 0 SCB3 keeps the set hidden.  With VRAMMOD=$200, three writes target
     * SCB2, SCB3, and SCB4 for the same sprite.
     */
    *REG_VRAMMOD = 0x0200u;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB2 + sprite);
    *REG_VRAMRW = NEO_ZOOM_8X8;
    *REG_VRAMRW = 0;
    *REG_VRAMRW = sprite_x_word(x);
}

static void write_single_tile_sprite(
    uint16_t sprite,
    uint16_t tile,
    uint16_t attributes,
    uint16_t x
) {
    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB1 + sprite * 64u);
    *REG_VRAMRW = tile;
    *REG_VRAMRW = attributes;
    write_sprite_control(sprite, x);
}

static void clear_hardware_sprites(void) {
    uint16_t sprite;

    *REG_VRAMMOD = 0x0200u;
    for (sprite = 0; sprite <= 380u; ++sprite) {
        *REG_VRAMADDR = (uint16_t)(ADDR_SCB2 + sprite);
        *REG_VRAMRW = 0;
        *REG_VRAMRW = 0;
        *REG_VRAMRW = 0;
    }
}

static void initialize_fix_map(void) {
    uint16_t x;
    uint16_t y;

    /*
     * The FIX layer is always above sprites.  Opaque 32-pixel side borders
     * crop fine-scroll and OAM pixels to the original 256-pixel NES viewport.
     */
    *REG_VRAMMOD = 1;
    for (x = 0; x < 40u; ++x) {
        uint16_t entry =
            (x < FIX_CONTENT_X || x >= FIX_CONTENT_X + FIX_CONTENT_COLUMNS)
                ? (uint16_t)((FIX_BORDER_PALETTE << 12) | FIX_SOLID_TILE)
                : FIX_BLANK_TILE;

        *REG_VRAMADDR = (uint16_t)(ADDR_FIXMAP + (x << 5));
        for (y = 0; y < 32u; ++y) {
            *REG_VRAMRW = entry;
        }
    }
}

static void upload_palette_group(
    uint16_t neogeo_palette,
    uint8_t nes_palette_offset,
    uint8_t sprite_palette
) {
    uint16_t base = (uint16_t)(neogeo_palette << 4);
    uint8_t i;

    MMAP_PALBANK1[base] = neogeo_color(palette.u8[0]);
    for (i = 1; i < 4u; ++i) {
        uint8_t source = (uint8_t)(nes_palette_offset + i);

        if (sprite_palette != 0u) {
            source = (uint8_t)(16u + nes_palette_offset + i);
        }
        MMAP_PALBANK1[base + i] = neogeo_color(palette.u8[source]);
    }
}

static void upload_palettes(void) {
    uint8_t i;
    uint16_t universal = neogeo_color(palette.u8[0]);

    for (i = 0; i < 4u; ++i) {
        uint8_t source = (uint8_t)(i << 2);

        upload_palette_group(i, source, 0);
        upload_palette_group((uint16_t)(NEO_BG_PALETTE_BASE + i), source, 0);
        upload_palette_group(
            (uint16_t)(NEO_SPRITE_PALETTE_BASE + i),
            source,
            1
        );
    }

    MMAP_PALBANK1[FIX_BORDER_PALETTE << 4] = universal;
    MMAP_PALBANK1[(FIX_BORDER_PALETTE << 4) + 1u] = universal;
    NEO_BACKDROP_COLOR = universal;
}

static void build_background(uint16_t set_base, uint8_t show_hud) {
    uint16_t current_nametable = (ppu_ctrl & 0x01u) ? 0x0400u : 0u;
    uint16_t other_nametable = (uint16_t)(current_nametable ^ 0x0400u);
    uint16_t pattern_base = (ppu_ctrl & 0x10u) ? 256u : 0u;
    uint8_t fine_scroll = (uint8_t)(ppu_scroll_x & 7u);
    uint8_t first_tile_x = (uint8_t)(ppu_scroll_x >> 3);
    uint8_t first_tile_y = show_hud ? 4u : 1u;
    uint8_t tile_rows = show_hud ? 26u : 28u;
    uint16_t output_y = show_hud ? 24u : 0u;
    uint8_t strip;

    if ((ppu_mask & 0x08u) == 0u) {
        return;
    }

    for (strip = 0; strip < BACKGROUND_STRIPS; ++strip) {
        uint8_t source_x = (uint8_t)(first_tile_x + strip);
        uint16_t nametable_offset = current_nametable;
        uint16_t sprite =
            (uint16_t)(set_base + BACKGROUND_OFFSET + strip);
        uint16_t x =
            (uint16_t)(NES_CONTENT_X - fine_scroll + (uint16_t)strip * 8u);
        uint8_t row;

        if (source_x >= 32u) {
            source_x = (uint8_t)(source_x - 32u);
            nametable_offset = other_nametable;
        }

        *REG_VRAMMOD = 1;
        *REG_VRAMADDR = (uint16_t)(ADDR_SCB1 + sprite * 64u);
        for (row = 0; row < tile_rows; ++row) {
            uint8_t source_y = (uint8_t)(first_tile_y + row);
            uint8_t tile =
                nametable[
                    nametable_offset + (uint16_t)source_y * 32u + source_x
                ];
            uint8_t palette_number = background_palette_index(
                nametable_offset,
                source_x,
                source_y
            );

            *REG_VRAMRW =
                (uint16_t)(CROM_NES_TILE_BASE + pattern_base + tile);
            *REG_VRAMRW =
                (uint16_t)((NEO_BG_PALETTE_BASE + palette_number) << 8);
        }

        write_sprite_control(sprite, x);
        next_scb3[BACKGROUND_OFFSET + strip] =
            sprite_y_word((int16_t)output_y, tile_rows);
    }
}

static uint8_t sprite_fits_scanline_budget(int16_t y) {
    int16_t first = y < 0 ? 0 : y;
    int16_t last = (int16_t)(y + 8);
    int16_t line;

    if (last > (int16_t)NEO_SCREEN_HEIGHT) {
        last = (int16_t)NEO_SCREEN_HEIGHT;
    }
    for (line = first; line < last; ++line) {
        if (scanline_sprite_count[line] >= 8u) {
            return 0;
        }
    }
    for (line = first; line < last; ++line) {
        ++scanline_sprite_count[line];
    }
    return 1;
}

static void build_oam_sprites(uint16_t set_base) {
    uint16_t pattern_base = (ppu_ctrl & 0x08u) ? 256u : 0u;
    uint8_t draw_left_edge = (uint8_t)(ppu_mask & 0x04u);
    uint8_t oam_index;

    if ((ppu_mask & 0x10u) == 0u) {
        return;
    }

    memset(scanline_sprite_count, 0, sizeof(scanline_sprite_count));

    /*
     * NES OAM is evaluated from entry 0 upward and exposes at most eight
     * sprites per scanline.  Neo Geo priority increases with sprite number,
     * so reversing each 64-slot bank keeps lower OAM indices in front.
     */
    for (oam_index = 0; oam_index < OAM_SPRITES; ++oam_index) {
        uint16_t offset = (uint16_t)oam_index * 4u;
        uint8_t source_x = oam[offset + 3u];
        int16_t source_y = (int16_t)oam[offset] + 1;
        int16_t output_y = (int16_t)(source_y - NES_CROP_TOP);
        uint8_t attributes = oam[offset + 2u];
        uint16_t priority_offset =
            (attributes & 0x20u) ? BEHIND_SPRITE_OFFSET : FRONT_SPRITE_OFFSET;
        uint16_t relative_sprite =
            (uint16_t)(priority_offset + (OAM_SPRITES - 1u - oam_index));
        uint16_t sprite = (uint16_t)(set_base + relative_sprite);
        uint16_t neogeo_attributes =
            (uint16_t)(
                (NEO_SPRITE_PALETTE_BASE + (attributes & 3u)) << 8
            );

        if (output_y >= (int16_t)NEO_SCREEN_HEIGHT || output_y + 8 <= 0) {
            continue;
        }
        if (draw_left_edge == 0u && source_x == 0u) {
            continue;
        }
        if (sprite_fits_scanline_budget(output_y) == 0u) {
            continue;
        }

        if ((attributes & 0x40u) != 0u) {
            neogeo_attributes |= NEO_ATTR_HORIZONTAL_FLIP;
        }
        if ((attributes & 0x80u) != 0u) {
            neogeo_attributes |= NEO_ATTR_VERTICAL_FLIP;
        }

        write_single_tile_sprite(
            sprite,
            (uint16_t)(
                CROM_NES_TILE_BASE + pattern_base + oam[offset + 1u]
            ),
            neogeo_attributes,
            (uint16_t)(NES_CONTENT_X + source_x)
        );
        next_scb3[relative_sprite] = sprite_y_word(output_y, 1u);
    }
}

static void clear_hud(void) {
    uint8_t row;

    *REG_VRAMMOD = 32u;
    for (row = 0; row < FIX_HUD_ROWS; ++row) {
        uint8_t column;

        *REG_VRAMADDR = (uint16_t)(
            ADDR_FIXMAP +
            (FIX_CONTENT_X << 5) +
            FIX_VISIBLE_Y +
            row
        );
        for (column = 0; column < FIX_CONTENT_COLUMNS; ++column) {
            *REG_VRAMRW = FIX_BLANK_TILE;
        }
    }
}

static void upload_hud(uint8_t show_hud) {
    uint16_t pattern_base = (ppu_ctrl & 0x10u) ? 256u : 0u;
    uint8_t row;

    if (show_hud == 0u) {
        if (hud_was_visible != 0u) {
            clear_hud();
        }
        hud_was_visible = 0;
        return;
    }

    /*
     * NES tile row 0 is overscan.  Rows 1..3 become the three visible FIX
     * rows, keeping the status bar stationary while SCB strips scroll below.
     */
    *REG_VRAMMOD = 32u;
    for (row = 0; row < FIX_HUD_ROWS; ++row) {
        uint8_t source_y = (uint8_t)(row + 1u);
        uint8_t column;

        *REG_VRAMADDR = (uint16_t)(
            ADDR_FIXMAP +
            (FIX_CONTENT_X << 5) +
            FIX_VISIBLE_Y +
            row
        );
        for (column = 0; column < FIX_CONTENT_COLUMNS; ++column) {
            uint8_t tile =
                nametable[(uint16_t)source_y * 32u + column];
            uint8_t palette_number =
                background_palette_index(0, column, source_y);

            *REG_VRAMRW = (uint16_t)(
                ((uint16_t)palette_number << 12) +
                pattern_base +
                tile
            );
        }
    }
    hud_was_visible = 1;
}

static void hide_sprite_set(uint8_t set) {
    uint16_t i;

    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB3 + sprite_set_base(set));
    for (i = 0; i < SPRITES_PER_SET; ++i) {
        *REG_VRAMRW = 0;
    }
}

static void show_next_sprite_set(uint8_t set) {
    uint16_t i;

    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB3 + sprite_set_base(set));
    for (i = 0; i < SPRITES_PER_SET; ++i) {
        *REG_VRAMRW = next_scb3[i];
    }
}

uint8_t neogeo_read_controller1(void) {
    uint8_t state = 0;
    uint8_t controls = bios_p1current;
    uint8_t system = bios_statcurnt;

    if ((controls & CNT_RIGHT) != 0u) {
        state |= NES_RIGHT;
    }
    if ((controls & CNT_LEFT) != 0u) {
        state |= NES_LEFT;
    }
    if ((controls & CNT_DOWN) != 0u) {
        state |= NES_DOWN;
    }
    if ((controls & CNT_UP) != 0u) {
        state |= NES_UP;
    }
    if ((controls & CNT_A) != 0u) {
        state |= NES_A;
    }
    if ((controls & CNT_B) != 0u) {
        state |= NES_B;
    }
    if ((system & CNT_START1) != 0u) {
        state |= NES_START;
    }
    if ((system & CNT_SELECT1) != 0u) {
        state |= NES_SELECT;
    }
    return state;
}

void neogeo_video_init(void) {
    clear_hardware_sprites();
    initialize_fix_map();
    upload_palettes();

    visible_set = 0;
    hud_was_visible = 0;
}

void neogeo_video_render(void) {
    uint8_t next_set = (uint8_t)(visible_set ^ 1u);
    uint16_t set_base = sprite_set_base(next_set);
    uint8_t show_hud =
        (uint8_t)(
            (ppu_mask & 0x08u) != 0u &&
            ram[Sprite0HitDetectFlag] != 0u
        );

    memset(next_scb3, 0, sizeof(next_scb3));
    build_background(set_base, show_hud);
    build_oam_sprites(set_base);

    /*
     * Palette/FIX/SCB3 are the only live-display state.  Do those writes in
     * VBlank, hide the old set, and reveal the already-built set atomically.
     */
    ng_wait_vblank();
    upload_palettes();
    upload_hud(show_hud);
    hide_sprite_set(visible_set);
    show_next_sprite_set(next_set);
    visible_set = next_set;
}
