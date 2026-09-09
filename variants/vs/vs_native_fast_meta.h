#ifndef SMBNEO_VS_NATIVE_FAST_META_H
#define SMBNEO_VS_NATIVE_FAST_META_H

#include <stdbool.h>

#include "native/smbneo_native.h"

/* Build one ordinary 13-metatile scenery column as a native C operation.
 * False is a transactional fallback for uncommon aliased display lists. */
bool vs_native_fast_meta_render_area(SmbNeoNativeContext *ctx);

#endif
