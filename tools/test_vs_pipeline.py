import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile
import zlib

from vs_rom import Chip, CHIPS, PALETTE, load_vs_rom, verify_chip
from translate_vs import instructions, translate, OPS, live_flags, emit_instruction, semantic_fast_paths


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


if __name__ == '__main__': unittest.main()
