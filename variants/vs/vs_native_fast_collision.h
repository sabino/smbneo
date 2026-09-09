#ifndef SMBNEO_VS_NATIVE_FAST_COLLISION_H
#define SMBNEO_VS_NATIVE_FAST_COLLISION_H

#include <stdbool.h>

#include "native/smbneo_native.h"

/*
 * CPU-free collision-box and off-screen-mask primitives for the native VS
 * core.  A false result is transactional: neither the context nor RAM has
 * changed, so the generated direct-C body remains the authoritative fallback.
 * A true result has consumed the complete named routine and returns with the
 * same A/X/Y/status and zero-page residue as that routine.
 *
 * Integration fingerprints (verified VS source graph):
 *   E1F2 col_box_proc
 *     e1f5a4cb13dd3a8e91f18a8ea7b43dabb77ebe02132f8b8e763ffdc244f2b938
 *   E348 col_box_buffer
 *     0a8c68a9f05633c3cbb20d2f43e4254fab8738ec42efdce557885abc255552df
 *     dependency AB14: 6107eab257aa20c4007e224692413e64499fe22d31cd02a2f081fa2882464ce8
 *   F15B pos_bits_get_do_x
 *     42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758
 *   F13C pos_bits_proc
 *     3896497d950205f45e1d297f94b0568ff79193a982b9f3388a5ef81f7afdae26
 *   F114 pos_bits_get_actor
 *     2c3faf7ad4821695f3e496c28bd307c9bf03f10fb9d6295d53fa84b9ddf59765
 * The F15B/F13C/F114 replacements additionally internalize the F1D2 helper,
 * fingerprint 2512abfcfa3e43f35588715357b4d81257c8f8158569ecc085b53aa91a7ab91a.
 */
bool vs_native_fast_col_box_proc(SmbNeoNativeContext *ctx);
bool vs_native_fast_col_box_buffer(SmbNeoNativeContext *ctx);

bool vs_native_fast_pos_bits_get_do_x(SmbNeoNativeContext *ctx);
bool vs_native_fast_pos_bits_proc(SmbNeoNativeContext *ctx);
bool vs_native_fast_pos_bits_get_actor(SmbNeoNativeContext *ctx);

#endif
