#!/usr/bin/env python3
"""Build and validate the Puzzle De Pon compatibility cartridge for SMBNeo."""

from __future__ import annotations

import argparse
import binascii
from dataclasses import dataclass
from pathlib import Path
import sys
import zipfile
from typing import Mapping, Sequence


FIXED_ZIP_TIME = (2026, 1, 1, 0, 0, 0)
MINIMUM_PADDING_RUN = 64


class CompatibilityError(RuntimeError):
    """The native regions cannot be represented by the compatibility profile."""


@dataclass(frozen=True)
class NativeRegion:
    part: str
    filename: str
    size: int
    padding_byte: int


@dataclass(frozen=True)
class CompatibilityRegion:
    part: str
    filename: str
    size: int
    crc32: int
    load_semantics: str


NATIVE_REGIONS = (
    NativeRegion("p", "smbneogeo-p1.p1", 0x100000, 0xFF),
    NativeRegion("s", "smbneogeo-s1.s1", 0x020000, 0x00),
    NativeRegion("m", "smbneogeo-m1.m1", 0x020000, 0x00),
    NativeRegion("v", "smbneogeo-v1.v1", 0x080000, 0x00),
    NativeRegion("c1", "smbneogeo-c1.c1", 0x200000, 0x00),
    NativeRegion("c2", "smbneogeo-c2.c2", 0x200000, 0x00),
)

PUZZLEDP_REGIONS = (
    CompatibilityRegion(
        "p", "202-p1.bin", 0x080000, 0x2B61415B, "load16_word_swap@0"
    ),
    CompatibilityRegion("s", "202-s1.bin", 0x020000, 0xCD19264F, "linear@0"),
    CompatibilityRegion("m", "202-m1.bin", 0x020000, 0x9C0291EA, "linear@0"),
    CompatibilityRegion("v", "202-v1.bin", 0x080000, 0xDEBEB8FB, "linear@0"),
    CompatibilityRegion(
        "c1", "202-c1.bin", 0x100000, 0xCC0095EF, "load16_byte@0"
    ),
    CompatibilityRegion(
        "c2", "202-c2.bin", 0x100000, 0x42371307, "load16_byte@1"
    ),
)

NATIVE_BY_PART = {region.part: region for region in NATIVE_REGIONS}
PUZZLEDP_BY_PART = {region.part: region for region in PUZZLEDP_REGIONS}


def crc32(data: bytes | bytearray | memoryview) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def _force_crc32(data: bytes, desired_crc: int, patch_offset: int) -> bytes:
    """Solve the final four bytes so data has desired_crc."""

    if patch_offset < 0 or patch_offset + 4 != len(data):
        raise CompatibilityError(
            f"CRC correction must occupy the final four bytes of "
            f"{len(data)}-byte input, not offset {patch_offset}"
        )

    base = bytearray(data)
    base[patch_offset : patch_offset + 4] = b"\0\0\0\0"
    prefix_crc = crc32(base[:patch_offset])
    zero_patch = b"\0\0\0\0"
    base_crc = binascii.crc32(zero_patch, prefix_crc) & 0xFFFFFFFF
    delta = desired_crc ^ base_crc

    columns: list[int] = []
    for bit in range(32):
        probe = bytearray(4)
        probe[bit // 8] = 1 << (bit % 8)
        columns.append(
            (binascii.crc32(probe, prefix_crc) & 0xFFFFFFFF) ^ base_crc
        )

    rows: list[tuple[int, int]] = []
    for output_bit in range(32):
        mask = 0
        for column, value in enumerate(columns):
            if (value >> output_bit) & 1:
                mask |= 1 << column
        rows.append((mask, (delta >> output_bit) & 1))

    rank = 0
    for column in range(32):
        pivot = next(
            (
                row
                for row in range(rank, 32)
                if (rows[row][0] >> column) & 1
            ),
            None,
        )
        if pivot is None:
            continue
        rows[rank], rows[pivot] = rows[pivot], rows[rank]
        pivot_mask, pivot_rhs = rows[rank]
        for row in range(32):
            if row != rank and ((rows[row][0] >> column) & 1):
                rows[row] = (
                    rows[row][0] ^ pivot_mask,
                    rows[row][1] ^ pivot_rhs,
                )
        rank += 1

    if rank != 32:
        raise CompatibilityError("CRC correction matrix is singular")

    patch_value = 0
    for mask, rhs in rows:
        if mask and rhs:
            patch_value |= mask & -mask

    patched = bytearray(base)
    patched[patch_offset : patch_offset + 4] = patch_value.to_bytes(4, "little")
    actual_crc = crc32(patched)
    if actual_crc != desired_crc:
        raise CompatibilityError(
            f"CRC correction failed: expected {desired_crc:08x}, "
            f"found {actual_crc:08x}"
        )
    return bytes(patched)


def _verify_uniform_padding(
    data: bytes,
    *,
    start: int,
    padding_byte: int,
    label: str,
    purpose: str,
) -> None:
    if start >= len(data):
        return
    first_live = next(
        (
            offset
            for offset in range(start, len(data))
            if data[offset] != padding_byte
        ),
        None,
    )
    if first_live is not None:
        raise CompatibilityError(
            f"{label} cannot {purpose}: byte {first_live:#x} is "
            f"{data[first_live]:#04x}, expected {padding_byte:#04x} padding"
        )


def correct_crc_in_padding(
    data: bytes,
    desired_crc: int,
    *,
    padding_byte: int,
    label: str,
) -> bytes:
    """Correct only the final four bytes of a verified padding tail."""

    if len(data) < MINIMUM_PADDING_RUN:
        raise CompatibilityError(
            f"{label} is too small for a safe CRC correction"
        )
    run_start = len(data) - MINIMUM_PADDING_RUN
    _verify_uniform_padding(
        data,
        start=run_start,
        padding_byte=padding_byte,
        label=label,
        purpose="reserve its CRC correction tail",
    )

    patch_offset = len(data) - 4
    patched = _force_crc32(data, desired_crc, patch_offset)
    if patched[:patch_offset] != data[:patch_offset]:
        raise CompatibilityError(f"{label} CRC correction changed live data")
    return patched


def convert_regions(native_regions: Mapping[str, bytes]) -> dict[str, bytes]:
    """Convert full native regions into exact puzzledp driver entries."""

    output: dict[str, bytes] = {}
    for compatibility in PUZZLEDP_REGIONS:
        native = NATIVE_BY_PART[compatibility.part]
        if compatibility.part not in native_regions:
            raise CompatibilityError(f"missing native {native.filename}")
        source = bytes(native_regions[compatibility.part])
        if len(source) != native.size:
            raise CompatibilityError(
                f"{native.filename} is {len(source)} bytes; expected {native.size}"
            )
        if compatibility.size > len(source):
            raise CompatibilityError(
                f"{compatibility.filename} requires {compatibility.size} bytes, "
                f"but {native.filename} has {len(source)}"
            )

        _verify_uniform_padding(
            source,
            start=compatibility.size,
            padding_byte=native.padding_byte,
            label=native.filename,
            purpose=f"omit bytes above {compatibility.size:#x}",
        )
        retained = source[: compatibility.size]
        output[compatibility.filename] = correct_crc_in_padding(
            retained,
            compatibility.crc32,
            padding_byte=native.padding_byte,
            label=compatibility.filename,
        )
    return output


def read_native_regions(rom_dir: Path) -> dict[str, bytes]:
    native_regions: dict[str, bytes] = {}
    for region in NATIVE_REGIONS:
        path = rom_dir / region.filename
        if not path.is_file():
            raise CompatibilityError(f"missing native ROM region: {path}")
        native_regions[region.part] = path.read_bytes()
    return native_regions


def validate_entries(entries: Mapping[str, bytes]) -> None:
    expected_names = {region.filename for region in PUZZLEDP_REGIONS}
    actual_names = set(entries)
    if actual_names != expected_names:
        missing = sorted(expected_names - actual_names)
        extra = sorted(actual_names - expected_names)
        details = []
        if missing:
            details.append(f"missing {', '.join(missing)}")
        if extra:
            details.append(f"unexpected {', '.join(extra)}")
        raise CompatibilityError("; ".join(details))

    for region in PUZZLEDP_REGIONS:
        data = bytes(entries[region.filename])
        if len(data) != region.size:
            raise CompatibilityError(
                f"{region.filename} is {len(data)} bytes; expected {region.size}"
            )
        actual_crc = crc32(data)
        if actual_crc != region.crc32:
            raise CompatibilityError(
                f"{region.filename} CRC is {actual_crc:08x}; "
                f"expected {region.crc32:08x}"
            )


def write_archive(output: Path, entries: Mapping[str, bytes]) -> None:
    validate_entries(entries)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        for name in sorted(entries):
            info = zipfile.ZipInfo(name, FIXED_ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(
                info,
                entries[name],
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9,
            )


def validate_archive(path: Path) -> None:
    try:
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise CompatibilityError(f"{path} contains duplicate filenames")
            entries = {name: archive.read(name) for name in names}
    except (OSError, zipfile.BadZipFile) as error:
        raise CompatibilityError(f"cannot read {path}: {error}") from error
    validate_entries(entries)


def build_archive(rom_dir: Path, output: Path) -> None:
    entries = convert_regions(read_native_regions(rom_dir))
    write_archive(output, entries)
    validate_archive(output)


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rom-dir",
        required=True,
        type=Path,
        help="directory containing the full native SMBNeo P/S/M/V/C regions",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="output archive (default: ROM_DIR/puzzledp.zip)",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    rom_dir = args.rom_dir.resolve()
    output = (
        args.output.resolve()
        if args.output is not None
        else rom_dir / "puzzledp.zip"
    )
    try:
        build_archive(rom_dir, output)
    except CompatibilityError as error:
        print(f"Puzzle De Pon compatibility build failed: {error}", file=sys.stderr)
        return 1

    print(f"Built Puzzle De Pon compatibility cartridge: {output}")
    for region in PUZZLEDP_REGIONS:
        print(
            f"  {region.filename}: {region.size} bytes, "
            f"CRC32 {region.crc32:08x}, {region.load_semantics}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
