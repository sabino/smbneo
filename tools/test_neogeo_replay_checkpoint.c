#include "replay_checkpoint.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int expect_bool(
    const char *name,
    bool actual,
    bool expected
) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %u, got %u\n",
        name,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int expect_u32(
    const char *name,
    uint32_t actual,
    uint32_t expected
) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lu, got %lu\n",
        name,
        (unsigned long)expected,
        (unsigned long)actual
    );
    return 1;
}

static volatile uint32_t present_live_vblank;
static volatile uint32_t present_published_vblank;
static volatile uint16_t present_live_render_generation;
static volatile uint32_t present_published_render_generation;
static volatile uint16_t present_live_presented_generation;
static volatile uint32_t present_published_presented_generation;
static uint8_t present_wait_called;
static uint8_t present_order_failed;

static void fake_presentation_wait(void) {
    if (
        present_published_vblank != 7u ||
        present_published_render_generation != 8u ||
        present_published_presented_generation != 7u
    ) {
        present_order_failed = 1;
    }
    present_wait_called = 1;
    present_live_vblank = 11u;
    present_live_presented_generation =
        present_live_render_generation;
}

static int test_presentation_fence_publishes_after_wait(void) {
    int failed = 0;

    present_live_vblank = 10u;
    present_published_vblank = 7u;
    present_live_render_generation = 9u;
    present_published_render_generation = 8u;
    present_live_presented_generation = 8u;
    present_published_presented_generation = 7u;
    present_wait_called = 0;
    present_order_failed = 0;
    neogeo_replay_checkpoint_present(
        fake_presentation_wait,
        &present_published_vblank,
        &present_live_vblank,
        &present_published_render_generation,
        &present_live_render_generation,
        &present_published_presented_generation,
        &present_live_presented_generation
    );
    failed |= expect_u32(
        "presentation wait called",
        present_wait_called,
        1
    );
    failed |= expect_u32(
        "presentation publish order",
        present_order_failed,
        0
    );
    failed |= expect_u32(
        "presented VBlank published",
        present_published_vblank,
        11
    );
    failed |= expect_u32(
        "render generation published",
        present_published_render_generation,
        9
    );
    failed |= expect_u32(
        "presented generation published",
        present_published_presented_generation,
        9
    );
    return failed;
}

static int test_stage_checkpoint_settles(void) {
    NeogeoReplayCheckpoint checkpoint;
    int failed = 0;

    neogeo_replay_checkpoint_init(&checkpoint, 0);
    failed |= expect_bool(
        "unchanged stage",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 0, 2),
        false
    );
    failed |= expect_bool(
        "new stage",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 1, 2),
        false
    );
    failed |= expect_u32(
        "recorded stage mask",
        checkpoint.observed_entered_mask,
        1
    );
    failed |= expect_bool(
        "first settling frame",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 1, 2),
        false
    );
    failed |= expect_bool(
        "second settling frame",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 1, 2),
        true
    );
    failed |= expect_bool(
        "stage does not repeat",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 1, 2),
        false
    );
    return failed;
}

static int test_zero_settle_and_replacement(void) {
    NeogeoReplayCheckpoint checkpoint;
    int failed = 0;

    neogeo_replay_checkpoint_init(&checkpoint, 0);
    failed |= expect_bool(
        "zero-settle stage",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 1, 0),
        true
    );
    failed |= expect_bool(
        "first pending stage",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 3, 2),
        false
    );
    failed |= expect_bool(
        "replacement stage",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 7, 2),
        false
    );
    failed |= expect_bool(
        "replacement settle one",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 7, 2),
        false
    );
    failed |= expect_bool(
        "replacement settle two",
        neogeo_replay_checkpoint_stage_ready(&checkpoint, 7, 2),
        true
    );
    return failed;
}

int main(void) {
    int failed = 0;

    failed |= test_stage_checkpoint_settles();
    failed |= test_zero_settle_and_replacement();
    failed |= test_presentation_fence_publishes_after_wait();
    if (failed != 0) {
        return 1;
    }
    puts("Neo Geo replay-checkpoint tests: OK");
    return 0;
}
