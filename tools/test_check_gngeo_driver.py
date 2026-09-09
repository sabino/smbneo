#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest
import zipfile
import zlib

import check_gngeo_driver as checker


def padded(value: str, size: int) -> bytes:
    return value.encode("ascii").ljust(size, b"\0")


class GnGeoDriverTests(unittest.TestCase):
    def build_fixture(
        self,
        root: Path,
        *,
        shortname: str = "smbneo",
        title: str = "Super Mario Bros. Neo",
        filename_prefix: str | None = None,
    ) -> tuple[Path, Path]:
        rom_dir = root / "roms"
        rom_dir.mkdir()
        region_sizes = [0] * 10
        records = bytearray()
        expected_roms = checker.expected_roms(filename_prefix or shortname)
        for index, expected in enumerate(expected_roms):
            data = bytes((index * 31 + offset) & 0xFF for offset in range(expected.size))
            (rom_dir / expected.filename).write_bytes(data)
            region_sizes[expected.region] += expected.size
            records += checker.RECORD.pack(
                padded(expected.filename, 32),
                expected.region,
                0,
                expected.destination,
                expected.size,
                zlib.crc32(data) & 0xFFFFFFFF,
            )

        driver = checker.HEADER.pack(
            padded(shortname, 32),
            padded("neogeo", 32),
            padded(title, 128),
            2026,
            *region_sizes,
            len(expected_roms),
        ) + records
        data_zip = root / "gngeo_data.zip"
        with zipfile.ZipFile(data_zip, "w") as archive:
            archive.writestr(f"rom/{shortname}.drv", driver)
            archive.writestr("skin/README.md", "fixture")
        return data_zip, rom_dir

    def test_accepts_exact_custom_identity_and_native_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            data_zip, rom_dir = self.build_fixture(Path(temporary))
            checker.validate_archive(data_zip, rom_dir)

    def test_rejects_donor_or_legacy_driver_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            data_zip, rom_dir = self.build_fixture(Path(temporary))
            with zipfile.ZipFile(data_zip, "a") as archive:
                archive.writestr("rom/puzzledp.drv", b"donor")
            with self.assertRaisesRegex(
                checker.DriverError,
                "expected only 'rom/smbneo.drv'",
            ):
                checker.validate_archive(data_zip, rom_dir)

    def test_rejects_crc_that_does_not_match_native_region(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            data_zip, rom_dir = self.build_fixture(Path(temporary))
            path = rom_dir / checker.EXPECTED_ROMS[0].filename
            data = bytearray(path.read_bytes())
            data[0] ^= 0xFF
            path.write_bytes(data)
            with self.assertRaisesRegex(
                checker.DriverError,
                "differs from the native",
            ):
                checker.validate_archive(data_zip, rom_dir)

    def test_accepts_vs_identity_without_changing_home_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            data_zip, rom_dir = self.build_fixture(
                Path(temporary),
                shortname="vssmbneo",
                title="VS. Super Mario Bros. Neo",
            )
            checker.validate_archive(
                data_zip,
                rom_dir,
                shortname="vssmbneo",
                title="VS. Super Mario Bros. Neo",
            )
            self.assertEqual(
                checker.EXPECTED_ROMS,
                checker.expected_roms("smbneo"),
            )

    def test_rejects_invalid_identity_before_constructing_archive_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(checker.DriverError, "invalid driver"):
                checker.validate_archive(
                    Path(temporary) / "missing.zip",
                    Path(temporary),
                    shortname="../vssmbneo",
                )


if __name__ == "__main__":
    unittest.main()
