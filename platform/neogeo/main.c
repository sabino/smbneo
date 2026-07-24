#include "apu.h"
#include "audio_cadence.h"
#include "code.h"
#include "cpu.h"
#include "ppu.h"
#include "video.h"

int main(void) {
    uint16_t audio_vblank;

    neogeo_video_init();

    cpu_init();
    apu_init(0);
    ppu_init(0);
    Start();
    audio_vblank = neogeo_video_current_vblank();

    for (;;) {
        uint16_t game_frame_vblank;

        audio_vblank = neogeo_audio_prepare_game_frame(
            audio_vblank,
            &game_frame_vblank
        );
        update_controller1(neogeo_read_controller1());
        next_frame();
        apu_step_frame();
        audio_vblank = game_frame_vblank;
        ppu_render();
    }

    return 0;
}
