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

#endif
