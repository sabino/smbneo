#include "vs_machine.h"
#include <stdio.h>
#include "test_check.h"

/* ROM-free adapter tests do not execute a game program. */
void vs_program_run(VsCpu *c, unsigned budget) { (void)c; (void)budget; }

int main(void) {
    static VsMachine m;
    static uint8_t prg[32768], chr[16384];
    vs_machine_init(&m, prg, chr);
    for (unsigned i = 0; i < 2048; ++i) {
        m.bus.ram[i] = (uint8_t)(i * 13 + (i >> 8));
        m.bus.extra_ram[i] = (uint8_t)(i * 11 + (i >> 8));
    }
    for (unsigned i = 0; i < sizeof(prg); ++i) prg[i] = (uint8_t)(i * 17 + (i >> 8));
    for (unsigned page = 0; page < 256; ++page) {
        if (page >= 0x20 && page < 0x60) continue; /* I/O has observable reads. */
        for (unsigned offset = 0; offset < 256; ++offset) {
            m.oam_address = (uint8_t)offset;
            vs_bus_write(&m.bus, 0x4014, (uint8_t)page);
            assert(m.oam_address == offset);
            for (unsigned i = 0; i < 256; ++i)
                assert(m.oam[(i + offset) & 255] == vs_bus_read(&m.bus, (uint16_t)(page * 256 + i)));
        }
    }
    puts("VS sprite DMA: every RAM mirror, extra RAM/ROM fallback and all OAM wrap offsets passed");
}
