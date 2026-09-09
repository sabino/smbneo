/*
 * Owned-ROM differential for the VS sound engine.
 *
 * The original ROM, generated C, and translated reference object are supplied
 * by the local build and are never embedded here.  Each source-side APU write
 * is compared in order, not merely as a final register snapshot.
 */
#include "smbneo_native.h"
#include "vs_machine.h"
#include "vs_native_platform.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NES_HEADER_SIZE = 16,
    NES_PRG_SIZE = 0x8000,
    NES_CHR_SIZE = 0x4000,
    APU_REGISTER_COUNT = 24,
    APU_WRITES_PER_TICK_MAX = 128
};

typedef struct {
    uint16_t address[APU_WRITES_PER_TICK_MAX];
    uint8_t value[APU_WRITES_PER_TICK_MAX];
    size_t count;
    uint64_t total;
    uint64_t hash;
} ApuTrace;

typedef struct {
    VsMachine reference;
    VsNativePlatform native_platform;
    SmbNeoNativeContext native;
    ApuTrace reference_trace;
    ApuTrace native_trace;
} AudioPair;

typedef struct {
    const char *name;
    uint8_t pulse_1;
    uint8_t pulse_2;
    uint8_t noise;
    uint8_t music_event;
    uint8_t music_base;
    unsigned ticks;
} AudioScenario;

extern void vs_program_reference(VsCpu *, unsigned);
void vs_program_run(VsCpu *cpu, unsigned budget) {
    vs_program_reference(cpu, budget);
}

extern void smbneo_native_fn_apu_cycle_f236(SmbNeoNativeContext *ctx);

static int load_nes(const char *path, uint8_t *prg, uint8_t *chr) {
    uint8_t header[NES_HEADER_SIZE];
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return 0;
    if (fread(header, 1u, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NES\x1a", 4u) != 0 ||
        header[4] != 2u || header[5] != 2u ||
        (header[6] & 4u) != 0u ||
        fread(prg, 1u, NES_PRG_SIZE, file) != NES_PRG_SIZE ||
        fread(chr, 1u, NES_CHR_SIZE, file) != NES_CHR_SIZE) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static void trace_reset_tick(ApuTrace *trace) {
    trace->count = 0u;
}

static void trace_write(ApuTrace *trace, uint16_t address, uint8_t value) {
    if (trace->count >= APU_WRITES_PER_TICK_MAX) {
        fprintf(stderr, "too many APU writes in one tick\n");
        abort();
    }
    trace->address[trace->count] = address;
    trace->value[trace->count] = value;
    ++trace->count;
    ++trace->total;
    trace->hash ^= ((uint64_t)address << 8) | value;
    trace->hash *= UINT64_C(1099511628211);
}

static AudioPair *active_pair;

static void reference_apu_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    VsMachine *machine = context;

    if (active_pair == NULL || machine != &active_pair->reference) {
        fprintf(stderr, "reference APU callback context mismatch\n");
        abort();
    }
    if (address >= 0x4000u && address <= 0x4017u) {
        machine->apu[address - 0x4000u] = value;
        ++machine->apu_writes;
    }
    trace_write(&active_pair->reference_trace, address, value);
}

static void native_apu_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    VsNativePlatform *platform = context;

    if (active_pair == NULL || platform != &active_pair->native_platform) {
        fprintf(stderr, "native APU callback context mismatch\n");
        abort();
    }
    if (address >= 0x4000u && address <= 0x4017u) {
        platform->apu[address - 0x4000u] = value;
        ++platform->apu_writes;
    }
    trace_write(&active_pair->native_trace, address, value);
}

static uint8_t native_read8(void *context, uint16_t address) {
    VsNativePlatform *platform = context;

    return vs_bus_read(&platform->bus, address);
}

static void native_write8(
    void *context,
    uint16_t address,
    uint8_t value
) {
    VsNativePlatform *platform = context;

    vs_bus_write(&platform->bus, address, value);
}

static int compare_trace(
    const AudioPair *pair,
    const char *scenario,
    unsigned tick
) {
    const ApuTrace *want = &pair->reference_trace;
    const ApuTrace *got = &pair->native_trace;

    if (want->count != got->count) {
        fprintf(stderr,
                "%s tick=%u APU write-count mismatch reference=%zu native=%zu\n",
                scenario, tick, want->count, got->count);
        return 0;
    }
    for (size_t index = 0u; index < want->count; ++index) {
        if (want->address[index] != got->address[index] ||
            want->value[index] != got->value[index]) {
            fprintf(stderr,
                    "%s tick=%u write=%zu mismatch reference=%04x:%02x "
                    "native=%04x:%02x\n",
                    scenario, tick, index,
                    want->address[index], want->value[index],
                    got->address[index], got->value[index]);
            return 0;
        }
    }
    return 1;
}

static int compare_audio_state(
    const AudioPair *pair,
    const char *scenario,
    unsigned tick
) {
    if (memcmp(pair->reference.apu, pair->native_platform.apu,
               APU_REGISTER_COUNT) != 0 ||
        pair->reference.apu_writes != pair->native_platform.apu_writes) {
        fprintf(stderr, "%s tick=%u APU register-state mismatch\n",
                scenario, tick);
        return 0;
    }
    for (unsigned address = 0u; address < 0x800u; ++address) {
        if (address >= 0x100u && address < 0x200u)
            continue;
        if (pair->reference.bus.ram[address] !=
            pair->native_platform.bus.ram[address]) {
            fprintf(stderr,
                    "%s tick=%u RAM mismatch address=%03x reference=%02x "
                    "native=%02x\n",
                    scenario, tick, address,
                    pair->reference.bus.ram[address],
                    pair->native_platform.bus.ram[address]);
            return 0;
        }
    }
    if (pair->reference.cpu.a != pair->native.a ||
        pair->reference.cpu.x != pair->native.x ||
        pair->reference.cpu.y != pair->native.y ||
        pair->reference.cpu.p != pair->native.status) {
        fprintf(stderr, "%s tick=%u migration-register mismatch\n",
                scenario, tick);
        return 0;
    }
    return 1;
}

static void configure_native_context(AudioPair *pair) {
    memset(&pair->native, 0, sizeof(pair->native));
    pair->native.opaque = &pair->native_platform;
    pair->native.read8 = native_read8;
    pair->native.write8 = native_write8;
    pair->native.ram = pair->native_platform.bus.ram;
    pair->native.extra_ram = pair->native_platform.bus.extra_ram;
    pair->native.prg = pair->native_platform.bus.prg;
}

static int boot_pair(
    AudioPair *pair,
    const uint8_t *prg,
    const uint8_t *chr
) {
    VsMachine callback_template;

    memset(pair, 0, sizeof(*pair));
    pair->reference_trace.hash = UINT64_C(14695981039346656037);
    pair->native_trace.hash = UINT64_C(14695981039346656037);

    /* Obtain the host PPU/OAM function pointers without exposing static
     * implementation details, then perform a fresh traced boot. */
    vs_machine_init_dips(&callback_template, prg, chr, 0x10u);
    memset(&pair->reference, 0, sizeof(pair->reference));
    vs_bus_init(&pair->reference.bus, prg, chr);
    pair->reference.bus.dips = 0x10u;
    pair->reference.bus.context = &pair->reference;
    pair->reference.bus.ppu_read = callback_template.bus.ppu_read;
    pair->reference.bus.ppu_write = callback_template.bus.ppu_write;
    pair->reference.bus.ppu_run = callback_template.bus.ppu_run;
    pair->reference.bus.oam_dma = callback_template.bus.oam_dma;
    pair->reference.bus.apu_write = reference_apu_write;
    vs_cpu_init(&pair->reference.cpu, &pair->reference.bus);

    vs_native_platform_init(&pair->native_platform, prg, chr, 0x10u);
    pair->native_platform.bus.apu_write = native_apu_write;
    configure_native_context(pair);

    active_pair = pair;
    vs_program_reference(&pair->reference.cpu, 200000u);
    smbneo_native_boot(&pair->native);
    if (pair->reference.cpu.fault != 0u ||
        pair->reference.cpu.idle == 0u || pair->native.unsupported != 0u ||
        !compare_trace(pair, "boot", 0u) ||
        !compare_audio_state(pair, "boot", 0u))
        return 0;
    return 1;
}

static int run_frame(
    AudioPair *pair,
    const char *scenario,
    unsigned frame,
    uint8_t p1,
    uint8_t p2,
    uint8_t coin
) {
    int reference_ok;

    trace_reset_tick(&pair->reference_trace);
    trace_reset_tick(&pair->native_trace);
    active_pair = pair;
    reference_ok = vs_machine_frame(&pair->reference, p1, p2, coin);
    vs_native_platform_inputs(&pair->native_platform, p1, p2, coin);
    pair->native.suspended = 0u;
    smbneo_native_frame(&pair->native);
    if (!reference_ok || pair->native.unsupported != 0u ||
        !compare_trace(pair, scenario, frame) ||
        !compare_audio_state(pair, scenario, frame))
        return 0;
    return 1;
}

static int run_frontend_script(
    const uint8_t *prg,
    const uint8_t *chr
) {
    AudioPair pair;

    if (!boot_pair(&pair, prg, chr))
        return 0;
    for (unsigned frame = 0u; frame < 1800u; ++frame) {
        uint8_t p1 = 0u;
        uint8_t coin = 0u;
        const char *phase = "title";

        if ((frame >= 240u && frame < 244u) ||
            (frame >= 280u && frame < 284u)) {
            coin = 0x20u;
            phase = "coin";
        }
        if (frame >= 400u && frame < 404u) {
            p1 = 0x04u;
            phase = "start";
        } else if (frame >= 600u) {
            p1 = (uint8_t)(0x80u | 0x02u |
                ((frame % 60u) < 30u ? 0x01u : 0u));
            phase = "gameplay-jump-run";
        }
        if (!run_frame(&pair, phase, frame, p1, 0u, coin))
            return 0;
    }
    if (pair.reference_trace.total == 0u ||
        pair.reference_trace.total != pair.native_trace.total ||
        pair.reference_trace.hash != pair.native_trace.hash) {
        fprintf(stderr, "frontend aggregate APU trace mismatch\n");
        return 0;
    }
    printf("audio differential: title/coin/start/gameplay/jump/run "
           "frames=1800 writes=%" PRIu64 " hash=%016" PRIx64 "\n",
           pair.reference_trace.total, pair.reference_trace.hash);
    return 1;
}

static int invoke_reference_apu(VsMachine *machine) {
    machine->cpu.s = 0xfdu;
    machine->cpu.pc = 0xf236u;
    machine->cpu.idle = 0u;
    machine->cpu.fault = 0u;
    vs_push(&machine->cpu, 0xffu);
    vs_push(&machine->cpu, 0xfeu);
    vs_program_reference(&machine->cpu, 200000u);
    if (machine->cpu.fault == 0u || machine->cpu.pc != 0xffffu)
        return 0;
    machine->cpu.fault = 0u;
    machine->cpu.pc = 0x808bu;
    machine->cpu.idle = 1u;
    return 1;
}

static int run_audio_scenario(
    const uint8_t *prg,
    const uint8_t *chr,
    const AudioScenario *scenario
) {
    AudioPair pair;

    if (!boot_pair(&pair, prg, chr))
        return 0;
    pair.reference.bus.ram[0x0770u] = 2u;
    pair.native_platform.bus.ram[0x0770u] = 2u;
    pair.reference.bus.ram[0xffu] = scenario->pulse_1;
    pair.native_platform.bus.ram[0xffu] = scenario->pulse_1;
    pair.reference.bus.ram[0xfeu] = scenario->pulse_2;
    pair.native_platform.bus.ram[0xfeu] = scenario->pulse_2;
    pair.reference.bus.ram[0xfdu] = scenario->noise;
    pair.native_platform.bus.ram[0xfdu] = scenario->noise;
    pair.reference.bus.ram[0xfcu] = scenario->music_event;
    pair.native_platform.bus.ram[0xfcu] = scenario->music_event;
    pair.reference.bus.ram[0xfbu] = scenario->music_base;
    pair.native_platform.bus.ram[0xfbu] = scenario->music_base;

    /* Direct calls begin with identical migration registers. */
    pair.native.a = pair.reference.cpu.a;
    pair.native.x = pair.reference.cpu.x;
    pair.native.y = pair.reference.cpu.y;
    pair.native.status = pair.reference.cpu.p;

    for (unsigned tick = 0u; tick < scenario->ticks; ++tick) {
        trace_reset_tick(&pair.reference_trace);
        trace_reset_tick(&pair.native_trace);
        active_pair = &pair;
        if (!invoke_reference_apu(&pair.reference)) {
            fprintf(stderr, "%s reference call failed tick=%u\n",
                    scenario->name, tick);
            return 0;
        }
        pair.native.suspended = 0u;
        smbneo_native_fn_apu_cycle_f236(&pair.native);
        if (pair.native.unsupported != 0u ||
            !compare_trace(&pair, scenario->name, tick) ||
            !compare_audio_state(&pair, scenario->name, tick))
            return 0;
    }
    if (pair.reference_trace.total != pair.native_trace.total ||
        pair.reference_trace.hash != pair.native_trace.hash) {
        fprintf(stderr, "%s aggregate trace mismatch\n", scenario->name);
        return 0;
    }
    printf("audio differential: %-18s ticks=%3u writes=%" PRIu64
           " hash=%016" PRIx64 "\n",
           scenario->name, scenario->ticks,
           pair.reference_trace.total, pair.reference_trace.hash);
    return 1;
}

int main(int argc, char **argv) {
    static const AudioScenario scenarios[] = {
        {"jump-big",       0x01u, 0x00u, 0x00u, 0x00u, 0x00u,  48u},
        {"jump-small",     0x80u, 0x00u, 0x00u, 0x00u, 0x00u,  48u},
        {"fireball",       0x20u, 0x00u, 0x00u, 0x00u, 0x00u,  20u},
        {"stomp",          0x04u, 0x00u, 0x00u, 0x00u, 0x00u,  20u},
        {"flag-pole",      0x40u, 0x00u, 0x00u, 0x00u, 0x00u,  72u},
        {"coin",           0x00u, 0x01u, 0x00u, 0x00u, 0x00u,  64u},
        {"block-break",    0x00u, 0x00u, 0x01u, 0x00u, 0x00u,  40u},
        {"death-music",    0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 240u},
        {"flag-level-end", 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 300u},
        {"overworld-music",0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 300u}
    };
    static uint8_t prg[NES_PRG_SIZE];
    static uint8_t chr[NES_CHR_SIZE];

    if (argc != 2 || !load_nes(argv[1], prg, chr)) {
        fprintf(stderr, "usage: %s VS_REFERENCE.NES\n", argv[0]);
        return 2;
    }
    if (!run_frontend_script(prg, chr))
        return 1;
    for (size_t index = 0u;
         index < sizeof(scenarios) / sizeof(scenarios[0]);
         ++index) {
        if (!run_audio_scenario(prg, chr, &scenarios[index]))
            return 1;
    }
    puts("VS native audio write order and source-engine state are exact");
    return 0;
}
