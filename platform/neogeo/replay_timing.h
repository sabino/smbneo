#ifndef SMB_NEOGEO_REPLAY_TIMING_H
#define SMB_NEOGEO_REPLAY_TIMING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NeogeoReplayTiming {
    uint8_t hold_frames_remaining;
    uint8_t area_init_hold_issued;
    uint32_t area_init_hold_count;
} NeogeoReplayTiming;

static inline bool neogeo_replay_timing_should_advance(
    NeogeoReplayTiming *timing,
    uint32_t source_frame,
    uint8_t oper_mode,
    uint8_t oper_mode_task,
    uint32_t bootstrap_frames,
    uint8_t area_init_hold_frames
) {
    if (source_frame >= bootstrap_frames) {
        if (oper_mode == 1u && oper_mode_task == 0u) {
            if (timing->area_init_hold_issued == 0u) {
                timing->area_init_hold_issued = 1u;
                timing->hold_frames_remaining =
                    area_init_hold_frames;
            }
        } else {
            timing->area_init_hold_issued = 0u;
        }
    }

    if (
        source_frame < bootstrap_frames ||
        timing->hold_frames_remaining != 0u
    ) {
        if (timing->hold_frames_remaining != 0u) {
            --timing->hold_frames_remaining;
            ++timing->area_init_hold_count;
        }
        return false;
    }
    return true;
}

#endif
