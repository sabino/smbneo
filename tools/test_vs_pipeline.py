import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile
import zlib

from vs_rom import Chip, CHIPS, PALETTE, load_vs_rom, verify_chip
from translate_vs import (instructions, translate, OPS, live_flags, emit_instruction,
                          semantic_fast_paths, native_routines, native_tables)


class VsInputTests(unittest.TestCase):
    def test_chip_checks_size_crc_and_sha(self):
        data = b'local synthetic test'
        good = Chip('test', len(data), f'{zlib.crc32(data):08x}', hashlib.sha1(data).hexdigest())
        verify_chip(data, good)
        for chip in (Chip('test', len(data) + 1, good.crc32, good.sha1),
                     Chip('test', len(data), '00000000', good.sha1),
                     Chip('test', len(data), good.crc32, '0' * 40)):
            with self.assertRaises(ValueError): verify_chip(data, chip)

    def test_input_order_and_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'input.zip'
            # Fixture content is synthetic, with matching synthetic identities.
            chips = tuple(Chip(c.name, c.size, f'{zlib.crc32(bytes([i])*c.size):08x}',
                               hashlib.sha1(bytes([i])*c.size).hexdigest()) for i, c in enumerate(CHIPS))
            with zipfile.ZipFile(path, 'w') as z:
                for i in reversed(range(6)): z.writestr(chips[i].name, bytes([i]) * 8192)
            with patch('vs_rom.CHIPS', chips):
                rom = load_vs_rom(path)
                self.assertEqual(rom.prg, b''.join(bytes([i]) * 8192 for i in range(4)))
                self.assertEqual(rom.chr, bytes([4]) * 8192 + bytes([5]) * 8192)
                self.assertEqual(len(rom.reference_image()), 49168)
                self.assertIsNone(rom.palette)
            with self.assertRaises(ValueError): load_vs_rom(path)
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(path, 'a') as z: z.writestr(chips[0].name, bytes(8192))
            with self.assertRaisesRegex(ValueError, 'Duplicate'): load_vs_rom(path)
            with zipfile.ZipFile(path, 'w') as z: z.writestr('Super Mario Bros.nes', b'NES\x1a')
            with self.assertRaisesRegex(ValueError, 'canonical'): load_vs_rom(path)


class TranslationTests(unittest.TestCase):
    def test_semantic_loop_requires_every_operation_and_safe_ram(self):
        code = {0x8000: ('LDA', 'n', 248), 0x8002: ('STA', 'wy', 512),
                **{0x8000 + i: ('INY', 'i', 0) for i in range(5, 9)},
                0x8009: ('BNE', 'r', 247), 0x800b: ('RTS', 'i', 0)}
        self.assertIn(0x8000, semantic_fast_paths(code, {}))
        for address in code:
            bad = dict(code); bad[address] = ('NOP', 'i', 0)
            self.assertNotIn(0x8000, semantic_fast_paths(bad, {}))
        for base in (0, 0x2000, 0x4000, 0x8000):
            bad = dict(code); bad[0x8002] = ('STA', 'wy', base)
            self.assertNotIn(0x8000, semantic_fast_paths(bad, {}))
        # A known symbol alone can never authorize an unrelated helper.
        self.assertNotIn(0x8002, semantic_fast_paths(code, {'render_chr_pair_do': 0x8002}))

    def test_flag_liveness_overwrites_branches_and_barriers(self):
        code = {0x8000: ('LDA', 'n', 7), 0x8002: ('LDX', 'n', 8),
                0x8004: ('BEQ', 'r', 2), 0x8006: ('LDA', 'n', 9),
                0x8008: ('RTS', 'i', 0)}
        live = live_flags(code)
        self.assertEqual(live[0x8000] & 0x82, 0)
        self.assertEqual(live[0x8002] & 0x82, 0x82)  # taken path reaches RTS
        for barrier in ('JSR', 'PHP', 'PLP', 'RTI', 'RTS'):
            code = {0x8000: ('LDA', 'n', 7), 0x8002: (barrier, 'i', 0)}
            self.assertEqual(live_flags(code)[0x8000], 0xff)
        # A page exit must preserve even a flag overwritten in the next page.
        code = {0x80fe: ('LDA', 'n', 7), 0x8100: ('LDA', 'n', 8)}
        self.assertEqual(live_flags(code)[0x80fe], 0xff)
        code = {0x8000: ('LDA', 'n', 7), 0x8002: ('JMP', 'w', 0x8002)}
        self.assertEqual(live_flags(code)[0x8000], 0xff)

    def test_flag_specialization_keeps_io_and_nested_expressions(self):
        output = '\n'.join(emit_instruction(0x8000, 'INC', 'w', 0x2007, 0))
        self.assertIn('vs_nz_live(c, v, 0x00); vs_wr(c, addr, v);', output)
        self.assertIn('(void)vs_nz(c, v); vs_wr(c, addr, v);', output)
        output = '\n'.join(emit_instruction(0x8000, 'SBC', 'z', 4, 1))
        self.assertIn('vs_adc_live(c, (uint8_t)~(c->bus->ram[addr]), 0x01);', output)

    def fixture(self, root, source, code, spans):
        (root / 'source.s').write_text(source)
        debug = 'file\tid=0,name="source.s"\nseg\tid=0,name="CODE",start=0x8000\n'
        for i, (start, size) in enumerate(spans):
            debug += f'span\tid={i},seg=0,start={start},size={size}\nline\tid={i},file=0,line={i + 1},span={i}\n'
        return code + bytes(32768 - len(code)), debug

    def test_static_branches_anonymous_labels_and_untyped_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prg, dbg = self.fixture(root, 'lda #0\nbne :-\nrts\n.res 1\n',
                                    bytes([0xa9, 0, 0xd0, 0xfc, 0x60, 0xea]), [(0, 2), (2, 2), (4, 1), (5, 1)])
            output, count = translate(prg, dbg, root)
            self.assertEqual(count, 3)
            self.assertNotIn(0x8005, instructions(prg, dbg, root))
            self.assertIn('goto L8000;', output)
            self.assertNotIn('switch (opcode)', output)
            self.assertEqual(translate(prg, dbg, root)[0], output)

    def test_mismatched_code_and_jump_into_data_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prg, dbg = self.fixture(root, 'lda #0\n', bytes([0xa2, 0]), [(0, 2)])
            with self.assertRaisesRegex(ValueError, 'mismatch'): translate(prg, dbg, root)
            prg, dbg = self.fixture(root, 'jmp $8003\n.res 1\n', bytes([0x4c, 3, 0x80, 0x60]), [(0, 3), (3, 1)])
            with self.assertRaisesRegex(ValueError, 'Unmapped direct'): translate(prg, dbg, root)

    def test_opcode_inventory(self):
        self.assertEqual(len(OPS), 256)
        self.assertEqual(sum(op != '-' for op in OPS), 151)

    def test_native_routines_follow_control_flow_not_address_pages(self):
        code = {0x8000: ('JSR', 'w', 0x80fe), 0x8003: ('JMP', 'w', 0x8003),
                0x80fe: ('LDA', 'n', 3), 0x8100: ('JSR', 'w', 0x8200),
                0x8103: ('JMP', 'w', 0x8200), 0x8200: ('RTS', 'i', 0)}
        routines = native_routines(code, {'actor_step': 0x80fe, 'unused_data': 0x8300})
        self.assertEqual(set(routines), {0x80fe, 0x8200})
        self.assertEqual(routines[0x80fe], ('vs_fn_actor_step_80fe', {0x80fe, 0x8100, 0x8103}))
        with patch('translate_vs.instructions', return_value=code):
            generated, _ = translate(bytes(32768), '', Path('.'))
            fallback, _ = translate(bytes(32768), '', Path('.'), native_functions=False)
        self.assertIn('vs_fn_sub_8200(c, depth + 1u);', generated)
        self.assertIn('c->pc != 0x8103', generated)
        self.assertIn('depth >= 16u', generated)
        self.assertNotIn('vs_fn_', fallback)

    def test_native_tables_require_explicit_address_data_and_known_code(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = bytearray(0x201)
            data[:9] = bytes([0x20, 0, 0x82, 0, 0x81, 1, 0x81, 2, 0x81])
            data[0x100:0x103] = bytes([0xea, 0x60, 0x60])
            data[0x200] = 0x60
            prg, debug = self.fixture(root,
                'jsr $8200\n.addr $8100\n.addr $8101\n.word $8102\nnop\nrts\nrts\nrts\n',
                bytes(data), [(0, 3), (3, 2), (5, 2), (7, 2),
                              (0x100, 1), (0x101, 1), (0x102, 1), (0x200, 1)])
            code = instructions(prg, debug, root)
            self.assertEqual(native_tables(prg, debug, root, code, 0x8200),
                             {0x8000: {0x8100, 0x8101}})
            self.assertEqual(native_tables(prg, debug, root, code, 0x8100), {})
            changed = bytearray(prg); changed[3:5] = bytes([0, 0x83])
            self.assertEqual(native_tables(changed, debug, root, code, 0x8200), {})
            with self.assertRaisesRegex(ValueError, 'address-table'):
                native_tables(prg, debug.replace('start=3,size=2', 'start=3,size=3'), root, code, 0x8200)

    @unittest.skipUnless(shutil.which('cc'), 'C compiler required')
    def test_native_calls_preserve_stack_escapes_and_bounded_resume(self):
        # All instructions here are synthetic, not extracted game content.
        code = {}
        entries = []
        def fragment(address, operations):
            from translate_vs import LENGTH
            for op, mode, value in operations:
                code[address] = op, mode, value
                address += LENGTH[mode]
        def main(address, callee):
            entries.append(address)
            fragment(address, [('JSR', 'w', callee), ('JMP', 'w', address + 3)])

        main(0x8000, 0x8100)  # nested direct calls across address pages
        fragment(0x8100, [('LDA', 'n', 1), ('JSR', 'w', 0x8200), ('INX', 'i', 0), ('RTS', 'i', 0)])
        fragment(0x8200, [('LDA', 'n', 2), ('RTS', 'i', 0)])
        entries.append(0x8400)  # shared tail also used as a direct callee
        fragment(0x8400, [('JSR', 'w', 0x8500), ('JSR', 'w', 0x8600), ('JMP', 'w', 0x8406)])
        fragment(0x8500, [('LDX', 'n', 17), ('JMP', 'w', 0x8600)])
        fragment(0x8600, [('INC', 'z', 9), ('RTS', 'i', 0)])
        main(0x8800, 0x8900)  # computed jump escapes the C chain
        fragment(0x8900, [('LDA', 'n', 0x20), ('STA', 'z', 6), ('LDA', 'n', 0x8a),
                          ('STA', 'z', 7), ('JMP', 'ind', 6)])
        fragment(0x8a20, [('INC', 'z', 8), ('RTS', 'i', 0)])
        main(0x8c00, 0x8d00)  # callee rewrites its return address
        fragment(0x8c03, [('LDA', 'n', 99), ('NOP', 'i', 0), ('JMP', 'w', 0x8c06)])
        fragment(0x8d00, [('PLA', 'i', 0), ('CLC', 'i', 0), ('ADC', 'n', 3),
                          ('PHA', 'i', 0), ('RTS', 'i', 0)])
        main(0x9000, 0x9100)  # callee discards a caller's return frame
        fragment(0x9100, [('JSR', 'w', 0x9200), ('LDA', 'n', 99), ('RTS', 'i', 0)])
        fragment(0x9200, [('PLA', 'i', 0), ('PLA', 'i', 0), ('RTS', 'i', 0)])
        entries.append(0x9400)  # deeper than the bounded C stack, still returns
        fragment(0x9400, [('LDX', 'n', 40), ('JSR', 'w', 0x9500), ('JMP', 'w', 0x9405)])
        fragment(0x9500, [('DEX', 'i', 0), ('BEQ', 'r', 3), ('JSR', 'w', 0x9500), ('RTS', 'i', 0)])
        main(0x9800, 0x9900)  # yields on an otherwise unlabeled back edge
        fragment(0x9900, [('LDX', 'n', 40), ('DEX', 'i', 0), ('BNE', 'r', 0xfd), ('RTS', 'i', 0)])
        main(0x9c00, 0x9d00)  # RTI ends execution even inside a native call
        fragment(0x9d00, [('RTI', 'i', 0)])
        main(0xa000, 0xa100)  # explicit table call, including unexpected targets
        fragment(0xa100, [('LDA', 'z', 0), ('JSR', 'w', 0xa500)])
        fragment(0xa500, [('ASL', 'a', 0), ('TAY', 'i', 0), ('PLA', 'i', 0),
                          ('STA', 'z', 4), ('PLA', 'i', 0), ('STA', 'z', 5),
                          ('INY', 'i', 0), ('LDA', 'iy', 4), ('STA', 'z', 6),
                          ('INY', 'i', 0), ('LDA', 'iy', 4), ('STA', 'z', 7),
                          ('JMP', 'ind', 6)])
        fragment(0xa600, [('LDA', 'n', 42), ('RTS', 'i', 0)])
        fragment(0xa700, [('LDA', 'n', 7), ('RTS', 'i', 0)])
        self.assertEqual(semantic_fast_paths(code, {'tbljmp': 0xa500})[0xa500],
                         ('vs_fast_table_jump(c); return;', None))
        # Computed destinations must remain explicit native dispatch entries.
        debug = ('sym\tname="computed_target",val=0x8a20,type=lab\n'
                 'sym\tname="tbljmp",val=0xa500,type=lab\n'
                 'sym\tname="unexpected_target",val=0xa700,type=lab\n')
        with patch('translate_vs.instructions', return_value=code), \
             patch('translate_vs.native_tables', return_value={0xa102: {0xa600}}):
            source, _ = translate(bytes(32768), debug, Path('.'))
        include = Path(__file__).resolve().parents[1] / 'variants' / 'vs'
        harness = r'''
#include "vs_cpu.h"
#include <assert.h>
#include <stdio.h>
void vs_program_reference(VsCpu *, unsigned);
static uint8_t prg[32768];
static const uint16_t entries[] = { ENTRIES };
int main(void) {
    unsigned checks = 0;
    prg[0x2105] = 0; prg[0x2106] = 0xa6;
    prg[0x2107] = 0; prg[0x2108] = 0xa7;
    for (unsigned e = 0; e < sizeof(entries) / sizeof(entries[0]); ++e)
    for (unsigned pattern = 0; pattern < 256; ++pattern)
    for (unsigned budget = 1; budget <= 16; budget *= 4) {
        VsBus a, b; VsCpu want, got;
        vs_bus_init(&a, prg, prg);
        for (unsigned i = 0; i < sizeof(a.ram); ++i) a.ram[i] = (uint8_t)(pattern + 13 * i);
        vs_cpu_init(&want, &a);
        want.pc = entries[e]; want.s = (uint8_t)(pattern * 37);
        want.p = (uint8_t)pattern; want.x = (uint8_t)pattern;
        b = a; got = want; got.bus = &b;
        vs_program_reference(&want, 10000);
        unsigned rounds = 0;
        while (!got.idle && !got.fault && !got.yielded && rounds++ < 10000)
            vs_program_run(&got, budget);
        if (rounds >= 10000 || want.pc != got.pc || want.s != got.s ||
            want.a != got.a || want.x != got.x || want.y != got.y || want.p != got.p ||
            want.fault != got.fault || want.idle != got.idle ||
            want.yielded != got.yielded || want.in_nmi != got.in_nmi ||
            memcmp(a.ram, b.ram, sizeof(a.ram)) ||
            memcmp(a.extra_ram, b.extra_ram, sizeof(a.extra_ram))) {
            fprintf(stderr, "call fixture %04x pattern %u budget %u rounds %u: pc %04x/%04x flags %02x/%02x\n",
                    entries[e], pattern, budget, rounds, want.pc, got.pc, want.p, got.p);
            return 1;
        }
        ++checks;
    }
    printf("Native call/escape/resume fixtures: %u cases passed\n", checks);
    return 0;
}
'''.replace('ENTRIES', ', '.join(hex(e) for e in entries))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'program.c').write_text(source)
            (root / 'test.c').write_text(harness)
            def run(args):
                result = subprocess.run(args, cwd=root, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            flags = ['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                     '-Wno-unused-label', '-I', str(include)]
            run(flags + ['-Dvs_program_run=vs_program_reference', '-c', 'program.c', '-o', 'reference.o'])
            for extra in ([], ['-DVS_PAGE_DISPATCH_ONLY'], ['-DVS_REFERENCE_KERNELS']):
                run(flags + ['-DSMB_VS'] + extra + ['-c', 'program.c', '-o', 'native.o'])
                run(flags + ['test.c', str(include / 'vs_bus.c'), 'reference.o', 'native.o', '-o', 'test'])
                run([str(root / 'test')])


if __name__ == '__main__': unittest.main()
