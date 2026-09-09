#ifndef SMBNEO_VS_NATIVE_FAST_ACTOR_POLICY_H
#define SMBNEO_VS_NATIVE_FAST_ACTOR_POLICY_H

#include <stdbool.h>

#include "native/smbneo_native.h"

/*
 * Native actor-state policy replacements.
 *
 * A false result is transactional: neither the migration registers nor RAM
 * have changed, so the generated direct-C routine remains a safe fallback.
 * A true result has consumed the complete named routine and the generated
 * wrapper must return normally.
 */
bool vs_native_fast_actor_proc_base(SmbNeoNativeContext *ctx);
bool vs_native_fast_actor_oob_proc(SmbNeoNativeContext *ctx);

#endif
