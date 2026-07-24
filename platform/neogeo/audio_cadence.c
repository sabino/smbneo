#include "audio_cadence.h"

#include "apu.h"
#include "video.h"

volatile uint32_t neogeo_audio_dropped_display_periods;

uint16_t neogeo_audio_prepare_game_frame(
    uint16_t last_audio_vblank,
    uint16_t *game_frame_vblank
) {
    uint8_t catch_up_count = 0;

    for (;;) {
        uint16_t current_vblank = neogeo_video_current_vblank();
        uint16_t missed = neogeo_audio_missed_display_periods(
            last_audio_vblank,
            current_vblank
        );

        if (missed == 0u) {
            *game_frame_vblank = current_vblank;
            return last_audio_vblank;
        }
        if (
            catch_up_count >= NEOGEO_AUDIO_MAX_CATCH_UP_PERIODS
        ) {
            /*
             * Preserve the slot for the upcoming normal game/audio frame, but
             * discard older bridge debt rather than risk a recovery spiral.
             */
            neogeo_audio_dropped_display_periods += missed;
            last_audio_vblank = (uint16_t)(current_vblank - 1u);
            continue;
        }

        /*
         * This advances only the native bridge's sweep, envelope, length and
         * linear-counter state. Calling the translated SoundEngine here would
         * consume queues and alter gameplay-coupled event-music state.
         */
        apu_step_frame();
        ++last_audio_vblank;
        ++catch_up_count;
    }
}
