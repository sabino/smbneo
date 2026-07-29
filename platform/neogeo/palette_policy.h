#ifndef SMB_NEOGEO_PALETTE_POLICY_H
#define SMB_NEOGEO_PALETTE_POLICY_H

#include <stdint.h>

/*
 * Palette RAM word zero is the LSPC's analog black reference, not an
 * ordinary transparent color entry.  Keep it at the hardware-defined value
 * even when the NES universal background color changes.  The backdrop
 * register is independent and must retain the requested visible color.
 */
#define NEO_PALETTE_REFERENCE_WORD_INDEX UINT16_C(0)
#define NEO_PALETTE_REFERENCE_BLACK UINT16_C(0x8000)

static inline uint16_t neogeo_palette_ram_value(
    uint16_t word_index,
    uint16_t requested_value
) {
    if (word_index == NEO_PALETTE_REFERENCE_WORD_INDEX) {
        return NEO_PALETTE_REFERENCE_BLACK;
    }
    return requested_value;
}

static inline uint16_t neogeo_backdrop_value(uint16_t requested_value) {
    return requested_value;
}

#endif
