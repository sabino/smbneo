#include "palette_policy.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t requested;
    uint32_t word_index;

    /* Every possible requested color is overridden at palette RAM word 0. */
    for (requested = 0; requested <= UINT16_MAX; ++requested) {
        assert(
            neogeo_palette_ram_value(
                NEO_PALETTE_REFERENCE_WORD_INDEX,
                (uint16_t)requested
            ) == NEO_PALETTE_REFERENCE_BLACK
        );

        /* The separate backdrop register must never receive that override. */
        assert(
            neogeo_backdrop_value((uint16_t)requested) ==
            (uint16_t)requested
        );
    }

    /* Every other palette RAM index passes the requested word through. */
    for (word_index = 1; word_index <= UINT16_MAX; ++word_index) {
        uint16_t low_pattern = (uint16_t)(word_index * UINT32_C(257));
        uint16_t high_pattern = (uint16_t)~low_pattern;

        assert(
            neogeo_palette_ram_value(
                (uint16_t)word_index,
                low_pattern
            ) == low_pattern
        );
        assert(
            neogeo_palette_ram_value(
                (uint16_t)word_index,
                high_pattern
            ) == high_pattern
        );
    }

    puts("Neo Geo palette-policy tests: OK");
    return 0;
}
