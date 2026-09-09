#ifndef SMBNEO_VS_NATIVE_FAST_VIDEO_H
#define SMBNEO_VS_NATIVE_FAST_VIDEO_H

#include "native/smbneo_native.h"
#include <stdbool.h>
#include <stdint.h>

/* An accepted run replaces the ordered writes to the PPU data port, not just
 * the renderer notification.  The callback must update all PPU-visible state
 * exactly as those writes would (storage, address increment and video sink).
 * A declined run must have no observable effect; the helper then issues the
 * individual writes through SmbNeoNativeContext::write8.
 *
 * source remains valid for the duration of the call and must not be changed.
 * destination is normalized to the PPU's 14-bit address space. */
typedef bool (*VsNativePpuDataRun)(void *opaque, uint16_t destination,
                                  const uint8_t *source, uint8_t count,
                                  uint16_t increment, bool repeat);

typedef struct VsNativeFastVideoHooks {
    void *opaque;
    VsNativePpuDataRun ppu_data_run;
} VsNativeFastVideoHooks;

/* Execute the ordinary, non-empty display-list record entered at the
 * ppu_displist_write_do semantic boundary.  At this boundary Y is zero and A
 * contains the record's first byte (the PPU destination high byte).
 *
 * False means that no context, RAM or I/O state was changed.  Callers must
 * continue through the generated generic body in that case.  True means one
 * complete record was consumed; callers continue at the display-list head so
 * the generic terminator and every exceptional record retain their original
 * behavior. */
bool vs_native_fast_ppu_displist_command(
    SmbNeoNativeContext *ctx, const VsNativeFastVideoHooks *hooks);

#endif
