#!/usr/bin/env python3
"""Fail if the Neo Geo ELF violates the port's architecture or RAM budget."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess


NEOGEO_USER_RAM = 0xF300
STATIC_RAM_GUARD = 48 * 1024
FORBIDDEN_SYMBOLS = {
    "audio_buffer",
    "bg_cached_tiles",
    "chr_rom",
    "frame",
    "opaque_bg_mask",
    "web_audio_buffer",
}
FORBIDDEN_FRAGMENTS = (
    "raylib",
    "__addsf3",
    "__divsf3",
    "__mulsf3",
    "__subsf3",
)
REQUIRED_SYMBOLS = {
    "data",
    "main",
    "nametable",
    "ram",
}
MINIMUM_TRANSLATED_CORE_SIZE = 64 * 1024


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return completed.stdout


def parse_size(output: str) -> tuple[int, int, int]:
    lines = [line.split() for line in output.splitlines() if line.strip()]
    for fields in reversed(lines):
        if len(fields) >= 6 and all(value.isdigit() for value in fields[:4]):
            return int(fields[0]), int(fields[1]), int(fields[2])
    raise ValueError(f"could not parse size output:\n{output}")


def parse_sections(output: str) -> dict[str, int]:
    sections: dict[str, int] = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[1].isdigit():
            sections[fields[0]] = int(fields[1])
    return sections


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument(
        "--prefix",
        default="m68k-neogeo-elf-",
        help="binutils executable prefix",
    )
    args = parser.parse_args()

    if not args.elf.is_file():
        raise SystemExit(f"ELF does not exist: {args.elf}")

    size_output = run([f"{args.prefix}size", str(args.elf)])
    text_size, _aggregate_data, _aggregate_bss = parse_size(size_output)
    sections = parse_sections(
        run([f"{args.prefix}size", "-A", str(args.elf)])
    )
    data_size = sections.get(".data", 0)
    bss_size = sections.get(".bss", 0)
    static_ram = data_size + bss_size
    ram_headroom = NEOGEO_USER_RAM - static_ram

    header = run([f"{args.prefix}readelf", "-h", str(args.elf)])
    if "MC68000" not in header and "Motorola 68000" not in header:
        raise SystemExit("ELF architecture check failed: target is not MC68000")

    symbols = run([f"{args.prefix}nm", "-a", str(args.elf)])
    present_symbols = {
        line.split()[-1]
        for line in symbols.splitlines()
        if len(line.split()) >= 2
    }
    forbidden = sorted(FORBIDDEN_SYMBOLS & present_symbols)
    forbidden.extend(
        fragment
        for fragment in FORBIDDEN_FRAGMENTS
        if fragment in symbols
    )
    if forbidden:
        raise SystemExit(
            "forbidden desktop/framebuffer/float symbols linked: "
            + ", ".join(forbidden)
        )

    missing = sorted(REQUIRED_SYMBOLS - present_symbols)
    if missing:
        raise SystemExit(
            "translated core reachability check failed; missing symbols: "
            + ", ".join(missing)
        )
    if text_size < MINIMUM_TRANSLATED_CORE_SIZE:
        raise SystemExit(
            "translated core reachability check failed: "
            f"ROM image is only {text_size} bytes"
        )
    if static_ram > STATIC_RAM_GUARD:
        raise SystemExit(
            f"static RAM guard failed: {static_ram} > {STATIC_RAM_GUARD} bytes"
        )
    if ram_headroom <= 0:
        raise SystemExit(
            f"Neo Geo work RAM overflow: static image is {static_ram} bytes"
        )

    print(f"MC68000 ROM image: text+rodata={text_size:,} data={data_size:,}")
    print(
        f"Static work RAM: {static_ram:,} bytes "
        f"(BSS={bss_size:,}, guard={STATIC_RAM_GUARD:,})"
    )
    print(
        f"Stack/heap headroom below $10F300: {ram_headroom:,} bytes "
        f"of {NEOGEO_USER_RAM:,}"
    )
    print(
        "Translated core reachability: "
        f"{', '.join(sorted(REQUIRED_SYMBOLS))} retained"
    )
    print("Forbidden framebuffer, CHR-copy, desktop, and float symbols: none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
