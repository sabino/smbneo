#ifndef SMBNEO_VS_NATIVE_FAST_NMI_H
#define SMBNEO_VS_NATIVE_FAST_NMI_H

#include "native/smbneo_native.h"

/*
 * CPU-free semantic replacements for RAM-only NMI housekeeping.  These
 * routines preserve the observable 6502 register and status results so the
 * generated direct-C body can hand control to the documented continuation.
 * They require the normal production invariant ctx != NULL && ctx->ram !=
 * NULL; a missing RAM binding marks the context unsupported.
 *
 * Partial NMI slices, source-graph SHA-256, and continuations:
 *   $817e..$81a1 timers -> $81a3
 *     1e148a96019465b3089f26408a70ad0933c7800bb4a1eb634145a3d4838b6af1
 *   $81a3..$81be random -> $81c0
 *     de79b7d38db5f79f664910f7d57b1a70d490bc66fd75617ed5abe9c9d9fbe393
 *   $81d9..$81dc delay -> $81de
 *     2f176738248cb186bd063eab050fc6deb527f7077fe4c9216437d08ca49f455f
 *
 * Complete callable routines:
 *   $8212 nmi_oam_shuffle
 *     f94b014c05ed7c56da8bd0e38168cbbb2267af4778a0cd545e8f0cf69918866f
 *   $826e oam_init
 *     99d85b327abde02561d9a0889599a708ad8d45bcbe3c5c74d2d7e5eeaf6d281e
 *   $8273 oam_init_all
 *     2ef4d0693c67e06453784e56bd25e387859081ed0fe6798f27cbf9695758affe
 */
void vs_native_fast_nmi_timers(SmbNeoNativeContext *ctx);
void vs_native_fast_nmi_random(SmbNeoNativeContext *ctx);
void vs_native_fast_nmi_hblank_delay(SmbNeoNativeContext *ctx);
void vs_native_fast_nmi_oam_shuffle(SmbNeoNativeContext *ctx);
void vs_native_fast_oam_init(SmbNeoNativeContext *ctx);
void vs_native_fast_oam_init_all(SmbNeoNativeContext *ctx);

#endif
