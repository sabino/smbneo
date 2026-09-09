/*
 * Local integration harness for the generated direct-C migration.
 *
 * This deliberately takes the generated include directory and C file as
 * command-line inputs.  No ROM bytes, generated data, or reference objects
 * are part of the repository.  The native side is linked only with the
 * CPU-free VS platform and bus; VsCpu is not a dependency of this test.
 *
 * Usage:
 *   test_native_program PATH/TO/vs-reference.nes [frames]
 *
 * Build (from variants/vs):
 *   cc -std=c99 -Wall -Wextra -Werror -pedantic \
 *      -Inative_codegen/generated -I. \
 *      test_native_program.c native_codegen/generated/smbneo_native.c \
 *      vs_native_platform.c vs_bus.c -o /tmp/test_native_program
 */
#include "smbneo_native.h"
#include "vs_native_platform.h"
#ifdef SMBNEO_NATIVE_EXPECT_CHECKPOINTS
#include <assert.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NES_HEADER_SIZE = 16,
    NES_PRG_SIZE = 0x8000,
    NES_CHR_SIZE = 0x4000,
    DEFAULT_FRAMES = 1200
};

static uint8_t native_read8(void *opaque, uint16_t address) {
    VsNativePlatform *platform = opaque;
    return vs_bus_read(vs_native_platform_bus(platform), address);
}

static void native_write8(void *opaque, uint16_t address, uint8_t value) {
    VsNativePlatform *platform = opaque;
    vs_bus_write(vs_native_platform_bus(platform), address, value);
}

static int load_nes_fixture(const char *path, uint8_t *prg, uint8_t *chr) {
    uint8_t header[NES_HEADER_SIZE];
    FILE *file = fopen(path, "rb");
    size_t trainer = 0;
    if (!file) return 0;
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NES\x1a", 4) != 0 || header[4] != 2u ||
        header[5] != 2u) {
        fclose(file);
        return 0;
    }
    /* iNES trainers precede PRG.  The owned VS fixture carries a historical
     * non-zero mapper nibble even though this direct 32 KiB address map uses
     * the payload unchanged; reject only layout-changing four-screen/NES2
     * variants here. */
    if ((header[6] & 4u) != 0u) trainer = 512u;
    if ((header[6] & 8u) != 0u || (header[7] & 0xf0u) != 0u) {
        fclose(file);
        return 0;
    }
    if (fseek(file, (long)trainer, SEEK_CUR) != 0 ||
        fread(prg, 1, NES_PRG_SIZE, file) != NES_PRG_SIZE ||
        fread(chr, 1, NES_CHR_SIZE, file) != NES_CHR_SIZE) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static uint64_t hash_bytes(uint64_t hash, const uint8_t *bytes, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t observable_hash(const VsNativePlatform *platform) {
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = hash_bytes(hash, platform->bus.ram, sizeof(platform->bus.ram));
    hash = hash_bytes(hash, platform->bus.extra_ram,
                      sizeof(platform->bus.extra_ram));
    hash = hash_bytes(hash, platform->nametable, sizeof(platform->nametable));
    hash = hash_bytes(hash, platform->oam, sizeof(platform->oam));
    hash = hash_bytes(hash, platform->palette, sizeof(platform->palette));
    hash = hash_bytes(hash, platform->apu, sizeof(platform->apu));
    return hash;
}

static int step(VsNativePlatform *platform, SmbNeoNativeContext *native,
                uint8_t p1, uint8_t p2, uint8_t coin_service) {
    vs_native_platform_inputs(platform, p1, p2, coin_service);
    /* A generated BRK/self-loop marks a migration suspension. Clear that
     * frame-local marker before the next host-owned semantic frame. */
    native->suspended = 0;
    smbneo_native_frame(native);
    return native->unsupported == 0u;
}

int main(int argc, char **argv) {
    uint8_t prg[NES_PRG_SIZE], chr[NES_CHR_SIZE];
    VsNativePlatform platform;
    SmbNeoNativeContext native;
    unsigned frames = DEFAULT_FRAMES;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s VS_REFERENCE.NES [frames]\n", argv[0]);
        return 2;
    }
    if (argc == 3) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[2], &end, 10);
        if (*argv[2] == '\0' || *end != '\0' || parsed > UINT32_MAX) {
            fprintf(stderr, "invalid frame count: %s\n", argv[2]);
            return 2;
        }
        frames = (unsigned)parsed;
    }
    if (!load_nes_fixture(argv[1], prg, chr)) {
        fprintf(stderr, "unsupported or unreadable VS .nes fixture: %s\n", argv[1]);
        return 2;
    }

    vs_native_platform_init(&platform, prg, chr, 0x10);
    memset(&native, 0, sizeof(native));
    native.opaque = &platform;
    native.read8 = native_read8;
    native.write8 = native_write8;
    native.ram = platform.bus.ram;
    native.extra_ram = platform.bus.extra_ram;
    native.prg = platform.bus.prg;
    smbneo_native_boot(&native);
    if (native.unsupported || native.calls == 0u) {
        fprintf(stderr, "native boot failed: unsupported=%u calls=%u\n",
                native.unsupported, native.calls);
        return 1;
    }

    for (unsigned frame = 0; frame < frames; ++frame) {
        uint8_t p1 = 0, p2 = 0, coin = 0;
        /* The VS bus consumes the same normalized, latched bytes as the
         * verified test_program.c path: A=01, B/run=02, Start=04,
         * right=80.  Coin 1 is VS $4016 bit 5. */
        if (frame >= 240u && frame < 244u) coin = 0x20u;
        if (frame >= 360u && frame < 364u) p1 = 0x04u;
        if (frame >= 600u) p1 = (uint8_t)(0x80u | 0x02u |
            ((frame % 60u) < 25u ? 0x01u : 0u));
        if (!step(&platform, &native, p1, p2, coin)) {
            fprintf(stderr, "native frame failed: frame=%u unsupported=%u calls=%u last=%04x\n",
                    frame, native.unsupported, native.calls,
                    native.last_symbol);
            return 1;
        }
        if (frame == 300u || frame == 600u || frame + 1u == frames)
            printf("native frame=%u observable_hash=%016llx credits=%02x mode=%02x\n",
                   frame + 1u,
                   (unsigned long long)observable_hash(&platform),
                   platform.bus.extra_ram[0x610], platform.bus.ram[0x770]);
#ifdef SMBNEO_NATIVE_EXPECT_CHECKPOINTS
        if (frame == 300u) assert(platform.bus.extra_ram[0x610] == 1u);
        if (frame == 600u) {
            assert(platform.bus.extra_ram[0x610] == 0u);
            assert(platform.bus.ram[0x770] == 2u);
        }
#endif
    }
    printf("native program passed: frames=%u calls=%u observable_hash=%016llx mode=%02x\n",
           frames, native.calls,
           (unsigned long long)observable_hash(&platform), platform.bus.ram[0x770]);
    return 0;
}
