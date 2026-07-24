#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_web_player as web_builder  # noqa: E402


class WebPlayerBuilderTests(unittest.TestCase):
    def test_crc_correction_reaches_requested_value(self) -> None:
        data = bytes(range(64)) + bytes(512)
        patch_offset, padding_byte, padding_run = (
            web_builder.find_padding_patch_offset(data, "fixture")
        )
        self.assertEqual(padding_byte, 0)
        self.assertGreaterEqual(padding_run, 512)

        desired = 0x59374C47
        patched = web_builder.force_crc32(data, desired, patch_offset)
        self.assertEqual(web_builder.crc32(patched), desired)
        self.assertEqual(patched[:64], data[:64])

    def test_title_symbol_offset_is_read_from_nm(self) -> None:
        completed = subprocess.CompletedProcess(
            ["m68k-neogeo-elf-nm"],
            0,
            stdout=(
                "000002ec T rom_title\n"
                "0000046a r neogeo_title_screen_data\n"
            ),
            stderr="",
        )
        with mock.patch.object(
            web_builder.subprocess,
            "run",
            return_value=completed,
        ):
            self.assertEqual(
                web_builder.find_title_offset(Path("fixture.elf"), "nm"),
                0x046A,
            )

    def test_complete_site_is_rom_free_and_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            templates = root / "web"
            templates.mkdir()
            (templates / "index.html").write_text(
                "<p>build __BUILD_ID__</p>\n",
                encoding="utf-8",
            )
            (templates / "player.mjs").write_text(
                "export const player = true;\n",
                encoding="utf-8",
            )
            title = root / "title.png"
            title.write_bytes(b"\x89PNG\r\n\x1a\nfixture")
            elf = root / "template.elf"
            elf.write_bytes(b"ELF fixture")
            prom = root / "smbneogeo-p1.p1"
            mrom = root / "smbneogeo-m1.m1"
            vrom = root / "smbneogeo-v1.v1"
            prom.write_bytes(bytes(web_builder.TEMPLATE_ENTRIES[prom.name]))
            mrom.write_bytes(bytes(web_builder.TEMPLATE_ENTRIES[mrom.name]))
            vrom.write_bytes(bytes(web_builder.TEMPLATE_ENTRIES[vrom.name]))

            bios = root / "neogeo.zip"
            with zipfile.ZipFile(bios, "w") as archive:
                archive.writestr("aes-bios.bin", bytes(0x20000))
                archive.writestr("sm1.sm1", bytes(0x20000))
                archive.writestr("sfix.sfix", bytes(0x20000))
                archive.writestr("000-lo.lo", bytes(0x20000))

            output = root / "site"
            args = argparse.Namespace(
                elf=elf,
                prom=prom,
                mrom=mrom,
                vrom=vrom,
                bios=bios,
                nm="nm",
                output=output,
                templates=templates,
                title_image=title,
            )
            completed = subprocess.CompletedProcess(
                ["nm"],
                0,
                stdout="0000046a r neogeo_title_screen_data\n",
                stderr="",
            )
            with mock.patch.object(
                web_builder.subprocess,
                "run",
                return_value=completed,
            ):
                web_builder.build(args)
                first_manifest = (output / "build-manifest.json").read_bytes()
                first_template = (
                    output / "assets" / "smbneo-template.zip"
                ).read_bytes()
                first_bios = (output / "assets" / "neogeo.zip").read_bytes()
                web_builder.build(args)

            self.assertEqual(
                (output / "build-manifest.json").read_bytes(),
                first_manifest,
            )
            self.assertEqual(
                (output / "assets" / "smbneo-template.zip").read_bytes(),
                first_template,
            )
            self.assertEqual(
                (output / "assets" / "neogeo.zip").read_bytes(),
                first_bios,
            )

            manifest = json.loads(first_manifest)
            self.assertEqual(manifest["project"], "SMBNeo")
            self.assertEqual(manifest["fbneo_driver"], "19yy")
            self.assertEqual(manifest["title_patch_offset"], 0x046A)
            self.assertFalse(manifest["privacy"]["game_image_included"])
            self.assertFalse(
                manifest["privacy"]["generated_graphics_included"]
            )
            self.assertFalse(
                manifest["privacy"]["generated_cartridge_included"]
            )
            self.assertNotIn(str(root), first_manifest.decode("utf-8"))

            with zipfile.ZipFile(
                output / "assets" / "smbneo-template.zip"
            ) as archive:
                self.assertEqual(
                    set(archive.namelist()),
                    set(web_builder.TEMPLATE_ENTRIES),
                )
            with zipfile.ZipFile(output / "assets" / "neogeo.zip") as archive:
                for name, (_, expected_crc) in web_builder.BIOS_ENTRIES.items():
                    self.assertEqual(archive.getinfo(name).CRC, expected_crc)


if __name__ == "__main__":
    unittest.main()
