#ifndef SMB_NEOGEO_TITLE_DATA_H
#define SMB_NEOGEO_TITLE_DATA_H

#include <stdint.h>

/*
 * The original program stores its title-screen nametable payload in unused
 * CHR space and reads it back through PPU_DATA during startup.
 */
#define TITLE_SCREEN_CHR_OFFSET 0x1ec0u
#define TITLE_SCREEN_CHR_SIZE 0x013au

extern const uint8_t neogeo_title_screen_data[TITLE_SCREEN_CHR_SIZE];

#endif
