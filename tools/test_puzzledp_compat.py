#!/usr/bin/env python3
"""Regression tests for the native Puzzle De Pon compatibility builder."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import puzzledp_compat as compatibility


ROOT = Path(__file__).resolve().parents[1]
WEB_CONVERTER = ROOT / "web" / "compat.mjs"


def synthetic_native_regions() -> dict[str, bytes]:
    regions: dict[str, bytes] = {}
    for region_index, region in enumerate(compatibility.NATIVE_REGIONS):
        data = bytearray([region.padding_byte]) * region.size
        for offset in range(4096):
            data[offset] = (region_index * 37 + offset * 13 + 7) & 0xFF
        regions[region.part] = bytes(data)
    return regions


def load16_word_swap(data: bytes) -> bytes:
    if len(data) % 2 != 0:
        raise ValueError("word-swapped data must have an even size")
    loaded = bytearray(len(data))
    for offset in range(0, len(data), 2):
        loaded[offset] = data[offset + 1]
        loaded[offset + 1] = data[offset]
    return bytes(loaded)


class PuzzledpCompatibilityTests(unittest.TestCase):
    def test_exact_filenames_sizes_and_crcs(self) -> None:
        entries = compatibility.convert_regions(synthetic_native_regions())
        self.assertEqual(
            set(entries),
            {region.filename for region in compatibility.PUZZLEDP_REGIONS},
        )
        for region in compatibility.PUZZLEDP_REGIONS:
            entry = entries[region.filename]
            self.assertEqual(len(entry), region.size, region.filename)
            self.assertEqual(
                compatibility.crc32(entry),
                region.crc32,
                region.filename,
            )

    def test_crc_correction_only_changes_verified_padding_tail(self) -> None:
        native = synthetic_native_regions()
        entries = compatibility.convert_regions(native)

        for region in compatibility.PUZZLEDP_REGIONS:
            source = native[region.part]
            converted = entries[region.filename]
            self.assertEqual(
                converted[:-4],
                source[: region.size - 4],
                region.filename,
            )
            native_region = compatibility.NATIVE_BY_PART[region.part]
            self.assertEqual(
                set(source[region.size - compatibility.MINIMUM_PADDING_RUN :]),
                {native_region.padding_byte},
                region.filename,
            )

    def test_rejects_non_padding_data_that_truncation_would_drop(self) -> None:
        for part in ("p", "c1", "c2"):
            with self.subTest(part=part):
                native = synthetic_native_regions()
                compatibility_region = compatibility.PUZZLEDP_BY_PART[part]
                changed = bytearray(native[part])
                changed[compatibility_region.size] ^= 0x5A
                native[part] = bytes(changed)
                with self.assertRaisesRegex(
                    compatibility.CompatibilityError,
                    "cannot omit",
                ):
                    compatibility.convert_regions(native)

    def test_rejects_crc_correction_over_live_tail_data(self) -> None:
        native = synthetic_native_regions()
        changed = bytearray(native["v"])
        changed[-16] = 0x7E
        native["v"] = bytes(changed)
        with self.assertRaisesRegex(
            compatibility.CompatibilityError,
            "CRC correction tail",
        ):
            compatibility.convert_regions(native)

    def test_p_rom_keeps_first_half_and_native_word_swap_semantics(self) -> None:
        native = synthetic_native_regions()
        p_rom = bytearray(native["p"])
        p_rom[:8] = bytes.fromhex("123456789abcdef0")
        p_rom[0x80000 : 0x80008] = b"\xff" * 8
        native["p"] = bytes(p_rom)

        converted = compatibility.convert_regions(native)["202-p1.bin"]
        self.assertEqual(converted[:8], bytes.fromhex("123456789abcdef0"))
        self.assertEqual(
            load16_word_swap(converted[:8]),
            bytes.fromhex("34127856bc9af0de"),
        )

    def test_archive_has_no_extra_or_duplicate_members(self) -> None:
        entries = compatibility.convert_regions(synthetic_native_regions())
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "puzzledp.zip"
            compatibility.write_archive(archive_path, entries)
            compatibility.validate_archive(archive_path)

            with zipfile.ZipFile(archive_path) as archive:
                self.assertEqual(
                    archive.namelist(),
                    sorted(
                        region.filename
                        for region in compatibility.PUZZLEDP_REGIONS
                    ),
                )
                for region in compatibility.PUZZLEDP_REGIONS:
                    info = archive.getinfo(region.filename)
                    self.assertEqual(info.file_size, region.size)
                    self.assertEqual(info.CRC, region.crc32)

    def test_web_and_native_converters_are_byte_identical(self) -> None:
        native = synthetic_native_regions()
        expected = compatibility.convert_regions(native)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            input_dir = root / "native"
            output_dir = root / "web"
            input_dir.mkdir()
            output_dir.mkdir()
            for region in compatibility.NATIVE_REGIONS:
                (input_dir / f"{region.part}.bin").write_bytes(native[region.part])

            script = """
import { readFile, writeFile } from "node:fs/promises";
import { buildPuzzledpEntries } from %s;
const [inputDir, outputDir] = process.argv.slice(1);
const parts = {};
for (const part of ["p", "s", "m", "v", "c1", "c2"]) {
  parts[part] = new Uint8Array(await readFile(`${inputDir}/${part}.bin`));
}
const entries = buildPuzzledpEntries(parts);
for (const [name, data] of Object.entries(entries)) {
  await writeFile(`${outputDir}/${name}`, data);
}
""" % json.dumps(WEB_CONVERTER.as_uri())
            subprocess.run(
                [
                    "node",
                    "--input-type=module",
                    "--eval",
                    script,
                    str(input_dir),
                    str(output_dir),
                ],
                check=True,
            )

            for name, expected_data in expected.items():
                self.assertEqual(
                    (output_dir / name).read_bytes(),
                    expected_data,
                    name,
                )


if __name__ == "__main__":
    unittest.main()
