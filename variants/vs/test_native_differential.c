/*
 * Differential test for the CPU-free direct-C VS backend.
 *
 * This is an owned-ROM test: the NES fixture and generated direct-C source
 * are supplied by the caller and are intentionally not part of the tree.
 * The reference object is linked under the renamed vs_program_reference
 * symbol by the build command below, so both implementations see the exact
 * same bus and PPU/APU model.
 *
 * Build from variants/vs (after generating a direct-C output directory):
 *   cc -std=c99 -Wall -Wextra -Werror -pedantic -I. -Igenerated \
 *      test_native_differential.c vs_machine.c vs_bus.c \
 *      vs_native_platform.c generated/smbneo_native.c \
 *      build/vs_program.reference.o -o /tmp/test_native_differential
 *
 * Run:
 *   /tmp/test_native_differential build/assets/vs-reference.nes
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
    FRAMES = 1200
};

static const uint8_t CREDITS_AFTER_THREE_COINS[8] = {
    3, 9, 1, 15, 1, 12, 6, 32
};

static const uint8_t CREDITS_AFTER_ONE_PLAYER_START[8] = {
    2, 8, 0, 14, 0, 11, 5, 32
};

/* The legacy generated object is renamed with objcopy so this wrapper keeps
 * vs_machine.c source-compatible while proving it is the reference object. */
extern void vs_program_reference(VsCpu *, unsigned);
void vs_program_run(VsCpu *cpu, unsigned budget) {
    vs_program_reference(cpu, budget);
}

/* These generated entry points are intentionally used only by this owned
 * reference gate; they are not part of the production native ABI. */
extern void smbneo_native_fn_vs_read_dips_9274(SmbNeoNativeContext *);
extern void smbneo_native_fn_course_descriptor_load_ab60(SmbNeoNativeContext *);
extern void smbneo_native_fn_game_init_0_do_965c(SmbNeoNativeContext *);

static uint8_t native_read8(void *opaque, uint16_t address) {
    VsNativePlatform *platform = opaque;
    return vs_bus_read(&platform->bus, address);
}

static void native_write8(void *opaque, uint16_t address, uint8_t value) {
    VsNativePlatform *platform = opaque;
    vs_bus_write(&platform->bus, address, value);
}

static int load_nes(const char *path, uint8_t *prg, uint8_t *chr) {
    uint8_t header[NES_HEADER_SIZE];
    size_t trainer = 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NES\x1a", 4) != 0 || header[4] != 2u ||
        header[5] != 2u) {
        fclose(file);
        return 0;
    }
    if ((header[6] & 4u) != 0u) trainer = 512u;
    if ((header[6] & 8u) != 0u || (header[7] & 0xf0u) != 0u ||
        fseek(file, (long)trainer, SEEK_CUR) != 0 ||
        fread(prg, 1, NES_PRG_SIZE, file) != NES_PRG_SIZE ||
        fread(chr, 1, NES_CHR_SIZE, file) != NES_CHR_SIZE) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static uint64_t hash_bytes(uint64_t hash, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t state_hash(const VsMachine *reference,
                           const VsNativePlatform *native) {
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = hash_bytes(hash, reference->bus.ram, sizeof(reference->bus.ram));
    hash = hash_bytes(hash, native->bus.ram, sizeof(native->bus.ram));
    hash = hash_bytes(hash, reference->bus.extra_ram,
                      sizeof(reference->bus.extra_ram));
    hash = hash_bytes(hash, native->bus.extra_ram,
                      sizeof(native->bus.extra_ram));
    hash = hash_bytes(hash, reference->nametable, sizeof(reference->nametable));
    hash = hash_bytes(hash, native->nametable, sizeof(native->nametable));
    hash = hash_bytes(hash, reference->oam, sizeof(reference->oam));
    hash = hash_bytes(hash, native->oam, sizeof(native->oam));
    hash = hash_bytes(hash, reference->palette, sizeof(reference->palette));
    hash = hash_bytes(hash, native->palette, sizeof(native->palette));
    hash = hash_bytes(hash, reference->apu, sizeof(reference->apu));
    hash = hash_bytes(hash, native->apu, sizeof(native->apu));
    return hash;
}

static int bytes_equal(const char *name, unsigned frame,
                       const uint8_t *want, const uint8_t *got, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        /* The legacy CPU object leaves return-frame bytes in the 6502 stack
         * page; direct C intentionally has no emulated JSR/RTS frames. */
        if (strcmp(name, "ram") == 0 && i >= 0x100u && i < 0x200u)
            continue;
        if (want[i] != got[i]) {
            fprintf(stderr, "divergence frame=%u area=%s offset=0x%zx "
                    "reference=%02x native=%02x\n", frame, name, i,
                    want[i], got[i]);
            return 0;
        }
    }
    return 1;
}

#define CHECK_BYTES(label, field) \
    do { if (!bytes_equal((label), frame, (reference)->field, \
                          (native_platform)->field, sizeof((reference)->field))) \
             return 0; } while (0)

static int compare_state(const VsMachine *reference,
                         const VsNativePlatform *native_platform,
                         const SmbNeoNativeContext *native_context,
                         unsigned frame) {
    /* Compare all mutable memory and every PPU/APU register exposed by the
     * host model.  Execution-only CPU PC/fuel/call counters are deliberately
     * excluded; no game-visible state is normalized or ignored. */
    CHECK_BYTES("ram", bus.ram);
    CHECK_BYTES("extra_ram", bus.extra_ram);
    CHECK_BYTES("nametable", nametable);
    CHECK_BYTES("oam", oam);
    CHECK_BYTES("palette", palette);
    CHECK_BYTES("apu", apu);
#undef CHECK_BYTES

#define CHECK_SCALAR(label, lhs, rhs) \
    do { if ((lhs) != (rhs)) { \
        fprintf(stderr, "divergence frame=%u field=%s reference=%" PRIu32 \
                " native=%" PRIu32 "\n", frame, (label), \
                (uint32_t)(lhs), (uint32_t)(rhs)); return 0; \
    } } while (0)
    CHECK_SCALAR("ppu.address", reference->address, native_platform->address);
    CHECK_SCALAR("ppu.address_latch", reference->address_latch,
                 native_platform->address_latch);
    CHECK_SCALAR("ppu.ctrl", reference->ctrl, native_platform->ctrl);
    CHECK_SCALAR("ppu.mask", reference->mask, native_platform->mask);
    CHECK_SCALAR("ppu.toggle", reference->toggle, native_platform->toggle);
    CHECK_SCALAR("ppu.buffer", reference->buffer, native_platform->buffer);
    CHECK_SCALAR("ppu.oam_address", reference->oam_address,
                 native_platform->oam_address);
    CHECK_SCALAR("ppu.status_phase", reference->status_phase,
                 native_platform->status_phase);
    CHECK_SCALAR("ppu.scroll_x", reference->scroll_x, native_platform->scroll_x);
    CHECK_SCALAR("ppu.scroll_y", reference->scroll_y, native_platform->scroll_y);
    CHECK_SCALAR("apu_writes", reference->apu_writes,
                 native_platform->apu_writes);
    CHECK_SCALAR("bus.pads[0]", reference->bus.pads[0], native_platform->bus.pads[0]);
    CHECK_SCALAR("bus.pads[1]", reference->bus.pads[1], native_platform->bus.pads[1]);
    CHECK_SCALAR("bus.latch[0]", reference->bus.latch[0], native_platform->bus.latch[0]);
    CHECK_SCALAR("bus.latch[1]", reference->bus.latch[1], native_platform->bus.latch[1]);
    CHECK_SCALAR("bus.strobe", reference->bus.strobe, native_platform->bus.strobe);
    CHECK_SCALAR("bus.dips", reference->bus.dips, native_platform->bus.dips);
    CHECK_SCALAR("bus.coin_service", reference->bus.coin_service,
                 native_platform->bus.coin_service);
    CHECK_SCALAR("bus.chr_bank", reference->bus.chr_bank,
                 native_platform->bus.chr_bank);
    CHECK_SCALAR("bus.ram_control", reference->bus.ram_control,
                 native_platform->bus.ram_control);
    CHECK_SCALAR("bus.counter_latch", reference->bus.counter_latch,
                 native_platform->bus.counter_latch);
    CHECK_SCALAR("bus.counter_line", reference->bus.counter_line,
                 native_platform->bus.counter_line);
    CHECK_SCALAR("bus.coin_counter", reference->bus.coin_counter,
                 native_platform->bus.coin_counter);
    CHECK_SCALAR("cpu.a", reference->cpu.a, native_context->a);
    CHECK_SCALAR("cpu.x", reference->cpu.x, native_context->x);
    CHECK_SCALAR("cpu.y", reference->cpu.y, native_context->y);
    CHECK_SCALAR("cpu.status", reference->cpu.p, native_context->status);
#undef CHECK_SCALAR
    return 1;
}

static int invoke_reference(VsMachine *machine, uint16_t entry) {
    machine->cpu.pc = entry;
    machine->cpu.idle = 0;
    machine->cpu.fault = 0;
    vs_push(&machine->cpu, 0xff);
    vs_push(&machine->cpu, 0xfe);
    vs_program_run(&machine->cpu, 200000u);
    if (!machine->cpu.fault || machine->cpu.pc != 0xffffu) return 0;
    machine->cpu.fault = 0;
    machine->cpu.pc = 0x808bu;
    machine->cpu.idle = 1;
    return 1;
}

static void rebind_reference(VsMachine *machine) {
    machine->cpu.bus = &machine->bus;
    machine->bus.context = machine;
}

static void rebind_native(VsNativePlatform *platform,
                          SmbNeoNativeContext *native) {
    platform->bus.context = platform;
    native->opaque = platform;
    native->ram = platform->bus.ram;
    native->extra_ram = platform->bus.extra_ram;
    native->prg = platform->bus.prg;
    native->suspended = 0;
}

static int run_dip_routine(const uint8_t *prg, const uint8_t *chr) {
    for (unsigned dip = 0; dip < 256u; ++dip) {
        VsMachine reference;
        VsNativePlatform native_platform;
        SmbNeoNativeContext native;
        memset(&reference, 0, sizeof(reference));
        vs_bus_init(&reference.bus, prg, chr);
        reference.bus.dips = (uint8_t)dip;
        reference.bus.context = &reference;
        vs_cpu_init(&reference.cpu, &reference.bus);
        reference.cpu.pc = 0x9274u;

        vs_native_platform_init(&native_platform, prg, chr, (uint8_t)dip);
        memset(&native, 0, sizeof(native));
        native.opaque = &native_platform;
        native.read8 = native_read8;
        native.write8 = native_write8;
        native.ram = native_platform.bus.ram;
        native.extra_ram = native_platform.bus.extra_ram;
        native.prg = native_platform.bus.prg;
        native.status = NATIVE_U | NATIVE_I;

        if (!invoke_reference(&reference, 0x9274u)) {
            fprintf(stderr, "reference DIP routine failed dip=%02x\n", dip);
            return 0;
        }
        smbneo_native_fn_vs_read_dips_9274(&native);
        if (native.unsupported != 0u) {
            fprintf(stderr, "native DIP routine failed dip=%02x last=%04x\n",
                    dip, native.last_symbol);
            return 0;
        }
        if (!compare_state(&reference, &native_platform, &native,
                           20000u + dip)) {
            fprintf(stderr, "DIP routine divergence dip=%02x\n", dip);
            return 0;
        }
    }
    return 1;
}

static int run_course_snapshots(
    const uint8_t *prg, const uint8_t *chr,
    const VsMachine *reference_started,
    const VsNativePlatform *native_platform_started,
    const SmbNeoNativeContext *native_started) {
    (void)chr;
    static const uint8_t sub_with_intro[4] = {0, 2, 3, 4};
    for (unsigned world = 0; world < 8u; ++world) {
        for (unsigned level = 0; level < 4u; ++level) {
            VsMachine reference = *reference_started;
            VsNativePlatform native_platform = *native_platform_started;
            SmbNeoNativeContext native = *native_started;
            unsigned sub = (world == 0u || world == 1u || world == 3u ||
                            world == 6u) ? sub_with_intro[level] : level;
            rebind_reference(&reference);
            rebind_native(&native_platform, &native);
            reference.bus.ram[0x75fu] = (uint8_t)world;
            reference.bus.ram[0x760u] = (uint8_t)sub;
            reference.bus.ram[0x75cu] = (uint8_t)level;
            native_platform.bus.ram[0x75fu] = (uint8_t)world;
            native_platform.bus.ram[0x760u] = (uint8_t)sub;
            native_platform.bus.ram[0x75cu] = (uint8_t)level;
            reference.bus.ram[0x772u] = 1u;
            native_platform.bus.ram[0x772u] = 1u;

            if (!invoke_reference(&reference, 0xab60u)) {
                fprintf(stderr, "reference descriptor call failed stage=%u-%u\n",
                        world + 1u, level + 1u);
                return 0;
            }
            smbneo_native_fn_course_descriptor_load_ab60(&native);
            if (native.unsupported != 0u ||
                !compare_state(&reference, &native_platform, &native,
                               30000u + world * 100u + level * 3u)) {
                fprintf(stderr, "descriptor divergence stage=%u-%u\n",
                        world + 1u, level + 1u);
                return 0;
            }

            if (!invoke_reference(&reference, 0x965cu)) {
                fprintf(stderr, "reference game-init call failed stage=%u-%u\n",
                        world + 1u, level + 1u);
                return 0;
            }
            smbneo_native_fn_game_init_0_do_965c(&native);
            if (native.unsupported != 0u ||
                !compare_state(&reference, &native_platform, &native,
                               30001u + world * 100u + level * 3u)) {
                fprintf(stderr, "game-init divergence stage=%u-%u\n",
                        world + 1u, level + 1u);
                return 0;
            }
            unsigned descriptor = prg[0xac7fu - 0x8000u +
                prg[0xac77u - 0x8000u + world] + sub];
            if (reference.bus.ram[0x750u] != descriptor) {
                fprintf(stderr, "descriptor checkpoint failed stage=%u-%u "
                        "got=%02x expected=%02x\n", world + 1u, level + 1u,
                        reference.bus.ram[0x750u], descriptor);
                return 0;
            }
            for (unsigned frame = 0; frame < 600u; ++frame) {
                uint8_t pad = frame < 180u ? 0u : (uint8_t)(0x80u | 0x02u |
                    ((frame % 60u) < 25u ? 0x01u : 0u));
                if (!vs_machine_frame(&reference, pad, 0u, 0u)) {
                    fprintf(stderr, "reference stage frame failed stage=%u-%u "
                            "frame=%u\n", world + 1u, level + 1u, frame);
                    return 0;
                }
                vs_native_platform_inputs(&native_platform, pad, 0u, 0u);
                native.suspended = 0;
                smbneo_native_frame(&native);
                if (native.unsupported != 0u ||
                    !compare_state(&reference, &native_platform, &native,
                                   30100u + (world * 4u + level) * 600u + frame)) {
                    fprintf(stderr, "stage divergence stage=%u-%u frame=%u\n",
                            world + 1u, level + 1u, frame);
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int run_differential(const uint8_t *prg, const uint8_t *chr,
                            uint8_t dips, unsigned *frames_done) {
    VsMachine reference;
    VsNativePlatform native_platform;
    SmbNeoNativeContext native;
    VsMachine reference_started;
    VsNativePlatform native_platform_started;
    SmbNeoNativeContext native_started;

    vs_machine_init_dips(&reference, prg, chr, dips);
    vs_native_platform_init(&native_platform, prg, chr, dips);
    memset(&native, 0, sizeof(native));
    native.opaque = &native_platform;
    native.read8 = native_read8;
    native.write8 = native_write8;
    native.ram = native_platform.bus.ram;
    native.extra_ram = native_platform.bus.extra_ram;
    native.prg = native_platform.bus.prg;
    smbneo_native_boot(&native);
    if (native.unsupported != 0u || native.calls == 0u) {
        fprintf(stderr, "native boot failed dips=%02x unsupported=%u "
                "calls=%" PRIu32 "\n", dips, native.unsupported,
                native.calls);
        return 0;
    }
    if (!compare_state(&reference, &native_platform, &native, 0u)) return 0;

    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        uint8_t p1 = 0, p2 = 0, coin = 0;
        /* Three isolated four-frame coin pulses exercise every coinage
         * setting before the start pulse.  The final 600 frames keep both
         * implementations in deterministic player-select/gameplay flow. */
        if ((frame >= 240u && frame < 244u) ||
            (frame >= 280u && frame < 284u) ||
            (frame >= 320u && frame < 324u)) coin = 0x20u;
        if (frame >= 400u && frame < 404u) p1 = 0x04u;
        if (frame >= 600u) p1 = (uint8_t)(0x80u | 0x02u |
            ((frame % 60u) < 25u ? 0x01u : 0u));

        int reference_ok = vs_machine_frame(&reference, p1, p2, coin);
        vs_native_platform_inputs(&native_platform, p1, p2, coin);
        native.suspended = 0;
        smbneo_native_frame(&native);
        if (!reference_ok || native.unsupported != 0u) {
            fprintf(stderr, "execution failure frame=%u dips=%02x "
                    "reference_ok=%d unsupported=%u last=%04x\n",
                    frame + 1u, dips, reference_ok, native.unsupported,
                    native.last_symbol);
            return 0;
        }
        if (frame == 360u &&
            reference.bus.extra_ram[0x610u] != CREDITS_AFTER_THREE_COINS[dips & 7u]) {
            fprintf(stderr, "reference coinage checkpoint failed frame=%u "
                    "dips=%02x credits=%02x expected=%02x\n", frame + 1u,
                    dips, reference.bus.extra_ram[0x610u],
                    CREDITS_AFTER_THREE_COINS[dips & 7u]);
            return 0;
        }
        if (frame == 600u &&
            (reference.bus.extra_ram[0x610u] !=
                 CREDITS_AFTER_ONE_PLAYER_START[dips & 7u] ||
             reference.bus.ram[0x770u] != 2u)) {
            fprintf(stderr, "reference start checkpoint failed frame=%u "
                    "dips=%02x credits=%02x expected=%02x mode=%02x\n",
                    frame + 1u,
                    dips, reference.bus.extra_ram[0x610u],
                    CREDITS_AFTER_ONE_PLAYER_START[dips & 7u],
                    reference.bus.ram[0x770u]);
            return 0;
        }
        if (!compare_state(&reference, &native_platform, &native, frame + 1u)) {
            fprintf(stderr, "dips=%02x state_hash=%016" PRIx64 "\n", dips,
                    state_hash(&reference, &native_platform));
            return 0;
        }
        if (frame == 599u) {
            reference_started = reference;
            native_platform_started = native_platform;
            native_started = native;
        }
        *frames_done = frame + 1u;
    }
    if (dips == 0x10u &&
        (!run_course_snapshots(prg, chr, &reference_started,
                               &native_platform_started, &native_started) ||
         !run_dip_routine(prg, chr)))
        return 0;
    return 1;
}

int main(int argc, char **argv) {
    uint8_t prg[NES_PRG_SIZE], chr[NES_CHR_SIZE];
    unsigned total = 0;
    if (argc != 2 || !load_nes(argv[1], prg, chr)) {
        fprintf(stderr, "usage: %s VS_REFERENCE.NES\n", argv[0]);
        return 2;
    }
    for (unsigned setting = 0; setting < 8u; ++setting) {
        unsigned frames = 0;
        if (!run_differential(prg, chr, (uint8_t)(0x10u | setting), &frames))
            return 1;
        total += frames;
        printf("differential: dips=%02x frames=%u passed\n",
               0x10u | setting, frames);
    }
    printf("native/reference differential passed: %u frames across 8 coinage modes\n",
           total);
    return 0;
}
