#include "audio_packet.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t command_symbol(uint8_t command) {
    assert(command >= NEOGEO_AUDIO_PACKET_COMMAND_OFFSET);
    return (uint8_t)(
        command - NEOGEO_AUDIO_PACKET_COMMAND_OFFSET
    );
}

static uint8_t symbol_delta(uint8_t previous, uint8_t current) {
    uint8_t delta = current >= previous
        ? (uint8_t)(current - previous)
        : (uint8_t)(
            current + NEOGEO_AUDIO_PACKET_SYMBOL_COUNT - previous
        );

    assert(delta != 0u);
    return (uint8_t)(delta - 1u);
}

int main(void) {
    uint16_t previous_seed;

    for (
        previous_seed = 0;
        previous_seed < NEOGEO_AUDIO_PACKET_SYMBOL_COUNT;
        ++previous_seed
    ) {
        uint16_t reg;

        for (reg = 0; reg <= NEOGEO_AUDIO_PACKET_MAX_REGISTER; ++reg) {
            uint16_t value;

            for (value = 0; value <= 0xffu; ++value) {
                uint8_t previous = (uint8_t)previous_seed;
                uint16_t packet = neogeo_audio_packet_encode(
                    (uint8_t)reg,
                    (uint8_t)value,
                    &previous
                );
                uint8_t first_command = (uint8_t)(packet >> 8);
                uint8_t second_command = (uint8_t)packet;
                uint8_t first_symbol = command_symbol(first_command);
                uint8_t second_symbol = command_symbol(second_command);
                uint16_t decoded = (uint16_t)(
                    (uint16_t)symbol_delta(
                        (uint8_t)previous_seed,
                        first_symbol
                    ) * NEOGEO_AUDIO_PACKET_PAYLOAD_RADIX +
                    symbol_delta(first_symbol, second_symbol)
                );

                assert(first_command >= 6u);
                assert(first_command <= 0x7fu);
                assert(second_command >= 6u);
                assert(second_command <= 0x7fu);
                assert(first_command != second_command);
                assert(
                    first_symbol != (uint8_t)previous_seed
                );
                assert(previous == second_symbol);
                assert(decoded == (uint16_t)((reg << 8) | value));
            }
        }
    }

    /*
     * A running stream also keeps packet boundaries distinct as the previous
     * symbol follows each acknowledged second byte.
     */
    {
        uint8_t previous = 0;
        uint8_t previous_command = 5u;
        uint16_t reg;

        for (reg = 0; reg <= NEOGEO_AUDIO_PACKET_MAX_REGISTER; ++reg) {
            uint16_t value;

            for (value = 0; value <= 0xffu; ++value) {
                uint16_t packet = neogeo_audio_packet_encode(
                    (uint8_t)reg,
                    (uint8_t)value,
                    &previous
                );
                uint8_t first_command = (uint8_t)(packet >> 8);
                uint8_t second_command = (uint8_t)packet;

                assert(first_command != previous_command);
                assert(second_command != first_command);
                previous_command = second_command;
            }
        }
    }

    puts("Neo Geo audio packet tests: OK");
    return 0;
}
