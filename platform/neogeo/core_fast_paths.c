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

typedef enum {
    FAST_OFFSCREEN_NONE = 0,
    FAST_OFFSCREEN_LEFT_CARRY = 1u << 0,
    FAST_OFFSCREEN_LEFT_ADD_38 = 1u << 1,
    FAST_OFFSCREEN_RIGHT_IMMUNE = 1u << 2,
    FAST_OFFSCREEN_STATE_5 = 1u << 3,
    FAST_OFFSCREEN_PLAIN = 1u << 4,
    FAST_OFFSCREEN_ANY_STATE = 1u << 5,
} FastOffscreenPolicy;

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

/*
 * These bits encode the ID-dependent flag flow in OffscreenBoundsCheck.
 * In particular, LEFT_CARRY is the carry produced by CPY #PiranhaPlant;
 * it is deliberately kept separate from the optional HammerBro/Piranha
 * margin adjustment because the original routine chains that carry through
 * all four boundary bytes.
 */
static const uint8_t fast_offscreen_policy[0x2bu] = {
    [Goomba] = FAST_OFFSCREEN_PLAIN,
    [PiranhaPlant] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_LEFT_ADD_38 |
        FAST_OFFSCREEN_RIGHT_IMMUNE,
    [Lakitu] = FAST_OFFSCREEN_LEFT_CARRY,
    [Spiny] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5,
    [0x24u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x25u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x26u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x27u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x28u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x29u] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
    [0x2au] =
        FAST_OFFSCREEN_LEFT_CARRY |
        FAST_OFFSCREEN_STATE_5 |
        FAST_OFFSCREEN_PLAIN |
        FAST_OFFSCREEN_ANY_STATE,
};

static void fast_erase_enemy_object(uint8_t slot) {
    a = 0u;
    nz_value = 0u;
    ram[Enemy_Flag + slot] = 0u;
    ram[Enemy_ID + slot] = 0u;
    ram[Enemy_State + slot] = 0u;
    ram[FloateyNum_Control + slot] = 0u;
    ram[EnemyIntervalTimer + slot] = 0u;
    ram[ShellChainCounter + slot] = 0u;
    ram[Enemy_SprAttrib + slot] = 0u;
    ram[EnemyFrameTimer + slot] = 0u;
}

typedef enum {
    FAST_ENEMY_BG_NONE = 0,
    FAST_ENEMY_BG_EARLY_EXIT,
    FAST_ENEMY_BG_GOOMBA_GROUND,
} FastEnemyBgPolicy;

typedef enum {
    FAST_ENEMY_COLLISION_NONE = 0,
    FAST_ENEMY_COLLISION_SCAN,
} FastEnemyCollisionPolicy;

typedef enum {
    FAST_PAIR_SKIP = 0,
    FAST_PAIR_HORIZONTAL_SEPARATION,
    FAST_PAIR_VERTICAL_SEPARATION,
} FastPairPolicy;

/*
 * State-zero Piranha Plants and Lakitu always leave the translated routine
 * after its vertical-range test and ID checks. State-zero Goombas use the
 * reviewed ground/side collision policy below. A zero policy deliberately
 * sends every other ID through the complete translated collision logic.
 */
static const uint8_t fast_enemy_bg_policy[Lakitu + 1u] = {
    [Goomba] = FAST_ENEMY_BG_GOOMBA_GROUND,
    [PiranhaPlant] = FAST_ENEMY_BG_EARLY_EXIT,
    [Lakitu] = FAST_ENEMY_BG_EARLY_EXIT,
};

/* ChkForNonSolids' exact six accepted metatiles, encoded as a 256-bit map. */
static const uint8_t fast_non_solid_tile_bits[32] = {
    [0x00u >> 3] = 0x01u,
    [0x26u >> 3] = 0x40u,
    [0x5fu >> 3] = 0x80u,
    [0x60u >> 3] = 0x01u,
    [0xc2u >> 3] = 0x0cu,
};

typedef struct {
    uint16_t block_address;
    uint8_t adder_index;
    uint8_t adjusted_x;
    uint8_t adjusted_y;
    uint8_t block_column;
    uint8_t coordinate;
    uint8_t tile;
} FastEnemyBlockProbe;

/*
 * EnemiesCollision accepts every ID below $15 except the two explicit source
 * exclusions. Keep that semantic allowlist visible and table-driven: zero is
 * always an exact translated fallback.
 */
static const uint8_t fast_enemy_collision_policy[0x15u] = {
    [0x00u] = FAST_ENEMY_COLLISION_SCAN,
    [0x01u] = FAST_ENEMY_COLLISION_SCAN,
    [0x02u] = FAST_ENEMY_COLLISION_SCAN,
    [0x03u] = FAST_ENEMY_COLLISION_SCAN,
    [0x04u] = FAST_ENEMY_COLLISION_SCAN,
    [0x05u] = FAST_ENEMY_COLLISION_SCAN,
    [0x06u] = FAST_ENEMY_COLLISION_SCAN,
    [0x07u] = FAST_ENEMY_COLLISION_SCAN,
    [0x08u] = FAST_ENEMY_COLLISION_SCAN,
    [0x09u] = FAST_ENEMY_COLLISION_SCAN,
    [0x0au] = FAST_ENEMY_COLLISION_SCAN,
    [0x0bu] = FAST_ENEMY_COLLISION_SCAN,
    [0x0cu] = FAST_ENEMY_COLLISION_SCAN,
    [0x0eu] = FAST_ENEMY_COLLISION_SCAN,
    [0x0fu] = FAST_ENEMY_COLLISION_SCAN,
    [0x10u] = FAST_ENEMY_COLLISION_SCAN,
    [0x12u] = FAST_ENEMY_COLLISION_SCAN,
    [0x13u] = FAST_ENEMY_COLLISION_SCAN,
    [0x14u] = FAST_ENEMY_COLLISION_SCAN,
};

static uint8_t rom_byte(uint16_t address) {
    return data[address - 0x8000u];
}

static inline __attribute__((always_inline)) FastEnemyBlockProbe
fast_probe_enemy_block(
    uint8_t slot,
    uint8_t adder_index,
    uint8_t return_horizontal
) {
    FastEnemyBlockProbe probe;
    const uint8_t object_index = (uint8_t)(slot + 1u);
    uint16_t coordinate_sum;
    uint16_t block_base;
    uint8_t block_bank;

    coordinate_sum = (uint16_t)rom_byte((uint16_t)(
        BlockBuffer_X_Adder + adder_index
    )) + ram[(uint8_t)(SprObject_X_Position + object_index)];
    probe.adjusted_x = (uint8_t)coordinate_sum;
    coordinate_sum = (uint16_t)ram[(uint8_t)(
        SprObject_PageLoc + object_index
    )] + (coordinate_sum > 0xffu ? 1u : 0u);
    probe.block_column = (uint8_t)(
        (((uint8_t)coordinate_sum & 1u) << 4) |
        (probe.adjusted_x >> 4)
    );
    block_bank = (uint8_t)(probe.block_column >> 4);
    block_base = block_bank == 0u ? Block_Buffer_1 : Block_Buffer_2;
    probe.block_address = (uint16_t)(
        block_base + (probe.block_column & 0x0fu)
    );
    coordinate_sum = (uint16_t)ram[(uint8_t)(
        SprObject_Y_Position + object_index
    )] + rom_byte((uint16_t)(BlockBuffer_Y_Adder + adder_index));
    probe.adjusted_y = (uint8_t)(
        (coordinate_sum & 0xf0u) - 0x20u
    );
    probe.tile = ram[(probe.block_address + probe.adjusted_y) &
        (RAM_SIZE - 1u)];
    probe.coordinate = (uint8_t)(
        ram[(uint8_t)((return_horizontal != 0u ?
            SprObject_X_Position : SprObject_Y_Position) + object_index)] &
        0x0fu
    );
    probe.adder_index = adder_index;
    return probe;
}

static inline __attribute__((always_inline)) void fast_apply_enemy_block_probe(
    const FastEnemyBlockProbe *probe,
    uint8_t flag
) {
    uint16_t block_base = (probe->block_column & 0x10u) != 0u ?
        Block_Buffer_2 : Block_Buffer_1;

    /*
     * BlockBufferChk_Enemy and the folded BlockBufferCollision leaf both
     * push the flag at the same stack address. The address helper then leaves
     * the column in the next byte. Preserve both otherwise-invisible writes.
     */
    ram[0x100u + sp] = flag;
    ram[0x100u + (uint8_t)(sp - 1u)] = probe->block_column;
    ram[0x4] = probe->adder_index;
    ram[0x5] = probe->adjusted_x;
    ram[0x7] = (uint8_t)(block_base >> 8);
    ram[0x6] = (uint8_t)probe->block_address;
    ram[0x2] = probe->adjusted_y;
    ram[0x3] = probe->tile;
    ram[0x4] = probe->coordinate;
    a = probe->tile;
    x = ram[ObjectOffset];
    y = probe->adder_index;
    carry_flag = true; /* BBChk_E ends with CMP #$00. */
    nz_value = a;
}

static uint8_t fast_enemy_tile_is_non_solid(uint8_t tile) {
    return (uint8_t)(
        fast_non_solid_tile_bits[tile >> 3] &
        (uint8_t)(1u << (tile & 7u))
    );
}

bool smb_core_fast_draw_large_platform(void) {
    const uint8_t slot = x;
    uint8_t sprite_offset;
    uint8_t platform_y;
    uint8_t tile;
    uint8_t offscreen;
    uint8_t index;

    /*
     * The translated stacker uses an eight-bit Y index. Reject layouts whose
     * six 4-byte OAM records would wrap, plus the unaligned layouts that are
     * not produced by the reviewed object allocator. Mismatched object
     * offsets can alias unrelated zero-page object fields, so they retain the
     * literal translated ordering as well.
     */
    if (slot >= 6u || slot != ram[ObjectOffset]) {
        return false;
    }
    sprite_offset = ram[Enemy_SprDataOffset + slot];
    if ((sprite_offset & 3u) != 0u || sprite_offset > 232u) {
        return false;
    }

    ram[0x2] = sprite_offset;
    for (index = 0u; index < 6u; ++index) {
        ram[Sprite_X_Position + sprite_offset + index * 4u] =
            (uint8_t)(ram[Enemy_Rel_XPos] + index * 8u);
    }

    platform_y = ram[Enemy_Y_Position + slot];
    for (index = 0u; index < 4u; ++index) {
        ram[Sprite_Y_Position + sprite_offset + index * 4u] = platform_y;
    }
    if (ram[AreaType] == 3u || ram[SecondaryHardMode] != 0u) {
        platform_y = 0xf8u;
    }
    ram[Sprite_Y_Position + sprite_offset + 16u] = platform_y;
    ram[Sprite_Y_Position + sprite_offset + 20u] = platform_y;

    tile = ram[CloudTypeOverride] != 0u ? 0x75u : 0x5bu;
    for (index = 0u; index < 6u; ++index) {
        const uint8_t output_offset =
            (uint8_t)(sprite_offset + index * 4u);

        ram[Sprite_Tilenumber + output_offset] = tile;
        ram[Sprite_Attributes + output_offset] = 0x02u;
    }

    /* Preserve the reviewed GetXOffscreenBits scratch/register semantics. */
    x = (uint8_t)(slot + 1u);
    GetXOffscreenBits();
    offscreen = a;
    x = slot;
    y = sprite_offset;

    for (index = 0u; index < 6u; ++index) {
        if ((offscreen & (uint8_t)(0x80u >> index)) != 0u) {
            ram[Sprite_Y_Position + sprite_offset + index * 4u] = 0xf8u;
        }
    }

    /* Five PHA stages leave this exact final byte at the original SP. */
    ram[0x100u + sp] = (uint8_t)(offscreen << 5);

    /* Reproduce the sixth ASL and its optional LDA #$f8 side effects. */
    a = (uint8_t)(offscreen << 6);
    carry_flag = (offscreen & 0x04u) != 0u;
    nz_value = a;
    if (carry_flag) {
        a = 0xf8u;
        nz_value = a;
    }

    /* The source deliberately checks the precomputed absolute enemy mask. */
    offscreen = ram[Enemy_OffscreenBits];
    a = (uint8_t)(offscreen << 1);
    carry_flag = (offscreen & 0x80u) != 0u;
    nz_value = a;
    if (carry_flag) {
        a = 0xf8u;
        nz_value = a;
        for (index = 0u; index < 6u; ++index) {
            ram[Sprite_Y_Position + sprite_offset + index * 4u] = a;
        }
    }

    return true;
}

static FastEnemyCollisionPolicy fast_enemy_collision_id_policy(
    uint8_t enemy_id
) {
    if (enemy_id >= sizeof(fast_enemy_collision_policy)) {
        return FAST_ENEMY_COLLISION_NONE;
    }
    return (FastEnemyCollisionPolicy)fast_enemy_collision_policy[enemy_id];
}

static FastPairPolicy fast_pair_policy(
    uint8_t first_box,
    uint8_t second_box
) {
    const uint8_t first_left = ram[BoundingBox_UL_Corner + first_box];
    const uint8_t first_top = ram[BoundingBox_UL_Corner + first_box + 1u];
    const uint8_t first_right = ram[BoundingBox_LR_Corner + first_box];
    const uint8_t first_bottom =
        ram[BoundingBox_LR_Corner + first_box + 1u];
    const uint8_t second_left = ram[BoundingBox_UL_Corner + second_box];
    const uint8_t second_top = ram[BoundingBox_UL_Corner + second_box + 1u];
    const uint8_t second_right = ram[BoundingBox_LR_Corner + second_box];
    const uint8_t second_bottom =
        ram[BoundingBox_LR_Corner + second_box + 1u];

    /* Wrapped/offscreen boxes retain the complete translated collision core. */
    if (
        first_left > first_right || first_top > first_bottom ||
        second_left > second_right || second_top > second_bottom
    ) {
        return FAST_PAIR_SKIP;
    }
    if (first_right < second_left || first_left > second_right) {
        return FAST_PAIR_HORIZONTAL_SEPARATION;
    }
    if (first_bottom < second_top || first_top > second_bottom) {
        return FAST_PAIR_VERTICAL_SEPARATION;
    }
    return FAST_PAIR_SKIP;
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

bool smb_core_fast_offscreen_bounds_check(void) {
    uint8_t slot = x;
    uint8_t enemy_id;
    uint8_t enemy_state;
    uint8_t policy;
    uint8_t value;
    uint8_t left_low;
    uint8_t left_page;
    uint8_t right_low;
    uint8_t right_page;
    uint16_t chain;
    uint16_t carry;
    uint16_t screen_position;
    uint16_t subtrahend;
    uint16_t enemy_position;
    uint16_t boundary;
    uint16_t difference;
    volatile uint8_t *scratch = ram;

    /* Every rejected dispatch must leave the complete emulated machine alone. */
    if (slot >= 6u || slot != ram[ObjectOffset]) {
        return false;
    }
    enemy_id = ram[Enemy_ID + slot];
    if (enemy_id >= sizeof(fast_offscreen_policy)) {
        return false;
    }
    policy = fast_offscreen_policy[enemy_id];
    if (policy == FAST_OFFSCREEN_NONE) {
        return false;
    }
    enemy_state = ram[Enemy_State + slot];
    if (
        (policy & FAST_OFFSCREEN_ANY_STATE) == 0u &&
        enemy_state != 0u &&
        (enemy_state != 5u ||
            (policy & FAST_OFFSCREEN_STATE_5) == 0u)
    ) {
        return false;
    }

    y = enemy_id;
    carry = (uint16_t)(policy & FAST_OFFSCREEN_LEFT_CARRY);
    value = ram[ScreenLeft_X_Pos];
    if ((policy & FAST_OFFSCREEN_LEFT_ADD_38) != 0u) {
        chain = (uint16_t)value + 0x38u + carry;
        value = (uint8_t)chain;
        carry = chain >> 8;
    }
    /*
     * SBC low followed by SBC page is one exact 16-bit subtraction once the
     * optional low-byte-only Piranha ADC has produced its carry. Keep that ADC
     * separate; folding it into the page would change wrap behavior.
     */
    screen_position = (uint16_t)(
        ((uint16_t)ram[ScreenLeft_PageLoc] << 8) | value
    );
    subtrahend = (uint16_t)(0x48u + (carry == 0u));
    boundary = (uint16_t)(screen_position - subtrahend);
    carry = screen_position >= subtrahend;
    left_low = (uint8_t)boundary;
    left_page = (uint8_t)(boundary >> 8);
    scratch[0x1] = left_low;
    scratch[0x0] = left_page;
    screen_position = (uint16_t)(
        ((uint16_t)ram[ScreenRight_PageLoc] << 8) |
        ram[ScreenRight_X_Pos]
    );
    boundary = (uint16_t)(screen_position + 0x48u + carry);
    right_low = (uint8_t)boundary;
    right_page = (uint8_t)(boundary >> 8);
    scratch[0x3] = right_low;
    scratch[0x2] = right_page;

    enemy_position = (uint16_t)(
        ((uint16_t)ram[Enemy_PageLoc + slot] << 8) |
        ram[Enemy_X_Position + slot]
    );
    boundary = (uint16_t)(((uint16_t)left_page << 8) | left_low);
    difference = (uint16_t)(enemy_position - boundary);
    value = (uint8_t)(difference >> 8);
    carry = enemy_position >= boundary;
    if ((value & 0x80u) != 0u) {
        carry_flag = carry != 0u;
        fast_erase_enemy_object(slot);
        return true;
    }

    boundary = (uint16_t)(((uint16_t)right_page << 8) | right_low);
    difference = (uint16_t)(enemy_position - boundary);
    value = (uint8_t)(difference >> 8);
    carry = enemy_position >= boundary;
    if ((value & 0x80u) != 0u) {
        a = value;
        carry_flag = carry != 0u;
        nz_value = value;
        return true;
    }

    if (enemy_state == 5u) {
        a = enemy_state;
        carry_flag = true;
        nz_value = 0u;
        return true;
    }
    if ((policy & FAST_OFFSCREEN_RIGHT_IMMUNE) != 0u) {
        a = enemy_state;
        carry_flag = true;
        nz_value = 0u;
        return true;
    }

    /* The final failed CPY #JumpspringObject leaves carry clear. */
    carry_flag = false;
    fast_erase_enemy_object(slot);
    return true;
}

bool smb_core_fast_enemy_to_bg_collision_det(void) {
    const uint8_t initial_x = x;
    uint8_t enemy_id;
    uint8_t adjusted_y;
    uint8_t policy;
    uint8_t moving_direction;
    uint8_t state_from_ground_check;
    FastEnemyBlockProbe under;
    FastEnemyBlockProbe side;

    /* Do not alter any emulated state until the policy is fully accepted. */
    if (ram[(uint8_t)(Enemy_State + initial_x)] != 0u) {
        return false;
    }
    enemy_id = ram[(uint8_t)(Enemy_ID + initial_x)];
    if (enemy_id > Lakitu) {
        return false;
    }
    policy = fast_enemy_bg_policy[enemy_id];
    if (policy == FAST_ENEMY_BG_NONE) {
        return false;
    }

    /* Exact folded SubtEnemyYPos semantics, including both CMP outcomes. */
    adjusted_y = (uint8_t)(
        ram[(uint8_t)(Enemy_Y_Position + initial_x)] + 0x3eu
    );
    if (policy == FAST_ENEMY_BG_GOOMBA_GROUND) {
        if (adjusted_y < 0x44u) {
            a = adjusted_y;
            carry_flag = false;
            nz_value = (uint8_t)(adjusted_y - 0x44u);
            return true;
        }
        /*
         * The fused path deliberately accepts only a real enemy slot whose
         * wrapper restores the same ObjectOffset. Probe every later decision
         * before touching the machine so exceptional block-hit and direction-
         * reversal paths can use the complete translated implementation.
         */
        if (initial_x >= 6u || initial_x != ram[ObjectOffset]) {
            return false;
        }
        under = fast_probe_enemy_block(initial_x, 0x15u, 0u);
        if (under.tile == 0x23u) {
            return false;
        }

        moving_direction = ram[Enemy_MovingDir + initial_x];
        if (
            ram[Enemy_Y_Position + initial_x] >= 0x20u &&
            (moving_direction == 1u || moving_direction == 2u)
        ) {
            side = fast_probe_enemy_block(
                initial_x,
                moving_direction == 2u ? 0x16u : 0x17u,
                1u
            );
            if (fast_enemy_tile_is_non_solid(side.tile) == 0u) {
                return false;
            }
        }

        /* Fold ChkUnderEnemy and both block-buffer wrapper layers. */
        fast_apply_enemy_block_probe(&under, 0u);
        state_from_ground_check = (uint8_t)(
            fast_enemy_tile_is_non_solid(under.tile) != 0u ||
            (uint8_t)(under.coordinate - 8u) >= 5u
        );
        if (state_from_ground_check != 0u) {
            y = 0u; /* ChkForRedKoopa's TAY of state zero. */
            a = rom_byte(EnemyBGCStateData);
            nz_value = a;
            ram[Enemy_State + initial_x] = a;
        } else {
            /* The state-zero LandEnemyProperly path reaches its second LDA. */
            a = 0u;
            carry_flag = false;
            nz_value = 0u;
        }

        /* DoEnemySideCheck's status-bar return preserves its incoming Y. */
        a = ram[Enemy_Y_Position + initial_x];
        carry_flag = a >= 0x20u;
        nz_value = (uint8_t)(a - 0x20u);
        if (!carry_flag) {
            return true;
        }

        ram[0xeb] = 2u;
        if (moving_direction == 2u) {
            fast_apply_enemy_block_probe(&side, 1u);
        }
        ram[0xeb] = 1u;
        if (moving_direction == 1u) {
            fast_apply_enemy_block_probe(&side, 1u);
        } else {
            a = 1u;
        }
        ram[0xeb] = 0u;
        y = 0x18u;
        carry_flag = true;
        nz_value = 0u;
        return true;
    }

    a = adjusted_y;
    nz_value = (uint8_t)(adjusted_y - 0x44u);
    if (adjusted_y < 0x44u) {
        carry_flag = false;
        return true;
    }

    /*
     * Both selected IDs bypass the Spiny path, then return at CPY #$07.
     * That final comparison is the source routine's observable flag state.
     */
    y = enemy_id;
    carry_flag = true;
    nz_value = (uint8_t)(enemy_id - 0x07u);
    return true;
}

bool smb_core_fast_enemies_collision(void) {
    const uint8_t slot = x;
    const uint8_t frame_counter = ram[FrameCounter];

    /*
     * Fold the source's cheap exits completely. Besides recovering their
     * instruction cost, these assignments preserve the exact observable
     * accumulator, X, Y, carry, and N/Z state at each exit.
     */
    if ((frame_counter & 1u) == 0u) {
        a = (uint8_t)(frame_counter >> 1);
        carry_flag = false;
        nz_value = a;
        return true;
    }
    if (ram[AreaType] == 0u) {
        a = 0u;
        carry_flag = true;
        nz_value = 0u;
        return true;
    }

    {
        /* Enemy_ID is zero page: preserve the source's indexed wrap. */
        const uint8_t enemy_id = ram[(uint8_t)(Enemy_ID + slot)];
        const uint8_t masked_offscreen =
            ram[EnemyOffscrBitsMasked + slot];

        if (
            enemy_id >= 0x15u || enemy_id == Lakitu ||
            enemy_id == PiranhaPlant
        ) {
            a = enemy_id;
            x = ram[ObjectOffset];
            carry_flag = true;
            nz_value = x;
            return true;
        }
        if (masked_offscreen != 0u) {
            a = masked_offscreen;
            x = ram[ObjectOffset];
            carry_flag = enemy_id >= PiranhaPlant;
            nz_value = x;
            return true;
        }
    }

    {
        const uint8_t object_offset = ram[ObjectOffset];
        const uint8_t first_box =
            (uint8_t)(object_offset * 4u + 4u);
        uint8_t ignored_pairs = 0u;
        uint8_t separated_pairs = 0u;
        uint8_t horizontal_pairs = 0u;
        uint8_t candidate;
        uint8_t final_carry;

        if (((uint8_t)(slot - 1u) & 0x80u) != 0u) {
            a = (uint8_t)(ram[Enemy_OffscreenBits] & 0x0fu);
            x = object_offset;
            y = first_box;
            carry_flag = a >= 0x0fu;
            nz_value = x;
            return true;
        }

        /*
         * Preflight every pair before the first emulated write. Noncanonical
         * aliases, wrapped boxes, offscreen candidates, and actual overlaps
         * keep the complete generated routine as a mutation-free fallback.
         */
        if (slot >= 6u || slot != object_offset) {
            return false;
        }

        candidate = slot;
        while (candidate != 0u) {
            const uint8_t pair_bit = (uint8_t)(1u << (candidate - 1u));
            uint8_t second_box;
            FastPairPolicy policy;

            --candidate;
            if (ram[Enemy_Flag + candidate] == 0u) {
                continue;
            }
            if (
                fast_enemy_collision_id_policy(ram[Enemy_ID + candidate]) ==
                    FAST_ENEMY_COLLISION_NONE
            ) {
                ignored_pairs |= pair_bit;
                continue;
            }
            if (ram[EnemyOffscrBitsMasked + candidate] != 0u) {
                return false;
            }
            second_box = (uint8_t)(candidate * 4u + 4u);
            policy = fast_pair_policy(first_box, second_box);
            if (policy == FAST_PAIR_SKIP) {
                return false;
            }
            separated_pairs |= pair_bit;
            if (policy == FAST_PAIR_HORIZONTAL_SEPARATION) {
                horizontal_pairs |= pair_bit;
            }
        }

        /* GetEnemyBoundBoxOfs ends CMP #$0f; inactive slots preserve it. */
        final_carry =
            (uint8_t)((ram[Enemy_OffscreenBits] & 0x0fu) >= 0x0fu);
        candidate = slot;
        while (candidate != 0u) {
            const uint8_t pair_bit = (uint8_t)(1u << (candidate - 1u));

            --candidate;
            ram[0x1] = candidate;

            /* ECLoop's balanced PHA/PLA overwrites the invisible byte. */
            ram[0x100u + sp] = first_box;
            if ((ignored_pairs & pair_bit) != 0u) {
                final_carry = 1u;
            } else if ((separated_pairs & pair_bit) != 0u) {
                ram[0x6] = first_box;
                ram[0x7] = (uint8_t)(
                    (horizontal_pairs & pair_bit) != 0u
                );
                ram[Enemy_CollisionBits + candidate] &=
                    rom_byte((uint16_t)(ClearBitsMask + slot));
                final_carry = 0u;
            }
        }

        a = first_box;
        x = slot;
        y = first_box;
        carry_flag = final_carry != 0u;
        nz_value = slot;
        return true;
    }
}
