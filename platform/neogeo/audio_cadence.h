#ifndef SMB_NEOGEO_AUDIO_CADENCE_H
#define SMB_NEOGEO_AUDIO_CADENCE_H

#include <stdint.h>

enum {
    /*
     * Bound bridge-only catch-up so a transport fault or an unexpectedly long
     * stall cannot turn one late frame into an unbounded recovery loop.
     */
    NEOGEO_AUDIO_MAX_CATCH_UP_PERIODS = 4
};

/*
 * Return the native APU steps owed before the next ordinary game/audio frame.
 * One elapsed display period belongs to that upcoming ordinary frame. The
 * uint16_t subtraction deliberately remains correct across counter wrap.
 */
static inline uint16_t neogeo_audio_missed_display_periods(
    uint16_t last_audio_vblank,
    uint16_t current_vblank
) {
    uint16_t elapsed = (uint16_t)(
        current_vblank - last_audio_vblank
    );

    return elapsed > 1u ? (uint16_t)(elapsed - 1u) : 0u;
}

/*
 * Clock only native APU hardware state for display periods missed by the
 * renderer, leaving translated music queues/counters and gameplay RAM alone.
 * The returned cursor must be replaced with game_frame_vblank after the
 * caller's normal next_frame()+apu_step_frame() pair.
 */
uint16_t neogeo_audio_prepare_game_frame(
    uint16_t last_audio_vblank,
    uint16_t *game_frame_vblank
);

/* Diagnostic count of old APU periods discarded by the bounded catch-up. */
extern volatile uint32_t neogeo_audio_dropped_display_periods;

#endif
