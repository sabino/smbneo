#ifndef SMB_NEOGEO_AUDIO_PACKET_H
#define SMB_NEOGEO_AUDIO_PACKET_H

#include <stdint.h>

enum {
    /*
     * Commands $00-$05 remain outside the packet alphabet: $01-$03 are
     * BIOS-reserved and $04/$05 are alternating transport-ready pings.
     */
    NEOGEO_AUDIO_PACKET_COMMAND_OFFSET = 6,
    NEOGEO_AUDIO_PACKET_SYMBOL_COUNT = 122,
    /*
     * A 121-value payload digit leaves one symbol free at each byte position.
     * Rotating both symbols relative to the preceding symbol then guarantees
     * adjacent command bytes differ, so a stale echoed acknowledgement can
     * never satisfy the next send.
     */
    NEOGEO_AUDIO_PACKET_PAYLOAD_RADIX = 121,
    NEOGEO_AUDIO_PACKET_MAX_REGISTER = 0x1b
};

static inline uint8_t neogeo_audio_packet_rotate(
    uint8_t previous_symbol,
    uint8_t delta
) {
    uint16_t rotated = (uint16_t)previous_symbol + delta;

    if (rotated >= NEOGEO_AUDIO_PACKET_SYMBOL_COUNT) {
        rotated -= NEOGEO_AUDIO_PACKET_SYMBOL_COUNT;
    }
    return (uint8_t)rotated;
}

/*
 * Return two commands packed first-byte high, second-byte low. The caller
 * commits *previous_symbol only after both commands have been acknowledged.
 */
static inline uint16_t neogeo_audio_packet_encode(
    uint8_t reg,
    uint8_t value,
    uint8_t *previous_symbol
) {
    uint16_t payload = (uint16_t)(((uint16_t)reg << 8) | value);
    uint16_t quotient = (uint16_t)(
        payload / NEOGEO_AUDIO_PACKET_PAYLOAD_RADIX
    );
    uint8_t remainder = (uint8_t)(
        payload -
        quotient * NEOGEO_AUDIO_PACKET_PAYLOAD_RADIX
    );
    uint8_t first_symbol = neogeo_audio_packet_rotate(
        *previous_symbol,
        (uint8_t)(quotient + 1u)
    );
    uint8_t second_symbol = neogeo_audio_packet_rotate(
        first_symbol,
        (uint8_t)(remainder + 1u)
    );

    *previous_symbol = second_symbol;
    return (uint16_t)(
        (
            (uint16_t)(
                first_symbol + NEOGEO_AUDIO_PACKET_COMMAND_OFFSET
            ) << 8
        ) |
        (uint16_t)(
            second_symbol + NEOGEO_AUDIO_PACKET_COMMAND_OFFSET
        )
    );
}

#endif
