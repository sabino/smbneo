#ifndef SMB_NEOGEO_PPU_RENDER_STATE_H
#define SMB_NEOGEO_PPU_RENDER_STATE_H

#include <stdint.h>

/*
 * Monotonic revisions maintained by the direct PPU backend. They let the
 * renderer skip unchanged nametable columns, HUD rows, and palettes without
 * keeping a second copy of NES video memory.
 */
extern uint32_t neogeo_ppu_column_generation[64];
extern uint32_t neogeo_ppu_background_full_generation;
extern uint32_t neogeo_ppu_background_hud_generation;
extern uint32_t neogeo_ppu_hud_generation;
extern uint32_t neogeo_ppu_palette_generation;

/*
 * Rows 1..3 of physical nametable zero are the stationary FIX-layer HUD.
 * Keep one exact bit per visible tile so the renderer can rebuild only the
 * cells touched since its previous completed build.  The validity byte is
 * deliberately separate: initialization and renderer-cache invalidation must
 * force one full reference scan before the bitset can be trusted again.
 */
extern uint32_t neogeo_ppu_hud_dirty_rows[3];
extern uint8_t neogeo_ppu_hud_dirty_tracking_valid;

static inline uint8_t neogeo_ppu_hud_dirty_any(void) {
    return (uint8_t)(
        (neogeo_ppu_hud_dirty_rows[0] |
            neogeo_ppu_hud_dirty_rows[1] |
            neogeo_ppu_hud_dirty_rows[2]) != 0u
    );
}

static inline void neogeo_ppu_hud_dirty_acknowledge(void) {
    neogeo_ppu_hud_dirty_rows[0] = 0u;
    neogeo_ppu_hud_dirty_rows[1] = 0u;
    neogeo_ppu_hud_dirty_rows[2] = 0u;
    neogeo_ppu_hud_dirty_tracking_valid = 1u;
}

static inline void neogeo_ppu_hud_dirty_invalidate(void) {
    neogeo_ppu_hud_dirty_tracking_valid = 0u;
}

#endif
