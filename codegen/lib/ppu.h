#ifndef SMB_PPU_H
#define SMB_PPU_H

#include <stdint.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

#define CHR_ROM_SIZE 8192
#define NAMETABLE_SIZE 2048
#define PALETTE_SIZE 32
#define OAM_SIZE 256

extern uint8_t chr_rom[CHR_ROM_SIZE]; // 1 page of CHR ROM (8KB)
extern uint8_t nametable[NAMETABLE_SIZE]; // 2KB of nametable RAM
extern uint8_t oam[OAM_SIZE]; // 256 bytes of OAM RAM

// 32 bytes of palette RAM
typedef union {
    uint8_t u8[PALETTE_SIZE];
    uint32_t u32[4];
} Palette;

extern Palette palette;

extern uint16_t ppu_v; // current VRAM address
extern uint8_t ppu_w; // write toggle (1 bit)
extern uint8_t ppu_f; // even/odd frame flag (1 bit)

// PPU registers
extern uint8_t ppu_ctrl; // Control register: $2000
extern uint8_t ppu_mask; // Mask register: $2001
extern uint8_t ppu_status; // Status register: $2002
extern uint8_t oam_addr; // OAM address: $2003
extern uint8_t ppu_scroll_x;
extern uint8_t ppu_scroll_y;

extern uint16_t vram_addr;
extern uint8_t vram_internal_buffer; // VRAM read/write buffer
extern uint8_t oam_dma; // OAM DMA: $4014

void ppu_transfer_oam(uint16_t start_addr);

// screen
extern uint8_t frame[SCREEN_WIDTH * SCREEN_HEIGHT * 3]; // 3 bytes per pixel (RGB)

void ppu_init(uint8_t* chr);

uint8_t ppu_read_register(uint16_t addr);
void ppu_write_scroll(uint8_t value);
void ppu_write_address(uint8_t value);
void ppu_write_data(uint8_t value);

/*
 * Attempt one decoded SMB VRAM-buffer run without the translated per-byte
 * CPU/PPU dispatch.  A false return guarantees that no state was changed, so
 * callers can execute the original byte loop unchanged.
 */
uint8_t ppu_write_buffer_run(
    uint16_t source_pointer,
    uint8_t length,
    uint8_t repeat
);

#if defined(SMB_NEOGEO_VRAM_BATCH_TEST)
extern uint8_t neogeo_ppu_batch_test_enabled;
extern uint32_t neogeo_ppu_batched_run_count;
extern uint32_t neogeo_ppu_rejected_run_count;
#endif

uint8_t ppu_read(uint16_t addr);
void ppu_write(uint16_t addr, uint8_t value);
void ppu_render(void);

#endif
