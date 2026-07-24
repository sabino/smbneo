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
    if (failed != 0) {
        return 1;
    }
    puts("Neo Geo replay-checkpoint tests: OK");
    return 0;
}
