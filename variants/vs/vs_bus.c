#include "vs_bus.h"
#include <string.h>

void vs_bus_init(VsBus *bus, const uint8_t *prg, const uint8_t *chr) {
    memset(bus, 0, sizeof(*bus));
    bus->prg = prg;
    bus->chr = chr;
    bus->dips = 0x10; /* MAME canonical defaults, not reversed software fields. */
}

void vs_bus_inputs(VsBus *bus, uint8_t port0, uint8_t port1,
                   uint8_t dips, uint8_t coin_service) {
    bus->pads[0] = port0;
    bus->pads[1] = port1;
    bus->dips = dips;
    bus->coin_service = coin_service & 0x64;
}

static void count_coin(VsBus *bus, uint8_t value) {
    if ((value & 1u) != 0 && bus->counter_line == 0)
        ++bus->coin_counter;
    bus->counter_line = value & 1u;
}

uint8_t vs_bus_read(VsBus *bus, uint16_t address) {
    if (address < 0x2000) return bus->ram[address & 0x7ff];
    if (address < 0x4000)
        return bus->ppu_read ? bus->ppu_read(bus->context, 0x2000 | (address & 7)) : 0;
    if (address == 0x4016 || address == 0x4017) {
        unsigned port = address & 1;
        uint8_t result;
        if (bus->strobe) bus->latch[port] = bus->pads[port];
        result = bus->latch[port] & 1;
        /* VS reference shifts in zero, unlike the home NES helper. */
        bus->latch[port] >>= 1;
        return port ? (result | (bus->dips & 0xfc))
                    : (result | bus->coin_service | ((bus->dips & 3) << 3));
    }
    if (address >= 0x4020 && address < 0x6000) {
        uint8_t value = bus->counter_latch;
        /* MAME's counter-read side effect uses offset bit 8. */
        count_coin(bus, (uint8_t)(((address - 0x4020) >> 8) & 1));
        return value;
    }
    if (address >= 0x6000 && address < 0x8000)
        return bus->extra_ram[address & 0x7ff];
    if (address >= 0x8000 && bus->prg) return bus->prg[address - 0x8000];
    return 0;
}

void vs_bus_write(VsBus *bus, uint16_t address, uint8_t value) {
    if (address < 0x2000) bus->ram[address & 0x7ff] = value;
    else if (address < 0x4000) {
        if (bus->ppu_write) bus->ppu_write(bus->context, 0x2000 | (address & 7), value);
    } else if (address == 0x4016) {
        if (bus->strobe && !(value & 1)) {
            bus->latch[0] = bus->pads[0];
            bus->latch[1] = bus->pads[1];
        }
        bus->strobe = value & 1;
        bus->chr_bank = (value >> 2) & 1;
        /* Record bit 1; MAME does not gate extra-RAM writes with it. */
        bus->ram_control = (value >> 1) & 1;
    } else if (address == 0x4014) {
        if (bus->oam_dma) bus->oam_dma(bus->context, (uint16_t)value << 8);
    } else if (address < 0x4020) {
        if (bus->apu_write) bus->apu_write(bus->context, address, value);
    } else if (address < 0x6000) {
        count_coin(bus, value);
        bus->counter_latch = value;
    }
    else if (address < 0x8000) bus->extra_ram[address & 0x7ff] = value;
}

uint8_t vs_bus_chr_read(const VsBus *bus, uint16_t address) {
    return bus->chr ? bus->chr[(unsigned)bus->chr_bank * 8192 + (address & 0x1fff)] : 0;
}
