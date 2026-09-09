#!/usr/bin/env python3
"""Extract the VS course manifest from verified PRG/CHR/debug artifacts.

The profile contains metadata only: pointers, lengths, and fingerprints.  It
never copies ROM or source bytes into the generated files.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

PRG_SIZE = 0x8000
CHR_HIGH_BANK_SIZE = 0x2000
COURSE_SUB_INDICES = ((0, 2, 3, 4), (0, 2, 3, 4), (0, 1, 2, 3),
                      (0, 2, 3, 4), (0, 1, 2, 3), (0, 1, 2, 3),
                      (0, 2, 3, 4), (0, 1, 2, 3))
DEFAULT_PRG_SHA256 = "6549391393081a5d51f9e8fb8db0a6751a530dca55c8017269ea23bdb7a9eb13"
DEFAULT_DEBUG_SHA256 = "690438d07c5447d6cba0579a09aecd8f1a6dcc887d4faf9dea29473f5c44f90f"
DEFAULT_LBL_SHA256 = "da49ff53ee2b873084a70055576099a1c4beaa727778b1ef5d8beede8e19d08d"
EXPECTED_WORLD_OFFSETS = (0x00, 0x05, 0x0A, 0x0E, 0x13, 0x17, 0x1B, 0x20)
EXPECTED_DESCRIPTORS = (
    0x25, 0xC0, 0x36, 0x66, 0x28, 0x03, 0x37, 0x62,
    0x24, 0x35, 0x20, 0x63, 0x22, 0x41, 0x2C, 0x67,
    0x2A, 0x31, 0x2D, 0x61, 0x2E, 0x23, 0x26, 0x60,
    0x33, 0x01, 0x27, 0x64, 0x30, 0x32, 0x21, 0x65,
)


def labels(text: str) -> dict[str, int]:
    """Parse ld65 label lines and reject conflicting duplicate names."""
    result: dict[str, int] = {}
    for line in text.splitlines():
        match = re.match(r"al\s+([0-9a-fA-F]+)\s+\.([^\s]+)", line)
        if not match:
            continue
        name, address = match.group(2), int(match.group(1), 16)
        if name in result and result[name] != address:
            raise ValueError(f"conflicting VS label {name!r}")
        result[name] = address
    return result


def _u8(prg: bytes | bytearray, address: int) -> int:
    if not 0x8000 <= address < 0x10000:
        raise ValueError(f"PRG table address outside ROM: {address:#x}")
    return prg[address - 0x8000]


def _u16(prg: bytes | bytearray, address: int) -> int:
    if not 0x8000 <= address <= 0xfffe:
        raise ValueError(f"PRG word address outside ROM: {address:#x}")
    return _u8(prg, address) | (_u8(prg, address + 1) << 8)


def _table_u16(prg: bytes | bytearray, table: int, index: int,
               end: int) -> int:
    """Read a table word only when both bytes lie inside its known span."""
    address = table + 2 * index
    if not 0x8000 <= table < end <= 0x10000 or address + 1 >= end:
        raise ValueError(f"course pointer index outside table: {address:#x}")
    return _u16(prg, address)


def _stream(chr_data: bytes | bytearray, pointer: int, terminator: int,
            name: str) -> dict[str, int | str]:
    """Return bounded stream metadata, stopping at its first terminator."""
    if not 0 <= pointer < len(chr_data):
        raise ValueError(f"{name} pointer outside CHR bank: {pointer:#x}")
    end = chr_data.find(bytes((terminator,)), pointer)
    if end < pointer:
        raise ValueError(f"unterminated {name} stream at {pointer:#x}")
    length = end - pointer + 1
    if length <= 0 or pointer + length > len(chr_data):
        raise ValueError(f"{name} stream exceeds CHR bank at {pointer:#x}")
    data = bytes(chr_data[pointer:pointer + length])
    return {"offset": pointer, "length": length, "terminator": terminator,
            "sha256": hashlib.sha256(data).hexdigest()}


def extract_profile(
    prg: bytes,
    chr_data: bytes,
    debug_text: str,
    lbl_text: str,
    expected_prg_sha256: str | None = DEFAULT_PRG_SHA256,
    expected_debug_sha256: str | None = DEFAULT_DEBUG_SHA256,
    expected_lbl_sha256: str | None = DEFAULT_LBL_SHA256,
) -> dict[str, object]:
    prg_sha = hashlib.sha256(prg).hexdigest()
    debug_sha = hashlib.sha256(debug_text.encode()).hexdigest()
    lbl_sha = hashlib.sha256(lbl_text.encode()).hexdigest()
    if len(prg) != PRG_SIZE:
        raise ValueError("VS PRG must be exactly 32768 bytes")
    if len(chr_data) != CHR_HIGH_BANK_SIZE:
        raise ValueError("VS high CHR bank must be exactly 8192 bytes")
    if expected_prg_sha256 and prg_sha != expected_prg_sha256:
        raise ValueError("VS PRG fingerprint mismatch")
    if expected_debug_sha256 and debug_sha != expected_debug_sha256:
        raise ValueError("VS debug fingerprint mismatch")
    if expected_lbl_sha256 and lbl_sha != expected_lbl_sha256:
        raise ValueError("VS label fingerprint mismatch")
    lab = labels(lbl_text)
    required = ("courses_world_offsets", "courses_area_offsets", "course_actor_pages",
                "course_actor_addr_lo", "course_scenery_pages", "course_scenery_addr_lo")
    if any(name not in lab for name in required):
        raise ValueError("debug label file lacks authoritative course pointer tables")
    world_table = lab["courses_world_offsets"]
    area_table = lab["courses_area_offsets"]
    actor_page_table = lab["course_actor_pages"]
    actor_pointer_table = lab["course_actor_addr_lo"]
    scenery_page_table = lab["course_scenery_pages"]
    scenery_pointer_table = lab["course_scenery_addr_lo"]
    worlds = [_u8(prg, world_table + i) for i in range(8)]
    if tuple(worlds) != EXPECTED_WORLD_OFFSETS:
        raise ValueError("unexpected VS world-offset table")
    area_base = lab["courses_area_offsets"]
    actor_pages = [_u8(prg, actor_page_table + i) for i in range(4)]
    scenery_pages = [_u8(prg, scenery_page_table + i) for i in range(4)]
    # The next authoritative table/label bounds each pointer table.  Keeping
    # these spans explicit prevents a corrupt descriptor from indexing into
    # unrelated PRG data while preserving the synthetic ROM-free test setup.
    actor_table_end = scenery_pointer_table
    scenery_table_end = lab.get("sys_game", 0x10000)
    streams = {
        "actor": (actor_pointer_table, actor_pages, 0xff, actor_table_end),
        "scenery": (scenery_pointer_table, scenery_pages, 0xf0,
                    scenery_table_end),
    }
    rows = []
    for world in range(8):
        for round_no in range(4):
            sub_index = COURSE_SUB_INDICES[world][round_no]
            descriptor = _u8(prg, area_base + worlds[world] + sub_index)
            kind = (descriptor & 0x60) >> 5
            page = descriptor & 0x1f
            row = {"course_id": world * 4 + round_no, "world": world + 1,
                   "round": round_no + 1, "descriptor": descriptor,
                   "type": kind, "page": page}
            for name, (table, pages, terminator, table_end) in streams.items():
                pointer_index = pages[kind] + page
                pointer = _table_u16(prg, table, pointer_index, table_end)
                row[name] = _stream(chr_data, pointer, terminator, name)
            rows.append(row)
    descriptors = tuple(row["descriptor"] for row in rows)
    if expected_prg_sha256 == DEFAULT_PRG_SHA256 and descriptors != EXPECTED_DESCRIPTORS:
        raise ValueError("unexpected VS 32-course descriptor mapping")
    return {
        "format": "smbneo-vs-native-profile-4",
        "title": "Super Mario Bros. Neo",
        "prg_sha256": prg_sha,
        "debug_sha256": debug_sha,
        "lbl_sha256": lbl_sha,
        "chr_sha256": hashlib.sha256(chr_data).hexdigest(),
        "course_count": 32,
        "courses": rows,
        "provenance": {
            "mapping": "courses_world_offsets+courses_area_offsets",
            "streams": "course_*_pages/address_lo plus format terminators",
        },
    }


def emit(profile: dict[str, object], output: Path) -> None:
    """Emit stable JSON and a metadata-only C interface."""
    courses = profile.get("courses")
    if not isinstance(courses, list) or len(courses) != 32:
        raise ValueError("profile must contain exactly 32 courses")
    output.mkdir(parents=True, exist_ok=True)
    (output / "vs_native_profile.json").write_text(
        json.dumps(profile, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    rows = ",\n".join(
        f"    {{{x['course_id']}u, {x['world']}u, {x['round']}u, "
        f"0x{x['descriptor']:02x}u, 0x{x['actor']['offset']:04x}u, "
        f"{x['actor']['length']}u, 0x{x['scenery']['offset']:04x}u, "
        f"{x['scenery']['length']}u}}" for x in courses)
    header = """#ifndef SMBNEO_VS_NATIVE_PROFILE_H
#define SMBNEO_VS_NATIVE_PROFILE_H
#include <stdint.h>
#define VS_NATIVE_COURSE_COUNT 32u
typedef struct {
    uint8_t id, world, round, descriptor;
    uint16_t actor_offset, actor_length;
    uint16_t scenery_offset, scenery_length;
} VsNativeCourse;
extern const VsNativeCourse vs_native_courses[VS_NATIVE_COURSE_COUNT];
#endif
"""
    (output / "vs_native_profile.h").write_text(header, encoding="utf-8")
    source = """/* Metadata only; no ROM bytes. */
#include "vs_native_profile.h"
const VsNativeCourse vs_native_courses[VS_NATIVE_COURSE_COUNT] = {
""" + rows + "\n};\n"
    (output / "vs_native_profile.c").write_text(source, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prg", type=Path, required=True)
    parser.add_argument("--chr", type=Path, required=True)
    parser.add_argument("--debug", type=Path, required=True)
    parser.add_argument("--lbl", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-prg-sha256", default=DEFAULT_PRG_SHA256)
    parser.add_argument("--expected-debug-sha256", default=DEFAULT_DEBUG_SHA256)
    parser.add_argument("--expected-lbl-sha256", default=DEFAULT_LBL_SHA256)
    args = parser.parse_args()
    profile = extract_profile(
        args.prg.read_bytes(), args.chr.read_bytes(),
        args.debug.read_text(encoding="utf-8"),
        args.lbl.read_text(encoding="utf-8"),
        args.expected_prg_sha256, args.expected_debug_sha256,
        args.expected_lbl_sha256)
    emit(profile, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
