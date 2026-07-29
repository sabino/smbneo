#ifndef SMB_NEOGEO_INPUT_POLICY_H
#define SMB_NEOGEO_INPUT_POLICY_H

#include <stdint.h>

#define SMB_NES_BUTTON_A UINT8_C(0x01)
#define SMB_NES_BUTTON_B UINT8_C(0x02)
#define SMB_NES_HORIZONTAL_DIRECTIONS UINT8_C(0xc0)
#define SMB_NES_VERTICAL_DIRECTIONS UINT8_C(0x30)

#define SMB_NEOGEO_BUTTON_A UINT8_C(0x10)
#define SMB_NEOGEO_BUTTON_B UINT8_C(0x20)
#define SMB_NEOGEO_BUTTON_C UINT8_C(0x40)
#define SMB_NEOGEO_BUTTON_D UINT8_C(0x80)
#define SMB_NEOGEO_JUMP_BUTTONS \
    (SMB_NEOGEO_BUTTON_A | SMB_NEOGEO_BUTTON_B)
#define SMB_NEOGEO_RUN_BUTTONS \
    (SMB_NEOGEO_BUTTON_C | SMB_NEOGEO_BUTTON_D)

/*
 * Neo Geo controller registers are active-low.  A/B both drive the source
 * game's A action (jump/swim), while C/D both drive its B action
 * (run/fire/grab/throw).
 */
static inline uint8_t neogeo_input_map_action_buttons(
    uint8_t active_low_controls
) {
    uint8_t state = 0u;

    if (
        (active_low_controls & SMB_NEOGEO_JUMP_BUTTONS) !=
        SMB_NEOGEO_JUMP_BUTTONS
    ) {
        state |= SMB_NES_BUTTON_A;
    }
    if (
        (active_low_controls & SMB_NEOGEO_RUN_BUTTONS) !=
        SMB_NEOGEO_RUN_BUTTONS
    ) {
        state |= SMB_NES_BUTTON_B;
    }
    return state;
}

/*
 * A physical Neo Geo stick cannot assert opposite directions, but keyboard
 * mappings and some emulator frontends can. Treat either impossible pair as
 * neutral so it cannot leak an invalid direction state into game logic.
 */
static inline uint8_t neogeo_input_normalize_directions(uint8_t state) {
    if (
        (state & SMB_NES_HORIZONTAL_DIRECTIONS) ==
        SMB_NES_HORIZONTAL_DIRECTIONS
    ) {
        state &= (uint8_t)~SMB_NES_HORIZONTAL_DIRECTIONS;
    }
    if (
        (state & SMB_NES_VERTICAL_DIRECTIONS) ==
        SMB_NES_VERTICAL_DIRECTIONS
    ) {
        state &= (uint8_t)~SMB_NES_VERTICAL_DIRECTIONS;
    }
    return state;
}

#endif
