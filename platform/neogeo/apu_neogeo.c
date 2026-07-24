#include "apu.h"
#include "apu_bridge.h"

#include <ngdevkit/registers.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    SOUND_RESET_COMMAND = 3,
    SOUND_READY_PING_0 = 4,
    SOUND_READY_PING_1 = 5,
    SOUND_REGISTER_COMMAND = 0x10,
    SOUND_VALUE_HIGH_COMMAND = 0x20,
    SOUND_VALUE_LOW_COMMAND = 0x30,
    SOUND_ACK_BIT = 0x80,
    SOUND_ACK_SPINS = 4096,
    SOUND_STARTUP_FRAMES = 8,
    SOUND_MAX_CONSECUTIVE_FAILURES = 3
};

static NeogeoApuBridge bridge;
static uint8_t startup_frames;
static uint8_t consecutive_failures;
static uint8_t transport_disabled;
static uint8_t ready;
static uint8_t ready_ping;

volatile uint32_t neogeo_apu_command_timeouts;

static bool send_command(uint8_t command) {
    uint16_t spin;
    uint8_t expected = (uint8_t)(command | SOUND_ACK_BIT);

    *REG_SOUND = command;
    for (spin = 0; spin < SOUND_ACK_SPINS; ++spin) {
        if (*REG_SOUND == expected) {
            return true;
        }
    }
    ++neogeo_apu_command_timeouts;
    return false;
}

static bool send_ym_register(
    void *context,
    uint8_t reg,
    uint8_t value
) {
    (void)context;

    /*
     * Every command stays below $80, leaving bit 7 exclusively available
     * for the Z80 acknowledgement. The three command classes differ, so an
     * acknowledgement from the previous byte cannot satisfy the next wait.
     */
    return
        send_command((uint8_t)(SOUND_REGISTER_COMMAND | reg)) &&
        send_command(
            (uint8_t)(SOUND_VALUE_HIGH_COMMAND | (value >> 4))
        ) &&
        send_command(
            (uint8_t)(SOUND_VALUE_LOW_COMMAND | (value & 0x0fu))
        );
}

static void recover_transport(void) {
    ++consecutive_failures;
    neogeo_apu_bridge_invalidate(&bridge);
    ready = 0;
    if (
        consecutive_failures >= SOUND_MAX_CONSECUTIVE_FAILURES
    ) {
        transport_disabled = 1;
        return;
    }

    ready_ping = ready_ping == SOUND_READY_PING_0
        ? SOUND_READY_PING_1
        : SOUND_READY_PING_0;
    *REG_SOUND = SOUND_RESET_COMMAND;
    startup_frames = SOUND_STARTUP_FRAMES;
}

void apu_init(size_t frequency) {
    (void)frequency;

    neogeo_apu_bridge_init(&bridge);
    startup_frames = SOUND_STARTUP_FRAMES;
    consecutive_failures = 0;
    transport_disabled = 0;
    ready = 0;
    ready_ping = SOUND_READY_PING_0;
    neogeo_apu_command_timeouts = 0;

    *REG_SOUND = SOUND_RESET_COMMAND;
}

void apu_write(uint16_t addr, uint8_t value) {
    neogeo_apu_bridge_write(&bridge, addr, value);
}

void apu_step_frame(void) {
    if (transport_disabled != 0u) {
        (void)neogeo_apu_bridge_step(&bridge, NULL, NULL);
        return;
    }
    if (startup_frames != 0u) {
        --startup_frames;
        (void)neogeo_apu_bridge_step(&bridge, NULL, NULL);
        return;
    }

    /*
     * Command 3 deliberately has no acknowledgement. A distinct ping after
     * every reset proves that the restarted driver is accepting FIFO input
     * and prevents an old packet acknowledgement from satisfying the first
     * selector wait.
     */
    if (ready == 0u) {
        if (!send_command(ready_ping)) {
            recover_transport();
            return;
        }
        ready = 1;
    }

    if (!neogeo_apu_bridge_step(&bridge, send_ym_register, NULL)) {
        recover_transport();
        return;
    }
    consecutive_failures = 0;
}

void apu_fill_buffer(uint8_t *buffer, size_t size) {
    size_t index;

    if (buffer == NULL) {
        return;
    }
    for (index = 0; index < size; ++index) {
        buffer[index] = 0;
    }
}
