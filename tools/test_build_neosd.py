#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_neosd as neosd  # noqa: E402


def small_regions() -> dict[str, bytes]:
    return {
        "p": b"\x12\x34\x56\x78",
        "s": b"\x21\x22",
        "m": b"\x31\x32\x33",
        "v1": b"\x41\x42\x43\x44",
        "c1": b"\x01\x02\x03\x04",
        "c2": b"\x81\x82\x83\x84",
    }


class NeoSdBuilderTests(unittest.TestCase):
    def test_header_order_padding_and_crom_interleave(self) -> None:
        regions = small_regions()
        image = neosd.build_image(regions)
        header = neosd.validate_image(image, regions)

        self.assertEqual(image[:4], b"NEO\x01")
        self.assertEqual(
            struct.unpack_from("<6I", image, 4),
            (
                0x10000,
                0x10000,
                0x10000,
                0x10000,
                0,
                0x40000,
            ),
        )
        self.assertEqual(header.name, "Super Mario Bros. Neo")
        self.assertEqual(header.manufacturer, "Community port")
        self.assertEqual(header.year, 2026)
        self.assertEqual(header.genre, 5)
        self.assertEqual(header.ngh, 0x2026)
        self.assertTrue(all(value == 0 for value in image[0x5E:0x1000]))

        offsets = neosd.section_offsets(header)
        p_start, p_end = offsets["p"]
        self.assertEqual(image[p_start : p_start + 4], regions["p"])
        self.assertTrue(all(value == 0xFF for value in image[p_start + 4 : p_end]))

        c_start, c_end = offsets["c"]
        self.assertEqual(
            image[c_start : c_start + 8],
            b"\x01\x81\x02\x82\x03\x83\x04\x84",
        )
        self.assertTrue(all(value == 0xFF for value in image[c_start + 8 : c_end]))
        self.assertEqual(len(image), 0x81000)

    def test_program_region_remains_in_native_cartridge_byte_order(self) -> None:
        regions = small_regions()
        regions["p"] = b"\x4e\xf9\x00\xc0\x00\x80"
        image = neosd.build_image(regions)
        header = neosd.parse_header(image)
        start, _ = neosd.section_offsets(header)["p"]
        self.assertEqual(image[start : start + 6], regions["p"])
        self.assertNotEqual(image[start : start + 6], b"\xf9\x4e\xc0\x00\x80\x00")

    def test_rejects_bad_region_sets_and_metadata(self) -> None:
        regions = small_regions()
        del regions["s"]
        with self.assertRaisesRegex(neosd.NeoSdError, "missing s"):
            neosd.build_image(regions)

        regions = small_regions()
        regions["c2"] = regions["c2"][:-1]
        with self.assertRaisesRegex(neosd.NeoSdError, "C1 and C2 sizes differ"):
            neosd.build_image(regions)

        with self.assertRaisesRegex(neosd.NeoSdError, "ASCII"):
            neosd.build_image(small_regions(), name="Mário")
        with self.assertRaisesRegex(neosd.NeoSdError, "limit is 32"):
            neosd.build_image(small_regions(), name="x" * 33)
        with self.assertRaisesRegex(neosd.NeoSdError, "limit is 16"):
            neosd.build_image(small_regions(), manufacturer="x" * 17)

    def test_ngh_must_be_four_digit_packed_bcd(self) -> None:
        self.assertEqual(neosd.validate_packed_bcd_ngh(0x2026), 0x2026)
        with self.assertRaisesRegex(neosd.NeoSdError, "packed-BCD"):
            neosd.validate_packed_bcd_ngh(0x534D)
        with self.assertRaisesRegex(neosd.NeoSdError, "packed-BCD"):
            neosd.build_image(small_regions(), ngh=0x534D)

        invalid_header = bytearray(neosd.build_image(small_regions()))
        struct.pack_into("<I", invalid_header, 0x28, 0x534D)
        with self.assertRaisesRegex(neosd.NeoSdError, "packed-BCD"):
            neosd.parse_header(invalid_header)

    def test_validation_rejects_reserved_data_truncation_and_payload_changes(self) -> None:
        regions = small_regions()
        image = neosd.build_image(regions)

        bad_reserved = bytearray(image)
        bad_reserved[0x100] = 1
        with self.assertRaisesRegex(neosd.NeoSdError, "reserved header"):
            neosd.validate_image(bad_reserved)

        with self.assertRaisesRegex(neosd.NeoSdError, "header describes"):
            neosd.validate_image(image[:-1])

        bad_payload = bytearray(image)
        bad_payload[neosd.HEADER_SIZE] ^= 0xFF
        with self.assertRaisesRegex(neosd.NeoSdError, "offset 0x1000"):
            neosd.validate_image(bad_payload, regions)

    def test_validation_rejects_every_noncanonical_metadata_field(self) -> None:
        regions = small_regions()
        image = neosd.build_image(regions)
        mutations = (
            ("game name", 0x2C, b"X"),
            ("manufacturer", 0x4D, b"X"),
            ("year", 0x1C, struct.pack("<I", 1990)),
            ("genre", 0x20, struct.pack("<I", 10)),
            ("screenshot", 0x24, struct.pack("<I", 99)),
            ("NGH", 0x28, struct.pack("<I", 0)),
        )
        for label, offset, replacement in mutations:
            with self.subTest(field=label):
                damaged = bytearray(image)
                damaged[offset : offset + len(replacement)] = replacement
                with self.assertRaisesRegex(neosd.NeoSdError, label):
                    neosd.validate_image(damaged, regions)

    def test_structural_validator_accepts_unaligned_format_regions(self) -> None:
        sizes = (3, 5, 7, 9, 0, 11)
        header = bytearray(neosd.build_image(small_regions())[: neosd.HEADER_SIZE])
        struct.pack_into("<6I", header, 0x04, *sizes)
        image = bytes(header) + bytes(range(sum(sizes)))

        parsed = neosd.validate_image(image)
        self.assertEqual(
            (
                parsed.p_size,
                parsed.s_size,
                parsed.m_size,
                parsed.v1_size,
                parsed.v2_size,
                parsed.c_size,
            ),
            sizes,
        )

    @unittest.skipUnless(shutil.which("romtool.py"), "ngdevkit romtool.py is unavailable")
    def test_reference_packer_matches_ngdevkit_writer(self) -> None:
        regions = small_regions()
        regions["m"] = b"\x31\x32\x33\x34"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths = {}
            for region, data in regions.items():
                path = root / f"fixture-{region}.bin"
                path.write_bytes(data)
                paths[region] = path
            output = root / "ngdevkit.neo"
            completed = subprocess.run(
                [
                    shutil.which("romtool.py"),
                    "-b",
                    "cartridge",
                    "-f",
                    "neo",
                    "-p",
                    str(paths["p"]),
                    "-s",
                    str(paths["s"]),
                    "-m",
                    str(paths["m"]),
                    "-v",
                    str(paths["v1"]),
                    "-c",
                    str(paths["c1"]),
                    str(paths["c2"]),
                    "-n",
                    "smbneo",
                    "-l",
                    neosd.PRODUCT_NAME,
                    "-y",
                    str(neosd.YEAR),
                    "--publisher",
                    neosd.MANUFACTURER,
                    "-x",
                    "neo.genre=Platformer",
                    "neo.ngh=2026",
                    "-o",
                    str(output),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(output.read_bytes(), neosd.build_image(regions))

    def test_cli_loader_requires_the_full_native_smbneo_layout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            rom_dir = root / "rom"
            rom_dir.mkdir()
            for index, (region, filename) in enumerate(
                neosd.REGION_FILENAMES.items()
            ):
                size = neosd.REGION_SIZES[region]
                (rom_dir / filename).write_bytes(bytes((index + 1,)) * size)

            output = root / "smbneo.neo"
            header = neosd.write_image(rom_dir, output)
            self.assertEqual(output.stat().st_size, 0x5C1000)
            self.assertEqual(header.p_size, 0x100000)
            self.assertEqual(header.c_size, 0x400000)
            neosd.validate_file(output, rom_dir)

            (rom_dir / neosd.REGION_FILENAMES["m"]).write_bytes(b"short")
            with self.assertRaisesRegex(neosd.NeoSdError, "expected 131072"):
                neosd.write_image(rom_dir, output)


if __name__ == "__main__":
    unittest.main()
