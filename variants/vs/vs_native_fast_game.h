#ifndef SMBNEO_VS_NATIVE_FAST_GAME_H
#define SMBNEO_VS_NATIVE_FAST_GAME_H

#include "native/smbneo_native.h"

/*
 * CPU-free coordinator for the complete game_proc body at $AD6E..$AE1A.
 * Gameplay children remain independent generated/native C routines.  The
 * coordinator preserves their source call order, every early return, and the
 * A/X/Y/status/RAM state visible at each call boundary.
 *
 * Source-graph SHA-256:
 *   f783e2dcbab8c15349e66708e5cf7b752b39d9e2ca68071f571b470578683e39
 * Delegated entry requirements, already encoded by that owner graph:
 *   $AEF7 $B4D1 $BF60 $86A4 $F0E5 $F08F $EE46 $BDE3 $BD7F
 *   $BA8A $B8B0 $B66C $B709 $B5FC $8BE2 $977E $B135 $B147
 *   and the static tail at $AE1C.
 */
void vs_native_fast_game_proc(SmbNeoNativeContext *ctx);

#endif
