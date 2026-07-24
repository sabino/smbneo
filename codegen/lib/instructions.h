#ifndef SMB_INSTRUCTIONS_H
#define SMB_INSTRUCTIONS_H

#include "cpu.h"

#if defined(SMB_NEOGEO_FAST_CORE)
#define SMB_INSTRUCTION_INLINE __attribute__((always_inline)) inline
#else
#define SMB_INSTRUCTION_INLINE
#endif

SMB_INSTRUCTION_INLINE void lda_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void lda_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void lda_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void lda_zpy(uint8_t addr);
SMB_INSTRUCTION_INLINE void lda_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void lda_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void lda_absy(uint16_t addr);
SMB_INSTRUCTION_INLINE void lda_indy(uint8_t addr);

SMB_INSTRUCTION_INLINE void ldx_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void ldx_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void ldx_zpy(uint8_t addr);
SMB_INSTRUCTION_INLINE void ldx_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void ldx_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void ldy_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void ldy_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void ldy_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void ldy_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void ldy_absx(uint16_t addr);

SMB_INSTRUCTION_INLINE void adc_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void adc_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void adc_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void adc_zpy(uint8_t addr);
SMB_INSTRUCTION_INLINE void adc_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void adc_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void adc_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void sbc_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void sbc_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void sbc_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void sbc_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void sbc_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void sbc_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void tax(void);
SMB_INSTRUCTION_INLINE void tay(void);
SMB_INSTRUCTION_INLINE void tsx(void);
SMB_INSTRUCTION_INLINE void txa(void);
SMB_INSTRUCTION_INLINE void txs(void);
SMB_INSTRUCTION_INLINE void tya(void);

SMB_INSTRUCTION_INLINE void and_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void and_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void and_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void and_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void and_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void ora_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void ora_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void ora_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void ora_zpy(uint8_t addr);
SMB_INSTRUCTION_INLINE void ora_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void ora_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void ora_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void eor_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void eor_zp(uint8_t addr);

SMB_INSTRUCTION_INLINE void asl_acc(void);
SMB_INSTRUCTION_INLINE void asl_abs(uint16_t addr);

SMB_INSTRUCTION_INLINE void lsr_acc(void);
SMB_INSTRUCTION_INLINE void lsr_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void lsr_abs(uint16_t addr);

SMB_INSTRUCTION_INLINE void inc_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void inc_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void inc_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void inc_absx(uint16_t addr);

SMB_INSTRUCTION_INLINE void inx(void);
SMB_INSTRUCTION_INLINE void iny(void);

SMB_INSTRUCTION_INLINE void dec_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void dec_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void dec_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void dec_absx(uint16_t addr);

SMB_INSTRUCTION_INLINE void dex(void);
SMB_INSTRUCTION_INLINE void dey(void);

SMB_INSTRUCTION_INLINE void clc(void);
SMB_INSTRUCTION_INLINE void cld(void);

SMB_INSTRUCTION_INLINE void sei(void);
SMB_INSTRUCTION_INLINE void sec(void);
SMB_INSTRUCTION_INLINE void sed(void);

SMB_INSTRUCTION_INLINE void cmp_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void cmp_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void cmp_zpx(uint8_t addr);
SMB_INSTRUCTION_INLINE void cmp_zpy(uint8_t addr);
SMB_INSTRUCTION_INLINE void cmp_abs(uint16_t addr);
SMB_INSTRUCTION_INLINE void cmp_absx(uint16_t addr);
SMB_INSTRUCTION_INLINE void cmp_absy(uint16_t addr);

SMB_INSTRUCTION_INLINE void cpx_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void cpx_zp(uint8_t addr);

SMB_INSTRUCTION_INLINE void cpy_imm(uint8_t value);
SMB_INSTRUCTION_INLINE void cpy_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void cpy_abs(uint16_t addr);

SMB_INSTRUCTION_INLINE void pha(void);
SMB_INSTRUCTION_INLINE void pla(void);

SMB_INSTRUCTION_INLINE void bit_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void bit_abs(uint16_t addr);

SMB_INSTRUCTION_INLINE void rol_acc(void);
SMB_INSTRUCTION_INLINE void rol_zp(uint8_t addr);
SMB_INSTRUCTION_INLINE void rol_abs(uint16_t addr);

SMB_INSTRUCTION_INLINE void ror_acc(void);
SMB_INSTRUCTION_INLINE void ror_absx(uint16_t addr);

#endif
