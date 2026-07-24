#include "oam_visibility.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t reference_visible(
    int16_t output_y,
    uint8_t source_x,
    uint8_t draw_left_edge
) {
    return (uint8_t)(
        output_y < 224 &&
        (int32_t)output_y + 8 > 0 &&
        (draw_left_edge != 0u || source_x != 0u)
    );
}

int main(void) {
    int32_t y;
    uint16_t x;
    uint8_t draw_left_edge;
    uint8_t admitted = 0;

    for (y = INT16_MIN; y <= INT16_MAX; ++y) {
        for (x = 0; x <= UINT8_MAX; ++x) {
            for (draw_left_edge = 0; draw_left_edge <= 1u; ++draw_left_edge) {
                assert(
                    neogeo_oam_entry_visible(
                        (int16_t)y,
                        (uint8_t)x,
                        draw_left_edge
                    ) ==
                    reference_visible(
                        (int16_t)y,
                        (uint8_t)x,
                        draw_left_edge
                    )
                );
            }
        }
    }

    /*
     * Visibility is independent for every OAM entry: sixty-four overlapping
     * in-range entries all remain admitted.
     */
    for (x = 0; x < 64u; ++x) {
        admitted = (uint8_t)(
            admitted + neogeo_oam_entry_visible(100, 80u, 1u)
        );
    }
    assert(admitted == 64u);
    assert(neogeo_oam_entry_visible(-8, 80u, 1u) == 0u);
    assert(neogeo_oam_entry_visible(-7, 80u, 1u) != 0u);
    assert(neogeo_oam_entry_visible(223, 80u, 1u) != 0u);
    assert(neogeo_oam_entry_visible(224, 80u, 1u) == 0u);
    assert(neogeo_oam_entry_visible(100, 0u, 0u) == 0u);
    assert(neogeo_oam_entry_visible(100, 0u, 1u) != 0u);

    puts("Neo Geo OAM visibility tests: OK");
    return 0;
}
