/* Owned-ROM integration tests; not required by the ROM-free CI lane. */
#include "vs_machine.h"
#include <assert.h>
#include <stdio.h>
#include "test_check.h"

static unsigned reverse(unsigned v) {
    unsigned r = 0;
    for (unsigned i = 0; i < 8; ++i) r = r * 2 + ((v >> i) & 1);
    return r;
}
static uint64_t trace_hash = UINT64_C(14695981039346656037);
static void trace_bytes(const void *data, size_t size) {
    const uint8_t *p = data;
    for (size_t i = 0; i < size; ++i) {
        trace_hash ^= p[i]; trace_hash *= UINT64_C(1099511628211);
    }
}
static int checked_frame(VsMachine *m, uint8_t p1, uint8_t p2, uint8_t coin) {
    int result = vs_machine_frame(m, p1, p2, coin);
    /* No pointers, struct padding, host endian words or diagnostic counters.
     * Both generated implementations must produce the same per-frame stream. */
    trace_bytes(m->bus.ram, sizeof(m->bus.ram));
    trace_bytes(m->bus.extra_ram, sizeof(m->bus.extra_ram));
    trace_bytes(m->nametable, sizeof(m->nametable));
    trace_bytes(m->oam, sizeof(m->oam));
    trace_bytes(m->palette, sizeof(m->palette));
    trace_bytes(m->apu, sizeof(m->apu));
    const uint8_t state[] = {m->cpu.pc >> 8, m->cpu.pc & 255,
        m->cpu.a, m->cpu.x, m->cpu.y, m->cpu.s, m->cpu.p,
        m->ctrl, m->mask, m->toggle, m->buffer, m->oam_address,
        m->scroll_x, m->scroll_y, m->bus.chr_bank,
        m->address >> 8, m->address & 255, m->address_latch >> 8, m->address_latch & 255};
    trace_bytes(state, sizeof(state));
    return result;
}
static void invoke(VsMachine *m, uint16_t entry) {
    m->cpu.pc = entry; m->cpu.idle = 0; m->cpu.fault = 0;
    vs_push(&m->cpu, 0xff); vs_push(&m->cpu, 0xfe);
    vs_program_run(&m->cpu, 200000);
    assert(m->cpu.fault && m->cpu.pc == 0xffff);
    m->cpu.fault = 0; m->cpu.pc = 0x808b; m->cpu.idle = 1;
}
int main(int argc, char **argv) {
    static uint8_t data[49152]; static VsMachine m, started;
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fseek(f, 16, SEEK_SET) == 0);
    assert(fread(data, 1, sizeof(data), f) == sizeof(data)); fclose(f);
    for (unsigned dip = 0; dip < 256; ++dip) {
        vs_bus_init(&m.bus, data, data + 32768); vs_cpu_init(&m.cpu, &m.bus);
        m.bus.dips = (uint8_t)dip; m.cpu.pc = 0x9274; /* vs_read_dips */
        vs_push(&m.cpu, 0xff); vs_push(&m.cpu, 0xfe); /* sentinel return to unmapped ffff */
        vs_program_run(&m.cpu, 1000);
        assert(m.cpu.fault && m.cpu.pc == 0xffff && m.cpu.s == 0xfd);
        unsigned r = reverse(dip);
        assert(m.bus.extra_ram[0x604] == (r & 1));
        assert(m.bus.extra_ram[0x603] == ((r >> 1) & 1));
        assert(m.bus.extra_ram[0x602] == ((r >> 2) & 3));
        assert(m.bus.extra_ram[0x605] == ((r >> 4) & 1));
        assert(m.bus.extra_ram[0x606] == ((r >> 5) & 7));
        assert(m.bus.chr_bank == 0);
    }
    vs_machine_init(&m, data, data + 32768);
    assert(m.cpu.idle && !m.cpu.fault && (m.ctrl & 128));
    for (unsigned frame = 0; frame < 1200; ++frame) {
        uint8_t input = frame >= 360 && frame < 364 ? 4 : 0;
        if (frame >= 600) input = 128 | 2 | ((frame % 60) < 25 ? 1 : 0);
        assert(checked_frame(&m, input, 0, frame >= 240 && frame < 244 ? 0x20 : 0));
        if (frame == 300) assert(m.bus.extra_ram[0x610] == 1);
        /* VS inserts player-select as mode 1, shifting gameplay to mode 2. */
        if (frame == 600) assert(m.bus.extra_ram[0x610] == 0 && m.bus.ram[0x770] == 2);
        if (frame == 599) started = m;
    }
    assert(m.apu_writes > 1000 && m.bus.coin_counter == 1);
    for (unsigned setting = 0; setting < 8; ++setting) {
        static const uint8_t credits_after_three_coins[8] = {3, 9, 1, 15, 1, 12, 6, 32};
        vs_machine_init_dips(&m, data, data + 32768, (uint8_t)(0x10 | setting));
        for (unsigned frame = 0; frame < 480; ++frame) {
            unsigned elapsed = frame >= 240 ? frame - 240 : 1000;
            uint8_t coin = elapsed < 120 && elapsed % 40 < 4 ? 0x20 : 0;
            assert(checked_frame(&m, 0, 0, coin));
        }
        assert(m.bus.extra_ram[0x610] == credits_after_three_coins[setting]);
    }
    puts("VS translated program: 256 DIP settings, boot, coin/start and 1200 gameplay frames passed");
    puts("VS arcade credits: all eight coinage/free-play settings passed");
    for (unsigned world = 0; world < 8; ++world) for (unsigned level = 0; level < 4; ++level) {
        static const uint8_t sub_with_intro[4] = {0, 2, 3, 4};
        unsigned sub = (world == 0 || world == 1 || world == 3 || world == 6) ? sub_with_intro[level] : level;
        m = started;
        /* Rebind self-pointers after restoring an independent snapshot. */
        m.cpu.bus = &m.bus; m.bus.context = &m;
        m.bus.ram[0x75f] = (uint8_t)world;
        m.bus.ram[0x760] = (uint8_t)sub;
        m.bus.ram[0x75c] = (uint8_t)level;
        m.bus.ram[0x772] = 1;
        /* Stage transition path, not game_init_0 which resets world progress. */
        invoke(&m, 0xab60); /* course_descriptor_load */
        invoke(&m, 0x965c); /* game_init_0_do, including banked course_load */
        unsigned expected_descriptor = data[0xac7f - 0x8000 + data[0xac77 - 0x8000 + world] + sub];
        if (m.bus.ram[0x750] != expected_descriptor) {
            fprintf(stderr, "Stage %u-%u descriptor %02x expected %02x; world=%u sub=%u\n",
                    world + 1, level + 1, m.bus.ram[0x750], expected_descriptor, m.bus.ram[0x75f], m.bus.ram[0x760]);
            return 1;
        }
        for (unsigned frame = 0; frame < 600; ++frame) {
            uint8_t pad = frame < 180 ? 0 : 128 | 2 | ((frame % 60) < 25 ? 1 : 0);
            if (!checked_frame(&m, pad, 0, 0)) {
                fprintf(stderr, "Stage %u-%u failed at frame %u pc=%04x\n", world + 1, level + 1, frame, m.cpu.pc);
                return 1;
            }
        }
    }
    puts("VS course coverage: all 32 stage loads and 600-frame input sequences passed (not full playthroughs)");
    printf("VS frame trace: %016llx\n", (unsigned long long)trace_hash);
}
