#!/usr/bin/env python3
"""Validate the generated custom GnGeo driver against native SMBNeo ROMs."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import struct
import sys
import zipfile
import zlib


HEADER = struct.Struct("<32s32s128sI10II")
RECORD = struct.Struct("<32sBIIII")


@dataclass(frozen=True)
class ExpectedRom:
    filename: str
    region: int
    destination: int
    size: int


class DriverError(RuntimeError):
    """The custom GnGeo driver does not describe the canonical cartridge."""


ROM_LAYOUT = (
    ("p1.p1", 8, 0, 0x100000),
    ("m1.m1", 1, 0, 0x020000),
    ("v1.v1", 3, 0, 0x080000),
    ("s1.s1", 6, 0, 0x020000),
    ("c1.c1", 9, 0, 0x200000),
    ("c2.c2", 9, 1, 0x200000),
)


def expected_roms(filename_prefix: str = "smbneo") -> tuple[ExpectedRom, ...]:
    """Return the canonical full-layout records for one cartridge prefix."""

    if not re.fullmatch(r"[a-z0-9_]{1,16}", filename_prefix):
        raise DriverError(f"invalid ROM filename prefix {filename_prefix!r}")
    return tuple(
        ExpectedRom(
            f"{filename_prefix}-{suffix}",
            region,
            destination,
            size,
        )
        for suffix, region, destination, size in ROM_LAYOUT
    )


# Kept as a public compatibility alias for the canonical Home Edition checks.
EXPECTED_ROMS = expected_roms()


def _text(field: bytes, label: str) -> str:
    value, separator, padding = field.partition(b"\0")
    if not separator or any(padding):
        raise DriverError(f"{label} is not NUL-padded")
    try:
        return value.decode("ascii")
    except UnicodeDecodeError as error:
        raise DriverError(f"{label} is not ASCII") from error


def validate_driver(
    data: bytes,
    rom_dir: Path,
    *,
    shortname: str = "smbneo",
    title: str = "Super Mario Bros. Neo",
    filename_prefix: str | None = None,
) -> None:
    if not re.fullmatch(r"[a-z0-9_]{1,16}", shortname):
        raise DriverError(f"invalid driver shortname {shortname!r}")
    expected_records = expected_roms(filename_prefix or shortname)

    if len(data) < HEADER.size:
        raise DriverError("driver is shorter than its header")

    unpacked = HEADER.unpack_from(data)
    name = _text(unpacked[0], "driver shortname")
    parent = _text(unpacked[1], "driver parent")
    long_name = _text(unpacked[2], "driver title")
    year = unpacked[3]
    region_sizes = unpacked[4:14]
    record_count = unpacked[14]

    if name != shortname:
        raise DriverError(f"driver shortname is {name!r}, expected {shortname!r}")
    if parent != "neogeo":
        raise DriverError(f"driver parent is {parent!r}, expected 'neogeo'")
    if long_name != title:
        raise DriverError(f"driver title is {long_name!r}, expected {title!r}")
    if year != 2026:
        raise DriverError(f"driver year is {year}, expected 2026")
    if record_count != len(expected_records):
        raise DriverError(
            f"driver has {record_count} ROM records, "
            f"expected {len(expected_records)}"
        )

    expected_length = HEADER.size + record_count * RECORD.size
    if len(data) != expected_length:
        raise DriverError(
            f"driver has {len(data)} bytes, expected {expected_length}"
        )

    expected_region_sizes = [0] * 10
    for rom in expected_records:
        expected_region_sizes[rom.region] += rom.size
    if tuple(region_sizes) != tuple(expected_region_sizes):
        raise DriverError("driver region-size table does not match the full cartridge")

    offset = HEADER.size
    for expected in expected_records:
        filename_field, region, flags, destination, size, crc = (
            RECORD.unpack_from(data, offset)
        )
        offset += RECORD.size
        filename = _text(filename_field, "ROM filename")
        actual_path = rom_dir / expected.filename
        if not actual_path.is_file():
            raise DriverError(f"native ROM is missing: {actual_path}")
        actual = actual_path.read_bytes()
        actual_crc = zlib.crc32(actual) & 0xFFFFFFFF

        actual_record = (filename, region, flags, destination, size, crc)
        expected_record = (
            expected.filename,
            expected.region,
            0,
            expected.destination,
            expected.size,
            actual_crc,
        )
        if actual_record != expected_record:
            raise DriverError(
                f"driver record for {expected.filename} differs from the "
                "native filename, region, destination, size, or CRC"
            )
        if len(actual) != expected.size:
            raise DriverError(
                f"{actual_path} has {len(actual)} bytes, expected {expected.size}"
            )


def validate_archive(
    data_zip: Path,
    rom_dir: Path,
    *,
    shortname: str = "smbneo",
    title: str = "Super Mario Bros. Neo",
    filename_prefix: str | None = None,
) -> None:
    if not re.fullmatch(r"[a-z0-9_]{1,16}", shortname):
        raise DriverError(f"invalid driver shortname {shortname!r}")
    driver_name = f"rom/{shortname}.drv"
    try:
        with zipfile.ZipFile(data_zip) as archive:
            driver_entries = sorted(
                name
                for name in archive.namelist()
                if name.startswith("rom/") and name.endswith(".drv")
            )
            if driver_entries != [driver_name]:
                raise DriverError(
                    f"{data_zip} has GnGeo drivers {driver_entries!r}; "
                    f"expected only {driver_name!r}"
                )
            data = archive.read(driver_name)
    except (OSError, KeyError, zipfile.BadZipFile) as error:
        raise DriverError(f"cannot read {data_zip}: {error}") from error

    validate_driver(
        data,
        rom_dir,
        shortname=shortname,
        title=title,
        filename_prefix=filename_prefix,
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--rom-dir", type=Path, required=True)
    parser.add_argument("--shortname", default="smbneo")
    parser.add_argument("--title", default="Super Mario Bros. Neo")
    parser.add_argument(
        "--filename-prefix",
        help="ROM filename prefix (defaults to the driver shortname)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        validate_archive(
            args.data,
            args.rom_dir,
            shortname=args.shortname,
            title=args.title,
            filename_prefix=args.filename_prefix,
        )
    except DriverError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"Verified GnGeo custom driver {args.shortname!r}: "
        f"{args.data}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
