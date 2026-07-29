#ifndef SMB_NEOGEO_CORE_FAST_PATHS_H
#define SMB_NEOGEO_CORE_FAST_PATHS_H

#include <stdbool.h>

/*
 * Return true only when a semantic direct-C implementation completed the
 * translated routine. Returning false leaves the generated 6502-equivalent
 * body as the exact fallback.
 */
bool smb_core_fast_enemy_gfx_handler(void);

#endif
