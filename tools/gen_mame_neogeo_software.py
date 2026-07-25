#!/usr/bin/env python3
"""Generate a local MAME software list for the locally built cartridge."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET
import zlib


GAME_NAME = "smbneogeo"
ROM_PARTS = (
    ("maincpu", "smbneogeo-p1.p1", 0x100000, 0x000000, "load16_word_swap"),
    ("fixed", "smbneogeo-s1.s1", 0x020000, 0x000000, None),
    ("audiocpu", "smbneogeo-m1.m1", 0x020000, 0x000000, None),
    ("ymsnd:adpcma", "smbneogeo-v1.v1", 0x080000, 0x000000, None),
    ("sprites", "smbneogeo-c1.c1", 0x200000, 0x000000, "load16_byte"),
    ("sprites", "smbneogeo-c2.c2", 0x200000, 0x000001, "load16_byte"),
)
DATA_AREA_SIZES = {
    "maincpu": 0x100000,
    "fixed": 0x040000,
    "audiocpu": 0x040000,
    "ymsnd:adpcma": 0x080000,
    "sprites": 0x400000,
}


def file_hashes(path: Path) -> tuple[str, str]:
    data = path.read_bytes()
    return f"{zlib.crc32(data) & 0xFFFFFFFF:08x}", hashlib.sha1(data).hexdigest()


def build_software_list(rom_dir: Path) -> ET.Element:
    files: dict[str, Path] = {}
    for _, filename, expected_size, _, _ in ROM_PARTS:
        path = rom_dir / filename
        if not path.is_file():
            raise ValueError(f"missing cartridge ROM: {path}")
        actual_size = path.stat().st_size
        if actual_size != expected_size:
            raise ValueError(
                f"{path}: expected {expected_size} bytes, found {actual_size}"
            )
        files[filename] = path

    software_list = ET.Element(
        "softwarelist",
        {"name": "neogeo", "description": "Local Neo Geo cartridge tests"},
    )
    software = ET.SubElement(software_list, "software", {"name": GAME_NAME})
    ET.SubElement(software, "description").text = "SMBNeo local validation build"
    ET.SubElement(software, "year").text = "2026"
    ET.SubElement(software, "publisher").text = "Community port"
    ET.SubElement(
        software,
        "sharedfeat",
        {"name": "compatibility", "value": "MVS,AES"},
    )
    part = ET.SubElement(
        software,
        "part",
        {"name": "cart", "interface": "neo_cart"},
    )

    areas: dict[str, ET.Element] = {}
    for area_name, filename, size, offset, loadflag in ROM_PARTS:
        if area_name not in areas:
            attributes = {
                "name": area_name,
                "size": f"0x{DATA_AREA_SIZES[area_name]:06x}",
            }
            if area_name == "maincpu":
                attributes.update({"width": "16", "endianness": "big"})
            areas[area_name] = ET.SubElement(part, "dataarea", attributes)

        crc, sha1 = file_hashes(files[filename])
        attributes = {
            "name": filename,
            "offset": f"0x{offset:06x}",
            "size": f"0x{size:06x}",
            "crc": crc,
            "sha1": sha1,
        }
        if loadflag is not None:
            attributes["loadflag"] = loadflag
        ET.SubElement(areas[area_name], "rom", attributes)

    return software_list


def write_software_list(rom_dir: Path, output: Path) -> None:
    root = build_software_list(rom_dir)
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        '<?xml version="1.0"?>\n'
        '<!DOCTYPE softwarelist SYSTEM "softwarelist.dtd">\n'
        f"{body}\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rom-dir",
        type=Path,
        required=True,
        help="directory containing the six unpacked cartridge ROM regions",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="destination neogeo.xml software list",
    )
    args = parser.parse_args()

    try:
        write_software_list(args.rom_dir, args.output)
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
