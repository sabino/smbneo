#include "vs_native_fast_game.h"

#include <stddef.h>
#include <stdint.h>

/* Direct-C child routines retained as the authoritative gameplay bodies. */
void smbneo_native_fn_player_proc_aef7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_proj_proc_b4d1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_bf60(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_score_disp_86a4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_player_f0e5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_player_f08f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_ee46(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_flatten_bde3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_proc_bd7f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_ba8a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_cannon_proc_b8b0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_whirlpool_proc_b66c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pole_proc_b709(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_time_proc_b5fc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_color_rotate_8be2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_area_music_977e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_firefl_do_b135(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_firefl_do_2_b147(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_proc_scenery_scroll_ae1c(
    SmbNeoNativeContext *ctx
);

enum {
    FRAME_COUNT = 0x0009,
    JOYPAD_A_B = 0x000a,
    JOYPAD_L_R = 0x000c,
    JOYPAD_A_B_HELD = 0x000d,
    ACTOR_INDEX = 0x0008,
    POS_Y_HI_PLAYER = 0x00b5,
    JOYPAD_P1 = 0x06fc,
    JOYPAD_P2 = 0x06fd,
    PROC_ID_GAME = 0x0772,
    PLAYER_ID = 0x0753,
    PLAYER_COUNT = 0x077a,
    TIMER_NMI_TICK = 0x077f,
    TIMER_PLAYER_STAR = 0x079f
};

#if defined(__GNUC__) || defined(__clang__)
#define GAME_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define GAME_FORCE_INLINE static inline
#endif

GAME_FORCE_INLINE uint8_t *game_ram(SmbNeoNativeContext *ctx) {
    if (ctx == NULL)
        return NULL;
    if (ctx->ram == NULL) {
        ctx->unsupported = 1u;
        return NULL;
    }
    return ctx->ram;
}

GAME_FORCE_INLINE uint8_t game_nz(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) |
        (value == 0u ? NATIVE_Z : 0u)
    );
    return value;
}

GAME_FORCE_INLINE void game_cmp(
    SmbNeoNativeContext *ctx,
    uint8_t lhs,
    uint8_t rhs
) {
    uint8_t result = (uint8_t)(lhs - rhs);
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) |
        (lhs >= rhs ? NATIVE_C : 0u) |
        (result & NATIVE_N) |
        (result == 0u ? NATIVE_Z : 0u)
    );
}

GAME_FORCE_INLINE uint8_t game_lsr(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    return game_nz(ctx, (uint8_t)(value >> 1));
}

#define GAME_CALL(child) \
    do { \
        child(ctx); \
        if (ctx->suspended != 0u || ctx->unsupported != 0u) \
            return; \
    } while (0)

void vs_native_fast_game_proc(SmbNeoNativeContext *ctx) {
    uint8_t *ram = game_ram(ctx);
    uint8_t input;

    if (ram == NULL)
        return;

    if (ram[PLAYER_COUNT] != 0u) {
        ctx->x = ram[PLAYER_ID];
        input = ram[JOYPAD_P1 + ctx->x];
    } else {
        input = (uint8_t)(ram[JOYPAD_P1] | ram[JOYPAD_P2]);
    }

    /* VS input arbitration: right wins over left and down wins over up. */
    if ((input & 0x01u) != 0u)
        input &= 0xfdu;
    if ((input & 0x04u) != 0u)
        input &= 0xf7u;
    ram[JOYPAD_P1] = input;
    ctx->a = game_nz(ctx, (input & 0x04u) != 0u ? input : 0u);

    GAME_CALL(smbneo_native_fn_player_proc_aef7);

    ctx->a = game_nz(ctx, ram[PROC_ID_GAME]);
    game_cmp(ctx, ctx->a, 3u);
    if ((ctx->status & NATIVE_C) == 0u)
        return;

    GAME_CALL(smbneo_native_fn_proj_proc_b4d1);

    ctx->x = game_nz(ctx, 0u);
    for (;;) {
        ram[ACTOR_INDEX] = ctx->x;
        GAME_CALL(smbneo_native_fn_actor_proc_bf60);
        GAME_CALL(smbneo_native_fn_bg_score_disp_86a4);
        ctx->x = game_nz(ctx, (uint8_t)(ctx->x + 1u));
        game_cmp(ctx, ctx->x, 6u);
        if ((ctx->status & NATIVE_Z) != 0u)
            break;
    }

    GAME_CALL(smbneo_native_fn_pos_bits_get_player_f0e5);
    GAME_CALL(smbneo_native_fn_pos_calc_x_rel_player_f08f);
    GAME_CALL(smbneo_native_fn_render_player_ee46);
    GAME_CALL(smbneo_native_fn_block_flatten_bde3);

    ctx->x = game_nz(ctx, 1u);
    ram[ACTOR_INDEX] = ctx->x;
    GAME_CALL(smbneo_native_fn_block_proc_bd7f);
    ctx->x = game_nz(ctx, (uint8_t)(ctx->x - 1u));
    ram[ACTOR_INDEX] = ctx->x;
    GAME_CALL(smbneo_native_fn_block_proc_bd7f);

    GAME_CALL(smbneo_native_fn_misc_proc_ba8a);
    GAME_CALL(smbneo_native_fn_cannon_proc_b8b0);
    GAME_CALL(smbneo_native_fn_whirlpool_proc_b66c);
    GAME_CALL(smbneo_native_fn_pole_proc_b709);
    GAME_CALL(smbneo_native_fn_stats_time_proc_b5fc);
    GAME_CALL(smbneo_native_fn_meta_color_rotate_8be2);

    ctx->a = game_nz(ctx, ram[POS_Y_HI_PLAYER]);
    game_cmp(ctx, ctx->a, 2u);
    if ((ctx->status & NATIVE_N) != 0u) {
        ctx->a = game_nz(ctx, ram[TIMER_PLAYER_STAR]);
        if ((ctx->status & NATIVE_Z) != 0u) {
            GAME_CALL(smbneo_native_fn_player_proc_firefl_do_2_b147);
            goto finish;
        }
        game_cmp(ctx, ctx->a, 4u);
        if ((ctx->status & NATIVE_Z) != 0u) {
            ctx->a = game_nz(ctx, ram[TIMER_NMI_TICK]);
            if ((ctx->status & NATIVE_Z) != 0u)
                GAME_CALL(smbneo_native_fn_game_init_area_music_977e);
        }
    }

    ctx->y = game_nz(ctx, ram[TIMER_PLAYER_STAR]);
    ctx->a = game_nz(ctx, ram[FRAME_COUNT]);
    game_cmp(ctx, ctx->y, 8u);
    if ((ctx->status & NATIVE_C) == 0u) {
        ctx->a = game_lsr(ctx, ctx->a);
        ctx->a = game_lsr(ctx, ctx->a);
    }
    ctx->a = game_lsr(ctx, ctx->a);
    GAME_CALL(smbneo_native_fn_player_proc_firefl_do_b135);

finish:
    ram[JOYPAD_A_B_HELD] = ram[JOYPAD_A_B];
    ctx->a = game_nz(ctx, 0u);
    ram[JOYPAD_L_R] = ctx->a;
    smbneo_native_fn_game_proc_scenery_scroll_ae1c(ctx);
}

#undef GAME_CALL
