#include "vs_cpu.h"
#include <assert.h>
#include <stdio.h>
#include "test_check.h"

int main(void) {
    VsBus b; VsCpu c;
    vs_bus_init(&b, NULL, NULL); vs_cpu_init(&c, &b);
    for (unsigned a = 0; a < 256; ++a) for (unsigned v = 0; v < 256; ++v) {
        for (unsigned carry = 0; carry < 2; ++carry) {
            c.a = (uint8_t)a; c.p = VS_U | VS_D | VS_I | carry;
            vs_adc(&c, (uint8_t)v);
            unsigned sum = a + v + carry;
            int signed_sum = (a < 128 ? (int)a : (int)a - 256)
                           + (v < 128 ? (int)v : (int)v - 256) + (int)carry;
            assert(c.a == (uint8_t)sum);
            assert(!!(c.p & VS_C) == (sum > 255));
            assert(!!(c.p & VS_V) == (signed_sum < -128 || signed_sum > 127));
            assert(!!(c.p & VS_Z) == ((uint8_t)sum == 0));
            assert(!!(c.p & VS_N) == !!(sum & 128));
            assert((c.p & (VS_U | VS_D | VS_I)) == (VS_U | VS_D | VS_I));
            uint8_t expected = c.p;
            for (unsigned mask = 0; mask < 16; ++mask) {
                unsigned live = (mask & 3) | ((mask & 12) << 4);
                c.a = (uint8_t)a; c.p = VS_U | VS_D | VS_I | carry;
                uint8_t old = c.p;
                vs_adc_live(&c, (uint8_t)v, live);
                assert(c.a == (uint8_t)sum);
                assert((c.p & live) == (expected & live));
                assert((c.p & ~live) == (old & ~live));
            }
        }
        vs_cmp(&c, (uint8_t)a, (uint8_t)v);
        assert(!!(c.p & VS_C) == (a >= v));
        assert(!!(c.p & VS_Z) == (a == v));
        assert(!!(c.p & VS_N) == !!((a - v) & 128));
        uint8_t expected = c.p;
        for (unsigned mask = 0; mask < 8; ++mask) {
            unsigned live = (mask & 3) | ((mask & 4) << 5);
            uint8_t old = c.p = 0xff;
            vs_cmp_live(&c, (uint8_t)a, (uint8_t)v, live);
            assert((c.p & live) == (expected & live));
            assert((c.p & ~live) == (old & ~live));
        }
    }
    c.s = 0;
    vs_push(&c, 0xaa); assert(c.s == 255 && b.ram[0x100] == 0xaa);
    assert(vs_pop(&c) == 0xaa && c.s == 0);
    b.ram[255] = 0x34; b.ram[0] = 0x12; b.ram[256] = 0xee;
    assert(vs_zpword(&c, 255) == 0x1234);
    puts("VS C state: exhaustive binary arithmetic/flags and wrapped stack/pointers passed");
}
