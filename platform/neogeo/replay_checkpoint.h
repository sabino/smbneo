#ifndef SMB_NEOGEO_REPLAY_CHECKPOINT_H
#define SMB_NEOGEO_REPLAY_CHECKPOINT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NeogeoReplayCheckpoint {
    uint32_t observed_entered_mask;
    uint8_t stage_pending;
    uint8_t settle_frames_remaining;
} NeogeoReplayCheckpoint;

static inline void neogeo_replay_checkpoint_init(
    NeogeoReplayCheckpoint *checkpoint,
    uint32_t entered_mask
) {
    checkpoint->observed_entered_mask = entered_mask;
    checkpoint->stage_pending = 0;
    checkpoint->settle_frames_remaining = 0;
}

static inline bool neogeo_replay_checkpoint_stage_ready(
    NeogeoReplayCheckpoint *checkpoint,
    uint32_t entered_mask,
    uint8_t settle_frames
) {
    if (entered_mask != checkpoint->observed_entered_mask) {
        checkpoint->observed_entered_mask = entered_mask;
        checkpoint->stage_pending = 1;
        checkpoint->settle_frames_remaining = settle_frames;
        if (settle_frames == 0u) {
            checkpoint->stage_pending = 0;
            return true;
        }
        return false;
    }

    if (checkpoint->stage_pending == 0u) {
        return false;
    }
    if (checkpoint->settle_frames_remaining != 0u) {
        --checkpoint->settle_frames_remaining;
    }
    if (checkpoint->settle_frames_remaining != 0u) {
        return false;
    }

    checkpoint->stage_pending = 0;
    return true;
}

#endif
