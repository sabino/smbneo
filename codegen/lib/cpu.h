#ifndef SMB_CPU_H
#define SMB_CPU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RAM_SIZE 2048 // 2KB

// registers
extern uint8_t a, x, y, sp;

// flags
extern bool carry_flag;
extern uint8_t nz_value;
#define zero_flag (nz_value == 0u)
#define neg_flag ((nz_value & 0x80u) != 0u)
// the overflow flag is never used :)

// memory
extern uint8_t ram[RAM_SIZE];

#if defined(SMB_NEOGEO_FAST_CORE)
#define SMB_CPU_INLINE __attribute__((always_inline)) inline
#else
#define SMB_CPU_INLINE
#endif

SMB_CPU_INLINE uint8_t read_byte(uint16_t addr);
SMB_CPU_INLINE void dynamic_ram_write(uint16_t addr, uint8_t value);

SMB_CPU_INLINE uint16_t read_word(uint16_t addr);
SMB_CPU_INLINE void write_word(uint16_t addr, uint16_t value);

// controllers
extern uint8_t controller1_state, controller2_state;
SMB_CPU_INLINE void update_controller1(uint8_t state);
SMB_CPU_INLINE void write_joypad1(uint8_t value);
SMB_CPU_INLINE void write_joypad2(uint8_t value);

void cpu_init(void);

// addressing mode utils

SMB_CPU_INLINE void update_nz(uint8_t value);

SMB_CPU_INLINE uint8_t zero_page(uint8_t addr);
SMB_CPU_INLINE uint8_t zero_page_x(uint8_t addr);
SMB_CPU_INLINE uint8_t zero_page_y(uint8_t addr);

SMB_CPU_INLINE uint8_t absolute(uint16_t addr);
SMB_CPU_INLINE uint16_t absolute_x_addr(uint16_t addr);
SMB_CPU_INLINE uint8_t absolute_x(uint16_t addr);
SMB_CPU_INLINE uint8_t absolute_y(uint16_t addr);

SMB_CPU_INLINE uint16_t indirect_x_addr(uint8_t addr);
SMB_CPU_INLINE uint16_t indirect_y_addr(uint8_t addr);

SMB_CPU_INLINE uint8_t indirect_x_val(uint8_t addr);
SMB_CPU_INLINE uint8_t indirect_y_val(uint8_t addr);

// engine
SMB_CPU_INLINE void next_frame(void);

#endif
