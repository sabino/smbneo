from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

from vs_rom import VsRom
from gen_vs_assets import generate
from gen_neogeo_assets import decode_nes_tile, encode_crom_tile, expand_2x, orient_tile
from gen_mame_neogeo_software import ROM_PARTS, build_software_list


class VsAssetTests(unittest.TestCase):
    def test_both_banks_all_orientations_and_reserved_tiles(self):
        rom = VsRom(bytes(32768), bytes((i * 31 + i // 8192) & 255 for i in range(16384)),
                    bytes(i % 8 for i in range(192)))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            generate(rom, root)
            c1 = (root / 'vssmbneo-c1.c1').read_bytes()
            c2 = (root / 'vssmbneo-c2.c2').read_bytes()
            s1 = (root / 'vssmbneo-s1.s1').read_bytes()
            self.assertEqual((len(c1), len(c2), len(s1)), (0x200000, 0x200000, 0x20000))
            self.assertFalse(any(c1[:257 * 64] + c2[:257 * 64]))
            self.assertFalse(any(s1[:32] + s1[1025 * 32:1026 * 32]))
            self.assertTrue(any(s1[1026 * 32:1027 * 32]))
            for tile in (0, 255, 511, 512, 767, 1023):
                pixels = decode_nes_tile(rom.chr[tile // 512 * 8192:][:8192], tile % 512)
                for orient in range(4):
                    offset = (257 + orient * 1024 + tile) * 64
                    want = encode_crom_tile(expand_2x(orient_tile(pixels, orient)))
                    self.assertEqual((c1[offset:offset + 64], c2[offset:offset + 64]), want)

    def test_vs_mame_identity_does_not_change_home_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for _, name, size, _, _ in ROM_PARTS:
                (root / name).write_bytes(bytes(size))
                (root / name.replace('smbneo-', 'vssmbneo-')).write_bytes(bytes(size))
            home = build_software_list(root)
            vs = build_software_list(root, 'vssmbneo', 'VS. Super Mario Bros. Neo')
            self.assertEqual(home.find('software').get('name'), 'smbneo')
            self.assertEqual(vs.findtext('software/description'), 'VS. Super Mario Bros. Neo')
            names = {r.get('name') for r in vs.findall('.//rom')}
            self.assertEqual(names, {n.replace('smbneo-', 'vssmbneo-') for _, n, *_ in ROM_PARTS})
            prom = vs.find(".//dataarea[@name='maincpu']/rom")
            self.assertEqual(prom.get('loadflag'), 'load16_word_swap')
            self.assertEqual(prom.get('size'), '0x100000')
            with self.assertRaises(ValueError): build_software_list(root, '../invalid')


if __name__ == '__main__': unittest.main()
