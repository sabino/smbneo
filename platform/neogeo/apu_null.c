#include "apu.h"

/*
 * Phase one deliberately has no software mixer.  The generated game logic
 * only writes APU registers and never reads them, so keeping the API while
 * discarding writes removes two 4 KiB buffers and all floating-point state.
 * A later YM2610 driver can replace this file without touching the game core.
 */

void apu_init(size_t frequency) {
    (void)frequency;
}

void apu_write(uint16_t addr, uint8_t value) {
    (void)addr;
    (void)value;
}

void apu_step_frame(void) {
}

void apu_fill_buffer(uint8_t *buffer, size_t size) {
    size_t i;

    if (buffer == 0) {
        return;
    }

    for (i = 0; i < size; ++i) {
        buffer[i] = 0;
    }
}
