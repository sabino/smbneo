#include "vs_native_fast_actor_loop.h"

#include <stddef.h>
#include <stdint.h>

/* Authoritative generated/native children retained by this coordinator. */
void smbneo_native_fn_actor_area_loop_bfa5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_killall_cfb4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_c160(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_do_c1b5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_enemy_c823(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_00_proc_c878(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_01_proc_d1d8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_02_proc_c819(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_06_proc_c88a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_0F_proc_c8a8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_16_proc_c890(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_18_proc_cfa8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_19_proc_bb90(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1A_proc_b7ff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1C_proc_d21c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1D_proc_b76e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1F_proc_b658(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_20_proc_c81a(SmbNeoNativeContext *ctx);

#if defined(__GNUC__) || defined(__clang__)
#define ACTOR_LOOP_INLINE static inline __attribute__((always_inline))
#else
#define ACTOR_LOOP_INLINE static inline
#endif

ACTOR_LOOP_INLINE uint8_t actor_loop_nz(
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

ACTOR_LOOP_INLINE void actor_loop_cmp(
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

ACTOR_LOOP_INLINE void actor_loop_adc(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    unsigned sum = (unsigned)ctx->a + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)sum;

    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) != 0u
            ? NATIVE_V : 0u)
    );
    ctx->a = actor_loop_nz(ctx, result);
}

ACTOR_LOOP_INLINE void actor_loop_sbc(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    actor_loop_adc(ctx, (uint8_t)~value);
}

ACTOR_LOOP_INLINE uint8_t actor_loop_asl(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u)
    );
    return actor_loop_nz(ctx, (uint8_t)(value << 1));
}

ACTOR_LOOP_INLINE uint8_t actor_loop_lsr(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    return actor_loop_nz(ctx, (uint8_t)(value >> 1));
}

ACTOR_LOOP_INLINE uint8_t actor_loop_read(
    SmbNeoNativeContext *ctx,
    uint16_t address
) {
    if (address < 0x2000u)
        return ctx->ram[address & 0x07ffu];
    if (address >= 0x6000u) {
        if (address < 0x8000u)
            return ctx->extra_ram[address & 0x07ffu];
        return ctx->prg[address - 0x8000u];
    }
    return ctx->read8(ctx->opaque, address);
}

ACTOR_LOOP_INLINE void actor_loop_write(
    SmbNeoNativeContext *ctx,
    uint16_t address,
    uint8_t value
) {
    if (address < 0x2000u) {
        ctx->ram[address & 0x07ffu] = value;
        return;
    }
    if (address >= 0x6000u) {
        if (address < 0x8000u)
            ctx->extra_ram[address & 0x07ffu] = value;
        return;
    }
    ctx->write8(ctx->opaque, address, value);
}

ACTOR_LOOP_INLINE uint16_t actor_loop_zpword(
    SmbNeoNativeContext *ctx,
    uint8_t address
) {
    return (uint16_t)(
        actor_loop_read(ctx, address) |
        ((uint16_t)actor_loop_read(ctx, (uint8_t)(address + 1u)) << 8)
    );
}

#define ACTOR_LOOP_CALL(child) \
    do { \
        child(ctx); \
        if (ctx->suspended != 0u || ctx->unsupported != 0u) \
            return; \
    } while (0)

/* Exact $C7D2 tbljmp expansion.  Staging $04..$07 is observable scratch RAM. */
static void actor_loop_dispatch_active(
    SmbNeoNativeContext *ctx,
    uint8_t selector
) {
    static const uint8_t target_low[34] = {
        0x23u, 0x78u, 0xd8u, 0x19u, 0x19u, 0x19u, 0x19u,
        0x8au, 0x8au, 0x8au, 0x8au, 0x8au, 0x8au, 0x8au, 0x8au,
        0x19u, 0xa8u, 0xa8u, 0xa8u, 0xa8u, 0xa8u, 0xa8u, 0xa8u,
        0x90u, 0x90u, 0xa8u, 0x90u, 0xffu, 0x19u, 0x1cu, 0x6eu,
        0x19u, 0x58u, 0x1au
    };
    static const uint8_t target_high[34] = {
        0xc8u, 0xc8u, 0xd1u, 0xc8u, 0xc8u, 0xc8u, 0xc8u,
        0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u,
        0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u, 0xc8u,
        0xc8u, 0xc8u, 0xcfu, 0xbbu, 0xb7u, 0xc8u, 0xd2u, 0xb7u,
        0xc8u, 0xb6u, 0xc8u
    };

    ctx->a = actor_loop_asl(ctx, ctx->a);
    ctx->y = actor_loop_nz(ctx, ctx->a);
    actor_loop_write(ctx, 0x04u, 0xd4u);
    actor_loop_write(ctx, 0x05u, 0xc7u);
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (selector >= 34u) {
        ctx->unsupported = 1u;
        return;
    }
    actor_loop_write(ctx, 0x06u, target_low[selector]);
    actor_loop_write(ctx, 0x07u, target_high[selector]);
    ctx->a = actor_loop_nz(ctx, target_high[selector]);

    switch (selector) {
    case 0u:
        smbneo_native_fn_actor_proc_enemy_c823(ctx);
        return;
    case 1u:
        smbneo_native_fn_actor_B_00_proc_c878(ctx);
        return;
    case 2u:
        smbneo_native_fn_actor_B_01_proc_d1d8(ctx);
        return;
    case 3u: case 4u: case 5u: case 6u: case 15u: case 28u: case 31u:
        smbneo_native_fn_actor_B_02_proc_c819(ctx);
        return;
    case 7u: case 8u: case 9u: case 10u: case 11u: case 12u:
    case 13u: case 14u:
        smbneo_native_fn_actor_B_06_proc_c88a(ctx);
        return;
    case 16u: case 17u: case 18u: case 19u: case 20u: case 21u:
    case 22u:
        smbneo_native_fn_actor_B_0F_proc_c8a8(ctx);
        return;
    case 23u: case 24u:
        smbneo_native_fn_actor_B_16_proc_c890(ctx);
        return;
    case 25u:
        smbneo_native_fn_actor_B_18_proc_cfa8(ctx);
        return;
    case 26u:
        smbneo_native_fn_actor_B_19_proc_bb90(ctx);
        return;
    case 27u:
        smbneo_native_fn_actor_B_1A_proc_b7ff(ctx);
        return;
    case 29u:
        smbneo_native_fn_actor_B_1C_proc_d21c(ctx);
        return;
    case 30u:
        smbneo_native_fn_actor_B_1D_proc_b76e(ctx);
        return;
    case 32u:
        smbneo_native_fn_actor_B_1F_proc_b658(ctx);
        return;
    case 33u:
        smbneo_native_fn_actor_B_20_proc_c81a(ctx);
        return;
    default:
        ctx->unsupported = 1u;
        return;
    }
}

void vs_native_fast_actor_proc(SmbNeoNativeContext *ctx) {
    uint8_t saved_value_0;
    uint8_t saved_value_1;

    if (ctx == NULL)
        return;
    if (ctx->ram == NULL || ctx->extra_ram == NULL || ctx->prg == NULL ||
        ctx->read8 == NULL || ctx->write8 == NULL) {
        ctx->unsupported = 1u;
        return;
    }
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;

    ctx->a = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x))
    );
    saved_value_0 = ctx->a;
    ctx->a = actor_loop_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C) != 0u)
        goto child_actor;
    ctx->a = actor_loop_nz(ctx, saved_value_0);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto free_actor;

    ctx->x = actor_loop_nz(ctx, actor_loop_read(ctx, 0x08u));
    ctx->a = actor_loop_nz(ctx, 0u);
    ctx->y = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x))
    );
    actor_loop_cmp(ctx, ctx->y, 0x15u);
    if ((ctx->status & NATIVE_C) != 0u) {
        ctx->a = actor_loop_nz(ctx, ctx->y);
        actor_loop_sbc(ctx, 0x14u);
    }
    actor_loop_dispatch_active(ctx, ctx->a);
    return;

free_actor:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x071fu));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    actor_loop_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z) != 0u)
        return;
    goto actor_loop;

child_actor:
    ctx->a = actor_loop_nz(ctx, saved_value_0);
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->y = actor_loop_nz(ctx, ctx->a);
    ctx->a = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(0x000fu + ctx->y))
    );
    if ((ctx->status & NATIVE_Z) != 0u)
        actor_loop_write(
            ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a
        );
    return;

actor_loop:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0745u));
    if ((ctx->status & NATIVE_Z) != 0u)
        goto swarm_queue;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0726u));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto swarm_queue;
    ctx->y = actor_loop_nz(ctx, 0x0bu);

course_loop_find:
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N) != 0u)
        goto swarm_queue;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x075fu));
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(
        ctx, (uint16_t)(0xbf84u + ctx->y)
    ));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_find;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0725u));
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(
        ctx, (uint16_t)(0xbf8fu + ctx->y)
    ));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_find;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x00ceu));
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(
        ctx, (uint16_t)(0xbf9au + ctx->y)
    ));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_wrong;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x001du));
    actor_loop_cmp(ctx, ctx->a, 0u);
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_wrong;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x075fu));
    actor_loop_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_reset;
    actor_loop_write(ctx, 0x06d9u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x06d9u) + 1u)
    ));

course_loop_count:
    actor_loop_write(ctx, 0x06dau, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x06dau) + 1u)
    ));
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x06dau));
    actor_loop_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z) == 0u)
        goto course_loop_flag_clear;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x06d9u));
    actor_loop_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto course_loop_reset;
    goto course_loop_apply;

course_loop_wrong:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x075fu));
    actor_loop_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto course_loop_count;

course_loop_apply:
    ACTOR_LOOP_CALL(smbneo_native_fn_actor_area_loop_bfa5);
    ACTOR_LOOP_CALL(smbneo_native_fn_actor_killall_cfb4);

course_loop_reset:
    ctx->a = actor_loop_nz(ctx, 0u);
    actor_loop_write(ctx, 0x06dau, ctx->a);
    actor_loop_write(ctx, 0x06d9u, ctx->a);

course_loop_flag_clear:
    ctx->a = actor_loop_nz(ctx, 0u);
    actor_loop_write(ctx, 0x0745u, ctx->a);

swarm_queue:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x06cdu));
    if ((ctx->status & NATIVE_Z) == 0u) {
        actor_loop_write(
            ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a
        );
        ctx->a = actor_loop_nz(ctx, 1u);
        actor_loop_write(
            ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a
        );
        ctx->a = actor_loop_nz(ctx, 0u);
        actor_loop_write(
            ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a
        );
        actor_loop_write(ctx, 0x06cdu, ctx->a);
        smbneo_native_fn_actor_init_c160(ctx);
        return;
    }

    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->y = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0739u));
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    actor_loop_cmp(ctx, ctx->a, 0xffu);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto actor_init_check;
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    actor_loop_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto actor_screen_window;
    actor_loop_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_C) == 0u)
        goto actor_screen_window;
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    actor_loop_cmp(ctx, ctx->a, 0x2eu);
    if ((ctx->status & NATIVE_Z) == 0u)
        return;

actor_screen_window:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x071du));
    ctx->status &= (uint8_t)~NATIVE_C;
    actor_loop_adc(ctx, 0x30u);
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    actor_loop_write(ctx, 0x07u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x071bu));
    actor_loop_adc(ctx, 0u);
    actor_loop_write(ctx, 0x06u, ctx->a);
    ctx->y = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0739u));
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C) != 0u &&
        actor_loop_read(ctx, 0x073bu) == 0u) {
        /* The LDA before this branch updates N/Z even on the false path. */
        ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x073bu));
        actor_loop_write(ctx, 0x073bu, actor_loop_nz(
            ctx, (uint8_t)(actor_loop_read(ctx, 0x073bu) + 1u)
        ));
        actor_loop_write(ctx, 0x073au, actor_loop_nz(
            ctx, (uint8_t)(actor_loop_read(ctx, 0x073au) + 1u)
        ));
    } else if ((ctx->status & NATIVE_C) != 0u) {
        ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x073bu));
    }

    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    actor_loop_cmp(ctx, ctx->a, 0x0fu);
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_spawn_position;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x073bu));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_spawn_position;
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    actor_loop_write(ctx, 0x073au, ctx->a);
    actor_loop_write(ctx, 0x0739u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x0739u) + 1u)
    ));
    actor_loop_write(ctx, 0x0739u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x0739u) + 1u)
    ));
    actor_loop_write(ctx, 0x073bu, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x073bu) + 1u)
    ));
    goto actor_loop;

actor_spawn_position:
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x073au));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a
    );
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(ctx, 0x071du));
    ctx->a = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x))
    );
    actor_loop_sbc(ctx, actor_loop_read(ctx, 0x071bu));
    if ((ctx->status & NATIVE_C) != 0u)
        goto actor_right_bound;
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    actor_loop_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto actor_special_a;
    goto actor_special_b;

actor_right_bound:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x07u));
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(
        ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)
    ));
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x06u));
    actor_loop_sbc(ctx, actor_loop_read(
        ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)
    ));
    if ((ctx->status & NATIVE_C) == 0u)
        goto actor_init_check;
    ctx->a = actor_loop_nz(ctx, 1u);
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_asl(ctx, ctx->a);
    ctx->a = actor_loop_asl(ctx, ctx->a);
    ctx->a = actor_loop_asl(ctx, ctx->a);
    ctx->a = actor_loop_asl(ctx, ctx->a);
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a
    );
    actor_loop_cmp(ctx, ctx->a, 0xe0u);
    if ((ctx->status & NATIVE_Z) != 0u)
        goto actor_special_a;
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    actor_loop_cmp(ctx, ctx->a, 0x37u);
    if ((ctx->status & NATIVE_C) == 0u)
        goto actor_spawn;
    actor_loop_cmp(ctx, ctx->a, 0x3fu);
    if ((ctx->status & NATIVE_C) == 0u)
        goto actor_group;

actor_spawn:
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, 1u);
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a
    );
    smbneo_native_fn_actor_init_c160(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    ctx->a = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x))
    );
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_special_do;
    return;

actor_init_check:
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x06cbu));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_init_check_spawn;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0398u));
    actor_loop_cmp(ctx, ctx->a, 1u);
    if ((ctx->status & NATIVE_Z) == 0u)
        return;
    ctx->a = actor_loop_nz(ctx, 0x2fu);

actor_init_check_spawn:
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a
    );
    smbneo_native_fn_actor_init_c160(ctx);
    return;

actor_group:
    ctx->y = actor_loop_nz(ctx, 0u);
    ctx->status |= NATIVE_C;
    actor_loop_sbc(ctx, 0x37u);
    saved_value_0 = ctx->a;
    actor_loop_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C) == 0u) {
        saved_value_1 = ctx->a;
        ctx->y = actor_loop_nz(ctx, 0x06u);
        ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x076au));
        if ((ctx->status & NATIVE_Z) == 0u)
            ctx->y = actor_loop_nz(ctx, 0x02u);
        ctx->a = actor_loop_nz(ctx, saved_value_1);
    }
    actor_loop_write(ctx, 0x01u, ctx->y);
    ctx->y = actor_loop_nz(ctx, 0xb0u);
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z) == 0u)
        ctx->y = actor_loop_nz(ctx, 0x70u);
    actor_loop_write(ctx, 0x00u, ctx->y);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x071bu));
    actor_loop_write(ctx, 0x02u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x071du));
    actor_loop_write(ctx, 0x03u, ctx->a);
    ctx->y = actor_loop_nz(ctx, 2u);
    ctx->a = actor_loop_nz(ctx, saved_value_0);
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C) != 0u)
        ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    actor_loop_write(ctx, 0x06d3u, ctx->y);

actor_group_next:
    ctx->x = actor_loop_nz(ctx, 0xffu);

actor_group_find_slot:
    ctx->x = actor_loop_nz(ctx, (uint8_t)(ctx->x + 1u));
    actor_loop_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_C) != 0u)
        goto actor_special_do;
    ctx->a = actor_loop_nz(
        ctx, actor_loop_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x))
    );
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_group_find_slot;
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x01u));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x02u));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x03u));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a
    );
    ctx->status &= (uint8_t)~NATIVE_C;
    actor_loop_adc(ctx, 0x18u);
    actor_loop_write(ctx, 0x03u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x02u));
    actor_loop_adc(ctx, 0u);
    actor_loop_write(ctx, 0x02u, ctx->a);
    ctx->a = actor_loop_nz(ctx, actor_loop_read(ctx, 0x00u));
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a
    );
    ctx->a = actor_loop_nz(ctx, 1u);
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a
    );
    actor_loop_write(
        ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a
    );
    smbneo_native_fn_actor_init_do_c1b5(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    actor_loop_write(ctx, 0x06d3u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x06d3u) - 1u)
    ));
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_group_next;
    goto actor_special_do;

actor_special_a:
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    ctx->a = actor_loop_lsr(ctx, ctx->a);
    actor_loop_cmp(ctx, ctx->a, actor_loop_read(ctx, 0x075fu));
    if ((ctx->status & NATIVE_Z) != 0u) {
        ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y - 1u));
        ctx->a = actor_loop_nz(ctx, 2u);
        actor_loop_write(ctx, 0x4016u, ctx->a);
        ctx->a = actor_loop_nz(ctx, actor_loop_read(
            ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
        ));
        actor_loop_write(ctx, 0x0750u, ctx->a);
        ctx->y = actor_loop_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->a = actor_loop_nz(ctx, actor_loop_read(
            ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
        ));
        ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
        actor_loop_write(ctx, 0x0751u, ctx->a);
    }
    goto actor_special_inc_one;

actor_special_b:
    ctx->a = actor_loop_nz(ctx, 2u);
    actor_loop_write(ctx, 0x4016u, ctx->a);
    ctx->y = actor_loop_nz(ctx, actor_loop_read(ctx, 0x0739u));
    ctx->a = actor_loop_nz(ctx, actor_loop_read(
        ctx, (uint16_t)(actor_loop_zpword(ctx, 0xe9u) + ctx->y)
    ));
    ctx->a = actor_loop_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    actor_loop_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z) == 0u)
        goto actor_special_do;

actor_special_inc_one:
    actor_loop_write(ctx, 0x0739u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x0739u) + 1u)
    ));

actor_special_do:
    actor_loop_write(ctx, 0x0739u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x0739u) + 1u)
    ));
    actor_loop_write(ctx, 0x0739u, actor_loop_nz(
        ctx, (uint8_t)(actor_loop_read(ctx, 0x0739u) + 1u)
    ));
    ctx->a = actor_loop_nz(ctx, 0u);
    actor_loop_write(ctx, 0x073bu, ctx->a);
    ctx->x = actor_loop_nz(ctx, actor_loop_read(ctx, 0x08u));
    return;
}

#undef ACTOR_LOOP_CALL
