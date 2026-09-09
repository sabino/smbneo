#ifndef SMBNEO_VS_NATIVE_FAST_ACTOR_LOOP_H
#define SMBNEO_VS_NATIVE_FAST_ACTOR_LOOP_H

#include "native/smbneo_native.h"

/*
 * CPU-free semantic replacement for the complete actor_proc owner rooted at
 * $BF60.  Besides the entry scheduler, that owner contains the course actor
 * stream at $BFEA, actor_init_check at $C150, group spawning at $C65E and the
 * active-actor table dispatch at $C7C5.
 *
 * The implementation keeps actor behavior in its independently generated or
 * native child routines.  It is an unconditional semantic owner under the
 * native runtime contract: RAM, work RAM, PRG, and bus callbacks are bound and
 * the caller has already stopped on suspension/unsupported state.  This lets
 * the compiler discard the much larger generated fallback body completely.
 *
 * Complete owner source-graph SHA-256 (288 instruction addresses):
 *   441517384e59ff6ac0ea07fccd67a75dc093adf7ce2ee773fc2a1cedcc675920
 * Delegated entry requirements:
 *   $BFA5 actor_area_loop, $CFB4 actor_killall, $C160 actor_init,
 *   $C1B5 actor_init_do, and the 14 unique table targets selected at $C7D2.
 */
void vs_native_fast_actor_proc(SmbNeoNativeContext *ctx);

#endif
