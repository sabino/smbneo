#include "cpu.h"
#include "ppu.h"
#include "ppu_render_state.h"
#include "video.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t ram[RAM_SIZE];

void neogeo_video_render(void) {
}

static void assert_column_range(
    uint8_t first,
    uint8_t count,
    uint32_t expected
) {
    uint8_t column;

    for (column = first; column < (uint8_t)(first + count); ++column) {
        assert(neogeo_ppu_column_generation[column] == expected);
    }
}

int main(void) {
    ppu_init(0);

    assert_column_range(0, 64, 0);
    assert(neogeo_ppu_hud_generation == 0);
    assert(neogeo_ppu_palette_generation == 0);

    /* Identical writes must not invalidate any renderer cache. */
    ppu_write(0x2000u, 0);
    assert(neogeo_ppu_column_generation[0] == 0);

    ppu_write(0x2000u, 1);
    assert(neogeo_ppu_column_generation[0] == 1);
    assert(neogeo_ppu_hud_generation == 0);
    ppu_write(0x2000u, 1);
    assert(neogeo_ppu_column_generation[0] == 1);

    /* $2800 vertically mirrors the first physical nametable. */
    ppu_write(0x2800u, 2);
    assert(neogeo_ppu_column_generation[0] == 2);
    assert(ppu_read(0x2000u) == 2);

    ppu_write(0x2405u, 3);
    assert(neogeo_ppu_column_generation[37] == 1);

    /* Only visible status-bar rows 1..3 invalidate the FIX HUD. */
    ppu_write(0x2020u, 4);
    assert(neogeo_ppu_column_generation[0] == 3);
    assert(neogeo_ppu_hud_generation == 1);
    ppu_write(0x2080u, 5);
    assert(neogeo_ppu_column_generation[0] == 4);
    assert(neogeo_ppu_hud_generation == 1);

    /* One attribute byte affects four adjacent tile columns. */
    ppu_write(0x23c0u, 6);
    assert(neogeo_ppu_column_generation[0] == 5);
    assert_column_range(1, 3, 1);
    assert(neogeo_ppu_hud_generation == 2);

    ppu_write(0x27c7u, 7);
    assert_column_range(60, 4, 1);
    assert(neogeo_ppu_hud_generation == 2);

    ppu_write(0x3f00u, 0);
    assert(neogeo_ppu_palette_generation == 0);
    ppu_write(0x3f00u, 0x21u);
    assert(neogeo_ppu_palette_generation == 1);
    ppu_write(0x3f00u, 0x61u);
    assert(neogeo_ppu_palette_generation == 1);

    /* $3f10 aliases universal background color at $3f00. */
    ppu_write(0x3f10u, 0x22u);
    assert(neogeo_ppu_palette_generation == 2);
    assert(ppu_read(0x3f00u) == 0x22u);

    ppu_init(0);
    assert_column_range(0, 64, 0);
    assert(neogeo_ppu_hud_generation == 0);
    assert(neogeo_ppu_palette_generation == 0);

    puts("Neo Geo PPU render-state tests: OK");
    return 0;
}
