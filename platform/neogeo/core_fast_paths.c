#include "core_fast_paths.h"

#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "data.h"

#include <stdint.h>

typedef enum {
    FAST_ENEMY_NONE = 0,
    FAST_ENEMY_GOOMBA,
    FAST_ENEMY_PIRANHA,
    FAST_ENEMY_LAKITU,
    FAST_ENEMY_SPINY,
} FastEnemyPolicy;

/*
 * EnemyGfxTableOffsets and EnemyAttributeData remain the shared canonical
 * descriptors. This sparse table adds only the behavior needed by the four
 * common semantic paths; zero always means use the translated fallback.
 */
static const uint8_t fast_enemy_policy[Spiny + 1u] = {
    [Goomba] = FAST_ENEMY_GOOMBA,
    [PiranhaPlant] = FAST_ENEMY_PIRANHA,
    [Lakitu] = FAST_ENEMY_LAKITU,
    [Spiny] = FAST_ENEMY_SPINY,
};

const uint8_t smb_core_enemy_column_actions[4] = {
    0u,
    HIDE_ENEMY_RIGHT_COLUMN,
    HIDE_ENEMY_LEFT_COLUMN,
    HIDE_ENEMY_RIGHT_COLUMN | HIDE_ENEMY_LEFT_COLUMN,
};

static uint8_t rom_byte(uint16_t address) {
    return data[address - 0x8000u];
}

static void draw_three_enemy_rows(
    uint8_t graphics_offset,
    uint8_t sprite_offset
) {
    uint8_t row;
    uint8_t source_y = ram[0x2];
    uint8_t source_x = ram[0x5];
    uint8_t attributes = ram[0x4];
    uint8_t flipped = (uint8_t)(ram[0x3] & 0x02u);

    for (row = 0; row < 3u; ++row) {
        uint8_t left_tile = rom_byte((uint16_t)(
            EnemyGraphicsTable + graphics_offset + row * 2u
        ));
        uint8_t right_tile = rom_byte((uint16_t)(
            EnemyGraphicsTable + graphics_offset + row * 2u + 1u
        ));
        uint8_t output_offset = (uint8_t)(sprite_offset + row * 8u);
        uint8_t output_attributes = attributes;

        ram[0x0] = left_tile;
        ram[0x1] = right_tile;
        if (flipped != 0u) {
            ram[Sprite_Tilenumber + output_offset] = right_tile;
            ram[Sprite_Tilenumber + output_offset + 4u] = left_tile;
            output_attributes |= 0x40u;
        } else {
            ram[Sprite_Tilenumber + output_offset] = left_tile;
            ram[Sprite_Tilenumber + output_offset + 4u] = right_tile;
        }
        ram[Sprite_Attributes + output_offset] = output_attributes;
        ram[Sprite_Attributes + output_offset + 4u] = output_attributes;
        ram[Sprite_Y_Position + output_offset] = source_y;
        ram[Sprite_Y_Position + output_offset + 4u] = source_y;
        ram[Sprite_X_Position + output_offset] = source_x;
        ram[Sprite_X_Position + output_offset + 4u] =
            (uint8_t)(source_x + 8u);
        source_y = (uint8_t)(source_y + 8u);
    }
    ram[0x2] = source_y;
}

static void mirror_three_rows(uint8_t sprite_offset, uint8_t egg) {
    uint8_t left_attributes =
        (uint8_t)(ram[Sprite_Attributes + sprite_offset] & 0xa3u);
    uint8_t right_attributes = (uint8_t)(left_attributes | 0x40u);
    uint8_t row;

    if (egg != 0u) {
        right_attributes |= 0x80u;
    }
    for (row = 0; row < 3u; ++row) {
        uint8_t output_offset = (uint8_t)(sprite_offset + row * 8u);

        ram[Sprite_Attributes + output_offset] = left_attributes;
        ram[Sprite_Attributes + output_offset + 4u] = right_attributes;
    }
}

static uint8_t animation_enabled(uint8_t slot) {
    return (uint8_t)(
        ram[EnemyIntervalTimer + slot] < 5u &&
        (ram[FrameCounter] & rom_byte(EnemyAnimTimingBMask)) == 0u
    );
}

bool smb_core_fast_enemy_gfx_handler(void) {
    uint8_t slot = x;
    uint8_t enemy_id;
    uint8_t enemy_state;
    uint8_t sprite_offset;
    uint8_t graphics_offset;
    FastEnemyPolicy policy;

    if (
        slot >= 6u || slot != ram[ObjectOffset] ||
        ram[BowserGfxFlag] != 0u || ram[TimerControl] != 0u
    ) {
        return false;
    }
    enemy_id = ram[Enemy_ID + slot];
    if (enemy_id > Spiny) {
        return false;
    }
    policy = (FastEnemyPolicy)fast_enemy_policy[enemy_id];
    if (policy == FAST_ENEMY_NONE) {
        return false;
    }
    enemy_state = ram[Enemy_State + slot];
    if (
        (policy != FAST_ENEMY_SPINY && enemy_state != 0u) ||
        (policy == FAST_ENEMY_SPINY &&
            enemy_state != 0u && enemy_state != 5u)
    ) {
        return false;
    }
    sprite_offset = ram[Enemy_SprDataOffset + slot];
    if ((sprite_offset & 3u) != 0u || sprite_offset > 232u) {
        return false;
    }

    ram[0x2] = ram[Enemy_Y_Position + slot];
    ram[0x5] = ram[Enemy_Rel_XPos];
    ram[0xeb] = sprite_offset;
    ram[VerticalFlipFlag] = 0u;
    ram[0x3] = ram[Enemy_MovingDir + slot];
    ram[0x4] = ram[Enemy_SprAttrib + slot];

    if (
        policy == FAST_ENEMY_PIRANHA &&
        (ram[PiranhaPlant_Y_Speed + slot] & 0x80u) == 0u &&
        ram[EnemyFrameTimer + slot] != 0u
    ) {
        a = enemy_id;
        y = ram[EnemyFrameTimer + slot];
        nz_value = y;
        carry_flag = true;
        return true;
    }

    ram[0xed] = enemy_state;
    ram[0xec] = (uint8_t)(enemy_state & 0x1fu);
    ram[0xef] = enemy_id;
    ram[0x4] |= rom_byte((uint16_t)(EnemyAttributeData + enemy_id));
    graphics_offset = rom_byte((uint16_t)(
        EnemyGfxTableOffsets + enemy_id
    ));

    switch (policy) {
    case FAST_ENEMY_GOOMBA:
        if ((ram[FrameCounter] & 0x08u) == 0u) {
            ram[0x3] ^= 0x03u;
        }
        break;
    case FAST_ENEMY_PIRANHA:
        if (animation_enabled(slot) != 0u) {
            graphics_offset = (uint8_t)(graphics_offset + 6u);
        }
        break;
    case FAST_ENEMY_LAKITU:
        if (ram[FrenzyEnemyTimer] < 0x10u) {
            graphics_offset = 0x96u;
        }
        break;
    case FAST_ENEMY_SPINY:
        if (enemy_state == 5u) {
            graphics_offset = 0x30u;
            ram[0x3] = 2u;
        }
        if (animation_enabled(slot) != 0u) {
            graphics_offset = (uint8_t)(graphics_offset + 6u);
        }
        break;
    case FAST_ENEMY_NONE:
        return false;
    }

    draw_three_enemy_rows(graphics_offset, sprite_offset);

    switch (policy) {
    case FAST_ENEMY_PIRANHA:
        mirror_three_rows(sprite_offset, 0u);
        break;
    case FAST_ENEMY_SPINY:
        if (enemy_state == 5u) {
            mirror_three_rows(sprite_offset, 1u);
        }
        break;
    case FAST_ENEMY_LAKITU:
        ram[Sprite_Attributes + sprite_offset + 16u] &= 0x81u;
        ram[Sprite_Attributes + sprite_offset + 20u] |= 0x41u;
        if (ram[FrenzyEnemyTimer] < 0x10u) {
            ram[Sprite_Attributes + sprite_offset + 12u] =
                ram[Sprite_Attributes + sprite_offset + 20u];
            ram[Sprite_Attributes + sprite_offset + 8u] = (uint8_t)(
                ram[Sprite_Attributes + sprite_offset + 20u] & 0x81u
            );
        }
        break;
    case FAST_ENEMY_GOOMBA:
    case FAST_ENEMY_NONE:
        break;
    }

    y = sprite_offset;
    if (!smb_core_fast_spr_object_offscr_known_safe(slot, sprite_offset)) {
        SprObjectOffscrChk();
    }
    return true;
}

bool smb_core_fast_move_normal_enemy(void) {
    uint8_t slot = x;
    uint8_t enemy_id;
    uint8_t enemy_state;
    uint8_t speed;

    /* Do not change emulated state until every dispatch guard has passed. */
    if (
        slot >= 6u || slot != ram[ObjectOffset] ||
        ram[TimerControl] != 0u
    ) {
        return false;
    }
    enemy_id = ram[Enemy_ID + slot];
    enemy_state = ram[Enemy_State + slot];
    if (
        (enemy_id != Goomba || enemy_state != 0u) &&
        (enemy_id != Spiny ||
            (enemy_state != 0u && enemy_state != 5u))
    ) {
        return false;
    }

    if (enemy_state == 5u) {
        MoveD_EnemyVertically();
    }

    /*
     * Both accepted states reach SteadM with Y selecting one of the two zero
     * entries in XSpeedAdderData. Preserve the observable 6502 stack write,
     * temporary speed store, nested movement call, and final PLA state while
     * avoiding the interpreted state tests and bytewise speed-table setup.
     */
    speed = ram[Enemy_X_Speed + slot];
    ram[0x100u + sp] = speed;
    --sp;
    ram[Enemy_X_Speed + slot] = speed;
    MoveEnemyHorizontally();
    ++sp;
    a = ram[0x100u + sp];
    nz_value = a;
    ram[Enemy_X_Speed + x] = a;
    return true;
}
