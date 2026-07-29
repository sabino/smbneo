#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "ppu.h"
#include "ppu_render_state.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t ram[RAM_SIZE];
    uint8_t nametable[NAMETABLE_SIZE];
    Palette palette;
    uint8_t oam[OAM_SIZE];
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    bool carry;
    uint8_t nz;
    uint8_t controller1_state;
    uint8_t controller2_state;
    bool controller1_strobe;
    bool controller2_strobe;
    uint8_t controller1_btn_index;
    uint8_t controller2_btn_index;
    uint16_t ppu_v;
    uint8_t ppu_w;
    uint8_t ppu_f;
    uint8_t ppu_ctrl;
    uint8_t ppu_mask;
    uint8_t ppu_status;
    uint8_t oam_addr;
    uint8_t ppu_scroll_x;
    uint8_t ppu_scroll_y;
    uint16_t vram_addr;
    uint8_t vram_internal_buffer;
    uint8_t oam_dma;
    uint32_t column_generation[64];
    uint32_t background_full_generation;
    uint32_t background_hud_generation;
    uint32_t hud_generation;
    uint32_t palette_generation;
    uint32_t hud_dirty_rows[3];
    uint8_t hud_dirty_tracking_valid;
    uint8_t next_status;
} ObservableState;

static uint64_t pipeline_comparisons;
static uint64_t helper_comparisons;
static uint64_t accepted_pipeline_runs;
static uint64_t rejected_pipeline_runs;

void neogeo_video_render(void) {
}

static uint8_t pattern_byte(uint32_t seed, uint32_t index) {
    uint32_t value = seed + index * UINT32_C(0x9e3779b9);

    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return (uint8_t)value;
}

static void prepare_state(uint32_t seed) {
    uint16_t index;

    cpu_init();
    ppu_init(NULL);

    for (index = 0u; index < RAM_SIZE; ++index) {
        ram[index] = pattern_byte(seed, index);
    }
    for (index = 0u; index < NAMETABLE_SIZE; ++index) {
        nametable[index] = pattern_byte(seed ^ UINT32_C(0x3ad8025f), index);
    }
    for (index = 0u; index < PALETTE_SIZE; ++index) {
        palette.u8[index] = (uint8_t)(pattern_byte(seed, index + 2048u) & 0x3fu);
    }
    for (index = 0u; index < OAM_SIZE; ++index) {
        oam[index] = pattern_byte(seed ^ UINT32_C(0x718293a4), index);
    }

    a = pattern_byte(seed, 3000u);
    x = pattern_byte(seed, 3001u);
    y = pattern_byte(seed, 3002u);
    sp = (uint8_t)(0xc0u | (seed & 0x1fu));
    carry_flag = (seed & 1u) != 0u;
    nz_value = pattern_byte(seed, 3003u);
    controller1_state = pattern_byte(seed, 3004u);
    controller2_state = pattern_byte(seed, 3005u);
    controller1_strobe = (seed & 2u) != 0u;
    controller2_strobe = (seed & 4u) != 0u;
    controller1_btn_index = (uint8_t)(seed & 0x0fu);
    controller2_btn_index = (uint8_t)((seed >> 4) & 0x0fu);

    ppu_v = (uint16_t)(seed & 0x7fffu);
    ppu_w = (uint8_t)((seed >> 1) & 1u);
    ppu_f = (uint8_t)((seed >> 2) & 1u);
    ppu_ctrl = pattern_byte(seed, 3010u);
    ppu_mask = pattern_byte(seed, 3011u);
    ppu_status = pattern_byte(seed, 3012u);
    oam_addr = pattern_byte(seed, 3013u);
    ppu_scroll_x = pattern_byte(seed, 3014u);
    ppu_scroll_y = pattern_byte(seed, 3015u);
    vram_addr = (uint16_t)(seed & 0x3fffu);
    vram_internal_buffer = pattern_byte(seed, 3016u);
    oam_dma = pattern_byte(seed, 3017u);

    for (index = 0u; index < 64u; ++index) {
        neogeo_ppu_column_generation[index] =
            UINT32_MAX - (uint32_t)index - seed;
    }
    neogeo_ppu_background_full_generation = UINT32_MAX - seed;
    neogeo_ppu_background_hud_generation =
        UINT32_MAX - seed * UINT32_C(3);
    neogeo_ppu_hud_generation = UINT32_MAX - seed * UINT32_C(5);
    neogeo_ppu_palette_generation = UINT32_MAX - seed * UINT32_C(7);
    neogeo_ppu_hud_dirty_rows[0] = seed ^ UINT32_C(0x00000001);
    neogeo_ppu_hud_dirty_rows[1] = seed ^ UINT32_C(0x80000000);
    neogeo_ppu_hud_dirty_rows[2] = seed ^ UINT32_C(0x55aa55aa);
    neogeo_ppu_hud_dirty_tracking_valid = (uint8_t)(seed & 1u);

    ram[Mirror_PPU_CTRL_REG1] = pattern_byte(seed, 3020u);
}

static void capture_state(ObservableState *state) {
    /* Reading the next status byte exposes the otherwise-private phase. */
    memset(state, 0, sizeof(*state));
    state->next_status = ppu_read_register(0x2002u);
    memcpy(state->ram, ram, sizeof(state->ram));
    memcpy(state->nametable, nametable, sizeof(state->nametable));
    memcpy(&state->palette, &palette, sizeof(state->palette));
    memcpy(state->oam, oam, sizeof(state->oam));
    state->a = a;
    state->x = x;
    state->y = y;
    state->sp = sp;
    state->carry = carry_flag;
    state->nz = nz_value;
    state->controller1_state = controller1_state;
    state->controller2_state = controller2_state;
    state->controller1_strobe = controller1_strobe;
    state->controller2_strobe = controller2_strobe;
    state->controller1_btn_index = controller1_btn_index;
    state->controller2_btn_index = controller2_btn_index;
    state->ppu_v = ppu_v;
    state->ppu_w = ppu_w;
    state->ppu_f = ppu_f;
    state->ppu_ctrl = ppu_ctrl;
    state->ppu_mask = ppu_mask;
    state->ppu_status = ppu_status;
    state->oam_addr = oam_addr;
    state->ppu_scroll_x = ppu_scroll_x;
    state->ppu_scroll_y = ppu_scroll_y;
    state->vram_addr = vram_addr;
    state->vram_internal_buffer = vram_internal_buffer;
    state->oam_dma = oam_dma;
    memcpy(
        state->column_generation,
        neogeo_ppu_column_generation,
        sizeof(state->column_generation)
    );
    state->background_full_generation =
        neogeo_ppu_background_full_generation;
    state->background_hud_generation =
        neogeo_ppu_background_hud_generation;
    state->hud_generation = neogeo_ppu_hud_generation;
    state->palette_generation = neogeo_ppu_palette_generation;
    memcpy(
        state->hud_dirty_rows,
        neogeo_ppu_hud_dirty_rows,
        sizeof(state->hud_dirty_rows)
    );
    state->hud_dirty_tracking_valid = neogeo_ppu_hud_dirty_tracking_valid;
}

static void assert_state_equal(
    const ObservableState *expected,
    const ObservableState *actual,
    const char *context
) {
    if (memcmp(expected, actual, sizeof(*expected)) != 0) {
        fprintf(stderr, "VRAM batch differential mismatch: %s\n", context);
        assert(memcmp(expected->ram, actual->ram, RAM_SIZE) == 0);
        assert(memcmp(expected->nametable, actual->nametable, NAMETABLE_SIZE) == 0);
        assert(memcmp(&expected->palette, &actual->palette, sizeof(Palette)) == 0);
        assert(memcmp(expected->oam, actual->oam, OAM_SIZE) == 0);
        assert(expected->a == actual->a);
        assert(expected->x == actual->x);
        assert(expected->y == actual->y);
        assert(expected->sp == actual->sp);
        assert(expected->carry == actual->carry);
        assert(expected->nz == actual->nz);
        assert(expected->vram_addr == actual->vram_addr);
        assert(expected->ppu_ctrl == actual->ppu_ctrl);
        assert(expected->ppu_w == actual->ppu_w);
        assert(expected->next_status == actual->next_status);
        assert(memcmp(expected, actual, sizeof(*expected)) == 0);
    }
}

static uint16_t physical_ram_address(uint16_t cpu_address) {
    assert(cpu_address < 0x2000u);
    return (uint16_t)(cpu_address & (RAM_SIZE - 1u));
}

static void write_cpu_ram(uint16_t cpu_address, uint8_t value) {
    ram[physical_ram_address(cpu_address)] = value;
}

static uint16_t record_size(uint8_t control) {
    uint16_t length = (uint16_t)(control & 0x3fu);

    if ((control & 0x40u) != 0u) {
        return 4u;
    }
    return (uint16_t)(length + 3u);
}

static void write_record(
    uint16_t pointer,
    uint16_t destination,
    uint8_t control,
    uint32_t seed
) {
    uint16_t length = (uint16_t)(control & 0x3fu);
    uint16_t write_count = length == 0u ? 256u : length;
    uint16_t data_count = (control & 0x40u) != 0u ? 1u : write_count;
    uint16_t index;

    assert((uint32_t)pointer + 3u + data_count < 0x2000u);
    write_cpu_ram(pointer, (uint8_t)(destination >> 8));
    write_cpu_ram((uint16_t)(pointer + 1u), (uint8_t)destination);
    write_cpu_ram((uint16_t)(pointer + 2u), control);
    for (index = 0u; index < data_count; ++index) {
        write_cpu_ram(
            (uint16_t)(pointer + 3u + index),
            pattern_byte(seed, index)
        );
    }

    /* A zero non-repeat run advances only three bytes after its 256 writes. */
    write_cpu_ram((uint16_t)(pointer + record_size(control)), 0u);
}

static bool should_batch(
    uint16_t source_pointer,
    uint16_t destination,
    uint8_t control
) {
    uint32_t length = control & 0x3fu;
    uint32_t increment = (control & 0x80u) != 0u ? 32u : 1u;
    uint32_t source_last;
    uint32_t destination_last;

    destination &= 0x3fffu;
    if (
        length == 0u ||
        source_pointer < 0x0300u ||
        source_pointer > 0x03ffu
    ) {
        return false;
    }
    source_last = (uint32_t)source_pointer + 3u;
    if ((control & 0x40u) == 0u) {
        source_last += length - 1u;
    }
    destination_last = destination + (length - 1u) * increment;
    return
        source_last <= 0x03ffu &&
        destination >= 0x2000u &&
        destination < 0x3f00u &&
        destination_last < 0x3f00u;
}

static void prepare_single_pipeline(
    uint32_t seed,
    uint16_t source_pointer,
    uint16_t destination,
    uint8_t control
) {
    prepare_state(seed);
    write_record(source_pointer, destination, control, seed ^ UINT32_C(0x52414d31));
    ram[0] = (uint8_t)source_pointer;
    ram[1] = (uint8_t)(source_pointer >> 8);
}

static void compare_pipeline_case(
    uint32_t seed,
    uint16_t source_pointer,
    uint16_t destination,
    uint8_t control,
    const char *context
) {
    ObservableState expected;
    ObservableState actual;
    bool expected_batch = should_batch(source_pointer, destination, control);
    bool record_present = (destination >> 8) != 0u;
    uint32_t batched_count;
    uint32_t rejected_count;

    prepare_single_pipeline(seed, source_pointer, destination, control);
    neogeo_ppu_batch_test_enabled = 0u;
    UpdateScreen();
    capture_state(&expected);

    prepare_single_pipeline(seed, source_pointer, destination, control);
    neogeo_ppu_batch_test_enabled = 1u;
    UpdateScreen();
    batched_count = neogeo_ppu_batched_run_count;
    rejected_count = neogeo_ppu_rejected_run_count;
    capture_state(&actual);

    assert_state_equal(&expected, &actual, context);
    assert(batched_count == (expected_batch ? 1u : 0u));
    assert(rejected_count == (expected_batch ? 0u : (record_present ? 1u : 0u)));
    accepted_pipeline_runs += batched_count;
    rejected_pipeline_runs += rejected_count;
    ++pipeline_comparisons;
}

static void compare_all_control_bytes(void) {
    uint16_t control;

    for (control = 0u; control < 256u; ++control) {
        compare_pipeline_case(
            UINT32_C(0x10100000) + control,
            0x0320u,
            0x2400u,
            (uint8_t)control,
            "all 256 control bytes"
        );
    }
}

static void compare_address_and_source_boundaries(void) {
    static const uint16_t destinations[] = {
        0x0000u, 0x1fffu, 0x2000u, 0x2020u, 0x23c0u, 0x23ffu,
        0x2400u, 0x27c0u, 0x2800u, 0x2be0u, 0x2ff0u, 0x3000u,
        0x37ffu, 0x3edfu, 0x3ee0u, 0x3effu, 0x3f00u, 0x3f10u,
        0x3f1cu, 0x4000u, 0x7f10u,
    };
    static const uint16_t sources[] = {
        0x0100u, 0x02ffu, 0x0300u, 0x03bdu, 0x03beu, 0x03fbu,
        0x03fcu, 0x03ffu, 0x0400u, 0x0700u, 0x0b00u, 0x17b0u,
    };
    static const uint8_t controls[] = {
        0x01u, 0x02u, 0x3fu, 0x41u, 0x7fu, 0x81u, 0x82u,
        0xbfu, 0xc1u, 0xffu, 0x00u, 0x40u, 0x80u, 0xc0u,
    };
    size_t destination_index;
    size_t source_index;
    size_t control_index;
    uint32_t seed = UINT32_C(0x22000000);

    for (destination_index = 0u;
         destination_index < sizeof(destinations) / sizeof(destinations[0]);
         ++destination_index) {
        for (control_index = 0u;
             control_index < sizeof(controls) / sizeof(controls[0]);
             ++control_index) {
            compare_pipeline_case(
                seed++,
                0x0320u,
                destinations[destination_index],
                controls[control_index],
                "destination wrap/mirror/palette/pattern boundary"
            );
        }
    }

    for (source_index = 0u;
         source_index < sizeof(sources) / sizeof(sources[0]);
         ++source_index) {
        for (control_index = 0u;
             control_index < sizeof(controls) / sizeof(controls[0]);
             ++control_index) {
            compare_pipeline_case(
                seed++,
                sources[source_index],
                0x2400u,
                controls[control_index],
                "source page edge and mirrored-RAM alias"
            );
        }
    }
}

static void prepare_multi_record_pipeline(uint32_t seed) {
    uint16_t pointer = 0x0310u;

    prepare_state(seed);
    write_record(pointer, 0x23f8u, 0x10u, seed + 1u);
    pointer = (uint16_t)(pointer + record_size(0x10u));
    write_record(pointer, 0x2f20u, 0x88u, seed + 2u);
    pointer = (uint16_t)(pointer + record_size(0x88u));
    write_record(pointer, 0x3f10u, 0x44u, seed + 3u);
    pointer = (uint16_t)(pointer + record_size(0x44u));
    write_record(pointer, 0x3000u, 0x46u, seed + 4u);
    pointer = (uint16_t)(pointer + record_size(0x46u));
    write_cpu_ram(pointer, 0u);
    ram[0] = 0x10u;
    ram[1] = 0x03u;
}

static void compare_multi_record_chain(void) {
    ObservableState expected;
    ObservableState actual;
    uint32_t seed;

    for (seed = 0u; seed < 64u; ++seed) {
        prepare_multi_record_pipeline(UINT32_C(0x33000000) + seed);
        neogeo_ppu_batch_test_enabled = 0u;
        UpdateScreen();
        capture_state(&expected);

        prepare_multi_record_pipeline(UINT32_C(0x33000000) + seed);
        neogeo_ppu_batch_test_enabled = 1u;
        UpdateScreen();
        assert(neogeo_ppu_batched_run_count == 3u);
        assert(neogeo_ppu_rejected_run_count == 1u);
        capture_state(&actual);
        assert_state_equal(&expected, &actual, "mixed multi-record recursion");
        accepted_pipeline_runs += 3u;
        rejected_pipeline_runs += 1u;
        ++pipeline_comparisons;
    }
}

static void prepare_helper_case(
    uint32_t seed,
    uint16_t source_pointer,
    uint16_t destination,
    uint8_t length,
    bool repeat,
    bool vertical
) {
    uint16_t index;

    prepare_state(seed);
    ppu_ctrl = (uint8_t)((ppu_ctrl & (uint8_t)~0x04u) | (vertical ? 0x04u : 0u));
    vram_addr = (uint16_t)(destination & 0x3fffu);
    for (index = 0u; index < length; ++index) {
        ram[source_pointer + 3u + (repeat ? 0u : index)] =
            pattern_byte(seed ^ UINT32_C(0xa53c2198), index);
    }
}

static void compare_accepted_helper_case(
    uint32_t seed,
    uint16_t source_pointer,
    uint16_t destination,
    uint8_t length,
    bool repeat,
    bool vertical
) {
    ObservableState expected;
    ObservableState actual;
    uint16_t index;

    prepare_helper_case(
        seed, source_pointer, destination, length, repeat, vertical
    );
    for (index = 0u; index < length; ++index) {
        ppu_write_data(ram[source_pointer + 3u + (repeat ? 0u : index)]);
    }
    capture_state(&expected);

    prepare_helper_case(
        seed, source_pointer, destination, length, repeat, vertical
    );
    neogeo_ppu_batch_test_enabled = 1u;
    assert(ppu_write_buffer_run(
        source_pointer, length, repeat ? 1u : 0u
    ));
    assert(neogeo_ppu_batched_run_count == 1u);
    capture_state(&actual);
    assert_state_equal(&expected, &actual, "direct helper increment/generation");
    ++helper_comparisons;
}

static void compare_helper_increment_and_generation(void) {
    static const uint16_t destinations[] = {
        0x2000u, 0x201fu, 0x23bfu, 0x23c0u, 0x2400u,
        0x27ffu, 0x2fe0u, 0x3000u, 0x37ffu, 0x3e00u,
    };
    static const uint8_t lengths[] = { 1u, 2u, 3u, 7u, 16u, 31u, 63u };
    size_t destination_index;
    size_t length_index;
    uint8_t repeat;
    uint8_t vertical;
    uint32_t seed = UINT32_C(0x44000000);

    for (destination_index = 0u;
         destination_index < sizeof(destinations) / sizeof(destinations[0]);
         ++destination_index) {
        for (length_index = 0u;
             length_index < sizeof(lengths) / sizeof(lengths[0]);
             ++length_index) {
            for (repeat = 0u; repeat < 2u; ++repeat) {
                for (vertical = 0u; vertical < 2u; ++vertical) {
                    uint8_t length = lengths[length_index];
                    uint32_t last = destinations[destination_index] +
                        (uint32_t)(length - 1u) * (vertical != 0u ? 32u : 1u);

                    if (last >= 0x3f00u) {
                        continue;
                    }
                    compare_accepted_helper_case(
                        seed++,
                        0x0310u,
                        destinations[destination_index],
                        length,
                        repeat != 0u,
                        vertical != 0u
                    );
                }
            }
        }
    }
}

static void compare_rejected_helper_is_noop(void) {
    static const uint16_t sources[] = {
        0x0000u, 0x0100u, 0x02ffu, 0x0400u, 0x0b00u,
        0x1ff0u, 0x2000u, 0x4016u, 0x8000u, 0xffffu,
    };
    static const uint16_t destinations[] = {
        0x0000u, 0x1fffu, 0x3f00u, 0x3f10u, 0x3effu,
    };
    size_t index;
    ObservableState expected;
    ObservableState actual;
    uint32_t seed = UINT32_C(0x55000000);

    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        prepare_state(seed);
        capture_state(&expected);
        prepare_state(seed++);
        neogeo_ppu_batch_test_enabled = 1u;
        assert(!ppu_write_buffer_run(sources[index], 3u, 0u));
        capture_state(&actual);
        assert_state_equal(&expected, &actual, "rejected CPU alias/device/ROM source");
        ++helper_comparisons;
    }

    for (index = 0u;
         index < sizeof(destinations) / sizeof(destinations[0]);
         ++index) {
        prepare_helper_case(seed, 0x0310u, destinations[index], 2u, false, false);
        capture_state(&expected);
        prepare_helper_case(seed++, 0x0310u, destinations[index], 2u, false, false);
        neogeo_ppu_batch_test_enabled = 1u;
        assert(!ppu_write_buffer_run(0x0310u, 2u, 0u));
        capture_state(&actual);
        assert_state_equal(&expected, &actual, "rejected pattern/palette/crossing destination");
        ++helper_comparisons;
    }

    prepare_helper_case(seed, 0x0310u, 0x2400u, 1u, false, false);
    capture_state(&expected);
    prepare_helper_case(seed, 0x0310u, 0x2400u, 1u, false, false);
    neogeo_ppu_batch_test_enabled = 1u;
    assert(!ppu_write_buffer_run(0x0310u, 0u, 0u));
    capture_state(&actual);
    assert_state_equal(&expected, &actual, "zero-length 256-write fallback");
    ++helper_comparisons;
}

int main(void) {
    compare_all_control_bytes();
    compare_address_and_source_boundaries();
    compare_multi_record_chain();
    compare_helper_increment_and_generation();
    compare_rejected_helper_is_noop();

    assert(accepted_pipeline_runs != 0u);
    assert(rejected_pipeline_runs != 0u);
    printf(
        "Neo Geo VRAM-buffer batch differential: PASS "
        "(pipeline=%" PRIu64 ", helper=%" PRIu64
        ", accepted=%" PRIu64 ", rejected=%" PRIu64 ")\n",
        pipeline_comparisons,
        helper_comparisons,
        accepted_pipeline_runs,
        rejected_pipeline_runs
    );
    return 0;
}
