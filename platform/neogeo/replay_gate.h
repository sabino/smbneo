#ifndef SMB_NEOGEO_REPLAY_GATE_H
#define SMB_NEOGEO_REPLAY_GATE_H

#include <stdint.h>

#define NEOGEO_REPLAY_STAGE_COUNT 32u
#define NEOGEO_REPLAY_FINAL_STAGE 31u
#define NEOGEO_REPLAY_NO_STAGE 0xffu
#define NEOGEO_REPLAY_FINAL_STABLE_FRAMES 60u
#define NEOGEO_REPLAY_ALL_STAGES UINT32_C(0xffffffff)

enum {
    NEOGEO_REPLAY_GAME_MODE = 1,
    NEOGEO_REPLAY_VICTORY_MODE = 2,
    NEOGEO_REPLAY_ACTIVE_TASK = 3,
    NEOGEO_REPLAY_VICTORY_TASK = 4
};

typedef enum NeogeoReplayGateResult {
    NEOGEO_REPLAY_GATE_RUNNING = 0,
    NEOGEO_REPLAY_GATE_COMPLETE = 1,
    NEOGEO_REPLAY_GATE_INVALID_STAGE = 2,
    NEOGEO_REPLAY_GATE_SKIPPED_STAGE = 3,
    NEOGEO_REPLAY_GATE_BACKWARDS_STAGE = 4,
    NEOGEO_REPLAY_GATE_GAME_OVER = 5,
    NEOGEO_REPLAY_GATE_RETURNED_TO_TITLE = 6
} NeogeoReplayGateResult;

/*
 * The replay driver copies these five bytes from the translated core after
 * each frame. Keeping the tracker independent of the core RAM layout makes it
 * usable by both the cartridge replay loop and a native host test.
 */
typedef struct NeogeoReplaySnapshot {
    uint8_t oper_mode;
    uint8_t oper_mode_task;
    uint8_t world;
    uint8_t level;
    uint8_t world_end_timer;
} NeogeoReplaySnapshot;

typedef struct NeogeoReplayGate {
    uint32_t entered_mask;
    uint32_t completed_mask;
    uint8_t current_stage;
    uint8_t victory_stable_frames;
    uint8_t result;
} NeogeoReplayGate;

void neogeo_replay_gate_init(NeogeoReplayGate *gate);

/*
 * Advances the gate by one emulated frame. Completion and errors are sticky,
 * so callers can stop polling immediately or keep rendering a result screen.
 */
NeogeoReplayGateResult neogeo_replay_gate_update(
    NeogeoReplayGate *gate,
    const NeogeoReplaySnapshot *snapshot
);

uint8_t neogeo_replay_gate_passed(const NeogeoReplayGate *gate);
uint8_t neogeo_replay_gate_failed(const NeogeoReplayGate *gate);

#endif
