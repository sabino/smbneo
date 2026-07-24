#ifndef SMB_NEOGEO_OAM_VISIBILITY_H
#define SMB_NEOGEO_OAM_VISIBILITY_H

#include <stdint.h>

#define NEO_OAM_SCREEN_HEIGHT 224
#define NEO_OAM_TILE_HEIGHT 8

static inline uint8_t neogeo_oam_entry_visible(
    int16_t output_y,
    uint8_t source_x,
    uint8_t draw_left_edge
) {
    return (uint8_t)(
        output_y < NEO_OAM_SCREEN_HEIGHT &&
        output_y + NEO_OAM_TILE_HEIGHT > 0 &&
        (draw_left_edge != 0u || source_x != 0u)
    );
}

#endif
