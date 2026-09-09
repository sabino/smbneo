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

    def test_odd_title_symbol_offset_is_rejected(self) -> None:
        completed = subprocess.CompletedProcess(
            ["m68k-neogeo-elf-nm"],
            0,
            stdout="000004c1 r neogeo_title_screen_data\n",
            stderr="",
        )
        with mock.patch.object(
            web_builder.subprocess,
            "run",
            return_value=completed,
        ):
            with self.assertRaisesRegex(
                web_builder.BuildError,
                "odd P-ROM offset 0x4c1",
            ):
                web_builder.find_title_offset(Path("fixture.elf"), "nm")

    def test_vs_data_offsets_require_exact_symbols_and_sizes(self) -> None:
        completed = subprocess.CompletedProcess(
            ["nm"],
            0,
            stdout=(
                "00001000 00008000 R vs_prg\n"
                "00009000 00004000 R vs_chr\n"
                "0000d000 00000080 R vs_palette_neogeo\n"
            ),
            stderr="",
        )
        with mock.patch.object(
            web_builder.subprocess,
            "run",
            return_value=completed,
        ):
            self.assertEqual(
                web_builder.find_vs_data_offsets(Path("fixture.elf"), "nm"),
                {"prg": 0x1000, "chr": 0x9000, "palette": 0xD000},
            )

        wrong_size = subprocess.CompletedProcess(
            ["nm"],
            0,
            stdout=completed.stdout.replace("00004000 R vs_chr", "00002000 R vs_chr"),
            stderr="",
        )
        with mock.patch.object(
            web_builder.subprocess,
            "run",
            return_value=wrong_size,
        ):
            with self.assertRaisesRegex(
                web_builder.BuildError,
                "vs_chr is 8192 bytes",
            ):
                web_builder.find_vs_data_offsets(Path("fixture.elf"), "nm")

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
            native_elf = root / "native-template.elf"
            native_elf.write_bytes(b"native ELF fixture")
            web_elf = root / "web-template.elf"
            web_elf.write_bytes(b"web ELF fixture")
            native_prom = root / "smbneo-p1.p1"
            web_prom = root / "smbneo-web-p1.p1"
            vs_native_elf = root / "vssmbneo-native-template.elf"
            vs_web_elf = root / "vssmbneo-web-template.elf"
            vs_native_elf.write_bytes(b"native VS ELF fixture")
            vs_web_elf.write_bytes(b"web VS ELF fixture")
            vs_native_prom = root / "vssmbneo-p1.p1"
            vs_web_prom = root / "vssmbneo-web-p1.p1"
            mrom = root / "smbneo-m1.m1"
            vrom = root / "smbneo-v1.v1"
            title_offset = 0x046A
            home_prom = bytearray([0xFF]) * web_builder.TEMPLATE_ENTRIES[
                native_prom.name
            ]
            home_prom[title_offset:title_offset + web_builder.TITLE_BYTES] = (
                bytes(web_builder.TITLE_BYTES)
            )
            native_prom.write_bytes(home_prom)
            web_prom.write_bytes(home_prom)
            vs_offsets = {"prg": 0x1000, "chr": 0x9000, "palette": 0xD000}
            vs_prom = bytearray([0xFF]) * web_builder.VS_TEMPLATE_ENTRIES[
                vs_native_prom.name
            ]
            for field, (_, size) in web_builder.VS_DATA_SYMBOLS.items():
                offset = vs_offsets[field]
                vs_prom[offset:offset + size] = bytes(size)
            vs_native_prom.write_bytes(vs_prom)
            vs_web_prom.write_bytes(vs_prom)
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
                native_elf=native_elf,
                native_prom=native_prom,
                web_elf=web_elf,
                web_prom=web_prom,
                vs_native_elf=vs_native_elf,
                vs_native_prom=vs_native_prom,
                vs_web_elf=vs_web_elf,
                vs_web_prom=vs_web_prom,
                mrom=mrom,
                vrom=vrom,
                bios=bios,
                nm="nm",
                output=output,
                templates=templates,
                title_image=title,
            )
            def inspect_elf(command, **_kwargs):
                if "-S" in command:
                    return subprocess.CompletedProcess(
                        command,
                        0,
                        stdout=(
                            "00001000 00008000 R vs_prg\n"
                            "00009000 00004000 R vs_chr\n"
                            "0000d000 00000080 R vs_palette_neogeo\n"
                        ),
                        stderr="",
                    )
                return subprocess.CompletedProcess(
                    command,
                    0,
                    stdout="0000046a r neogeo_title_screen_data\n",
                    stderr="",
                )
            with mock.patch.object(
                web_builder.subprocess,
                "run",
                side_effect=inspect_elf,
            ):
                web_builder.build(args)
                first_manifest = (output / "build-manifest.json").read_bytes()
                first_template = (
                    output / "assets" / "smbneo-template.zip"
                ).read_bytes()
                first_vs_template = (
                    output / "assets" / "vssmbneo-template.zip"
                ).read_bytes()
                first_bios = (output / "neogeo.zip").read_bytes()
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
                (output / "assets" / "vssmbneo-template.zip").read_bytes(),
                first_vs_template,
            )
            self.assertEqual(
                (output / "neogeo.zip").read_bytes(),
                first_bios,
            )

            manifest = json.loads(first_manifest)
            self.assertEqual(manifest["project"], "SMBNeo")
            self.assertEqual(manifest["product"]["shortname"], "smbneo")
            self.assertEqual(
                manifest["product"]["title"],
                "Super Mario Bros. Neo",
            )
            self.assertEqual(manifest["fbneo_driver"], "puzzledp")
            self.assertEqual(manifest["bios"]["path"], "neogeo.zip")
            self.assertEqual(
                manifest["title_patch_offsets"],
                {"native": 0x046A, "web": 0x046A},
            )
            self.assertEqual(
                manifest["downloads"]["canonical"]["filename"],
                "smbneo.zip",
            )
            self.assertEqual(
                manifest["downloads"]["neosd"]["filename"],
                "smbneo.neo",
            )
            self.assertEqual(
                manifest["downloads"]["neosd"]["shortname"],
                "smbneo",
            )
            self.assertEqual(
                manifest["downloads"]["compatibility"]["filename"],
                "puzzledp.zip",
            )
            self.assertEqual(set(manifest["editions"]), {"home", "vs"})
            self.assertEqual(
                manifest["editions"]["vs"]["title"],
                "VS. Super Mario Bros. Neo",
            )
            self.assertEqual(
                manifest["editions"]["vs"]["patch_offsets"],
                {"native": vs_offsets, "web": vs_offsets},
            )
            self.assertEqual(
                manifest["editions"]["vs"]["downloads"]["canonical"]["filename"],
                "vssmbneo.zip",
            )
            self.assertEqual(
                manifest["editions"]["vs"]["downloads"]["neosd"]["filename"],
                "vssmbneo.neo",
            )
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
            with zipfile.ZipFile(
                output / "assets" / "vssmbneo-template.zip"
            ) as archive:
                self.assertEqual(
                    set(archive.namelist()),
                    set(web_builder.VS_TEMPLATE_ENTRIES),
                )
                for name in ("vssmbneo-p1.p1", "vssmbneo-web-p1.p1"):
                    prom = archive.read(name)
                    for field, (_, size) in web_builder.VS_DATA_SYMBOLS.items():
                        offset = vs_offsets[field]
                        self.assertEqual(prom[offset:offset + size], bytes(size))
                    self.assertEqual(
                        prom[web_builder.FBNEO_P_BYTES:],
                        bytes([0xFF]) * (len(prom) - web_builder.FBNEO_P_BYTES),
                    )
            with zipfile.ZipFile(output / "neogeo.zip") as archive:
                for name, (_, expected_crc) in web_builder.BIOS_ENTRIES.items():
                    self.assertEqual(archive.getinfo(name).CRC, expected_crc)


if __name__ == "__main__":
    unittest.main()
