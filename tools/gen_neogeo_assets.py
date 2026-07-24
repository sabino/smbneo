#!/usr/bin/env python3
"""Convert a user-supplied SMB iNES dump into Neo Geo cartridge assets.

The ROM is read in memory (including when it is inside a ZIP archive), and
only converted CHR graphics plus the CHR-resident title nametable payload are
written. PRG data is never copied to output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zipfile


EXPECTED_ROM_SHA1 = "ea343f4e445a9050d4b4fbac2c77d0693b1d0922"
NES_CHR_SIZE = 8 * 1024
NES_CHR_TILES = 512
TITLE_SCREEN_CHR_OFFSET = 0x1EC0
TITLE_SCREEN_CHR_SIZE = 0x013A
TITLE_SCREEN_SOURCE = "smbneogeo-title.c"

CROM_TILE_BYTES_PER_CHIP = 64
CROM_RESERVED_TILES = 256
CROM_BLANK_TILE = 256
CROM_NES_TILE_BASE = 257
CROM_CHIP_SIZE = 2 * 1024 * 1024

SROM_TILE_BYTES = 32
SROM_BLANK_TILE = 512
SROM_SOLID_TILE = 513
SROM_SIZE = 128 * 1024


class AssetError(ValueError):
    """A source ROM or generated-asset validation error."""


def load_rom(path: Path) -> tuple[bytes, str]:
    """Read a raw .nes file or the sole .nes member of a ZIP, without extraction."""

    if not path.is_file():
        raise AssetError(f"ROM input does not exist: {path}")

    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            members = [
                info
                for info in archive.infolist()
                if not info.is_dir() and info.filename.lower().endswith(".nes")
            ]
            if len(members) != 1:
                raise AssetError(
                    f"expected exactly one .nes member in {path}, found {len(members)}"
                )
            member = members[0]
            return archive.read(member), f"{path.name}:{member.filename}"

    return path.read_bytes(), path.name


def extract_chr(rom: bytes) -> bytes:
    """Validate the mapper-0 SMB iNES layout and return its 8 KiB CHR bank."""

    if len(rom) < 16 or rom[:4] != b"NES\x1a":
        raise AssetError("input is not an iNES ROM")

    prg_banks = rom[4]
    chr_banks = rom[5]
    flags6 = rom[6]
    flags7 = rom[7]
    mapper = (flags6 >> 4) | (flags7 & 0xF0)

    if (flags7 & 0x0C) == 0x08:
        raise AssetError("NES 2.0 input is not supported")
    if mapper != 0:
        raise AssetError(f"expected mapper 0, found mapper {mapper}")
    if prg_banks != 2 or chr_banks != 1:
        raise AssetError(
            f"expected 2x16 KiB PRG and 1x8 KiB CHR, found "
            f"{prg_banks} PRG and {chr_banks} CHR banks"
        )
    if (flags6 & 1) == 0:
        raise AssetError("expected the vertically mirrored SMB cartridge")

    trainer_size = 512 if (flags6 & 0x04) else 0
    chr_offset = 16 + trainer_size + prg_banks * 16 * 1024
    chr_end = chr_offset + chr_banks * NES_CHR_SIZE
    if len(rom) < chr_end:
        raise AssetError(
            f"truncated ROM: need at least {chr_end} bytes, found {len(rom)}"
        )
    return rom[chr_offset:chr_end]


def decode_nes_tile(chr_data: bytes, tile_index: int) -> bytes:
    """Decode one NES 8x8 2bpp CHR tile to one byte per pixel."""

    if not 0 <= tile_index < NES_CHR_TILES:
        raise AssetError(f"NES tile index out of range: {tile_index}")

    offset = tile_index * 16
    pixels = bytearray(64)
    for y in range(8):
        plane0 = chr_data[offset + y]
        plane1 = chr_data[offset + y + 8]
        for x in range(8):
            bit = 7 - x
            pixels[y * 8 + x] = (
                ((plane1 >> bit) & 1) << 1
            ) | ((plane0 >> bit) & 1)
    return bytes(pixels)


def expand_2x(tile8: bytes) -> bytes:
    """Expand an 8x8 tile to 16x16 so Neo Geo shrink hardware yields 8x8."""

    if len(tile8) != 64:
        raise AssetError(f"expected 64 pixels, found {len(tile8)}")

    tile16 = bytearray(256)
    for y in range(8):
        for x in range(8):
            color = tile8[y * 8 + x]
            output = (y * 2) * 16 + x * 2
            tile16[output] = color
            tile16[output + 1] = color
            tile16[output + 16] = color
            tile16[output + 17] = color
    return bytes(tile16)


def encode_crom_tile(tile: bytes) -> tuple[bytes, bytes]:
    """Encode one 16x16 4bpp tile into complementary C1/C2 bitplanes."""

    if len(tile) != 256:
        raise AssetError(f"expected 256 pixels, found {len(tile)}")

    crom1 = bytearray(CROM_TILE_BYTES_PER_CHIP)
    crom2 = bytearray(CROM_TILE_BYTES_PER_CHIP)
    position = 0

    # Neo Geo stores the four 8x8 quadrants in this hardware order.
    for quadrant_offset in (8, 136, 0, 128):
        offset = quadrant_offset
        for _y in range(8):
            planes = [0, 0, 0, 0]
            for x in range(8):
                color = tile[offset]
                for plane in range(4):
                    planes[plane] |= ((color >> plane) & 1) << x
                offset += 1
            crom1[position : position + 2] = bytes(planes[:2])
            crom2[position : position + 2] = bytes(planes[2:])
            position += 2
            offset += 8
    return bytes(crom1), bytes(crom2)


def encode_srom_tile(tile: bytes) -> bytes:
    """Encode one 8x8 4bpp tile in Neo Geo FIX-layer order."""

    if len(tile) != 64:
        raise AssetError(f"expected 64 pixels, found {len(tile)}")

    output = bytearray(SROM_TILE_BYTES)
    position = 0
    for pixel_a, pixel_b in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            a = tile[y * 8 + pixel_a] & 0x0F
            b = (tile[y * 8 + pixel_b] & 0x0F) << 4
            output[position] = a | b
            position += 1
    return bytes(output)


def pad_file(path: Path, content: bytes, size: int) -> None:
    if len(content) > size:
        raise AssetError(
            f"{path.name} content ({len(content)} bytes) exceeds {size}-byte ROM"
        )

    with path.open("wb") as output:
        output.write(content)
        remaining = size - len(content)
        zeroes = bytes(64 * 1024)
        while remaining:
            chunk = min(remaining, len(zeroes))
            output.write(zeroes[:chunk])
            remaining -= chunk


def title_screen_data(chr_data: bytes) -> bytes:
    """Return the nametable payload the game reads from unused CHR space."""

    if len(chr_data) != NES_CHR_SIZE:
        raise AssetError(f"expected {NES_CHR_SIZE} CHR bytes, found {len(chr_data)}")

    end = TITLE_SCREEN_CHR_OFFSET + TITLE_SCREEN_CHR_SIZE
    return chr_data[TITLE_SCREEN_CHR_OFFSET:end]


def format_title_screen_source(data: bytes) -> str:
    """Build a deterministic C translation unit for the P-ROM title payload."""

    if len(data) != TITLE_SCREEN_CHR_SIZE:
        raise AssetError(
            f"expected {TITLE_SCREEN_CHR_SIZE} title bytes, found {len(data)}"
        )

    lines = [
        '#include "title_data.h"',
        "",
        "const uint8_t neogeo_title_screen_data[TITLE_SCREEN_CHR_SIZE] = {",
    ]
    for offset in range(0, len(data), 12):
        values = ", ".join(f"0x{value:02x}" for value in data[offset : offset + 12])
        lines.append(f"    {values},")
    lines.extend(("};", ""))
    return "\n".join(lines)


def build_assets(chr_data: bytes, output_dir: Path) -> dict[str, int | str]:
    if len(chr_data) != NES_CHR_SIZE:
        raise AssetError(f"expected {NES_CHR_SIZE} CHR bytes, found {len(chr_data)}")

    crom1 = bytearray(CROM_NES_TILE_BASE * CROM_TILE_BYTES_PER_CHIP)
    crom2 = bytearray(CROM_NES_TILE_BASE * CROM_TILE_BYTES_PER_CHIP)
    srom = bytearray()

    for tile_index in range(NES_CHR_TILES):
        tile8 = decode_nes_tile(chr_data, tile_index)
        tile_c1, tile_c2 = encode_crom_tile(expand_2x(tile8))
        crom1.extend(tile_c1)
        crom2.extend(tile_c2)
        srom.extend(encode_srom_tile(tile8))

    # FIX tile 512 is transparent; tile 513 is an opaque border mask.
    srom.extend(encode_srom_tile(bytes(64)))
    srom.extend(encode_srom_tile(bytes([1]) * 64))

    output_dir.mkdir(parents=True, exist_ok=True)
    title_data = title_screen_data(chr_data)
    pad_file(output_dir / "smbneogeo-c1.c1", bytes(crom1), CROM_CHIP_SIZE)
    pad_file(output_dir / "smbneogeo-c2.c2", bytes(crom2), CROM_CHIP_SIZE)
    pad_file(output_dir / "smbneogeo-s1.s1", bytes(srom), SROM_SIZE)
    (output_dir / TITLE_SCREEN_SOURCE).write_text(
        format_title_screen_source(title_data),
        encoding="ascii",
    )

    return {
        "crom_blank_tile": CROM_BLANK_TILE,
        "crom_nes_tile_base": CROM_NES_TILE_BASE,
        "crom_tiles_generated": CROM_NES_TILE_BASE + NES_CHR_TILES,
        "srom_blank_tile": SROM_BLANK_TILE,
        "srom_solid_tile": SROM_SOLID_TILE,
        "srom_tiles_generated": SROM_SOLID_TILE + 1,
        "title_screen_chr_bytes": len(title_data),
        "title_screen_chr_sha256": hashlib.sha256(title_data).hexdigest(),
    }


def generate(input_path: Path, output_dir: Path, allow_unverified: bool) -> dict:
    rom, source = load_rom(input_path)
    rom_sha1 = hashlib.sha1(rom).hexdigest()

    if rom_sha1 != EXPECTED_ROM_SHA1 and not allow_unverified:
        raise AssetError(
            "ROM SHA-1 does not match the supported Super Mario Bros. (World) "
            f"revision: expected {EXPECTED_ROM_SHA1}, found {rom_sha1}"
        )

    chr_data = extract_chr(rom)
    tile_info = build_assets(chr_data, output_dir)
    manifest = {
        "source": source,
        "source_sha1": rom_sha1,
        "expected_sha1": EXPECTED_ROM_SHA1,
        "verified_revision": rom_sha1 == EXPECTED_ROM_SHA1,
        "prg_bytes_written": 0,
        "chr_bytes_read": len(chr_data),
        **tile_info,
    }
    (output_dir / "asset-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        required=True,
        type=Path,
        help="user-supplied SMB .nes file or ZIP containing one .nes file",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
        help="ignored build directory for generated cartridge assets",
    )
    parser.add_argument(
        "--allow-unverified",
        action="store_true",
        help="accept another mapper-0 32K+8K ROM revision (development only)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = generate(args.input, args.output_dir, args.allow_unverified)
    except (AssetError, OSError, zipfile.BadZipFile) as error:
        raise SystemExit(f"asset generation failed: {error}") from error

    print(
        "Generated Neo Geo graphics and title data from verified SMB CHR "
        f"({manifest['source_sha1']}); no PRG bytes were written."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
