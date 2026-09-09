#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
import zlib

import build_neosd
import check_gngeo_driver
import check_vs_package as checker
from gen_mame_neogeo_software import write_software_list
from package_vs import write_deterministic_zip


def padded(value: str, size: int) -> bytes:
    return value.encode("ascii").ljust(size, b"\0")


class VsPackageTests(unittest.TestCase):
    def build_fixture(self, root: Path) -> tuple[Path, dict[str, bytes]]:
        build = root / "build"
        rom_dir = build / "rom"
        rom_dir.mkdir(parents=True)
        regions = {
            region.filename: bytes((index + 1,)) * region.size
            for index, region in enumerate(checker.REGIONS)
        }
        for filename, data in regions.items():
            (rom_dir / filename).write_bytes(data)

        write_deterministic_zip(
            rom_dir / checker.CANONICAL_ZIP,
            list(regions.items()),
        )
        write_software_list(
            rom_dir,
            build / checker.MAME_XML,
            checker.SHORTNAME,
            checker.TITLE,
        )
        native_regions = {
            region.key: regions[region.filename] for region in checker.REGIONS
        }
        (rom_dir / checker.NEOSD_IMAGE).write_bytes(
            build_neosd.build_image(
                native_regions,
                name=checker.TITLE,
                manufacturer=checker.MANUFACTURER,
                ngh=checker.NGH,
            )
        )
        self.write_gngeo(rom_dir / checker.GNGEO_DATA, regions)
        (rom_dir / checker.MANIFEST).write_text(
            json.dumps(checker.build_manifest(regions), indent=2) + "\n",
            encoding="utf-8",
        )
        return build, regions

    def write_gngeo(
        self,
        path: Path,
        regions: dict[str, bytes],
        *,
        corrupt_crc: bool = False,
    ) -> None:
        expected_roms = check_gngeo_driver.expected_roms(checker.SHORTNAME)
        region_sizes = [0] * 10
        records = bytearray()
        for index, expected in enumerate(expected_roms):
            region_sizes[expected.region] += expected.size
            crc = zlib.crc32(regions[expected.filename]) & 0xFFFFFFFF
            if corrupt_crc and index == 0:
                crc ^= 1
            records += check_gngeo_driver.RECORD.pack(
                padded(expected.filename, 32),
                expected.region,
                0,
                expected.destination,
                expected.size,
                crc,
            )
        driver = check_gngeo_driver.HEADER.pack(
            padded(checker.SHORTNAME, 32),
            padded("neogeo", 32),
            padded(checker.TITLE, 128),
            checker.YEAR,
            *region_sizes,
            len(expected_roms),
        ) + records
        write_deterministic_zip(
            path,
            [
                (f"rom/{checker.SHORTNAME}.drv", driver),
                ("skin/README.md", b"ROM-free test fixture\n"),
            ],
        )

    def test_accepts_all_exact_full_layout_representations(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build, regions = self.build_fixture(root)
            self.assertEqual(checker.validate_package(build), regions)

            duplicate = root / "same.zip"
            write_deterministic_zip(duplicate, list(regions.items()))
            self.assertEqual(
                duplicate.read_bytes(),
                (build / "rom" / checker.CANONICAL_ZIP).read_bytes(),
            )

    def test_rejects_zip_bytes_that_differ_from_loose_regions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build, regions = self.build_fixture(Path(temporary))
            archived = dict(regions)
            archived[checker.REGION_NAMES[0]] = (
                bytes((archived[checker.REGION_NAMES[0]][0] ^ 1,))
                + archived[checker.REGION_NAMES[0]][1:]
            )
            write_deterministic_zip(
                build / "rom" / checker.CANONICAL_ZIP,
                list(archived.items()),
            )
            with self.assertRaisesRegex(
                checker.PackageError,
                "wrong ZIP CRC|differs",
            ):
                checker.validate_package(build)

    def test_rejects_incorrect_mame_p_rom_loading_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build, _ = self.build_fixture(Path(temporary))
            path = build / checker.MAME_XML
            path.write_text(
                path.read_text(encoding="utf-8").replace(
                    'loadflag="load16_word_swap"',
                    'loadflag="load16_byte"',
                    1,
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(checker.PackageError, "loading semantics"):
                checker.validate_package(build)

    def test_rejects_neosd_metadata_or_payload_divergence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build, regions = self.build_fixture(Path(temporary))
            native_regions = {
                region.key: regions[region.filename]
                for region in checker.REGIONS
            }
            (build / "rom" / checker.NEOSD_IMAGE).write_bytes(
                build_neosd.build_image(
                    native_regions,
                    name="Wrong title",
                    manufacturer=checker.MANUFACTURER,
                    ngh=checker.NGH,
                )
            )
            with self.assertRaisesRegex(checker.PackageError, "game name"):
                checker.validate_package(build)

    def test_rejects_gngeo_driver_not_bound_to_the_same_regions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build, regions = self.build_fixture(Path(temporary))
            self.write_gngeo(
                build / "rom" / checker.GNGEO_DATA,
                regions,
                corrupt_crc=True,
            )
            with self.assertRaisesRegex(checker.PackageError, "GnGeo"):
                checker.validate_package(build)

    def test_rejects_stale_experimental_or_inexact_hash_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build, regions = self.build_fixture(Path(temporary))
            manifest = checker.build_manifest(regions)
            manifest["experimental"] = True
            (build / "rom" / checker.MANIFEST).write_text(
                json.dumps(manifest),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(checker.PackageError, "exactly describe"):
                checker.validate_package(build)


if __name__ == "__main__":
    unittest.main()
