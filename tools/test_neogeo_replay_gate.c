#include "replay_gate.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static NeogeoReplaySnapshot snapshot(
    uint8_t mode,
    uint8_t task,
    uint8_t world,
    uint8_t level,
    uint8_t world_end_timer
) {
    NeogeoReplaySnapshot value;

    value.oper_mode = mode;
    value.oper_mode_task = task;
    value.world = world;
    value.level = level;
    value.world_end_timer = world_end_timer;
    return value;
}

static NeogeoReplaySnapshot active_stage(uint8_t stage) {
    return snapshot(
        NEOGEO_REPLAY_GAME_MODE,
        NEOGEO_REPLAY_ACTIVE_TASK,
        (uint8_t)(stage / 4u),
        (uint8_t)(stage % 4u),
        0
    );
}

static NeogeoReplaySnapshot final_victory(void) {
    return snapshot(
        NEOGEO_REPLAY_VICTORY_MODE,
        NEOGEO_REPLAY_VICTORY_TASK,
        7,
        3,
        0
    );
}

static uint32_t prefix_mask(uint8_t bit_count) {
    if (bit_count >= NEOGEO_REPLAY_STAGE_COUNT) {
        return NEOGEO_REPLAY_ALL_STAGES;
    }
    if (bit_count == 0u) {
        return 0;
    }
    return (UINT32_C(1) << bit_count) - UINT32_C(1);
}

static void assert_running(const NeogeoReplayGate *gate) {
    assert(gate->result == NEOGEO_REPLAY_GATE_RUNNING);
    assert(neogeo_replay_gate_passed(gate) == 0);
    assert(neogeo_replay_gate_failed(gate) == 0);
}

static void enter_all_stages(NeogeoReplayGate *gate) {
    uint8_t stage;

    for (stage = 0; stage < NEOGEO_REPLAY_STAGE_COUNT; ++stage) {
        NeogeoReplaySnapshot frame = active_stage(stage);
        NeogeoReplaySnapshot inactive = frame;
        uint32_t entered_before = prefix_mask(stage);
        uint32_t completed_before = prefix_mask(
            stage == 0u ? 0u : (uint8_t)(stage - 1u)
        );

        /*
         * Coordinates may advance during loading, but the stage is not
         * entered until the game engine reaches task 3.
         */
        inactive.oper_mode_task = 2;
        assert(
            neogeo_replay_gate_update(gate, &inactive) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(gate->entered_mask == entered_before);
        assert(gate->completed_mask == completed_before);

        assert(
            neogeo_replay_gate_update(gate, &frame) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(gate->current_stage == stage);
        assert(gate->entered_mask == prefix_mask((uint8_t)(stage + 1u)));
        assert(gate->completed_mask == prefix_mask(stage));

        /* Repeated active frames do not complete or re-enter the stage. */
        assert(
            neogeo_replay_gate_update(gate, &frame) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(gate->entered_mask == prefix_mask((uint8_t)(stage + 1u)));
        assert(gate->completed_mask == prefix_mask(stage));

        /* A death/loading interval and same-stage re-entry are also benign. */
        inactive.oper_mode_task = 0;
        assert(
            neogeo_replay_gate_update(gate, &inactive) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(
            neogeo_replay_gate_update(gate, &frame) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(gate->current_stage == stage);
        assert(gate->entered_mask == prefix_mask((uint8_t)(stage + 1u)));
        assert(gate->completed_mask == prefix_mask(stage));
    }
}

static void test_all_stages_and_final_threshold(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot victory = final_victory();
    unsigned int frame;

    neogeo_replay_gate_init(&gate);
    assert(gate.current_stage == NEOGEO_REPLAY_NO_STAGE);
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);
    assert_running(&gate);

    enter_all_stages(&gate);
    assert(gate.entered_mask == NEOGEO_REPLAY_ALL_STAGES);
    assert(
        gate.completed_mask ==
        (NEOGEO_REPLAY_ALL_STAGES >> 1)
    );

    for (
        frame = 1;
        frame < NEOGEO_REPLAY_FINAL_STABLE_FRAMES;
        ++frame
    ) {
        assert(
            neogeo_replay_gate_update(&gate, &victory) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
        assert(gate.victory_stable_frames == frame);
        assert(
            gate.completed_mask ==
            (NEOGEO_REPLAY_ALL_STAGES >> 1)
        );
    }

    assert(
        neogeo_replay_gate_update(&gate, &victory) ==
        NEOGEO_REPLAY_GATE_COMPLETE
    );
    assert(
        gate.victory_stable_frames ==
        NEOGEO_REPLAY_FINAL_STABLE_FRAMES
    );
    assert(gate.completed_mask == NEOGEO_REPLAY_ALL_STAGES);
    assert(neogeo_replay_gate_passed(&gate) != 0);
    assert(neogeo_replay_gate_failed(&gate) == 0);

    /* Completion and the saturated counter remain stable on later frames. */
    assert(
        neogeo_replay_gate_update(&gate, &victory) ==
        NEOGEO_REPLAY_GATE_COMPLETE
    );
    assert(
        gate.victory_stable_frames ==
        NEOGEO_REPLAY_FINAL_STABLE_FRAMES
    );
    assert(gate.completed_mask == NEOGEO_REPLAY_ALL_STAGES);
}

static void test_only_active_frames_enter_stages(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot frame;

    neogeo_replay_gate_init(&gate);

    frame = snapshot(0, NEOGEO_REPLAY_ACTIVE_TASK, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.entered_mask == 0);

    frame = snapshot(NEOGEO_REPLAY_GAME_MODE, 2, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.entered_mask == 0);

    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.entered_mask == UINT32_C(1));
    assert(gate.completed_mask == 0);

    frame = snapshot(NEOGEO_REPLAY_VICTORY_MODE, 0, 0, 1, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.entered_mask == UINT32_C(1));
    assert(gate.completed_mask == 0);

    frame = active_stage(1);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.entered_mask == UINT32_C(3));
    assert(gate.completed_mask == UINT32_C(1));
}

static void assert_error_is_sticky(
    NeogeoReplayGate *gate,
    NeogeoReplayGateResult expected
) {
    NeogeoReplaySnapshot benign = active_stage(0);
    uint32_t entered = gate->entered_mask;
    uint32_t completed = gate->completed_mask;

    assert(neogeo_replay_gate_failed(gate) != 0);
    assert(neogeo_replay_gate_passed(gate) == 0);
    assert(neogeo_replay_gate_update(gate, &benign) == expected);
    assert(gate->entered_mask == entered);
    assert(gate->completed_mask == completed);
}

static void test_skips_and_backtracks(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot frame;

    neogeo_replay_gate_init(&gate);
    frame = active_stage(1);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_SKIPPED_STAGE
    );
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);
    assert_error_is_sticky(&gate, NEOGEO_REPLAY_GATE_SKIPPED_STAGE);

    neogeo_replay_gate_init(&gate);
    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = active_stage(2);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_SKIPPED_STAGE
    );
    assert(gate.current_stage == 0);
    assert(gate.entered_mask == UINT32_C(1));
    assert(gate.completed_mask == 0);
    assert_error_is_sticky(&gate, NEOGEO_REPLAY_GATE_SKIPPED_STAGE);

    neogeo_replay_gate_init(&gate);
    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = active_stage(1);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_BACKWARDS_STAGE
    );
    assert(gate.current_stage == 1);
    assert(gate.entered_mask == UINT32_C(3));
    assert(gate.completed_mask == UINT32_C(1));
    assert_error_is_sticky(&gate, NEOGEO_REPLAY_GATE_BACKWARDS_STAGE);
}

static void test_invalid_coordinates(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot frame;

    neogeo_replay_gate_init(&gate);
    frame = snapshot(NEOGEO_REPLAY_GAME_MODE, 3, 8, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_INVALID_STAGE
    );
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);
    assert_error_is_sticky(&gate, NEOGEO_REPLAY_GATE_INVALID_STAGE);

    neogeo_replay_gate_init(&gate);
    frame = snapshot(NEOGEO_REPLAY_GAME_MODE, 3, 0, 4, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_INVALID_STAGE
    );
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);

    neogeo_replay_gate_init(&gate);
    frame = snapshot(0, 0, UINT8_MAX, UINT8_MAX, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_INVALID_STAGE
    );
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);
}

static void test_final_condition_must_be_consecutive_and_exact(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot victory = final_victory();
    NeogeoReplaySnapshot wrong;
    unsigned int frame;

    neogeo_replay_gate_init(&gate);
    enter_all_stages(&gate);

    for (
        frame = 0;
        frame < NEOGEO_REPLAY_FINAL_STABLE_FRAMES - 1u;
        ++frame
    ) {
        assert(
            neogeo_replay_gate_update(&gate, &victory) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
    }
    assert(
        gate.victory_stable_frames ==
        NEOGEO_REPLAY_FINAL_STABLE_FRAMES - 1u
    );

    wrong = victory;
    wrong.world_end_timer = 1;
    assert(
        neogeo_replay_gate_update(&gate, &wrong) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.victory_stable_frames == 0);

    wrong = victory;
    wrong.oper_mode = NEOGEO_REPLAY_GAME_MODE;
    assert(
        neogeo_replay_gate_update(&gate, &wrong) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.victory_stable_frames == 0);

    wrong = victory;
    wrong.oper_mode_task = 3;
    assert(
        neogeo_replay_gate_update(&gate, &wrong) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.victory_stable_frames == 0);

    wrong = victory;
    wrong.world = 6;
    assert(
        neogeo_replay_gate_update(&gate, &wrong) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.victory_stable_frames == 0);

    wrong = victory;
    wrong.level = 2;
    assert(
        neogeo_replay_gate_update(&gate, &wrong) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    assert(gate.victory_stable_frames == 0);

    for (
        frame = 0;
        frame < NEOGEO_REPLAY_FINAL_STABLE_FRAMES;
        ++frame
    ) {
        NeogeoReplayGateResult expected =
            frame + 1u == NEOGEO_REPLAY_FINAL_STABLE_FRAMES
                ? NEOGEO_REPLAY_GATE_COMPLETE
                : NEOGEO_REPLAY_GATE_RUNNING;
        assert(neogeo_replay_gate_update(&gate, &victory) == expected);
    }
    assert(neogeo_replay_gate_passed(&gate) != 0);
}

static void test_final_condition_requires_progression(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot victory = final_victory();
    unsigned int frame;

    neogeo_replay_gate_init(&gate);
    for (
        frame = 0;
        frame < NEOGEO_REPLAY_FINAL_STABLE_FRAMES + 1u;
        ++frame
    ) {
        assert(
            neogeo_replay_gate_update(&gate, &victory) ==
            NEOGEO_REPLAY_GATE_RUNNING
        );
    }
    assert(gate.victory_stable_frames == 0);
    assert(gate.entered_mask == 0);
    assert(gate.completed_mask == 0);
    assert_running(&gate);
}

static void test_game_over_and_title_return_fail_after_gameplay(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot frame;

    neogeo_replay_gate_init(&gate);

    /* Title/game-over modes are harmless before the first active stage. */
    frame = snapshot(0, 1, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = snapshot(3, 0, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );

    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = snapshot(3, 0, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_GAME_OVER
    );
    assert_error_is_sticky(&gate, NEOGEO_REPLAY_GATE_GAME_OVER);

    neogeo_replay_gate_init(&gate);
    frame = active_stage(0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RUNNING
    );
    frame = snapshot(0, 0, 0, 0, 0);
    assert(
        neogeo_replay_gate_update(&gate, &frame) ==
        NEOGEO_REPLAY_GATE_RETURNED_TO_TITLE
    );
    assert_error_is_sticky(
        &gate,
        NEOGEO_REPLAY_GATE_RETURNED_TO_TITLE
    );
}

int main(void) {
    test_all_stages_and_final_threshold();
    test_only_active_frames_enter_stages();
    test_skips_and_backtracks();
    test_invalid_coordinates();
    test_final_condition_must_be_consecutive_and_exact();
    test_final_condition_requires_progression();
    test_game_over_and_title_return_fail_after_gameplay();

    puts("Neo Geo replay-gate tests: OK");
    return 0;
}
