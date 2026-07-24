#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_reproducible_cart as checker  # noqa: E402


REAL_REGIONS = checker.REGIONS
TINY_REGIONS = tuple(
    checker.Region(region.label, region.filename, index + 3)
    for index, region in enumerate(REAL_REGIONS)
)


class FakeBuilder:
    def __init__(
        self,
        *,
        mutate_second: dict[str, bytes] | None = None,
        manifest: dict | None = None,
        omit_manifest: bool = False,
    ) -> None:
        self.build_dirs: list[Path] = []
        self.commands: list[list[str]] = []
        self.mutate_second = mutate_second or {}
        self.manifest = manifest or {
            "verified_revision": True,
            "prg_bytes_written": 0,
        }
        self.omit_manifest = omit_manifest

    def __call__(self, command: list[str], *, check: bool) -> subprocess.CompletedProcess:
        self.commands.append(command)
        self.assert_checked(check)
        build_dir = Path(
            next(argument.split("=", 1)[1] for argument in command if argument.startswith("BUILD="))
        )
        self.build_dirs.append(build_dir)
        rom_dir = build_dir / "rom"
        assets_dir = build_dir / "assets"
        rom_dir.mkdir(parents=True)
        assets_dir.mkdir(parents=True)

        build_number = len(self.build_dirs)
        for index, region in enumerate(checker.REGIONS):
            content = bytes([0x20 + index]) * region.expected_size
            if build_number == 2 and region.filename in self.mutate_second:
                content = self.mutate_second[region.filename]
            (rom_dir / region.filename).write_bytes(content)

        zip_content = b"stable zip bytes"
        if build_number == 2 and checker.CART_FILENAME in self.mutate_second:
            zip_content = self.mutate_second[checker.CART_FILENAME]
        (rom_dir / checker.CART_FILENAME).write_bytes(zip_content)

        if not self.omit_manifest:
            (assets_dir / checker.MANIFEST_FILENAME).write_text(
                json.dumps(self.manifest),
                encoding="utf-8",
            )
        return subprocess.CompletedProcess(command, 0)

    @staticmethod
    def assert_checked(check: bool) -> None:
        if check is not True:
            raise AssertionError("build subprocess must use check=True")


class ReproducibleCartTests(unittest.TestCase):
    def setUp(self) -> None:
        self.region_patch = mock.patch.object(checker, "REGIONS", TINY_REGIONS)
        self.region_patch.start()
        self.addCleanup(self.region_patch.stop)

    def make_workspace(self, directory: str) -> tuple[Path, Path, Path]:
        root = Path(directory)
        neogeo_dir = root / "platform" / "neogeo"
        neogeo_dir.mkdir(parents=True)
        (neogeo_dir / "Makefile").write_text("# test fixture\n", encoding="utf-8")
        rom = root / "owned input.zip"
        rom.write_bytes(b"input")
        temp_parent = root / "checker-temporaries"
        temp_parent.mkdir()
        return neogeo_dir, rom, temp_parent

    def test_two_isolated_builds_match_and_preserve_working_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            neogeo_dir, rom, temp_parent = self.make_workspace(directory)
            working_build = neogeo_dir / "build"
            working_build.mkdir()
            sentinel = working_build / "keep-me"
            sentinel.write_text("untouched", encoding="utf-8")
            fake_builder = FakeBuilder()

            with mock.patch.object(
                checker.subprocess,
                "run",
                side_effect=fake_builder,
            ):
                results = checker.check_reproducible_cart(
                    rom,
                    neogeo_dir=neogeo_dir,
                    temp_parent=temp_parent,
                )

            self.assertEqual(
                [result.label for result in results],
                [region.label for region in TINY_REGIONS] + ["ZIP"],
            )
            self.assertEqual(len(fake_builder.build_dirs), 2)
            self.assertNotEqual(fake_builder.build_dirs[0], fake_builder.build_dirs[1])
            self.assertTrue(
                all(temp_parent in path.parents for path in fake_builder.build_dirs)
            )
            self.assertTrue(
                all(path != working_build for path in fake_builder.build_dirs)
            )
            self.assertTrue(
                all(not path.exists() for path in fake_builder.build_dirs)
            )
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "untouched")

            for command, build_dir in zip(
                fake_builder.commands,
                fake_builder.build_dirs,
            ):
                self.assertIn("cart", command)
                self.assertIn(f"BUILD={build_dir}", command)
                self.assertIn(f"SMB_ROM={rom.resolve()}", command)

    def test_reports_region_and_zip_first_byte_mismatches(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            neogeo_dir, rom, temp_parent = self.make_workspace(directory)
            c1 = next(region for region in checker.REGIONS if region.label == "C1")
            changed_c1 = bytes([0x21]) + bytes([0x55]) + bytes([0x21]) * (
                c1.expected_size - 2
            )
            fake_builder = FakeBuilder(
                mutate_second={
                    c1.filename: changed_c1,
                    checker.CART_FILENAME: b"stable Zip bytes",
                }
            )

            with mock.patch.object(
                checker.subprocess,
                "run",
                side_effect=fake_builder,
            ):
                with self.assertRaises(checker.CartCheckError) as raised:
                    checker.check_reproducible_cart(
                        rom,
                        neogeo_dir=neogeo_dir,
                        temp_parent=temp_parent,
                    )

            message = str(raised.exception)
            self.assertIn("C1 bytes differ at offset 0x1", message)
            self.assertIn("build 1 has 0x21, build 2 has 0x55", message)
            self.assertIn("ZIP bytes differ at offset 0x7", message)
            self.assertIn("SHA-256 values are", message)

    def test_reports_all_bad_sizes_and_manifest_fields(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            neogeo_dir, rom, temp_parent = self.make_workspace(directory)
            p_region = next(region for region in checker.REGIONS if region.label == "P")
            fake_builder = FakeBuilder(
                mutate_second={p_region.filename: b"x"},
                manifest={
                    "verified_revision": False,
                    "prg_bytes_written": 1,
                },
            )

            with mock.patch.object(
                checker.subprocess,
                "run",
                side_effect=fake_builder,
            ):
                with self.assertRaises(checker.CartCheckError) as raised:
                    checker.check_reproducible_cart(
                        rom,
                        neogeo_dir=neogeo_dir,
                        temp_parent=temp_parent,
                    )

            message = str(raised.exception)
            self.assertIn("build 2 P region has 1 bytes", message)
            self.assertIn("verified_revision=False; expected true", message)
            self.assertIn("prg_bytes_written=1; expected 0", message)
            self.assertIn("P bytes differ", message)

    def test_missing_manifest_is_rejected_for_both_builds(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            neogeo_dir, rom, temp_parent = self.make_workspace(directory)
            fake_builder = FakeBuilder(omit_manifest=True)

            with mock.patch.object(
                checker.subprocess,
                "run",
                side_effect=fake_builder,
            ):
                with self.assertRaises(checker.CartCheckError) as raised:
                    checker.check_reproducible_cart(
                        rom,
                        neogeo_dir=neogeo_dir,
                        temp_parent=temp_parent,
                    )

            message = str(raised.exception)
            self.assertIn("build 1 is missing asset manifest", message)
            self.assertIn("build 2 is missing asset manifest", message)

    def test_real_region_sizes_match_cartridge_layout(self) -> None:
        expected = {
            "P": 1024 * 1024,
            "C1": 2 * 1024 * 1024,
            "C2": 2 * 1024 * 1024,
            "S": 128 * 1024,
            "M": 128 * 1024,
            "V": 512 * 1024,
        }
        actual = {
            region.label: region.expected_size
            for region in REAL_REGIONS
        }
        self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
