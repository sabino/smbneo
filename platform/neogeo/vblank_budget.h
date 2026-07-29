#ifndef SMB_NEOGEO_VBLANK_BUDGET_H
#define SMB_NEOGEO_VBLANK_BUDGET_H

#include <stdint.h>

/*
 * Keep each visible-state commit below a conservative 20,000-cycle 68000
 * VBlank accounting ceiling. These are policy costs, not exact linked-code
 * timings: they include loop, address, and LSPC pacing overhead.
 */
#define NEO_VBLANK_PHASE_BUDGET_CYCLES UINT32_C(20000)
#define NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES UINT32_C(1024)
#define NEO_VBLANK_PHASE_FIXED_CYCLES UINT32_C(1024)
#define NEO_VBLANK_PALETTE_WORD_CYCLES UINT32_C(64)
#define NEO_VBLANK_HUD_ENTRY_CYCLES UINT32_C(128)
#define NEO_VBLANK_SCB_OPERATION_CYCLES UINT32_C(128)

/* 12 four-color groups, two FIX-border words, and one backdrop word. */
#define NEO_VBLANK_PALETTE_MAX_WORDS UINT16_C(51)

/* Commit at most one 32-entry FIX row per live-update phase. */
#define NEO_VBLANK_HUD_MAX_ENTRIES UINT16_C(32)

/* A small FIX delta may share the otherwise atomic SCB3 phase. */
#define NEO_VBLANK_SHARED_HUD_MAX_ENTRIES UINT16_C(8)

/* A small palette delta may also share the worst-case SCB3 phase. */
#define NEO_VBLANK_SHARED_PALETTE_MAX_WORDS UINT16_C(16)

/* Two 64-object SCB3 banks plus two old and two new background drivers. */
#define NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS UINT16_C(132)

typedef enum {
    NEO_VBLANK_COMMIT_NONE = 0,
    NEO_VBLANK_COMMIT_LIVE_UPDATES = 1,
    NEO_VBLANK_COMMIT_SCB_SWAP = 2,
    NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB = 3
} NeoVblankCommitPhase;

static inline uint16_t neogeo_vblank_clamp_chunk(
    uint16_t pending,
    uint16_t maximum
) {
    return pending < maximum ? pending : maximum;
}

static inline uint16_t neogeo_vblank_palette_chunk(uint16_t pending) {
    return neogeo_vblank_clamp_chunk(
        pending,
        NEO_VBLANK_PALETTE_MAX_WORDS
    );
}

static inline uint16_t neogeo_vblank_hud_chunk(uint16_t pending) {
    return neogeo_vblank_clamp_chunk(pending, NEO_VBLANK_HUD_MAX_ENTRIES);
}

static inline uint16_t neogeo_vblank_scb_swap_chunk(uint16_t pending) {
    return neogeo_vblank_clamp_chunk(
        pending,
        NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS
    );
}

static inline uint32_t neogeo_vblank_shared_live_phase_cycles(
    uint16_t palette_words,
    uint16_t hud_entries,
    uint16_t scb_operations
) {
    return NEO_VBLANK_PHASE_FIXED_CYCLES +
        (uint32_t)neogeo_vblank_clamp_chunk(
            palette_words,
            NEO_VBLANK_SHARED_PALETTE_MAX_WORDS
        ) * NEO_VBLANK_PALETTE_WORD_CYCLES +
        (uint32_t)neogeo_vblank_clamp_chunk(
            hud_entries,
            NEO_VBLANK_SHARED_HUD_MAX_ENTRIES
        ) * NEO_VBLANK_HUD_ENTRY_CYCLES +
        (uint32_t)neogeo_vblank_scb_swap_chunk(scb_operations) *
            NEO_VBLANK_SCB_OPERATION_CYCLES;
}

static inline uint8_t neogeo_vblank_shared_live_fits(
    uint16_t palette_words,
    uint16_t hud_entries
) {
    if (
        palette_words > NEO_VBLANK_SHARED_PALETTE_MAX_WORDS ||
        hud_entries > NEO_VBLANK_SHARED_HUD_MAX_ENTRIES
    ) {
        return 0u;
    }
    return (uint8_t)(
        neogeo_vblank_shared_live_phase_cycles(
            palette_words,
            hud_entries,
            NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS
        ) <=
            NEO_VBLANK_PHASE_BUDGET_CYCLES -
            NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES
    );
}

/*
 * A double-buffer reveal is atomic. Small measured palette/FIX deltas may
 * share it only when the worst-case SCB3 accounting still stays below the
 * ceiling with an explicit modeled safety margin. Larger live updates use
 * separate VBlanks first.
 */
static inline NeoVblankCommitPhase neogeo_vblank_choose_commit_phase(
    uint8_t scb_swap_pending,
    uint16_t palette_words,
    uint16_t hud_entries
) {
    if (scb_swap_pending != 0u) {
        if (
            (palette_words != 0u || hud_entries != 0u) &&
            neogeo_vblank_shared_live_fits(
                palette_words,
                hud_entries
            ) != 0u
        ) {
            return NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB;
        }
        return NEO_VBLANK_COMMIT_SCB_SWAP;
    }
    if (palette_words != 0u || hud_entries != 0u) {
        return NEO_VBLANK_COMMIT_LIVE_UPDATES;
    }
    return NEO_VBLANK_COMMIT_NONE;
}

static inline uint32_t neogeo_vblank_live_phase_cycles(
    uint16_t palette_words,
    uint16_t hud_entries
) {
    return NEO_VBLANK_PHASE_FIXED_CYCLES +
        (uint32_t)neogeo_vblank_palette_chunk(palette_words) *
            NEO_VBLANK_PALETTE_WORD_CYCLES +
        (uint32_t)neogeo_vblank_hud_chunk(hud_entries) *
            NEO_VBLANK_HUD_ENTRY_CYCLES;
}

static inline uint32_t neogeo_vblank_scb_phase_cycles(
    uint16_t operations
) {
    return NEO_VBLANK_PHASE_FIXED_CYCLES +
        (uint32_t)neogeo_vblank_scb_swap_chunk(operations) *
            NEO_VBLANK_SCB_OPERATION_CYCLES;
}

#endif
