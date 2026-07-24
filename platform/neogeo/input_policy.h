#ifndef SMB_NEOGEO_INPUT_POLICY_H
#define SMB_NEOGEO_INPUT_POLICY_H

#include <stdint.h>

#define SMB_NES_HORIZONTAL_DIRECTIONS UINT8_C(0xc0)
#define SMB_NES_VERTICAL_DIRECTIONS UINT8_C(0x30)

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
