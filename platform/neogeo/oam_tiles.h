#ifndef SMB_NEOGEO_OAM_TILES_H
#define SMB_NEOGEO_OAM_TILES_H

#include <stdint.h>

/* C-ROM tiles 0..255 are reserved for the BIOS/ngdevkit eyecatcher. */
#define CROM_BLANK_TILE 256u
#define CROM_NES_TILE_BASE 257u
#define CROM_NES_TILE_BANK_SIZE 512u
#define CROM_NES_HFLIP_TILE_BASE \
    (CROM_NES_TILE_BASE + CROM_NES_TILE_BANK_SIZE)
#define CROM_NES_VFLIP_TILE_BASE \
    (CROM_NES_HFLIP_TILE_BASE + CROM_NES_TILE_BANK_SIZE)
#define CROM_NES_HVFLIP_TILE_BASE \
    (CROM_NES_VFLIP_TILE_BASE + CROM_NES_TILE_BANK_SIZE)

#define NEO_OAM_PALETTE_BASE 20u

/*
 * Source OAM bits 6 and 7 request horizontal and vertical mirroring. The
 * converted C-ROM contains one complete tile bank for each orientation, so
 * the renderer never depends on LSPC flip sampling while shrinking 16x16
 * source tiles to 8x8. This also keeps SCB1 bits 0 and 1 clear.
 */
static inline uint16_t neogeo_oam_tile_number(
    uint16_t pattern_base,
    uint8_t tile,
    uint8_t source_attributes
) {
    uint16_t orientation = (uint16_t)((source_attributes >> 6) & 3u);

    return (uint16_t)(
        CROM_NES_TILE_BASE +
        orientation * CROM_NES_TILE_BANK_SIZE +
        pattern_base +
        tile
    );
}

static inline uint16_t neogeo_oam_tile_attributes(uint8_t source_attributes) {
    return (uint16_t)(
        (NEO_OAM_PALETTE_BASE + (source_attributes & 3u)) << 8
    );
}

#endif
