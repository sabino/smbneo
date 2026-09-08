#include "oam_tiles.h"
#include "hud_update.h"
#include <assert.h>
#include <stdio.h>
#include "test_check.h"

int main(void) {
    assert(CROM_NES_TILE_BANK_SIZE == 1024);
    assert(NEOGEO_HUD_FIX_BLANK_TILE == 1025);
    for (unsigned tile = 0; tile < 1024; ++tile) {
        for (unsigned attr = 0; attr < 256; ++attr) {
            unsigned pattern_base = tile & ~255u;
            unsigned expected = 257 + (attr >> 6) * 1024 + tile;
            assert(neogeo_oam_tile_number(pattern_base, (uint8_t)tile, (uint8_t)attr) == expected);
            assert((neogeo_oam_tile_attributes((uint8_t)attr) & 3) == 0);
            assert(expected < 0x200000 / 64);
        }
    }
    puts("VS tiles: both CHR banks, four baked orientations, no hardware flip bits passed");
}
