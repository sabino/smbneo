#include "title_data.h"

/*
 * ROM-less ELF verification does not have user-owned CHR data. Cartridge
 * builds replace this object with a generated array from the supplied ROM.
 */
#if defined(SMB_NEOGEO_TITLE_TEST_DATA)
TITLE_SCREEN_DATA_ALIGNMENT
const uint8_t neogeo_title_screen_data[TITLE_SCREEN_CHR_SIZE] = {
    [0] = 0xa5u,
    [TITLE_SCREEN_CHR_SIZE - 1u] = 0x5au,
};
#else
TITLE_SCREEN_DATA_ALIGNMENT
const uint8_t neogeo_title_screen_data[TITLE_SCREEN_CHR_SIZE] = {0};
#endif
