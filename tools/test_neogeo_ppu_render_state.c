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
static void assert_background_dirty_mask(
    uint8_t dirty
        [NEOGEO_PPU_BACKGROUND_RENDER_BANKS]
        [NEOGEO_PPU_BACKGROUND_DIRTY_BYTES],
    uint8_t first_column,
    uint8_t count,
    uint8_t expected_dirty
) {
    uint8_t bank;
    uint8_t column;

    for (
        bank = 0u;
        bank < NEOGEO_PPU_BACKGROUND_RENDER_BANKS;
        ++bank
    ) {
        for (column = 0u; column < 64u; ++column) {
            uint8_t expected = (uint8_t)(
                expected_dirty != 0u &&
                column >= first_column &&
                column < (uint8_t)(first_column + count)
            );
            uint8_t observed = (uint8_t)(
                (dirty[bank][column >> 3] >> (column & 7u)) & 1u
            );

            assert(observed == expected);
        }
    }
}

static void exhaustive_background_dirty_rows(void) {
    uint8_t table;
    uint8_t row;
    uint8_t column;

    for (table = 0u; table < 2u; ++table) {
        for (row = 0u; row < 30u; ++row) {
            for (column = 0u; column < 32u; ++column) {
                uint8_t world_column = (uint8_t)(table * 32u + column);
                uint16_t address = (uint16_t)(
                    0x2000u + (uint16_t)table * 0x0400u +
                    (uint16_t)row * 32u + column
                );

                ppu_init(0);
                ppu_write(address, 1u);
                assert_background_dirty_mask(
                    neogeo_ppu_background_full_dirty_columns,
                    world_column,
                    1u,
                    (uint8_t)(row >= 1u && row <= 28u)
                );
                assert_background_dirty_mask(
                    neogeo_ppu_background_hud_dirty_columns,
                    world_column,
                    1u,
                    (uint8_t)(row >= 4u && row <= 29u)
                );
            }
        }
    }
}

static void exhaustive_background_dirty_attributes(void) {
    uint8_t table;
    uint8_t attribute_row;
    uint8_t attribute_column;

    for (table = 0u; table < 2u; ++table) {
        for (attribute_row = 0u; attribute_row < 8u; ++attribute_row) {
            for (
                attribute_column = 0u;
                attribute_column < 8u;
                ++attribute_column
            ) {
                uint8_t first_column = (uint8_t)(
                    table * 32u + attribute_column * 4u
                );
                uint16_t address = (uint16_t)(
                    0x23c0u + (uint16_t)table * 0x0400u +
                    (uint16_t)attribute_row * 8u + attribute_column
                );

                ppu_init(0);
                ppu_write(address, 1u);
                assert_background_dirty_mask(
                    neogeo_ppu_background_full_dirty_columns,
                    first_column,
                    4u,
                    1u
                );
                assert_background_dirty_mask(
                    neogeo_ppu_background_hud_dirty_columns,
                    first_column,
                    4u,
                    (uint8_t)(attribute_row != 0u)
                );
            }
        }
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

    assert(neogeo_ppu_hud_generation == 0);
    assert(neogeo_ppu_palette_generation == 0);
    assert(neogeo_ppu_hud_dirty_rows[0] == 0u);
    assert(neogeo_ppu_hud_dirty_rows[1] == 0u);
    assert(neogeo_ppu_hud_dirty_rows[2] == 0u);
    assert(neogeo_ppu_hud_dirty_tracking_valid == 0u);
    assert_background_dirty_mask(
        neogeo_ppu_background_full_dirty_columns,
        0u,
        0u,
        0u
    );
    assert_background_dirty_mask(
        neogeo_ppu_background_hud_dirty_columns,
        0u,
        0u,
        0u
    );

    /* Identical writes must not invalidate any renderer cache. */
    ppu_write(0x2000u, 0);
    assert(ppu_read(0x2000u) == 0u);

    ppu_write(0x2000u, 1);
    assert(ppu_read(0x2000u) == 1u);
    assert(neogeo_ppu_hud_generation == 0);
    ppu_write(0x2000u, 1);
    assert(ppu_read(0x2000u) == 1u);

    /* $2800 vertically mirrors the first physical nametable. */
    ppu_write(0x2800u, 2);
    assert(ppu_read(0x2000u) == 2);

    ppu_write(0x2405u, 3);
    assert(ppu_read(0x2405u) == 3u);

    /* Only visible status-bar rows 1..3 invalidate the FIX HUD. */
    ppu_write(0x2020u, 4);
    assert(neogeo_ppu_hud_generation == 1);
    ppu_write(0x2080u, 5);
    assert(neogeo_ppu_hud_generation == 1);

    /* One attribute byte affects four adjacent tile columns. */
    ppu_write(0x23c0u, 6);
    assert(neogeo_ppu_hud_generation == 2);

    ppu_write(0x27c7u, 7);
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

    exhaustive_background_dirty_rows();
    exhaustive_background_dirty_attributes();

    puts("Neo Geo PPU render-state tests: OK");
    return 0;
}
