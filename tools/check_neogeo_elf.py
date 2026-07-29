#!/usr/bin/env python3
"""Fail if the Neo Geo ELF violates the port's architecture or RAM budget."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess


NEOGEO_USER_RAM = 0xF300
NEOGEO_USER_RAM_START = 0x100000
NEOGEO_USER_RAM_END = NEOGEO_USER_RAM_START + NEOGEO_USER_RAM
STATIC_RAM_GUARD = 48 * 1024
EXPECTED_NGH_ID = 0x2026
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
STARTUP_SYMBOLS = {
    "__bss_end",
    "__bss_start",
    "__bss_start_in_ram",
    "__data_end",
    "__data_start",
    "__data_start_in_ram",
}
MEMORY_CARD_SYMBOL_PREFIXES = ("bios_card", "ng_memory_card")
MEMORY_CARD_CONTROL_ADDRESSES = {
    0x3A0005,  # unlock 1
    0x3A0007,  # lock 2
    0x3A0009,  # register select
    0x3A0015,  # lock 1
    0x3A0017,  # unlock 2
    0x3A0019,  # normal mode
    0x380011,  # card bank
}
MEMORY_CARD_BIOS_CALLS = {0xC00468, 0xC0046E}
MEMORY_CARD_BIOS_RAM_START = 0x10FDC4
MEMORY_CARD_BIOS_RAM_END = 0x10FDD2
MEMORY_CARD_WINDOW_START = 0x800000
MEMORY_CARD_WINDOW_END = 0xC00000
LSPC_REGISTER_ADDRESSES = (0x3C0000, 0x3C0002, 0x3C0004)
LSPC_REGISTER_NAMES = {
    0x3C0000: "address",
    0x3C0002: "data",
    0x3C0004: "modifier",
}


class ElfCheckError(ValueError):
    """The linked cartridge violates a hardware-safety invariant."""


def parse_nm_symbols(output: str) -> dict[str, int]:
    """Return address-bearing GNU nm symbols, rejecting conflicting values."""

    symbols: dict[str, int] = {}
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]+)\s+([a-zA-Z?])\s+(\S+)\s*$"
    )
    for line in output.splitlines():
        match = pattern.match(line)
        if match is None:
            continue
        value = int(match.group(1), 16)
        name = match.group(3)
        previous = symbols.get(name)
        if previous is not None and previous != value:
            raise ElfCheckError(
                f"nm reports conflicting values for {name}: "
                f"${previous:x} and ${value:x}"
            )
        symbols[name] = value
    return symbols


def ngdevkit_bss_clear_size(size: int) -> int:
    """Bytes touched by ngdevkit's 32-byte unrolled BSS clear loop."""

    if size < 0:
        raise ElfCheckError("BSS size cannot be negative")
    return ((size // 32) + 1) * 32


def ngdevkit_data_copy_size(size: int) -> int:
    """Bytes touched by ngdevkit's DBF-based data-copy loops."""

    if size < 0:
        raise ElfCheckError("data size cannot be negative")
    return size


def validate_startup_layout(
    symbols: dict[str, int],
    *,
    ram_end: int = NEOGEO_USER_RAM_END,
) -> tuple[int, int, int, int]:
    """Validate the exact RAM envelopes touched by ngdevkit startup.

    ngdevkit's BSS loop always executes one more 32-byte unrolled iteration.
    Its initialized-data loops copy the nominal data size exactly. The linker
    places initialized data immediately after BSS, so the data image must
    cover every byte that the BSS loop clears past BSS.
    """

    missing = sorted(STARTUP_SYMBOLS - symbols.keys())
    if missing:
        raise ElfCheckError(
            "startup layout symbols missing: " + ", ".join(missing)
        )

    bss_size = symbols["__bss_end"] - symbols["__bss_start"]
    data_size = symbols["__data_end"] - symbols["__data_start"]
    if bss_size < 0:
        raise ElfCheckError("__bss_end precedes __bss_start")
    if data_size < 0:
        raise ElfCheckError("__data_end precedes __data_start")

    bss_start = symbols["__bss_start_in_ram"]
    data_start = symbols["__data_start_in_ram"]
    if bss_start != NEOGEO_USER_RAM_START:
        raise ElfCheckError(
            "BSS does not start at cartridge work RAM: "
            f"expected ${NEOGEO_USER_RAM_START:x}, got ${bss_start:x}"
        )
    nominal_bss_end = bss_start + bss_size
    if data_start != nominal_bss_end:
        raise ElfCheckError(
            "initialized data is not immediately after BSS: "
            f"expected ${nominal_bss_end:x}, got ${data_start:x}"
        )

    bss_clear_end = bss_start + ngdevkit_bss_clear_size(bss_size)
    data_copy_end = data_start + ngdevkit_data_copy_size(data_size)
    if bss_clear_end > ram_end:
        raise ElfCheckError(
            "ngdevkit BSS clear crosses the safe work-RAM limit: "
            f"${bss_clear_end:x} > ${ram_end:x}"
        )
    if data_copy_end > ram_end:
        raise ElfCheckError(
            "ngdevkit data copy crosses the safe work-RAM limit: "
            f"${data_copy_end:x} > ${ram_end:x}"
        )
    if data_copy_end < bss_clear_end:
        raise ElfCheckError(
            "initialized data does not restore the ngdevkit BSS over-clear: "
            f"clear ends at ${bss_clear_end:x}, copy ends at ${data_copy_end:x}"
        )

    return bss_size, data_size, bss_clear_end, data_copy_end


def is_packed_bcd(value: int, *, digits: int = 4) -> bool:
    if value < 0 or value >= (1 << (digits * 4)):
        return False
    return all(((value >> shift) & 0xF) <= 9 for shift in range(0, digits * 4, 4))


def validate_ngh_id(
    symbols: dict[str, int],
    *,
    expected: int = EXPECTED_NGH_ID,
) -> int:
    value = symbols.get("rom_NGH_ID")
    if value is None:
        raise ElfCheckError("linked ELF is missing rom_NGH_ID")
    if not is_packed_bcd(value):
        raise ElfCheckError(
            f"rom_NGH_ID ${value:04x} is not four-digit packed BCD"
        )
    if value != expected:
        raise ElfCheckError(
            f"rom_NGH_ID is ${value:04x}; expected ${expected:04x}"
        )
    return value


def validate_title_data_alignment(symbols: dict[str, int]) -> int:
    """Require a word-aligned title payload for swabbed P-ROM patching."""

    name = "neogeo_title_screen_data"
    address = symbols.get(name)
    if address is None:
        raise ElfCheckError(f"required title data symbol missing: {name}")
    if address & 1:
        raise ElfCheckError(
            f"title data symbol has odd P-ROM address: {address:#x}"
        )
    return address


def _parse_objdump_instruction(line: str) -> tuple[str, str] | None:
    match = re.match(
        r"^\s*[0-9a-fA-F]+:\s+"
        r"(?:[0-9a-fA-F]{4}\s+)+"
        r"([a-zA-Z][a-zA-Z0-9.]*)\s*(.*?)\s*$",
        line,
    )
    if match is None:
        return None
    return match.group(1).lower(), match.group(2).lower()


def _operand_constant(operand: str) -> int | None:
    """Parse the leading absolute/immediate value printed by GNU objdump."""

    text = operand.strip()
    if text.startswith("#"):
        text = text[1:].lstrip()
    match = re.match(r"(?:\$|0x)?([0-9a-f]+)(?:\s|<|$)", text)
    if match is None:
        return None
    token = match.group(1)
    if token.isdigit() and len(token) > 6:
        return int(token, 10)
    return int(token, 16)


def validate_lspc_vram_stores(
    disassembly: str,
    *,
    require_all_register_stores: bool = False,
) -> tuple[int, int, int]:
    """Reject unsafe indirect address, data, or modifier register writes.

    Absolute-long writes are both auditable and safe for the renderer's
    explicit post-write pacing.  This lightweight data-flow check catches the
    compiler regression that previously collapsed two indirect ``move.w``
    stores into an eight-cycle pair.
    """

    known_address_registers: dict[str, int] = {}
    checked_stores = {address: 0 for address in LSPC_REGISTER_ADDRESSES}

    for line_number, line in enumerate(disassembly.splitlines(), 1):
        if re.match(r"^\s*[0-9a-fA-F]+\s+<[^>]+>:\s*$", line):
            known_address_registers.clear()
            continue
        instruction = _parse_objdump_instruction(line)
        if instruction is None:
            continue
        mnemonic, operands_text = instruction
        operands = [part.strip() for part in operands_text.split(",")]

        if mnemonic in {"jsr", "bsr", "bsrs", "bsrw", "bsrl"}:
            known_address_registers.pop("a0", None)
            known_address_registers.pop("a1", None)

        if mnemonic.startswith("move") and len(operands) >= 2:
            destination = operands[-1]
            for register, address in known_address_registers.items():
                if address in LSPC_REGISTER_ADDRESSES and re.match(
                    rf"%?{register}@", destination
                ):
                    raise ElfCheckError(
                        "unsafe indirect LSPC register write at objdump line "
                        f"{line_number}: {line.strip()}"
                    )
            destination_address = _operand_constant(destination)
            if destination_address is not None and (
                0x3C0000 <= destination_address <= 0x3C0005
            ):
                if (
                    destination_address not in LSPC_REGISTER_ADDRESSES or
                    mnemonic not in {"movew", "move.w"}
                ):
                    raise ElfCheckError(
                        "non-word or odd-address LSPC register write at "
                        f"objdump line {line_number}: {line.strip()}"
                    )
                checked_stores[destination_address] += 1

        destination_register: str | None = None
        if operands:
            match = re.fullmatch(r"%?(a[0-7])", operands[-1])
            if match is not None:
                destination_register = match.group(1)
        if destination_register is None:
            continue

        value = None
        if mnemonic.startswith("lea") or mnemonic.startswith("move"):
            if len(operands) >= 2:
                value = _operand_constant(operands[0])
        if value is None:
            known_address_registers.pop(destination_register, None)
        else:
            known_address_registers[destination_register] = value

    if require_all_register_stores:
        missing = [
            LSPC_REGISTER_NAMES[address]
            for address in LSPC_REGISTER_ADDRESSES
            if checked_stores[address] == 0
        ]
    else:
        missing = []
    if missing:
        raise ElfCheckError(
            "LSPC register audit found no absolute-long word writes for: "
            + ", ".join(missing)
        )
    return tuple(checked_stores[address] for address in LSPC_REGISTER_ADDRESSES)


def validate_palette_reference_init(disassembly: str) -> None:
    """Require both physical palette banks' word zero to become $8000."""

    expected = ("bank1", "black", "bank0", "black")
    matched = 0
    for line in disassembly.splitlines():
        instruction = _parse_objdump_instruction(line)
        if instruction is None:
            continue
        mnemonic, operands_text = instruction
        operands = [part.strip() for part in operands_text.split(",")]
        if len(operands) < 2:
            continue
        source = operands[0]
        destination = _operand_constant(operands[-1])
        event: str | None = None
        if mnemonic in {"moveb", "move.b"} and source in {"#1", "#0001"}:
            if destination == 0x3A000F:
                event = "bank1"
            elif destination == 0x3A001F:
                event = "bank0"
        elif (
            mnemonic in {"movew", "move.w"}
            and source in {"#-32768", "#8000", "#0x8000"}
            and destination == 0x400000
        ):
            event = "black"
        if event == expected[matched]:
            matched += 1
            if matched == len(expected):
                return
        elif event == expected[0]:
            matched = 1
    raise ElfCheckError(
        "palette initialization does not set word zero to $8000 in both banks"
    )


def validate_no_memory_card_access(
    symbol_names: set[str],
    disassembly: str,
) -> None:
    """Reject linked memory-card APIs and direct card-related writes."""

    forbidden_symbols = sorted(
        name
        for name in symbol_names
        if name.startswith(MEMORY_CARD_SYMBOL_PREFIXES)
    )
    if forbidden_symbols:
        raise ElfCheckError(
            "memory-card API linked: " + ", ".join(forbidden_symbols)
        )

    write_prefixes = ("move", "clr", "st", "sf", "tas")
    for line_number, line in enumerate(disassembly.splitlines(), 1):
        instruction = _parse_objdump_instruction(line)
        if instruction is None:
            continue
        mnemonic, operands_text = instruction
        operands = [part.strip() for part in operands_text.split(",")]
        constants = [
            value
            for value in (_operand_constant(operand) for operand in operands)
            if value is not None
        ]
        if any(value in MEMORY_CARD_BIOS_CALLS for value in constants):
            raise ElfCheckError(
                "memory-card BIOS call referenced at objdump line "
                f"{line_number}: {line.strip()}"
            )
        if not mnemonic.startswith(write_prefixes) or not operands:
            continue
        destination = _operand_constant(operands[-1])
        if destination is None:
            continue
        if (
            destination in MEMORY_CARD_CONTROL_ADDRESSES
            or MEMORY_CARD_BIOS_RAM_START <= destination <
                MEMORY_CARD_BIOS_RAM_END
            or MEMORY_CARD_WINDOW_START <= destination <
                MEMORY_CARD_WINDOW_END
        ):
            raise ElfCheckError(
                "memory-card write referenced at objdump line "
                f"{line_number}: {line.strip()}"
            )


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
    try:
        symbol_values = parse_nm_symbols(symbols)
    except ElfCheckError as error:
        raise SystemExit(str(error)) from error
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

    try:
        startup = validate_startup_layout(symbol_values)
        ngh_id = validate_ngh_id(symbol_values)
        title_data_address = validate_title_data_alignment(symbol_values)
        disassembly = run(
            [f"{args.prefix}objdump", "-d", str(args.elf)]
        )
        lspc_store_counts = validate_lspc_vram_stores(
            disassembly,
            require_all_register_stores=True,
        )
        validate_palette_reference_init(disassembly)
        validate_no_memory_card_access(present_symbols, disassembly)
    except ElfCheckError as error:
        raise SystemExit(str(error)) from error

    symbol_bss_size, symbol_data_size, bss_clear_end, data_copy_end = startup
    if symbol_bss_size != bss_size or symbol_data_size != data_size:
        raise SystemExit(
            "section sizes disagree with startup linker symbols: "
            f"BSS {bss_size}/{symbol_bss_size}, data {data_size}/{symbol_data_size}"
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
    print(
        "Startup RAM writes: "
        f"BSS clear ends at ${bss_clear_end:06x}, "
        f"data restore ends at ${data_copy_end:06x}"
    )
    print(f"Cartridge identity: packed-BCD NGH ${ngh_id:04x}")
    print(f"Title data: word-aligned P-ROM address {title_data_address:#x}")
    print(
        "LSPC register safety: address/data/modifier writes are word-sized "
        "absolute-long "
        f"({lspc_store_counts[0]}/{lspc_store_counts[1]}/"
        f"{lspc_store_counts[2]} writes inspected)"
    )
    print("Palette reference: word zero is $8000 in both physical banks")
    print(
        "Memory-card safety: no API or direct BIOS/control/raw write linked"
    )
    print("Forbidden framebuffer, CHR-copy, desktop, and float symbols: none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
