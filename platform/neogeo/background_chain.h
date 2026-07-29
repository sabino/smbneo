#ifndef SMB_NEOGEO_BACKGROUND_CHAIN_H
#define SMB_NEOGEO_BACKGROUND_CHAIN_H

#include <stdint.h>

typedef struct {
    uint8_t full_rebuild;
    uint8_t clear_root;
    uint8_t set_sticky;
} NeoGeoBackgroundChainPlan;

/*
 * Strip zero is always a chain root. A wrapped 33-strip ring has one extra
 * root at its physical origin; advancing that origin therefore changes at
 * most two SCB3 words. An invalid state requests a complete reconciliation
 * because a benchmark reset or BIOS handoff may leave stale hardware words.
 */
static inline NeoGeoBackgroundChainPlan neogeo_background_chain_plan(
    uint8_t previous_valid,
    uint8_t previous_origin,
    uint8_t next_origin
) {
    NeoGeoBackgroundChainPlan plan = {0u, 0xffu, 0xffu};

    if (previous_valid == 0u) {
        plan.full_rebuild = 1u;
        return plan;
    }
    if (previous_origin == next_origin) {
        return plan;
    }
    if (next_origin != 0u) {
        plan.clear_root = next_origin;
    }
    if (previous_origin != 0u) {
        plan.set_sticky = previous_origin;
    }
    return plan;
}

#endif
