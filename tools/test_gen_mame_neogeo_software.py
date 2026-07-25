#!/usr/bin/env python3
"""Tests for the local MAME software-list generator."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "gen_mame_neogeo_software.py"
SPEC = importlib.util.spec_from_file_location("gen_mame_neogeo_software", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
mame_list = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mame_list)


class MameSoftwareListTests(unittest.TestCase):
    def write_test_roms(self, rom_dir: Path) -> None:
        for index, (_, filename, size, _, _) in enumerate(mame_list.ROM_PARTS):
            (rom_dir / filename).write_bytes(bytes([index + 1]) * size)

    def test_generates_hashed_interleaved_cartridge_regions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            rom_dir = root / "rom"
            output = root / "hash" / "neogeo.xml"
            rom_dir.mkdir()
            self.write_test_roms(rom_dir)

            mame_list.write_software_list(rom_dir, output)
            text = output.read_text()
            self.assertIn(
                '<!DOCTYPE softwarelist SYSTEM "softwarelist.dtd">',
                text,
            )

            software_list = ET.parse(output).getroot()
            software = software_list.find("./software[@name='smbneogeo']")
            self.assertIsNotNone(software)

            maincpu = software.find("./part/dataarea[@name='maincpu']")
            self.assertIsNotNone(maincpu)
            self.assertEqual(maincpu.get("width"), "16")
            self.assertEqual(maincpu.get("endianness"), "big")
            self.assertEqual(
                maincpu.find("rom").get("loadflag"),
                "load16_word_swap",
            )

            sprites = software.find("./part/dataarea[@name='sprites']")
            self.assertIsNotNone(sprites)
            self.assertIsNone(sprites.get("width"))
            self.assertIsNone(sprites.get("endianness"))
            sprite_roms = sprites.findall("rom")
            self.assertEqual(
                [rom.get("offset") for rom in sprite_roms],
                ["0x000000", "0x000001"],
            )
            self.assertTrue(
                all(rom.get("loadflag") == "load16_byte" for rom in sprite_roms)
            )

            for rom in software.findall("./part/dataarea/rom"):
                self.assertRegex(rom.get("crc", ""), r"^[0-9a-f]{8}$")
                self.assertRegex(rom.get("sha1", ""), r"^[0-9a-f]{40}$")

    def test_rejects_wrong_region_size(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            rom_dir = Path(temporary)
            self.write_test_roms(rom_dir)
            (rom_dir / "smbneogeo-s1.s1").write_bytes(b"short")

            with self.assertRaisesRegex(ValueError, "expected 131072 bytes"):
                mame_list.build_software_list(rom_dir)


if __name__ == "__main__":
    unittest.main()
