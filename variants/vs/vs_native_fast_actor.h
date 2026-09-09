#ifndef SMBNEO_VS_NATIVE_FAST_ACTOR_H
#define SMBNEO_VS_NATIVE_FAST_ACTOR_H

#include <stdbool.h>

#include "native/smbneo_native.h"

/*
 * CPU-free semantic replacements for the hottest ordinary actor rendering
 * operations.  Every function returns false before changing ctx or RAM when
 * its preconditions are not met, allowing the generated direct-C routine to
 * remain the authoritative fallback.
 *
 * Integration fingerprints (verified VS source graph):
 *   EB0F render_chr_pair, including its F1E7 static tail
 *     d54c67667edbf0ed1efb0d2e74aedbd900521e433eed972dc7c0ccdf97355566
 *   EB07 render_actor_chr_pair direct body
 *     ff9162c043c5049940231483b4d793f001dc2e8e4c87bffa9a6b3f67c1f4b244
 *   E9A8 render_actor_tiles slice
 *     fa163b1ef14fdffe172e3f9fb12ea738706390bac638bf9e3f20ea61fb9b12bd
 *   E7DA render_actor direct body through EB06
 *     affd324b3a1a646a7132e1280044abc7eca3456f1b415e90c88ef34b844f6093
 * The generator additionally fingerprints every called leaf internalized by
 * the semantic implementation before enabling the E7DA replacement.
 */
bool vs_native_fast_render_chr_pair(SmbNeoNativeContext *ctx);
bool vs_native_fast_render_actor_chr_pair(SmbNeoNativeContext *ctx);
bool vs_native_fast_render_actor_tiles(SmbNeoNativeContext *ctx);
bool vs_native_fast_render_actor(SmbNeoNativeContext *ctx);

#endif
