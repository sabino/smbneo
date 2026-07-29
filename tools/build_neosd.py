#!/usr/bin/env python3
"""Build and validate SMBNeo images in the NeoSD .neo container format."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import sys
from typing import Mapping, Sequence


HEADER_SIZE = 4096
MAGIC = b"NEO\x01"
ALIGN_64K = 64 * 1024
ALIGN_CROM = 256 * 1024
PADDING_BYTE = 0xFF

PRODUCT_NAME = "Super Mario Bros. Neo"
MANUFACTURER = "Community port"
YEAR = 2026
GENRE_PLATFORMER = 5
SCREENSHOT = 0
NGH = 0x2026

REGION_FILENAMES = {
    "p": "smbneo-p1.p1",
    "s": "smbneo-s1.s1",
    "m": "smbneo-m1.m1",
    "v1": "smbneo-v1.v1",
    "c1": "smbneo-c1.c1",
    "c2": "smbneo-c2.c2",
}

REGION_SIZES = {
    "p": 0x100000,
    "s": 0x020000,
    "m": 0x020000,
    "v1": 0x080000,
    "c1": 0x200000,
    "c2": 0x200000,
}


class NeoSdError(RuntimeError):
    """The input cartridge or generated NeoSD image is invalid."""


@dataclass(frozen=True)
class NeoSdHeader:
    p_size: int
    s_size: int
    m_size: int
    v1_size: int
    v2_size: int
    c_size: int
    year: int
    genre: int
    screenshot: int
    ngh: int
    name: str
    manufacturer: str

    @property
    def payload_size(self) -> int:
        return (
            self.p_size
            + self.s_size
            + self.m_size
            + self.v1_size
            + self.v2_size
            + self.c_size
        )


def _as_bytes(value: bytes | bytearray | memoryview, label: str) -> bytes:
    try:
        return bytes(value)
    except (TypeError, ValueError) as error:
        raise NeoSdError(f"{label} is not byte data") from error


def _uint32(value: int, label: str) -> int:
    if type(value) is not int or value < 0 or value > 0xFFFFFFFF:
        raise NeoSdError(f"{label} must be an unsigned 32-bit integer")
    return value


def validate_packed_bcd_ngh(value: int) -> int:
    """Return a valid four-digit packed-BCD Neo Geo game number."""

    ngh = _uint32(value, "NGH")
    if ngh > 0xFFFF or any(((ngh >> shift) & 0xF) > 9 for shift in (0, 4, 8, 12)):
        raise NeoSdError(
            f"NGH 0x{ngh:x} must be a four-digit packed-BCD value"
        )
    return ngh


def _text_field(value: str, width: int, label: str) -> bytes:
    if not isinstance(value, str):
        raise NeoSdError(f"{label} must be text")
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError as error:
        raise NeoSdError(f"{label} must contain ASCII characters only") from error
    if b"\0" in encoded:
        raise NeoSdError(f"{label} must not contain a NUL byte")
    limit = width - 1
    if len(encoded) > limit:
        raise NeoSdError(
            f"{label} is {len(encoded)} bytes; the NeoSD limit is {limit}"
        )
    return encoded + bytes(width - len(encoded))


def _decode_text_field(data: bytes, label: str) -> str:
    encoded = data.split(b"\0", 1)[0]
    try:
        return encoded.decode("ascii")
    except UnicodeDecodeError as error:
        raise NeoSdError(f"NeoSD {label} field is not ASCII") from error


def _pad_region(data: bytes, alignment: int) -> bytes:
    remainder = len(data) % alignment
    if remainder == 0:
        return data
    return data + bytes((PADDING_BYTE,)) * (alignment - remainder)


def interleave_crom(c1_input: bytes, c2_input: bytes) -> bytes:
    """Merge physical C1/C2 chip bytes into the NeoSD C payload."""

    c1 = _as_bytes(c1_input, "C1 region")
    c2 = _as_bytes(c2_input, "C2 region")
    if len(c1) != len(c2):
        raise NeoSdError(
            f"C1 and C2 sizes differ: {len(c1)} and {len(c2)} bytes"
        )
    if not c1:
        raise NeoSdError("C1 and C2 regions must not be empty")

    output = bytearray(len(c1) + len(c2))
    output[0::2] = c1
    output[1::2] = c2
    return bytes(output)


def build_image(
    regions: Mapping[str, bytes | bytearray | memoryview],
    *,
    name: str = PRODUCT_NAME,
    manufacturer: str = MANUFACTURER,
    year: int = YEAR,
    genre: int = GENRE_PLATFORMER,
    screenshot: int = SCREENSHOT,
    ngh: int = NGH,
) -> bytes:
    """Create a deterministic NeoSD image from native P/S/M/V/C regions."""

    required = set(REGION_FILENAMES)
    missing = required - set(regions)
    extra = set(regions) - required
    if missing or extra:
        details = []
        if missing:
            details.append(f"missing {', '.join(sorted(missing))}")
        if extra:
            details.append(f"unexpected {', '.join(sorted(extra))}")
        raise NeoSdError("invalid native region set: " + "; ".join(details))

    p = _pad_region(_as_bytes(regions["p"], "P region"), ALIGN_64K)
    s = _pad_region(_as_bytes(regions["s"], "S region"), ALIGN_64K)
    m = _pad_region(_as_bytes(regions["m"], "M region"), ALIGN_64K)
    v1 = _pad_region(_as_bytes(regions["v1"], "V1 region"), ALIGN_64K)
    v2 = b""
    c = _pad_region(
        interleave_crom(regions["c1"], regions["c2"]),
        ALIGN_CROM,
    )

    sizes = (len(p), len(s), len(m), len(v1), len(v2), len(c))
    for value, label in zip(
        sizes,
        ("P size", "S size", "M size", "V1 size", "V2 size", "C size"),
    ):
        _uint32(value, label)

    header = bytearray(HEADER_SIZE)
    header[:4] = MAGIC
    struct.pack_into("<6I", header, 0x04, *sizes)
    struct.pack_into(
        "<4I",
        header,
        0x1C,
        _uint32(year, "year"),
        _uint32(genre, "genre"),
        _uint32(screenshot, "screenshot"),
        validate_packed_bcd_ngh(ngh),
    )
    header[0x2C:0x4D] = _text_field(name, 33, "game name")
    header[0x4D:0x5E] = _text_field(manufacturer, 17, "manufacturer")

    return b"".join((header, p, s, m, v1, v2, c))


def parse_header(image_input: bytes | bytearray | memoryview) -> NeoSdHeader:
    image = _as_bytes(image_input, "NeoSD image")
    if len(image) < HEADER_SIZE:
        raise NeoSdError(
            f"NeoSD image is {len(image)} bytes; header requires {HEADER_SIZE}"
        )
    if image[:4] != MAGIC:
        raise NeoSdError("NeoSD image does not start with NEO version 1 magic")

    sizes = struct.unpack_from("<6I", image, 0x04)
    year, genre, screenshot, ngh = struct.unpack_from("<4I", image, 0x1C)
    validate_packed_bcd_ngh(ngh)
    return NeoSdHeader(
        *sizes,
        year,
        genre,
        screenshot,
        ngh,
        _decode_text_field(image[0x2C:0x4D], "game name"),
        _decode_text_field(image[0x4D:0x5E], "manufacturer"),
    )


def section_offsets(header: NeoSdHeader) -> dict[str, tuple[int, int]]:
    offset = HEADER_SIZE
    output = {}
    for name, size in (
        ("p", header.p_size),
        ("s", header.s_size),
        ("m", header.m_size),
        ("v1", header.v1_size),
        ("v2", header.v2_size),
        ("c", header.c_size),
    ):
        output[name] = (offset, offset + size)
        offset += size
    return output


def validate_image(
    image_input: bytes | bytearray | memoryview,
    regions: Mapping[str, bytes | bytearray | memoryview] | None = None,
    *,
    name: str = PRODUCT_NAME,
    manufacturer: str = MANUFACTURER,
    year: int = YEAR,
    genre: int = GENRE_PLATFORMER,
    screenshot: int = SCREENSHOT,
    ngh: int = NGH,
) -> NeoSdHeader:
    """Validate SMBNeo metadata, structure, and optional native-region semantics."""

    image = _as_bytes(image_input, "NeoSD image")
    header = parse_header(image)
    expected_length = HEADER_SIZE + header.payload_size
    if len(image) != expected_length:
        raise NeoSdError(
            f"NeoSD image has {len(image)} bytes; header describes {expected_length}"
        )
    if any(image[0x5E:HEADER_SIZE]):
        raise NeoSdError("NeoSD reserved header bytes are not zero-filled")

    expected_metadata = (
        ("game name", header.name, name),
        ("manufacturer", header.manufacturer, manufacturer),
        ("year", header.year, _uint32(year, "year")),
        ("genre", header.genre, _uint32(genre, "genre")),
        (
            "screenshot",
            header.screenshot,
            _uint32(screenshot, "screenshot"),
        ),
        ("NGH", header.ngh, validate_packed_bcd_ngh(ngh)),
    )
    for label, actual, wanted in expected_metadata:
        if actual != wanted:
            raise NeoSdError(
                f"NeoSD {label} is {actual!r}; expected {wanted!r}"
            )

    if regions is None:
        return header

    expected = build_image(
        regions,
        name=name,
        manufacturer=manufacturer,
        year=year,
        genre=genre,
        screenshot=screenshot,
        ngh=ngh,
    )
    if image != expected:
        first_difference = next(
            index
            for index, (actual, wanted) in enumerate(zip(image, expected))
            if actual != wanted
        )
        raise NeoSdError(
            "NeoSD payload does not match the native cartridge at "
            f"offset 0x{first_difference:x}"
        )
    return header


def load_native_regions(rom_dir: Path) -> dict[str, bytes]:
    output = {}
    for region, filename in REGION_FILENAMES.items():
        path = rom_dir / filename
        try:
            data = path.read_bytes()
        except OSError as error:
            raise NeoSdError(f"cannot read {region.upper()} region {path}: {error}") from error
        expected_size = REGION_SIZES[region]
        if len(data) != expected_size:
            raise NeoSdError(
                f"{filename} is {len(data)} bytes; expected {expected_size}"
            )
        output[region] = data
    return output


def validate_file(
    path: Path,
    rom_dir: Path | None = None,
    *,
    name: str = PRODUCT_NAME,
    manufacturer: str = MANUFACTURER,
    year: int = YEAR,
    genre: int = GENRE_PLATFORMER,
    screenshot: int = SCREENSHOT,
    ngh: int = NGH,
) -> NeoSdHeader:
    try:
        image = path.read_bytes()
    except OSError as error:
        raise NeoSdError(f"cannot read NeoSD image {path}: {error}") from error
    regions = load_native_regions(rom_dir) if rom_dir is not None else None
    return validate_image(
        image,
        regions,
        name=name,
        manufacturer=manufacturer,
        year=year,
        genre=genre,
        screenshot=screenshot,
        ngh=ngh,
    )


def write_image(
    rom_dir: Path,
    output: Path,
    *,
    name: str = PRODUCT_NAME,
    manufacturer: str = MANUFACTURER,
    year: int = YEAR,
    genre: int = GENRE_PLATFORMER,
    screenshot: int = SCREENSHOT,
    ngh: int = NGH,
) -> NeoSdHeader:
    regions = load_native_regions(rom_dir)
    image = build_image(
        regions,
        name=name,
        manufacturer=manufacturer,
        year=year,
        genre=genre,
        screenshot=screenshot,
        ngh=ngh,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        output.write_bytes(image)
    except OSError as error:
        raise NeoSdError(f"cannot write NeoSD image {output}: {error}") from error
    return validate_file(
        output,
        rom_dir,
        name=name,
        manufacturer=manufacturer,
        year=year,
        genre=genre,
        screenshot=screenshot,
        ngh=ngh,
    )


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom-dir", type=Path, required=True)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--output", type=Path)
    action.add_argument(
        "--validate",
        type=Path,
        help="validate an existing image against the native regions",
    )
    parser.add_argument("--name", default=PRODUCT_NAME)
    parser.add_argument("--manufacturer", default=MANUFACTURER)
    parser.add_argument("--year", type=int, default=YEAR)
    parser.add_argument("--genre", type=int, default=GENRE_PLATFORMER)
    parser.add_argument("--screenshot", type=int, default=SCREENSHOT)
    parser.add_argument("--ngh", type=lambda value: int(value, 0), default=NGH)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.validate is not None:
            header = validate_file(
                args.validate,
                args.rom_dir,
                name=args.name,
                manufacturer=args.manufacturer,
                year=args.year,
                genre=args.genre,
                screenshot=args.screenshot,
                ngh=args.ngh,
            )
            output = args.validate
        else:
            header = write_image(
                args.rom_dir,
                args.output,
                name=args.name,
                manufacturer=args.manufacturer,
                year=args.year,
                genre=args.genre,
                screenshot=args.screenshot,
                ngh=args.ngh,
            )
            output = args.output
    except NeoSdError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"NeoSD image: {output}")
    print(
        "Regions: "
        f"P={header.p_size} S={header.s_size} M={header.m_size} "
        f"V1={header.v1_size} V2={header.v2_size} C={header.c_size}"
    )
    print(f"Title: {header.name} ({header.year})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
