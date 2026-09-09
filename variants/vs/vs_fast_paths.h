#ifndef SMBNEO_VS_FAST_PATHS_H
#define SMBNEO_VS_FAST_PATHS_H
#include "vs_cpu.h"

static inline void vs_fast_return(VsCpu *c) {
    uint16_t address = vs_pop(c);
    address |= (uint16_t)vs_pop(c) << 8;
    c->pc = (uint16_t)(address + 1u);
}

/* Recognized RAM-only stride-four loop. Other Y residues never terminate in
 * the source loop, so leave them to the bounded instruction implementation. */
static inline int vs_fast_stride_fill(VsCpu *c, unsigned base, uint8_t fill) {
    if (c->y & 3u) return 0;
    for (unsigned i = c->y; i < 256; i += 4)
        c->bus->ram[(base + i) & 0x7ff] = fill;
    c->a = fill;
    c->y = vs_nz(c, 0);
    return 1;
}

/* Exact semantic sprite-row operation. This edits emulated OAM in ordinary
 * RAM, never physical Neo Geo VRAM. The translator validates the entire source
 * instruction body before selecting this helper; unknown bodies fall back. */
static inline void vs_fast_render_pair(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    unsigned offset = c->y;
    unsigned next = offset + 8;
    uint8_t flip = ram[3] & 2u;
    uint8_t attr = ram[4] | (flip ? 0x40u : 0);
    ram[0x201 + offset] = ram[flip ? 1 : 0];
    ram[0x205 + offset] = ram[flip ? 0 : 1];
    ram[0x202 + offset] = ram[0x206 + offset] = attr;
    ram[0x200 + offset] = ram[0x204 + offset] = ram[2];
    ram[0x203 + offset] = ram[5];
    ram[0x207 + offset] = (uint8_t)(ram[5] + 8);
    ram[2] = (uint8_t)(ram[2] + 8);
    c->a = c->y = (uint8_t)next;
    c->p = (c->p & ~(VS_C | VS_V)) | (next > 255 ? VS_C : 0)
         | ((offset >= 120 && offset < 128) ? VS_V : 0);
    c->x = vs_nz(c, (uint8_t)(c->x + 2));
}

static inline void vs_fast_render_pair_right(VsCpu *c) {
    c->bus->ram[1] = c->a;
    vs_fast_render_pair(c);
}

/* Player rows are four two-sprite pairs in ordinary gameplay.  Folding the
 * reviewed loop avoids re-entering the translated dispatcher for every tile
 * while retaining the balanced JSR writes that remain observable in page 1. */
static inline int vs_fast_render_player_tiles(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t rows = ram[7];

    if (rows == 0u || rows > 4u)
        return 0;
    do {
        c->a = vs_nz(c, c->bus->prg[0xed74u - 0x8000u + c->x]);
        ram[0] = c->a;
        c->a = vs_nz(c, c->bus->prg[0xed75u - 0x8000u + c->x]);
        /* JSR render_chr_pair at $ef49 stores $ef4b. */
        vs_push(c, 0xef);
        vs_push(c, 0x4b);
        vs_fast_render_pair_right(c);
        (void)vs_pop(c);
        (void)vs_pop(c);
        rows = (uint8_t)(rows - 1u);
        ram[7] = rows;
        (void)vs_nz(c, rows);
    } while (rows != 0u);
    vs_fast_return(c);
    return 1;
}

static inline void vs_fast_render_actor_pair(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    const uint8_t *prg = c->bus->prg;

    ram[0] = prg[0xe69bu - 0x8000u + c->x];
    ram[1] = prg[0xe69cu - 0x8000u + c->x];
    vs_fast_render_pair(c);
}

static inline int vs_fast_misc_proc_inactive(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    for (unsigned slot = 0; slot < 9; ++slot)
        if (ram[0x2au + slot] != 0)
            return 0;
    ram[8] = 0;
    c->a = 0;
    c->x = 0xff;
    c->p = (c->p & ~(VS_N | VS_Z)) | VS_N;
    vs_fast_return(c);
    return 1;
}

static inline void vs_fast_table_jump(VsCpu *c) {
    uint8_t shifted = (uint8_t)(c->a << 1);
    c->p = (c->p & ~VS_C) | (c->a >> 7);
    c->a = c->y = vs_nz(c, shifted);
    c->a = vs_nz(c, vs_pop(c)); c->bus->ram[4] = c->a;
    c->a = vs_nz(c, vs_pop(c)); c->bus->ram[5] = c->a;
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    c->a = vs_nz(c, vs_rd(c, (uint16_t)(vs_zpword(c, 4) + c->y)));
    c->bus->ram[6] = c->a;
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    c->a = vs_nz(c, vs_rd(c, (uint16_t)(vs_zpword(c, 4) + c->y)));
    c->bus->ram[7] = c->a;
    c->pc = vs_zpword(c, 6);
}

static inline void vs_fast_nmi_oam_shuffle(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    c->y = vs_nz(c, ram[1870]);
    c->a = vs_nz(c, 40); ram[0] = c->a;
    c->x = vs_nz(c, 14);
    for (;;) {
        unsigned slot = c->x;
        c->a = vs_nz(c, ram[1764 + slot]);
        vs_cmp(c, c->a, ram[0]);
        if (c->p & VS_C) {
            c->y = vs_nz(c, ram[1760]);
            c->p &= ~VS_C;
            vs_adc(c, ram[1761 + c->y]);
            if (c->p & VS_C) {
                c->p &= ~VS_C;
                vs_adc(c, ram[0]);
            }
            ram[1764 + slot] = c->a;
        }
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        if (c->p & VS_N) break;
    }
    c->x = vs_nz(c, ram[1760]);
    c->x = vs_nz(c, (uint8_t)(c->x + 1));
    vs_cmp(c, c->x, 3);
    if (c->p & VS_Z) c->x = vs_nz(c, 0);
    ram[1760] = c->x;
    c->x = vs_nz(c, 8); c->y = vs_nz(c, 2);
    for (;;) {
        unsigned source = 1769 + c->y, target = 1777 + c->x;
        c->a = vs_nz(c, ram[source]); ram[target] = c->a;
        c->p &= ~VS_C; vs_adc(c, 8); ram[target + 1] = c->a;
        c->p &= ~VS_C; vs_adc(c, 8); ram[target + 2] = c->a;
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
        if (c->p & VS_N) break;
    }
}

static inline void vs_fast_lsr_a(VsCpu *c) {
    uint8_t value = c->a;
    c->p = (c->p & ~VS_C) | (value & 1u);
    c->a = vs_nz(c, (uint8_t)(value >> 1));
}

static inline void vs_fast_ror_a(VsCpu *c) {
    uint8_t value = c->a;
    uint8_t carry = (c->p & VS_C) != 0;
    c->p = (c->p & ~VS_C) | (value & 1u);
    c->a = vs_nz(c, (uint8_t)((value >> 1) | (carry << 7)));
}

static inline void vs_fast_asl_a(VsCpu *c) {
    uint8_t value = c->a;
    c->p = (c->p & ~VS_C) | (value >> 7);
    c->a = vs_nz(c, (uint8_t)(value << 1));
}

static inline void vs_fast_rol_a(VsCpu *c) {
    uint8_t value = c->a;
    uint8_t carry = (c->p & VS_C) != 0;
    c->p = (c->p & ~VS_C) | (value >> 7);
    c->a = vs_nz(c, (uint8_t)((value << 1) | carry));
}

static inline void vs_fast_pos_bits_diff(VsCpu *c, uint8_t return_low) {
    uint8_t *ram = c->bus->ram;

    /* Preserve the observable JSR/RTS stack writes from the source helper. */
    vs_push(c, 0xf1);
    vs_push(c, return_low);
    ram[5] = c->a;
    c->a = vs_nz(c, ram[7]);
    vs_cmp(c, c->a, ram[6]);
    if (!(c->p & VS_C)) {
        vs_fast_lsr_a(c);
        vs_fast_lsr_a(c);
        vs_fast_lsr_a(c);
        c->a = vs_nz(c, c->a & 7u);
        vs_cmp(c, c->y, 1);
        if (!(c->p & VS_C))
            vs_adc(c, ram[5]);
        c->x = vs_nz(c, c->a);
    }
    (void)vs_pop(c);
    (void)vs_pop(c);
}

static inline void vs_fast_pos_bits_get_do_x_alias_exact(VsCpu *c) {
    static const uint8_t offsets[3] = {7, 15, 7};
    static const uint8_t bits[16] = {
        0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00,
        0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
    };
    uint8_t *ram = c->bus->ram;

    ram[4] = c->x;
    c->y = vs_nz(c, 1);
    for (;;) {
        c->a = vs_nz(c, ram[0x071c + c->y]);
        c->p |= VS_C;
        vs_adc(c, (uint8_t)~ram[(uint8_t)(0x86 + c->x)]);
        ram[7] = c->a;
        c->a = vs_nz(c, ram[0x071a + c->y]);
        vs_adc(c, (uint8_t)~ram[(uint8_t)(0x6d + c->x)]);
        c->x = vs_nz(c, offsets[c->y]);
        vs_cmp(c, c->a, 0);
        if (!(c->p & VS_N)) {
            c->x = vs_nz(c, offsets[c->y + 1]);
            vs_cmp(c, c->a, 1);
            if (c->p & VS_N) {
                c->a = vs_nz(c, 56); ram[6] = c->a;
                c->a = vs_nz(c, 8);
                vs_fast_pos_bits_diff(c, 0x82);
            }
        }
        c->a = vs_nz(c, bits[c->x]);
        c->x = vs_nz(c, ram[4]);
        vs_cmp(c, c->a, 0);
        if (!(c->p & VS_Z)) break;
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
        if (c->p & VS_N) break;
    }
}

static inline void vs_fast_pos_bits_get_do_y_alias_exact(VsCpu *c) {
    static const uint8_t high[2] = {0xff, 0x00};
    static const uint8_t offsets[3] = {4, 0, 4};
    static const uint8_t bits[9] = {
        0x00, 0x08, 0x0c, 0x0e, 0x0f, 0x07, 0x03, 0x01, 0x00
    };
    uint8_t *ram = c->bus->ram;

    ram[4] = c->x;
    c->y = vs_nz(c, 1);
    for (;;) {
        c->a = vs_nz(c, high[c->y]);
        c->p |= VS_C;
        vs_adc(c, (uint8_t)~ram[(uint8_t)(0xce + c->x)]);
        ram[7] = c->a;
        c->a = vs_nz(c, 1);
        vs_adc(c, (uint8_t)~ram[(uint8_t)(0xb5 + c->x)]);
        c->x = vs_nz(c, offsets[c->y]);
        vs_cmp(c, c->a, 0);
        if (!(c->p & VS_N)) {
            c->x = vs_nz(c, offsets[c->y + 1]);
            vs_cmp(c, c->a, 1);
            if (c->p & VS_N) {
                c->a = vs_nz(c, 32); ram[6] = c->a;
                c->a = vs_nz(c, 4);
                vs_fast_pos_bits_diff(c, 0xc4);
            }
        }
        c->a = vs_nz(c, bits[c->x]);
        c->x = vs_nz(c, ram[4]);
        vs_cmp(c, c->a, 0);
        if (!(c->p & VS_Z)) break;
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
        if (c->p & VS_N) break;
    }
}

/* Compute the two screen-edge masks as geometry instead of replaying the
 * original 6502 instruction stream.  The scratch bytes, status register and
 * nested-JSR stack residue remain exact because other translated routines can
 * observe all of them. */
static inline uint8_t vs_fast_pos_bits_x_table(uint8_t index) {
    static const uint8_t table[16] = {
        0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00,
        0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
    };
    return table[index];
}

static inline uint8_t vs_fast_pos_bits_y_table(uint8_t index) {
    static const uint8_t table[9] = {
        0x00, 0x08, 0x0c, 0x0e, 0x0f, 0x07, 0x03, 0x01, 0x00
    };
    return table[index];
}

static inline void vs_fast_pos_bits_finish(
    VsCpu *c,
    uint8_t object_index,
    uint8_t side,
    uint8_t bits,
    uint8_t overflow
) {
    uint8_t nz = bits != 0u ? bits : side;

    c->a = bits;
    c->x = object_index;
    c->y = side;
    c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z)) | VS_C
        | (overflow ? VS_V : 0u) | (nz & VS_N)
        | (nz == 0u ? VS_Z : 0u);
}

static inline void vs_fast_pos_bits_get_do_x(VsCpu *c) {
    static const uint8_t defaults[3] = {7, 15, 7};
    uint8_t *ram = c->bus->ram;
    uint8_t object_index = c->x;
    uint8_t side = 1;
    uint8_t bits;
    uint8_t overflow = 0;

    /* Larger synthetic indices can make the zero-page object fields alias the
     * routine's own scratch bytes. Real object slots are all in this range;
     * retain the literal ordering for the alias cases. */
    if (object_index > 21u) {
        vs_fast_pos_bits_get_do_x_alias_exact(c);
        return;
    }
    ram[4] = object_index;
    for (;;) {
        uint8_t edge_pixel = ram[0x071cu + side];
        uint8_t object_pixel = ram[(uint8_t)(0x86u + object_index)];
        uint8_t pixel_diff = (uint8_t)(edge_pixel - object_pixel);
        uint8_t pixel_borrow = edge_pixel < object_pixel;
        uint8_t edge_page = ram[0x071au + side];
        uint8_t object_page = ram[(uint8_t)(0x6du + object_index)];
        uint8_t page_diff = (uint8_t)(edge_page - object_page - pixel_borrow);
        uint8_t table_index = defaults[side];

        ram[7] = pixel_diff;
        overflow = ((edge_page ^ page_diff) & (edge_page ^ object_page)
            & 0x80u) != 0u;
        if ((page_diff & 0x80u) == 0u) {
            table_index = defaults[side + 1u];
            if (page_diff == 0u) {
                ram[6] = 56;
                ram[5] = 8;
                ram[0x100u | c->s] = 0xf1;
                ram[0x100u | (uint8_t)(c->s - 1u)] = 0x82;
                if (pixel_diff < 56u) {
                    table_index = (uint8_t)((pixel_diff >> 3)
                        + (side == 0u ? 8u : 0u));
                    if (side == 0u)
                        overflow = 0;
                }
            }
        }
        bits = vs_fast_pos_bits_x_table(table_index);
        if (bits != 0u)
            break;
        side = (uint8_t)(side - 1u);
        if (side & 0x80u)
            break;
    }
    vs_fast_pos_bits_finish(c, object_index, side, bits, overflow);
}

static inline void vs_fast_pos_bits_get_do_y(VsCpu *c) {
    static const uint8_t edge_high[2] = {0xff, 0x00};
    static const uint8_t defaults[3] = {4, 0, 4};
    uint8_t *ram = c->bus->ram;
    uint8_t object_index = c->x;
    uint8_t side = 1;
    uint8_t bits;
    uint8_t overflow = 0;

    if (object_index > 21u) {
        vs_fast_pos_bits_get_do_y_alias_exact(c);
        return;
    }
    ram[4] = object_index;
    for (;;) {
        uint8_t edge_pixel = edge_high[side];
        uint8_t object_pixel = ram[(uint8_t)(0xceu + object_index)];
        uint8_t pixel_diff = (uint8_t)(edge_pixel - object_pixel);
        uint8_t pixel_borrow = edge_pixel < object_pixel;
        uint8_t object_high = ram[(uint8_t)(0xb5u + object_index)];
        uint8_t high_diff = (uint8_t)(1u - object_high - pixel_borrow);
        uint8_t table_index = defaults[side];

        ram[7] = pixel_diff;
        overflow = ((1u ^ high_diff) & (1u ^ object_high) & 0x80u) != 0u;
        if ((high_diff & 0x80u) == 0u) {
            table_index = defaults[side + 1u];
            if (high_diff == 0u) {
                ram[6] = 32;
                ram[5] = 4;
                ram[0x100u | c->s] = 0xf1;
                ram[0x100u | (uint8_t)(c->s - 1u)] = 0xc4;
                if (pixel_diff < 32u) {
                    table_index = (uint8_t)((pixel_diff >> 3)
                        + (side == 0u ? 4u : 0u));
                    if (side == 0u)
                        overflow = 0;
                }
            }
        }
        bits = vs_fast_pos_bits_y_table(table_index);
        if (bits != 0u)
            break;
        side = (uint8_t)(side - 1u);
        if (side & 0x80u)
            break;
    }
    vs_fast_pos_bits_finish(c, object_index, side, bits, overflow);
}

static inline void vs_fast_pos_bits_get(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    c->a = vs_nz(c, c->y);
    vs_push(c, c->a);
    /* JSR pos_bits_proc, then its JSR pos_bits_get_do_x. */
    vs_push(c, 0xf1); vs_push(c, 0x29);
    vs_push(c, 0xf1); vs_push(c, 0x3e);
    vs_fast_pos_bits_get_do_x(c);
    (void)vs_pop(c); (void)vs_pop(c);
    vs_fast_lsr_a(c); vs_fast_lsr_a(c);
    vs_fast_lsr_a(c); vs_fast_lsr_a(c);
    ram[0] = c->a;
    /* pos_bits_get_do_y is a tail call and returns from pos_bits_proc. */
    vs_fast_pos_bits_get_do_y(c);
    (void)vs_pop(c); (void)vs_pop(c);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    c->a = vs_nz(c, c->a | ram[0]);
    ram[0] = c->a;
    c->a = vs_nz(c, vs_pop(c));
    c->y = vs_nz(c, c->a);
    c->a = vs_nz(c, ram[0]);
    ram[(0x03d0u + c->y) & 0x7ff] = c->a;
    c->x = vs_nz(c, ram[8]);
}

static inline void vs_fast_joypad_read_do_alias_exact(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    c->y = vs_nz(c, 8);
    do {
        uint8_t old_carry;
        vs_push(c, c->a);
        c->a = vs_nz(c, vs_rd(c, (uint16_t)(0x4016 + c->x)));
        ram[0] = c->a;
        vs_fast_lsr_a(c);
        c->a = vs_nz(c, vs_pop(c));
        old_carry = (c->p & VS_C) != 0;
        c->p = (c->p & ~VS_C) | (c->a >> 7);
        c->a = vs_nz(c, (uint8_t)((c->a << 1) | old_carry));
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
    } while (!(c->p & VS_Z));
    ram[(0x06fcu + c->x) & 0x7ff] = c->a;
    vs_push(c, c->a);
    c->a = vs_nz(c, c->a & 0x30u);
    c->a = vs_nz(c, c->a & ram[(0x074au + c->x) & 0x7ff]);
    {
        uint8_t masked_zero = c->p & VS_Z;
        c->a = vs_nz(c, vs_pop(c));
        if (!masked_zero) {
            c->a = vs_nz(c, c->a & 0xcfu);
            ram[(0x06fcu + c->x) & 0x7ff] = c->a;
        } else {
            ram[(0x074au + c->x) & 0x7ff] = c->a;
        }
    }
}

static inline void vs_fast_joypad_read_do(VsCpu *c) {
    VsBus *bus = c->bus;
    uint8_t *ram = bus->ram;
    uint8_t port = c->x;
    uint8_t initial_a = c->a;
    uint8_t input, last_bit, output;

    if (port > 1) {
        vs_fast_joypad_read_do_alias_exact(c);
        return;
    }
    if (bus->strobe) {
        last_bit = bus->pads[port] & 1u;
        input = last_bit ? 0xffu : 0u;
        bus->latch[port] = (uint8_t)(bus->pads[port] >> 1);
    } else {
        uint8_t latched = bus->latch[port];
        last_bit = latched >> 7;
        latched = (uint8_t)(((latched & 0x55u) << 1) | ((latched >> 1) & 0x55u));
        latched = (uint8_t)(((latched & 0x33u) << 2) | ((latched >> 2) & 0x33u));
        input = (uint8_t)((latched << 4) | (latched >> 4));
        bus->latch[port] = 0;
    }
    ram[0] = port
        ? (uint8_t)(last_bit | (bus->dips & 0xfcu))
        : (uint8_t)(last_bit | bus->coin_service | ((bus->dips & 3u) << 3));
    ram[(0x06fcu + port) & 0x7ff] = input;
    /* All nine balanced PHA/PLA pairs finish with this same stack byte. */
    ram[0x100u | c->s] = input;
    output = input;
    if ((input & 0x30u & ram[(0x074au + port) & 0x7ff]) != 0) {
        output &= 0xcfu;
        ram[(0x06fcu + port) & 0x7ff] = output;
    } else {
        ram[(0x074au + port) & 0x7ff] = input;
    }
    c->a = output;
    c->y = 0;
    c->p = (c->p & ~(VS_C | VS_N | VS_Z))
        | (initial_a & 1u)
        | (output & VS_N)
        | (output == 0 ? VS_Z : 0);
}

static inline void vs_fast_col_box_buffer_alias_exact(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint16_t address;

    vs_push(c, c->a);
    ram[4] = c->y;
    c->a = vs_nz(c, c->bus->prg[0xe306 - 0x8000 + c->y]);
    c->p &= ~VS_C;
    vs_adc(c, ram[(uint8_t)(0x86 + c->x)]);
    ram[5] = c->a;
    c->a = vs_nz(c, ram[(uint8_t)(0x6d + c->x)]);
    vs_adc(c, 0);
    c->a = vs_nz(c, c->a & 1u);
    vs_fast_lsr_a(c);
    c->a = vs_nz(c, c->a | ram[5]);
    vs_fast_ror_a(c);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);

    /* Inline scenery_get_buffer_ptr, including JSR/RTS and PHA/PLA residue. */
    vs_push(c, 0xe3);
    vs_push(c, 0x62);
    vs_push(c, c->a);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    c->y = vs_nz(c, c->a);
    c->a = vs_nz(c, c->bus->prg[0xab12 - 0x8000 + c->y]);
    ram[7] = c->a;
    c->a = vs_nz(c, vs_pop(c));
    c->a = vs_nz(c, c->a & 15u);
    c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xab10 - 0x8000 + c->y]);
    ram[6] = c->a;
    (void)vs_pop(c);
    (void)vs_pop(c);

    c->y = vs_nz(c, ram[4]);
    c->a = vs_nz(c, ram[(uint8_t)(0xce + c->x)]);
    c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xe322 - 0x8000 + c->y]);
    c->a = vs_nz(c, c->a & 0xf0u);
    c->p |= VS_C;
    vs_adc(c, (uint8_t)~32u);
    ram[2] = c->a;
    c->y = vs_nz(c, c->a);
    address = (uint16_t)(ram[6] | ((uint16_t)ram[7] << 8));
    c->a = vs_nz(c, vs_rd(c, (uint16_t)(address + c->y)));
    ram[3] = c->a;
    c->y = vs_nz(c, ram[4]);
    c->a = vs_nz(c, vs_pop(c));
    if (c->p & VS_Z)
        c->a = vs_nz(c, ram[(uint8_t)(0xce + c->x)]);
    else
        c->a = vs_nz(c, ram[(uint8_t)(0x86 + c->x)]);
    c->a = vs_nz(c, c->a & 15u);
    ram[4] = c->a;
    c->a = vs_nz(c, ram[3]);
}

static inline void vs_fast_col_box_buffer(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t initial_a = c->a, initial_y = c->y;
    uint8_t adjusted_x, column, adjusted_y, tile;
    uint16_t sum, pointer;

    if (c->x > 21 || c->y > 27) {
        vs_fast_col_box_buffer_alias_exact(c);
        return;
    }
    sum = (uint16_t)c->bus->prg[0xe306 - 0x8000 + c->y]
        + ram[(uint8_t)(0x86 + c->x)];
    adjusted_x = (uint8_t)sum;
    ram[5] = adjusted_x;
    column = (uint8_t)((((ram[(uint8_t)(0x6d + c->x)]
        + (sum > 0xffu ? 1u : 0u)) & 1u) << 4) | (adjusted_x >> 4));
    pointer = (column & 0x10u ? 0x05d0u : 0x0500u) + (column & 15u);
    ram[6] = (uint8_t)pointer;
    ram[7] = (uint8_t)(pointer >> 8);

    sum = (uint16_t)ram[(uint8_t)(0xce + c->x)]
        + c->bus->prg[0xe322 - 0x8000 + c->y];
    c->a = (uint8_t)sum & 0xf0u;
    c->p |= VS_C;
    vs_adc(c, (uint8_t)~32u);
    adjusted_y = c->a;
    ram[2] = adjusted_y;
    tile = ram[(pointer + adjusted_y) & 0x7ff];
    ram[3] = tile;
    ram[4] = (initial_a == 0
        ? ram[(uint8_t)(0xce + c->x)]
        : ram[(uint8_t)(0x86 + c->x)]) & 15u;

    /* Net-balanced source stack traffic is still observable in page $01. */
    ram[0x100u | c->s] = initial_a;
    ram[0x100u | (uint8_t)(c->s - 1)] = 0xe3;
    ram[0x100u | (uint8_t)(c->s - 2)] = 0x62;
    ram[0x100u | (uint8_t)(c->s - 3)] = column;
    c->a = vs_nz(c, tile);
    c->y = initial_y;
}

/* NMI bookkeeping has no useful relationship with 6502 instruction timing on
 * Neo Geo. Preserve its state transition while removing the emulated loops. */
static inline void vs_fast_nmi_timers(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t first = 20;

    ram[1919] = (uint8_t)(ram[1919] - 1);
    if (ram[1919] & 0x80u) {
        ram[1919] = 20;
        first = 35;
    }
    for (int i = first; i >= 0; --i) {
        c->a = ram[1920 + i];
        if (c->a != 0)
            ram[1920 + i] = (uint8_t)(c->a - 1);
    }
    c->x = 0xff;
    ram[9] = (uint8_t)(ram[9] + 1);
    c->p = (c->p & ~(VS_N | VS_Z)) | (ram[9] & VS_N)
        | (ram[9] == 0 ? VS_Z : 0);
}

static inline void vs_fast_nmi_rand(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t carry;

    c->a = ram[1959] & 2u;
    ram[0] = c->a;
    c->a = (ram[1960] & 2u) ^ ram[0];
    carry = c->a != 0;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned address = 1959u + i;
        uint8_t value = ram[address];
        ram[address] = (uint8_t)((value >> 1) | (carry << 7));
        carry = value & 1u;
    }
    c->x = 7;
    c->y = 0;
    c->p = (c->p & ~(VS_C | VS_N | VS_Z)) | carry | VS_Z;
}

static inline void vs_fast_nmi_hblank_delay(VsCpu *c) {
    c->y = vs_nz(c, 0);
}

static inline void vs_fast_stats_score_hi_check_do(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    if (c->x == 5 || c->x == 11) {
        uint8_t carry = 1, overflow = 0;
        c->y = 5;
        for (unsigned digit = 0; digit < 6; ++digit) {
            uint8_t player = ram[(0x07ddu + c->x) & 0x7ff];
            uint8_t top = ram[0x07d7u + c->y];
            uint16_t difference = (uint16_t)player - top - (carry ? 0u : 1u);
            c->a = (uint8_t)difference;
            carry = difference <= 0xffu;
            overflow = ((player ^ c->a) & (player ^ top) & 0x80u) != 0;
            c->x = (uint8_t)(c->x - 1);
            c->y = (uint8_t)(c->y - 1);
        }
        if (!carry) {
            c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z))
                | (overflow ? VS_V : 0) | VS_N;
            return;
        }
        c->x = (uint8_t)(c->x + 1);
        c->y = (uint8_t)(c->y + 1);
        for (unsigned digit = 0; digit < 6; ++digit) {
            c->a = ram[(0x07ddu + c->x) & 0x7ff];
            ram[0x07d7u + c->y] = c->a;
            c->x = (uint8_t)(c->x + 1);
            c->y = (uint8_t)(c->y + 1);
        }
        c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z))
            | VS_C | VS_Z | (overflow ? VS_V : 0);
        return;
    }

    c->y = vs_nz(c, 5);
    c->p |= VS_C;
    do {
        c->a = vs_nz(c, ram[(0x07ddu + c->x) & 0x7ff]);
        vs_adc(c, (uint8_t)~ram[(0x07d7u + c->y) & 0x7ff]);
        c->x = vs_nz(c, (uint8_t)(c->x - 1));
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
    } while (!(c->p & VS_N));
    if (!(c->p & VS_C)) return;
    c->x = vs_nz(c, (uint8_t)(c->x + 1));
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    do {
        c->a = vs_nz(c, ram[(0x07ddu + c->x) & 0x7ff]);
        ram[(0x07d7u + c->y) & 0x7ff] = c->a;
        c->x = vs_nz(c, (uint8_t)(c->x + 1));
        c->y = vs_nz(c, (uint8_t)(c->y + 1));
        vs_cmp(c, c->y, 6);
    } while (!(c->p & VS_C));
}


static inline void vs_fast_motion_x_do_alias_exact(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    c->a = vs_nz(c, ram[(uint8_t)(0x57 + c->x)]);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    ram[1] = c->a;
    c->a = vs_nz(c, ram[(uint8_t)(0x57 + c->x)]);
    vs_fast_lsr_a(c); vs_fast_lsr_a(c);
    vs_fast_lsr_a(c); vs_fast_lsr_a(c);
    vs_cmp(c, c->a, 8);
    if (c->p & VS_C)
        c->a = vs_nz(c, c->a | 0xf0u);
    ram[0] = c->a;
    c->y = vs_nz(c, 0);
    vs_cmp(c, c->a, 0);
    if (c->p & VS_N)
        c->y = vs_nz(c, (uint8_t)(c->y - 1));
    ram[2] = c->y;
    c->a = vs_nz(c, ram[(0x0400u + c->x) & 0x7ff]);
    c->p &= ~VS_C;
    vs_adc(c, ram[1]);
    ram[(0x0400u + c->x) & 0x7ff] = c->a;
    c->a = vs_nz(c, 0);
    vs_fast_rol_a(c);
    vs_push(c, c->a);
    vs_fast_ror_a(c);
    c->a = vs_nz(c, ram[(uint8_t)(0x86 + c->x)]);
    vs_adc(c, ram[0]);
    ram[(uint8_t)(0x86 + c->x)] = c->a;
    c->a = vs_nz(c, ram[(uint8_t)(0x6d + c->x)]);
    vs_adc(c, ram[2]);
    ram[(uint8_t)(0x6d + c->x)] = c->a;
    c->a = vs_nz(c, vs_pop(c));
    c->p &= ~VS_C;
    vs_adc(c, ram[0]);
}

static inline void vs_fast_motion_x_do(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t speed, fraction, pixel_delta, page_delta, force_carry, result;
    uint16_t sum;

    if (c->x > 6) {
        vs_fast_motion_x_do_alias_exact(c);
        return;
    }
    speed = ram[0x57u + c->x];
    fraction = (uint8_t)(speed << 4);
    pixel_delta = (uint8_t)(((speed >> 4) ^ 8u) - 8u);
    page_delta = (uint8_t)-(pixel_delta >> 7);
    ram[1] = fraction;
    ram[0] = pixel_delta;
    ram[2] = page_delta;
    c->y = page_delta;

    sum = (uint16_t)ram[0x0400u + c->x] + fraction;
    ram[0x0400u + c->x] = (uint8_t)sum;
    force_carry = sum > 0xffu;
    ram[0x100u | c->s] = force_carry;
    sum = (uint16_t)ram[0x86u + c->x] + pixel_delta + force_carry;
    ram[0x86u + c->x] = (uint8_t)sum;
    sum = (uint16_t)ram[0x6du + c->x] + page_delta
        + (sum > 0xffu ? 1u : 0u);
    ram[0x6du + c->x] = (uint8_t)sum;

    result = (uint8_t)(force_carry + pixel_delta);
    c->a = result;
    c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z))
        | (force_carry && pixel_delta == 0xffu ? VS_C : 0)
        | (result & VS_N) | (result == 0 ? VS_Z : 0);
}

/* Fold the two call-site wrappers around the horizontal motion formula. */
static inline void vs_fast_motion_x(VsCpu *c) {
    c->x = vs_nz(c, (uint8_t)(c->x + 1u));
    /* JSR motion_x_do at $be12 stores $be14. */
    vs_push(c, 0xbe);
    vs_push(c, 0x14);
    vs_fast_motion_x_do(c);
    (void)vs_pop(c);
    (void)vs_pop(c);
    c->x = vs_nz(c, c->bus->ram[8]);
    vs_fast_return(c);
}

static inline void vs_fast_motion_x_player(VsCpu *c) {
    c->a = vs_nz(c, c->bus->ram[0x070e]);
    if (c->a == 0u) {
        c->x = vs_nz(c, c->a);
        vs_fast_motion_x_do(c);
    }
    vs_fast_return(c);
}

/* Relative coordinates are geometry, not an emulated timing primitive.  Keep
 * the exact zero-page aliases, flag residue and nested JSR stack writes. */
static inline void vs_fast_pos_calc_x_rel_do(
    VsCpu *c,
    uint8_t object_index,
    uint8_t relative_index
) {
    uint8_t *ram = c->bus->ram;

    c->a = vs_nz(c, ram[(uint8_t)(0xceu + object_index)]);
    ram[0x03b8u + relative_index] = c->a;
    c->a = vs_nz(c, ram[(uint8_t)(0x86u + object_index)]);
    c->p |= VS_C;
    vs_adc(c, (uint8_t)~ram[0x071c]);
    ram[0x03adu + relative_index] = c->a;
}

static inline void vs_fast_pos_calc_x_rel_player(VsCpu *c) {
    c->x = vs_nz(c, 0);
    c->y = vs_nz(c, 0);
    /* JSR pos_calc_x_rel_do at $f0a7 stores $f0a9. */
    vs_push(c, 0xf0);
    vs_push(c, 0xa9);
    vs_fast_pos_calc_x_rel_do(c, c->x, c->y);
    (void)vs_pop(c);
    (void)vs_pop(c);
    c->x = vs_nz(c, c->bus->ram[8]);
    vs_fast_return(c);
}

static inline void vs_fast_pos_calc_x_rel_actor(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t initial_x = c->x;

    c->a = vs_nz(c, 1);
    c->y = vs_nz(c, 1);
    ram[0] = initial_x;
    c->p &= ~VS_C;
    vs_adc(c, ram[0]);
    c->x = vs_nz(c, c->a);
    /* JSR pos_calc_x_rel_do at $f0d0 stores $f0d2. */
    vs_push(c, 0xf0);
    vs_push(c, 0xd2);
    vs_fast_pos_calc_x_rel_do(c, c->x, c->y);
    (void)vs_pop(c);
    (void)vs_pop(c);
    c->x = vs_nz(c, ram[8]);
    vs_fast_return(c);
}

static inline void vs_fast_actor_erase(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;

    c->a = vs_nz(c, 0);
    ram[(uint8_t)(0x0fu + slot)] = 0;
    ram[(uint8_t)(0x16u + slot)] = 0;
    ram[(uint8_t)(0x1eu + slot)] = 0;
    ram[(0x0110u + slot) & 0x07ffu] = 0;
    ram[(0x0796u + slot) & 0x07ffu] = 0;
    ram[(0x0125u + slot) & 0x07ffu] = 0;
    ram[(0x03c5u + slot) & 0x07ffu] = 0;
    ram[(0x078au + slot) & 0x07ffu] = 0;
}

/* The normal actor slots all use this boundary check once per game tick.  It
 * contains an intentional 6502 carry chain across both screen edges, so keep
 * the operations in source order instead of simplifying the arithmetic. */
static inline int vs_fast_actor_oob_proc(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;
    uint8_t actor_id;

    /* Low synthetic stack positions can overlap the actor scratch arrays and
     * deliberately redirect the nested RTS; retain the translated fallback. */
    if (slot >= 6u || c->s < 0x40u)
        return 0;
    actor_id = ram[(uint8_t)(0x16u + slot)];
    c->a = vs_nz(c, actor_id);
    vs_cmp(c, c->a, 0x14);
    if (c->p & VS_Z) {
        vs_fast_return(c);
        return 1;
    }

    c->a = vs_nz(c, ram[0x071c]);
    c->y = vs_nz(c, actor_id);
    vs_cmp(c, c->y, 0x05);
    if (!(c->p & VS_Z))
        vs_cmp(c, c->y, 0x0d);
    if (c->p & VS_Z)
        vs_adc(c, 0x38);
    vs_adc(c, (uint8_t)~0x48u);
    ram[1] = c->a;

    c->a = vs_nz(c, ram[0x071a]);
    vs_adc(c, 0xff);
    ram[0] = c->a;
    c->a = vs_nz(c, ram[0x071d]);
    vs_adc(c, 0x48);
    ram[3] = c->a;
    c->a = vs_nz(c, ram[0x071b]);
    vs_adc(c, 0);
    ram[2] = c->a;

    c->a = vs_nz(c, ram[(uint8_t)(0x87u + slot)]);
    vs_cmp(c, c->a, ram[1]);
    c->a = vs_nz(c, ram[(uint8_t)(0x6eu + slot)]);
    vs_adc(c, (uint8_t)~ram[0]);
    if (c->p & VS_N)
        goto erase;

    c->a = vs_nz(c, ram[(uint8_t)(0x87u + slot)]);
    vs_cmp(c, c->a, ram[3]);
    c->a = vs_nz(c, ram[(uint8_t)(0x6eu + slot)]);
    vs_adc(c, (uint8_t)~ram[2]);
    if (c->p & VS_N) {
        vs_fast_return(c);
        return 1;
    }

    c->a = vs_nz(c, ram[(uint8_t)(0x1eu + slot)]);
    vs_cmp(c, c->a, 0x05);
    if (c->p & VS_Z) {
        vs_fast_return(c);
        return 1;
    }
    for (unsigned i = 0; i < 4; ++i) {
        static const uint8_t immune_ids[4] = {0x0d, 0x30, 0x31, 0x32};
        vs_cmp(c, c->y, immune_ids[i]);
        if (c->p & VS_Z) {
            vs_fast_return(c);
            return 1;
        }
    }

erase:
    /* JSR actor_erase at $d615 stores $d617. */
    vs_push(c, 0xd6);
    vs_push(c, 0x17);
    vs_fast_actor_erase(c);
    vs_fast_return(c);
    /* Synthetic stack positions can alias actor_erase's ordinary RAM fields.
     * In that case the real RTS follows the overwritten address too. */
    if (c->pc != 0xd618u)
        return 1;
    vs_fast_return(c);
    return 1;
}

/* Free actor slots spend most frames peeking at the next course entry only to
 * find that it is still beyond the 48-pixel activation window.  Collapse that
 * read-only decision while preserving the source's page marker bookkeeping,
 * controller/bank request, candidate coordinates and final flags. */
static inline int vs_fast_actor_loop_no_spawn(VsCpu *c) {
    VsBus *bus = c->bus;
    uint8_t *ram = bus->ram;
    uint8_t slot = c->x;
    uint8_t offset, next_offset, data0, data1;
    uint8_t actor_page, actor_pixel;
    uint8_t boundary_pixel, boundary_page;
    uint16_t pointer, address0, address1;
    uint16_t actor_position, screen_position, boundary_position, sum;

    /* Preflight without touching emulated state so every unusual course-loop,
     * swarm, page-command, special-actor and spawn case remains translated. */
    if (slot >= 5u || ram[0x0745] != 0u || ram[0x06cd] != 0u)
        return 0;
    offset = ram[0x0739];
    next_offset = (uint8_t)(offset + 1u);
    pointer = (uint16_t)(ram[0x00e9] | ((uint16_t)ram[0x00ea] << 8));
    address0 = (uint16_t)(pointer + offset);
    address1 = (uint16_t)(pointer + next_offset);
    if (address0 < 0x6000u || address1 < 0x6000u ||
        (address0 >= 0x8000u && !bus->prg) ||
        (address1 >= 0x8000u && !bus->prg))
        return 0;
    data0 = address0 < 0x8000u
        ? bus->extra_ram[address0 & 0x07ffu]
        : bus->prg[address0 - 0x8000u];
    data1 = address1 < 0x8000u
        ? bus->extra_ram[address1 & 0x07ffu]
        : bus->prg[address1 - 0x8000u];
    if (data0 == 0xffu || (data0 & 0x0fu) == 0x0eu ||
        (data0 & 0x0fu) == 0x0fu)
        return 0;

    actor_page = ram[0x073a];
    if ((data1 & 0x80u) != 0u && ram[0x073b] == 0u)
        actor_page = (uint8_t)(actor_page + 1u);
    actor_pixel = data0 & 0xf0u;
    actor_position = (uint16_t)(((uint16_t)actor_page << 8) | actor_pixel);
    screen_position = (uint16_t)(((uint16_t)ram[0x071b] << 8) | ram[0x071d]);
    sum = (uint16_t)ram[0x071d] + 48u;
    boundary_pixel = (uint8_t)sum & 0xf0u;
    boundary_page = (uint8_t)(ram[0x071b] + (sum > 0xffu ? 1u : 0u));
    boundary_position = (uint16_t)(
        ((uint16_t)boundary_page << 8) | boundary_pixel
    );
    if (actor_position < screen_position || boundary_position >= actor_position ||
        ram[0x06cb] != 0u || ram[0x0398] == 1u)
        return 0;

    /* The five identical VS_REQ writes on this path have only the first
     * falling-edge effect; one write produces the exact final bus state. */
    c->a = vs_nz(c, 2);
    vs_wr(c, 0x4016, c->a);
    if ((data1 & 0x80u) != 0u && ram[0x073b] == 0u) {
        ram[0x073b] = (uint8_t)(ram[0x073b] + 1u);
        ram[0x073a] = actor_page;
    }
    ram[7] = boundary_pixel;
    ram[6] = boundary_page;
    ram[(uint8_t)(0x6eu + slot)] = actor_page;
    ram[(uint8_t)(0x87u + slot)] = actor_pixel;
    c->y = offset;

    /* Materialize the final high-byte SBC's overflow before actor_init_check
     * replaces only N/Z/C with its CMP #1. */
    c->a = vs_nz(c, boundary_pixel);
    vs_cmp(c, c->a, actor_pixel);
    c->a = vs_nz(c, boundary_page);
    vs_adc(c, (uint8_t)~actor_page);
    c->a = vs_nz(c, ram[0x06cb]);
    c->a = vs_nz(c, ram[0x0398]);
    vs_cmp(c, c->a, 1);
    vs_fast_return(c);
    return 1;
}

static inline void vs_fast_col_box_proc_alias_exact(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    ram[0] = c->x;
    c->a = vs_nz(c, ram[(0x03b8u + c->y) & 0x7ff]); ram[2] = c->a;
    c->a = vs_nz(c, ram[(0x03adu + c->y) & 0x7ff]); ram[1] = c->a;
    c->a = vs_nz(c, c->x);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    vs_push(c, c->a);
    c->y = vs_nz(c, c->a);
    c->a = vs_nz(c, ram[(0x0499u + c->x) & 0x7ff]);
    vs_fast_asl_a(c); vs_fast_asl_a(c);
    c->x = vs_nz(c, c->a);
    c->a = vs_nz(c, ram[1]); c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xe153 - 0x8000 + c->x]);
    ram[(0x04acu + c->y) & 0x7ff] = c->a;
    c->a = vs_nz(c, ram[1]); c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xe155 - 0x8000 + c->x]);
    ram[(0x04aeu + c->y) & 0x7ff] = c->a;
    c->x = vs_nz(c, (uint8_t)(c->x + 1));
    c->y = vs_nz(c, (uint8_t)(c->y + 1));
    c->a = vs_nz(c, ram[2]); c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xe153 - 0x8000 + c->x]);
    ram[(0x04acu + c->y) & 0x7ff] = c->a;
    c->a = vs_nz(c, ram[2]); c->p &= ~VS_C;
    vs_adc(c, c->bus->prg[0xe155 - 0x8000 + c->x]);
    ram[(0x04aeu + c->y) & 0x7ff] = c->a;
    c->a = vs_nz(c, vs_pop(c));
    c->y = vs_nz(c, c->a);
    c->x = vs_nz(c, ram[0]);
}

static inline void vs_fast_col_box_proc(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t initial_x = c->x, box_offset, shape_offset;
    uint8_t rel_x, rel_y, final_result, table_value;
    uint16_t sum;
    uint8_t overflow;

    if (c->x > 12 || c->y > 6) {
        vs_fast_col_box_proc_alias_exact(c);
        return;
    }
    ram[0] = initial_x;
    rel_y = ram[0x03b8u + c->y]; ram[2] = rel_y;
    rel_x = ram[0x03adu + c->y]; ram[1] = rel_x;
    box_offset = (uint8_t)(initial_x << 2);
    ram[0x100u | c->s] = box_offset;
    shape_offset = (uint8_t)(ram[0x0499u + initial_x] << 2);

    sum = (uint16_t)rel_x + c->bus->prg[0xe153 - 0x8000 + shape_offset];
    ram[(0x04acu + box_offset) & 0x7ff] = (uint8_t)sum;
    sum = (uint16_t)rel_x + c->bus->prg[0xe155 - 0x8000 + shape_offset];
    ram[(0x04aeu + box_offset) & 0x7ff] = (uint8_t)sum;
    ++shape_offset;
    ++box_offset;
    sum = (uint16_t)rel_y + c->bus->prg[0xe153 - 0x8000 + shape_offset];
    ram[(0x04acu + box_offset) & 0x7ff] = (uint8_t)sum;
    table_value = c->bus->prg[0xe155 - 0x8000 + shape_offset];
    sum = (uint16_t)rel_y + table_value;
    final_result = (uint8_t)sum;
    ram[(0x04aeu + box_offset) & 0x7ff] = final_result;
    overflow = (~(rel_y ^ table_value) & (rel_y ^ final_result) & 0x80u) != 0;

    c->a = (uint8_t)(initial_x << 2);
    c->y = c->a;
    c->x = initial_x;
    c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z))
        | (sum > 0xffu ? VS_C : 0) | (overflow ? VS_V : 0)
        | (initial_x & VS_N) | (initial_x == 0 ? VS_Z : 0);
}
#endif
