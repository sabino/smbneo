#ifndef SMBNEO_VS_NATIVE_FAST_MOTION_H
#define SMBNEO_VS_NATIVE_FAST_MOTION_H

#include "native/smbneo_native.h"

/*
 * Native fixed-point motion primitives.
 *
 * These functions preserve the migration ABI's A/X/Y/status residue and the
 * original zero-page scratch layout.  They deliberately do not update the
 * optional calls/last_symbol diagnostics; the generated named-function
 * wrapper owns that instrumentation.
 */
void vs_native_fast_motion_x(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_x_player(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_x_do(SmbNeoNativeContext *ctx);

void vs_native_fast_motion_fall_fast(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_fall_slow(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_fall_mid(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_fall(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_gravity_do(SmbNeoNativeContext *ctx);
void vs_native_fast_motion_gravity(SmbNeoNativeContext *ctx);

#endif
