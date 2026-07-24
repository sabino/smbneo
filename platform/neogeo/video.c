#include "video.h"

#include "constants.h"
#include "cpu.h"
#include "external.h"
#include "input_policy.h"
#include "ppu.h"
#include "ppu_render_state.h"

#include <ngdevkit/neogeo.h>

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
#define BACKGROUND_MAX_ROWS 28u
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
#define NEO_SCB3_STICKY 0x0040u
#define BACKGROUND_MAX_DRIVERS 2u

#define NES_RIGHT 0x80u
#define NES_LEFT 0x40u
#define NES_DOWN 0x20u
#define NES_UP 0x10u
#define NES_START 0x08u
#define NES_SELECT 0x04u
#define NES_B 0x02u
#define NES_A 0x01u

#define NEO_BACKDROP_COLOR (*(volatile uint16_t *)0x401ffe)

#define RENDER_PALETTE_COUNT 12u
#define HUD_ENTRY_COUNT (FIX_HUD_ROWS * FIX_CONTENT_COLUMNS)
#define HUD_SHARED_SWAP_MAX_CHANGES 5u

typedef struct {
    uint16_t tile;
    uint16_t attributes;
} BackgroundTileCache;

typedef struct {
    uint16_t tile;
    uint16_t attributes;
    uint16_t x;
} OamSpriteCache;

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
static volatile uint16_t neogeo_vblank_signal;

static BackgroundTileCache
    background_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS][BACKGROUND_MAX_ROWS];
static uint16_t
    background_x_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint8_t
    background_chain_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint32_t
    background_generation_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint8_t
    background_world_column_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint16_t background_config_cache[SPRITE_SET_COUNT];
static uint8_t background_first_column[SPRITE_SET_COUNT];
static uint8_t background_ring_origin[SPRITE_SET_COUNT];
static uint8_t background_ring_valid[SPRITE_SET_COUNT];

static OamSpriteCache oam_cache[SPRITE_SET_COUNT][SPRITES_PER_SET];
static uint8_t active_oam[SPRITE_SET_COUNT][OAM_SPRITES];
static uint8_t active_oam_count[SPRITE_SET_COUNT];
static uint8_t active_background[SPRITE_SET_COUNT];
static uint8_t
    active_background_drivers[SPRITE_SET_COUNT][BACKGROUND_MAX_DRIVERS];
static uint8_t active_background_driver_count[SPRITE_SET_COUNT];
static uint8_t next_oam[OAM_SPRITES];
static uint8_t next_oam_count;
static uint8_t next_background_active;
static uint8_t next_background_drivers[BACKGROUND_MAX_DRIVERS];
static uint8_t next_background_driver_count;
static uint16_t next_background_y_word;

static uint16_t desired_hud[HUD_ENTRY_COUNT];
static uint16_t cached_hud[HUD_ENTRY_COUNT];
static uint32_t built_hud_generation;
static uint16_t built_hud_config;
static uint8_t hud_upload_pending;
static uint8_t hud_changed_count;
static uint16_t desired_palettes[24][4];
static uint16_t cached_palettes[24][4];
static uint16_t desired_backdrop;
static uint16_t cached_backdrop;
static uint32_t built_palette_generation;
static uint8_t palette_upload_pending;

volatile uint32_t neogeo_vblank_count;
volatile uint32_t neogeo_game_frame_count;
volatile uint16_t neogeo_render_generation;
volatile uint16_t neogeo_presented_generation;

static const uint8_t render_palette_ids[RENDER_PALETTE_COUNT] = {
    0u, 1u, 2u, 3u,
    16u, 17u, 18u, 19u,
    20u, 21u, 22u, 23u,
};

/*
 * ngdevkit's VBlank handler calls this regular C callback after preserving
 * the 68000 registers. Keeping our own monotonic count makes missed display
 * periods measurable without a software timer or emulator-specific hook.
 */
void rom_callback_VBlank(void) {
    /*
     * The active display that just ended used the last fully uploaded live
     * generation. Publishing it before waking a waiter binds debugger
     * evidence to completed scanout instead of merely elapsed VBlanks.
     */
    neogeo_presented_generation = neogeo_render_generation;
    ++neogeo_vblank_count;
    ++neogeo_vblank_signal;
}

static void wait_for_next_vblank(void) {
    uint16_t observed_signal = neogeo_vblank_signal;

    /*
     * Always wait for a display interrupt observed after rendering finishes.
     * If rendering crossed an earlier VBlank, immediately consuming that old
     * count would make the live palette/FIX/SCB3 writes happen during active
     * display. The 16-bit signal is atomic on the 68000 and equality remains
     * safe across wraparound.
     */
    while (neogeo_vblank_signal == observed_signal) {
    }
}

uint16_t neogeo_video_current_vblank(void) {
    return neogeo_vblank_signal;
}

void neogeo_video_wait_for_present(void) {
    uint16_t target_generation = neogeo_render_generation;

    while (neogeo_presented_generation != target_generation) {
        wait_for_next_vblank();
    }
}

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

static void write_sprite_x(uint16_t sprite, uint16_t x_word) {
    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB4 + sprite);
    *REG_VRAMRW = x_word;
}

static void write_single_tile_sprite_data(
    uint16_t sprite,
    uint16_t tile,
    uint16_t attributes
) {
    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = (uint16_t)(ADDR_SCB1 + sprite * 64u);
    *REG_VRAMRW = tile;
    *REG_VRAMRW = attributes;
}

static void clear_hardware_sprites(void) {
    uint16_t sprite;

    /*
     * Initialize each control block as one sequential transfer. This avoids
     * 381 address-register rewrites and leaves zoom static for the whole run.
     */
    *REG_VRAMMOD = 1;
    *REG_VRAMADDR = ADDR_SCB2;
    for (sprite = 0; sprite <= 380u; ++sprite) {
        *REG_VRAMRW = 0;
    }
    *REG_VRAMADDR = ADDR_SCB3;
    for (sprite = 0; sprite <= 380u; ++sprite) {
        *REG_VRAMRW = 0;
    }
    *REG_VRAMADDR = ADDR_SCB4;
    for (sprite = 0; sprite <= 380u; ++sprite) {
        *REG_VRAMRW = 0;
    }

    *REG_VRAMADDR = (uint16_t)(ADDR_SCB2 + FIRST_GAME_SPRITE);
    for (sprite = FIRST_GAME_SPRITE; sprite <= LAST_GAME_SPRITE; ++sprite) {
        *REG_VRAMRW = NEO_ZOOM_8X8;
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

static void build_palette_state(void) {
    uint8_t i;
    uint16_t universal;

    if (built_palette_generation == neogeo_ppu_palette_generation) {
        return;
    }
    universal = neogeo_color(palette.u8[0]);

    for (i = 0; i < 4u; ++i) {
        uint8_t source = (uint8_t)(i << 2);
        uint8_t color;

        for (color = 0; color < 4u; ++color) {
            uint16_t background_color = universal;
            uint16_t sprite_color = universal;

            if (color != 0u) {
                background_color =
                    neogeo_color(palette.u8[source + color]);
                sprite_color =
                    neogeo_color(palette.u8[16u + source + color]);
            }
            desired_palettes[i][color] = background_color;
            desired_palettes[NEO_BG_PALETTE_BASE + i][color] =
                background_color;
            desired_palettes[NEO_SPRITE_PALETTE_BASE + i][color] =
                sprite_color;
        }
    }

    desired_palettes[FIX_BORDER_PALETTE][0] = universal;
    desired_palettes[FIX_BORDER_PALETTE][1] = universal;
    desired_backdrop = universal;
    built_palette_generation = neogeo_ppu_palette_generation;
    palette_upload_pending = 1;
}

static void upload_palette_changes(void) {
    uint8_t group;

    if (palette_upload_pending == 0u) {
        return;
    }
    for (group = 0; group < RENDER_PALETTE_COUNT; ++group) {
        uint8_t palette_id = render_palette_ids[group];
        uint16_t base = (uint16_t)(palette_id << 4);
        uint8_t color;

        for (color = 0; color < 4u; ++color) {
            uint16_t value = desired_palettes[palette_id][color];

            if (cached_palettes[palette_id][color] != value) {
                MMAP_PALBANK1[base + color] = value;
                cached_palettes[palette_id][color] = value;
            }
        }
    }

    if (cached_palettes[FIX_BORDER_PALETTE][0] !=
        desired_palettes[FIX_BORDER_PALETTE][0]) {
        MMAP_PALBANK1[FIX_BORDER_PALETTE << 4] =
            desired_palettes[FIX_BORDER_PALETTE][0];
        cached_palettes[FIX_BORDER_PALETTE][0] =
            desired_palettes[FIX_BORDER_PALETTE][0];
    }
    if (cached_palettes[FIX_BORDER_PALETTE][1] !=
        desired_palettes[FIX_BORDER_PALETTE][1]) {
        MMAP_PALBANK1[(FIX_BORDER_PALETTE << 4) + 1u] =
            desired_palettes[FIX_BORDER_PALETTE][1];
        cached_palettes[FIX_BORDER_PALETTE][1] =
            desired_palettes[FIX_BORDER_PALETTE][1];
    }
    if (cached_backdrop != desired_backdrop) {
        NEO_BACKDROP_COLOR = desired_backdrop;
        cached_backdrop = desired_backdrop;
    }
    palette_upload_pending = 0;
}

static void build_background(
    uint8_t set,
    uint16_t set_base,
    uint8_t show_hud
) {
    uint16_t pattern_base = (ppu_ctrl & 0x10u) ? 256u : 0u;
    uint16_t render_config =
        (uint16_t)(pattern_base | (show_hud != 0u ? 1u : 0u));
    uint8_t fine_scroll = (uint8_t)(ppu_scroll_x & 7u);
    uint8_t first_tile_x = (uint8_t)(ppu_scroll_x >> 3);
    uint8_t first_column = (uint8_t)(
        ((ppu_ctrl & 0x01u) ? 32u : 0u) + first_tile_x
    );
    uint8_t first_tile_y = show_hud ? 4u : 1u;
    uint8_t tile_rows = show_hud ? 26u : 28u;
    uint16_t output_y = show_hud ? 24u : 0u;
    uint8_t ring_origin;
    uint8_t strip;

    next_background_active = 0;
    next_background_driver_count = 0;
    if ((ppu_mask & 0x08u) == 0u) {
        return;
    }

    if (background_config_cache[set] != render_config) {
        memset(
            background_generation_cache[set],
            0xff,
            sizeof(background_generation_cache[set])
        );
        background_config_cache[set] = render_config;
    }

    /*
     * Preserve each hardware strip's world column while the camera advances.
     * Usually only the newly entering right-hand column needs new SCB1 data;
     * fine scrolling becomes cheap SCB4 movement.
     */
    if (background_ring_valid[set] != 0u) {
        uint8_t delta = (uint8_t)(
            (first_column - background_first_column[set]) & 63u
        );

        if (delta < BACKGROUND_STRIPS) {
            uint8_t origin =
                (uint8_t)(background_ring_origin[set] + delta);

            if (origin >= BACKGROUND_STRIPS) {
                origin = (uint8_t)(origin - BACKGROUND_STRIPS);
            }
            background_ring_origin[set] = origin;
        } else {
            background_ring_origin[set] = 0;
            memset(
                background_generation_cache[set],
                0xff,
                sizeof(background_generation_cache[set])
            );
            memset(
                background_world_column_cache[set],
                0xff,
                sizeof(background_world_column_cache[set])
            );
        }
    } else {
        background_ring_valid[set] = 1;
        background_ring_origin[set] = 0;
        memset(
            background_generation_cache[set],
            0xff,
            sizeof(background_generation_cache[set])
        );
        memset(
            background_world_column_cache[set],
            0xff,
            sizeof(background_world_column_cache[set])
        );
    }

    background_first_column[set] = first_column;
    ring_origin = background_ring_origin[set];
    next_background_active = 1;

    for (strip = 0; strip < BACKGROUND_STRIPS; ++strip) {
        uint8_t world_column =
            (uint8_t)((first_column + strip) & 63u);
        uint32_t column_generation =
            neogeo_ppu_column_generation[world_column];
        uint8_t physical_strip = (uint8_t)(ring_origin + strip);
        uint16_t relative_sprite;
        uint16_t sprite;

        if (physical_strip >= BACKGROUND_STRIPS) {
            physical_strip =
                (uint8_t)(physical_strip - BACKGROUND_STRIPS);
        }
        relative_sprite =
            (uint16_t)(BACKGROUND_OFFSET + physical_strip);
        sprite = (uint16_t)(set_base + relative_sprite);

        if (
            background_world_column_cache[set][physical_strip] !=
                world_column ||
            background_generation_cache[set][physical_strip] !=
                column_generation
        ) {
            uint8_t source_x = (uint8_t)(world_column & 31u);
            uint16_t nametable_offset =
                (world_column & 32u) ? 0x0400u : 0u;
            uint8_t row;

            for (row = 0; row < tile_rows; ++row) {
                uint8_t source_y = (uint8_t)(first_tile_y + row);
                uint8_t tile =
                    nametable[
                        nametable_offset +
                        (uint16_t)source_y * 32u +
                        source_x
                    ];
                uint8_t palette_number = background_palette_index(
                    nametable_offset,
                    source_x,
                    source_y
                );
                uint16_t neogeo_tile =
                    (uint16_t)(CROM_NES_TILE_BASE + pattern_base + tile);
                uint16_t attributes = (uint16_t)(
                    (NEO_BG_PALETTE_BASE + palette_number) << 8
                );
                BackgroundTileCache *cached =
                    &background_cache[set][physical_strip][row];

                if (
                    cached->tile != neogeo_tile ||
                    cached->attributes != attributes
                ) {
                    *REG_VRAMMOD = 1;
                    *REG_VRAMADDR = (uint16_t)(
                        ADDR_SCB1 +
                        sprite * 64u +
                        (uint16_t)row * 2u
                    );
                    *REG_VRAMRW = neogeo_tile;
                    *REG_VRAMRW = attributes;
                    cached->tile = neogeo_tile;
                    cached->attributes = attributes;
                }
            }
            background_world_column_cache[set][physical_strip] =
                world_column;
            background_generation_cache[set][physical_strip] =
                column_generation;
        }
    }

    /*
     * Hardware sticky sprites inherit the preceding strip's position, height,
     * and vertical shrink. The circular strip ring therefore needs only one
     * chain when its origin is slot zero, or two chains around the physical
     * slot wrap. Configure sticky control words while this set is hidden;
     * VBlank then has only one or two live driver words to reveal.
     */
    next_background_drivers[0] = ring_origin;
    next_background_driver_count = 1;
    if (ring_origin != 0u) {
        next_background_drivers[1] = 0;
        next_background_driver_count = 2;
    }
    next_background_y_word =
        sprite_y_word((int16_t)output_y, tile_rows);

    for (strip = 0; strip < BACKGROUND_STRIPS; ++strip) {
        uint8_t sticky = (uint8_t)(
            strip != ring_origin &&
            (ring_origin == 0u || strip != 0u)
        );

        if (background_chain_cache[set][strip] != sticky) {
            *REG_VRAMMOD = 1;
            *REG_VRAMADDR = (uint16_t)(
                ADDR_SCB3 + set_base + BACKGROUND_OFFSET + strip
            );
            *REG_VRAMRW = sticky != 0u ? NEO_SCB3_STICKY : 0u;
            background_chain_cache[set][strip] = sticky;
        }
    }

    for (strip = 0; strip < next_background_driver_count; ++strip) {
        uint8_t physical_strip = next_background_drivers[strip];
        uint8_t logical_strip = physical_strip == ring_origin
            ? 0u
            : (uint8_t)(BACKGROUND_STRIPS - ring_origin);
        uint16_t x = (uint16_t)(
            NES_CONTENT_X - fine_scroll + (uint16_t)logical_strip * 8u
        );
        uint16_t x_word = sprite_x_word(x);

        if (background_x_cache[set][physical_strip] == x_word) {
            continue;
        }
        *REG_VRAMMOD = 1;
        *REG_VRAMADDR = (uint16_t)(
            ADDR_SCB4 + set_base + BACKGROUND_OFFSET + physical_strip
        );
        *REG_VRAMRW = x_word;
        background_x_cache[set][physical_strip] = x_word;
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

static void build_oam_sprites(uint8_t set, uint16_t set_base) {
    uint16_t pattern_base = (ppu_ctrl & 0x08u) ? 256u : 0u;
    uint8_t draw_left_edge = (uint8_t)(ppu_mask & 0x04u);
    uint8_t oam_index;

    next_oam_count = 0;
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
        uint8_t attributes;
        uint16_t priority_offset;
        uint16_t relative_sprite;
        uint16_t sprite;
        uint16_t neogeo_tile;
        uint16_t x_word;
        uint16_t neogeo_attributes;
        OamSpriteCache *cached;

        if (output_y >= (int16_t)NEO_SCREEN_HEIGHT || output_y + 8 <= 0) {
            continue;
        }
        if (draw_left_edge == 0u && source_x == 0u) {
            continue;
        }
        if (sprite_fits_scanline_budget(output_y) == 0u) {
            continue;
        }

        attributes = oam[offset + 2u];
        priority_offset =
            (attributes & 0x20u) ? BEHIND_SPRITE_OFFSET : FRONT_SPRITE_OFFSET;
        relative_sprite =
            (uint16_t)(priority_offset + (OAM_SPRITES - 1u - oam_index));
        sprite = (uint16_t)(set_base + relative_sprite);
        neogeo_tile = (uint16_t)(
            CROM_NES_TILE_BASE + pattern_base + oam[offset + 1u]
        );
        x_word = sprite_x_word((uint16_t)(NES_CONTENT_X + source_x));
        neogeo_attributes = (uint16_t)(
            (NEO_SPRITE_PALETTE_BASE + (attributes & 3u)) << 8
        );
        cached = &oam_cache[set][relative_sprite];

        if ((attributes & 0x40u) != 0u) {
            neogeo_attributes |= NEO_ATTR_HORIZONTAL_FLIP;
        }
        if ((attributes & 0x80u) != 0u) {
            neogeo_attributes |= NEO_ATTR_VERTICAL_FLIP;
        }

        if (cached->tile != neogeo_tile ||
            cached->attributes != neogeo_attributes) {
            write_single_tile_sprite_data(
                sprite,
                neogeo_tile,
                neogeo_attributes
            );
            cached->tile = neogeo_tile;
            cached->attributes = neogeo_attributes;
        }
        if (cached->x != x_word) {
            write_sprite_x(sprite, x_word);
            cached->x = x_word;
        }

        next_scb3[relative_sprite] = sprite_y_word(output_y, 1u);
        next_oam[next_oam_count++] = (uint8_t)relative_sprite;
    }
}

static void build_hud(uint8_t show_hud) {
    uint16_t pattern_base = (ppu_ctrl & 0x10u) ? 256u : 0u;
    uint16_t render_config =
        (uint16_t)(pattern_base | (show_hud != 0u ? 1u : 0u));
    uint8_t row;

    if (
        built_hud_generation == neogeo_ppu_hud_generation &&
        built_hud_config == render_config
    ) {
        return;
    }

    /*
     * NES tile row 0 is overscan.  Rows 1..3 become the three visible FIX
     * rows, keeping the status bar stationary while SCB strips scroll below.
     */
    hud_changed_count = 0;
    for (row = 0; row < FIX_HUD_ROWS; ++row) {
        uint8_t source_y = (uint8_t)(row + 1u);
        uint8_t column;

        for (column = 0; column < FIX_CONTENT_COLUMNS; ++column) {
            uint16_t index =
                (uint16_t)row * FIX_CONTENT_COLUMNS + column;
            uint16_t entry = FIX_BLANK_TILE;

            if (show_hud != 0u) {
                uint8_t tile =
                    nametable[(uint16_t)source_y * 32u + column];
                uint8_t palette_number =
                    background_palette_index(0, column, source_y);

                entry = (uint16_t)(
                    ((uint16_t)palette_number << 12) +
                    pattern_base +
                    tile
                );
            }
            desired_hud[index] = entry;
            if (cached_hud[index] != entry) {
                ++hud_changed_count;
            }
        }
    }
    built_hud_generation = neogeo_ppu_hud_generation;
    built_hud_config = render_config;
    hud_upload_pending = 1;
}

static void upload_hud_changes(void) {
    uint16_t index;

    if (hud_upload_pending == 0u) {
        return;
    }
    *REG_VRAMMOD = 1;
    for (index = 0; index < HUD_ENTRY_COUNT; ++index) {
        uint16_t value = desired_hud[index];

        if (cached_hud[index] != value) {
            uint8_t row = (uint8_t)(index >> 5);
            uint8_t column = (uint8_t)(index & 31u);

            *REG_VRAMADDR = (uint16_t)(
                ADDR_FIXMAP +
                ((FIX_CONTENT_X + column) << 5) +
                FIX_VISIBLE_Y +
                row
            );
            *REG_VRAMRW = value;
            cached_hud[index] = value;
        }
    }
    hud_upload_pending = 0;
    hud_changed_count = 0;
}

static void hide_sprite_set(uint8_t set) {
    uint16_t set_base = sprite_set_base(set);
    uint8_t i;

    *REG_VRAMMOD = 1;
    if (active_background[set] != 0u) {
        for (i = 0; i < active_background_driver_count[set]; ++i) {
            *REG_VRAMADDR = (uint16_t)(
                ADDR_SCB3 + set_base + BACKGROUND_OFFSET +
                active_background_drivers[set][i]
            );
            *REG_VRAMRW = 0;
        }
    }

    for (i = 0; i < active_oam_count[set]; ++i) {
        *REG_VRAMADDR = (uint16_t)(
            ADDR_SCB3 + set_base + active_oam[set][i]
        );
        *REG_VRAMRW = 0;
    }
}

static void show_next_sprite_set(uint8_t set) {
    uint16_t set_base = sprite_set_base(set);
    uint8_t i;

    *REG_VRAMMOD = 1;
    if (next_background_active != 0u) {
        for (i = 0; i < next_background_driver_count; ++i) {
            *REG_VRAMADDR = (uint16_t)(
                ADDR_SCB3 + set_base + BACKGROUND_OFFSET +
                next_background_drivers[i]
            );
            *REG_VRAMRW = next_background_y_word;
        }
    }

    for (i = 0; i < next_oam_count; ++i) {
        uint8_t relative_sprite = next_oam[i];

        *REG_VRAMADDR =
            (uint16_t)(ADDR_SCB3 + set_base + relative_sprite);
        *REG_VRAMRW = next_scb3[relative_sprite];
    }

    active_background[set] = next_background_active;
    active_background_driver_count[set] =
        next_background_driver_count;
    memcpy(
        active_background_drivers[set],
        next_background_drivers,
        next_background_driver_count
    );
    active_oam_count[set] = next_oam_count;
    memcpy(active_oam[set], next_oam, next_oam_count);
}

uint8_t neogeo_read_controller1(void) {
    uint8_t state = 0;
    /*
     * Read the Neo Geo ports themselves. Both registers are active-low, so a
     * cleared bit means "pressed".
     *
     * REG_STATUS_B's player-two Start bit is used as NES Select.  This is
     * a practical keyboard/joystick convention that keeps Select available
     * even when running an AES/home-system BIOS.
     */
    uint8_t controls = *REG_P1CNT;
    uint8_t system = *REG_STATUS_B;

    if ((controls & CNT_RIGHT) == 0u) {
        state |= NES_RIGHT;
    }
    if ((controls & CNT_LEFT) == 0u) {
        state |= NES_LEFT;
    }
    if ((controls & CNT_DOWN) == 0u) {
        state |= NES_DOWN;
    }
    if ((controls & CNT_UP) == 0u) {
        state |= NES_UP;
    }
    if ((controls & CNT_A) == 0u) {
        state |= NES_A;
    }
    if ((controls & CNT_B) == 0u) {
        state |= NES_B;
    }
    if ((system & CNT_START1) == 0u) {
        state |= NES_START;
    }
    if ((system & CNT_START2) == 0u) {
        state |= NES_SELECT;
    }
    return neogeo_input_normalize_directions(state);
}

void neogeo_video_init(void) {
    uint16_t i;

    clear_hardware_sprites();
    initialize_fix_map();

    memset(background_cache, 0xff, sizeof(background_cache));
    memset(background_x_cache, 0xff, sizeof(background_x_cache));
    memset(background_chain_cache, 0, sizeof(background_chain_cache));
    memset(
        background_generation_cache,
        0xff,
        sizeof(background_generation_cache)
    );
    memset(
        background_world_column_cache,
        0xff,
        sizeof(background_world_column_cache)
    );
    memset(background_config_cache, 0xff, sizeof(background_config_cache));
    memset(background_ring_valid, 0, sizeof(background_ring_valid));
    memset(oam_cache, 0xff, sizeof(oam_cache));
    memset(active_oam_count, 0, sizeof(active_oam_count));
    memset(active_background, 0, sizeof(active_background));
    memset(
        active_background_driver_count,
        0,
        sizeof(active_background_driver_count)
    );
    memset(cached_palettes, 0xff, sizeof(cached_palettes));
    cached_backdrop = 0xffffu;
    built_palette_generation = 0xffffffffu;
    palette_upload_pending = 0;
    built_hud_generation = 0xffffffffu;
    built_hud_config = 0xffffu;
    hud_upload_pending = 0;
    hud_changed_count = 0;

    for (i = 0; i < HUD_ENTRY_COUNT; ++i) {
        desired_hud[i] = FIX_BLANK_TILE;
        cached_hud[i] = FIX_BLANK_TILE;
    }

    build_palette_state();
    wait_for_next_vblank();
    upload_palette_changes();

    visible_set = 0;
    neogeo_game_frame_count = 0;
    neogeo_render_generation = 0;
    neogeo_presented_generation = 0;
}

void neogeo_video_render(void) {
#if !defined(SMB_NEOGEO_LOGIC_BENCH)
    uint8_t next_set = (uint8_t)(visible_set ^ 1u);
    uint16_t set_base = sprite_set_base(next_set);
    uint8_t split_live_update;
    uint8_t show_hud =
        (uint8_t)(
            (ppu_mask & 0x08u) != 0u &&
            ram[Sprite0HitDetectFlag] != 0u
        );

    build_background(next_set, set_base, show_hud);
    build_oam_sprites(next_set, set_base);
    build_palette_state();
    build_hud(show_hud);

    /*
     * Linked MC68000 cycle analysis gives these conservative post-wait
     * payload bounds for this implementation:
     *
     *   palette clean + HUD scan + split gate + worst SCB swap:
     *       24394 + 106 * changed HUD cells
     *   worst split palette + HUD phase: 22724
     *   worst split SCB phase: 18316
     *
     * Five changed HUD cells keep the shared path at 24924 cycles; six would
     * need 25030. A palette-only update plus the worst SCB swap remains below
     * the same 25000-cycle ceiling, but palette and a pending HUD scan
     * together must split. This decision is made before waiting, so crossing
     * a VBlank while building still cannot make the live writes consume a
     * stale interrupt.
     */
    split_live_update = (uint8_t)(
        hud_upload_pending != 0u &&
        (
            palette_upload_pending != 0u ||
            hud_changed_count > HUD_SHARED_SWAP_MAX_CHANGES
        )
    );
#endif

    /*
     * Palette/FIX/SCB3 are the only live-display state. Upload palette/FIX in
     * a fresh VBlank. Large updates wait for another fresh VBlank before the
     * bounded SCB3 swap; common frames safely complete all work in one.
     */
    wait_for_next_vblank();
#if !defined(SMB_NEOGEO_LOGIC_BENCH)
    upload_palette_changes();
    upload_hud_changes();
    if (split_live_update != 0u) {
        wait_for_next_vblank();
    }
    hide_sprite_set(visible_set);
    show_next_sprite_set(next_set);
    visible_set = next_set;
    ++neogeo_render_generation;
#endif
    ++neogeo_game_frame_count;
}
