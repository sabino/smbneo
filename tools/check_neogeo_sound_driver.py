#!/usr/bin/env python3
"""Fail if the Neo Geo Z80 sound-driver map violates its ROM/RAM budget."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


Z80_FIXED_ROM_LIMIT = 0x8000
Z80_DATA_ORIGIN = 0xF800
Z80_STACK_START = 0xFFFD
MINIMUM_STACK_HEADROOM = 64

AREA_RE = re.compile(
    r"^(?P<name>CODE|DATA)\s+"
    r"(?P<address>[0-9A-Fa-f]{8})\s+"
    r"(?P<size>[0-9A-Fa-f]{8})\s+="
)


class SoundMapError(ValueError):
    """Raised when linker-map evidence is absent or inconsistent."""


def parse_areas(text: str) -> dict[str, tuple[int, int]]:
    found: dict[str, set[tuple[int, int]]] = {
        "CODE": set(),
        "DATA": set(),
    }
    for line in text.splitlines():
        match = AREA_RE.match(line)
        if match is None:
            continue
        found[match.group("name")].add(
            (
                int(match.group("address"), 16),
                int(match.group("size"), 16),
            )
        )

    result: dict[str, tuple[int, int]] = {}
    for name, values in found.items():
        if not values:
            raise SoundMapError(f"map is missing the {name} area")
        if len(values) != 1:
            rendered = ", ".join(
                f"${address:04x}+${size:x}"
                for address, size in sorted(values)
            )
            raise SoundMapError(
                f"map reports inconsistent {name} areas: {rendered}"
            )
        result[name] = next(iter(values))
    return result


def validate_areas(
    areas: dict[str, tuple[int, int]],
) -> tuple[int, int, int]:
    code_address, code_size = areas["CODE"]
    data_address, data_size = areas["DATA"]

    if code_address != 0:
        raise SoundMapError(
            f"Z80 CODE starts at ${code_address:04x}; expected $0000"
        )
    if code_size == 0 or code_size > Z80_FIXED_ROM_LIMIT:
        raise SoundMapError(
            f"Z80 fixed CODE size ${code_size:x} exceeds "
            f"${Z80_FIXED_ROM_LIMIT:x}"
        )
    if data_address != Z80_DATA_ORIGIN:
        raise SoundMapError(
            f"Z80 DATA starts at ${data_address:04x}; "
            f"expected ${Z80_DATA_ORIGIN:04x}"
        )

    data_end = data_address + data_size
    if data_end > Z80_STACK_START:
        raise SoundMapError(
            f"Z80 DATA ends at ${data_end:04x}, beyond stack "
            f"${Z80_STACK_START:04x}"
        )
    headroom = Z80_STACK_START - data_end
    if headroom < MINIMUM_STACK_HEADROOM:
        raise SoundMapError(
            f"Z80 DATA leaves only {headroom} stack bytes; "
            f"guard requires {MINIMUM_STACK_HEADROOM}"
        )
    return code_size, data_size, headroom


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=Path)
    args = parser.parse_args()

    if not args.map.is_file():
        raise SystemExit(f"sound-driver map does not exist: {args.map}")
    try:
        code_size, data_size, headroom = validate_areas(
            parse_areas(args.map.read_text(errors="strict"))
        )
    except (OSError, UnicodeError, SoundMapError) as error:
        raise SystemExit(f"sound-driver map check failed: {error}") from error

    print(
        f"Z80 sound driver: code={code_size:,} bytes "
        f"data={data_size:,} bytes stack_headroom={headroom:,} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
