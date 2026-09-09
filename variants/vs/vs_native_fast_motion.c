#include "vs_native_fast_motion.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MOTION_X_VELOCITY = 0x0057,
    MOTION_X_PAGE = 0x006d,
    MOTION_X_POSITION = 0x0086,
    MOTION_Y_VELOCITY = 0x009f,
    MOTION_Y_PAGE = 0x00b5,
    MOTION_Y_POSITION = 0x00ce,
    MOTION_X_FRACTION = 0x0400,
    MOTION_Y_FRACTION = 0x0416,
    MOTION_Y_GRAVITY = 0x0433,
    ACTOR_INDEX = 0x0008,
    PLAYER_ON_SPRING = 0x070e
};

static uint8_t *motion_ram(SmbNeoNativeContext *ctx) {
    if (ctx == NULL)
        return NULL;
    if (ctx->ram == NULL) {
        ctx->unsupported = 1u;
        return NULL;
    }
    return ctx->ram;
}

static uint8_t motion_nz(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) |
        (value == 0u ? NATIVE_Z : 0u)
    );
    return value;
}

static void motion_cmp(
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

static void motion_adc(SmbNeoNativeContext *ctx, uint8_t value) {
    unsigned sum = (unsigned)ctx->a + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)sum;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) != 0u
            ? NATIVE_V : 0u)
    );
    ctx->a = motion_nz(ctx, result);
}

static void motion_sbc(SmbNeoNativeContext *ctx, uint8_t value) {
    motion_adc(ctx, (uint8_t)~value);
}

static void motion_x_do_impl(SmbNeoNativeContext *ctx, uint8_t *ram) {
    uint8_t speed_address = (uint8_t)(MOTION_X_VELOCITY + ctx->x);
    uint8_t position_address = (uint8_t)(MOTION_X_POSITION + ctx->x);
    uint8_t page_address = (uint8_t)(MOTION_X_PAGE + ctx->x);
    uint8_t speed = ram[speed_address];
    uint8_t pixel_delta;
    uint8_t fraction_carry;
    unsigned sum;

    /* Retain the second velocity read: unusual X values can alias ZP $01. */
    ram[0x01u] = (uint8_t)(speed << 4);
    speed = ram[speed_address];
    pixel_delta = (uint8_t)(((speed >> 4) ^ 8u) - 8u);
    ram[0x00u] = pixel_delta;
    ctx->y = (uint8_t)-(pixel_delta >> 7);
    ram[0x02u] = ctx->y;

    sum = (unsigned)ram[MOTION_X_FRACTION + ctx->x] + ram[0x01u];
    ram[MOTION_X_FRACTION + ctx->x] = (uint8_t)sum;
    fraction_carry = (uint8_t)(sum > 255u);

    sum = (unsigned)ram[position_address] + ram[0x00u] + fraction_carry;
    ram[position_address] = (uint8_t)sum;
    sum = (unsigned)ram[page_address] + ram[0x02u] + (sum > 255u ? 1u : 0u);
    ram[page_address] = (uint8_t)sum;

    /* RAM $00 is intentionally re-read after the position writes. */
    ctx->a = motion_nz(ctx, fraction_carry);
    ctx->status &= (uint8_t)~NATIVE_C;
    motion_adc(ctx, ram[0x00u]);
}

void vs_native_fast_motion_x_do(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    motion_x_do_impl(ctx, ram);
}

void vs_native_fast_motion_x(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ctx->x = motion_nz(ctx, (uint8_t)(ctx->x + 1u));
    motion_x_do_impl(ctx, ram);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    ctx->x = motion_nz(ctx, ram[ACTOR_INDEX]);
}

void vs_native_fast_motion_x_player(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ctx->a = motion_nz(ctx, ram[PLAYER_ON_SPRING]);
    if (ctx->a == 0u) {
        ctx->x = motion_nz(ctx, ctx->a);
        motion_x_do_impl(ctx, ram);
    }
}

static void motion_gravity_impl(SmbNeoNativeContext *ctx, uint8_t *ram) {
    uint8_t saved_a = ctx->a;
    uint8_t speed_address = (uint8_t)(MOTION_Y_VELOCITY + ctx->x);
    uint8_t position_address = (uint8_t)(MOTION_Y_POSITION + ctx->x);
    uint8_t page_address = (uint8_t)(MOTION_Y_PAGE + ctx->x);
    unsigned fraction_address = MOTION_Y_FRACTION + ctx->x;
    unsigned gravity_address = MOTION_Y_GRAVITY + ctx->x;

    ctx->a = motion_nz(ctx, ram[fraction_address]);
    ctx->status &= (uint8_t)~NATIVE_C;
    motion_adc(ctx, ram[gravity_address]);
    ram[fraction_address] = ctx->a;

    ctx->y = motion_nz(ctx, 0u);
    ctx->a = motion_nz(ctx, ram[speed_address]);
    if ((ctx->status & NATIVE_N) != 0u)
        ctx->y = motion_nz(ctx, (uint8_t)(ctx->y - 1u));
    ram[0x07u] = ctx->y;
    motion_adc(ctx, ram[position_address]);
    ram[position_address] = ctx->a;
    ctx->a = motion_nz(ctx, ram[page_address]);
    motion_adc(ctx, ram[0x07u]);
    ram[page_address] = ctx->a;

    ctx->a = motion_nz(ctx, ram[gravity_address]);
    ctx->status &= (uint8_t)~NATIVE_C;
    motion_adc(ctx, ram[0x00u]);
    ram[gravity_address] = ctx->a;
    ctx->a = motion_nz(ctx, ram[speed_address]);
    motion_adc(ctx, 0u);
    ram[speed_address] = ctx->a;
    motion_cmp(ctx, ctx->a, ram[0x02u]);
    if ((ctx->status & NATIVE_N) == 0u) {
        ctx->a = motion_nz(ctx, ram[gravity_address]);
        motion_cmp(ctx, ctx->a, 0x80u);
        if ((ctx->status & NATIVE_C) != 0u) {
            ctx->a = motion_nz(ctx, ram[0x02u]);
            ram[speed_address] = ctx->a;
            ctx->a = motion_nz(ctx, 0u);
            ram[gravity_address] = ctx->a;
        }
    }

    ctx->a = motion_nz(ctx, saved_a);
    if ((ctx->status & NATIVE_Z) != 0u)
        return;

    ctx->a = motion_nz(ctx, ram[0x02u]);
    ctx->a = motion_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = motion_nz(ctx, ctx->a);
    ctx->y = motion_nz(ctx, (uint8_t)(ctx->y + 1u));
    ram[0x07u] = ctx->y;
    ctx->a = motion_nz(ctx, ram[gravity_address]);
    ctx->status |= NATIVE_C;
    motion_sbc(ctx, ram[0x01u]);
    ram[gravity_address] = ctx->a;
    ctx->a = motion_nz(ctx, ram[speed_address]);
    motion_sbc(ctx, 0u);
    ram[speed_address] = ctx->a;
    motion_cmp(ctx, ctx->a, ram[0x07u]);
    if ((ctx->status & NATIVE_N) == 0u)
        return;

    ctx->a = motion_nz(ctx, ram[gravity_address]);
    motion_cmp(ctx, ctx->a, 0x80u);
    if ((ctx->status & NATIVE_C) != 0u)
        return;

    ctx->a = motion_nz(ctx, ram[0x07u]);
    ram[speed_address] = ctx->a;
    ctx->a = motion_nz(ctx, 0xffu);
    ram[gravity_address] = ctx->a;
}

void vs_native_fast_motion_gravity(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    motion_gravity_impl(ctx, ram);
}

void vs_native_fast_motion_gravity_do(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ram[0x02u] = ctx->a;
    ctx->a = motion_nz(ctx, 0u);
    motion_gravity_impl(ctx, ram);
}

void vs_native_fast_motion_fall(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ram[0x00u] = ctx->y;
    ctx->x = motion_nz(ctx, (uint8_t)(ctx->x + 1u));
    ram[0x02u] = ctx->a;
    ctx->a = motion_nz(ctx, 0u);
    motion_gravity_impl(ctx, ram);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    ctx->x = motion_nz(ctx, ram[ACTOR_INDEX]);
}

void vs_native_fast_motion_fall_fast(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ctx->y = motion_nz(ctx, 0x7fu);
    ctx->a = motion_nz(ctx, 0x02u);
    vs_native_fast_motion_fall(ctx);
}

void vs_native_fast_motion_fall_slow(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ctx->y = motion_nz(ctx, 0x0fu);
    ctx->a = motion_nz(ctx, 0x02u);
    vs_native_fast_motion_fall(ctx);
}

void vs_native_fast_motion_fall_mid(SmbNeoNativeContext *ctx) {
    uint8_t *ram = motion_ram(ctx);
    if (ram == NULL)
        return;
    ctx->y = motion_nz(ctx, 0x1cu);
    ctx->a = motion_nz(ctx, 0x03u);
    vs_native_fast_motion_fall(ctx);
}
