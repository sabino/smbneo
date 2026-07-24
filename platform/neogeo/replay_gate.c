#include "replay_gate.h"

static uint32_t neogeo_replay_stage_bit(uint8_t stage) {
    /*
     * All callers validate or derive stage in the closed range 0..31 before
     * reaching this helper, so the shift count is always defined.
     */
    return UINT32_C(1) << stage;
}

static NeogeoReplayGateResult neogeo_replay_gate_set_result(
    NeogeoReplayGate *gate,
    NeogeoReplayGateResult result
) {
    gate->result = (uint8_t)result;
    return result;
}

void neogeo_replay_gate_init(NeogeoReplayGate *gate) {
    gate->entered_mask = 0;
    gate->completed_mask = 0;
    gate->current_stage = NEOGEO_REPLAY_NO_STAGE;
    gate->victory_stable_frames = 0;
    gate->result = (uint8_t)NEOGEO_REPLAY_GATE_RUNNING;
}

NeogeoReplayGateResult neogeo_replay_gate_update(
    NeogeoReplayGate *gate,
    const NeogeoReplaySnapshot *snapshot
) {
    uint8_t stage;
    uint8_t active;
    uint8_t stable_final;

    if (gate->result != (uint8_t)NEOGEO_REPLAY_GATE_RUNNING) {
        return (NeogeoReplayGateResult)gate->result;
    }

    /*
     * Validate both coordinates before calculating a stage or shifting. This
     * also catches corrupted state on non-gameplay frames.
     */
    if (snapshot->world >= 8u || snapshot->level >= 4u) {
        gate->victory_stable_frames = 0;
        return neogeo_replay_gate_set_result(
            gate,
            NEOGEO_REPLAY_GATE_INVALID_STAGE
        );
    }

    /*
     * Once gameplay has started, a full-game oracle must neither enter game
     * over nor fall back to the title loop. Failing immediately preserves the
     * exact first terminal frame instead of consuming the rest of a long
     * movie while no longer synchronized.
     */
    if (gate->entered_mask != 0u && snapshot->oper_mode == 3u) {
        return neogeo_replay_gate_set_result(
            gate,
            NEOGEO_REPLAY_GATE_GAME_OVER
        );
    }
    if (gate->entered_mask != 0u && snapshot->oper_mode == 0u) {
        return neogeo_replay_gate_set_result(
            gate,
            NEOGEO_REPLAY_GATE_RETURNED_TO_TITLE
        );
    }

    stage = (uint8_t)(snapshot->world * 4u + snapshot->level);
    active = (uint8_t)(
        snapshot->oper_mode == NEOGEO_REPLAY_GAME_MODE &&
        snapshot->oper_mode_task == NEOGEO_REPLAY_ACTIVE_TASK
    );

    if (active != 0u) {
        if (gate->current_stage == NEOGEO_REPLAY_NO_STAGE) {
            if (stage != 0u) {
                return neogeo_replay_gate_set_result(
                    gate,
                    NEOGEO_REPLAY_GATE_SKIPPED_STAGE
                );
            }

            gate->entered_mask = neogeo_replay_stage_bit(0);
            gate->current_stage = 0;
        } else if (stage == gate->current_stage) {
            /* Repeated frames and re-entry after a death are expected. */
        } else if (
            gate->current_stage < NEOGEO_REPLAY_FINAL_STAGE &&
            stage == (uint8_t)(gate->current_stage + 1u)
        ) {
            gate->completed_mask |=
                neogeo_replay_stage_bit(gate->current_stage);
            gate->entered_mask |= neogeo_replay_stage_bit(stage);
            gate->current_stage = stage;
        } else if (stage < gate->current_stage) {
            return neogeo_replay_gate_set_result(
                gate,
                NEOGEO_REPLAY_GATE_BACKWARDS_STAGE
            );
        } else {
            return neogeo_replay_gate_set_result(
                gate,
                NEOGEO_REPLAY_GATE_SKIPPED_STAGE
            );
        }
    }

    stable_final = (uint8_t)(
        snapshot->world == 7u &&
        snapshot->level == 3u &&
        snapshot->oper_mode == NEOGEO_REPLAY_VICTORY_MODE &&
        snapshot->oper_mode_task == NEOGEO_REPLAY_VICTORY_TASK &&
        snapshot->world_end_timer == 0u
    );

    if (
        stable_final != 0u &&
        gate->current_stage == NEOGEO_REPLAY_FINAL_STAGE &&
        gate->entered_mask == NEOGEO_REPLAY_ALL_STAGES &&
        gate->completed_mask == (NEOGEO_REPLAY_ALL_STAGES >> 1)
    ) {
        if (
            gate->victory_stable_frames <
            NEOGEO_REPLAY_FINAL_STABLE_FRAMES
        ) {
            ++gate->victory_stable_frames;
        }

        if (
            gate->victory_stable_frames ==
            NEOGEO_REPLAY_FINAL_STABLE_FRAMES
        ) {
            gate->completed_mask |=
                neogeo_replay_stage_bit(NEOGEO_REPLAY_FINAL_STAGE);
            return neogeo_replay_gate_set_result(
                gate,
                NEOGEO_REPLAY_GATE_COMPLETE
            );
        }
    } else {
        gate->victory_stable_frames = 0;
    }

    return NEOGEO_REPLAY_GATE_RUNNING;
}

uint8_t neogeo_replay_gate_passed(const NeogeoReplayGate *gate) {
    return (uint8_t)(
        gate->result == (uint8_t)NEOGEO_REPLAY_GATE_COMPLETE
    );
}

uint8_t neogeo_replay_gate_failed(const NeogeoReplayGate *gate) {
    return (uint8_t)(
        gate->result > (uint8_t)NEOGEO_REPLAY_GATE_COMPLETE
    );
}
