#include "vs_native_fast_actor.h"

#include <stddef.h>

static uint8_t fast_nz(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) | (value == 0u ? NATIVE_Z : 0u));
    return value;
}

static void fast_adc(SmbNeoNativeContext *ctx, uint8_t value) {
    unsigned sum = (unsigned)ctx->a + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)sum;
    ctx->status = (uint8_t)((ctx->status &
        (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) != 0u
            ? NATIVE_V : 0u));
    ctx->a = fast_nz(ctx, result);
}

static uint8_t fast_lsr(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u));
    return fast_nz(ctx, (uint8_t)(value >> 1));
}

/* F1E7 render_chr_pair_do.  The caller has already staged tiles in $00/$01. */
static void render_pair_staged(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    unsigned offset = ctx->y;
    unsigned next = offset + 8u;
    uint8_t flip = (uint8_t)(ram[3] & 2u);
    uint8_t attr = (uint8_t)(ram[4] | (flip != 0u ? 0x40u : 0u));

    ram[0x0201u + offset] = ram[flip != 0u ? 1u : 0u];
    ram[0x0205u + offset] = ram[flip != 0u ? 0u : 1u];
    ram[0x0202u + offset] = attr;
    ram[0x0206u + offset] = attr;
    ram[0x0200u + offset] = ram[2];
    ram[0x0204u + offset] = ram[2];
    ram[0x0203u + offset] = ram[5];
    ram[0x0207u + offset] = (uint8_t)(ram[5] + 8u);
    ram[2] = (uint8_t)(ram[2] + 8u);

    ctx->a = ctx->y = (uint8_t)next;
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (next > 255u ? NATIVE_C : 0u) |
        (offset >= 120u && offset < 128u ? NATIVE_V : 0u));
    ctx->x = fast_nz(ctx, (uint8_t)(ctx->x + 2u));
}

bool vs_native_fast_render_chr_pair(SmbNeoNativeContext *ctx) {
    if (ctx == NULL || ctx->ram == NULL)
        return false;
    ctx->ram[1] = ctx->a;
    render_pair_staged(ctx);
    return true;
}

bool vs_native_fast_render_actor_chr_pair(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    const uint8_t *prg;

    if (ctx == NULL || ctx->ram == NULL || ctx->prg == NULL)
        return false;
    ram = ctx->ram;
    prg = ctx->prg;
    ram[0] = prg[0xe69bu - 0x8000u + ctx->x];
    ctx->a = fast_nz(ctx, prg[0xe69cu - 0x8000u + ctx->x]);
    ram[1] = ctx->a;
    render_pair_staged(ctx);
    return true;
}

/* The exported tile compositor needs to fetch each pair.  Keep this private
 * implementation separate so render_actor can use it without rechecking. */
static void render_actor_tiles(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    const uint8_t *prg = ctx->prg;

    ctx->y = fast_nz(ctx, ram[0xeb]);
    for (unsigned pair = 0u; pair < 3u; ++pair) {
        ram[0] = prg[0xe69bu - 0x8000u + ctx->x];
        ctx->a = fast_nz(ctx, prg[0xe69cu - 0x8000u + ctx->x]);
        ram[1] = ctx->a;
        render_pair_staged(ctx);
    }
}

bool vs_native_fast_render_actor_tiles(SmbNeoNativeContext *ctx) {
    if (ctx == NULL || ctx->ram == NULL || ctx->prg == NULL)
        return false;
    render_actor_tiles(ctx);
    return true;
}

static void render_actor_clear_h(
    SmbNeoNativeContext *ctx,
    uint8_t offset
) {
    uint8_t *ram = ctx->ram;

    ctx->a = fast_nz(ctx, offset);
    ctx->status &= (uint8_t)~NATIVE_C;
    fast_adc(ctx, ram[0x06e5u + ctx->x]);
    ctx->y = fast_nz(ctx, ctx->a);
    ctx->a = fast_nz(ctx, 0xf8u);
    ram[0x0200u + ctx->y] = ctx->a;
    ram[0x0208u + ctx->y] = ctx->a;
    ram[0x0210u + ctx->y] = ctx->a;
}

/* Common horizontal-only tail at EAC1. */
static void render_actor_cleanup(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    uint8_t shifted;

    ctx->x = fast_nz(ctx, ram[8]);
    ctx->a = fast_nz(ctx, ram[0x03d1]);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    shifted = ctx->a;
    if ((ctx->status & NATIVE_C) != 0u)
        render_actor_clear_h(ctx, 4u);

    ctx->a = fast_nz(ctx, shifted);
    ctx->a = fast_lsr(ctx, ctx->a);
    shifted = ctx->a;
    if ((ctx->status & NATIVE_C) != 0u)
        render_actor_clear_h(ctx, 0u);

    /* The fast-path precondition proves bits 5..7 clear.  Still perform every
     * source shift so A, C, N and Z leave the routine exactly as they do in
     * the direct-C fallback. */
    ctx->a = fast_nz(ctx, shifted);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    shifted = ctx->a;
    ctx->a = fast_nz(ctx, shifted);
    ctx->a = fast_lsr(ctx, ctx->a);
    shifted = ctx->a;
    ctx->a = fast_nz(ctx, shifted);
    ctx->a = fast_lsr(ctx, ctx->a);
}

bool vs_native_fast_render_actor(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    const uint8_t *prg;
    uint8_t slot;
    uint8_t sprite;
    uint8_t id;
    uint8_t state;
    uint8_t next;
    uint8_t graphics;
    uint8_t bowser;

    if (ctx == NULL || ctx->ram == NULL || ctx->prg == NULL)
        return false;
    ram = ctx->ram;
    prg = ctx->prg;
    slot = ctx->x;
    if (slot >= 6u || ram[8] != slot || (ram[0x03d1] & 0xe0u) != 0u)
        return false;
    sprite = ram[0x06e5u + slot];
    if ((sprite & 3u) != 0u || sprite > 232u)
        return false;

    ram[2] = ram[0xcfu + slot];
    ram[5] = ram[0x03ae];
    ram[0xeb] = sprite;
    ram[0x0109] = 0u;
    ram[3] = ram[0x46u + slot];
    ram[4] = ram[0x03c5u + slot];
    id = ram[0x16u + slot];

    if (id == 0x0du && (ram[0x58u + slot] & 0x80u) == 0u &&
        ram[0x078au + slot] != 0u) {
        ctx->a = id;
        ctx->y = fast_nz(ctx, ram[0x078au + slot]);
        ctx->status |= NATIVE_C;
        return true;
    }

    state = ram[0x1eu + slot];
    next = (uint8_t)(state & 0x1fu);
    if (id == 0x35u) {
        next = 0u;
        ram[3] = 1u;
        id = 0x15u;
    }
    if (id == 0x33u) {
        --ram[2];
        ram[4] = (uint8_t)(3u | (ram[0x078au + slot] != 0u ? 0x20u : 0u));
        next = state = 0u;
        id = 8u;
    }
    if (id == 0x32u) {
        next = 3u;
        id = prg[0xe7d5u - 0x8000u + ram[0x070e]];
    }
    if (id == 0x0cu && (ram[0xa0u + slot] & 0x80u) == 0u)
        ram[0x0109] = 1u;
    bowser = ram[0x036a];
    if (bowser != 0u)
        id = bowser == 1u ? 0x16u : 0x17u;
    if (id == 6u) {
        if (ram[0x1eu + slot] >= 2u)
            next = 4u;
        if ((ram[0x1eu + slot] & 0x20u) == 0u && ram[0x0747] == 0u &&
            (ram[9] & 8u) == 0u)
            ram[3] ^= 3u;
    }

    ram[4] |= prg[0xe7b8u - 0x8000u + id];
    graphics = prg[0xe79du - 0x8000u + id];
    if (bowser != 0u) {
        if (bowser == 1u) {
            if ((ram[0x0363] & 0x80u) != 0u)
                graphics = 0xdeu;
        } else {
            if ((ram[0x0363] & 1u) != 0u)
                graphics = 0xe4u;
            if ((state & 0x20u) != 0u)
                ram[2] = (uint8_t)(ram[2] - 16u);
        }
        if ((state & 0x20u) != 0u)
            ram[0x0109] = graphics;
    } else {
        bool animate = false;
        bool check_animation = true;

        if (graphics == 0x24u) {
            if (next == 5u) {
                graphics = 0x30u;
                ram[3] = 2u;
                next = 5u;
            }
        } else if (graphics == 0x90u) {
            if ((state & 0x20u) == 0u && ram[0x078f] < 16u)
                graphics = 0x96u;
            check_animation = false;
        } else {
            if (id < 4u && next >= 2u) {
                graphics = 0x5au;
                if (id == 2u) {
                    graphics = 0x7eu;
                    ++ram[2];
                }
            }
            if (next == 4u) {
                graphics = 0x72u;
                ++ram[2];
                if (id != 2u) {
                    graphics = 0x66u;
                    ++ram[2];
                }
                if (id == 6u) {
                    graphics = 0x54u;
                    if ((state & 0x20u) == 0u) {
                        graphics = 0x8au;
                        --ram[2];
                    }
                }
            }
        }

        if (check_animation) {
            uint8_t timer = ram[0x0796u + slot];
            bool check_frame = true;

            if (id == 5u) {
                if (state != 0u) {
                    if ((state & 8u) == 0u)
                        check_frame = false;
                    else
                        graphics = 0xb4u;
                }
            } else if (graphics != 0x48u) {
                if (timer >= 5u) {
                    check_frame = false;
                } else if (graphics == 0x3cu) {
                    check_frame = false;
                    if (timer != 1u) {
                        ram[2] = (uint8_t)(ram[2] + 3u);
                        animate = true;
                    }
                }
            }
            if (check_frame && id != 6u && id != 8u && id != 0x0cu &&
                id < 0x18u) {
                if (id == 0x15u) {
                    if (ram[0x075f] < 7u) {
                        graphics = 0xa2u;
                        next = 3u;
                    }
                } else if ((ram[9] & prg[0xe7d3u - 0x8000u]) == 0u) {
                    animate = true;
                }
            }
            if (animate && (state & 0xa0u) == 0u && ram[0x0747] == 0u)
                graphics = (uint8_t)(graphics + 6u);
        }
        if ((state & 0x20u) != 0u && id >= 4u) {
            ram[0x0109] = 1u;
            next = 0u;
        }
    }

    ram[0xed] = state;
    ram[0xec] = next;
    ram[0xef] = id;
    ctx->x = graphics;
    render_actor_tiles(ctx);
    ctx->y = sprite;

    if (id != 8u) {
        if (ram[0x0109] != 0u) {
            uint8_t attr = (uint8_t)(ram[0x0202u + sprite] | 0x80u);
            uint8_t swap = sprite;
            uint8_t saved_left;
            uint8_t saved_right;

            for (unsigned row = 0u; row < 6u; ++row)
                ram[0x0202u + sprite + row * 4u] = attr;
            if (id != 5u && id != 0x11u && id < 0x15u) {
                ctx->a = sprite;
                ctx->status &= (uint8_t)~NATIVE_C;
                fast_adc(ctx, 8u);
                swap = ctx->a;
            }
            saved_left = ram[0x0201u + swap];
            saved_right = ram[0x0205u + swap];
            ram[0x0201u + swap] = ram[0x0211u + sprite];
            ram[0x0205u + swap] = ram[0x0215u + sprite];
            ram[0x0215u + sprite] = saved_right;
            ram[0x0211u + sprite] = saved_left;
        }
        if (bowser == 0u && id != 5u) {
            bool mirror = id == 7u || id == 0x0du || id == 0x0cu;
            if (!mirror && !(id == 0x12u && next != 5u)) {
                if (id == 0x15u)
                    ram[0x0216u + sprite] = 0x42u;
                mirror = next >= 2u;
            }
            if (mirror) {
                uint8_t left = (uint8_t)(ram[0x0202u + sprite] & 0xa3u);
                uint8_t right = (uint8_t)(left | 0x40u |
                    (next == 5u ? 0x80u : 0u));
                for (unsigned row = 0u; row < 3u; ++row) {
                    ram[0x0202u + sprite + row * 8u] = left;
                    ram[0x0206u + sprite + row * 8u] = right;
                }
                if (next == 4u) {
                    left |= 0x80u;
                    right = (uint8_t)(left | 0x40u);
                    ram[0x020au + sprite] = left;
                    ram[0x0212u + sprite] = left;
                    ram[0x020eu + sprite] = right;
                    ram[0x0216u + sprite] = right;
                }
            }
            if (id == 0x11u) {
                if (ram[0x0109] == 0u) {
                    ram[0x0212u + sprite] &= 0x81u;
                    ram[0x0216u + sprite] |= 0x41u;
                    if (ram[0x078f] < 16u) {
                        ram[0x020eu + sprite] = ram[0x0216u + sprite];
                        ram[0x020au + sprite] =
                            (uint8_t)(ram[0x0216u + sprite] & 0x81u);
                    }
                } else {
                    ram[0x0202u + sprite] &= 0x81u;
                    ram[0x0206u + sprite] |= 0x41u;
                }
            } else if (id >= 0x18u) {
                ram[0x020au + sprite] = 0x82u;
                ram[0x0212u + sprite] = 0x82u;
                ram[0x020eu + sprite] = 0xc2u;
                ram[0x0216u + sprite] = 0xc2u;
            }
        }
    }

    render_actor_cleanup(ctx);
    return true;
}
