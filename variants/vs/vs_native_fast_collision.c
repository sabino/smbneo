#include "vs_native_fast_collision.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define SMBNEO_FAST_INLINE static inline __attribute__((always_inline))
#else
#define SMBNEO_FAST_INLINE static inline
#endif

enum {
    FAST_RAM_SIZE = 0x0800,
    FAST_MAX_OBJECT_INDEX = 21,
    FAST_MAX_ACTOR_SLOT = 20,
    FAST_MAX_COL_BOX_INDEX = 12,
    FAST_MAX_REL_INDEX = 6,
    FAST_MAX_BG_OFFSET = 27
};

SMBNEO_FAST_INLINE bool fast_ready(const SmbNeoNativeContext *ctx) {
    return ctx != NULL && ctx->ram != NULL && ctx->prg != NULL &&
        ctx->suspended == 0u && ctx->unsupported == 0u;
}

SMBNEO_FAST_INLINE uint8_t fast_prg(
    const SmbNeoNativeContext *ctx,
    uint16_t address
) {
    return ctx->prg[(uint16_t)(address - 0x8000u)];
}

SMBNEO_FAST_INLINE uint8_t fast_nz(
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

SMBNEO_FAST_INLINE void fast_adc(
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
    ctx->a = fast_nz(ctx, result);
}

SMBNEO_FAST_INLINE uint8_t fast_asl(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u)
    );
    return fast_nz(ctx, (uint8_t)(value << 1));
}

SMBNEO_FAST_INLINE uint8_t fast_lsr(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    return fast_nz(ctx, (uint8_t)(value >> 1));
}

bool vs_native_fast_col_box_proc(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    uint8_t initial_x;
    uint8_t box_offset;
    uint8_t shape_offset;
    uint8_t rel_x;
    uint8_t rel_y;
    uint8_t last_operand;
    uint8_t last_result;
    uint8_t initial_status;
    unsigned sum;

    if (!fast_ready(ctx) || ctx->x > FAST_MAX_COL_BOX_INDEX ||
        ctx->y > FAST_MAX_REL_INDEX)
        return false;

    ram = ctx->ram;
    initial_x = ctx->x;
    initial_status = ctx->status;
    rel_y = ram[0x03b8u + ctx->y];
    rel_x = ram[0x03adu + ctx->y];
    box_offset = (uint8_t)(initial_x << 2);
    shape_offset = (uint8_t)(ram[0x0499u + initial_x] << 2);

    ram[0x00u] = initial_x;
    ram[0x02u] = rel_y;
    ram[0x01u] = rel_x;

    sum = (unsigned)rel_x + fast_prg(ctx,
        (uint16_t)(0xe153u + shape_offset));
    ram[0x04acu + box_offset] = (uint8_t)sum;
    sum = (unsigned)rel_x + fast_prg(ctx,
        (uint16_t)(0xe155u + shape_offset));
    ram[0x04aeu + box_offset] = (uint8_t)sum;

    shape_offset = (uint8_t)(shape_offset + 1u);
    box_offset = (uint8_t)(box_offset + 1u);
    sum = (unsigned)rel_y + fast_prg(ctx,
        (uint16_t)(0xe153u + shape_offset));
    ram[0x04acu + box_offset] = (uint8_t)sum;
    last_operand = fast_prg(ctx, (uint16_t)(0xe155u + shape_offset));
    sum = (unsigned)rel_y + last_operand;
    last_result = (uint8_t)sum;
    ram[0x04aeu + box_offset] = last_result;

    ctx->a = (uint8_t)(initial_x << 2);
    ctx->y = ctx->a;
    ctx->x = initial_x;
    ctx->status = (uint8_t)(
        (initial_status & (uint8_t)~(NATIVE_C | NATIVE_V |
                                     NATIVE_N | NATIVE_Z)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(rel_y ^ last_operand) & (rel_y ^ last_result) & 0x80u) != 0u
            ? NATIVE_V : 0u) |
        (initial_x & NATIVE_N) |
        (initial_x == 0u ? NATIVE_Z : 0u)
    );
    return true;
}

bool vs_native_fast_col_box_buffer(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    uint8_t initial_a;
    uint8_t initial_x;
    uint8_t initial_y;
    uint8_t initial_status;
    uint8_t adjusted_x;
    uint8_t column;
    uint8_t pointer_low;
    uint8_t pointer_high;
    uint8_t y_before_sub;
    uint8_t adjusted_y;
    uint8_t tile;
    uint8_t selected_coordinate;
    uint16_t pointer;
    uint16_t tile_address;
    unsigned x_sum;

    if (!fast_ready(ctx) || ctx->x > FAST_MAX_OBJECT_INDEX ||
        ctx->y > FAST_MAX_BG_OFFSET)
        return false;

    ram = ctx->ram;
    initial_a = ctx->a;
    initial_x = ctx->x;
    initial_y = ctx->y;
    initial_status = ctx->status;

    /* Preflight the complete direct-RAM access before the first write.  The
     * verified cartridge maps both scenery columns into the $05xx buffer;
     * an altered table can therefore fall back without partial mutation. */
    x_sum = (unsigned)fast_prg(ctx, (uint16_t)(0xe306u + initial_y)) +
        ram[(uint8_t)(0x86u + initial_x)];
    adjusted_x = (uint8_t)x_sum;
    column = (uint8_t)(
        (((ram[(uint8_t)(0x6du + initial_x)] +
           (x_sum > 255u ? 1u : 0u)) & 1u) << 4) |
        (adjusted_x >> 4)
    );
    pointer_low = (uint8_t)(fast_prg(ctx,
        (uint16_t)(0xab10u + (column >> 4))) + (column & 0x0fu));
    pointer_high = fast_prg(ctx,
        (uint16_t)(0xab12u + (column >> 4)));
    pointer = (uint16_t)(pointer_low | ((uint16_t)pointer_high << 8));
    y_before_sub = (uint8_t)(
        ram[(uint8_t)(0xceu + initial_x)] +
        fast_prg(ctx, (uint16_t)(0xe322u + initial_y))
    );
    y_before_sub &= 0xf0u;
    adjusted_y = (uint8_t)(y_before_sub - 0x20u);
    tile_address = (uint16_t)(pointer + adjusted_y);
    if ((pointer & 0xff00u) != 0x0500u || tile_address >= FAST_RAM_SIZE)
        return false;

    tile = ram[tile_address];
    selected_coordinate = ram[(uint8_t)(
        (initial_a == 0u ? 0xceu : 0x86u) + initial_x)];

    ram[0x05u] = adjusted_x;
    ram[0x06u] = pointer_low;
    ram[0x07u] = pointer_high;
    ram[0x02u] = adjusted_y;
    ram[0x03u] = tile;
    ram[0x04u] = (uint8_t)(selected_coordinate & 0x0fu);

    ctx->a = tile;
    ctx->x = initial_x;
    ctx->y = initial_y;
    ctx->status = (uint8_t)(
        (initial_status & (uint8_t)~(NATIVE_C | NATIVE_V |
                                     NATIVE_N | NATIVE_Z)) |
        (y_before_sub >= 0x20u ? NATIVE_C : 0u) |
        (((y_before_sub ^ 0x20u) & (y_before_sub ^ adjusted_y) & 0x80u)
            != 0u ? NATIVE_V : 0u) |
        (tile & NATIVE_N) |
        (tile == 0u ? NATIVE_Z : 0u)
    );
    return true;
}

/*
 * Compute one two-sided off-screen mask directly from object geometry.  The
 * x/y tables remain runtime cartridge data; the fast routine only removes
 * the translated instruction/control-flow overhead.
 */
static void pos_bits_axis(
    SmbNeoNativeContext *ctx,
    bool vertical
) {
    uint8_t *ram = ctx->ram;
    uint8_t object_index = ctx->x;
    uint8_t side = 1u;
    uint8_t bits;
    uint8_t overflow = 0u;
    uint8_t preserved = ctx->status;

    for (;;) {
        uint8_t edge_low = vertical
            ? fast_prg(ctx, (uint16_t)(0xf19cu + side))
            : ram[0x071cu + side];
        uint8_t object_low = ram[(uint8_t)(
            (vertical ? 0xceu : 0x86u) + object_index)];
        uint8_t low_difference = (uint8_t)(edge_low - object_low);
        uint8_t borrow = edge_low < object_low ? 1u : 0u;
        uint8_t edge_high = vertical ? 1u : ram[0x071au + side];
        uint8_t object_high = ram[(uint8_t)(
            (vertical ? 0xb5u : 0x6du) + object_index)];
        uint8_t high_difference = (uint8_t)(
            edge_high - object_high - borrow);
        uint16_t defaults = vertical ? 0xf199u : 0xf158u;
        uint16_t bit_table = vertical ? 0xf190u : 0xf148u;
        uint8_t limit = vertical ? 0x20u : 0x38u;
        uint8_t current = vertical ? 0x04u : 0x08u;
        uint8_t table_index = fast_prg(ctx,
            (uint16_t)(defaults + side));

        ram[0x07u] = low_difference;
        overflow = (uint8_t)(
            ((edge_high ^ object_high) &
             (edge_high ^ high_difference) & 0x80u) != 0u);
        if ((high_difference & 0x80u) == 0u) {
            table_index = fast_prg(ctx,
                (uint16_t)(defaults + side + 1u));
            if (high_difference == 0u) {
                ram[0x06u] = limit;
                ram[0x05u] = current;
                if (low_difference < limit) {
                    table_index = (uint8_t)(
                        (low_difference >> 3) +
                        (side == 0u ? current : 0u));
                    if (side == 0u)
                        overflow = 0u;
                }
            }
        }

        bits = fast_prg(ctx, (uint16_t)(bit_table + table_index));
        if (bits != 0u)
            break;
        side = (uint8_t)(side - 1u);
        if ((side & 0x80u) != 0u)
            break;
    }

    ram[0x04u] = object_index;
    ctx->a = bits;
    ctx->x = object_index;
    ctx->y = side;
    ctx->status = (uint8_t)(
        (preserved & (uint8_t)~(NATIVE_C | NATIVE_V |
                                NATIVE_N | NATIVE_Z)) |
        NATIVE_C |
        (overflow != 0u ? NATIVE_V : 0u) |
        ((bits != 0u ? bits : side) & NATIVE_N) |
        ((bits == 0u && side == 0u) ? NATIVE_Z : 0u)
    );
}

bool vs_native_fast_pos_bits_get_do_x(SmbNeoNativeContext *ctx) {
    if (!fast_ready(ctx) || ctx->x > FAST_MAX_OBJECT_INDEX)
        return false;
    pos_bits_axis(ctx, false);
    return true;
}

static void pos_bits_proc(SmbNeoNativeContext *ctx) {
    pos_bits_axis(ctx, false);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->a = fast_lsr(ctx, ctx->a);
    ctx->ram[0x00u] = ctx->a;
    pos_bits_axis(ctx, true);
}

bool vs_native_fast_pos_bits_proc(SmbNeoNativeContext *ctx) {
    if (!fast_ready(ctx) || ctx->x > FAST_MAX_OBJECT_INDEX)
        return false;
    pos_bits_proc(ctx);
    return true;
}

bool vs_native_fast_pos_bits_get_actor(SmbNeoNativeContext *ctx) {
    uint8_t *ram;
    uint8_t saved_y;

    if (!fast_ready(ctx) || ctx->x > FAST_MAX_ACTOR_SLOT)
        return false;

    ram = ctx->ram;
    ctx->a = fast_nz(ctx, 0x01u);
    ctx->y = fast_nz(ctx, 0x01u);
    ram[0x00u] = ctx->x;
    ctx->status &= (uint8_t)~NATIVE_C;
    fast_adc(ctx, ram[0x00u]);
    ctx->x = fast_nz(ctx, ctx->a);
    ctx->a = fast_nz(ctx, ctx->y);
    saved_y = ctx->a;

    pos_bits_proc(ctx);
    ctx->a = fast_asl(ctx, ctx->a);
    ctx->a = fast_asl(ctx, ctx->a);
    ctx->a = fast_asl(ctx, ctx->a);
    ctx->a = fast_asl(ctx, ctx->a);
    ctx->a = fast_nz(ctx, (uint8_t)(ctx->a | ram[0x00u]));
    ram[0x00u] = ctx->a;
    ctx->a = fast_nz(ctx, saved_y);
    ctx->y = fast_nz(ctx, ctx->a);
    ctx->a = fast_nz(ctx, ram[0x00u]);
    ram[0x03d0u + ctx->y] = ctx->a;
    ctx->x = fast_nz(ctx, ram[0x08u]);
    return true;
}
