#include "instructions.h"

// LDA - Load Accumulator

void lda_imm(uint8_t value) {
    a = value;
    update_nz(a);
}

void lda_zp(uint8_t addr) {
    lda_imm(zero_page(addr));
}

void lda_zpx(uint8_t addr) {
    lda_imm(zero_page_x(addr));
}

void lda_zpy(uint8_t addr) {
    lda_imm(zero_page_y(addr));
}

void lda_abs(uint16_t addr) {
    lda_imm(absolute(addr));
}

void lda_absx(uint16_t addr) {
    lda_imm(absolute_x(addr));
}

void lda_absy(uint16_t addr) {
    lda_imm(absolute_y(addr));
}

void lda_indy(uint8_t addr) {
    lda_imm(indirect_y_val(addr));
}

// LDX - Load X Register

void ldx_imm(uint8_t value) {
    x = value;
    update_nz(x);
}

void ldx_zp(uint8_t addr) {
    ldx_imm(zero_page(addr));
}

void ldx_zpy(uint8_t addr) {
    ldx_imm(zero_page_y(addr));
}

void ldx_abs(uint16_t addr) {
    ldx_imm(absolute(addr));
}

void ldx_absy(uint16_t addr) {
    ldx_imm(absolute_y(addr));
}

// LDY - Load Y Register

void ldy_imm(uint8_t value) {
    y = value;
    update_nz(y);
}

void ldy_zp(uint8_t addr) {
    ldy_imm(zero_page(addr));
}

void ldy_zpx(uint8_t addr) {
    ldy_imm(zero_page_x(addr));
}

void ldy_abs(uint16_t addr) {
    ldy_imm(absolute(addr));
}

void ldy_absx(uint16_t addr) {
    ldy_imm(absolute_x(addr));
}

// ADC - Add with Carry

void adc_imm(uint8_t value) {
    uint16_t sum = (uint16_t)a + (uint16_t)value + (uint16_t)carry_flag;
    carry_flag = sum & 0x100;
    a = (uint8_t)sum;
    update_nz(a);
}

void adc_zp(uint8_t addr) {
    adc_imm(zero_page(addr));
}

void adc_zpx(uint8_t addr) {
    adc_imm(zero_page_x(addr));
}

void adc_zpy(uint8_t addr) {
    adc_imm(zero_page_y(addr));
}

void adc_abs(uint16_t addr) {
    adc_imm(absolute(addr));
}

void adc_absx(uint16_t addr) {
    adc_imm(absolute_x(addr));
}

void adc_absy(uint16_t addr) {
    adc_imm(absolute_y(addr));
}

// SBC - Subtract with Carry

void sbc_imm(uint8_t value) {
    uint16_t diff = a - value - (carry_flag ? 0 : 1);
    carry_flag = diff <= 0xff;
    a = diff & 0xff;
    update_nz(a);
}

void sbc_zp(uint8_t addr) {
    sbc_imm(zero_page(addr));
}

void sbc_zpx(uint8_t addr) {
    sbc_imm(zero_page_x(addr));
}

void sbc_abs(uint16_t addr) {
    sbc_imm(absolute(addr));
}

void sbc_absx(uint16_t addr) {
    sbc_imm(absolute_x(addr));
}

void sbc_absy(uint16_t addr) {
    sbc_imm(absolute_y(addr));
}

// TAX - Transfer Accumulator to X

void tax(void) {
    x = a;
    update_nz(x);
}

// TAY - Transfer Accumulator to Y

void tay(void) {
    y = a;
    update_nz(y);
}

// TSX - Transfer Stack Pointer to X

void tsx(void) {
    x = sp;
    update_nz(x);
}

// TXA - Transfer X to Accumulator

void txa(void) {
    a = x;
    update_nz(a);
}

// TXS - Transfer X to Stack Pointer

void txs(void) {
    sp = x;
}

// TYA - Transfer Y to Accumulator

void tya(void) {
    a = y;
    update_nz(a);
}

// AND - Logical AND

void and_imm(uint8_t value) {
    a &= value;
    update_nz(a);
}

void and_zp(uint8_t addr) {
    and_imm(zero_page(addr));
}

void and_abs(uint16_t addr) {
    and_imm(absolute(addr));
}

void and_absx(uint16_t addr) {
    and_imm(absolute_x(addr));
}

void and_absy(uint16_t addr) {
    and_imm(absolute_y(addr));
}

// ORA - Logical Inclusive OR

void ora_imm(uint8_t value) {
    a |= value;
    update_nz(a);
}

void ora_zp(uint8_t addr) {
    ora_imm(zero_page(addr));
}

void ora_zpx(uint8_t addr) {
    ora_imm(zero_page_x(addr));
}

void ora_zpy(uint8_t addr) {
    ora_imm(zero_page_y(addr));
}

void ora_abs(uint16_t addr) {
    ora_imm(absolute(addr));
}

void ora_absx(uint16_t addr) {
    ora_imm(absolute_x(addr));
}

void ora_absy(uint16_t addr) {
    ora_imm(absolute_y(addr));
}

// EOR - Exclusive OR

void eor_imm(uint8_t value) {
    a ^= value;
    update_nz(a);
}

void eor_zp(uint8_t addr) {
    eor_imm(zero_page(addr));
}

// ASL - Arithmetic Shift Left

void asl_acc(void) {
    carry_flag = a & 0x80;
    a <<= 1;
    update_nz(a);
}

void asl_abs(uint16_t addr) {
    uint8_t val = read_byte(addr);
    carry_flag = val & 0x80;
    val <<= 1;
    dynamic_ram_write(addr, val);
    update_nz(val);
}

// LSR - Logical Shift Right

void lsr_acc(void) {
    carry_flag = a & 1;
    a >>= 1;
    update_nz(a);
}

static SMB_INSTRUCTION_INLINE void _lsr(uint16_t addr) {
    uint8_t val = read_byte(addr);
    carry_flag = val & 1;
    val >>= 1;
    dynamic_ram_write(addr, val);
    update_nz(val);
}

void lsr_zp(uint8_t addr) {
    _lsr(addr);
}

void lsr_abs(uint16_t addr) {
    _lsr(addr);
}

// INC - Increment Memory

static SMB_INSTRUCTION_INLINE void _inc(uint16_t addr) {
    uint8_t val = read_byte(addr);
    val++;
    dynamic_ram_write(addr, val);
    update_nz(val);
}

void inc_zp(uint8_t addr) {
    _inc(addr);
}

void inc_zpx(uint8_t addr) {
    _inc(addr + x);
}

void inc_abs(uint16_t addr) {
    _inc(addr);
}

void inc_absx(uint16_t addr) {
    _inc(addr + x);
}

// INX - Increment X Register

void inx(void) {
    x++;
    update_nz(x);
}

// INY - Increment Y Register

void iny(void) {
    y++;
    update_nz(y);
}

// DEC - Decrement Memory

static SMB_INSTRUCTION_INLINE void _dec(uint16_t addr) {
    uint8_t val = read_byte(addr);
    val--;
    dynamic_ram_write(addr, val);
    update_nz(val);
}

void dec_zp(uint8_t addr) {
    _dec(addr);
}

void dec_zpx(uint8_t addr) {
    _dec(addr + x);
}

void dec_abs(uint16_t addr) {
    _dec(addr);
}

void dec_absx(uint16_t addr) {
    _dec(addr + x);
}

// DEX - Decrement X Register

void dex(void) {
    x--;
    update_nz(x);
}

// DEY - Decrement Y Register

void dey(void) {
    y--;
    update_nz(y);
}

// CLC - Clear Carry Flag

void clc(void) {
    carry_flag = false;
}

// SEC - Set Carry Flag

void sec(void) {
    carry_flag = true;
}

// CLD - Clear Decimal Flag

void cld(void) {}

// SED - Set Decimal Flag

void sed(void) {}

// SEI - Set Interrupt Disable

void sei(void) {}

static SMB_INSTRUCTION_INLINE void cmp_vals(uint8_t lhs, uint8_t rhs) {
    carry_flag = lhs >= rhs;
    update_nz(lhs - rhs);
}

// CMP - Compare

void cmp_imm(uint8_t value) {
    cmp_vals(a, value);
}

void cmp_zp(uint8_t addr) {
    cmp_vals(a, zero_page(addr));
}

void cmp_zpx(uint8_t addr) {
    cmp_vals(a, zero_page_x(addr));
}

void cmp_zpy(uint8_t addr) {
    cmp_vals(a, zero_page_y(addr));
}

void cmp_abs(uint16_t addr) {
    cmp_vals(a, absolute(addr));
}

void cmp_absx(uint16_t addr) {
    cmp_vals(a, absolute_x(addr));
}

void cmp_absy(uint16_t addr) {
    cmp_vals(a, absolute_y(addr));
}

// CPX - Compare X Register

void cpx_imm(uint8_t value) {
    cmp_vals(x, value);
}

void cpx_zp(uint8_t addr) {
    cmp_vals(x, zero_page(addr));
}

// CPY - Compare Y Register

void cpy_imm(uint8_t value) {
    cmp_vals(y, value);
}

void cpy_zp(uint8_t addr) {
    cmp_vals(y, zero_page(addr));
}

void cpy_abs(uint16_t addr) {
    cmp_vals(y, absolute(addr));
}

// PHA - Push Accumulator

void pha(void) {
    dynamic_ram_write(sp | 0x100, a);
    sp--;
}

// PLA - Pull Accumulator

void pla(void) {
    sp++;
    a = read_byte(sp | 0x100);
    update_nz(a);
}

// BIT - Bit Test

static SMB_INSTRUCTION_INLINE void _bit(uint8_t val) {
    /*
     * Both BIT sites in the statically translated program consume only Z;
     * the next flag-producing instruction overwrites N before any N branch.
     */
    nz_value = (uint8_t)(a & val);
}

void bit_zp(uint8_t addr) {
    _bit(zero_page(addr));
}

void bit_abs(uint16_t addr) {
    _bit(absolute(addr));
}

// ROL - Rotate Left

static SMB_INSTRUCTION_INLINE void _rol(uint16_t addr) {
    uint8_t val = read_byte(addr);
    bool next_carry_flag = val & 0x80;
    val <<= 1;
    val |= carry_flag;
    carry_flag = next_carry_flag;
    dynamic_ram_write(addr, val);
    update_nz(val);
}

void rol_acc(void) {
    bool next_carry_flag = a & 0x80;
    a <<= 1;
    a |= carry_flag;
    carry_flag = next_carry_flag;
    update_nz(a);
}

void rol_zp(uint8_t addr) {
    _rol(addr);
}

void rol_abs(uint16_t addr) {
    _rol(addr);
}

// ROR - Rotate Right

static SMB_INSTRUCTION_INLINE void _ror(uint16_t addr) {
    uint8_t val = read_byte(addr);
    bool old_carry = carry_flag;
    carry_flag = val & 1;
    val >>= 1;

    if (old_carry) {
        val |= 0x80;
    }

    dynamic_ram_write(addr, val);
    update_nz(val);
}

inline void ror_acc(void) {
    bool old_carry = carry_flag;
    carry_flag = a & 1;
    a >>= 1;

    if (old_carry) {
        a |= 0x80;
    }

    update_nz(a);
}

inline void ror_absx(uint16_t addr) {
    _ror(absolute_x_addr(addr));
}
