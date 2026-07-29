#include "cpu.h"
#include "hud_update.h"
#include "ppu.h"
#include "ppu_render_state.h"
#include "vblank_budget.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_STREAM_CAPACITY 256u

enum TestVramOperation {
    TEST_VRAM_MOD,
    TEST_VRAM_ADDRESS,
    TEST_VRAM_DATA,
};

typedef struct {
    uint8_t operation;
    uint16_t value;
} TestVramEntry;

typedef struct {
    TestVramEntry entries[TEST_STREAM_CAPACITY];
    uint16_t count;
} TestVramStream;

uint8_t ram[RAM_SIZE];

void neogeo_video_render(void) {
}

static uint16_t reference_entry(
    uint8_t index,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    uint8_t row = (uint8_t)(index >> 5);
    uint8_t column = (uint8_t)(index & 31u);
    uint8_t source_y = (uint8_t)(row + 1u);
    uint8_t tile;
    uint16_t attribute_index;
    uint8_t attribute;
    uint8_t shift;
    uint8_t palette_number;

    if (show_hud == 0u) {
        return NEOGEO_HUD_FIX_BLANK_TILE;
    }
    tile = nametable[(uint16_t)source_y * 32u + column];
    attribute_index =
        (uint16_t)((source_y >> 2) * 8u + (column >> 2));
    attribute = nametable[960u + attribute_index];
    shift = (uint8_t)(
        (((source_y & 3u) >> 1) << 2) +
        (((column & 3u) >> 1) << 1)
    );
    palette_number = (uint8_t)((attribute >> shift) & 3u);
    return (uint16_t)(
        ((uint16_t)palette_number << 12) +
        1u + pattern_base + tile
    );
}

static uint8_t reference_build(
    NeoGeoHudState *state,
    uint32_t source_generation,
    uint8_t dirty_any,
    uint8_t dirty_tracking_valid,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    uint16_t render_config =
        (uint16_t)(pattern_base | (show_hud != 0u ? 1u : 0u));
    uint8_t index;

    if (
        dirty_tracking_valid != 0u &&
        dirty_any == 0u &&
        state->built_generation == source_generation &&
        state->built_config == render_config
    ) {
        return 0u;
    }

    state->changed_count = 0u;
    for (index = 0u; index < NEOGEO_HUD_ENTRY_COUNT; ++index) {
        uint16_t entry = reference_entry(
            index,
            pattern_base,
            show_hud
        );

        state->desired[index] = entry;
        if (state->cached[index] != entry) {
            state->changed_indices[state->changed_count++] = index;
        }
    }
    state->built_generation = source_generation;
    state->built_config = render_config;
    state->upload_cursor = 0u;
    state->upload_pending = (uint8_t)(state->changed_count != 0u);
    return 1u;
}

static void assert_states_equal(
    const NeoGeoHudState *actual,
    const NeoGeoHudState *expected
) {
    assert(
        memcmp(actual->desired, expected->desired, sizeof(actual->desired)) ==
        0
    );
    assert(
        memcmp(actual->cached, expected->cached, sizeof(actual->cached)) == 0
    );
    assert(actual->built_generation == expected->built_generation);
    assert(actual->built_config == expected->built_config);
    assert(actual->upload_pending == expected->upload_pending);
    assert(actual->changed_count == expected->changed_count);
    assert(actual->upload_cursor == expected->upload_cursor);
    assert(
        memcmp(
            actual->changed_indices,
            expected->changed_indices,
            sizeof(actual->changed_indices)
        ) == 0
    );
}

static uint8_t compare_build(
    NeoGeoHudState *actual,
    NeoGeoHudState *expected,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    uint32_t dirty_rows[NEOGEO_HUD_ROWS];
    uint8_t dirty_any;
    uint8_t tracking_valid;
    uint8_t actual_built;
    uint8_t expected_built;

    memcpy(dirty_rows, neogeo_ppu_hud_dirty_rows, sizeof(dirty_rows));
    dirty_any = neogeo_ppu_hud_dirty_any();
    tracking_valid = neogeo_ppu_hud_dirty_tracking_valid;
    actual_built = neogeo_hud_build(
        actual,
        nametable,
        neogeo_ppu_hud_generation,
        neogeo_ppu_hud_dirty_rows,
        &neogeo_ppu_hud_dirty_tracking_valid,
        pattern_base,
        show_hud
    );
    expected_built = reference_build(
        expected,
        neogeo_ppu_hud_generation,
        dirty_any,
        tracking_valid,
        pattern_base,
        show_hud
    );

    assert(actual_built == expected_built);
    assert_states_equal(actual, expected);
    if (actual_built != 0u) {
        assert(neogeo_ppu_hud_dirty_any() == 0u);
        assert(neogeo_ppu_hud_dirty_tracking_valid == 1u);
    } else {
        assert(
            memcmp(
                dirty_rows,
                neogeo_ppu_hud_dirty_rows,
                sizeof(dirty_rows)
            ) == 0
        );
        assert(neogeo_ppu_hud_dirty_tracking_valid == tracking_valid);
    }
    return actual_built;
}

static void append_stream(
    TestVramStream *stream,
    uint8_t operation,
    uint16_t value
) {
    assert(stream->count < TEST_STREAM_CAPACITY);
    stream->entries[stream->count].operation = operation;
    stream->entries[stream->count].value = value;
    ++stream->count;
}

static void upload_actual_chunk(
    NeoGeoHudState *state,
    TestVramStream *stream
) {
    uint8_t chunk_end;

    if (state->upload_pending == 0u) {
        return;
    }
    chunk_end = (uint8_t)(
        state->upload_cursor +
        neogeo_vblank_hud_chunk(
            (uint16_t)(state->changed_count - state->upload_cursor)
        )
    );
    append_stream(stream, TEST_VRAM_MOD, 1u);
    while (state->upload_cursor < chunk_end) {
        uint8_t index = state->changed_indices[state->upload_cursor];
        uint16_t value = state->desired[index];

        append_stream(
            stream,
            TEST_VRAM_ADDRESS,
            neogeo_hud_fixmap_address(index)
        );
        append_stream(stream, TEST_VRAM_DATA, value);
        state->cached[index] = value;
        ++state->upload_cursor;
    }
    if (state->upload_cursor == state->changed_count) {
        state->upload_pending = 0u;
        state->changed_count = 0u;
        state->upload_cursor = 0u;
    }
}

static void upload_reference_chunk(
    NeoGeoHudState *state,
    TestVramStream *stream
) {
    uint8_t remaining;
    uint8_t chunk_size;
    uint8_t chunk_end;

    if (state->upload_pending == 0u) {
        return;
    }
    remaining = (uint8_t)(state->changed_count - state->upload_cursor);
    chunk_size = remaining < 32u ? remaining : 32u;
    chunk_end = (uint8_t)(state->upload_cursor + chunk_size);
    append_stream(stream, TEST_VRAM_MOD, 1u);
    while (state->upload_cursor < chunk_end) {
        uint8_t index = state->changed_indices[state->upload_cursor];
        uint8_t row = (uint8_t)(index / 32u);
        uint8_t column = (uint8_t)(index % 32u);
        uint16_t address = (uint16_t)(
            UINT16_C(0x7000) +
            ((4u + column) * 32u) +
            2u + row
        );
        uint16_t value = state->desired[index];

        append_stream(stream, TEST_VRAM_ADDRESS, address);
        append_stream(stream, TEST_VRAM_DATA, value);
        state->cached[index] = value;
        ++state->upload_cursor;
    }
    if (state->upload_cursor == state->changed_count) {
        state->upload_pending = 0u;
        state->changed_count = 0u;
        state->upload_cursor = 0u;
    }
}

static void assert_streams_equal(
    const TestVramStream *actual,
    const TestVramStream *expected
) {
    assert(actual->count == expected->count);
    assert(
        memcmp(
            actual->entries,
            expected->entries,
            (size_t)actual->count * sizeof(actual->entries[0])
        ) == 0
    );
}

static void compare_upload_one_chunk(
    NeoGeoHudState *actual,
    NeoGeoHudState *expected
) {
    TestVramStream actual_stream = {0};
    TestVramStream expected_stream = {0};

    upload_actual_chunk(actual, &actual_stream);
    upload_reference_chunk(expected, &expected_stream);
    assert_streams_equal(&actual_stream, &expected_stream);
    assert_states_equal(actual, expected);
}

static void compare_upload_all(
    NeoGeoHudState *actual,
    NeoGeoHudState *expected
) {
    TestVramStream actual_stream = {0};
    TestVramStream expected_stream = {0};

    while (actual->upload_pending != 0u) {
        upload_actual_chunk(actual, &actual_stream);
    }
    while (expected->upload_pending != 0u) {
        upload_reference_chunk(expected, &expected_stream);
    }
    assert_streams_equal(&actual_stream, &expected_stream);
    assert_states_equal(actual, expected);
}

static void initialize_state(NeoGeoHudState *state) {
    uint8_t index;

    memset(state, 0, sizeof(*state));
    state->built_generation = UINT32_MAX;
    state->built_config = UINT16_MAX;
    for (index = 0u; index < NEOGEO_HUD_ENTRY_COUNT; ++index) {
        state->desired[index] = NEOGEO_HUD_FIX_BLANK_TILE;
        state->cached[index] = NEOGEO_HUD_FIX_BLANK_TILE;
    }
}

static void fill_test_nametable(void) {
    uint8_t row;
    uint8_t column;

    memset(nametable, 0, NAMETABLE_SIZE);
    for (row = 1u; row <= 3u; ++row) {
        for (column = 0u; column < 32u; ++column) {
            nametable[(uint16_t)row * 32u + column] =
                (uint8_t)(row * 53u + column * 7u);
        }
    }
    for (column = 0u; column < 8u; ++column) {
        nametable[960u + column] = (uint8_t)(0x1bu + column * 29u);
    }
}

static void prepare_synced_states(
    NeoGeoHudState *actual,
    NeoGeoHudState *expected,
    uint16_t pattern_base,
    uint8_t show_hud
) {
    ppu_init(0);
    fill_test_nametable();
    initialize_state(actual);
    *expected = *actual;
    assert(compare_build(actual, expected, pattern_base, show_hud) != 0u);
    compare_upload_all(actual, expected);
    assert(neogeo_ppu_hud_dirty_tracking_valid == 1u);
}

static void test_all_tiles_and_address_mirrors(void) {
    static const uint16_t aliases[] = {
        UINT16_C(0x2000),
        UINT16_C(0x2800),
        UINT16_C(0x3000),
        UINT16_C(0x3800),
    };
    NeoGeoHudState actual;
    NeoGeoHudState expected;
    uint8_t index;
    uint8_t alias;

    for (alias = 0u; alias < sizeof(aliases) / sizeof(aliases[0]); ++alias) {
        for (index = 0u; index < NEOGEO_HUD_ENTRY_COUNT; ++index) {
            uint8_t row = (uint8_t)(index >> 5);
            uint8_t column = (uint8_t)(index & 31u);
            uint16_t offset =
                (uint16_t)(row + 1u) * 32u + column;
            uint8_t old_value;

            prepare_synced_states(&actual, &expected, 0u, 1u);
            old_value = nametable[offset];
            ppu_write(
                (uint16_t)(aliases[alias] + offset),
                (uint8_t)(old_value + 1u)
            );
            assert(
                neogeo_ppu_hud_dirty_rows[row] ==
                (UINT32_C(1) << column)
            );
            assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
            assert(actual.changed_count == 1u);
            assert(actual.changed_indices[0] == index);
            compare_upload_all(&actual, &expected);
        }
    }
}

static void test_all_attribute_transitions(void) {
    NeoGeoHudState baseline;
    NeoGeoHudState actual;
    NeoGeoHudState expected;
    uint16_t old_value;
    uint16_t new_value;
    uint8_t attribute_column;

    for (attribute_column = 0u; attribute_column < 8u; ++attribute_column) {
        uint16_t attribute_offset = (uint16_t)(960u + attribute_column);

        for (old_value = 0u; old_value <= UINT8_MAX; ++old_value) {
            prepare_synced_states(&baseline, &expected, 0u, 1u);
            nametable[attribute_offset] = (uint8_t)old_value;
            neogeo_ppu_hud_dirty_tracking_valid = 0u;
            baseline.built_generation = UINT32_MAX;
            baseline.built_config = UINT16_MAX;
            expected = baseline;
            assert(compare_build(&baseline, &expected, 0u, 1u) != 0u);
            compare_upload_all(&baseline, &expected);

            for (new_value = 0u; new_value <= UINT8_MAX; ++new_value) {
                uint32_t expected_mask =
                    UINT32_C(0x0f) << (attribute_column * 4u);

                actual = baseline;
                expected = baseline;
                nametable[attribute_offset] = (uint8_t)old_value;
                neogeo_ppu_hud_generation = 0u;
                neogeo_ppu_hud_dirty_rows[0] = 0u;
                neogeo_ppu_hud_dirty_rows[1] = 0u;
                neogeo_ppu_hud_dirty_rows[2] = 0u;
                neogeo_ppu_hud_dirty_tracking_valid = 1u;

                ppu_write(
                    (uint16_t)(0x2000u + attribute_offset),
                    (uint8_t)new_value
                );
                if (new_value == old_value) {
                    assert(neogeo_ppu_hud_generation == 0u);
                    assert(neogeo_ppu_hud_dirty_any() == 0u);
                    assert(
                        compare_build(&actual, &expected, 0u, 1u) == 0u
                    );
                } else {
                    assert(neogeo_ppu_hud_generation == 1u);
                    assert(neogeo_ppu_hud_dirty_rows[0] == expected_mask);
                    assert(neogeo_ppu_hud_dirty_rows[1] == expected_mask);
                    assert(neogeo_ppu_hud_dirty_rows[2] == expected_mask);
                    assert(
                        compare_build(&actual, &expected, 0u, 1u) != 0u
                    );
                }
                compare_upload_all(&actual, &expected);
            }
        }
    }
}

static void test_duplicates_reverts_and_mixed_writes(void) {
    NeoGeoHudState actual;
    NeoGeoHudState expected;
    uint8_t original_tile;
    uint8_t original_attribute;

    prepare_synced_states(&actual, &expected, 0u, 1u);
    original_tile = nametable[32u + 7u];
    ppu_write(0x2027u, (uint8_t)(original_tile + 1u));
    ppu_write(0x2027u, (uint8_t)(original_tile + 1u));
    ppu_write(0x2027u, original_tile);
    assert(neogeo_ppu_hud_generation == 2u);
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    assert(actual.changed_count == 0u);

    original_attribute = nametable[960u + 2u];
    ppu_write(0x23c2u, (uint8_t)(original_attribute ^ 0xffu));
    ppu_write(0x23c2u, original_attribute);
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    assert(actual.changed_count == 0u);

    /* Writes arrive in reverse order; the queue must remain row-major. */
    ppu_write(0x207fu, (uint8_t)(nametable[0x7fu] + 1u));
    ppu_write(0x2041u, (uint8_t)(nametable[0x41u] + 1u));
    ppu_write(0x23c0u, (uint8_t)(nametable[0x3c0u] ^ 0x55u));
    ppu_write(0x2020u, (uint8_t)(nametable[0x20u] + 1u));
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    {
        uint8_t i;
        for (i = 1u; i < actual.changed_count; ++i) {
            assert(
                actual.changed_indices[i - 1u] < actual.changed_indices[i]
            );
        }
    }
    compare_upload_all(&actual, &expected);
}

static void test_configuration_initialization_and_invalidation(void) {
    NeoGeoHudState actual;
    NeoGeoHudState expected;

    prepare_synced_states(&actual, &expected, 0u, 1u);
    assert(compare_build(&actual, &expected, 0u, 1u) == 0u);

    assert(compare_build(&actual, &expected, 0u, 0u) != 0u);
    compare_upload_all(&actual, &expected);
    assert(compare_build(&actual, &expected, 256u, 0u) != 0u);
    assert(actual.changed_count == 0u);
    assert(compare_build(&actual, &expected, 256u, 1u) != 0u);
    compare_upload_all(&actual, &expected);

    /* Equal generation/config still requires a full first synchronization. */
    actual.built_generation = neogeo_ppu_hud_generation;
    actual.built_config = 1u;
    memset(actual.cached, 0xff, sizeof(actual.cached));
    expected = actual;
    neogeo_ppu_hud_dirty_tracking_valid = 0u;
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    assert(actual.changed_count == NEOGEO_HUD_ENTRY_COUNT);
    compare_upload_all(&actual, &expected);

    /* Renderer cache invalidation also forces the reference path. */
    memset(actual.cached, 0xff, sizeof(actual.cached));
    actual.built_generation = UINT32_MAX;
    actual.built_config = UINT16_MAX;
    actual.upload_pending = 0u;
    actual.changed_count = 0u;
    actual.upload_cursor = 0u;
    expected = actual;
    neogeo_ppu_hud_dirty_invalidate();
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    compare_upload_all(&actual, &expected);
}

static void test_pending_upload_falls_back_exactly(void) {
    NeoGeoHudState actual;
    NeoGeoHudState expected;
    uint8_t column;

    prepare_synced_states(&actual, &expected, 0u, 1u);
    for (column = 0u; column < 32u; ++column) {
        ppu_write(
            (uint16_t)(0x2020u + column),
            (uint8_t)(nametable[32u + column] + 1u)
        );
    }
    for (column = 0u; column < 16u; ++column) {
        ppu_write(
            (uint16_t)(0x2040u + column),
            (uint8_t)(nametable[64u + column] + 1u)
        );
    }
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    assert(actual.changed_count == 48u);
    compare_upload_one_chunk(&actual, &expected);
    assert(actual.upload_pending != 0u);
    assert(actual.upload_cursor == 32u);

    /* Touch one committed and one queued cell while the old queue is live. */
    ppu_write(0x2020u, (uint8_t)(nametable[32u] + 1u));
    ppu_write(0x204fu, (uint8_t)(nametable[64u + 15u] + 1u));
    assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
    compare_upload_all(&actual, &expected);
}

static void test_attribute_address_mirrors(void) {
    static const uint16_t addresses[] = {
        UINT16_C(0x23c4),
        UINT16_C(0x2bc4),
        UINT16_C(0x33c4),
        UINT16_C(0x3bc4),
    };
    NeoGeoHudState actual;
    NeoGeoHudState expected;
    uint8_t alias;

    for (alias = 0u; alias < sizeof(addresses) / sizeof(addresses[0]); ++alias) {
        prepare_synced_states(&actual, &expected, 0u, 1u);
        ppu_write(
            addresses[alias],
            (uint8_t)(nametable[960u + 4u] ^ 0xffu)
        );
        assert(neogeo_ppu_hud_dirty_rows[0] == UINT32_C(0x000f0000));
        assert(neogeo_ppu_hud_dirty_rows[1] == UINT32_C(0x000f0000));
        assert(neogeo_ppu_hud_dirty_rows[2] == UINT32_C(0x000f0000));
        assert(compare_build(&actual, &expected, 0u, 1u) != 0u);
        compare_upload_all(&actual, &expected);
    }
}

int main(void) {
    test_all_tiles_and_address_mirrors();
    test_all_attribute_transitions();
    test_duplicates_reverts_and_mixed_writes();
    test_configuration_initialization_and_invalidation();
    test_pending_upload_falls_back_exactly();
    test_attribute_address_mirrors();
    puts("Neo Geo dirty HUD update oracle: OK");
    return 0;
}
