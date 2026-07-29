#include "code.h"
#include "constants.h"
#include "core_fast_paths.h"
#include "cpu.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t ram[RAM_SIZE];
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
} MachineState;

typedef struct {
    uint8_t page;
    uint8_t pixel;
} ObjectPosition;

static const uint8_t valid_sprite_offsets[] = {
    0u, 4u, 24u, 112u, 232u,
};

static const uint8_t canonical_x_masks[] = {
    0x00u, 0x01u, 0x03u, 0x07u,
    0x0fu, 0x1fu, 0x3fu, 0x7fu,
    0x80u, 0xc0u, 0xe0u, 0xf0u,
    0xf8u, 0xfcu, 0xfeu, 0xffu,
};

static uint64_t direct_comparisons;
static uint64_t fallback_checks;
static uint8_t observed_x_masks[256];

static uint32_t lcg_next(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void save_machine(MachineState *state) {
    memcpy(state->ram, ram, RAM_SIZE);
    state->a = a;
    state->x = x;
    state->y = y;
    state->sp = sp;
    state->carry = carry_flag;
    state->nz = nz_value;
}

static void load_machine(const MachineState *state) {
    memcpy(ram, state->ram, RAM_SIZE);
    a = state->a;
    x = state->x;
    y = state->y;
    sp = state->sp;
    carry_flag = state->carry;
    nz_value = state->nz;
}

static void assert_machine_equal(
    const MachineState *expected,
    const MachineState *actual
) {
    assert(memcmp(expected->ram, actual->ram, RAM_SIZE) == 0);
    assert(expected->a == actual->a);
    assert(expected->x == actual->x);
    assert(expected->y == actual->y);
    assert(expected->sp == actual->sp);
    assert(expected->carry == actual->carry);
    assert(expected->nz == actual->nz);
}

static void fill_pattern(uint32_t seed) {
    uint32_t state = seed;
    size_t index;

    for (index = 0u; index < RAM_SIZE; ++index) {
        ram[index] = (uint8_t)(lcg_next(&state) >> 24);
    }
    a = (uint8_t)(lcg_next(&state) >> 24);
    x = (uint8_t)(lcg_next(&state) >> 24);
    y = (uint8_t)(lcg_next(&state) >> 24);
    sp = (uint8_t)(lcg_next(&state) >> 24);
    carry_flag = (lcg_next(&state) & 1u) != 0u;
    nz_value = (uint8_t)(lcg_next(&state) >> 24);
}

static void prepare_valid_case(
    uint32_t seed,
    uint8_t slot,
    uint8_t sprite_offset,
    uint8_t area_type,
    uint8_t hard_mode,
    uint8_t cloud,
    uint8_t absolute_offscreen,
    uint8_t stack_pointer,
    ObjectPosition object
) {
    fill_pattern(seed);
    x = slot;
    ram[ObjectOffset] = slot;
    ram[Enemy_SprDataOffset + slot] = sprite_offset;
    ram[AreaType] = area_type;
    ram[SecondaryHardMode] = hard_mode;
    ram[CloudTypeOverride] = cloud;
    ram[Enemy_OffscreenBits] = absolute_offscreen;
    sp = stack_pointer;

    /* A stable one-page viewport makes every canonical X mask reachable. */
    ram[ScreenEdge_PageLoc] = 4u;
    ram[ScreenEdge_X_Pos] = 0u;
    ram[ScreenEdge_PageLoc + 1u] = 4u;
    ram[ScreenEdge_X_Pos + 1u] = 0xffu;
    ram[SprObject_PageLoc + slot + 1u] = object.page;
    ram[SprObject_X_Position + slot + 1u] = object.pixel;
}

static uint8_t get_prepared_x_mask(void) {
    MachineState entry;
    uint8_t mask;

    save_machine(&entry);
    x = (uint8_t)(ram[ObjectOffset] + 1u);
    GetXOffscreenBits();
    mask = a;
    load_machine(&entry);
    return mask;
}

static void compare_prepared_case(void) {
    MachineState entry;
    MachineState expected;
    MachineState actual;
    uint8_t mask = get_prepared_x_mask();

    observed_x_masks[mask] = 1u;
    save_machine(&entry);
    DrawLargePlatform();
    save_machine(&expected);

    load_machine(&entry);
    assert(smb_core_fast_draw_large_platform());
    save_machine(&actual);
    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static ObjectPosition find_position_for_mask(uint8_t wanted) {
    uint16_t page;
    uint16_t pixel;

    for (page = 2u; page <= 6u; ++page) {
        for (pixel = 0u; pixel <= 0xffu; ++pixel) {
            ObjectPosition object = { (uint8_t)page, (uint8_t)pixel };

            prepare_valid_case(
                UINT32_C(0x41c6ce57), 2u, 48u, 1u, 0u, 0u,
                0u, 0xd3u, object
            );
            if (get_prepared_x_mask() == wanted) {
                return object;
            }
        }
    }
    assert(!"canonical horizontal offscreen mask was unreachable");
    return (ObjectPosition){ 0u, 0u };
}

static void compare_boundary_cross_product(void) {
    ObjectPosition positions[sizeof(canonical_x_masks)];
    const uint8_t area_types[] = { 0u, 2u, 3u, 0xffu };
    const uint8_t modes[] = { 0u, 1u, 0xffu };
    const uint8_t absolute_masks[] = { 0u, 0x7fu, 0x80u, 0xffu };
    const uint8_t stack_pointers[] = { 0u, 1u, 0x80u, 0xffu };
    size_t mask_index;
    uint8_t slot;
    size_t offset_index;

    for (mask_index = 0u;
         mask_index < sizeof(canonical_x_masks);
         ++mask_index) {
        positions[mask_index] =
            find_position_for_mask(canonical_x_masks[mask_index]);
    }

    for (slot = 0u; slot < 6u; ++slot) {
        for (offset_index = 0u;
             offset_index < sizeof(valid_sprite_offsets);
             ++offset_index) {
            for (mask_index = 0u;
                 mask_index < sizeof(canonical_x_masks);
                 ++mask_index) {
                size_t selector =
                    (size_t)slot * sizeof(valid_sprite_offsets) +
                    offset_index + mask_index;

                prepare_valid_case(
                    (uint32_t)(UINT32_C(0x8f31a2d7) + selector * 97u),
                    slot,
                    valid_sprite_offsets[offset_index],
                    area_types[selector % sizeof(area_types)],
                    modes[(selector / 3u) % sizeof(modes)],
                    modes[(selector / 5u) % sizeof(modes)],
                    absolute_masks[
                        (selector / 7u) % sizeof(absolute_masks)
                    ],
                    stack_pointers[
                        (selector / 11u) % sizeof(stack_pointers)
                    ],
                    positions[mask_index]
                );
                compare_prepared_case();
            }
        }
    }

    for (mask_index = 0u;
         mask_index < sizeof(canonical_x_masks);
         ++mask_index) {
        assert(observed_x_masks[canonical_x_masks[mask_index]] != 0u);
    }
}

static void compare_every_coordinate_byte(void) {
    ObjectPosition centered = { 4u, 0x70u };
    uint16_t value;

    for (value = 0u; value <= 0xffu; ++value) {
        prepare_valid_case(
            UINT32_C(0x72891f43) + value,
            (uint8_t)(value % 6u), 72u, 1u, 0u, 0u, 0u,
            (uint8_t)value, centered
        );
        ram[Enemy_Rel_XPos] = (uint8_t)value;
        compare_prepared_case();

        prepare_valid_case(
            UINT32_C(0xe4b17a09) + value,
            (uint8_t)(value % 6u), 72u, 1u, 0u, 1u, 0x80u,
            (uint8_t)(value ^ 0xffu), centered
        );
        ram[Enemy_Y_Position + ram[ObjectOffset]] = (uint8_t)value;
        compare_prepared_case();
    }
}

static void compare_patterned_full_state(void) {
    uint32_t state = UINT32_C(0x3a16729d);
    uint32_t ordinal;

    for (ordinal = 0u; ordinal < 20000u; ++ordinal) {
        uint32_t random = lcg_next(&state);
        uint8_t slot = (uint8_t)(random % 6u);
        uint8_t sprite_offset = valid_sprite_offsets[
            (random >> 4) % sizeof(valid_sprite_offsets)
        ];
        ObjectPosition object = {
            (uint8_t)(2u + ((random >> 8) % 5u)),
            (uint8_t)(random >> 16),
        };

        prepare_valid_case(
            random ^ ordinal, slot, sprite_offset,
            (uint8_t)(random >> 3), (uint8_t)(random >> 11),
            (uint8_t)(random >> 19), (uint8_t)(random >> 7),
            (uint8_t)(random >> 23), object
        );
        compare_prepared_case();
    }
}

static void assert_six_oam_records(
    uint8_t sprite_offset,
    uint8_t y_position,
    uint8_t hidden_mask,
    uint8_t x_position
) {
    uint8_t index;

    for (index = 0u; index < 6u; ++index) {
        uint8_t output = (uint8_t)(sprite_offset + index * 4u);
        uint8_t expected_y =
            (hidden_mask & (uint8_t)(1u << index)) != 0u
                ? 0xf8u
                : y_position;

        assert(ram[Sprite_Y_Position + output] == expected_y);
        assert(ram[Sprite_Tilenumber + output] == 0x5bu);
        assert(ram[Sprite_Attributes + output] == 0x02u);
        assert(
            ram[Sprite_X_Position + output] ==
            (uint8_t)(x_position + index * 8u)
        );
    }
}

static void compare_captured_scene_case(bool world_4_3) {
    MachineState entry;
    MachineState expected;
    MachineState actual;
    uint8_t slot;
    uint8_t sprite_offset;

    memset(ram, 0, RAM_SIZE);
    if (!world_4_3) {
        /*
         * First paired-lift DrawLargePlatform call after screen page 8 in
         * World 1-2. Captured from the 67,677-frame replay at 5ca5268:
         * entry 42689f95... / exit 556f734d.... Only the exact read set and
         * observable stack byte are retained here; no ROM payload is stored.
         */
        a = 0xbbu;
        x = 2u;
        y = 1u;
        sp = 0xfeu;
        carry_flag = true;
        nz_value = 2u;
        slot = 2u;
        sprite_offset = 0x28u;
        ram[0] = 0x02u;
        ram[1] = 0x08u;
        ram[2] = 0x81u;
        ram[3] = 0x49u;
        ram[4] = 0x03u;
        ram[5] = 0x08u;
        ram[6] = 0x0cu;
        ram[7] = 0x01u;
        ram[ObjectOffset] = slot;
        ram[Enemy_SprDataOffset + slot] = sprite_offset;
        ram[Enemy_Rel_XPos] = 0xbbu;
        ram[Enemy_Y_Position + slot] = 0xa0u;
        ram[AreaType] = 2u;
        ram[ScreenEdge_PageLoc] = 8u;
        ram[ScreenEdge_PageLoc + 1u] = 9u;
        ram[ScreenEdge_X_Pos] = 1u;
        ram[ScreenEdge_X_Pos + 1u] = 0u;
        ram[SprObject_PageLoc + slot + 1u] = 8u;
        ram[SprObject_X_Position + slot + 1u] = 0xbcu;
        ram[0x100u + sp] = 0x02u;
    } else {
        /*
         * First balance-platform draw on screen pages 4-6 in World 4-3.
         * Replay capture: entry 02cfc026... / exit e2c59db0.... This is also
         * the reviewed maximum non-wrapping OAM base (0xe8).
         */
        a = 0xffu;
        x = 1u;
        y = 1u;
        sp = 0xfeu;
        carry_flag = false;
        nz_value = 1u;
        slot = 1u;
        sprite_offset = 0xe8u;
        ram[0] = 0x01u;
        ram[1] = 0x04u;
        ram[2] = 0x81u;
        ram[3] = 0x49u;
        ram[4] = 0x02u;
        ram[5] = 0x08u;
        ram[6] = 0x38u;
        ram[7] = 0x00u;
        ram[ObjectOffset] = slot;
        ram[Enemy_SprDataOffset + slot] = sprite_offset;
        ram[Enemy_Rel_XPos] = 0xffu;
        ram[Enemy_Y_Position + slot] = 0x4eu;
        ram[AreaType] = 1u;
        ram[Enemy_OffscreenBits] = 0x07u;
        ram[ScreenEdge_PageLoc] = 4u;
        ram[ScreenEdge_PageLoc + 1u] = 5u;
        ram[ScreenEdge_X_Pos] = 1u;
        ram[ScreenEdge_X_Pos + 1u] = 0u;
        ram[SprObject_PageLoc + slot + 1u] = 5u;
        ram[SprObject_X_Position + slot + 1u] = 0u;
        ram[0x100u + sp] = 0x08u;
    }

    save_machine(&entry);
    DrawLargePlatform();
    if (!world_4_3) {
        assert(a == 0u && x == 2u && y == 0x28u && sp == 0xfeu);
        assert(!carry_flag && nz_value == 0u);
        assert(ram[0x1feu] == 0u);
        assert_six_oam_records(0x28u, 0xa0u, 0u, 0xbbu);
    } else {
        assert(a == 0x0eu && x == 1u && y == 0xe8u && sp == 0xfeu);
        assert(!carry_flag && nz_value == 0x0eu);
        assert(ram[0x1feu] == 0xe0u);
        assert_six_oam_records(0xe8u, 0x4eu, 0x3eu, 0xffu);
    }
    save_machine(&expected);

    load_machine(&entry);
    assert(smb_core_fast_draw_large_platform());
    save_machine(&actual);
    assert_machine_equal(&expected, &actual);
    ++direct_comparisons;
}

static void assert_fallback_preserves_entry(void) {
    static const uint8_t unsafe_offsets[] = {
        1u, 2u, 3u, 233u, 236u, 252u, 255u,
    };
    MachineState entry;
    MachineState actual;
    size_t index;

    for (index = 0u; index < sizeof(unsafe_offsets); ++index) {
        fill_pattern((uint32_t)(UINT32_C(0xb4712e83) + index));
        x = 2u;
        ram[ObjectOffset] = 2u;
        ram[Enemy_SprDataOffset + 2u] = unsafe_offsets[index];
        save_machine(&entry);
        assert(!smb_core_fast_draw_large_platform());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
        ++fallback_checks;
    }

    for (index = 0u; index < 256u; ++index) {
        fill_pattern((uint32_t)(UINT32_C(0x14d09637) + index));
        x = (uint8_t)index;
        ram[ObjectOffset] = (uint8_t)index;
        if (x < 6u) {
            ram[ObjectOffset] = (uint8_t)((x + 1u) % 6u);
        }
        save_machine(&entry);
        assert(!smb_core_fast_draw_large_platform());
        save_machine(&actual);
        assert_machine_equal(&entry, &actual);
        ++fallback_checks;
    }
}

int main(void) {
    compare_boundary_cross_product();
    compare_every_coordinate_byte();
    compare_patterned_full_state();
    compare_captured_scene_case(false);
    compare_captured_scene_case(true);
    assert_fallback_preserves_entry();
    printf(
        "DrawLargePlatform direct-C differential tests: OK "
        "(%llu exact, %llu untouched fallback)\n",
        (unsigned long long)direct_comparisons,
        (unsigned long long)fallback_checks
    );
    return 0;
}
