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
    def build_fixture(self, root: Path) -> tuple[Path, Path]:
        rom_dir = root / "roms"
        rom_dir.mkdir()
        region_sizes = [0] * 10
        records = bytearray()
        for index, expected in enumerate(checker.EXPECTED_ROMS):
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
            padded("smbneo", 32),
            padded("neogeo", 32),
            padded("Super Mario Bros. Neo", 128),
            2026,
            *region_sizes,
            len(checker.EXPECTED_ROMS),
        ) + records
        data_zip = root / "gngeo_data.zip"
        with zipfile.ZipFile(data_zip, "w") as archive:
            archive.writestr("rom/smbneo.drv", driver)
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


if __name__ == "__main__":
    unittest.main()
