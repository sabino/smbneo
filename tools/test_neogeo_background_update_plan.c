#include "background_update_plan.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t before;
    uint8_t dirty[NEOGEO_BACKGROUND_DIRTY_BYTES];
    uint8_t after;
} GuardedDirtyColumns;

static uint8_t dirty_test(
    const uint8_t dirty[NEOGEO_BACKGROUND_DIRTY_BYTES],
    uint8_t world_column
) {
    return (uint8_t)(
        dirty[world_column >> 3] &
        ((uint8_t)1u << (world_column & 7u))
    );
}

static void check_case(
    uint8_t first_column,
    uint8_t forward_delta,
    uint8_t force_full_rebuild,
    const uint8_t input_dirty[NEOGEO_BACKGROUND_DIRTY_BYTES]
) {
    GuardedDirtyColumns guarded;
    NeoGeoBackgroundUpdatePlan plan;
    uint8_t expected[NEOGEO_BACKGROUND_VISIBLE_STRIPS];
    uint8_t observed[NEOGEO_BACKGROUND_VISIBLE_STRIPS];
    uint8_t retained = forward_delta < NEOGEO_BACKGROUND_VISIBLE_STRIPS
        ? (uint8_t)(
            NEOGEO_BACKGROUND_VISIBLE_STRIPS - forward_delta
        )
        : 0u;
    uint8_t full = (uint8_t)(
        force_full_rebuild != 0u ||
        forward_delta >= NEOGEO_BACKGROUND_VISIBLE_STRIPS
    );
    uint8_t logical;
    uint8_t index;

    guarded.before = 0xa5u;
    memcpy(guarded.dirty, input_dirty, sizeof(guarded.dirty));
    guarded.after = 0x5au;
    memset(expected, 0, sizeof(expected));
    memset(observed, 0, sizeof(observed));

    for (
        logical = 0u;
        logical < NEOGEO_BACKGROUND_VISIBLE_STRIPS;
        ++logical
    ) {
        uint8_t world_column = (uint8_t)(
            (first_column + logical) &
            (NEOGEO_BACKGROUND_WORLD_COLUMNS - 1u)
        );

        expected[logical] = (uint8_t)(
            full != 0u ||
            logical >= retained ||
            dirty_test(input_dirty, world_column) != 0u
        );
    }

    neogeo_background_plan_updates(
        first_column,
        forward_delta,
        force_full_rebuild,
        guarded.dirty,
        &plan
    );

    assert(guarded.before == 0xa5u);
    assert(guarded.after == 0x5au);
    for (index = 0u; index < NEOGEO_BACKGROUND_DIRTY_BYTES; ++index) {
        assert(guarded.dirty[index] == 0u);
    }
    assert(plan.count <= NEOGEO_BACKGROUND_VISIBLE_STRIPS);
    for (index = 0u; index < plan.count; ++index) {
        logical = plan.logical_strips[index];
        assert(logical < NEOGEO_BACKGROUND_VISIBLE_STRIPS);
        assert(observed[logical] == 0u);
        observed[logical] = 1u;
    }
    assert(memcmp(expected, observed, sizeof(expected)) == 0);
}

static void exhaustive_empty_and_single_column_cases(void) {
    uint8_t dirty[NEOGEO_BACKGROUND_DIRTY_BYTES];
    uint8_t first;
    uint8_t delta;
    uint8_t column;

    for (first = 0u; first < 64u; ++first) {
        for (delta = 0u; delta < 64u; ++delta) {
            memset(dirty, 0, sizeof(dirty));
            check_case(first, delta, 0u, dirty);
            check_case(first, delta, 1u, dirty);
            for (column = 0u; column < 64u; ++column) {
                memset(dirty, 0, sizeof(dirty));
                dirty[column >> 3] =
                    (uint8_t)((uint8_t)1u << (column & 7u));
                check_case(first, delta, 0u, dirty);
            }
        }
    }
}

static void exhaustive_dirty_byte_cases(void) {
    static const uint8_t deltas[] = {0u, 1u, 2u, 16u, 32u, 33u, 63u};
    uint8_t dirty[NEOGEO_BACKGROUND_DIRTY_BYTES];
    uint8_t first;
    uint8_t byte_index;
    uint16_t value;
    size_t delta_index;

    for (first = 0u; first < 64u; ++first) {
        for (delta_index = 0u; delta_index < sizeof(deltas); ++delta_index) {
            for (
                byte_index = 0u;
                byte_index < NEOGEO_BACKGROUND_DIRTY_BYTES;
                ++byte_index
            ) {
                for (value = 0u; value <= 255u; ++value) {
                    memset(dirty, 0, sizeof(dirty));
                    dirty[byte_index] = (uint8_t)value;
                    check_case(
                        first,
                        deltas[delta_index],
                        0u,
                        dirty
                    );
                }
            }
        }
    }
}

static void combined_pattern_cases(void) {
    uint8_t dirty[NEOGEO_BACKGROUND_DIRTY_BYTES];
    uint32_t random = UINT32_C(0x6d2b79f5);
    uint32_t iteration;

    memset(dirty, 0xff, sizeof(dirty));
    for (iteration = 0u; iteration < 64u; ++iteration) {
        check_case(
            (uint8_t)iteration,
            (uint8_t)(iteration & 63u),
            0u,
            dirty
        );
    }

    for (iteration = 0u; iteration < 100000u; ++iteration) {
        uint8_t byte_index;

        for (
            byte_index = 0u;
            byte_index < NEOGEO_BACKGROUND_DIRTY_BYTES;
            ++byte_index
        ) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            dirty[byte_index] = (uint8_t)random;
        }
        check_case(
            (uint8_t)(random & 63u),
            (uint8_t)((random >> 6) & 63u),
            (uint8_t)((random >> 12) & 1u),
            dirty
        );
    }
}

typedef struct {
    uint32_t generations[2][NEOGEO_BACKGROUND_VISIBLE_STRIPS];
    uint32_t rendered_content[2][NEOGEO_BACKGROUND_VISIBLE_STRIPS];
    uint8_t world_columns[2][NEOGEO_BACKGROUND_VISIBLE_STRIPS];
    uint32_t built_generation[2];
    uint8_t config[2];
    uint8_t first_column[2];
    uint8_t ring_origin[2];
    uint8_t ring_valid[2];
} SimulatedRenderer;

static void simulated_renderer_init(SimulatedRenderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));
    memset(renderer->generations, 0xff, sizeof(renderer->generations));
    memset(renderer->rendered_content, 0xff,
        sizeof(renderer->rendered_content));
    memset(renderer->world_columns, 0xff, sizeof(renderer->world_columns));
    memset(renderer->built_generation, 0xff, sizeof(renderer->built_generation));
    memset(renderer->config, 0xff, sizeof(renderer->config));
}

static uint8_t simulated_prepare_ring(
    SimulatedRenderer *renderer,
    uint8_t bank,
    uint8_t first_column,
    uint8_t config,
    uint8_t *forward_delta
) {
    uint8_t reset_config = (uint8_t)(renderer->config[bank] != config);
    uint8_t was_valid = renderer->ring_valid[bank];

    *forward_delta = 0u;
    if (reset_config != 0u) {
        memset(renderer->generations[bank], 0xff,
            sizeof(renderer->generations[bank]));
        renderer->config[bank] = config;
    }
    if (was_valid != 0u) {
        uint8_t origin;

        *forward_delta = (uint8_t)(
            (first_column - renderer->first_column[bank]) & 63u
        );
        if (*forward_delta < NEOGEO_BACKGROUND_VISIBLE_STRIPS) {
            origin = (uint8_t)(
                renderer->ring_origin[bank] + *forward_delta
            );
            if (origin >= NEOGEO_BACKGROUND_VISIBLE_STRIPS) {
                origin = (uint8_t)(
                    origin - NEOGEO_BACKGROUND_VISIBLE_STRIPS
                );
            }
            renderer->ring_origin[bank] = origin;
        } else {
            renderer->ring_origin[bank] = 0u;
            memset(renderer->generations[bank], 0xff,
                sizeof(renderer->generations[bank]));
            memset(renderer->world_columns[bank], 0xff,
                sizeof(renderer->world_columns[bank]));
        }
    } else {
        renderer->ring_valid[bank] = 1u;
        renderer->ring_origin[bank] = 0u;
        memset(renderer->generations[bank], 0xff,
            sizeof(renderer->generations[bank]));
        memset(renderer->world_columns[bank], 0xff,
            sizeof(renderer->world_columns[bank]));
    }
    renderer->first_column[bank] = first_column;
    return (uint8_t)(was_valid == 0u || reset_config != 0u);
}

static void simulated_update_strip(
    SimulatedRenderer *renderer,
    uint8_t bank,
    uint8_t first_column,
    uint8_t logical_strip,
    uint8_t mode,
    uint8_t unconditional_update,
    const uint32_t column_generations[64],
    const uint32_t content_generations[2][64]
) {
    uint8_t physical_strip = (uint8_t)(
        renderer->ring_origin[bank] + logical_strip
    );
    uint8_t world_column = (uint8_t)(
        (first_column + logical_strip) & 63u
    );

    if (physical_strip >= NEOGEO_BACKGROUND_VISIBLE_STRIPS) {
        physical_strip = (uint8_t)(
            physical_strip - NEOGEO_BACKGROUND_VISIBLE_STRIPS
        );
    }
    if (
        unconditional_update != 0u ||
        renderer->world_columns[bank][physical_strip] != world_column ||
        renderer->generations[bank][physical_strip] !=
            column_generations[world_column]
    ) {
        renderer->world_columns[bank][physical_strip] = world_column;
        renderer->generations[bank][physical_strip] =
            column_generations[world_column];
        renderer->rendered_content[bank][physical_strip] =
            content_generations[mode][world_column];
    }
}

static void differential_alternating_bank_simulation(void) {
    SimulatedRenderer reference;
    SimulatedRenderer candidate;
    uint8_t pending[2][2][NEOGEO_BACKGROUND_DIRTY_BYTES];
    uint32_t column_generations[64];
    uint32_t content_generations[2][64];
    uint32_t source_generations[2] = {0u, 0u};
    uint32_t random = UINT32_C(0x91e10da5);
    uint8_t scroll = 0u;
    uint32_t frame;

    simulated_renderer_init(&reference);
    simulated_renderer_init(&candidate);
    memset(pending, 0, sizeof(pending));
    memset(column_generations, 0, sizeof(column_generations));
    memset(content_generations, 0, sizeof(content_generations));

    for (frame = 0u; frame < 250000u; ++frame) {
        uint8_t write_count;
        uint8_t write_index;
        uint8_t bank = (uint8_t)(frame & 1u);
        uint8_t mode;
        uint8_t config;
        uint8_t reference_delta;
        uint8_t candidate_delta;
        uint8_t reference_force;
        uint8_t candidate_force;
        uint8_t reference_scan;
        NeoGeoBackgroundUpdatePlan plan;
        uint8_t logical;

        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        write_count = (uint8_t)(random & 3u);
        for (write_index = 0u; write_index < write_count; ++write_index) {
            uint8_t column;
            uint8_t relevance;
            uint8_t byte_index;
            uint8_t bit;
            uint8_t mode_index;

            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            column = (uint8_t)(random & 63u);
            relevance = (uint8_t)((random >> 6) & 3u);
            ++column_generations[column];
            byte_index = (uint8_t)(column >> 3);
            bit = (uint8_t)((uint8_t)1u << (column & 7u));
            for (mode_index = 0u; mode_index < 2u; ++mode_index) {
                if ((relevance & ((uint8_t)1u << mode_index)) != 0u) {
                    ++content_generations[mode_index][column];
                    pending[mode_index][0][byte_index] |= bit;
                    pending[mode_index][1][byte_index] |= bit;
                    ++source_generations[mode_index];
                }
            }
        }

        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        if ((random & 31u) < 23u) {
            scroll = (uint8_t)((scroll + (random & 1u)) & 63u);
        } else if ((random & 31u) == 31u) {
            scroll = (uint8_t)((random >> 8) & 63u);
        }
        mode = (uint8_t)((random >> 14) & 1u);
        config = mode;

        if ((frame % 8191u) == 8190u) {
            simulated_renderer_init(&reference);
            simulated_renderer_init(&candidate);
        }

        reference_scan = (uint8_t)(
            reference.ring_valid[bank] == 0u ||
            reference.config[bank] != config ||
            reference.first_column[bank] != scroll ||
            reference.built_generation[bank] != source_generations[mode]
        );
        reference_force = simulated_prepare_ring(
            &reference,
            bank,
            scroll,
            config,
            &reference_delta
        );
        candidate_force = simulated_prepare_ring(
            &candidate,
            bank,
            scroll,
            config,
            &candidate_delta
        );
        assert(reference_delta == candidate_delta);
        assert(reference_force == candidate_force);
        if (reference_delta >= NEOGEO_BACKGROUND_VISIBLE_STRIPS) {
            reference_force = 1u;
            candidate_force = 1u;
        }

        if (reference_scan != 0u) {
            for (
                logical = 0u;
                logical < NEOGEO_BACKGROUND_VISIBLE_STRIPS;
                ++logical
            ) {
                simulated_update_strip(
                    &reference,
                    bank,
                    scroll,
                    logical,
                    mode,
                    0u,
                    column_generations,
                    content_generations
                );
            }
        }
        reference.built_generation[bank] = source_generations[mode];

        neogeo_background_plan_updates(
            scroll,
            candidate_delta,
            candidate_force,
            pending[mode][bank],
            &plan
        );
        for (logical = 0u; logical < plan.count; ++logical) {
            simulated_update_strip(
                &candidate,
                bank,
                scroll,
                plan.logical_strips[logical],
                mode,
                1u,
                column_generations,
                content_generations
            );
        }
        candidate.built_generation[bank] = source_generations[mode];

        assert(memcmp(
            reference.rendered_content,
            candidate.rendered_content,
            sizeof(reference.rendered_content)
        ) == 0);
        assert(memcmp(
            reference.world_columns,
            candidate.world_columns,
            sizeof(reference.world_columns)
        ) == 0);
        assert(memcmp(
            reference.first_column,
            candidate.first_column,
            sizeof(reference.first_column)
        ) == 0);
        assert(memcmp(
            reference.ring_origin,
            candidate.ring_origin,
            sizeof(reference.ring_origin)
        ) == 0);
    }
}

int main(void) {
    exhaustive_empty_and_single_column_cases();
    exhaustive_dirty_byte_cases();
    combined_pattern_cases();
    differential_alternating_bank_simulation();
    puts("Neo Geo background update-plan tests: OK");
    return 0;
}
