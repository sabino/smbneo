#include "oam_tiles.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_orientation_banks(void) {
    static const uint16_t expected_bases[4] = {
        CROM_NES_TILE_BASE,
        CROM_NES_HFLIP_TILE_BASE,
        CROM_NES_VFLIP_TILE_BASE,
        CROM_NES_HVFLIP_TILE_BASE,
    };
    uint16_t orientation;

    for (orientation = 0; orientation < 4u; ++orientation) {
        uint8_t source_attributes = (uint8_t)(orientation << 6);

        assert(
            neogeo_oam_tile_number(0u, 0u, source_attributes) ==
            expected_bases[orientation]
        );
        assert(
            neogeo_oam_tile_number(256u, 255u, source_attributes) ==
            expected_bases[orientation] + 511u
        );
    }
}

static void test_palette_attributes_never_enable_hardware_flip(void) {
    uint16_t orientation;
    uint16_t palette;

    for (orientation = 0; orientation < 4u; ++orientation) {
        for (palette = 0; palette < 4u; ++palette) {
            uint8_t source_attributes =
                (uint8_t)((orientation << 6) | palette);
            uint16_t attributes =
                neogeo_oam_tile_attributes(source_attributes);

            assert(attributes == (uint16_t)((NEO_OAM_PALETTE_BASE + palette) << 8));
            assert((attributes & 0x0003u) == 0u);
        }
    }
}

int main(void) {
    test_orientation_banks();
    test_palette_attributes_never_enable_hardware_flip();
    puts("Neo Geo OAM tile tests passed");
    return 0;
}
