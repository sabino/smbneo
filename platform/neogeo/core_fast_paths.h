#ifndef SMB_NEOGEO_CORE_FAST_PATHS_H
#define SMB_NEOGEO_CORE_FAST_PATHS_H

#include "constants.h"
#include "cpu.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(SMB_NEOGEO_FAST_CORE)
#define SMB_CORE_FAST_INLINE static __attribute__((always_inline)) inline
#else
#define SMB_CORE_FAST_INLINE static inline
#endif

enum {
    HIDE_ENEMY_RIGHT_COLUMN = 0x01u,
    HIDE_ENEMY_LEFT_COLUMN = 0x02u,
};

/* Indexed by Enemy_OffscreenBits bits 2-3, in original call order. */
extern const uint8_t smb_core_enemy_column_actions[4];

/*
 * Return true only when a semantic direct-C implementation completed the
 * translated routine. Returning false leaves the generated 6502-equivalent
 * body as the exact fallback.
 */
bool smb_core_fast_enemy_gfx_handler(void);
bool smb_core_fast_move_normal_enemy(void);

SMB_CORE_FAST_INLINE bool smb_core_fast_spr_object_offscr_known_safe(
    uint8_t slot,
    uint8_t sprite_offset
) {
    const uint8_t offscreen = ram[Enemy_OffscreenBits];
    uint8_t action;

    /* Row bits call other helpers and bit 7 may erase the enemy. */
    if ((offscreen & 0xe0u) != 0u) {
        return false;
    }

    action = smb_core_enemy_column_actions[(offscreen >> 2) & 0x03u];
    x = slot;
    if ((action & HIDE_ENEMY_RIGHT_COLUMN) != 0u) {
        const uint8_t right = (uint8_t)(sprite_offset + 4u);

        ram[Sprite_Y_Position + right] = 0xf8u;
        ram[Sprite_Y_Position + right + 8u] = 0xf8u;
        ram[Sprite_Y_Position + right + 16u] = 0xf8u;
        y = right;
    }
    if ((action & HIDE_ENEMY_LEFT_COLUMN) != 0u) {
        ram[Sprite_Y_Position + sprite_offset] = 0xf8u;
        ram[Sprite_Y_Position + sprite_offset + 8u] = 0xf8u;
        ram[Sprite_Y_Position + sprite_offset + 16u] = 0xf8u;
        y = sprite_offset;
    }

    /* Four PHA/PLA stages reuse this byte; the final pushed value is zero. */
    ram[0x100u + sp] = 0u;
    a = 0u;
    carry_flag = false;
    nz_value = 0u;
    return true;
}

SMB_CORE_FAST_INLINE bool smb_core_fast_spr_object_offscr_chk(void) {
    const uint8_t slot = ram[ObjectOffset];
    uint8_t sprite_offset;

    /*
     * Keep invalid enemy slots and wrapped/unaligned OAM layouts on the
     * canonical translated implementation. The shared semantic leaf performs
     * the independent high-row mask guard before making any machine write.
     */
    if (slot >= 6u) {
        return false;
    }
    sprite_offset = ram[Enemy_SprDataOffset + slot];
    if ((sprite_offset & 3u) != 0u || sprite_offset > 232u) {
        return false;
    }
    return smb_core_fast_spr_object_offscr_known_safe(slot, sprite_offset);
}

#undef SMB_CORE_FAST_INLINE

#endif
