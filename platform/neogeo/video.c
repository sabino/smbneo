#include "video.h"

#include "background_chain.h"
#include "constants.h"
#include "cpu.h"
#include "external.h"
#include "hud_update.h"
#include "input_policy.h"
#include "oam_tiles.h"
#include "oam_visibility.h"
#include "palette_policy.h"
#include "ppu.h"
#include "ppu_render_state.h"
#include "vblank_budget.h"

#include <ngdevkit/neogeo.h>

#define NEO_SCREEN_WIDTH 320u
#define NES_CROP_TOP 8
#define NES_CONTENT_X ((NEO_SCREEN_WIDTH - SCREEN_WIDTH) / 2u)

#define FIX_CONTENT_X (NES_CONTENT_X / 8u)
#define FIX_CONTENT_COLUMNS 32u
#define FIX_BLANK_TILE NEOGEO_HUD_FIX_BLANK_TILE
#define FIX_SOLID_TILE 514u
#define FIX_BORDER_PALETTE 15u

/*
 * GnGeo's full-frame DDA emits nine rows for vertical zoom 0x7f on a
 * one-tile object, duplicating the first source row. The 0x7e and 0x7f L0
 * tables select the same eight in-tile rows on hardware; 0x7e also keeps the
 * emulator at exactly eight output rows. Multi-tile background strips retain
 * 0x7f because their vertical chain uses the following table entries.
 */
#define NEO_ZOOM_OAM_8X8 0x077eu
#define NEO_ZOOM_BACKGROUND_8X8 0x077fu
#define NEO_BG_PALETTE_BASE 16u

#define OAM_SPRITES 64u
#define BACKGROUND_STRIPS 33u
#define BACKGROUND_MAX_ROWS 28u
#define BACKGROUND_HARDWARE_TILE_ROWS 32u
#define BACKGROUND_HARDWARE_CHAIN_ROWS 33u
#define BACKGROUND_HUD_PADDING_ROWS 3u
#if defined(SMB_NEOGEO_FBNEO)
#define BACKGROUND_FBNEO_HALF_ROWS 16u
#define BACKGROUND_FBNEO_CHAIN_ROWS 32u
#endif
#define BEHIND_SPRITE_OFFSET 0u
#define BACKGROUND_OFFSET (BEHIND_SPRITE_OFFSET + OAM_SPRITES)
#define FRONT_SPRITE_OFFSET (BACKGROUND_OFFSET + BACKGROUND_STRIPS)
#define SPRITES_PER_SET (FRONT_SPRITE_OFFSET + OAM_SPRITES)
#define SPRITE_SET_COUNT 2u
#define FIRST_GAME_SPRITE 1u
#define BEHIND_SPRITE_BASE FIRST_GAME_SPRITE
#define BACKGROUND_SPRITE_BASE \
    (BEHIND_SPRITE_BASE + OAM_SPRITES * SPRITE_SET_COUNT)
#define FRONT_SPRITE_BASE \
    (BACKGROUND_SPRITE_BASE + BACKGROUND_STRIPS * SPRITE_SET_COUNT)
#define LAST_GAME_SPRITE \
    (FRONT_SPRITE_BASE + OAM_SPRITES * SPRITE_SET_COUNT - 1u)

#if LAST_GAME_SPRITE > 380u
#error Neo Geo renderer exceeds the 381 displayable sprite slots
#endif

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
#define HUD_ENTRY_COUNT NEOGEO_HUD_ENTRY_COUNT

_Static_assert(
    FIX_CONTENT_X == NEOGEO_HUD_FIX_CONTENT_X,
    "HUD FIX-map X origin must match the renderer crop"
);
_Static_assert(
    ADDR_FIXMAP == NEOGEO_HUD_FIXMAP_BASE,
    "HUD FIX-map base must match ngdevkit"
);

typedef struct {
    uint16_t tile;
    uint16_t attributes;
} BackgroundTileCache;

/*
 * The pinned visual-reference palette, converted offline to the Neo Geo's
 * 5-bit-per-channel plus shared-low-bit color format. Keeping this in ROM
 * costs 128 bytes and avoids an RGB framebuffer or per-frame conversion.
 */
static const uint16_t nes_palette_to_neogeo[64] = {
#include "nes_palette_neogeo.inc"
};

/*
 * Only SCB3 needs staging.  SCB1, SCB2, and SCB4 can be updated while their
 * set is hidden; 322 bytes buys a clean VBlank reveal without a framebuffer.
 */
static uint16_t next_scb3[SPRITES_PER_SET];
static uint8_t visible_set;
static volatile uint16_t neogeo_vblank_signal;

static BackgroundTileCache
    background_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS][BACKGROUND_MAX_ROWS];
static uint16_t background_x_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint32_t
    background_generation_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint8_t
    background_world_column_cache[SPRITE_SET_COUNT][BACKGROUND_STRIPS];
static uint16_t background_config_cache[SPRITE_SET_COUNT];
static uint8_t background_first_column[SPRITE_SET_COUNT];
static uint8_t background_ring_origin[SPRITE_SET_COUNT];
static uint8_t background_ring_valid[SPRITE_SET_COUNT];
static uint8_t background_chain_origin[SPRITE_SET_COUNT];
static uint8_t background_chain_valid[SPRITE_SET_COUNT];
static uint32_t background_built_generation[SPRITE_SET_COUNT];

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
static uint16_t next_background_x_word[BACKGROUND_MAX_DRIVERS];

static NeoGeoHudState hud_state;
static uint16_t desired_palettes[24][4];
static uint16_t cached_palettes[24][4];
static uint16_t desired_backdrop;
static uint16_t cached_backdrop;
static uint32_t built_palette_generation;
static uint8_t palette_upload_pending;
static uint8_t palette_changed_count;

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

#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
void neogeo_video_benchmark_invalidate(void) {
    memset(background_cache, 0xff, sizeof(background_cache));
    memset(background_x_cache, 0xff, sizeof(background_x_cache));
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
    memset(background_chain_valid, 0, sizeof(background_chain_valid));
    memset(
        background_built_generation,
        0xff,
        sizeof(background_built_generation)
    );

    memset(cached_palettes, 0xff, sizeof(cached_palettes));
    cached_backdrop = 0xffffu;
    built_palette_generation = 0xffffffffu;
    palette_upload_pending = 0u;
    palette_changed_count = 0u;

    memset(hud_state.cached, 0xff, sizeof(hud_state.cached));
    hud_state.built_generation = 0xffffffffu;
    hud_state.built_config = 0xffffu;
    hud_state.upload_pending = 0u;
    hud_state.changed_count = 0u;
    hud_state.upload_cursor = 0u;
    neogeo_ppu_hud_dirty_invalidate();
}
#endif

/*
 * LSPC requires at least twelve 68000 cycles between VRAM register accesses.
 * Forcing the absolute-long form makes each store take sixteen cycles by
 * itself, even under LTO; an address-register loop can otherwise collapse
 * consecutive writes to eight cycles.
 */
static __attribute__((always_inline)) inline void write_vram_address(
    uint16_t value
) {
    __asm__ volatile(
        "move.w %0,0x3c0000.l"
        :
        : "d"(value)
        : "memory"
    );
}

static __attribute__((always_inline)) inline void write_vram_data(
    uint16_t value
) {
    __asm__ volatile(
        "move.w %0,0x3c0002.l"
        :
        : "d"(value)
        : "memory"
    );
}

static __attribute__((always_inline)) inline void write_vram_mod(
    uint16_t value
) {
    __asm__ volatile(
        "move.w %0,0x3c0004.l"
        :
        : "d"(value)
        : "memory"
    );
}

static uint16_t background_hardware_sprite(
    uint8_t set,
    uint8_t physical_strip
) {
    return (uint16_t)(
        BACKGROUND_SPRITE_BASE +
        (uint16_t)set * BACKGROUND_STRIPS +
        physical_strip
    );
}

static uint16_t oam_hardware_sprite(
    uint8_t set,
    uint16_t relative_sprite
) {
    if (relative_sprite < FRONT_SPRITE_OFFSET) {
        return (uint16_t)(
            BEHIND_SPRITE_BASE +
            (uint16_t)set * OAM_SPRITES +
            relative_sprite
        );
    }
    return (uint16_t)(
        FRONT_SPRITE_BASE +
        (uint16_t)set * OAM_SPRITES +
        relative_sprite -
        FRONT_SPRITE_OFFSET
    );
}

static uint16_t sprite_y_word(int16_t y, uint16_t height_tiles) {
    uint16_t hardware_y = (uint16_t)((496 - (int32_t)y) & 0x01ff);
    return (uint16_t)((hardware_y << 7) | (height_tiles & 0x003fu));
}

static uint16_t sprite_x_word(uint16_t x) {
    return (uint16_t)(x << 7);
}

#if defined(SMB_NEOGEO_FBNEO)
static uint8_t background_scb1_row(uint8_t logical_row) {
    return logical_row < BACKGROUND_FBNEO_HALF_ROWS
        ? (uint8_t)(logical_row + BACKGROUND_FBNEO_HALF_ROWS)
        : (uint8_t)(logical_row - BACKGROUND_FBNEO_HALF_ROWS);
}
#endif

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
    write_vram_address((uint16_t)(ADDR_SCB4 + sprite));
    write_vram_data(x_word);
}

static void write_single_tile_sprite_data(
    uint16_t sprite,
    uint16_t tile,
    uint16_t attributes
) {
    write_vram_address((uint16_t)(ADDR_SCB1 + sprite * 64u));
    write_vram_data(tile);
    write_vram_data(attributes);
}

static void clear_hardware_sprites(void) {
    uint16_t sprite;

    /*
     * Initialize each control block as one sequential transfer. This avoids
     * 381 address-register rewrites and leaves zoom static for the whole run.
     */
    write_vram_mod(1);
    wait_for_next_vblank();
    write_vram_address(ADDR_SCB2);
    for (sprite = 0; sprite <= 380u; ++sprite) {
        write_vram_data(0);
    }
    wait_for_next_vblank();
    write_vram_address(ADDR_SCB3);
    for (sprite = 0; sprite <= 380u; ++sprite) {
        write_vram_data(0);
    }
    wait_for_next_vblank();
    write_vram_address(ADDR_SCB4);
    for (sprite = 0; sprite <= 380u; ++sprite) {
        write_vram_data(0);
    }

    wait_for_next_vblank();
    write_vram_address((uint16_t)(ADDR_SCB2 + FIRST_GAME_SPRITE));
    for (
        sprite = BEHIND_SPRITE_BASE;
        sprite < BACKGROUND_SPRITE_BASE;
        ++sprite
    ) {
        write_vram_data(NEO_ZOOM_OAM_8X8);
    }
    for (
        sprite = BACKGROUND_SPRITE_BASE;
        sprite < FRONT_SPRITE_BASE;
        ++sprite
    ) {
        write_vram_data(NEO_ZOOM_BACKGROUND_8X8);
    }
    for (
        sprite = FRONT_SPRITE_BASE;
        sprite <= LAST_GAME_SPRITE;
        ++sprite
    ) {
        write_vram_data(NEO_ZOOM_OAM_8X8);
    }
}

static void initialize_fix_map(void) {
    uint16_t x;
    uint16_t y;

    /*
     * The FIX layer is always above sprites.  Opaque 32-pixel side borders
     * crop fine-scroll and OAM pixels to the original 256-pixel NES viewport.
     */
    write_vram_mod(1);
    for (x = 0; x < 40u; ++x) {
        uint16_t entry =
            (x < FIX_CONTENT_X || x >= FIX_CONTENT_X + FIX_CONTENT_COLUMNS)
                ? (uint16_t)((FIX_BORDER_PALETTE << 12) | FIX_SOLID_TILE)
                : FIX_BLANK_TILE;

        if ((x & 7u) == 0u) {
            wait_for_next_vblank();
        }
        write_vram_address((uint16_t)(ADDR_FIXMAP + (x << 5)));
        for (y = 0; y < 32u; ++y) {
            write_vram_data(entry);
        }
    }
}

static uint8_t count_palette_changes(void) {
    uint8_t changed = 0;
    uint8_t group;

    for (group = 0; group < RENDER_PALETTE_COUNT; ++group) {
        uint8_t palette_id = render_palette_ids[group];
        uint16_t base = (uint16_t)(palette_id << 4);
        uint8_t color;

        for (color = 0; color < 4u; ++color) {
            if (
                base + color != NEO_PALETTE_REFERENCE_WORD_INDEX &&
                cached_palettes[palette_id][color] !=
                    desired_palettes[palette_id][color]
            ) {
                ++changed;
            }
        }
    }
    if (
        cached_palettes[FIX_BORDER_PALETTE][0] !=
        desired_palettes[FIX_BORDER_PALETTE][0]
    ) {
        ++changed;
    }
    if (
        cached_palettes[FIX_BORDER_PALETTE][1] !=
        desired_palettes[FIX_BORDER_PALETTE][1]
    ) {
        ++changed;
    }
    if (cached_backdrop != desired_backdrop) {
        ++changed;
    }
    return changed;
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
            desired_palettes[i][color] = neogeo_palette_ram_value(
                (uint16_t)((uint16_t)i * 16u + color),
                background_color
            );
            desired_palettes[NEO_BG_PALETTE_BASE + i][color] =
                neogeo_palette_ram_value(
                    (uint16_t)(
                        (NEO_BG_PALETTE_BASE + i) * 16u + color
                    ),
                    background_color
                );
            desired_palettes[NEO_OAM_PALETTE_BASE + i][color] =
                neogeo_palette_ram_value(
                    (uint16_t)(
                        (NEO_OAM_PALETTE_BASE + i) * 16u + color
                    ),
                    sprite_color
                );
        }
    }

    desired_palettes[FIX_BORDER_PALETTE][0] = universal;
    desired_palettes[FIX_BORDER_PALETTE][1] = universal;
    desired_backdrop = neogeo_backdrop_value(universal);
    built_palette_generation = neogeo_ppu_palette_generation;
    palette_changed_count = count_palette_changes();
    palette_upload_pending = (uint8_t)(palette_changed_count != 0u);
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

            /* Word zero is initialized explicitly in both hardware banks. */
            if (base + color == NEO_PALETTE_REFERENCE_WORD_INDEX) {
                continue;
            }

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
    palette_changed_count = 0;
}

static void build_background(uint8_t set, uint8_t show_hud) {
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
    uint32_t source_generation = show_hud
        ? neogeo_ppu_background_hud_generation
        : neogeo_ppu_background_full_generation;
    uint8_t scan_background;
    uint8_t reset_physical_map;
    uint8_t ring_origin;
    uint8_t strip;

    next_background_active = 0;
    next_background_driver_count = 0;
    if ((ppu_mask & 0x08u) == 0u) {
        return;
    }

    reset_physical_map =
        (uint8_t)(background_config_cache[set] != render_config);
    scan_background = (uint8_t)(
        background_ring_valid[set] == 0u ||
        reset_physical_map != 0u ||
        background_first_column[set] != first_column ||
        background_built_generation[set] != source_generation
    );
    if (reset_physical_map != 0u) {
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
     * fine scrolling becomes one or two driver updates on the hidden bank.
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

    if (scan_background != 0u) {
        write_vram_mod(1);
        for (strip = 0; strip < BACKGROUND_STRIPS; ++strip) {
            uint8_t world_column =
                (uint8_t)((first_column + strip) & 63u);
            uint32_t column_generation =
                neogeo_ppu_column_generation[world_column];
            uint8_t physical_strip = (uint8_t)(ring_origin + strip);

            if (physical_strip >= BACKGROUND_STRIPS) {
                physical_strip =
                    (uint8_t)(physical_strip - BACKGROUND_STRIPS);
            }

            if (
                background_world_column_cache[set][physical_strip] !=
                    world_column ||
                background_generation_cache[set][physical_strip] !=
                    column_generation
            ) {
                uint8_t source_x = (uint8_t)(world_column & 31u);
                uint16_t nametable_offset =
                    (world_column & 32u) ? 0x0400u : 0u;
                uint16_t sprite =
                    background_hardware_sprite(set, physical_strip);
                uint8_t previous_physical_row = 0xffu;
                uint8_t row;

#if defined(SMB_NEOGEO_FBNEO)
                if (tile_rows > BACKGROUND_FBNEO_HALF_ROWS) {
                    uint8_t blank_row = (uint8_t)(
                        tile_rows - BACKGROUND_FBNEO_HALF_ROWS
                    );

                    write_vram_address((uint16_t)(
                        ADDR_SCB1 + sprite * 64u +
                        (uint16_t)blank_row * 2u
                    ));
                    while (blank_row < BACKGROUND_FBNEO_HALF_ROWS) {
                        write_vram_data(CROM_BLANK_TILE);
                        write_vram_data(0);
                        ++blank_row;
                    }
                }
#else
                if (reset_physical_map != 0u) {
                    uint8_t physical_row = 0;

                    write_vram_address(
                        (uint16_t)(ADDR_SCB1 + sprite * 64u)
                    );
                    while (physical_row < BACKGROUND_HARDWARE_TILE_ROWS) {
                        write_vram_data(CROM_BLANK_TILE);
                        write_vram_data(0);
                        ++physical_row;
                    }
                }
#endif

                for (row = 0; row < tile_rows; ++row) {
                    uint8_t source_y = (uint8_t)(first_tile_y + row);
                    uint8_t tile = nametable[
                        nametable_offset +
                        (uint16_t)source_y * 32u +
                        source_x
                    ];
                    uint8_t palette_number = background_palette_index(
                        nametable_offset,
                        source_x,
                        source_y
                    );
                    uint16_t neogeo_tile = (uint16_t)(
                        CROM_NES_TILE_BASE + pattern_base + tile
                    );
                    uint16_t attributes = (uint16_t)(
                        (NEO_BG_PALETTE_BASE + palette_number) << 8
                    );
                    BackgroundTileCache *cached =
                        &background_cache[set][physical_strip][row];
                    uint8_t physical_row;

                    if (
                        reset_physical_map == 0u &&
                        cached->tile == neogeo_tile &&
                        cached->attributes == attributes
                    ) {
                        continue;
                    }
#if defined(SMB_NEOGEO_FBNEO)
                    physical_row = background_scb1_row(row);
#else
                    physical_row = (uint8_t)(
                        row +
                        (show_hud != 0u ? BACKGROUND_HUD_PADDING_ROWS : 0u)
                    );
#endif
                    if (
                        previous_physical_row == 0xffu ||
                        physical_row != (uint8_t)(previous_physical_row + 1u)
                    ) {
                        write_vram_address((uint16_t)(
                            ADDR_SCB1 + sprite * 64u +
                            (uint16_t)physical_row * 2u
                        ));
                    }
                    write_vram_data(neogeo_tile);
                    write_vram_data(attributes);
                    cached->tile = neogeo_tile;
                    cached->attributes = attributes;
                    previous_physical_row = physical_row;
                }
                background_world_column_cache[set][physical_strip] =
                    world_column;
                background_generation_cache[set][physical_strip] =
                    column_generation;
            }
        }
    }
    background_built_generation[set] = source_generation;

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
#if defined(SMB_NEOGEO_FBNEO)
    /*
     * FBNeo evaluates a chain longer than 16 tiles across both halves of the
     * Neo Geo shrink table. Put logical rows 16+ before rows 0..15 in SCB1,
     * then start the 32-tile wrapped chain 128 pixels lower so the visible
     * result retains the native row positions.
     */
    if (tile_rows > BACKGROUND_FBNEO_HALF_ROWS) {
        next_background_y_word = sprite_y_word(
            (int16_t)(output_y + BACKGROUND_FBNEO_HALF_ROWS * 8u),
            BACKGROUND_FBNEO_CHAIN_ROWS
        );
    } else {
        next_background_y_word =
            sprite_y_word((int16_t)output_y, tile_rows);
    }
#else
    /*
     * Real LSPC hardware applies vertical shrink to the entire strip, not to
     * each tile independently. Height 33 selects the documented 32-tile
     * full-height mode; zoom 0x7f then maps every 16-pixel C-ROM tile to one
     * 8-pixel source row across the complete 256-line period.
     */
    next_background_y_word =
        sprite_y_word(0, BACKGROUND_HARDWARE_CHAIN_ROWS);
#endif

    for (strip = 0; strip < next_background_driver_count; ++strip) {
        uint8_t physical_strip = next_background_drivers[strip];
        uint8_t logical_strip = physical_strip == ring_origin
            ? 0u
            : (uint8_t)(BACKGROUND_STRIPS - ring_origin);
        uint16_t x = (uint16_t)(
            NES_CONTENT_X - fine_scroll + (uint16_t)logical_strip * 8u
        );

        next_background_x_word[strip] = sprite_x_word(x);
    }
}

static void prepare_background_hidden(uint8_t set) {
    NeoGeoBackgroundChainPlan chain_plan;
    uint8_t ring_origin;
    uint8_t strip;

    if (next_background_active == 0u) {
        return;
    }
    ring_origin = next_background_drivers[0];
    chain_plan = neogeo_background_chain_plan(
        background_chain_valid[set],
        background_chain_origin[set],
        ring_origin
    );
    write_vram_mod(1);

    /*
     * Reconcile every SCB3 word after initialization/invalidation. Thereafter
     * a ring-origin change affects only the entering root and the old wrapped
     * root. Clear the new root before making the old one sticky so even an
     * interrupted hidden-bank update cannot join neighboring sprite banks.
     */
    if (chain_plan.full_rebuild != 0u) {
        for (strip = 0; strip < BACKGROUND_STRIPS; ++strip) {
            uint8_t sticky = (uint8_t)(
                strip != 0u && strip != ring_origin
            );

            write_vram_address((uint16_t)(
                ADDR_SCB3 + background_hardware_sprite(set, strip)
            ));
            write_vram_data(sticky != 0u ? NEO_SCB3_STICKY : 0u);
        }
    } else {
        if (chain_plan.clear_root != 0xffu) {
            strip = chain_plan.clear_root;
            write_vram_address((uint16_t)(
                ADDR_SCB3 + background_hardware_sprite(set, strip)
            ));
            write_vram_data(0u);
        }
        if (chain_plan.set_sticky != 0xffu) {
            strip = chain_plan.set_sticky;
            write_vram_address((uint16_t)(
                ADDR_SCB3 + background_hardware_sprite(set, strip)
            ));
            write_vram_data(NEO_SCB3_STICKY);
        }
    }
    background_chain_origin[set] = ring_origin;
    background_chain_valid[set] = 1u;
    for (strip = 0; strip < next_background_driver_count; ++strip) {
        uint8_t physical_strip = next_background_drivers[strip];
        uint16_t x_word = next_background_x_word[strip];

        if (background_x_cache[set][physical_strip] != x_word) {
            write_vram_address((uint16_t)(
                ADDR_SCB4 +
                background_hardware_sprite(set, physical_strip)
            ));
            write_vram_data(x_word);
            background_x_cache[set][physical_strip] = x_word;
        }
    }
}

static void build_oam_sprites(uint8_t set) {
    uint16_t pattern_base = (ppu_ctrl & 0x08u) ? 256u : 0u;
    uint8_t draw_left_edge = (uint8_t)(ppu_mask & 0x04u);
    uint8_t oam_index;

    next_oam_count = 0;
    if ((ppu_mask & 0x10u) == 0u) {
        return;
    }

    /* No OAM SCB1/SCB4 path below changes the sequential VRAM modifier. */
    write_vram_mod(1);

    /*
     * Evaluate every in-range OAM entry. Neo Geo priority increases with
     * sprite number, so reversing each 64-slot bank keeps lower source OAM
     * indices in front without reproducing the source hardware's much lower
     * eight-sprites-per-scanline limit.
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

        if (
            neogeo_oam_entry_visible(
                output_y,
                source_x,
                draw_left_edge
            ) == 0u
        ) {
            continue;
        }
        attributes = oam[offset + 2u];
        priority_offset =
            (attributes & 0x20u) ? BEHIND_SPRITE_OFFSET : FRONT_SPRITE_OFFSET;
        relative_sprite =
            (uint16_t)(priority_offset + (OAM_SPRITES - 1u - oam_index));
        sprite = oam_hardware_sprite(set, relative_sprite);
        neogeo_tile = neogeo_oam_tile_number(
            pattern_base,
            oam[offset + 1u],
            attributes
        );
        x_word = sprite_x_word((uint16_t)(NES_CONTENT_X + source_x));
        neogeo_attributes = neogeo_oam_tile_attributes(attributes);

        write_single_tile_sprite_data(
            sprite,
            neogeo_tile,
            neogeo_attributes
        );
        write_sprite_x(sprite, x_word);

        next_scb3[relative_sprite] = sprite_y_word(output_y, 1u);
        next_oam[next_oam_count++] = (uint8_t)relative_sprite;
    }
}

static void build_hud(uint8_t show_hud) {
    uint16_t pattern_base = (ppu_ctrl & 0x10u) ? 256u : 0u;

    /*
     * NES tile row 0 is overscan.  Rows 1..3 become the three visible FIX
     * rows, keeping the status bar stationary while SCB strips scroll below.
     */
    (void)neogeo_hud_build(
        &hud_state,
        nametable,
        neogeo_ppu_hud_generation,
        neogeo_ppu_hud_dirty_rows,
        &neogeo_ppu_hud_dirty_tracking_valid,
        pattern_base,
        show_hud
    );
}

static void upload_hud_chunk(void) {
    uint8_t chunk_end;

    if (hud_state.upload_pending == 0u) {
        return;
    }
    chunk_end = (uint8_t)(
        hud_state.upload_cursor +
        neogeo_vblank_hud_chunk(
            (uint16_t)(
                hud_state.changed_count - hud_state.upload_cursor
            )
        )
    );
    write_vram_mod(1);
    while (hud_state.upload_cursor < chunk_end) {
        uint16_t index =
            hud_state.changed_indices[hud_state.upload_cursor];
        uint16_t value = hud_state.desired[index];

        write_vram_address(neogeo_hud_fixmap_address((uint8_t)index));
        write_vram_data(value);
        hud_state.cached[index] = value;
        ++hud_state.upload_cursor;
    }
    if (hud_state.upload_cursor == hud_state.changed_count) {
        hud_state.upload_pending = 0;
        hud_state.changed_count = 0;
        hud_state.upload_cursor = 0;
    }
}

static void hide_sprite_set(uint8_t set) {
    uint8_t i;
    uint16_t previous_sprite = 0xffffu;

    write_vram_mod(1);
    if (active_background[set] != 0u) {
        for (i = 0; i < active_background_driver_count[set]; ++i) {
            write_vram_address((uint16_t)(
                ADDR_SCB3 + background_hardware_sprite(
                    set,
                    active_background_drivers[set][i]
                )
            ));
            write_vram_data(0);
        }
    }
    active_background[set] = 0;

    /*
     * Source OAM order maps consecutive same-plane entries onto descending
     * hardware slots. A -1 modifier turns each such run into one address load.
     */
    write_vram_mod(0xffffu);
    for (i = 0; i < active_oam_count[set]; ++i) {
        uint16_t sprite =
            oam_hardware_sprite(set, active_oam[set][i]);

        if (sprite + 1u != previous_sprite) {
            write_vram_address((uint16_t)(ADDR_SCB3 + sprite));
        }
        write_vram_data(0);
        previous_sprite = sprite;
    }
}

static void show_next_sprite_set(uint8_t set) {
    uint8_t i;
    uint16_t previous_sprite = 0xffffu;

    write_vram_mod(1);
    if (next_background_active != 0u) {
        for (i = 0; i < next_background_driver_count; ++i) {
            write_vram_address((uint16_t)(
                ADDR_SCB3 + background_hardware_sprite(
                    set,
                    next_background_drivers[i]
                )
            ));
            write_vram_data(next_background_y_word);
        }
    }

    write_vram_mod(0xffffu);
    for (i = 0; i < next_oam_count; ++i) {
        uint8_t relative_sprite = next_oam[i];
        uint16_t sprite = oam_hardware_sprite(set, relative_sprite);

        if (sprite + 1u != previous_sprite) {
            write_vram_address((uint16_t)(ADDR_SCB3 + sprite));
        }
        write_vram_data(next_scb3[relative_sprite]);
        previous_sprite = sprite;
    }
    write_vram_mod(1);

    active_background[set] = next_background_active;
    active_background_driver_count[set] = next_background_driver_count;
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

    /*
     * BIOS variants do not all hand the cartridge the same video-latch
     * state. Establish the palette state used by this renderer before its
     * first palette upload instead of inheriting shadow or bank 1.
     */
    *REG_NOSHADOW = 1;
    *REG_PALBANK0 = 1;

    clear_hardware_sprites();
    initialize_fix_map();

    memset(background_cache, 0xff, sizeof(background_cache));
    memset(background_x_cache, 0xff, sizeof(background_x_cache));
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
    memset(background_chain_valid, 0, sizeof(background_chain_valid));
    memset(
        background_built_generation,
        0xff,
        sizeof(background_built_generation)
    );
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
    palette_changed_count = 0;
    hud_state.built_generation = 0xffffffffu;
    hud_state.built_config = 0xffffu;
    hud_state.upload_pending = 0;
    hud_state.changed_count = 0;
    hud_state.upload_cursor = 0;
    neogeo_ppu_hud_dirty_invalidate();

    for (i = 0; i < HUD_ENTRY_COUNT; ++i) {
        hud_state.desired[i] = FIX_BLANK_TILE;
        hud_state.cached[i] = FIX_BLANK_TILE;
    }

    build_palette_state();
    wait_for_next_vblank();

    /* Both physical palette banks reserve word zero as analog black. */
    *REG_PALBANK1 = 1;
    MMAP_PALBANK1[NEO_PALETTE_REFERENCE_WORD_INDEX] =
        NEO_PALETTE_REFERENCE_BLACK;
    *REG_PALBANK0 = 1;
    MMAP_PALBANK1[NEO_PALETTE_REFERENCE_WORD_INDEX] =
        NEO_PALETTE_REFERENCE_BLACK;
    cached_palettes[0][0] = NEO_PALETTE_REFERENCE_BLACK;
    upload_palette_changes();

    visible_set = 0;
    neogeo_game_frame_count = 0;
    neogeo_render_generation = 0;
    neogeo_presented_generation = 0;
}

void neogeo_video_render(void) {
#if !defined(SMB_NEOGEO_LOGIC_BENCH)
    uint8_t next_set = (uint8_t)(visible_set ^ 1u);
    NeoVblankCommitPhase commit_phase;
    uint8_t share_live_with_swap;
    uint8_t show_hud =
        (uint8_t)(
            (ppu_mask & 0x08u) != 0u &&
            ram[Sprite0HitDetectFlag] != 0u
    );

    build_background(next_set, show_hud);
    prepare_background_hidden(next_set);
    build_oam_sprites(next_set);
    build_palette_state();
    build_hud(show_hud);
    commit_phase = neogeo_vblank_choose_commit_phase(
        1u,
        palette_changed_count,
        hud_state.changed_count
    );
    share_live_with_swap = (uint8_t)(
        commit_phase == NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
#endif

    /*
     * SCB1/SCB4 construction above targets only the hidden background/OAM
     * bank, so it may safely span display periods. Palette/FIX work is split
     * into bounded live-update chunks. Small measured palette/FIX deltas can
     * share the old/new SCB3 swap only when the worst-case policy stays under
     * its ceiling; larger live changes get an earlier VBlank.
     * Normal gameplay therefore retains the one-game-tick-per-VBlank path.
     */
#if !defined(SMB_NEOGEO_LOGIC_BENCH)
    while (
        (palette_upload_pending != 0u &&
            share_live_with_swap == 0u) ||
        (hud_state.upload_pending != 0u && share_live_with_swap == 0u)
    ) {
        wait_for_next_vblank();
        upload_palette_changes();
        upload_hud_chunk();
    }
#endif

    wait_for_next_vblank();
#if !defined(SMB_NEOGEO_LOGIC_BENCH)
    if (share_live_with_swap != 0u) {
        upload_palette_changes();
        upload_hud_chunk();
    }
    hide_sprite_set(visible_set);
    show_next_sprite_set(next_set);
    visible_set = next_set;
    ++neogeo_render_generation;
#endif
    ++neogeo_game_frame_count;
}
