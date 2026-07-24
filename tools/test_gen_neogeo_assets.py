#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import re
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_neogeo_assets as assets  # noqa: E402


ROOT = Path(__file__).resolve().parents[1]
NEOGEO_VIDEO = ROOT / "platform" / "neogeo" / "video.c"


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

    def test_hardware_shrink_recovers_every_source_pixel(self) -> None:
        video_source = NEOGEO_VIDEO.read_text(encoding="utf-8")
        self.assertRegex(
            video_source,
            r"(?m)^#define NEO_ZOOM_OAM_8X8 0x077eu$",
        )
        self.assertRegex(
            video_source,
            r"(?m)^#define NEO_ZOOM_BACKGROUND_8X8 0x077fu$",
        )

        expanded = assets.expand_2x(self.tile)
        # The in-tile portion of both documented vertical shrink maps selects
        # the even source rows; horizontal zoom 7 selects even columns.
        # expand_2x duplicates each pixel into that exact 2x2 footprint.
        hardware_output = bytes(
            expanded[y * 16 + x]
            for y in range(0, 16, 2)
            for x in range(0, 16, 2)
        )
        self.assertEqual(hardware_output, self.tile)

    def test_srom_round_trip(self) -> None:
        encoded = assets.encode_srom_tile(self.tile)
        self.assertEqual(len(encoded), 32)
        self.assertEqual(decode_srom_tile(encoded), self.tile)

    def test_complete_asset_layout_and_helper_tiles(self) -> None:
        chr_data = bytearray(
            encode_nes_tile(self.tile) + bytes(assets.NES_CHR_SIZE - 16)
        )
        expected_title = bytes(
            (index * 29 + 7) & 0xFF
            for index in range(assets.TITLE_SCREEN_CHR_SIZE)
        )
        title_end = (
            assets.TITLE_SCREEN_CHR_OFFSET + assets.TITLE_SCREEN_CHR_SIZE
        )
        chr_data[assets.TITLE_SCREEN_CHR_OFFSET:title_end] = expected_title
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            info = assets.build_assets(bytes(chr_data), output)
            crom1 = (output / "smbneogeo-c1.c1").read_bytes()
            crom2 = (output / "smbneogeo-c2.c2").read_bytes()
            srom = (output / "smbneogeo-s1.s1").read_bytes()
            title_source = (output / assets.TITLE_SCREEN_SOURCE).read_text(
                encoding="ascii"
            )

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
            self.assertEqual(
                info["title_screen_chr_bytes"],
                assets.TITLE_SCREEN_CHR_SIZE,
            )
            emitted_title = bytes(
                int(value, 16)
                for value in re.findall(r"0x([0-9a-f]{2})", title_source)
            )
            self.assertEqual(emitted_title, expected_title)
            self.assertIn('#include "title_data.h"', title_source)

    def test_title_screen_data_requires_complete_chr(self) -> None:
        with self.assertRaisesRegex(assets.AssetError, "expected 8192 CHR bytes"):
            assets.title_screen_data(bytes(assets.NES_CHR_SIZE - 1))

    def test_title_screen_source_requires_exact_payload(self) -> None:
        with self.assertRaisesRegex(assets.AssetError, "expected 314 title bytes"):
            assets.format_title_screen_source(
                bytes(assets.TITLE_SCREEN_CHR_SIZE - 1)
            )

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
