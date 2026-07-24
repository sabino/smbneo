#include "audio_cadence.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint16_t fake_vblank;
static uint16_t advance_vblank_on_apu_call;
static unsigned int apu_calls;

uint16_t neogeo_video_current_vblank(void) {
    return fake_vblank;
}

void apu_step_frame(void) {
    ++apu_calls;
    if (apu_calls == advance_vblank_on_apu_call) {
        ++fake_vblank;
    }
}

static void reset_probe(uint16_t current_vblank) {
    fake_vblank = current_vblank;
    advance_vblank_on_apu_call = 0;
    apu_calls = 0;
    neogeo_audio_dropped_display_periods = 0;
}

int main(void) {
    uint16_t cursor;
    uint16_t game_vblank;

    assert(neogeo_audio_missed_display_periods(100u, 100u) == 0u);
    assert(neogeo_audio_missed_display_periods(100u, 101u) == 0u);
    assert(neogeo_audio_missed_display_periods(100u, 103u) == 2u);
    assert(
        neogeo_audio_missed_display_periods(0xffffu, 1u) == 1u
    );

    reset_probe(101u);
    cursor = neogeo_audio_prepare_game_frame(100u, &game_vblank);
    assert(cursor == 100u);
    assert(game_vblank == 101u);
    assert(apu_calls == 0u);

    reset_probe(103u);
    cursor = neogeo_audio_prepare_game_frame(100u, &game_vblank);
    assert(cursor == 102u);
    assert(game_vblank == 103u);
    assert(apu_calls == 2u);
    assert(neogeo_audio_dropped_display_periods == 0u);

    /*
     * Catch-up itself may cross another VBlank. Re-reading the display signal
     * drains that newly completed period while still reserving the latest one
     * for the caller's ordinary game/audio frame.
     */
    reset_probe(102u);
    advance_vblank_on_apu_call = 1u;
    cursor = neogeo_audio_prepare_game_frame(100u, &game_vblank);
    assert(cursor == 102u);
    assert(game_vblank == 103u);
    assert(apu_calls == 2u);

    reset_probe(1u);
    cursor = neogeo_audio_prepare_game_frame(
        0xfffeu,
        &game_vblank
    );
    assert(cursor == 0u);
    assert(game_vblank == 1u);
    assert(apu_calls == 2u);

    reset_probe(110u);
    cursor = neogeo_audio_prepare_game_frame(100u, &game_vblank);
    assert(cursor == 109u);
    assert(game_vblank == 110u);
    assert(apu_calls == NEOGEO_AUDIO_MAX_CATCH_UP_PERIODS);
    assert(neogeo_audio_dropped_display_periods == 5u);

    puts("Neo Geo audio cadence tests: OK");
    return 0;
}
