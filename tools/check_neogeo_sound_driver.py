#!/usr/bin/env python3
"""Validate the Neo Geo Z80 sound-driver protocol and ROM/RAM budget."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


Z80_FIXED_ROM_LIMIT = 0x8000
Z80_DATA_ORIGIN = 0xF800
Z80_STACK_START = 0xFFFD
MINIMUM_STACK_HEADROOM = 64
Z80_COMMAND_TABLE_ENTRIES = 128
EXPECTED_LOCAL_DATA = (
    ("apu_packet_phase", 1),
    ("apu_packet_quotient", 1),
    ("apu_previous_symbol", 1),
)
EXPECTED_READY_PING_BODY = (
    "xor a",
    "ld (apu_packet_phase), a",
    "ld (apu_previous_symbol), a",
    "ret",
)
EXPECTED_EXPLICIT_COMMANDS = (
    "snd_command_unused",
    "snd_command_01_prepare_for_rom_switch",
    "snd_command_unused",
    "snd_command_03_reset_driver",
    "apu_ready_ping",
    "apu_ready_ping",
) + ("apu_packet_byte",) * 122
EXPECTED_PACKET_BODY = (
    "sub a, #6",
    "ld e, a",
    "ld a, (apu_packet_phase)",
    "or a",
    "jr nz, apu_packet_second",
    "ld a, (apu_previous_symbol)",
    "ld c, a",
    "ld a, e",
    "sub c",
    "ret z",
    "jr nc, apu_packet_first_delta",
    "add a, #122",
    "apu_packet_first_delta:",
    "dec a",
    "cp #60",
    "ret nc",
    "ld (apu_packet_quotient), a",
    "ld a, e",
    "ld (apu_previous_symbol), a",
    "ld a, #1",
    "ld (apu_packet_phase), a",
    "ret",
    "apu_packet_second:",
    "ld d, e",
    "ld a, (apu_previous_symbol)",
    "ld c, a",
    "ld a, e",
    "sub c",
    "jr z, apu_packet_abort",
    "jr nc, apu_packet_second_delta",
    "add a, #122",
    "apu_packet_second_delta:",
    "dec a",
    "ld e, a",
    "xor a",
    "ld (apu_packet_phase), a",
    "ld a, d",
    "ld (apu_previous_symbol), a",
    "ld a, (apu_packet_quotient)",
    "ld l, a",
    "ld h, #0",
    "add hl, hl",
    "ld bc, #apu_packet_bases",
    "add hl, bc",
    "ld c, (hl)",
    "inc hl",
    "ld b, (hl)",
    "ld a, c",
    "add a, e",
    "ld c, a",
    "jr nc, apu_packet_validate",
    "inc b",
    "apu_packet_validate:",
    "ld a, b",
    "cp #0x1c",
    "ret nc",
    "call ym2610_write_port_a",
    "ret",
    "apu_packet_abort:",
    "xor a",
    "ld (apu_packet_phase), a",
    "ret",
)
EXPECTED_PACKET_BASES = tuple(index * 121 for index in range(60))
DEFAULT_DRIVER_SOURCE = (
    Path(__file__).resolve().parent.parent
    / "platform"
    / "neogeo"
    / "sound_driver.s"
)

AREA_RE = re.compile(
    r"^(?P<name>CODE|DATA)\s+"
    r"(?P<address>[0-9A-Fa-f]{8})\s+"
    r"(?P<size>[0-9A-Fa-f]{8})\s+="
)
JP_RE = re.compile(r"^jp\s+(?P<target>[A-Za-z_][A-Za-z0-9_]*)$")
REPT_RE = re.compile(r"^\.rept\s+(?P<count>[0-9]+)$")
LABEL_RE = re.compile(r"^(?P<label>[A-Za-z_][A-Za-z0-9_]*):$")
BLKB_RE = re.compile(r"^\.blkb\s+(?P<size>[0-9]+)$")


class SoundMapError(ValueError):
    """Raised when sound-driver evidence is absent or inconsistent."""


def _source_lines(text: str) -> list[str]:
    return [
        re.sub(r"\s+", " ", code)
        for line in text.splitlines()
        if (code := line.split(";", 1)[0].strip())
    ]


def parse_explicit_commands(text: str) -> tuple[str, ...]:
    lines = _source_lines(text)
    if lines.count("cmd_jmptable::") != 1:
        raise SoundMapError(
            "sound driver must define exactly one cmd_jmptable"
        )
    if lines.count("init_unused_cmd_jmptable") != 1:
        raise SoundMapError(
            "sound driver must pad exactly one command table"
        )

    start = lines.index("cmd_jmptable::") + 1
    end = lines.index("init_unused_cmd_jmptable")
    if end < start:
        raise SoundMapError(
            "command-table padding precedes cmd_jmptable"
        )

    commands: list[str] = []
    index = start
    while index < end:
        line = lines[index]
        repeat = REPT_RE.fullmatch(line)
        if repeat is not None:
            if index + 2 >= end:
                raise SoundMapError("truncated .rept in command table")
            jump = JP_RE.fullmatch(lines[index + 1])
            if jump is None or lines[index + 2] != ".endm":
                raise SoundMapError(
                    "command-table .rept must contain exactly one jp"
                )
            commands.extend(
                [jump.group("target")] * int(repeat.group("count"))
            )
            index += 3
            continue

        jump = JP_RE.fullmatch(line)
        if jump is None:
            raise SoundMapError(
                f"unsupported command-table statement: {line}"
            )
        commands.append(jump.group("target"))
        index += 1

    return tuple(commands)


def parse_handler(text: str, name: str) -> tuple[str, ...]:
    lines = _source_lines(text)
    label = f"{name}:"
    if lines.count(label) != 1:
        raise SoundMapError(
            f"sound driver must define exactly one {name}"
        )

    body: list[str] = []
    for line in lines[lines.index(label) + 1:]:
        if line.startswith(".area") or re.fullmatch(
            r"[A-Za-z_][A-Za-z0-9_]*:{1,2}",
            line,
        ) is not None:
            break
        body.append(line)
    return tuple(body)


def parse_code_region(
    text: str,
    start_label: str,
    end_label: str,
) -> tuple[str, ...]:
    lines = _source_lines(text)
    start = f"{start_label}:"
    end = f"{end_label}:"
    if lines.count(start) != 1 or lines.count(end) != 1:
        raise SoundMapError(
            f"sound driver must define exactly one {start_label} "
            f"and {end_label}"
        )
    start_index = lines.index(start) + 1
    end_index = lines.index(end)
    if end_index < start_index:
        raise SoundMapError(
            f"{end_label} precedes {start_label}"
        )
    return tuple(lines[start_index:end_index])


def parse_word_table(text: str, name: str) -> tuple[int, ...]:
    lines = _source_lines(text)
    label = f"{name}:"
    if lines.count(label) != 1:
        raise SoundMapError(
            f"sound driver must define exactly one {name}"
        )

    values: list[int] = []
    for line in lines[lines.index(label) + 1:]:
        if line.startswith(".area") or line.endswith(":"):
            break
        if not line.startswith(".dw "):
            raise SoundMapError(
                f"unsupported {name} statement: {line}"
            )
        try:
            values.extend(
                int(value.strip(), 0)
                for value in line[4:].split(",")
            )
        except ValueError as error:
            raise SoundMapError(
                f"invalid word in {name}: {line}"
            ) from error
    return tuple(values)


def parse_local_data(text: str) -> tuple[tuple[str, int], ...]:
    lines = _source_lines(text)
    data_areas = [
        index
        for index, line in enumerate(lines)
        if re.fullmatch(r"\.area\s+DATA", line) is not None
    ]
    if len(data_areas) != 1:
        raise SoundMapError(
            "sound driver must define exactly one DATA area"
        )

    entries: list[tuple[str, int]] = []
    pending_label: str | None = None
    for line in lines[data_areas[0] + 1:]:
        if line.startswith(".area"):
            break

        label = LABEL_RE.fullmatch(line)
        if label is not None:
            if pending_label is not None:
                raise SoundMapError(
                    f"DATA label {pending_label} has no allocation"
                )
            pending_label = label.group("label")
            continue

        allocation = BLKB_RE.fullmatch(line)
        if allocation is None or pending_label is None:
            raise SoundMapError(
                f"unsupported DATA statement: {line}"
            )
        entries.append(
            (pending_label, int(allocation.group("size")))
        )
        pending_label = None

    if pending_label is not None:
        raise SoundMapError(
            f"DATA label {pending_label} has no allocation"
        )
    return tuple(entries)


def validate_driver_source(text: str) -> tuple[int, int]:
    commands = parse_explicit_commands(text)
    if commands != EXPECTED_EXPLICIT_COMMANDS:
        limit = max(len(commands), len(EXPECTED_EXPLICIT_COMMANDS))
        for command in range(limit):
            actual = (
                commands[command]
                if command < len(commands)
                else "<missing>"
            )
            expected = (
                EXPECTED_EXPLICIT_COMMANDS[command]
                if command < len(EXPECTED_EXPLICIT_COMMANDS)
                else "<implicit unused padding>"
            )
            if actual != expected:
                raise SoundMapError(
                    f"command ${command:02x} dispatches to {actual}; "
                    f"expected {expected}"
                )

    ready_ping = parse_handler(text, "apu_ready_ping")
    if ready_ping != EXPECTED_READY_PING_BODY:
        raise SoundMapError(
            f"ready-ping reset is {ready_ping!r}; "
            f"expected {EXPECTED_READY_PING_BODY!r}"
        )

    packet_body = parse_code_region(
        text,
        "apu_packet_byte",
        "apu_packet_bases",
    )
    if packet_body != EXPECTED_PACKET_BODY:
        raise SoundMapError(
            f"packet decoder is {packet_body!r}; "
            f"expected {EXPECTED_PACKET_BODY!r}"
        )

    packet_bases = parse_word_table(text, "apu_packet_bases")
    if packet_bases != EXPECTED_PACKET_BASES:
        raise SoundMapError(
            f"packet base table is {packet_bases!r}; "
            f"expected {EXPECTED_PACKET_BASES!r}"
        )

    data = parse_local_data(text)
    if data != EXPECTED_LOCAL_DATA:
        raise SoundMapError(
            f"sound-driver DATA is {data!r}; "
            f"expected {EXPECTED_LOCAL_DATA!r}"
        )

    return Z80_COMMAND_TABLE_ENTRIES, sum(
        size for _label, size in data
    )


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
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_DRIVER_SOURCE,
    )
    args = parser.parse_args()

    if not args.map.is_file():
        raise SystemExit(f"sound-driver map does not exist: {args.map}")
    if not args.source.is_file():
        raise SystemExit(
            f"sound-driver source does not exist: {args.source}"
        )
    try:
        command_entries, local_data_size = validate_driver_source(
            args.source.read_text(errors="strict")
        )
        code_size, data_size, headroom = validate_areas(
            parse_areas(args.map.read_text(errors="strict"))
        )
    except (OSError, UnicodeError, SoundMapError) as error:
        raise SystemExit(f"sound-driver check failed: {error}") from error

    print(
        f"Z80 sound driver: code={code_size:,} bytes "
        f"data={data_size:,} bytes stack_headroom={headroom:,} bytes "
        f"commands={command_entries} local_data={local_data_size} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
