#include "apu.h"
#include "code.h"
#include "cpu.h"
#include "ppu.h"
#include "video.h"

int main(void) {
    neogeo_video_init();

    cpu_init();
    apu_init(0);
    ppu_init(0);
    Start();

    for (;;) {
        update_controller1(neogeo_read_controller1());
        next_frame();
        apu_step_frame();
        ppu_render();
    }

    return 0;
}
