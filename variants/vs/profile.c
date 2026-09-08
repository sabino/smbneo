/* Host-only counters over the moving portion of the integration sequence. */
#include "vs_machine.h"
#include <stdio.h>
#include "test_check.h"
extern uint32_t vs_profile[32768];
int main(int argc, char **argv) {
    static uint8_t data[49152]; static VsMachine m;
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fseek(f, 16, SEEK_SET) == 0);
    assert(fread(data, 1, sizeof(data), f) == sizeof(data)); fclose(f);
    vs_machine_init(&m, data, data + 32768);
    for (unsigned frame = 0; frame < 1200; ++frame) {
        if (frame == 600) memset(vs_profile, 0, sizeof(vs_profile));
        uint8_t input = frame >= 360 && frame < 364 ? 4 : 0;
        if (frame >= 600) input = 128 | 2 | ((frame % 60) < 25 ? 1 : 0);
        assert(vs_machine_frame(&m, input, 0, frame >= 240 && frame < 244 ? 0x20 : 0));
    }
    for (unsigned i = 0; i < 32768; ++i)
        if (vs_profile[i]) printf("%04x %u\n", i + 0x8000, vs_profile[i]);
}
