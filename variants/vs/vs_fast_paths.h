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

/* Decode one ordinary RAM display-list record and transfer its whole payload.
 * The source occupies page $03, separate from CPU scratch/stack and video RAM.
 * Other aliases and the special zero-count loop use the original routine. */
static inline int vs_fast_ppu_displist_command(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint16_t source = (uint16_t)(ram[0] | ((uint16_t)ram[1] << 8));
    uint8_t command;
    uint8_t count;
    uint8_t repeat;
    uint8_t control;
    uint16_t next;

    if (c->y != 0u || c->s < 0x40u || source < 0x0300u || source > 0x03fcu)
        return 0;
    command = ram[source + 2u];
    count = command & 0x3fu;
    repeat = (command & 0x40u) != 0u;
    if (count == 0u || source + 3u + (repeat ? 0u : count - 1u) > 0x03ffu)
        return 0;

    vs_wr(c, 0x2006u, c->a);
    vs_wr(c, 0x2006u, ram[source + 1u]);
    vs_push(c, (uint8_t)(command << 1));
    control = (uint8_t)((ram[0x0778] & 0xfbu) |
        ((command & 0x80u) != 0u ? 4u : 0u));
    /* JSR ppu_displist_write_ctlr0 at $9161 stores $9163. */
    vs_push(c, 0x91);
    vs_push(c, 0x63);
    vs_wr(c, 0x2000u, control);
    ram[0x0778] = control;
    vs_fast_return(c);
    (void)vs_pop(c);

    if (c->bus->ppu_run) {
        c->bus->ppu_run(c->bus->context, ram + source + 3u, count, repeat);
    } else {
        for (unsigned i = 0; i < count; ++i)
            vs_wr(c, 0x2007u, ram[source + 3u + (repeat ? 0u : i)]);
    }

    c->x = 0u;
    c->y = repeat ? 3u : (uint8_t)(count + 2u);
    next = (uint16_t)(source + c->y + 1u);
    ram[0] = (uint8_t)next;
    ram[1] = (uint8_t)(next >> 8);
    vs_wr(c, 0x2006u, 0x3fu);
    vs_wr(c, 0x2006u, 0u);
    vs_wr(c, 0x2006u, 0u);
    vs_wr(c, 0x2006u, 0u);
    /* The final pointer-high ADC clears overflow and carry for page $03. */
    c->p &= (uint8_t)~(VS_C | VS_V);
    c->a = vs_nz(c, 0u);
    c->pc = 0x9195;
    return 1;
}

/* Batch the ordinary 13-metatile background column.  This is the same
 * semantic rendering pass used by the regular game core, adapted to retain
 * the VS CPU's complete observable register, scratch-RAM and status state.
 * Non-empty display-list offsets retain the byte-ordered translated path
 * because its writes can alias data that later iterations still consume. */
static inline int vs_fast_meta_render_area(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    const uint8_t *prg = c->bus->prg;
    uint8_t buffer_offset;
    uint8_t attribute_column;
    uint8_t metatile_side;
    uint8_t attribute_row = 0u;
    uint8_t row;

    if (!prg || ram[0x0340] != 0u)
        return 0;

    buffer_offset = 0u;
    attribute_column = (uint8_t)(ram[0x0726] & 1u);
    metatile_side = (uint8_t)(((ram[0x071f] & 1u) ^ 1u) << 1);
    ram[5] = attribute_column;
    ram[0] = buffer_offset;
    ram[0x0341] = ram[0x0720];
    ram[0x0342] = ram[0x0721];
    ram[0x0343] = 0x9au;
    ram[4] = 0u;

    for (row = 0u; row < 13u; ++row) {
        uint8_t metatile = ram[0x06a1u + row];
        uint8_t palette = (uint8_t)(metatile >> 6);
        uint8_t graphics_low = prg[0x8d09u - 0x8000u + palette];
        uint8_t graphics_high = prg[0x8d0du - 0x8000u + palette];
        uint16_t graphics_address = (uint16_t)(
            graphics_low | ((uint16_t)graphics_high << 8));
        uint8_t tile_offset = (uint8_t)(metatile << 2);
        uint8_t tile_index;
        uint8_t attribute_bits;

        ram[1] = row;
        ram[3] = (uint8_t)(metatile & 0xc0u);
        ram[6] = graphics_low;
        ram[7] = graphics_high;
        ram[2] = tile_offset;

        /* ASL of a zero-or-one value clears carry before the source ADC.  Use
         * the real helper so the final iteration also preserves V exactly. */
        c->p &= (uint8_t)~VS_C;
        c->a = metatile_side;
        vs_adc(c, tile_offset);
        tile_index = c->a;
        ram[0x0344u + buffer_offset] =
            vs_rd(c, (uint16_t)(graphics_address + tile_index));
        ram[0x0345u + buffer_offset] =
            vs_rd(c, (uint16_t)(graphics_address + tile_index + 1u));

        if (attribute_column == 0u) {
            attribute_bits = (uint8_t)(
                palette << ((row & 1u) != 0u ? 4u : 0u));
        } else {
            attribute_bits = (uint8_t)(
                palette << ((row & 1u) != 0u ? 6u : 2u));
        }
        ram[3] = attribute_bits;
        ram[0x03f9u + attribute_row] = (uint8_t)(
            ram[0x03f9u + attribute_row] | attribute_bits);
        if ((row & 1u) != 0u)
            ++attribute_row;
        ram[4] = attribute_row;

        buffer_offset = (uint8_t)(buffer_offset + 2u);
        ram[0] = buffer_offset;
    }

    c->x = 13u;
    c->y = (uint8_t)(buffer_offset + 3u);
    ram[0x0341u + c->y] = 0u;
    ram[0x0340] = c->y;
    ++ram[0x0721];
    if ((ram[0x0721] & 0x1fu) == 0u) {
        ram[0x0721] = 0x80u;
        ram[0x0720] ^= 0x04u;
    }

    /* The final CPX has carry set; meta_render_attr_end then loads six. */
    c->p |= VS_C;
    c->a = vs_nz(c, 6u);
    ram[0x0773] = c->a;
    vs_fast_return(c);
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

static inline void vs_fast_render_player_init(VsCpu *c, uint8_t rows) {
    uint8_t *ram = c->bus->ram;
    ram[7] = rows;
    ram[0x0755] = ram[5] = ram[0x03ad];
    ram[2] = ram[0x03b8];
    ram[3] = ram[0x33];
    ram[4] = ram[0x03c4];
    c->x = ram[0x06d5];
    c->y = ram[0x06e4];
    (void)vs_fast_render_player_tiles(c);
}

/* Compose Mario's complete OAM record, including standing/death mirroring,
 * the fireball-throw overlay and per-row clipping. Animation selection stays
 * in the caller; each selected frame is now built without page dispatch. */
static inline int vs_fast_render_player_do(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t sprite = ram[0x06e4];
    uint8_t group = c->a;
    uint8_t bits;

    if (!c->bus->prg || (sprite & 3u) != 0u || sprite > 224u ||
        c->s < 0x40u)
        return 0;
    ram[0x06d5] = group;
    vs_push(c, 0xee);
    vs_push(c, 0xb1);
    vs_fast_render_player_init(c, 4u);

    vs_push(c, 0xee);
    vs_push(c, 0xb4);
    if (ram[0x0e] == 0x0bu || group == 0xc8u) {
        ram[0x0212u + sprite] &= 0x3fu;
        ram[0x0216u + sprite] =
            (uint8_t)((ram[0x0216u + sprite] & 0x3fu) | 0x40u);
    }
    if (ram[0x0e] == 0x0bu || group == 0xc8u || group == 0x50u ||
        group == 0xb8u || group == 0xc0u) {
        ram[0x021au + sprite] &= 0x3fu;
        ram[0x021eu + sprite] =
            (uint8_t)((ram[0x021eu + sprite] & 0x3fu) | 0x40u);
    }
    vs_fast_return(c);

    if (ram[0x0711] != 0u) {
        uint8_t timer = ram[0x0781];
        uint8_t throwing = ram[0x0711];
        ram[0x0711] = 0u;
        if (timer < throwing) {
            ram[0x0711] = timer;
            ram[0x06d5] = c->bus->prg[0xed6bu - 0x8000u];
            vs_push(c, 0xee);
            vs_push(c, 0xde);
            vs_fast_render_player_init(c,
                (ram[0x57] | ram[0x0c]) != 0u ? 3u : 4u);
        }
    }

    bits = (uint8_t)(ram[0x03d0] >> 4);
    for (unsigned row = 0; row < 4u; ++row) {
        if (bits & (1u << row)) {
            unsigned output = 0x0200u + sprite + (3u - row) * 8u;
            vs_push(c, 0xee);
            vs_push(c, 0xf9);
            ram[output] = ram[output + 4u] = 0xf8u;
            vs_fast_return(c);
        }
    }
    ram[0] = 0u;
    c->a = c->y = (uint8_t)(sprite - 8u);
    c->x = 0xffu;
    c->p = (c->p & ~(VS_C | VS_V | VS_N | VS_Z)) | VS_N
        | (sprite >= 8u ? VS_C : 0u)
        | (sprite >= 128u && sprite < 136u ? VS_V : 0u);
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

static inline void vs_fast_render_actor_tiles(VsCpu *c) {
    static const uint8_t returns[3] = {0xac, 0xaf, 0xb2};

    c->y = vs_nz(c, c->bus->ram[0xeb]);
    for (unsigned pair = 0; pair < 3; ++pair) {
        vs_push(c, 0xe9);
        vs_push(c, returns[pair]);
        vs_fast_render_actor_pair(c);
        vs_fast_return(c);
    }
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

static inline void vs_fast_render_actor_clear_h(
    VsCpu *c,
    uint8_t offset,
    uint8_t return_low
) {
    uint8_t *ram = c->bus->ram;

    c->a = vs_nz(c, offset);
    vs_push(c, 0xea);
    vs_push(c, return_low);
    c->p &= ~VS_C;
    vs_adc(c, ram[(0x06e5u + c->x) & 0x07ffu]);
    c->y = vs_nz(c, c->a);
    /* JSR render_offscr_top_clear at $eb23 stores $eb25. */
    vs_push(c, 0xeb);
    vs_push(c, 0x25);
    c->a = vs_nz(c, 0xf8);
    ram[0x0200u + c->y] = c->a;
    ram[0x0208u + c->y] = c->a;
    vs_fast_return(c);
    ram[0x0210u + c->y] = c->a;
    vs_fast_return(c);
}

static inline int vs_fast_render_actor_clear_offscr(VsCpu *c) {
    uint8_t bits = c->bus->ram[0x03d1];

    /* The common actor path is only horizontally clipped. Vertical clipping
     * and its possible actor erase retain the complete translated routine. */
    if ((bits & 0xe0u) != 0u || c->bus->ram[8] >= 6u || c->s < 0x40u)
        return 0;
    c->x = vs_nz(c, c->bus->ram[8]);
    c->a = vs_nz(c, bits);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_push(c, c->a);
    if (c->p & VS_C)
        vs_fast_render_actor_clear_h(c, 4, 0xd0);

    c->a = vs_nz(c, vs_pop(c));
    vs_fast_lsr_a(c);
    vs_push(c, c->a);
    if (c->p & VS_C)
        vs_fast_render_actor_clear_h(c, 0, 0xda);

    c->a = vs_nz(c, vs_pop(c));
    vs_fast_lsr_a(c);
    vs_fast_lsr_a(c);
    vs_push(c, c->a);
    c->a = vs_nz(c, vs_pop(c));
    vs_fast_lsr_a(c);
    vs_push(c, c->a);
    c->a = vs_nz(c, vs_pop(c));
    vs_fast_lsr_a(c);
    vs_fast_return(c);
    return 1;
}

/* Complete actor graphics composition for the game's allocated six-sprite
 * records.  The original graphics/attribute tables remain authoritative;
 * animation decisions and sprite layout are ordinary C operations.  All
 * sprite state is still staged in RAM for the shared hardware-safe renderer.
 * Unallocated/misaligned layouts retain the translated byte ordering. */
static inline int vs_fast_render_actor(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    const uint8_t *prg = c->bus->prg;
    uint8_t slot = c->x;
    uint8_t sprite;
    uint8_t id;
    uint8_t state;
    uint8_t next;
    uint8_t graphics;
    uint8_t bowser;

    if (!prg || slot >= 6u || ram[8] != slot || c->s < 0x40u)
        return 0;
    sprite = ram[0x06e5u + slot];
    if ((sprite & 3u) != 0u || sprite > 232u)
        return 0;

    ram[2] = ram[0xcfu + slot];
    ram[5] = ram[0x03ae];
    ram[0xeb] = sprite;
    ram[0x0109] = 0u;
    ram[3] = ram[0x46u + slot];
    ram[4] = ram[0x03c5u + slot];
    id = ram[0x16u + slot];

    /* A submerged Piranha can deliberately leave its previous OAM untouched.
     * This early return precedes the sprite arithmetic, so preserve its
     * incoming overflow flag and exact CMP/LDY result. */
    if (id == 0x0du && (ram[0x58u + slot] & 0x80u) == 0u &&
        ram[0x078au + slot] != 0u) {
        c->a = id;
        c->y = vs_nz(c, ram[0x078au + slot]);
        c->p |= VS_C;
        vs_fast_return(c);
        return 1;
    }

    state = ram[0x1eu + slot];
    next = (uint8_t)(state & 0x1fu);
    if (id == 0x35u) {             /* Toad uses the second graphics page. */
        next = 0u;
        ram[3] = 1u;
        id = 0x15u;
    }
    if (id == 0x33u) {             /* Cannon Bullet Bill. */
        --ram[2];
        ram[4] = (uint8_t)(3u | (ram[0x078au + slot] ? 0x20u : 0u));
        next = state = 0u;
        id = 8u;
    }
    if (id == 0x32u) {             /* Springboard compression frame. */
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
        if ((ram[0x1eu + slot] & 0x20u) == 0u &&
            ram[0x0747] == 0u && (ram[9] & 8u) == 0u)
            ram[3] ^= 3u;
    }

    ram[4] |= prg[0xe7b8u - 0x8000u + id];
    graphics = prg[0xe79du - 0x8000u + id];
    if (bowser != 0u) {
        if (bowser == 1u) {
            if (ram[0x0363] & 0x80u)
                graphics = 0xdeu;
        } else {
            if (ram[0x0363] & 1u)
                graphics = 0xe4u;
            if (state & 0x20u)
                ram[2] = (uint8_t)(ram[2] - 16u);
        }
        if (state & 0x20u)
            ram[0x0109] = graphics;
    } else {
        int animate = 0;
        int check_animation = 1;

        if (graphics == 0x24u) {   /* Spiny walking / egg. */
            if (next == 5u) {
                graphics = 0x30u;
                ram[3] = 2u;
                next = 5u;
            }
        } else if (graphics == 0x90u) { /* Lakitu throwing / ducking. */
            if ((state & 0x20u) == 0u && ram[0x078f] < 16u)
                graphics = 0x96u;
            check_animation = 0;
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
            int check_frame = 1;

            if (id == 5u) {
                if (state != 0u) {
                    if ((state & 8u) == 0u)
                        check_frame = 0;
                    else
                        graphics = 0xb4u;
                }
            } else if (graphics != 0x48u) {
                if (timer >= 5u) {
                    check_frame = 0;
                } else if (graphics == 0x3cu) {
                    check_frame = 0;
                    if (timer != 1u) {
                        ram[2] = (uint8_t)(ram[2] + 3u);
                        animate = 1;
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
                    animate = 1;
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
    c->x = graphics;
    vs_fast_render_actor_tiles(c);
    c->y = sprite;

    if (id != 8u) {
        if (ram[0x0109] != 0u) {
            uint8_t attr = (uint8_t)(ram[0x0202u + sprite] | 0x80u);
            uint8_t swap = sprite;

            for (unsigned row = 0; row < 6u; ++row)
                ram[0x0202u + sprite + row * 4u] = attr;
            /* The source stores this JSR return before swapping two rows. */
            vs_push(c, 0xe9);
            vs_push(c, 0xcf);
            vs_fast_return(c);
            if (id != 5u && id != 0x11u && id < 0x15u) {
                c->a = sprite;
                c->p &= (uint8_t)~VS_C;
                vs_adc(c, 8u);
                swap = c->a;
            }
            vs_push(c, ram[0x0201u + swap]);
            vs_push(c, ram[0x0205u + swap]);
            ram[0x0201u + swap] = ram[0x0211u + sprite];
            ram[0x0205u + swap] = ram[0x0215u + sprite];
            ram[0x0215u + sprite] = vs_pop(c);
            ram[0x0211u + sprite] = vs_pop(c);
        }
        if (bowser == 0u && id != 5u) {
            int mirror = id == 7u || id == 0x0du || id == 0x0cu;
            if (!mirror && !(id == 0x12u && next != 5u)) {
                if (id == 0x15u)
                    ram[0x0216u + sprite] = 0x42u;
                mirror = next >= 2u;
            }
            if (mirror) {
                uint8_t left = (uint8_t)(ram[0x0202u + sprite] & 0xa3u);
                uint8_t right = (uint8_t)(left | 0x40u |
                    (next == 5u ? 0x80u : 0u));
                for (unsigned row = 0; row < 3u; ++row) {
                    ram[0x0202u + sprite + row * 8u] = left;
                    ram[0x0206u + sprite + row * 8u] = right;
                }
                if (next == 4u) {
                    left |= 0x80u;
                    right = (uint8_t)(left | 0x40u);
                    ram[0x020au + sprite] = ram[0x0212u + sprite] = left;
                    ram[0x020eu + sprite] = ram[0x0216u + sprite] = right;
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
                ram[0x020au + sprite] = ram[0x0212u + sprite] = 0x82u;
                ram[0x020eu + sprite] = ram[0x0216u + sprite] = 0xc2u;
            }
        }
    }

    /* The established cleanup helper handles ordinary horizontal clipping;
     * vertical erase cases continue through the complete original tail. */
    if (!vs_fast_render_actor_clear_offscr(c))
        c->pc = 0xeac1;
    return 1;
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

/* Six tiles make one large platform. Build its coordinates, tile IDs and
 * attributes in one pass, then apply the shared horizontal bounds result.
 * This replaces the regular port's same multi-call sprite-stacker sequence. */
static inline int vs_fast_render_plat_large(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;
    uint8_t sprite;
    uint8_t y;
    uint8_t tile;
    uint8_t offscreen;

    if (slot >= 6u || ram[8] != slot || c->s < 0x40u)
        return 0;
    sprite = ram[0x06e5u + slot];
    if ((sprite & 3u) != 0u || sprite > 232u)
        return 0;
    ram[2] = sprite;
    y = ram[0xcfu + slot];
    tile = ram[0x0743] != 0u ? 0x75u : 0x5bu;
    for (unsigned index = 0; index < 6u; ++index) {
        unsigned output = 0x0200u + sprite + index * 4u;
        ram[output] = index >= 4u &&
            (ram[0x074e] == 3u || ram[0x06cc] != 0u) ? 0xf8u : y;
        ram[output + 1u] = tile;
        ram[output + 2u] = 2u;
        ram[output + 3u] = (uint8_t)(ram[0x03ae] + index * 8u);
    }

    /* All previous leaf calls use the same stack bytes. This final call
     * overwrites them and preserves any deeper bounds-helper stack writes. */
    c->x = (uint8_t)(slot + 1u);
    vs_push(c, 0xe5);
    vs_push(c, 0x69);
    vs_fast_pos_bits_get_do_x(c);
    vs_fast_return(c);
    offscreen = c->a;
    c->x = slot;
    c->y = sprite;
    for (unsigned index = 0; index < 6u; ++index) {
        if (offscreen & (0x80u >> index))
            ram[0x0200u + sprite + index * 4u] = 0xf8u;
    }
    /* Five balanced PHA/PLA operations leave the fifth shifted mask here. */
    ram[0x0100u | c->s] = (uint8_t)(offscreen << 5);
    c->a = ram[0x03d1];
    vs_fast_asl_a(c);
    if (c->p & VS_C) {
        vs_push(c, 0xe5);
        vs_push(c, 0xb0);
        c->a = vs_nz(c, 0xf8u);
        for (unsigned index = 0; index < 6u; ++index)
            ram[0x0200u + sprite + index * 4u] = c->a;
        vs_fast_return(c);
    }
    vs_fast_return(c);
    return 1;
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

/* Integrate vertical position and acceleration as fixed-point arithmetic.
 * Player, enemy and platform callers share this operation; only the final
 * comparisons need materialized CPU flags. Preserve the source's byte-sign
 * clamp tests (including wraparound), rather than imposing signed C clamps. */
static inline int vs_fast_motion_gravity(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    unsigned slot = c->x;
    unsigned sum;
    uint8_t speed;
    uint8_t acceleration;
    uint8_t result;

    if (slot > 21u || c->s < 0x40u)
        return 0;
    vs_push(c, c->a);
    sum = ram[0x0416u + slot] + ram[0x0433u + slot];
    ram[0x0416u + slot] = (uint8_t)sum;
    speed = ram[0x9fu + slot];
    c->y = (speed & 0x80u) != 0u ? 0xffu : 0u;
    ram[7] = c->y;
    sum = speed + ram[0xceu + slot] + (sum >> 8);
    ram[0xceu + slot] = (uint8_t)sum;
    ram[0xb5u + slot] = (uint8_t)(
        ram[0xb5u + slot] + c->y + (sum >> 8));

    sum = ram[0x0433u + slot] + ram[0];
    acceleration = (uint8_t)sum;
    ram[0x0433u + slot] = acceleration;
    result = (uint8_t)(speed + (sum >> 8));
    ram[0x9fu + slot] = result;
    c->p = (c->p & (uint8_t)~VS_V) |
        (speed == 0x7fu && sum > 255u ? VS_V : 0u);
    vs_cmp(c, result, ram[2]);
    if ((c->p & VS_N) == 0u) {
        vs_cmp(c, acceleration, 0x80u);
        if (c->p & VS_C) {
            ram[0x9fu + slot] = ram[2];
            ram[0x0433u + slot] = 0u;
        }
    }
    c->a = vs_nz(c, vs_pop(c));
    if (c->a != 0u) {
        uint8_t borrow;
        c->y = (uint8_t)(0u - ram[2]);
        ram[7] = c->y;
        acceleration = ram[0x0433u + slot];
        borrow = acceleration < ram[1];
        acceleration = (uint8_t)(acceleration - ram[1]);
        ram[0x0433u + slot] = acceleration;
        speed = ram[0x9fu + slot];
        c->a = (uint8_t)(speed - borrow);
        ram[0x9fu + slot] = c->a;
        c->p = (c->p & (uint8_t)~VS_V) |
            (speed == 0x80u && borrow ? VS_V : 0u);
        vs_cmp(c, c->a, c->y);
        if (c->p & VS_N) {
            c->a = acceleration;
            vs_cmp(c, c->a, 0x80u);
            if ((c->p & VS_C) == 0u) {
                ram[0x9fu + slot] = c->y;
                c->a = vs_nz(c, 0xffu);
                ram[0x0433u + slot] = c->a;
            }
        }
    }
    vs_fast_return(c);
    return 1;
}

/* Relative coordinates are geometry, not an emulated timing primitive. Keep
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

static inline int vs_fast_actor_proc_dispatch(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t mode = ram[(uint8_t)(0x0fu + c->x)];

    if (mode & 0x80u)
        return 0;
    c->a = vs_nz(c, mode);
    vs_push(c, c->a);
    vs_fast_asl_a(c);
    c->a = vs_nz(c, vs_pop(c));
    if (mode != 0u) {
        c->pc = 0xc7c5;
        return 1;
    }
    c->a = vs_nz(c, ram[0x071f]);
    c->a = vs_nz(c, c->a & 7u);
    vs_cmp(c, c->a, 7);
    if (c->p & VS_Z)
        vs_fast_return(c);
    else
        c->pc = 0xbfea;
    return 1;
}

static inline int vs_fast_actor_proc_base_state0(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;
    uint8_t speed;

    if (slot >= 6u || ram[(uint8_t)(0x1eu + slot)] != 0u ||
        ram[8] >= 6u || c->s < 0x40u)
        return 0;
    speed = ram[(uint8_t)(0x58u + slot)];
    vs_push(c, speed);
    /* State zero selects either of the two zero acceleration entries. */
    ram[(uint8_t)(0x58u + slot)] = speed;
    /* JSR motion_x at $ca04 stores $ca06. */
    vs_push(c, 0xca);
    vs_push(c, 0x06);
    vs_fast_motion_x(c);
    c->a = vs_nz(c, vs_pop(c));
    ram[(uint8_t)(0x58u + c->x)] = c->a;
    vs_fast_return(c);
    return 1;
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

/* Player/enemy contact is deliberately split across alternating frames.  The
 * odd-frame half is only an LSR and RTS, but entering the generic translated
 * routine for it once per active actor is comparatively expensive. */
static inline int vs_fast_col_player_actor_odd(VsCpu *c) {
    uint8_t frame = c->bus->ram[9];

    if ((frame & 1u) == 0u)
        return 0;
    c->a = vs_nz(c, frame);
    vs_fast_lsr_a(c);
    vs_fast_return(c);
    return 1;
}

typedef enum {
    VS_FAST_PAIR_UNSAFE = 0,
    VS_FAST_PAIR_HORIZONTAL,
    VS_FAST_PAIR_VERTICAL,
} VsFastPairPolicy;

static inline VsFastPairPolicy vs_fast_actor_pair_policy(
    const uint8_t *ram, uint8_t first, uint8_t second
) {
    uint8_t first_left = ram[0x04acu + first];
    uint8_t first_top = ram[0x04acu + first + 1u];
    uint8_t first_right = ram[0x04aeu + first];
    uint8_t first_bottom = ram[0x04aeu + first + 1u];
    uint8_t second_left = ram[0x04acu + second];
    uint8_t second_top = ram[0x04acu + second + 1u];
    uint8_t second_right = ram[0x04aeu + second];
    uint8_t second_bottom = ram[0x04aeu + second + 1u];

    if (first_left > first_right || first_top > first_bottom ||
        second_left > second_right || second_top > second_bottom)
        return VS_FAST_PAIR_UNSAFE;
    if (first_right < second_left || first_left > second_right)
        return VS_FAST_PAIR_HORIZONTAL;
    if (first_bottom < second_top || first_top > second_bottom)
        return VS_FAST_PAIR_VERTICAL;
    return VS_FAST_PAIR_UNSAFE;
}

static inline int vs_fast_actor_collision_id(uint8_t id) {
    return id < 0x15u && id != 0x11u && id != 0x0du;
}

/* Preserve every cheap exit in the alternating actor/actor collision gate and
 * collapse the overwhelmingly common scan where all earlier actors are either
 * inactive, excluded, or geometrically separate.  Actual overlaps fall back. */
static inline int vs_fast_col_actor_actor_exit(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t frame = ram[9];
    uint8_t actor_id, masked;
    uint8_t first_box, candidate;
    uint8_t ignored = 0, separated = 0, horizontal = 0;
    uint8_t final_carry;

    if ((frame & 1u) == 0u) {
        c->a = vs_nz(c, frame);
        vs_fast_lsr_a(c);
        vs_fast_return(c);
        return 1;
    }
    if (ram[0x074e] == 0u) {
        c->a = vs_nz(c, frame);
        vs_fast_lsr_a(c);
        c->a = vs_nz(c, 0);
        vs_fast_return(c);
        return 1;
    }

    actor_id = ram[(uint8_t)(0x16u + c->x)];
    masked = ram[(0x03d8u + c->x) & 0x07ffu];
    if (vs_fast_actor_collision_id(actor_id) && masked == 0u) {
        uint8_t slot = c->x;

        if (!c->bus->prg || slot >= 6u || ram[8] != slot || c->s < 0x40u)
            return 0;
        first_box = (uint8_t)(slot * 4u + 4u);
        candidate = slot;
        while (candidate != 0u) {
            uint8_t bit;
            uint8_t id;
            uint8_t second_box;
            VsFastPairPolicy policy;

            --candidate;
            bit = (uint8_t)(1u << candidate);
            if (ram[(uint8_t)(0x0fu + candidate)] == 0u)
                continue;
            id = ram[(uint8_t)(0x16u + candidate)];
            if (!vs_fast_actor_collision_id(id)) {
                ignored |= bit;
                continue;
            }
            if (ram[0x03d8u + candidate] != 0u)
                return 0;
            second_box = (uint8_t)(candidate * 4u + 4u);
            policy = vs_fast_actor_pair_policy(ram, first_box, second_box);
            if (policy == VS_FAST_PAIR_UNSAFE)
                return 0;
            separated |= bit;
            if (policy == VS_FAST_PAIR_HORIZONTAL)
                horizontal |= bit;
        }

        /* Entry filters. */
        c->a = vs_nz(c, frame);
        vs_fast_lsr_a(c);
        c->a = vs_nz(c, ram[0x074e]);
        c->a = vs_nz(c, actor_id);
        vs_cmp(c, c->a, 0x15);
        vs_cmp(c, c->a, 0x11);
        vs_cmp(c, c->a, 0x0d);
        c->a = vs_nz(c, masked);

        /* JSR col_actor_box_get at $d9a7 stores $d9a9. */
        vs_push(c, 0xd9);
        vs_push(c, 0xa9);
        c->a = vs_nz(c, ram[8]);
        vs_fast_asl_a(c);
        vs_fast_asl_a(c);
        c->p &= ~VS_C;
        vs_adc(c, 4);
        c->y = vs_nz(c, c->a);
        c->a = vs_nz(c, ram[0x03d1] & 0x0fu);
        vs_cmp(c, c->a, 0x0f);
        vs_fast_return(c);
        c->x = vs_nz(c, (uint8_t)(c->x - 1u));

        final_carry = (uint8_t)((ram[0x03d1] & 0x0fu) >= 0x0fu);
        candidate = slot;
        while (candidate != 0u) {
            uint8_t bit;
            uint8_t id;

            --candidate;
            bit = (uint8_t)(1u << candidate);
            ram[1] = candidate;
            c->a = vs_nz(c, c->y);
            vs_push(c, c->a);
            c->a = vs_nz(c, ram[(uint8_t)(0x0fu + candidate)]);
            if (c->a != 0u) {
                id = ram[(uint8_t)(0x16u + candidate)];
                c->a = vs_nz(c, id);
                vs_cmp(c, c->a, 0x15);
                if (!(c->p & VS_C)) {
                    vs_cmp(c, c->a, 0x11);
                    if (!(c->p & VS_Z))
                        vs_cmp(c, c->a, 0x0d);
                }
                if ((ignored & bit) != 0u) {
                    final_carry = 1;
                } else {
                    c->a = vs_nz(c, ram[0x03d8u + candidate]);
                    c->a = vs_nz(c, candidate);
                    vs_fast_asl_a(c);
                    vs_fast_asl_a(c);
                    c->p &= ~VS_C;
                    vs_adc(c, 4);
                    c->x = vs_nz(c, c->a);
                    /* JSR col_base_actor at $d9cf stores $d9d1. */
                    vs_push(c, 0xd9);
                    vs_push(c, 0xd1);
                    ram[6] = first_box;
                    ram[7] = (horizontal & bit) != 0u ? 1u : 0u;
                    c->p &= ~VS_C;
                    c->y = vs_nz(c, first_box);
                    vs_fast_return(c);
                    c->x = vs_nz(c, ram[8]);
                    c->y = vs_nz(c, ram[1]);
                    if ((separated & bit) != 0u) {
                        c->a = vs_nz(c, ram[0x0491u + candidate]);
                        c->a = vs_nz(c, c->a &
                            c->bus->prg[0xd983u - 0x8000u + slot]);
                        ram[0x0491u + candidate] = c->a;
                        final_carry = 0;
                    }
                }
            }
            c->a = vs_nz(c, vs_pop(c));
            c->y = vs_nz(c, c->a);
            c->x = vs_nz(c, ram[1]);
            c->x = vs_nz(c, (uint8_t)(c->x - 1u));
        }
        c->x = vs_nz(c, ram[8]);
        c->p = (c->p & ~VS_C) | (final_carry ? VS_C : 0);
        vs_fast_return(c);
        return 1;
    }

    c->a = vs_nz(c, frame);
    vs_fast_lsr_a(c);
    c->a = vs_nz(c, ram[0x074e]);
    c->a = vs_nz(c, actor_id);
    vs_cmp(c, c->a, 0x15);
    if (!(c->p & VS_C)) {
        vs_cmp(c, c->a, 0x11);
        if (!(c->p & VS_Z)) {
            vs_cmp(c, c->a, 0x0d);
            if (!(c->p & VS_Z))
                c->a = vs_nz(c, masked);
        }
    }
    c->x = vs_nz(c, ram[8]);
    vs_fast_return(c);
    return 1;
}

static inline int vs_fast_col_actor_ground_disabled(VsCpu *c) {
    uint8_t state = c->bus->ram[(uint8_t)(0x1eu + c->x)];

    if ((state & 0x20u) == 0u)
        return 0;
    c->a = vs_nz(c, state);
    c->a = vs_nz(c, c->a & 0x20u);
    vs_fast_return(c);
    return 1;
}

static inline void vs_fast_col_actor_pos_y_diff(VsCpu *c) {
    c->a = vs_nz(c, c->bus->ram[(uint8_t)(0xcfu + c->x)]);
    c->p &= ~VS_C;
    vs_adc(c, 62);
    vs_cmp(c, c->a, 68);
    vs_fast_return(c);
}

static inline void vs_fast_col_box_buffer(VsCpu *c);

/* Inline both small wrappers around the already verified block-buffer kernel.
 * This is shared by ground and side probes and retains the otherwise invisible
 * stack-page writes made by PHA and the nested JSRs. */
static inline void vs_fast_col_box_buffer_check_actor(VsCpu *c) {
    uint8_t *ram = c->bus->ram;

    vs_push(c, c->a);
    c->a = vs_nz(c, c->x);
    c->p &= ~VS_C;
    vs_adc(c, 1);
    c->x = vs_nz(c, c->a);
    c->a = vs_nz(c, vs_pop(c));

    /* JSR col_box_buffer at $e2fb stores $e2fd. */
    vs_push(c, 0xe2);
    vs_push(c, 0xfd);
    vs_fast_col_box_buffer(c);
    vs_fast_return(c);
    c->x = vs_nz(c, ram[8]);
    vs_cmp(c, c->a, 0);
    vs_fast_return(c);
}

static inline void vs_fast_col_actor_block_col(VsCpu *c) {
    c->a = vs_nz(c, 0);
    c->y = vs_nz(c, 0x15);
    vs_fast_col_box_buffer_check_actor(c);
}

typedef struct {
    uint8_t tile;
} VsFastActorBlockProbe;

static inline VsFastActorBlockProbe vs_fast_actor_block_probe(
    VsCpu *c, uint8_t slot, uint8_t adder
) {
    uint8_t *ram = c->bus->ram;
    uint8_t object = (uint8_t)(slot + 1u);
    uint16_t sum = (uint16_t)c->bus->prg[0xe306u - 0x8000u + adder]
        + ram[(uint8_t)(0x86u + object)];
    uint8_t adjusted_x = (uint8_t)sum;
    uint8_t column = (uint8_t)((((ram[(uint8_t)(0x6du + object)]
        + (sum > 0xffu ? 1u : 0u)) & 1u) << 4) | (adjusted_x >> 4));
    uint16_t pointer = (uint16_t)((column & 0x10u ? 0x05d0u : 0x0500u)
        + (column & 15u));
    uint16_t ysum = (uint16_t)ram[(uint8_t)(0xceu + object)]
        + c->bus->prg[0xe322u - 0x8000u + adder];
    uint8_t adjusted_y = (uint8_t)(((uint8_t)ysum & 0xf0u) - 0x20u);
    VsFastActorBlockProbe result;

    result.tile = ram[(pointer + adjusted_y) & 0x07ffu];
    return result;
}

static inline int vs_fast_actor_tile_non_solid(uint8_t tile) {
    return tile == 0x26u || tile == 0xc2u || tile == 0xc3u ||
        tile == 0x5fu || tile == 0x60u;
}

static inline void vs_fast_col_bg_non_solid(VsCpu *c) {
    static const uint8_t values[5] = {0x26, 0xc2, 0xc3, 0x5f, 0x60};

    for (unsigned i = 0; i < 5; ++i) {
        vs_cmp(c, c->a, values[i]);
        if (c->p & VS_Z)
            break;
    }
    vs_fast_return(c);
}

/* Most actor side checks inspect one tile and find empty space.  Preflight the
 * tile so a real wall hit still falls back to the complete reversal/sound path,
 * then execute the two source iterations with exact scratch and stack residue. */
static inline int vs_fast_col_actor_check_side_do(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;
    uint8_t position, direction;

    if (!c->bus->prg || slot >= 6u || ram[8] != slot || c->s < 0x40u)
        return 0;
    position = ram[(uint8_t)(0xcfu + slot)];
    direction = ram[(uint8_t)(0x46u + slot)];
    if (position >= 0x20u && (direction == 1u || direction == 2u)) {
        uint8_t adder = direction == 2u ? 0x16u : 0x17u;
        uint8_t tile = vs_fast_actor_block_probe(c, slot, adder).tile;
        if (tile != 0u && !vs_fast_actor_tile_non_solid(tile))
            return 0;
    }

    c->a = vs_nz(c, position);
    vs_cmp(c, c->a, 0x20);
    if (!(c->p & VS_C)) {
        vs_fast_return(c);
        return 1;
    }
    c->y = vs_nz(c, 0x16);
    c->a = vs_nz(c, 2);
    ram[0xeb] = c->a;
    do {
        c->a = vs_nz(c, ram[0xeb]);
        vs_cmp(c, c->a, ram[(uint8_t)(0x46u + c->x)]);
        if (c->p & VS_Z) {
            c->a = vs_nz(c, 1);
            /* JSR col_box_buffer_check_actor at $e068 stores $e06a. */
            vs_push(c, 0xe0);
            vs_push(c, 0x6a);
            vs_fast_col_box_buffer_check_actor(c);
            if (!(c->p & VS_Z)) {
                /* JSR col_bg_proc_check_non_solid at $e06d stores $e06f. */
                vs_push(c, 0xe0);
                vs_push(c, 0x6f);
                vs_fast_col_bg_non_solid(c);
            }
        }
        ram[0xeb] = vs_nz(c, (uint8_t)(ram[0xeb] - 1u));
        c->y = vs_nz(c, (uint8_t)(c->y + 1u));
        vs_cmp(c, c->y, 0x18);
    } while (!(c->p & VS_C));
    vs_fast_return(c);
    return 1;
}

/* State-zero Goombas account for the bulk of ordinary ground probes.  This is
 * the VS counterpart of the regular port's proven semantic ground fast path:
 * preflight both tiles, retain exceptional bump/reversal behavior as fallback,
 * and fold the empty/solid-aligned cases without changing game rules. */
static inline int vs_fast_col_actor_ground(VsCpu *c) {
    uint8_t *ram = c->bus->ram;
    uint8_t slot = c->x;
    uint8_t state = ram[(uint8_t)(0x1eu + slot)];
    uint8_t position = ram[(uint8_t)(0xcfu + slot)];
    uint8_t adjusted = (uint8_t)(position + 62u);
    uint8_t actor_id;
    VsFastActorBlockProbe under;
    int empty_or_non_solid;
    int offset_outside;

    if ((state & 0x20u) != 0u)
        return vs_fast_col_actor_ground_disabled(c);

    /* The vertical-range return is valid for every actor state and ID. */
    if (adjusted < 68u) {
        c->a = vs_nz(c, state);
        c->a = vs_nz(c, c->a & 0x20u);
        /* JSR col_actor_pos_y_diff at $df1d stores $df1f. */
        vs_push(c, 0xdf);
        vs_push(c, 0x1f);
        vs_fast_col_actor_pos_y_diff(c);
        vs_fast_return(c);
        return 1;
    }

    actor_id = ram[(uint8_t)(0x16u + slot)];
    if (state == 0u && (actor_id == 0x0du || actor_id == 0x11u)) {
        c->a = vs_nz(c, state);
        c->a = vs_nz(c, c->a & 0x20u);
        vs_push(c, 0xdf);
        vs_push(c, 0x1f);
        vs_fast_col_actor_pos_y_diff(c);
        c->y = vs_nz(c, actor_id);
        vs_cmp(c, c->y, 0x12);
        vs_cmp(c, c->y, 0x0e);
        vs_cmp(c, c->y, 0x05);
        vs_cmp(c, c->y, 0x12);
        vs_cmp(c, c->y, 0x2e);
        vs_cmp(c, c->y, 0x07);
        vs_fast_return(c);
        return 1;
    }

    if (!c->bus->prg || slot >= 6u || ram[8] != slot || state != 0u ||
        actor_id != 6u || c->s < 0x40u)
        return 0;
    under = vs_fast_actor_block_probe(c, slot, 0x15);
    if (under.tile == 0x23u)
        return 0;
    if (position >= 0x20u) {
        uint8_t direction = ram[(uint8_t)(0x46u + slot)];
        if (direction == 1u || direction == 2u) {
            uint8_t adder = direction == 2u ? 0x16u : 0x17u;
            uint8_t tile = vs_fast_actor_block_probe(c, slot, adder).tile;
            if (tile != 0u && !vs_fast_actor_tile_non_solid(tile))
                return 0;
        }
    }
    empty_or_non_solid = under.tile == 0u ||
        vs_fast_actor_tile_non_solid(under.tile);
    offset_outside = (uint8_t)((position & 0x0fu) - 8u) >= 5u;

    c->a = vs_nz(c, state);
    c->a = vs_nz(c, c->a & 0x20u);
    vs_push(c, 0xdf);
    vs_push(c, 0x1f);
    vs_fast_col_actor_pos_y_diff(c);
    c->y = vs_nz(c, actor_id);
    vs_cmp(c, c->y, 0x12);
    vs_cmp(c, c->y, 0x0e);
    vs_cmp(c, c->y, 0x05);
    vs_cmp(c, c->y, 0x12);
    vs_cmp(c, c->y, 0x2e);
    vs_cmp(c, c->y, 0x07);

    /* JSR col_actor_block_col at $df48 stores $df4a. */
    vs_push(c, 0xdf);
    vs_push(c, 0x4a);
    vs_fast_col_actor_block_col(c);
    if (under.tile != 0u) {
        /* JSR col_bg_proc_check_non_solid at $df50 stores $df52. */
        vs_push(c, 0xdf);
        vs_push(c, 0x52);
        vs_fast_col_bg_non_solid(c);
        if (!empty_or_non_solid)
            vs_cmp(c, c->a, 0x23);
    }

    if (!empty_or_non_solid) {
        c->a = vs_nz(c, ram[4]);
        c->p |= VS_C;
        vs_adc(c, (uint8_t)~8u);
        vs_cmp(c, c->a, 5);
        if (!offset_outside) {
            c->a = vs_nz(c, ram[(uint8_t)(0x1eu + c->x)]);
            c->a = vs_nz(c, c->a & 0x40u);
            c->a = vs_nz(c, ram[(uint8_t)(0x1eu + c->x)]);
            vs_fast_asl_a(c);
            c->a = vs_nz(c, ram[(uint8_t)(0x1eu + c->x)]);
        }
    }

    if (empty_or_non_solid || offset_outside) {
        /* The Goomba state-zero branch of col_actor_check_side. */
        c->a = vs_nz(c, ram[(uint8_t)(0x16u + c->x)]);
        vs_cmp(c, c->a, 3);
        c->a = vs_nz(c, ram[(uint8_t)(0x1eu + c->x)]);
        c->y = vs_nz(c, c->a);
        vs_fast_asl_a(c);
        c->a = vs_nz(c, c->bus->prg[0xdf0fu - 0x8000u + c->y]);
        ram[(uint8_t)(0x1eu + c->x)] = c->a;
    }

    return vs_fast_col_actor_check_side_do(c);
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
