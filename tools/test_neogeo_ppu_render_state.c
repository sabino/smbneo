#include "cpu.h"
#include "ppu.h"
#include "ppu_render_state.h"
#include "title_data.h"
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

    /* The only CPU-readable CHR window is the generated title payload. */
    assert(ppu_read(TITLE_SCREEN_CHR_OFFSET - 1u) == 0);
    assert(ppu_read(TITLE_SCREEN_CHR_OFFSET) == 0xa5u);
    assert(ppu_read(TITLE_SCREEN_CHR_OFFSET + 1u) == 0);
    assert(
        ppu_read(
            TITLE_SCREEN_CHR_OFFSET + TITLE_SCREEN_CHR_SIZE - 1u
        ) == 0x5au
    );
    assert(
        ppu_read(TITLE_SCREEN_CHR_OFFSET + TITLE_SCREEN_CHR_SIZE) == 0
    );

    /* PPU_DATA retains the original buffered-read behavior for this window. */
    ppu_write_address((uint8_t)(TITLE_SCREEN_CHR_OFFSET >> 8));
    ppu_write_address((uint8_t)TITLE_SCREEN_CHR_OFFSET);
    assert(ppu_read_register(0x2007u) == 0);
    assert(ppu_read_register(0x2007u) == 0xa5u);

    assert_column_range(0, 64, 0);
    assert(neogeo_ppu_hud_generation == 0);
    assert(neogeo_ppu_palette_generation == 0);
    assert(neogeo_ppu_hud_dirty_rows[0] == 0u);
    assert(neogeo_ppu_hud_dirty_rows[1] == 0u);
    assert(neogeo_ppu_hud_dirty_rows[2] == 0u);
    assert(neogeo_ppu_hud_dirty_tracking_valid == 0u);

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

    /* All vertical and $3000 aliases feed the same three-row HUD bitset. */
    ppu_write(0x2020u, 1u);
    ppu_write(0x2821u, 1u);
    ppu_write(0x3022u, 1u);
    assert(neogeo_ppu_hud_dirty_rows[0] == UINT32_C(0x00000007));
    assert(neogeo_ppu_hud_dirty_rows[1] == 0u);
    assert(neogeo_ppu_hud_dirty_rows[2] == 0u);
    assert(neogeo_ppu_hud_generation == 3u);

    /* A top attribute byte can alter all three rows across four columns. */
    ppu_write(0x23c7u, 0xffu);
    assert(neogeo_ppu_hud_dirty_rows[0] == UINT32_C(0xf0000007));
    assert(neogeo_ppu_hud_dirty_rows[1] == UINT32_C(0xf0000000));
    assert(neogeo_ppu_hud_dirty_rows[2] == UINT32_C(0xf0000000));
    assert(neogeo_ppu_hud_generation == 4u);

    neogeo_ppu_hud_dirty_acknowledge();
    assert(neogeo_ppu_hud_dirty_any() == 0u);
    assert(neogeo_ppu_hud_dirty_tracking_valid == 1u);

    puts("Neo Geo PPU render-state tests: OK");
    return 0;
}
