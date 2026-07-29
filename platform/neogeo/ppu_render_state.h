#ifndef SMB_NEOGEO_PPU_RENDER_STATE_H
#define SMB_NEOGEO_PPU_RENDER_STATE_H

#include <stdint.h>

/* Revisions maintained for the independently cached FIX HUD and palettes. */
extern uint32_t neogeo_ppu_hud_generation;
extern uint32_t neogeo_ppu_palette_generation;

#define NEOGEO_PPU_BACKGROUND_RENDER_BANKS 2u
#define NEOGEO_PPU_BACKGROUND_DIRTY_BYTES 8u

/*
 * Nametable columns pending in each hidden renderer bank. Full-screen and
 * HUD-split backgrounds cover different NES rows, so their pending sets stay
 * independent. Each write marks both banks; rendering consumes only the bank
 * and mode it just rebuilt.
 */
extern uint8_t neogeo_ppu_background_full_dirty_columns
    [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
    [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES];
extern uint8_t neogeo_ppu_background_hud_dirty_columns
    [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
    [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES];

static inline uint8_t *neogeo_ppu_background_dirty_columns(
    uint8_t render_bank,
    uint8_t show_hud
) {
    return show_hud != 0u
        ? neogeo_ppu_background_hud_dirty_columns[render_bank]
        : neogeo_ppu_background_full_dirty_columns[render_bank];
}

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
