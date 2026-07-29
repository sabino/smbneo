#ifndef SMB_NEOGEO_HUD_UPDATE_H
#define SMB_NEOGEO_HUD_UPDATE_H

#include <stdint.h>

#define NEOGEO_HUD_ROWS 3u
#define NEOGEO_HUD_COLUMNS 32u
#define NEOGEO_HUD_ENTRY_COUNT (NEOGEO_HUD_ROWS * NEOGEO_HUD_COLUMNS)
#define NEOGEO_HUD_FIX_TILE_BASE 1u
#define NEOGEO_HUD_FIX_BLANK_TILE 513u
#define NEOGEO_HUD_FIXMAP_BASE UINT16_C(0x7000)
#define NEOGEO_HUD_FIX_CONTENT_X 4u
#define NEOGEO_HUD_FIX_VISIBLE_Y 2u

typedef struct {
    uint16_t desired[NEOGEO_HUD_ENTRY_COUNT];
    uint16_t cached[NEOGEO_HUD_ENTRY_COUNT];
    uint32_t built_generation;
    uint16_t built_config;
    uint8_t upload_pending;
    uint8_t changed_count;
    uint8_t upload_cursor;
    uint8_t changed_indices[NEOGEO_HUD_ENTRY_COUNT];
} NeoGeoHudState;

static inline uint16_t neogeo_hud_fixmap_address(uint8_t index) {
    uint8_t row = (uint8_t)(index >> 5);
    uint8_t column = (uint8_t)(index & 31u);

    return (uint16_t)(
        NEOGEO_HUD_FIXMAP_BASE +
        ((NEOGEO_HUD_FIX_CONTENT_X + column) << 5) +
        NEOGEO_HUD_FIX_VISIBLE_Y + row
    );
}

static inline uint16_t neogeo_hud_entry(
    const uint8_t *nametable,
    uint8_t index,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    uint8_t row;
    uint8_t column;
    uint8_t source_y;
    uint8_t tile;
    uint16_t attribute_index;
    uint8_t attribute;
    uint8_t shift;
    uint8_t palette_number;

    if (show_hud == 0u) {
        return NEOGEO_HUD_FIX_BLANK_TILE;
    }

    row = (uint8_t)(index >> 5);
    column = (uint8_t)(index & 31u);
    source_y = (uint8_t)(row + 1u);
    tile = nametable[(uint16_t)source_y * NEOGEO_HUD_COLUMNS + column];
    attribute_index =
        (uint16_t)((source_y >> 2) * 8u + (column >> 2));
    attribute = nametable[960u + attribute_index];
    shift = (uint8_t)(
        ((source_y & 3u) >> 1) * 4u +
        ((column & 3u) >> 1) * 2u
    );
    palette_number = (uint8_t)((attribute >> shift) & 3u);

    return (uint16_t)(
        ((uint16_t)palette_number << 12) +
        NEOGEO_HUD_FIX_TILE_BASE + pattern_base + tile
    );
}

static inline uint8_t neogeo_hud_rebuild_full(
    const uint8_t *nametable,
    uint16_t pattern_base,
    uint8_t show_hud,
    uint16_t *desired,
    const uint16_t *cached,
    uint8_t *changed_indices
) {
    uint8_t changed_count = 0u;
    uint8_t index;

    for (index = 0u; index < NEOGEO_HUD_ENTRY_COUNT; ++index) {
        uint16_t entry = neogeo_hud_entry(
            nametable,
            index,
            pattern_base,
            show_hud
        );

        desired[index] = entry;
        if (cached[index] != entry) {
            changed_indices[changed_count++] = index;
        }
    }
    return changed_count;
}

static inline uint8_t neogeo_hud_rebuild_dirty(
    const uint8_t *nametable,
    const uint32_t dirty_rows[NEOGEO_HUD_ROWS],
    uint16_t pattern_base,
    uint8_t show_hud,
    uint16_t *desired,
    const uint16_t *cached,
    uint8_t *changed_indices
) {
    uint8_t changed_count = 0u;
    uint8_t row;

    for (row = 0u; row < NEOGEO_HUD_ROWS; ++row) {
        uint32_t dirty = dirty_rows[row];
        uint8_t column = 0u;

        while (dirty != 0u) {
            if ((dirty & 1u) != 0u) {
                uint8_t index =
                    (uint8_t)(row * NEOGEO_HUD_COLUMNS + column);
                uint16_t entry = neogeo_hud_entry(
                    nametable,
                    index,
                    pattern_base,
                    show_hud
                );

                desired[index] = entry;
                if (cached[index] != entry) {
                    changed_indices[changed_count++] = index;
                }
            }
            dirty >>= 1;
            ++column;
        }
    }
    return changed_count;
}

/*
 * Rebuild the desired FIX state. A dirty-only pass is valid only when the
 * render configuration is unchanged, dirty tracking has already completed a
 * full synchronization, and no older upload queue is partially committed.
 * Every other case deliberately takes the byte-exact 96-cell reference path.
 */
static inline uint8_t neogeo_hud_build(
    NeoGeoHudState *state,
    const uint8_t *nametable,
    uint32_t source_generation,
    uint32_t dirty_rows[NEOGEO_HUD_ROWS],
    uint8_t *dirty_tracking_valid,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    uint16_t render_config =
        (uint16_t)(pattern_base | (show_hud != 0u ? 1u : 0u));
    uint8_t dirty_any = (uint8_t)(
        (dirty_rows[0] | dirty_rows[1] | dirty_rows[2]) != 0u
    );
    uint8_t dirty_rebuild_safe;

    if (
        *dirty_tracking_valid != 0u &&
        dirty_any == 0u &&
        state->built_generation == source_generation &&
        state->built_config == render_config
    ) {
        return 0u;
    }

    dirty_rebuild_safe = (uint8_t)(
        *dirty_tracking_valid != 0u &&
        dirty_any != 0u &&
        state->built_config == render_config &&
        state->upload_pending == 0u &&
        state->changed_count == 0u &&
        state->upload_cursor == 0u
    );
    if (dirty_rebuild_safe != 0u) {
        state->changed_count = neogeo_hud_rebuild_dirty(
            nametable,
            dirty_rows,
            pattern_base,
            show_hud,
            state->desired,
            state->cached,
            state->changed_indices
        );
    } else {
        state->changed_count = neogeo_hud_rebuild_full(
            nametable,
            pattern_base,
            show_hud,
            state->desired,
            state->cached,
            state->changed_indices
        );
    }

    state->built_generation = source_generation;
    state->built_config = render_config;
    state->upload_cursor = 0u;
    state->upload_pending = (uint8_t)(state->changed_count != 0u);
    dirty_rows[0] = 0u;
    dirty_rows[1] = 0u;
    dirty_rows[2] = 0u;
    *dirty_tracking_valid = 1u;
    return 1u;
}

#endif
