#ifndef SMB_NEOGEO_BACKGROUND_UPDATE_PLAN_H
#define SMB_NEOGEO_BACKGROUND_UPDATE_PLAN_H

#include <stdint.h>

#define NEOGEO_BACKGROUND_WORLD_COLUMNS 64u
#define NEOGEO_BACKGROUND_VISIBLE_STRIPS 33u
#define NEOGEO_BACKGROUND_DIRTY_BYTES 8u

typedef struct {
    uint8_t count;
    uint8_t logical_strips[NEOGEO_BACKGROUND_VISIBLE_STRIPS];
} NeoGeoBackgroundUpdatePlan;

/*
 * Build the exact set of logical strips whose hidden-bank SCB1 data may be
 * stale.  A forward ring movement retains the first (33 - delta) strips and
 * introduces the remainder at the right edge.  Nametable writes contribute
 * only retained columns; dirty entering columns are already covered by the
 * ring movement.  Large/backward jumps and cache/config invalidation rebuild
 * every strip.
 *
 * Dirty bytes are consumed after their final nametable values have been
 * planned.  Each physical renderer bank owns its own bitmap, so consuming one
 * bank can never hide an update from the other bank.
 */
static inline void
neogeo_background_plan_updates(
    uint8_t first_column,
    uint8_t forward_delta,
    uint8_t force_full_rebuild,
    uint8_t dirty_columns[NEOGEO_BACKGROUND_DIRTY_BYTES],
    NeoGeoBackgroundUpdatePlan *plan
) {
    uint8_t retained_strips;
    uint8_t byte_index;
    uint8_t logical_strip;

    plan->count = 0u;
    if (
        force_full_rebuild != 0u ||
        forward_delta >= NEOGEO_BACKGROUND_VISIBLE_STRIPS
    ) {
        for (
            logical_strip = 0u;
            logical_strip < NEOGEO_BACKGROUND_VISIBLE_STRIPS;
            ++logical_strip
        ) {
            plan->logical_strips[plan->count++] = logical_strip;
        }
        for (
            byte_index = 0u;
            byte_index < NEOGEO_BACKGROUND_DIRTY_BYTES;
            ++byte_index
        ) {
            dirty_columns[byte_index] = 0u;
        }
        return;
    }

    retained_strips = (uint8_t)(
        NEOGEO_BACKGROUND_VISIBLE_STRIPS - forward_delta
    );
    for (
        byte_index = 0u;
        byte_index < NEOGEO_BACKGROUND_DIRTY_BYTES;
        ++byte_index
    ) {
        uint8_t bits = dirty_columns[byte_index];
        uint8_t bit_index;

        dirty_columns[byte_index] = 0u;
        if (bits == 0u) {
            continue;
        }
        for (bit_index = 0u; bit_index < 8u; ++bit_index) {
            uint8_t world_column;

            if ((bits & ((uint8_t)1u << bit_index)) == 0u) {
                continue;
            }
            world_column = (uint8_t)(byte_index * 8u + bit_index);
            logical_strip = (uint8_t)(
                (world_column - first_column) &
                (NEOGEO_BACKGROUND_WORLD_COLUMNS - 1u)
            );
            if (logical_strip < retained_strips) {
                plan->logical_strips[plan->count++] = logical_strip;
            }
        }
    }

    for (
        logical_strip = retained_strips;
        logical_strip < NEOGEO_BACKGROUND_VISIBLE_STRIPS;
        ++logical_strip
    ) {
        plan->logical_strips[plan->count++] = logical_strip;
    }
}

#endif
