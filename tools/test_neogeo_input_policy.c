#include "input_policy.h"

#include <stdint.h>
#include <stdio.h>

static int expect_state(
    const char *name,
    uint8_t actual,
    uint8_t expected
) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected 0x%02x, got 0x%02x\n",
        name,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

int main(void) {
    int failed = 0;
    uint8_t pressed;

    for (pressed = 0u; pressed < 16u; ++pressed) {
        const uint8_t active_low_controls =
            (uint8_t)~(uint8_t)(pressed << 4u);
        const uint8_t expected = (uint8_t)(
            ((pressed & 0x03u) != 0u ? SMB_NES_BUTTON_A : 0u) |
            ((pressed & 0x0cu) != 0u ? SMB_NES_BUTTON_B : 0u)
        );

        failed |= expect_state(
            "A/B jump and C/D run button matrix",
            neogeo_input_map_action_buttons(active_low_controls),
            expected
        );
    }

    failed |= expect_state(
        "single directions unchanged",
        neogeo_input_normalize_directions(0x90),
        0x90
    );
    failed |= expect_state(
        "horizontal opposites neutralized",
        neogeo_input_normalize_directions(0xc3),
        0x03
    );
    failed |= expect_state(
        "vertical opposites neutralized",
        neogeo_input_normalize_directions(0x3c),
        0x0c
    );
    failed |= expect_state(
        "both opposite pairs preserve action bits",
        neogeo_input_normalize_directions(0xff),
        0x0f
    );

    if (failed != 0) {
        return 1;
    }
    puts("Neo Geo input-policy tests: OK");
    return 0;
}
