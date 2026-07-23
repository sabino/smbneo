#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_neogeo_assets as assets  # noqa: E402


def encode_nes_tile(pixels: bytes) -> bytes:
    output = bytearray(16)
    for y in range(8):
        for x in range(8):
            color = pixels[y * 8 + x]
            bit = 7 - x
            output[y] |= (color & 1) << bit
            output[y + 8] |= ((color >> 1) & 1) << bit
    return bytes(output)


def decode_crom_tile(crom1: bytes, crom2: bytes) -> bytes:
    tile = bytearray(256)
    position = 0
    for quadrant_offset in (8, 136, 0, 128):
        offset = quadrant_offset
        for _y in range(8):
            planes = (
                crom1[position],
                crom1[position + 1],
                crom2[position],
                crom2[position + 1],
            )
            position += 2
            for x in range(8):
                tile[offset] = sum(
                    ((planes[plane] >> x) & 1) << plane
                    for plane in range(4)
                )
                offset += 1
            offset += 8
    return bytes(tile)


def decode_srom_tile(srom: bytes) -> bytes:
    tile = bytearray(64)
    position = 0
    for pixel_a, pixel_b in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            packed = srom[position]
            position += 1
            tile[y * 8 + pixel_a] = packed & 0x0F
            tile[y * 8 + pixel_b] = packed >> 4
    return bytes(tile)


class AssetConversionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tile = bytes((x + y * 3) & 3 for y in range(8) for x in range(8))

    def test_nes_chr_decode(self) -> None:
        chr_data = encode_nes_tile(self.tile) + bytes(
            assets.NES_CHR_SIZE - 16
        )
        self.assertEqual(assets.decode_nes_tile(chr_data, 0), self.tile)

    def test_crom_round_trip_after_two_x_expansion(self) -> None:
        expanded = assets.expand_2x(self.tile)
        crom1, crom2 = assets.encode_crom_tile(expanded)
        self.assertEqual(len(crom1), 64)
        self.assertEqual(len(crom2), 64)
        self.assertEqual(decode_crom_tile(crom1, crom2), expanded)

    def test_srom_round_trip(self) -> None:
        encoded = assets.encode_srom_tile(self.tile)
        self.assertEqual(len(encoded), 32)
        self.assertEqual(decode_srom_tile(encoded), self.tile)

    def test_complete_asset_layout_and_helper_tiles(self) -> None:
        chr_data = encode_nes_tile(self.tile) + bytes(
            assets.NES_CHR_SIZE - 16
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            info = assets.build_assets(chr_data, output)
            crom1 = (output / "smbneogeo-c1.c1").read_bytes()
            crom2 = (output / "smbneogeo-c2.c2").read_bytes()
            srom = (output / "smbneogeo-s1.s1").read_bytes()

            self.assertEqual(len(crom1), assets.CROM_CHIP_SIZE)
            self.assertEqual(len(crom2), assets.CROM_CHIP_SIZE)
            self.assertEqual(len(srom), assets.SROM_SIZE)
            self.assertEqual(
                crom1[: assets.CROM_NES_TILE_BASE * 64],
                bytes(assets.CROM_NES_TILE_BASE * 64),
            )

            offset = assets.CROM_NES_TILE_BASE * 64
            self.assertEqual(
                decode_crom_tile(
                    crom1[offset : offset + 64],
                    crom2[offset : offset + 64],
                ),
                assets.expand_2x(self.tile),
            )
            self.assertEqual(
                decode_srom_tile(srom[:32]),
                self.tile,
            )
            blank = assets.SROM_BLANK_TILE * 32
            solid = assets.SROM_SOLID_TILE * 32
            self.assertEqual(
                decode_srom_tile(srom[blank : blank + 32]),
                bytes(64),
            )
            self.assertEqual(
                decode_srom_tile(srom[solid : solid + 32]),
                bytes([1]) * 64,
            )
            self.assertEqual(info["crom_nes_tile_base"], 257)

    def test_zip_is_read_without_extracting_member(self) -> None:
        rom = (
            b"NES\x1a"
            + bytes((2, 1, 1, 0))
            + bytes(8)
            + bytes(32 * 1024)
            + bytes(8 * 1024)
        )
        with tempfile.TemporaryDirectory() as directory:
            archive_path = Path(directory) / "input.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("nested/game.nes", rom)

            loaded, source = assets.load_rom(archive_path)
            self.assertEqual(loaded, rom)
            self.assertTrue(source.endswith(":nested/game.nes"))
            self.assertEqual(assets.extract_chr(loaded), bytes(8 * 1024))
            self.assertFalse((Path(directory) / "nested").exists())


if __name__ == "__main__":
    unittest.main()
