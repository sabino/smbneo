#ifndef SMB_NEOGEO_VIDEO_H
#define SMB_NEOGEO_VIDEO_H

#include <stdint.h>

/*
 * Initialize the cartridge FIX layer, palettes, and both hidden sprite sets.
 * No copyrighted graphics are linked into the program ROM; tile data comes
 * from C/S ROMs produced locally by tools/gen_neogeo_assets.py.
 */
void neogeo_video_init(void);

/*
 * Build the next frame into the hidden Neo Geo sprite set, wait for VBlank,
 * then reveal it.  The function reads the compact NES PPU state directly.
 */
void neogeo_video_render(void);

/* Map the Neo Geo player-one controls to the bit order expected by SMB. */
uint8_t neogeo_read_controller1(void);

/*
 * Lightweight cadence counters for emulator and hardware profiling.
 * A fully real-time build should complete one game frame per VBlank.
 */
extern volatile uint32_t neogeo_vblank_count;
extern volatile uint32_t neogeo_game_frame_count;

#endif
