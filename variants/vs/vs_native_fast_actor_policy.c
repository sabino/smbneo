#include "vs_native_fast_actor_policy.h"

#include <stddef.h>
#include <stdint.h>

#include "vs_native_fast_motion.h"

#if defined(__GNUC__) || defined(__clang__)
#define ACTOR_POLICY_INLINE static inline __attribute__((always_inline))
#else
#define ACTOR_POLICY_INLINE static inline
#endif

enum {
    ACTOR_SLOT_COUNT = 6,
    CURRENT_ACTOR = 0x0008,
    FRAME_COUNT = 0x0009,
    ACTOR_ACTIVE = 0x000f,
    ACTOR_ID = 0x0016,
    ACTOR_STATE = 0x001e,
    ACTOR_DIRECTION = 0x0046,
    ACTOR_VELOCITY_X = 0x0058,
    ACTOR_POSITION_X_HIGH = 0x006e,
    ACTOR_POSITION_X_LOW = 0x0087,
    ACTOR_SPRITE_ATTRIBUTE = 0x0110,
    ACTOR_MISC_STATE = 0x0125,
    ACTOR_COLLISION_BITS = 0x03c5,
    HARD_MODE = 0x076a,
    ACTOR_OOB_TIMER = 0x078a,
    ACTOR_TIMER = 0x0796,
    SCREEN_LEFT_HIGH = 0x071a,
    SCREEN_RIGHT_HIGH = 0x071b,
    SCREEN_LEFT_LOW = 0x071c,
    SCREEN_RIGHT_LOW = 0x071d,
    ACTOR_ACCEL_TABLE = 0xc913,
    ACTOR_VELOCITY_TABLE = 0xc917
};

ACTOR_POLICY_INLINE uint8_t policy_nz(
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

ACTOR_POLICY_INLINE void policy_cmp(
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

ACTOR_POLICY_INLINE void policy_adc(
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
    ctx->a = policy_nz(ctx, result);
}

ACTOR_POLICY_INLINE void policy_sbc(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    policy_adc(ctx, (uint8_t)~value);
}

ACTOR_POLICY_INLINE uint8_t policy_asl(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u)
    );
    return policy_nz(ctx, (uint8_t)(value << 1));
}

/* Exact native equivalent of motion_y ($BE72), including its state-five
 * gravity selector.  The fixed-point integrator itself is shared with the
 * independently verified native motion module. */
ACTOR_POLICY_INLINE void actor_motion_y(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;

    ctx->y = policy_nz(ctx, 0x3du);
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + ctx->x)]);
    policy_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z) != 0u)
        ctx->y = policy_nz(ctx, 0x20u);
    ctx->a = policy_nz(ctx, 0x03u);
    vs_native_fast_motion_fall(ctx);
}

ACTOR_POLICY_INLINE void actor_erase(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    uint8_t slot = ctx->x;

    ctx->a = policy_nz(ctx, 0u);
    ram[(uint8_t)(ACTOR_ACTIVE + slot)] = ctx->a;
    ram[(uint8_t)(ACTOR_ID + slot)] = ctx->a;
    ram[(uint8_t)(ACTOR_STATE + slot)] = ctx->a;
    ram[ACTOR_SPRITE_ATTRIBUTE + slot] = ctx->a;
    ram[ACTOR_TIMER + slot] = ctx->a;
    ram[ACTOR_MISC_STATE + slot] = ctx->a;
    ram[ACTOR_COLLISION_BITS + slot] = ctx->a;
    ram[ACTOR_OOB_TIMER + slot] = ctx->a;
}

bool vs_native_fast_actor_proc_base(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    uint8_t slot;
    uint8_t saved_velocity;

    if (ctx == NULL || ctx->ram == NULL || ctx->prg == NULL ||
        ctx->suspended != 0u || ctx->unsupported != 0u)
        return false;
    ram = ctx->ram;
    slot = ctx->x;
    if (slot >= ACTOR_SLOT_COUNT || ram[CURRENT_ACTOR] != slot)
        return false;

    ctx->y = policy_nz(ctx, 0u);
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
    ctx->a = policy_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z) != 0u) {
        ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
        ctx->a = policy_asl(ctx, ctx->a);
        if ((ctx->status & NATIVE_C) != 0u)
            goto accelerate;

        ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
        ctx->a = policy_nz(ctx, (uint8_t)(ctx->a & 0x20u));
        if ((ctx->status & NATIVE_Z) == 0u)
            goto defeated;

        ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
        ctx->a = policy_nz(ctx, (uint8_t)(ctx->a & 0x07u));
        if ((ctx->status & NATIVE_Z) != 0u)
            goto accelerate;
        policy_cmp(ctx, ctx->a, 0x05u);
        if ((ctx->status & NATIVE_Z) != 0u)
            goto falling;
        policy_cmp(ctx, ctx->a, 0x03u);
        if ((ctx->status & NATIVE_C) != 0u)
            goto timed_state;
    }

falling:
    actor_motion_y(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return true;
    ctx->y = policy_nz(ctx, 0u);
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
    policy_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z) != 0u) {
        vs_native_fast_motion_x(ctx);
        return true;
    }
    ctx->a = policy_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z) == 0u) {
        ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_ID + slot)]);
        policy_cmp(ctx, ctx->a, 0x2eu);
        if ((ctx->status & NATIVE_Z) == 0u)
            ctx->y = policy_nz(ctx, 1u);
    }

accelerate:
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_VELOCITY_X + slot)]);
    saved_velocity = ctx->a;
    if ((ctx->status & NATIVE_N) != 0u) {
        ctx->y = policy_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->y = policy_nz(ctx, (uint8_t)(ctx->y + 1u));
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    policy_adc(ctx, ctx->prg[ACTOR_ACCEL_TABLE - 0x8000u + ctx->y]);
    ram[(uint8_t)(ACTOR_VELOCITY_X + slot)] = ctx->a;
    vs_native_fast_motion_x(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return true;
    ctx->a = policy_nz(ctx, saved_velocity);
    ram[(uint8_t)(ACTOR_VELOCITY_X + ctx->x)] = ctx->a;
    return true;

timed_state:
    ctx->a = policy_nz(ctx, ram[ACTOR_TIMER + slot]);
    if ((ctx->status & NATIVE_Z) == 0u) {
        policy_cmp(ctx, ctx->a, 0x0eu);
        if ((ctx->status & NATIVE_Z) != 0u) {
            ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_ID + slot)]);
            policy_cmp(ctx, ctx->a, 0x06u);
            if ((ctx->status & NATIVE_Z) != 0u)
                actor_erase(ctx);
        }
        return true;
    }

    ram[(uint8_t)(ACTOR_STATE + slot)] = ctx->a;
    ctx->a = policy_nz(ctx, ram[FRAME_COUNT]);
    ctx->a = policy_nz(ctx, (uint8_t)(ctx->a & 1u));
    ctx->y = policy_nz(ctx, ctx->a);
    ctx->y = policy_nz(ctx, (uint8_t)(ctx->y + 1u));
    ram[(uint8_t)(ACTOR_DIRECTION + slot)] = ctx->y;
    ctx->y = policy_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = policy_nz(ctx, ram[HARD_MODE]);
    if ((ctx->status & NATIVE_Z) == 0u) {
        ctx->y = policy_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->y = policy_nz(ctx, (uint8_t)(ctx->y + 1u));
    }
    ctx->a = policy_nz(
        ctx,
        ctx->prg[ACTOR_VELOCITY_TABLE - 0x8000u + ctx->y]
    );
    ram[(uint8_t)(ACTOR_VELOCITY_X + slot)] = ctx->a;
    return true;

defeated:
    actor_motion_y(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return true;
    vs_native_fast_motion_x(ctx);
    return true;
}

bool vs_native_fast_actor_oob_proc(SmbNeoNativeContext *ctx) {
    static const uint8_t immune_ids[] = {0x0du, 0x30u, 0x31u, 0x32u};
    uint8_t *ram;
    uint8_t slot;

    if (ctx == NULL || ctx->ram == NULL ||
        ctx->suspended != 0u || ctx->unsupported != 0u)
        return false;
    ram = ctx->ram;
    slot = ctx->x;
    if (slot >= ACTOR_SLOT_COUNT)
        return false;

    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_ID + slot)]);
    policy_cmp(ctx, ctx->a, 0x14u);
    if ((ctx->status & NATIVE_Z) != 0u)
        return true;

    ctx->a = policy_nz(ctx, ram[SCREEN_LEFT_LOW]);
    ctx->y = policy_nz(ctx, ram[(uint8_t)(ACTOR_ID + slot)]);
    policy_cmp(ctx, ctx->y, 0x05u);
    if ((ctx->status & NATIVE_Z) == 0u)
        policy_cmp(ctx, ctx->y, 0x0du);
    if ((ctx->status & NATIVE_Z) != 0u)
        policy_adc(ctx, 0x38u);
    policy_sbc(ctx, 0x48u);
    ram[0x01u] = ctx->a;

    ctx->a = policy_nz(ctx, ram[SCREEN_LEFT_HIGH]);
    policy_sbc(ctx, 0u);
    ram[0x00u] = ctx->a;
    ctx->a = policy_nz(ctx, ram[SCREEN_RIGHT_LOW]);
    policy_adc(ctx, 0x48u);
    ram[0x03u] = ctx->a;
    ctx->a = policy_nz(ctx, ram[SCREEN_RIGHT_HIGH]);
    policy_adc(ctx, 0u);
    ram[0x02u] = ctx->a;

    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_POSITION_X_LOW + slot)]);
    policy_cmp(ctx, ctx->a, ram[0x01u]);
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_POSITION_X_HIGH + slot)]);
    policy_sbc(ctx, ram[0x00u]);
    if ((ctx->status & NATIVE_N) != 0u)
        goto erase;

    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_POSITION_X_LOW + slot)]);
    policy_cmp(ctx, ctx->a, ram[0x03u]);
    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_POSITION_X_HIGH + slot)]);
    policy_sbc(ctx, ram[0x02u]);
    if ((ctx->status & NATIVE_N) != 0u)
        return true;

    ctx->a = policy_nz(ctx, ram[(uint8_t)(ACTOR_STATE + slot)]);
    policy_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z) != 0u)
        return true;
    for (unsigned index = 0u;
         index < sizeof(immune_ids) / sizeof(immune_ids[0]);
         ++index) {
        policy_cmp(ctx, ctx->y, immune_ids[index]);
        if ((ctx->status & NATIVE_Z) != 0u)
            return true;
    }

erase:
    actor_erase(ctx);
    return true;
}
