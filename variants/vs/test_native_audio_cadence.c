/*
 * VS loop contract for the Neo Geo APU bridge cadence.
 *
 * Source SoundEngine work remains tied to one game-logic tick.  Only the
 * native bridge's envelopes, sweeps and length/linear counters are caught up
 * when rendering spans more than one display period.
 */
#include "audio_cadence.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint16_t fake_vblank;
static unsigned bridge_steps;
static unsigned source_sound_ticks;

uint16_t neogeo_video_current_vblank(void) {
    return fake_vblank;
}

void apu_step_frame(void) {
    ++bridge_steps;
}

static void reset_probe(uint16_t vblank) {
    fake_vblank = vblank;
    bridge_steps = 0u;
    source_sound_ticks = 0u;
    neogeo_audio_dropped_display_periods = 0u;
}

static uint16_t run_vs_game_frame(uint16_t audio_vblank) {
    uint16_t game_frame_vblank;

    audio_vblank = neogeo_audio_prepare_game_frame(
        audio_vblank,
        &game_frame_vblank
    );

    /* The real call is the VS frame routine, which runs SoundEngine once. */
    ++source_sound_ticks;
    apu_step_frame();
    (void)audio_vblank;
    return game_frame_vblank;
}

int main(void) {
    uint16_t cursor;

    /* One ordinary display/game period needs no bridge-only catch-up. */
    reset_probe(101u);
    cursor = run_vs_game_frame(100u);
    assert(cursor == 101u);
    assert(source_sound_ticks == 1u);
    assert(bridge_steps == 1u);

    /*
     * A later slow frame spans three display periods: two bridge-only steps
     * plus one normal source/bridge step.  SoundEngine still runs only once.
     */
    fake_vblank = 104u;
    cursor = run_vs_game_frame(cursor);
    assert(cursor == 104u);
    assert(source_sound_ticks == 2u);
    assert(bridge_steps == 4u);
    assert(neogeo_audio_dropped_display_periods == 0u);

    /* The uint16_t display counter may wrap without losing cadence. */
    reset_probe(1u);
    cursor = run_vs_game_frame(0xfffeu);
    assert(cursor == 1u);
    assert(source_sound_ticks == 1u);
    assert(bridge_steps == 3u);
    assert(neogeo_audio_dropped_display_periods == 0u);

    /* Recovery remains bounded and cannot turn a stall into a catch-up loop. */
    reset_probe(110u);
    cursor = run_vs_game_frame(100u);
    assert(cursor == 110u);
    assert(source_sound_ticks == 1u);
    assert(bridge_steps == NEOGEO_AUDIO_MAX_CATCH_UP_PERIODS + 1u);
    assert(neogeo_audio_dropped_display_periods == 5u);

    puts("VS native audio cadence contract: OK");
    return 0;
}
