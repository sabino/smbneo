#ifndef SMBNEO_VS_BUS_H
#define SMBNEO_VS_BUS_H
#include <stdint.h>

typedef struct {
    uint8_t ram[2048];
    uint8_t extra_ram[2048];
    const uint8_t *prg; /* immutable 32 KiB */
    const uint8_t *chr; /* immutable 16 KiB, also CPU-readable through PPU */
    uint8_t pads[2], latch[2], strobe;
    uint8_t dips, coin_service, chr_bank, ram_control;
    uint8_t counter_latch, counter_line;
    uint32_t coin_counter;
    void *context;
    uint8_t (*ppu_read)(void *, uint16_t);
    void (*ppu_write)(void *, uint16_t, uint8_t);
    /* Decoded data-port run; source bytes are ordinary read-only RAM. */
    void (*ppu_run)(void *, const uint8_t *, uint8_t, uint8_t);
    void (*apu_write)(void *, uint16_t, uint8_t);
    void (*oam_dma)(void *, uint16_t);
} VsBus;

/* No backup RAM, memory card, filesystem or BIOS writes in this core. */
void vs_bus_init(VsBus *bus, const uint8_t *prg, const uint8_t *chr);
void vs_bus_inputs(VsBus *bus, uint8_t port0, uint8_t port1,
                   uint8_t dips, uint8_t coin_service);
uint8_t vs_bus_read(VsBus *bus, uint16_t address);
void vs_bus_write(VsBus *bus, uint16_t address, uint8_t value);
uint8_t vs_bus_chr_read(const VsBus *bus, uint16_t address);
#endif
