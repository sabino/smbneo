/* Generated direct-C migration ABI. No ROM or source data. */
#ifndef SMBNEO_NATIVE_CODEGEN_H
#define SMBNEO_NATIVE_CODEGEN_H
#include <stdint.h>

typedef uint8_t (*SmbNeoNativeRead8)(void *opaque, uint16_t address);
typedef void (*SmbNeoNativeWrite8)(void *opaque, uint16_t address, uint8_t value);
struct VsNativeFastVideoHooks;

typedef struct {
    uint8_t a, x, y, status;
    uint8_t *ram;             /* mirrored 2 KiB CPU RAM */
    uint8_t *extra_ram;       /* mirrored 2 KiB VS work RAM */
    const uint8_t *prg;       /* immutable 32 KiB program data */
    void *opaque;
    SmbNeoNativeRead8 read8;
    SmbNeoNativeWrite8 write8;
    const struct VsNativeFastVideoHooks *fast_video_hooks;
    uint8_t suspended, unsupported;
    uint32_t calls;
    uint16_t last_symbol;
} SmbNeoNativeContext;

enum { SMBNEO_NATIVE_TITLE = 0, SMBNEO_NATIVE_GAME = 1 };
enum { NATIVE_C = 1, NATIVE_Z = 2, NATIVE_I = 4, NATIVE_D = 8,
       NATIVE_B = 16, NATIVE_U = 32, NATIVE_V = 64, NATIVE_N = 128 };

void smbneo_native_boot(SmbNeoNativeContext *ctx);
void smbneo_native_frame(SmbNeoNativeContext *ctx);
void smbneo_native_title_tick(SmbNeoNativeContext *ctx);
void smbneo_native_game_tick(SmbNeoNativeContext *ctx);
#endif
