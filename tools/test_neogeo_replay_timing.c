#include "replay_timing.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static int test_default_timing(void) {
    NeogeoReplayTiming timing;
    uint32_t frame;
    uint32_t core_frames = 0;
    int failed = 0;

    memset(&timing, 0, sizeof(timing));
    for (frame = 0; frame < 7; ++frame) {
        if (
            neogeo_replay_timing_should_advance(
                &timing,
                frame,
                0,
                0,
                7,
                1
            )
        ) {
            ++core_frames;
        }
    }
    failed |= expect_u32("bootstrap core frames", core_frames, 0);
    failed |= expect_u32(
        "bootstrap area holds",
        timing.area_init_hold_count,
        0
    );

    if (
        neogeo_replay_timing_should_advance(
            &timing,
            7,
            1,
            0,
            7,
            1
        )
    ) {
        ++core_frames;
    }
    failed |= expect_u32("first area hold core frames", core_frames, 0);
    failed |= expect_u32(
        "first area hold count",
        timing.area_init_hold_count,
        1
    );

    if (
        neogeo_replay_timing_should_advance(
            &timing,
            8,
            1,
            0,
            7,
            1
        )
    ) {
        ++core_frames;
    }
    failed |= expect_u32(
        "continuous area-init episode advances once",
        core_frames,
        1
    );
    failed |= expect_u32(
        "continuous area-init episode holds once",
        timing.area_init_hold_count,
        1
    );

    if (
        neogeo_replay_timing_should_advance(
            &timing,
            9,
            1,
            1,
            7,
            1
        )
    ) {
        ++core_frames;
    }
    if (
        neogeo_replay_timing_should_advance(
            &timing,
            10,
            1,
            0,
            7,
            1
        )
    ) {
        ++core_frames;
    }
    failed |= expect_u32("re-entry core frames", core_frames, 2);
    failed |= expect_u32(
        "re-entry area hold count",
        timing.area_init_hold_count,
        2
    );
    return failed;
}

static int test_configurable_hold_length(void) {
    NeogeoReplayTiming timing;
    uint32_t core_frames = 0;
    uint32_t frame;
    int failed = 0;

    memset(&timing, 0, sizeof(timing));
    for (frame = 0; frame < 3; ++frame) {
        if (
            neogeo_replay_timing_should_advance(
                &timing,
                frame,
                1,
                0,
                0,
                2
            )
        ) {
            ++core_frames;
        }
    }
    failed |= expect_u32("two-frame hold count", timing.area_init_hold_count, 2);
    failed |= expect_u32("two-frame hold core frames", core_frames, 1);

    memset(&timing, 0, sizeof(timing));
    if (
        neogeo_replay_timing_should_advance(
            &timing,
            0,
            1,
            0,
            0,
            0
        )
    ) {
        ++core_frames;
    }
    failed |= expect_u32("zero-frame hold count", timing.area_init_hold_count, 0);
    failed |= expect_u32("zero-frame hold advances", core_frames, 2);
    return failed;
}

int main(void) {
    int failed = 0;

    failed |= test_default_timing();
    failed |= test_configurable_hold_length();
    if (failed != 0) {
        return 1;
    }
    puts("Neo Geo replay-timing tests: OK");
    return 0;
}
