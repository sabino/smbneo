#!/usr/bin/env python3
"""Prototype direct-C backend for the verified Vs. SMB graph.

This is deliberately a separate backend from ``translate_vs.py``.  It uses
the linked PRG and ca65 debug symbols as an oracle for names and direct-call
edges, but emits a small, ordinary C call graph.  It does not emit CPU
instructions or any resumable emulation machinery.

The generated file is a control-flow scaffold, not yet the game core.  Each
named routine is an explicit C function and the title/game mode boundaries
are direct calls with switch dispatch.  The scaffold is useful as a build and
ABI milestone before replacing individual bodies with semantic C.
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


REQUIRED_SYMBOLS = (
    "start",
    "nmi",
    "sys_title",
    "title_proc",
    "sys_game",
    "game_proc",
    "game_mode_proc",
)

# The ca65 build configuration is part of the oracle identity.  The source
# files themselves are never copied into generated output.
REQUIRED_DEFINES = ("VS", "VS_2C04_0004", "VS_SMB", "SMBV1")

_OP_ROWS = (
    "BRK:i ORA:ix - - - ORA:z ASL:z - PHP:i ORA:n ASL:a - - ORA:w ASL:w -",
    "BPL:r ORA:iy - - - ORA:zx ASL:zx - CLC:i ORA:wy - - - ORA:wx ASL:wx -",
    "JSR:w AND:ix - - BIT:z AND:z ROL:z - PLP:i AND:n ROL:a - BIT:w AND:w ROL:w -",
    "BMI:r AND:iy - - - AND:zx ROL:zx - SEC:i AND:wy - - - AND:wx ROL:wx -",
    "RTI:i EOR:ix - - - EOR:z LSR:z - PHA:i EOR:n LSR:a - JMP:w EOR:w LSR:w -",
    "BVC:r EOR:iy - - - EOR:zx LSR:zx - CLI:i EOR:wy - - - EOR:wx LSR:wx -",
    "RTS:i ADC:ix - - - ADC:z ROR:z - PLA:i ADC:n ROR:a - JMP:ind ADC:w ROR:w -",
    "BVS:r ADC:iy - - - ADC:zx ROR:zx - SEI:i ADC:wy - - - ADC:wx ROR:wx -",
    "- STA:ix - - STY:z STA:z STX:z - DEY:i - TXA:i - STY:w STA:w STX:w -",
    "BCC:r STA:iy - - STY:zx STA:zx STX:zy - TYA:i STA:wy TXS:i - - STA:wx - -",
    "LDY:n LDA:ix LDX:n - LDY:z LDA:z LDX:z - TAY:i LDA:n TAX:i - LDY:w LDA:w LDX:w -",
    "BCS:r LDA:iy - - LDY:zx LDA:zx LDX:zy - CLV:i LDA:wy TSX:i - LDY:wx LDA:wx LDX:wy -",
    "CPY:n CMP:ix - - CPY:z CMP:z DEC:z - INY:i CMP:n DEX:i - CPY:w CMP:w DEC:w -",
    "BNE:r CMP:iy - - - CMP:zx DEC:zx - CLD:i CMP:wy - - - CMP:wx DEC:wx -",
    "CPX:n SBC:ix - - CPX:z SBC:z INC:z - INX:i SBC:n NOP:i - CPX:w SBC:w INC:w -",
    "BEQ:r SBC:iy - - - SBC:zx INC:zx - SED:i SBC:wy - - - SBC:wx INC:wx -",
)
_OPS = [item for row in _OP_ROWS for item in row.split()]
_LENGTH = dict(i=1, a=1, n=2, z=2, zx=2, zy=2, ix=2, iy=2, r=2,
               w=3, wx=3, wy=3, ind=3)


def _fields(rest: str) -> dict[str, str]:
    return dict(re.findall(r'(\w+)=("[^"]*"|[^,]+)', rest))


def parse_symbols(debug: str) -> dict[str, int]:
    """Read exported/label symbols without retaining paths or source text."""
    symbols: dict[str, int] = {}
    for line in debug.splitlines():
        kind, _, rest = line.partition("\t")
        if kind != "sym":
            continue
        fields = _fields(rest)
        if fields.get("type") != "lab" or "name" not in fields or "val" not in fields:
            continue
        name = json.loads(fields["name"])
        value = int(fields["val"], 0)
        # A duplicate unscoped label with a different address is ambiguous;
        # reject it rather than generating a misleading call graph.
        if name in symbols and symbols[name] != value:
            raise ValueError(f"ambiguous debug symbol {name!r}")
        symbols[name] = value
    return symbols


def source_manifest(source_root: Path) -> dict[str, object]:
    """Validate the structured source checkout without copying it."""
    root = source_root.resolve()
    if not (root / "Makefile").is_file() or not (root / "src").is_dir():
        raise ValueError("source root must contain Makefile and src/")
    makefile = (root / "Makefile").read_text(encoding="latin1")
    missing = [define for define in REQUIRED_DEFINES if f"-D{define}" not in makefile]
    if missing:
        raise ValueError("source Makefile is missing VS defines: " + ", ".join(missing))
    modules = sorted(
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".s", ".i"}
    )
    if not modules:
        raise ValueError("source root contains no ca65 modules")
    return {"module_count": len(modules), "defines": list(REQUIRED_DEFINES)}


def load_prg(path: Path) -> bytes:
    prg = path.read_bytes()
    if len(prg) != 0x8000:
        raise ValueError(f"VS PRG must be exactly 32768 bytes, got {len(prg)}")
    return prg


def decode_marked(debug: str, source_root: Path, prg: bytes) -> dict[int, tuple[str, str, int]]:
    """Reuse the verified debug-span rules, without importing the VS emitter."""
    segments: dict[str, dict[str, str]] = {}
    spans: list[dict[str, str]] = []
    files: dict[str, list[str]] = {}
    source_lines: list[dict[str, str]] = []
    root = source_root.resolve()
    for line in debug.splitlines():
        kind, _, rest = line.partition("\t")
        if kind not in ("seg", "span", "file", "line"):
            continue
        fields = _fields(rest)
        if kind == "seg":
            segments[fields["id"]] = fields
        elif kind == "span":
            spans.append(fields)
        elif kind == "file":
            path = (root / json.loads(fields["name"])).resolve()
            if not path.is_relative_to(root):
                raise ValueError("debug source path escapes source root")
            files[fields["id"]] = path.read_text(encoding="latin1").splitlines()
        elif "span" in fields:
            source_lines.append(fields)
    mnemonics = {item.split(":")[0].lower() for item in _OPS if item != "-"}
    marked: dict[str, str] = {}
    for line in source_lines:
        text = files[line["file"]][int(line["line"]) - 1].split(";")[0].strip()
        text = re.sub(r"^(?:[A-Za-z_][\w]*)?:\s*", "", text)
        word = text.split()[0].lower() if text else ""
        if word in mnemonics:
            for span_id in line["span"].split("+"):
                marked[span_id] = word.upper()
    result: dict[int, tuple[str, str, int]] = {}
    for span in spans:
        segment = segments[span["seg"]]
        if segment.get("name") != '"CODE"' or "type" in span or span["id"] not in marked:
            continue
        address = int(segment["start"], 0) + int(span["start"], 0)
        size = int(span["size"], 0)
        offset = address - 0x8000
        if not 0 <= offset <= len(prg) - size:
            raise ValueError(f"invalid code span at {address:04x}")
        entry = _OPS[prg[offset]]
        if entry == "-":
            raise ValueError(f"unsupported opcode at {address:04x}")
        op, mode = entry.split(":")
        if _LENGTH[mode] != size or marked[span["id"]] != op:
            raise ValueError(f"code/debug mismatch at {address:04x}")
        operand = int.from_bytes(prg[offset + 1:offset + size], "little")
        result[address] = op, mode, operand
    if not result:
        raise ValueError("no CODE instruction spans")
    return result


def call_graph(code: dict[int, tuple[str, str, int]], symbols: dict[str, int], names: tuple[str, ...]) -> dict[str, list[str]]:
    """Recover direct calls within each named routine, stopping at named callees."""
    by_address = {address: name for name, address in symbols.items()}
    result: dict[str, list[str]] = {}
    for name in names:
        entry = symbols.get(name)
        if entry is None or entry not in code:
            continue
        pending, seen, edges = [entry], set(), set()
        while pending:
            address = pending.pop()
            if address in seen or address not in code:
                continue
            if address != entry and address in by_address:
                continue
            seen.add(address)
            op, mode, operand = code[address]
            nxt = address + _LENGTH[mode]
            if op == "JSR":
                target = by_address.get(operand)
                if target is not None:
                    edges.add(target)
                pending.append(nxt)
            elif op == "RTS" or op == "RTI":
                continue
            elif op == "JMP":
                if mode == "w":
                    pending.append(operand)
            elif mode == "r":
                delta = operand if operand < 128 else operand - 256
                pending.extend((nxt, (nxt + delta) & 0xffff))
            else:
                pending.append(nxt)
        result[name] = sorted(edges)
    return result


def _c_name(name: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", name)
    return "smbneo_native_" + value


def emit_header() -> str:
    return """/* Generated direct-C ABI. No copyrighted ROM or source data. */
#ifndef SMBNEO_NATIVE_CODEGEN_H
#define SMBNEO_NATIVE_CODEGEN_H
#include <stdint.h>

typedef struct {
    uint8_t mode;
    uint8_t title_mode;
    uint8_t game_mode;
    uint8_t player_state;
    uint32_t calls;
    uint16_t last_symbol;
} SmbNeoNativeContext;

enum { SMBNEO_NATIVE_TITLE = 0, SMBNEO_NATIVE_GAME = 1 };
void smbneo_native_boot(SmbNeoNativeContext *ctx);
void smbneo_native_frame(SmbNeoNativeContext *ctx);
void smbneo_native_title_tick(SmbNeoNativeContext *ctx);
void smbneo_native_game_tick(SmbNeoNativeContext *ctx);
#endif
"""


def emit_c(symbols: dict[str, int], graph: dict[str, list[str]]) -> str:
    """Emit a direct-call scaffold with no emulated CPU state."""
    names = tuple(name for name in REQUIRED_SYMBOLS if name in symbols)
    lines = [
        "/* Generated direct-C scaffold; semantic routine bodies are next. */",
        '#include "smbneo_native.h"',
        "",
        "#ifdef SMBNEO_NATIVE_DIAGNOSTICS",
        "#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (ctx)->calls++; (ctx)->last_symbol = (symbol); } while (0)",
        "#else",
        "#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (void)(ctx); (void)(symbol); } while (0)",
        "#endif",
        "",
    ]
    for name in names:
        lines.append(f"static void {_c_name(name)}(SmbNeoNativeContext *ctx);")
    lines += [
        "",
    ]
    for name in names:
        lines += [
            f"static void {_c_name(name)}(SmbNeoNativeContext *ctx) {{",
            f"    SMBNEO_NATIVE_MARK(ctx, 0x{symbols[name]:04x}u);",
        ]
        # The first milestone makes the top-level title/game boundaries
        # explicit. Lower routines remain stubs until their semantic bodies
        # are individually verified.
        if name == "start" and "sys_title" in symbols:
            lines.append("    smbneo_native_sys_title(ctx);")
        elif name == "sys_title" and "title_proc" in symbols:
            lines.append("    smbneo_native_title_proc(ctx);")
        elif name == "sys_game" and "game_mode_proc" in symbols:
            lines.append("    smbneo_native_game_mode_proc(ctx);")
        elif name == "game_mode_proc" and "game_proc" in symbols:
            lines.append("    smbneo_native_game_proc(ctx);")
        lines += ["}", ""]
    lines += [
        "void smbneo_native_boot(SmbNeoNativeContext *ctx) {",
        "    ctx->mode = SMBNEO_NATIVE_TITLE;",
        "    smbneo_native_start(ctx);",
        "}",
        "",
        "void smbneo_native_frame(SmbNeoNativeContext *ctx) {",
        "    smbneo_native_nmi(ctx);",
        "    switch (ctx->mode) {",
        "    case SMBNEO_NATIVE_TITLE: smbneo_native_title_tick(ctx); break;",
        "    case SMBNEO_NATIVE_GAME: smbneo_native_game_tick(ctx); break;",
        "    default: ctx->mode = SMBNEO_NATIVE_TITLE; break;",
        "    }",
        "}",
        "",
        "void smbneo_native_title_tick(SmbNeoNativeContext *ctx) {",
        "    smbneo_native_sys_title(ctx);",
        "}",
        "",
        "void smbneo_native_game_tick(SmbNeoNativeContext *ctx) {",
        "    switch (ctx->game_mode) {",
        "    case 0: smbneo_native_sys_game(ctx); break;",
        "    default: smbneo_native_sys_game(ctx); break;",
        "    }",
        "}",
        "",
    ]
    return "\n".join(lines)


def ordered_tables(debug: str, source_root: Path, prg: bytes, code: dict[int, tuple[str, str, int]], table_entry: int) -> dict[int, list[int]]:
    """Recover ordered .addr tables following calls to tbljmp.

    The order matters: duplicate destinations are legal and a set-valued
    table would silently change the selector semantics.  Only explicit ca65
    address spans are accepted; arbitrary PRG bytes are never treated as a
    table.
    """
    segments: dict[str, dict[str, str]] = {}
    spans: list[dict[str, str]] = []
    files: dict[str, list[str]] = {}
    address_spans: set[str] = set()
    root = source_root.resolve()
    for line in debug.splitlines():
        kind, _, rest = line.partition("\t")
        if kind not in ("seg", "span", "file", "line"):
            continue
        fields = _fields(rest)
        if kind == "seg":
            segments[fields["id"]] = fields
        elif kind == "span":
            spans.append(fields)
        elif kind == "file":
            path = (root / json.loads(fields["name"])).resolve()
            if not path.is_relative_to(root):
                raise ValueError("debug source path escapes source root")
            files[fields["id"]] = path.read_text(encoding="latin1").splitlines()
        elif "span" in fields:
            text = files[fields["file"]][int(fields["line"]) - 1].split(";")[0].strip()
            text = re.sub(r"^(?:[A-Za-z_][\w]*)?:\s*", "", text)
            if text.split()[:1] == [".addr"]:
                address_spans.update(fields["span"].split("+"))
    words: dict[int, int] = {}
    for span in spans:
        if span["id"] not in address_spans:
            continue
        segment = segments[span["seg"]]
        if segment.get("name") != '"CODE"':
            continue
        start = int(segment["start"], 0) + int(span["start"], 0)
        size = int(span["size"], 0)
        if size % 2 or start < 0x8000 or start + size > 0x10000:
            raise ValueError("invalid explicit address table span")
        for address in range(start, start + size, 2):
            offset = address - 0x8000
            words[address] = int.from_bytes(prg[offset:offset + 2], "little")
    tables: dict[int, list[int]] = {}
    for call, ins in code.items():
        if ins != ("JSR", "w", table_entry):
            continue
        cursor, targets = call + 3, []
        while cursor in words and len(targets) < 256:
            target = words[cursor]
            if target not in code:
                break
            targets.append(target)
            cursor += 2
        if targets:
            tables[call] = targets
    return tables


def direct_entries(code: dict[int, tuple[str, str, int]], tables: dict[int, list[int]], symbols: dict[str, int]) -> set[int]:
    entries = {value for op, mode, value in code.values() if op == "JSR" and mode == "w"}
    entries.update(target for targets in tables.values() for target in targets)
    entries.update(symbols[name] for name in ("start", "nmi") if name in symbols)
    return {entry for entry in entries if entry in code}


def direct_bodies(code: dict[int, tuple[str, str, int]], entries: set[int]) -> tuple[dict[int, set[int]], list[int]]:
    """Partition marked code into direct-C routines and orphan regions.

    JSR/table targets are hard routine boundaries.  Any marked code not
    reachable from one of those boundaries is retained as a synthetic region
    rather than being silently dropped.  This avoids splitting ordinary
    branch labels that happen to have debug symbols.
    """
    bodies: dict[int, set[int]] = {}

    def walk(entry: int) -> set[int]:
        pending, body = [entry], set()
        while pending:
            address = pending.pop()
            if address in body or address not in code:
                continue
            if address != entry and address in entries:
                continue
            body.add(address)
            op, mode, operand = code[address]
            nxt = address + _LENGTH[mode]
            if op == "JSR":
                pending.append(nxt)
            elif op in ("RTS", "RTI", "BRK"):
                continue
            elif op == "JMP" and mode == "w":
                if operand != address:
                    pending.append(operand)
            elif mode == "r":
                delta = operand if operand < 128 else operand - 256
                pending.extend((nxt, (nxt + delta) & 0xffff))
            else:
                pending.append(nxt)
        return body

    for entry in sorted(entries):
        bodies[entry] = walk(entry)
    covered = set().union(*bodies.values()) if bodies else set()
    orphans = []
    for address in sorted(set(code) - covered):
        if address in covered:
            continue
        orphans.append(address)
        entries.add(address)
        bodies[address] = walk(address)
        covered.update(bodies[address])
    return bodies, orphans


@dataclass(frozen=True)
class SavedValueLayout:
    """Compile-time layout for balanced processor save/restore pairs.

    ``depth_at`` is the proven depth immediately before each instruction.
    ``local_count`` is the number of ordinary scalar C locals needed by the
    routine. ``frame_discards`` contains terminal values discarded by the VS
    interrupt entry; a native function call has no processor-created frame.
    """

    depth_at: dict[int, int]
    local_count: int
    frame_discards: frozenset[int]


@dataclass(frozen=True)
class NativeSemanticRequirement:
    """Exact additional source body consumed by a semantic replacement."""

    entry: int
    addresses: tuple[int, ...]
    digest: str


@dataclass(frozen=True)
class NativeSemanticRoutine:
    """Fingerprint-gated replacement for one complete generated routine."""

    addresses: tuple[int, ...]
    digest: str
    function: str
    header: str
    conditional: bool = False
    requirements: tuple[NativeSemanticRequirement, ...] = ()


@dataclass(frozen=True)
class NativeSemanticSlice:
    """Fingerprint-gated replacement for a region inside one routine.

    A true-returning helper has consumed the complete slice and resumes at
    ``continuation``.  A false return is transactional and leaves the normal
    generated body authoritative.
    """

    owner: int
    start: int
    continuation: int
    addresses: tuple[int, ...]
    digest: str
    function: str
    header: str
    arguments: tuple[str, ...] = ("ctx",)
    conditional: bool = True
    requirements: tuple[NativeSemanticRequirement, ...] = ()


def _address_tuple(text: str) -> tuple[int, ...]:
    """Keep long reviewed address sets readable without weakening equality."""
    return tuple(int(value, 16) for value in text.split())


_RENDER_CHR_PAIR_BODY = NativeSemanticRequirement(
    0xeb0f,
    _address_tuple("""
        eb0f eb11 f1e7 f1e9 f1ea f1eb f1ed f1ef f1f2 f1f4 f1f7 f1f9
        f1fb f1fe f200 f203 f205 f207 f20a f20d f20f f212 f215 f217
        f21a f21b f21d f220 f222 f223 f225 f227 f228 f229 f22b f22c
        f22d f22e
    """),
    "d54c67667edbf0ed1efb0d2e74aedbd900521e433eed972dc7c0ccdf97355566",
)

_RENDER_ACTOR_PAIR_BODY = NativeSemanticRequirement(
    0xeb07,
    (0xeb07, 0xeb0a, 0xeb0c),
    "ff9162c043c5049940231483b4d793f001dc2e8e4c87bffa9a6b3f67c1f4b244",
)

_RENDER_SET_SIX_BODY = NativeSemanticRequirement(
    0xe512, (0xe512, 0xe515),
    "02bac629d76b1936014c7b75c2bc994fe043c16248c09959cffc60bbfb477e8d",
)
_RENDER_SET_FOUR_BODY = NativeSemanticRequirement(
    0xe518, (0xe518,),
    "95f366ca76fad69857a0b6d78b546b69704ba0d6c12ae1986f178647279c5cad",
)
_RENDER_SET_THREE_BODY = NativeSemanticRequirement(
    0xe51b, (0xe51b,),
    "48a005e2aea96f1ee894775d9926c6d820a3b12485fb011692c63c1934ca136a",
)
_RENDER_SET_TWO_BODY = NativeSemanticRequirement(
    0xe51e, (0xe51e, 0xe521, 0xe524),
    "29bb026601033d3bf9b080ebc4672eb8d8b617b08b1c798e2d32e058ab427d95",
)
_RENDER_CLEAR_H_BODY = NativeSemanticRequirement(
    0xeb1e, (0xeb1e, 0xeb1f, 0xeb22, 0xeb23, 0xeb26, 0xeb29),
    "75ad2026efd2e607ffe196518b1acb5f48b0dab6664d443d0dbdcb2aea8b4840",
)
_RENDER_CLEAR_TOP_BODY = NativeSemanticRequirement(
    0xeba7, (0xeba7, 0xeba9, 0xebac, 0xebaf),
    "a19a1aa287e517aa05fe50be8d8049b92161d1ef8df6bc0acdd402c030dbf751",
)

_RENDER_ACTOR_BODY = _address_tuple("""
    e7da e7dc e7de e7e1 e7e3 e7e6 e7e8 e7ea e7ed e7ef e7f1 e7f4 e7f6
    e7f8 e7fa e7fc e7fe e800 e803 e805 e806 e808 e80a e80c e80d e80f
    e811 e813 e815 e817 e819 e81b e81d e81f e821 e823 e826 e828 e82a
    e82c e82e e830 e832 e834 e836 e838 e83b e83e e840 e842 e844 e846
    e848 e84a e84c e84f e852 e854 e856 e858 e85a e85b e85d e85f e861
    e863 e865 e867 e869 e86b e86d e86f e872 e874 e876 e878 e87a e87c
    e87e e880 e883 e885 e887 e88a e88b e88d e890 e892 e894 e896 e899
    e89b e89d e89f e8a1 e8a3 e8a6 e8a9 e8ac e8ae e8b0 e8b2 e8b4 e8b6
    e8b8 e8ba e8bb e8bd e8bf e8c2 e8c4 e8c6 e8c8 e8ca e8cc e8ce e8d0
    e8d2 e8d4 e8d7 e8d9 e8db e8dd e8df e8e1 e8e4 e8e6 e8e8 e8ea e8ed
    e8ef e8f1 e8f3 e8f5 e8f7 e8f9 e8fb e8fd e8ff e901 e903 e905 e907
    e909 e90b e90d e90f e911 e913 e915 e917 e919 e91b e91d e91f e921
    e923 e925 e927 e929 e92b e92d e92f e931 e933 e935 e937 e939 e93b
    e93d e93f e942 e944 e946 e948 e94a e94c e94e e950 e952 e954 e957
    e959 e95b e95d e95f e961 e963 e965 e967 e969 e96b e96d e96f e970
    e973 e975 e977 e979 e97b e97d e97f e981 e984 e986 e988 e98a e98d
    e98f e990 e991 e993 e994 e996 e998 e99a e99c e99e e9a0 e9a2 e9a5
    e9a6 e9a8 e9aa e9ad e9b0 e9b3 e9b5 e9b8 e9ba e9bc e9be e9c1 e9c4
    e9c6 e9c9 e9cb e9cc e9cd e9d0 e9d1 e9d2 e9d3 e9d4 e9d6 e9d8 e9da
    e9dc e9de e9e0 e9e2 e9e3 e9e4 e9e6 e9e7 e9ea e9eb e9ee e9ef e9f2
    e9f5 e9f8 e9fb e9fc e9ff ea00 ea03 ea06 ea08 ea0a ea0c ea0e ea10
    ea13 ea15 ea17 ea19 ea1b ea1d ea1f ea21 ea23 ea25 ea27 ea29 ea2b
    ea2d ea30 ea32 ea34 ea37 ea39 ea3c ea3e ea41 ea44 ea47 ea49 ea4b
    ea4d ea4f ea52 ea55 ea58 ea5a ea5c ea5f ea61 ea64 ea67 ea69 ea6c
    ea6f ea71 ea73 ea75 ea78 ea7a ea7d ea7f ea82 ea85 ea87 ea8a ea8d
    ea8f ea91 ea94 ea96 ea99 ea9b ea9e eaa0 eaa3 eaa6 eaa8 eaab eaad
    eaaf eab1 eab3 eab6 eab9 eabb eabe eac1 eac3 eac6 eac7 eac8 eac9
    eaca eacc eace ead1 ead2 ead3 ead4 ead6 ead8 eadb eadc eadd eade
    eadf eae1 eae3 eae6 eae7 eae8 eae9 eaeb eaed eaf0 eaf1 eaf2 eaf4
    eaf7 eaf9 eafb eafd eaff eb01 eb03 eb06
""")

_ACTOR_ERASE_BODY = NativeSemanticRequirement(
    0xc8db,
    (0xc8db, 0xc8dd, 0xc8df, 0xc8e1, 0xc8e3, 0xc8e6, 0xc8e9,
     0xc8ec, 0xc8ef, 0xc8f2),
    "da46d438d54338de15c5f5cb266ee00ff3936e1987923469ec8b34508054a6ea",
)
_ACTOR_FALL_SLOW_BODY = NativeSemanticRequirement(
    0xbe72, (0xbe72, 0xbe74, 0xbe76, 0xbe78, 0xbe7c, 0xbea3),
    "4605701b9bc54794b00e627d2b22d65fcaa006d9f3287aa016c94156b1fb651c",
)
_ACTOR_FALL_FAST_BODY = NativeSemanticRequirement(
    0xbe7a, (0xbe7a, 0xbe7c, 0xbea3),
    "41e76526e808703ff90f5bc1cf97b163f1ee132043ab4a92e2038cd6142d7702",
)
_MOTION_X_BODY = NativeSemanticRequirement(
    0xbe11, (0xbe11, 0xbe12, 0xbe15, 0xbe17),
    "7b4d79c3d245d53c9d89b7aaebd8272a7d81380e708388070d15c0f430828655",
)
_MOTION_X_DO_BODY = NativeSemanticRequirement(
    0xbe1e,
    (0xbe1e, 0xbe20, 0xbe21, 0xbe22, 0xbe23, 0xbe24, 0xbe26, 0xbe28,
     0xbe29, 0xbe2a, 0xbe2b, 0xbe2c, 0xbe2e, 0xbe30, 0xbe32, 0xbe34,
     0xbe36, 0xbe38, 0xbe3a, 0xbe3b, 0xbe3d, 0xbe40, 0xbe41, 0xbe43,
     0xbe46, 0xbe48, 0xbe49, 0xbe4a, 0xbe4b, 0xbe4d, 0xbe4f, 0xbe51,
     0xbe53, 0xbe55, 0xbe57, 0xbe58, 0xbe59, 0xbe5b),
    "34446f0bd877b97ab0ccbe68e61ab85adc38b6707f78e6b4340d9eb1ac5cbe13",
)
_MOTION_FALL_BODY = NativeSemanticRequirement(
    0xbea5, (0xbea5, 0xbea7, 0xbea8, 0xbeab, 0xbead),
    "d37c255cec57ecc0fc71efc8a9d63878c50f8f10241ce01d0f0d53e5cc57f6a7",
)
_MOTION_GRAVITY_DO_BODY = NativeSemanticRequirement(
    0xbebe, (0xbebe, 0xbec0, 0xbec2),
    "a836b7b438091d196f832f57eaf14241838acead87727398c88563c478750dd3",
)
_MOTION_GRAVITY_BODY = NativeSemanticRequirement(
    0xbeea,
    (0xbeea, 0xbeeb, 0xbeee, 0xbeef, 0xbef2, 0xbef5, 0xbef7, 0xbef9,
     0xbefb, 0xbefc, 0xbefe, 0xbf00, 0xbf02, 0xbf04, 0xbf06, 0xbf08,
     0xbf0b, 0xbf0c, 0xbf0e, 0xbf11, 0xbf13, 0xbf15, 0xbf17, 0xbf19,
     0xbf1b, 0xbf1e, 0xbf20, 0xbf22, 0xbf24, 0xbf26, 0xbf28, 0xbf2b,
     0xbf2c, 0xbf2e, 0xbf30, 0xbf32, 0xbf33, 0xbf34, 0xbf36, 0xbf39,
     0xbf3a, 0xbf3c, 0xbf3f, 0xbf41, 0xbf43, 0xbf45, 0xbf47, 0xbf49,
     0xbf4c, 0xbf4e, 0xbf50, 0xbf52, 0xbf54, 0xbf56, 0xbf59),
    "dcaec7c8f29ecd6b849d0b69bdab6ccc891fa1d14d39032f552dbcf4ea1b48fc",
)

_ACTOR_PROC_BASE_BODY = _address_tuple("""
    c9ba c9bc c9be c9c0 c9c2 c9c4 c9c5 c9c7 c9c9 c9cb c9cd c9cf
    c9d1 c9d3 c9d5 c9d7 c9d9 c9db c9de c9e0 c9e2 c9e4 c9e6 c9e8
    c9ea c9ec c9ee c9f0 c9f2 c9f5 c9f7 c9f9 c9fa c9fc c9fd c9fe
    c9ff ca02 ca04 ca07 ca08 ca0a ca0b ca0e ca10 ca12 ca14 ca16
    ca17 ca18 ca1a ca1b ca1e ca20 ca21 ca22 ca25 ca27 ca28 ca2b
    ca2e ca30 ca32 ca34 ca36 ca38 ca3b
""")

_ACTOR_OOB_BODY = _address_tuple("""
    d5bd d5bf d5c1 d5c3 d5c6 d5c8 d5ca d5cc d5ce d5d0 d5d2 d5d4
    d5d6 d5d9 d5db d5dd d5e0 d5e2 d5e4 d5e7 d5e9 d5eb d5ed d5ef
    d5f1 d5f3 d5f5 d5f7 d5f9 d5fb d5fd d5ff d601 d603 d605 d607
    d609 d60b d60d d60f d611 d613 d615 d618
""")

_COL_BOX_PROC_BODY = _address_tuple("""
    e1f2 e1f4 e1f7 e1f9 e1fc e1fe e1ff e200 e201 e202 e203 e206
    e207 e208 e209 e20b e20c e20f e212 e214 e215 e218 e21b e21c
    e21d e21f e220 e223 e226 e228 e229 e22c e22f e230 e231 e233
""")
_COL_BOX_BUFFER_BODY = _address_tuple("""
    e348 e349 e34b e34e e34f e351 e353 e355 e357 e359 e35a e35c
    e35d e35e e35f e360 e363 e365 e367 e368 e36b e36d e36e e370
    e372 e373 e375 e377 e379 e37a e37c e37e e381 e383 e385 e387
    e389
""")
_POS_BITS_GET_X_BODY = _address_tuple("""
    f15b f15d f15f f162 f163 f165 f167 f16a f16c f16f f171 f173
    f176 f178 f17a f17c f17e f180 f183 f186 f188 f18a f18c f18d
    f18f
""")
_POS_BITS_PROC_BODY = _address_tuple("""
    f13c f13f f140 f141 f142 f143 f145 f19e f1a0 f1a2 f1a5 f1a6
    f1a8 f1aa f1ac f1ae f1b1 f1b3 f1b5 f1b8 f1ba f1bc f1be f1c0
    f1c2 f1c5 f1c8 f1ca f1cc f1ce f1cf f1d1
""")
_POS_BITS_ACTOR_BODY = _address_tuple("""
    f114 f116 f118 f11f f121 f122 f124 f125 f126 f127 f12a f12b
    f12c f12d f12e f130 f132 f133 f134 f136 f139 f13b
""")
_COL_BOX_BLOCK_BUFFER_BODY = NativeSemanticRequirement(
    0xab14,
    (0xab14, 0xab15, 0xab16, 0xab17, 0xab18, 0xab19, 0xab1a,
     0xab1d, 0xab1f, 0xab20, 0xab22, 0xab23, 0xab26, 0xab28),
    "6107eab257aa20c4007e224692413e64499fe22d31cd02a2f081fa2882464ce8",
)
_POS_BITS_DIFF_BODY = NativeSemanticRequirement(
    0xf1d2,
    (0xf1d2, 0xf1d4, 0xf1d6, 0xf1d8, 0xf1da, 0xf1db, 0xf1dc,
     0xf1dd, 0xf1df, 0xf1e1, 0xf1e3, 0xf1e5, 0xf1e6),
    "2512abfcfa3e43f35588715357b4d81257c8f8158569ecc085b53aa91a7ab91a",
)
_POS_BITS_GET_X_REQUIREMENT = NativeSemanticRequirement(
    0xf15b, _POS_BITS_GET_X_BODY,
    "42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758",
)
_POS_BITS_PROC_REQUIREMENT = NativeSemanticRequirement(
    0xf13c, _POS_BITS_PROC_BODY,
    "3896497d950205f45e1d297f94b0568ff79193a982b9f3388a5ef81f7afdae26",
)

_META_RENDER_AREA_BODY = _address_tuple("""
    8aaf 8ab2 8ab4 8ab6 8ab9 8abb 8abe 8ac1 8ac4 8ac7 8ac9 8acc
    8ace 8ad0 8ad1 8ad3 8ad6 8ad8 8ada 8adb 8adc 8add 8ade 8ae1
    8ae3 8ae6 8ae8 8aeb 8aec 8aed 8aef 8af2 8af4 8af6 8af7 8af9
    8afa 8afc 8afe 8b01 8b02 8b04 8b07 8b09 8b0b 8b0d 8b0f 8b10
    8b12 8b14 8b16 8b18 8b1b 8b1d 8b1e 8b20 8b22 8b24 8b26 8b28
    8b2b 8b2d 8b2f 8b31 8b34 8b36 8b39 8b3b 8b3d 8b3f 8b40 8b42
    8b44 8b46 8b47 8b48 8b49 8b4b 8b4e 8b51 8b54 8b57 8b59 8b5b
    8b5d 8b60 8b63 8b65 8b68 8bbe 8bc0 8bc3
""")

_GAME_PROC_BODY = _address_tuple("""
    ad6e ad71 ad73 ad76 ad79 ad7c ad7f ad82 ad85 ad87 ad89 ad8c
    ad8e ad91 ad94 ad96 ad98 ad9b ad9d ada0 ada3 ada6 ada8 adaa
    adab adae adb0 adb2 adb5 adb8 adb9 adbb adbd adc0 adc3 adc6
    adc9 adcb adcd add0 add1 add3 add6 add9 addc addf ade2 ade5
    ade8 adea adec adee adf1 adf3 adf5 adf7 adfa adfc adff ae02
    ae04 ae06 ae08 ae09 ae0a ae0b ae0e ae11 ae14 ae16 ae18 ae1a
""")

_ACTOR_PROC_BODY = _address_tuple("""
    bf60 bf62 bf63 bf64 bf66 bf67 bf69 bf6c bf6f bf71 bf73 bf75 bf78 bf79
    bf7b bf7c bf7f bf81 bf83 bfea bfed bfef bff2 bff4 bff6 bff7 bff9 bffc
    bfff c001 c004 c007 c009 c00b c00e c010 c012 c014 c016 c019 c01b c01d
    c020 c023 c026 c028 c02a c02d c02f c031 c033 c036 c038 c03a c03d c040
    c042 c045 c048 c04a c04d c050 c052 c054 c056 c058 c05a c05c c05f c062
    c064 c067 c06a c06c c06e c070 c073 c075 c077 c079 c07b c07d c07e c080
    c083 c085 c087 c089 c08b c08c c08f c090 c092 c094 c096 c099 c09b c09d
    c0a0 c0a1 c0a3 c0a6 c0a8 c0a9 c0ab c0ae c0b0 c0b3 c0b6 c0b7 c0b9 c0bc
    c0be c0c0 c0c2 c0c4 c0c7 c0c9 c0ca c0cc c0cf c0d1 c0d3 c0d6 c0d9 c0dc
    c0df c0e2 c0e4 c0e7 c0ea c0ec c0ee c0f1 c0f3 c0f5 c0f7 c0fa c0fc c0ff
    c101 c103 c106 c108 c10a c10c c10e c111 c113 c115 c117 c119 c11b c11d
    c11f c121 c124 c126 c127 c128 c129 c12a c12c c12e c130 c131 c133 c136
    c138 c13a c13c c13e c140 c142 c144 c146 c148 c14b c14d c14f c150 c153
    c155 c158 c15a c15c c15e c167 c168 c16b c16d c170 c171 c172 c174 c175
    c176 c177 c178 c179 c17c c17e c17f c181 c184 c186 c189 c18a c18c c18e
    c191 c194 c196 c199 c19c c19e c1a0 c1a2 c1a4 c1a7 c1aa c1ad c1af c1b2
    c1b4 c65e c660 c661 c663 c664 c666 c668 c669 c66b c66e c670 c672 c673
    c675 c677 c679 c67b c67d c67f c682 c684 c687 c689 c68b c68c c68d c68f
    c690 c693 c695 c696 c698 c69a c69c c69e c6a0 c6a2 c6a4 c6a6 c6a8 c6aa
    c6ab c6ad c6af c6b1 c6b3 c6b5 c6b7 c6b9 c6bb c6bd c6bf c6c2 c6c5 c6c7
    c7c5 c7c7 c7c9 c7cb c7cd c7cf c7d0 c7d2
""")


_NATIVE_SEMANTIC_ROUTINES = {
    0x8aaf: NativeSemanticRoutine(
        _META_RENDER_AREA_BODY,
        "c274be12dfc9e6558977e3181af1adc766784d095d6f40ac1f2855dd6416c5f9",
        "vs_native_fast_meta_render_area", "vs_native_fast_meta.h",
        conditional=True,
    ),
    0xad6e: NativeSemanticRoutine(
        _GAME_PROC_BODY,
        "f783e2dcbab8c15349e66708e5cf7b752b39d9e2ca68071f571b470578683e39",
        "vs_native_fast_game_proc", "vs_native_fast_game.h",
    ),
    0xbf60: NativeSemanticRoutine(
        _ACTOR_PROC_BODY,
        "441517384e59ff6ac0ea07fccd67a75dc093adf7ce2ee773fc2a1cedcc675920",
        "vs_native_fast_actor_proc", "vs_native_fast_actor_loop.h",
    ),
    0x8212: NativeSemanticRoutine(
        _address_tuple("""
            8212 8215 8217 8219 821b 821e 8220 8222 8225 8226 8229 822b
            822c 822e 8231 8232 8234 8237 8238 823a 823c 823e 8241 8243
            8245 8248 824b 824c 824e 8251 8252 8254 8257 8258 8259 825a
            825b 825d
        """),
        "f94b014c05ed7c56da8bd0e38168cbbb2267af4778a0cd545e8f0cf69918866f",
        "vs_native_fast_nmi_oam_shuffle", "vs_native_fast_nmi.h"),
    0x826e: NativeSemanticRoutine(
        (0x826e, 0x8270, 0x8275, 0x8277, 0x827a, 0x827b, 0x827c,
         0x827d, 0x827e, 0x8280),
        "99d85b327abde02561d9a0889599a708ad8d45bcbe3c5c74d2d7e5eeaf6d281e",
        "vs_native_fast_oam_init", "vs_native_fast_nmi.h"),
    0x8273: NativeSemanticRoutine(
        (0x8273, 0x8275, 0x8277, 0x827a, 0x827b, 0x827c, 0x827d,
         0x827e, 0x8280),
        "2ef4d0693c67e06453784e56bd25e387859081ed0fe6798f27cbf9695758affe",
        "vs_native_fast_oam_init_all", "vs_native_fast_nmi.h"),
    0xe7da: NativeSemanticRoutine(
        _RENDER_ACTOR_BODY,
        "affd324b3a1a646a7132e1280044abc7eca3456f1b415e90c88ef34b844f6093",
        "vs_native_fast_render_actor", "vs_native_fast_actor.h",
        conditional=True,
        requirements=(
            _RENDER_ACTOR_PAIR_BODY,
            _RENDER_CHR_PAIR_BODY,
            _RENDER_SET_SIX_BODY,
            _RENDER_SET_FOUR_BODY,
            _RENDER_SET_THREE_BODY,
            _RENDER_SET_TWO_BODY,
            _RENDER_CLEAR_H_BODY,
            _RENDER_CLEAR_TOP_BODY,
        ),
    ),
    0xeb07: NativeSemanticRoutine(
        _RENDER_ACTOR_PAIR_BODY.addresses,
        _RENDER_ACTOR_PAIR_BODY.digest,
        "vs_native_fast_render_actor_chr_pair", "vs_native_fast_actor.h",
        conditional=True,
        requirements=(_RENDER_CHR_PAIR_BODY,),
    ),
    0xeb0f: NativeSemanticRoutine(
        _RENDER_CHR_PAIR_BODY.addresses,
        _RENDER_CHR_PAIR_BODY.digest,
        "vs_native_fast_render_chr_pair", "vs_native_fast_actor.h",
        conditional=True,
    ),
    0xc9ba: NativeSemanticRoutine(
        _ACTOR_PROC_BASE_BODY,
        "321140d5c489194c532d65fb73d0d3d8b6353a6962837a501768514e5efb5a64",
        "vs_native_fast_actor_proc_base", "vs_native_fast_actor_policy.h",
        conditional=True,
        requirements=(
            _ACTOR_ERASE_BODY,
            _ACTOR_FALL_SLOW_BODY,
            _ACTOR_FALL_FAST_BODY,
            _MOTION_X_BODY,
            _MOTION_X_DO_BODY,
            _MOTION_FALL_BODY,
            _MOTION_GRAVITY_DO_BODY,
            _MOTION_GRAVITY_BODY,
        ),
    ),
    0xd5bd: NativeSemanticRoutine(
        _ACTOR_OOB_BODY,
        "edb1d0eecc751178f3eb45e32cedb4be1b79cd8733094490c3d4cd20f1c7ca72",
        "vs_native_fast_actor_oob_proc", "vs_native_fast_actor_policy.h",
        conditional=True,
        requirements=(_ACTOR_ERASE_BODY,),
    ),
    0xe1f2: NativeSemanticRoutine(
        _COL_BOX_PROC_BODY,
        "e1f5a4cb13dd3a8e91f18a8ea7b43dabb77ebe02132f8b8e763ffdc244f2b938",
        "vs_native_fast_col_box_proc", "vs_native_fast_collision.h",
        conditional=True,
    ),
    0xe348: NativeSemanticRoutine(
        _COL_BOX_BUFFER_BODY,
        "0a8c68a9f05633c3cbb20d2f43e4254fab8738ec42efdce557885abc255552df",
        "vs_native_fast_col_box_buffer", "vs_native_fast_collision.h",
        conditional=True,
        requirements=(_COL_BOX_BLOCK_BUFFER_BODY,),
    ),
    0xf15b: NativeSemanticRoutine(
        _POS_BITS_GET_X_BODY,
        "42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758",
        "vs_native_fast_pos_bits_get_do_x", "vs_native_fast_collision.h",
        conditional=True,
        requirements=(_POS_BITS_DIFF_BODY,),
    ),
    0xf13c: NativeSemanticRoutine(
        _POS_BITS_PROC_BODY,
        "3896497d950205f45e1d297f94b0568ff79193a982b9f3388a5ef81f7afdae26",
        "vs_native_fast_pos_bits_proc", "vs_native_fast_collision.h",
        conditional=True,
        requirements=(_POS_BITS_GET_X_REQUIREMENT, _POS_BITS_DIFF_BODY),
    ),
    0xf114: NativeSemanticRoutine(
        _POS_BITS_ACTOR_BODY,
        "2c3faf7ad4821695f3e496c28bd307c9bf03f10fb9d6295d53fa84b9ddf59765",
        "vs_native_fast_pos_bits_get_actor", "vs_native_fast_collision.h",
        conditional=True,
        requirements=(
            _POS_BITS_PROC_REQUIREMENT,
            _POS_BITS_GET_X_REQUIREMENT,
            _POS_BITS_DIFF_BODY,
        ),
    ),
    0xbe11: NativeSemanticRoutine(
        (0xbe11, 0xbe12, 0xbe15, 0xbe17),
        "7b4d79c3d245d53c9d89b7aaebd8272a7d81380e708388070d15c0f430828655",
        "vs_native_fast_motion_x", "vs_native_fast_motion.h"),
    0xbe18: NativeSemanticRoutine(
        (0xbe18, 0xbe1b, 0xbe1d, 0xbe5b),
        "26ef256952f781ca34708a465c9465df9bdca5de59da1d07ea4eb0be80c346b1",
        "vs_native_fast_motion_x_player", "vs_native_fast_motion.h"),
    0xbe1e: NativeSemanticRoutine(
        (0xbe1e, 0xbe20, 0xbe21, 0xbe22, 0xbe23, 0xbe24, 0xbe26, 0xbe28,
         0xbe29, 0xbe2a, 0xbe2b, 0xbe2c, 0xbe2e, 0xbe30, 0xbe32, 0xbe34,
         0xbe36, 0xbe38, 0xbe3a, 0xbe3b, 0xbe3d, 0xbe40, 0xbe41, 0xbe43,
         0xbe46, 0xbe48, 0xbe49, 0xbe4a, 0xbe4b, 0xbe4d, 0xbe4f, 0xbe51,
         0xbe53, 0xbe55, 0xbe57, 0xbe58, 0xbe59, 0xbe5b),
        "34446f0bd877b97ab0ccbe68e61ab85adc38b6707f78e6b4340d9eb1ac5cbe13",
        "vs_native_fast_motion_x_do", "vs_native_fast_motion.h"),
    0xbe97: NativeSemanticRoutine(
        (0xbe97, 0xbe99, 0xbe9d, 0xbe9f),
        "85f94cebc4736b6eb62d0e1fdae514634248e654b1062a364dfefbfbf9174aad",
        "vs_native_fast_motion_fall_fast", "vs_native_fast_motion.h"),
    0xbe9b: NativeSemanticRoutine(
        (0xbe9b, 0xbe9d, 0xbe9f),
        "ff1fd3e5bd0602e95a071e266d98b6de6db92f71f32277b7de485f21d1e4999e",
        "vs_native_fast_motion_fall_slow", "vs_native_fast_motion.h"),
    0xbea1: NativeSemanticRoutine(
        (0xbea1, 0xbea3),
        "f59233e5d10661cbbdbfd9ecda02d9cc8a938f9cb5b42bdb8ed5e354302c77a9",
        "vs_native_fast_motion_fall_mid", "vs_native_fast_motion.h"),
    0xbea5: NativeSemanticRoutine(
        (0xbea5, 0xbea7, 0xbea8, 0xbeab, 0xbead),
        "d37c255cec57ecc0fc71efc8a9d63878c50f8f10241ce01d0f0d53e5cc57f6a7",
        "vs_native_fast_motion_fall", "vs_native_fast_motion.h"),
    0xbebe: NativeSemanticRoutine(
        (0xbebe, 0xbec0, 0xbec2),
        "a836b7b438091d196f832f57eaf14241838acead87727398c88563c478750dd3",
        "vs_native_fast_motion_gravity_do", "vs_native_fast_motion.h"),
    0xbeea: NativeSemanticRoutine(
        (0xbeea, 0xbeeb, 0xbeee, 0xbeef, 0xbef2, 0xbef5, 0xbef7, 0xbef9,
         0xbefb, 0xbefc, 0xbefe, 0xbf00, 0xbf02, 0xbf04, 0xbf06, 0xbf08,
         0xbf0b, 0xbf0c, 0xbf0e, 0xbf11, 0xbf13, 0xbf15, 0xbf17, 0xbf19,
         0xbf1b, 0xbf1e, 0xbf20, 0xbf22, 0xbf24, 0xbf26, 0xbf28, 0xbf2b,
         0xbf2c, 0xbf2e, 0xbf30, 0xbf32, 0xbf33, 0xbf34, 0xbf36, 0xbf39,
         0xbf3a, 0xbf3c, 0xbf3f, 0xbf41, 0xbf43, 0xbf45, 0xbf47, 0xbf49,
         0xbf4c, 0xbf4e, 0xbf50, 0xbf52, 0xbf54, 0xbf56, 0xbf59),
        "dcaec7c8f29ecd6b849d0b69bdab6ccc891fa1d14d39032f552dbcf4ea1b48fc",
        "vs_native_fast_motion_gravity", "vs_native_fast_motion.h"),
}


_PPU_DISPLIST_CTLR0_BODY = NativeSemanticRequirement(
    0x91a5,
    (0x91a5, 0x91a8, 0x91ab),
    "e263115cf8e9b468233d0d2f2a5d4b2885e73dd2addd4a16cf245fd16a2b9c8a",
)

_NATIVE_SEMANTIC_SLICES = (
    NativeSemanticSlice(
        owner=0x810a,
        start=0x817e,
        continuation=0x81a3,
        addresses=(
            0x817e, 0x8181, 0x8183, 0x8186, 0x8188, 0x818a, 0x818d,
            0x818f, 0x8191, 0x8194, 0x8196, 0x8199, 0x819b, 0x819e,
            0x819f, 0x81a1,
        ),
        digest="1e148a96019465b3089f26408a70ad0933c7800bb4a1eb634145a3d4838b6af1",
        function="vs_native_fast_nmi_timers",
        header="vs_native_fast_nmi.h",
        conditional=False,
    ),
    NativeSemanticSlice(
        owner=0x810a,
        start=0x81a3,
        continuation=0x81c0,
        addresses=(
            0x81a3, 0x81a5, 0x81a7, 0x81aa, 0x81ac, 0x81ae, 0x81b1,
            0x81b3, 0x81b5, 0x81b6, 0x81b8, 0x81b9, 0x81bc, 0x81bd,
            0x81be,
        ),
        digest="de79b7d38db5f79f664910f7d57b1a70d490bc66fd75617ed5abe9c9d9fbe393",
        function="vs_native_fast_nmi_random",
        header="vs_native_fast_nmi.h",
        conditional=False,
    ),
    NativeSemanticSlice(
        owner=0x810a,
        start=0x81d9,
        continuation=0x81de,
        addresses=(0x81d9, 0x81db, 0x81dc),
        digest="2f176738248cb186bd063eab050fc6deb527f7077fe4c9216437d08ca49f455f",
        function="vs_native_fast_nmi_hblank_delay",
        header="vs_native_fast_nmi.h",
        conditional=False,
    ),
    NativeSemanticSlice(
        owner=0x9195,
        start=0x914a,
        continuation=0x9195,
        addresses=_address_tuple("""
            914a 914d 914e 9150 9153 9154 9156 9157 9158 915b 915d 915f
            9161 9164 9165 9166 9168 916a 916b 916c 916d 916e 9170 9171
            9173 9176 9177 9179 917a 917b 917d 917f 9181 9183 9185 9187
            918a 918c 918f 9192
        """),
        digest="df68b2f9c2ffaf8b0b2c615ea5964721e93c53d4696c2feb8124bb7ddc221b47",
        function="vs_native_fast_ppu_displist_command",
        header="vs_native_fast_video.h",
        arguments=("ctx", "ctx->fast_video_hooks"),
        requirements=(_PPU_DISPLIST_CTLR0_BODY,),
    ),
)


def verified_native_semantics(
    code: dict[int, tuple[str, str, int]],
    bodies: dict[int, set[int]],
) -> dict[int, NativeSemanticRoutine]:
    """Select only semantic bodies matching the reviewed address and opcode graph."""
    import hashlib

    def body_matches(
        entry: int, addresses: tuple[int, ...], expected_digest: str
    ) -> bool:
        if bodies.get(entry) != set(addresses):
            return False
        digest = hashlib.sha256(json.dumps(
            [code[address] for address in addresses],
            separators=(",", ":"),
        ).encode()).hexdigest()
        return digest == expected_digest

    verified: dict[int, NativeSemanticRoutine] = {}
    for entry, routine in _NATIVE_SEMANTIC_ROUTINES.items():
        if not body_matches(entry, routine.addresses, routine.digest):
            continue
        if all(body_matches(
            requirement.entry,
            requirement.addresses,
            requirement.digest,
        ) for requirement in routine.requirements):
            verified[entry] = routine
    return verified


def verified_native_semantic_slices(
    code: dict[int, tuple[str, str, int]],
    bodies: dict[int, set[int]],
) -> dict[tuple[int, int], NativeSemanticSlice]:
    """Select reviewed interior slices only when their whole region matches."""
    import hashlib

    def exact_body(requirement: NativeSemanticRequirement) -> bool:
        if bodies.get(requirement.entry) != set(requirement.addresses):
            return False
        digest = hashlib.sha256(json.dumps(
            [code[address] for address in requirement.addresses],
            separators=(",", ":"),
        ).encode()).hexdigest()
        return digest == requirement.digest

    verified: dict[tuple[int, int], NativeSemanticSlice] = {}
    for semantic in _NATIVE_SEMANTIC_SLICES:
        owner_body = bodies.get(semantic.owner)
        if owner_body is None:
            continue
        # The numeric interval is part of this source seam's contract.  An
        # inserted or removed instruction cannot evade the explicit digest.
        actual = tuple(sorted(
            address for address in owner_body
            if semantic.start <= address < semantic.continuation
        ))
        if actual != semantic.addresses:
            continue
        digest = hashlib.sha256(json.dumps(
            [code[address] for address in semantic.addresses],
            separators=(",", ":"),
        ).encode()).hexdigest()
        if digest != semantic.digest:
            continue
        if not all(exact_body(requirement)
                   for requirement in semantic.requirements):
            continue
        verified[(semantic.owner, semantic.start)] = semantic
    return verified


_BRANCH_COMPLEMENT = {
    "BPL": "BMI", "BMI": "BPL", "BVC": "BVS", "BVS": "BVC",
    "BCC": "BCS", "BCS": "BCC", "BNE": "BEQ", "BEQ": "BNE",
}


def _raw_body_successors(
    code: dict[int, tuple[str, str, int]], address: int
) -> tuple[int, ...]:
    op, mode, operand = code[address]
    nxt = address + _LENGTH[mode]
    if op in ("RTS", "RTI", "BRK"):
        return ()
    if op == "JMP" and mode == "w":
        return () if operand == address else (operand,)
    if mode == "r":
        delta = operand if operand < 128 else operand - 256
        return nxt, (nxt + delta) & 0xffff
    return (nxt,)


def _complementary_branch_fallthroughs(
    code: dict[int, tuple[str, str, int]], body: set[int]
) -> set[int]:
    """Find second branches whose fallthrough is provably impossible.

    ca65 commonly emits ``bcc target_a; bcs target_b`` (and equivalent flag
    pairs) as an unconditional two-way split. A depth-only CFG otherwise
    invents a third path through both branches. Suppression is safe only when
    the second branch has exactly the first branch's fallthrough as its sole
    in-body predecessor, so another edge cannot arrive with an unknown flag.
    """
    incoming: dict[int, set[int]] = {address: set() for address in body}
    for address in body:
        for target in _raw_body_successors(code, address):
            if target in body:
                incoming[target].add(address)

    impossible: set[int] = set()
    for address in body:
        op, mode, _ = code[address]
        if mode != "r":
            continue
        sequential = [
            prior for prior in body
            if prior + _LENGTH[code[prior][1]] == address
        ]
        if len(sequential) != 1 or incoming[address] != {sequential[0]}:
            continue
        prior = sequential[0]
        prior_op, prior_mode, _ = code[prior]
        if prior_mode == "r" and _BRANCH_COMPLEMENT.get(prior_op) == op:
            impossible.add(address)
    return impossible


def _is_terminal_frame_discard(
    code: dict[int, tuple[str, str, int]], body: set[int], address: int
) -> bool:
    """Return true for a suffix that only drops an interrupt frame and exits."""
    cursor = address
    while cursor in body:
        op, mode, _ = code[cursor]
        if op in ("PLA", "PLP"):
            cursor += _LENGTH[mode]
            continue
        return op in ("RTS", "RTI")
    return False


def saved_value_layouts(
    code: dict[int, tuple[str, str, int]],
    bodies: dict[int, set[int]],
    table_entry: int | None = None,
    reset_txs: set[int] | None = None,
    hardware_frame_entries: set[int] | None = None,
) -> tuple[dict[int, SavedValueLayout], dict[str, object]]:
    """Prove processor save/restore operations can be scalar C locals.

    Calls do not change the saved-value depth because native C owns its call
    storage. Every ordinary exit must have depth zero, and every instruction
    must have one compile-time depth. The emitted context therefore needs no
    runtime pointer or value array.
    """
    reset_txs = reset_txs or set()
    hardware_frame_entries = hardware_frame_entries or set()
    layouts: dict[int, SavedValueLayout] = {}
    unexpected_pointer_ops: set[str] = set()
    underflows: set[str] = set()
    frame_discards_report: set[str] = set()
    ambiguous: set[str] = set()
    unbalanced_exits: set[str] = set()
    maximum = 0

    for entry, body in bodies.items():
        # Each call site for the table helper is expanded to a static switch.
        # Its processor return-frame pops have no callable native body.
        if table_entry is not None and entry == table_entry:
            layouts[entry] = SavedValueLayout({}, 0, frozenset())
            continue

        impossible_fallthrough = _complementary_branch_fallthroughs(code, body)
        depths: dict[int, set[int]] = {entry: {0}}
        pending: list[tuple[int, int]] = [(entry, 0)]
        seen: set[tuple[int, int]] = set()
        frame_discards: set[int] = set()
        local_count = 0

        while pending:
            address, depth = pending.pop()
            if (address, depth) in seen or address not in body:
                continue
            seen.add((address, depth))
            op, _, _ = code[address]
            next_depth = depth
            if op in ("TSX", "TXS") and address not in reset_txs:
                unexpected_pointer_ops.add(f"0x{address:04x}")
            if op in ("PHA", "PHP"):
                next_depth += 1
                local_count = max(local_count, next_depth)
            elif op in ("PLA", "PLP"):
                if depth == 0:
                    if (entry in hardware_frame_entries and
                            _is_terminal_frame_discard(code, body, address)):
                        frame_discards.add(address)
                        frame_discards_report.add(f"0x{address:04x}")
                    else:
                        underflows.add(f"0x{address:04x}")
                else:
                    next_depth -= 1

            if next_depth > 256:
                ambiguous.add(f"0x{address:04x}:growing")
                continue

            successors = list(_raw_body_successors(code, address))
            if address in impossible_fallthrough:
                # Relative successors are ordered fallthrough, target.
                successors = successors[1:]
            inside = [target for target in successors if target in body]
            outside = [target for target in successors if target not in body]
            if (not successors or outside) and next_depth != 0:
                unbalanced_exits.add(f"0x{address:04x}:depth={next_depth}")
            for target in inside:
                values = depths.setdefault(target, set())
                if next_depth not in values:
                    values.add(next_depth)
                    pending.append((target, next_depth))

        depth_at: dict[int, int] = {}
        for address, values in depths.items():
            if len(values) != 1:
                value_text = "/".join(str(value) for value in sorted(values))
                ambiguous.add(f"0x{address:04x}:depth={value_text}")
                continue
            depth_at[address] = next(iter(values))
        layouts[entry] = SavedValueLayout(
            depth_at, local_count, frozenset(frame_discards)
        )
        maximum = max(maximum, local_count)

    report: dict[str, object] = {
        # Retain the historic keys for callers that display diagnostics.
        "tsx_txs": sorted(unexpected_pointer_ops),
        "possible_data_underflow": sorted(underflows),
        "hardware_frame_discards": sorted(frame_discards_report),
        "ambiguous_depths": sorted(ambiguous),
        "unbalanced_exits": sorted(unbalanced_exits),
        "max_local_values": maximum,
    }
    return layouts, report


def data_stack_report(
    code: dict[int, tuple[str, str, int]],
    bodies: dict[int, set[int]],
    table_entry: int | None = None,
    reset_txs: set[int] | None = None,
    hardware_frame_entries: set[int] | None = None,
) -> dict[str, object]:
    """Compatibility wrapper for the compile-time saved-value report."""
    _, report = saved_value_layouts(
        code, bodies, table_entry, reset_txs, hardware_frame_entries
    )
    return report


def direct_function_names(symbols: dict[str, int], bodies: dict[int, set[int]]) -> dict[int, str]:
    aliases: dict[int, list[str]] = {}
    for name, address in symbols.items():
        if address in bodies:
            aliases.setdefault(address, []).append(name)
    result = {}
    for address in bodies:
        names = sorted(aliases.get(address, []), key=lambda name: (len(name), name))
        stem = names[0] if names else f"region_{address:04x}"
        result[address] = _c_name(f"fn_{stem}_{address:04x}")
    return result


def _native_addr(mode: str, operand: int) -> str:
    if mode == "z": return f"0x{operand:02x}u"
    if mode == "w": return f"0x{operand:04x}u"
    if mode == "zx": return f"(uint16_t)(uint8_t)(0x{operand:02x}u + ctx->x)"
    if mode == "zy": return f"(uint16_t)(uint8_t)(0x{operand:02x}u + ctx->y)"
    if mode == "wx": return f"(uint16_t)(0x{operand:04x}u + ctx->x)"
    if mode == "wy": return f"(uint16_t)(0x{operand:04x}u + ctx->y)"
    if mode == "ix": return f"native_zpword(ctx, (uint8_t)(0x{operand:02x}u + ctx->x))"
    if mode == "iy": return f"(uint16_t)(native_zpword(ctx, 0x{operand:02x}u) + ctx->y)"
    raise ValueError(f"no address expression for {mode}")


def _native_read(mode: str, operand: int) -> str:
    if mode == "n": return f"0x{operand:02x}u"
    if mode == "a": return "ctx->a"
    return f"native_read(ctx, {_native_addr(mode, operand)})"


def emit_direct_header() -> str:
    return """/* Generated direct-C migration ABI. No ROM or source data. */
#ifndef SMBNEO_NATIVE_CODEGEN_H
#define SMBNEO_NATIVE_CODEGEN_H
#include <stdint.h>

typedef uint8_t (*SmbNeoNativeRead8)(void *opaque, uint16_t address);
typedef void (*SmbNeoNativeWrite8)(void *opaque, uint16_t address, uint8_t value);
struct VsNativeFastVideoHooks;

typedef struct {
    uint8_t a, x, y, status;
    uint8_t *ram;             /* mirrored 2 KiB CPU RAM */
    uint8_t *extra_ram;       /* mirrored 2 KiB VS work RAM */
    const uint8_t *prg;       /* immutable 32 KiB program data */
    void *opaque;
    SmbNeoNativeRead8 read8;
    SmbNeoNativeWrite8 write8;
    const struct VsNativeFastVideoHooks *fast_video_hooks;
    uint8_t suspended, unsupported;
    uint32_t calls;
    uint16_t last_symbol;
} SmbNeoNativeContext;

enum { SMBNEO_NATIVE_TITLE = 0, SMBNEO_NATIVE_GAME = 1 };
enum { NATIVE_C = 1, NATIVE_Z = 2, NATIVE_I = 4, NATIVE_D = 8,
       NATIVE_B = 16, NATIVE_U = 32, NATIVE_V = 64, NATIVE_N = 128 };

void smbneo_native_boot(SmbNeoNativeContext *ctx);
void smbneo_native_frame(SmbNeoNativeContext *ctx);
void smbneo_native_title_tick(SmbNeoNativeContext *ctx);
void smbneo_native_game_tick(SmbNeoNativeContext *ctx);
#endif
"""


def _emit_simple_op(address: int, op: str, mode: str, operand: int) -> list[str]:
    value = _native_read(mode, operand) if mode != "i" else None
    addr = _native_addr(mode, operand) if mode not in ("i", "n", "a") else None
    if op in ("LDA", "LDX", "LDY"):
        reg = op[-1].lower()
        return [f"ctx->{reg} = native_nz(ctx, {value});"]
    if op in ("STA", "STX", "STY"):
        reg = op[-1].lower()
        return [f"native_write(ctx, {addr}, ctx->{reg});"]
    if op in ("AND", "ORA", "EOR"):
        symbol = {"AND": "&", "ORA": "|", "EOR": "^"}[op]
        return [f"ctx->a = native_nz(ctx, (uint8_t)(ctx->a {symbol} {value}));"]
    if op in ("ADC", "SBC"):
        return [f"native_{op.lower()}(ctx, {value});"]
    if op in ("CMP", "CPX", "CPY"):
        reg = "a" if op == "CMP" else op[-1].lower()
        return [f"native_cmp(ctx, ctx->{reg}, {value});"]
    if op in ("INC", "DEC"):
        delta = "+ 1u" if op == "INC" else "- 1u"
        return [f"native_write(ctx, {addr}, native_nz(ctx, (uint8_t)(native_read(ctx, {addr}) {delta})));" ]
    if op in ("ASL", "LSR", "ROL", "ROR"):
        if mode == "a":
            return [f"ctx->a = native_{op.lower()}(ctx, ctx->a);"]
        return [f"native_write(ctx, {addr}, native_{op.lower()}(ctx, native_read(ctx, {addr})));" ]
    if op in ("INX", "INY", "DEX", "DEY"):
        reg = op[-1].lower(); delta = "+ 1u" if op[0] == "I" else "- 1u"
        return [f"ctx->{reg} = native_nz(ctx, (uint8_t)(ctx->{reg} {delta}));"]
    if op in ("TAX", "TAY", "TXA", "TYA"):
        dst, src = {
            "TAX": ("x", "a"),
            "TAY": ("y", "a"),
            "TXA": ("a", "x"),
            "TYA": ("a", "y"),
        }[op]
        return [f"ctx->{dst} = native_nz(ctx, ctx->{src});"]
    if op == "BIT":
        return [f"native_bit(ctx, {_native_read(mode, operand)});"]
    if op == "CLC": return ["ctx->status &= (uint8_t)~NATIVE_C;"]
    if op == "SEC": return ["ctx->status |= NATIVE_C;"]
    if op == "CLI": return ["ctx->status &= (uint8_t)~NATIVE_I;"]
    if op == "SEI": return ["ctx->status |= NATIVE_I;"]
    if op == "CLD": return ["ctx->status &= (uint8_t)~NATIVE_D;"]
    if op == "SED": return ["ctx->status |= NATIVE_D;"]
    if op == "CLV": return ["ctx->status &= (uint8_t)~NATIVE_V;"]
    if op in ("PHA", "PHP", "PLA", "PLP", "TSX", "TXS"):
        raise ValueError(f"processor save operation requires static layout at {address:04x}")
    if op == "NOP": return []
    raise ValueError(f"unsupported direct-C opcode {op} at {address:04x}")


def _branch_condition(op: str) -> str:
    return {
        "BPL": "!(ctx->status & NATIVE_N)", "BMI": "(ctx->status & NATIVE_N)",
        "BVC": "!(ctx->status & NATIVE_V)", "BVS": "(ctx->status & NATIVE_V)",
        "BCC": "!(ctx->status & NATIVE_C)", "BCS": "(ctx->status & NATIVE_C)",
        "BNE": "!(ctx->status & NATIVE_Z)", "BEQ": "(ctx->status & NATIVE_Z)",
    }[op]


def emit_direct_c(
    code: dict[int, tuple[str, str, int]],
    symbols: dict[str, int],
    tables: dict[int, list[int]],
    bodies: dict[int, set[int]],
    data_report: dict[str, object] | None = None,
    saved_layouts: dict[int, SavedValueLayout] | None = None,
) -> str:
    names = direct_function_names(symbols, bodies)
    semantic_routines = verified_native_semantics(code, bodies)
    semantic_slices = verified_native_semantic_slices(code, bodies)
    tbljmp = symbols.get("tbljmp")
    reset_txs = {
        address for address, (op, _, _) in code.items()
        if op == "TXS" and address == symbols.get("start", -1) + 9
    }
    if saved_layouts is None:
        saved_layouts, computed_report = saved_value_layouts(
            code, bodies, tbljmp, reset_txs,
            {symbols["irq"]} if "irq" in symbols else set(),
        )
        data_report = computed_report
    if data_report is None:
        raise ValueError("saved-value analysis report is required")
    fatal_report_keys = (
        "tsx_txs", "possible_data_underflow", "ambiguous_depths",
        "unbalanced_exits",
    )
    failures = [
        f"{key}={data_report[key]}" for key in fatal_report_keys
        if data_report.get(key)
    ]
    if failures:
        raise ValueError(
            "cannot lower processor saves to C locals: " + "; ".join(failures)
        )
    lines = [
        "/* Generated direct-C migration backend; no opcode dispatcher. */",
        '#include "smbneo_native.h"',
        "#ifdef SMBNEO_NATIVE_FAST_PATHS",
        *[f'#include "{header}"' for header in sorted(
            {routine.header for routine in semantic_routines.values()} |
            {semantic.header for semantic in semantic_slices.values()}
        )],
        "#endif",
        "#include <stddef.h>",
        "#include <string.h>",
        "",
        "#ifndef SMBNEO_NATIVE_FORCE_INLINE",
        "#if defined(__GNUC__) || defined(__clang__)",
        "#define SMBNEO_NATIVE_FORCE_INLINE static inline __attribute__((always_inline))",
        "#else",
        "#define SMBNEO_NATIVE_FORCE_INLINE static inline",
        "#endif",
        "#endif",
        "",
        "#ifdef SMBNEO_NATIVE_DIAGNOSTICS",
        "#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (ctx)->calls++; (ctx)->last_symbol = (symbol); } while (0)",
        "#define SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx) do { (ctx)->calls = 0; (ctx)->last_symbol = 0; } while (0)",
        "#else",
        "#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (void)(ctx); (void)(symbol); } while (0)",
        "#define SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx) do { (void)(ctx); } while (0)",
        "#endif",
        "",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_read(SmbNeoNativeContext *ctx, uint16_t address) {",
        "    if (address < 0x2000u) {",
        "        if (ctx->ram == NULL) { ctx->unsupported = 1; return 0; }",
        "        return ctx->ram[address & 0x07ffu];",
        "    }",
        "    if (address >= 0x6000u) {",
        "        if (address < 0x8000u) {",
        "            if (ctx->extra_ram == NULL) { ctx->unsupported = 1; return 0; }",
        "            return ctx->extra_ram[address & 0x07ffu];",
        "        }",
        "        if (ctx->prg == NULL) { ctx->unsupported = 1; return 0; }",
        "        return ctx->prg[address - 0x8000u];",
        "    }",
        "    if (ctx->read8 == NULL) { ctx->unsupported = 1; return 0; }",
        "    return ctx->read8(ctx->opaque, address);",
        "}",
        "SMBNEO_NATIVE_FORCE_INLINE void native_write(SmbNeoNativeContext *ctx, uint16_t address, uint8_t value) {",
        "    if (address < 0x2000u) {",
        "        if (ctx->ram == NULL) { ctx->unsupported = 1; return; }",
        "        ctx->ram[address & 0x07ffu] = value;",
        "        return;",
        "    }",
        "    if (address >= 0x6000u) {",
        "        if (address < 0x8000u) {",
        "            if (ctx->extra_ram == NULL) { ctx->unsupported = 1; return; }",
        "            ctx->extra_ram[address & 0x07ffu] = value;",
        "        }",
        "        /* Program data is immutable; cartridge writes are ignored. */",
        "        return;",
        "    }",
        "    if (ctx->write8 == NULL) { ctx->unsupported = 1; return; }",
        "    ctx->write8(ctx->opaque, address, value);",
        "}",
        "SMBNEO_NATIVE_FORCE_INLINE uint16_t native_zpword(SmbNeoNativeContext *ctx, uint8_t address) { return (uint16_t)(native_read(ctx, address) | ((uint16_t)native_read(ctx, (uint8_t)(address + 1u)) << 8)); }",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_nz(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) | (value & NATIVE_N) | (value == 0 ? NATIVE_Z : 0)); return value; }",
        "SMBNEO_NATIVE_FORCE_INLINE void native_cmp(SmbNeoNativeContext *ctx, uint8_t lhs, uint8_t rhs) { uint8_t result = (uint8_t)(lhs - rhs); ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) | (lhs >= rhs ? NATIVE_C : 0) | (result & NATIVE_N) | (result == 0 ? NATIVE_Z : 0)); }",
        "SMBNEO_NATIVE_FORCE_INLINE void native_bit(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_Z | NATIVE_V | NATIVE_N)) | ((ctx->a & value) == 0 ? NATIVE_Z : 0) | (value & (NATIVE_V | NATIVE_N))); }",
        "SMBNEO_NATIVE_FORCE_INLINE void native_adc(SmbNeoNativeContext *ctx, uint8_t value) { unsigned sum = ctx->a + value + (ctx->status & NATIVE_C ? 1u : 0u); uint8_t result = (uint8_t)sum; ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) | (sum > 255u ? NATIVE_C : 0) | ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) ? NATIVE_V : 0)); ctx->a = native_nz(ctx, result); }",
        "SMBNEO_NATIVE_FORCE_INLINE void native_sbc(SmbNeoNativeContext *ctx, uint8_t value) { native_adc(ctx, (uint8_t)~value); }",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_asl(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value >> 7 ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)(value << 1)); }",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_lsr(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value & 1u ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)(value >> 1)); }",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_rol(SmbNeoNativeContext *ctx, uint8_t value) { uint8_t old = (ctx->status & NATIVE_C) ? 1u : 0u; ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value >> 7 ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)((value << 1) | old)); }",
        "SMBNEO_NATIVE_FORCE_INLINE uint8_t native_ror(SmbNeoNativeContext *ctx, uint8_t value) { uint8_t old = (ctx->status & NATIVE_C) ? 0x80u : 0u; ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value & 1u ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)((value >> 1) | old)); }",
        "",
    ]
    for entry in sorted(bodies):
        lines.append(f"void {names[entry]}(SmbNeoNativeContext *ctx);")
    lines.append("")

    def transfer(target: int, current: int, current_body: set[int]) -> list[str]:
        if target in current_body:
            return [f"goto L{target:04x};"]
        if target in bodies:
            return [f"{names[target]}(ctx);", "return;"]
        return ["ctx->unsupported = 1; return;"]

    for entry in sorted(bodies):
        body = bodies[entry]
        layout = saved_layouts[entry]
        if tbljmp is not None and entry == tbljmp:
            lines += [
                f"void {names[entry]}(SmbNeoNativeContext *ctx) {{",
                f"    SMBNEO_NATIVE_MARK(ctx, 0x{entry:04x}u);",
                "    /* All table selections are statically expanded at their call sites. */",
                "    ctx->unsupported = 1;",
                "    return;",
                "}",
                "",
            ]
            continue
        label_targets: set[int] = set()
        for address in body:
            op, mode, operand = code[address]
            nxt = address + _LENGTH[mode]
            if mode == "r":
                delta = operand if operand < 128 else operand - 256
                target = (nxt + delta) & 0xffff
                if target in body:
                    label_targets.add(target)
            elif op == "JMP" and mode == "w" and operand != address and operand in body:
                label_targets.add(operand)
        semantic_only_labels: set[int] = set()
        for (owner, _), semantic_slice in semantic_slices.items():
            if owner == entry:
                # A continuation that is also the public function entry is
                # reached by the unconditional entry trampoline when the
                # disassembly body begins earlier.  Keep that label available
                # in non-fast-path test builds as well.
                if semantic_slice.continuation != entry and semantic_slice.continuation not in label_targets:
                    semantic_only_labels.add(semantic_slice.continuation)
                label_targets.add(semantic_slice.continuation)
        if min(body) != entry:
            label_targets.add(entry)
        lines += [f"void {names[entry]}(SmbNeoNativeContext *ctx) {{", f"    SMBNEO_NATIVE_MARK(ctx, 0x{entry:04x}u);"]
        semantic = semantic_routines.get(entry)
        if semantic is not None:
            lines.append("#ifdef SMBNEO_NATIVE_FAST_PATHS")
            if semantic.conditional:
                lines.append(f"    if ({semantic.function}(ctx)) return;")
            else:
                lines += [f"    {semantic.function}(ctx);", "    return;"]
            lines.append("#endif")
        for slot in range(layout.local_count):
            lines.append(f"    uint8_t saved_value_{slot} = 0u;")
        if min(body) != entry:
            lines.append(f"    goto L{entry:04x};")
        for address in sorted(body):
            op, mode, operand = code[address]
            nxt = address + _LENGTH[mode]
            if address in label_targets:
                # A C label must label a statement; the empty statement also
                # keeps declarations immediately after a branch legal.
                if address in semantic_only_labels:
                    lines += [
                        "#ifdef SMBNEO_NATIVE_FAST_PATHS",
                        f"L{address:04x}: ;",
                        "#endif",
                    ]
                else:
                    lines.append(f"L{address:04x}: ;")
            semantic_slice = semantic_slices.get((entry, address))
            if semantic_slice is not None:
                arguments = ", ".join(semantic_slice.arguments)
                lines.append("#ifdef SMBNEO_NATIVE_FAST_PATHS")
                if semantic_slice.conditional:
                    lines.append(
                        f"    if ({semantic_slice.function}({arguments})) "
                        f"goto L{semantic_slice.continuation:04x};"
                    )
                else:
                    lines += [
                        f"    {semantic_slice.function}({arguments});",
                        f"    goto L{semantic_slice.continuation:04x};",
                    ]
                lines.append("#endif")
            if op in ("RTS", "RTI"):
                lines.append("    return;")
                continue
            if op == "BRK":
                lines.append("    ctx->suspended = 1; return;")
                continue
            if op == "JSR":
                if tbljmp is not None and operand == tbljmp:
                    targets = tables.get(address)
                    if not targets:
                        lines.append("    ctx->unsupported = 1; return;")
                    else:
                        # Reproduce tbljmp's observable register/ZP effects
                        # without materializing its consumed JSR frame.  The
                        # table pointer is call+2 and the selected word is
                        # already verified from an explicit .addr span.
                        lines += [
                            f"    uint8_t table_selector_{address:04x} = ctx->a;",
                            f"    ctx->a = native_asl(ctx, ctx->a);",
                            "    ctx->y = native_nz(ctx, ctx->a);",
                            f"    native_write(ctx, 0x04u, 0x{(address + 2) & 0xff:02x}u);",
                            f"    native_write(ctx, 0x05u, 0x{((address + 2) >> 8) & 0xff:02x}u);",
                            "    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));",
                            "    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));",
                        ]
                        lines.append(f"    switch (table_selector_{address:04x}) {{")
                        for index, target in enumerate(targets):
                            lines += [
                                f"    case {index}: native_write(ctx, 0x06u, 0x{target & 0xff:02x}u);",
                                f"             native_write(ctx, 0x07u, 0x{target >> 8:02x}u);",
                                f"             ctx->a = native_nz(ctx, 0x{target >> 8:02x}u);",
                                f"             {names.get(target, 'ctx->unsupported = 1;')}(ctx); return;" if target in names else "             ctx->unsupported = 1; return;",
                            ]
                        lines.append("    default: ctx->unsupported = 1; return;")
                        lines.append("    }")
                    continue
                if operand in bodies:
                    lines += [f"    {names[operand]}(ctx);", "    if (ctx->suspended || ctx->unsupported) return;"]
                else:
                    lines += ["    ctx->unsupported = 1; return;"]
            elif op == "JMP" and mode == "w":
                if operand == address:
                    lines.append("    ctx->suspended = 1; return;")
                else:
                    lines += [f"    /* static tail transfer */"] + [f"    {line}" for line in transfer(operand, entry, body)]
                    continue
            elif op in ("BPL", "BMI", "BVC", "BVS", "BCC", "BCS", "BNE", "BEQ"):
                delta = operand if operand < 128 else operand - 256
                target = (nxt + delta) & 0xffff
                lines.append(f"    if ({_branch_condition(op)}) {{")
                lines += [f"        {line}" for line in transfer(target, entry, body)]
                lines.append("    }")
            elif op == "JMP" and mode == "ind":
                lines.append("    ctx->unsupported = 1; return;")
            elif op == "TXS" and address in reset_txs:
                # Reset's sole TXS initializes processor-only state. Native C
                # call storage is already initialized by the language runtime.
                lines.append("    /* reset-only processor state eliminated */")
            elif op in ("PHA", "PHP", "PLA", "PLP"):
                if address in layout.frame_discards:
                    lines.append(
                        "    /* processor-created interrupt frame absent in native C */"
                    )
                else:
                    depth = layout.depth_at[address]
                    if op == "PHA":
                        lines.append(f"    saved_value_{depth} = ctx->a;")
                    elif op == "PHP":
                        lines.append(
                            f"    saved_value_{depth} = (uint8_t)(ctx->status | NATIVE_B | NATIVE_U);"
                        )
                    elif op == "PLA":
                        lines.append(
                            f"    ctx->a = native_nz(ctx, saved_value_{depth - 1});"
                        )
                    else:
                        lines.append(
                            f"    ctx->status = (uint8_t)((saved_value_{depth - 1} | NATIVE_U) & (uint8_t)~NATIVE_B);"
                        )
            else:
                try:
                    lines += [f"    {line}" for line in _emit_simple_op(address, op, mode, operand)]
                except ValueError:
                    lines.append("    ctx->unsupported = 1; return;")
            if nxt in body:
                continue
            lines += [f"    {line}" for line in transfer(nxt, entry, body)]
        lines += ["    return;", "}", ""]

    # Host-facing entry points are deliberately tiny and contain no hidden
    # scheduler state.  A caller owns the frame cadence and can inspect the
    # suspended/unsupported migration flags.
    start = names.get(symbols.get("start"))
    nmi = names.get(symbols.get("nmi"))
    sys_title = names.get(symbols.get("sys_title"))
    sys_game = names.get(symbols.get("sys_game"))
    lines += [
        "#if defined(__GNUC__) && !defined(__clang__)",
        "#define SMBNEO_NATIVE_LINK_API __attribute__((noinline, used, externally_visible))",
        "#elif defined(__clang__)",
        "#define SMBNEO_NATIVE_LINK_API __attribute__((noinline, used))",
        "#else",
        "#define SMBNEO_NATIVE_LINK_API",
        "#endif",
        "SMBNEO_NATIVE_LINK_API void smbneo_native_boot(SmbNeoNativeContext *ctx) { ctx->a = 0; ctx->x = 0; ctx->y = 0; ctx->status = NATIVE_U | NATIVE_I; ctx->suspended = 0; ctx->unsupported = 0; SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx);",
        f"    {start}(ctx);" if start else "    ctx->unsupported = 1;",
        "}",
        "SMBNEO_NATIVE_LINK_API void smbneo_native_frame(SmbNeoNativeContext *ctx) { uint8_t interrupted_status = ctx->status;",
        f"    {nmi}(ctx);" if nmi else "    ctx->unsupported = 1;",
        "    ctx->status = (uint8_t)((interrupted_status | NATIVE_U) & (uint8_t)~NATIVE_B);",
        "}",
        "SMBNEO_NATIVE_LINK_API void smbneo_native_title_tick(SmbNeoNativeContext *ctx) {",
        f"    {sys_title}(ctx);" if sys_title else "    ctx->unsupported = 1;",
        "}",
        "SMBNEO_NATIVE_LINK_API void smbneo_native_game_tick(SmbNeoNativeContext *ctx) {",
        f"    {sys_game}(ctx);" if sys_game else "    ctx->unsupported = 1;",
        "}",
        "#undef SMBNEO_NATIVE_LINK_API",
        "",
        "/* Saved-value analysis is compile-time only. */",
        f"/* max_local_values={data_report['max_local_values']} */",
    ]
    return "\n".join(lines)


def generate(prg_path: Path, debug_path: Path, source_root: Path) -> tuple[str, dict[str, object]]:
    prg = load_prg(prg_path)
    debug = debug_path.read_text(encoding="latin1")
    source = source_manifest(source_root)
    symbols = parse_symbols(debug)
    missing = [name for name in REQUIRED_SYMBOLS if name not in symbols]
    if missing:
        raise ValueError("debug map is missing required symbols: " + ", ".join(missing))
    code = decode_marked(debug, source_root, prg)
    graph = call_graph(code, symbols, REQUIRED_SYMBOLS)
    table_entry = symbols.get("tbljmp")
    tables = ordered_tables(debug, source_root, prg, code, table_entry) if table_entry is not None else {}
    entries = direct_entries(code, tables, symbols)
    bodies, orphans = direct_bodies(code, entries)
    reset_txs = {
        address for address, (op, _, _) in code.items()
        if op == "TXS" and address == symbols.get("start", -1) + 9
    }
    hardware_frame_entries = {symbols["irq"]} if "irq" in symbols else set()
    layouts, data_report = saved_value_layouts(
        code, bodies, table_entry, reset_txs, hardware_frame_entries
    )
    semantic_routines = verified_native_semantics(code, bodies)
    semantic_slices = verified_native_semantic_slices(code, bodies)
    direct_source = emit_direct_c(
        code, symbols, tables, bodies, data_report, layouts
    )
    manifest = {
        "backend": "smbneo-direct-c-v1",
        "source": source,
        "prg_size": len(prg),
        "symbols": {name: symbols[name] for name in REQUIRED_SYMBOLS},
        "direct_call_graph": graph,
        "marked_instruction_count": len(code),
        "function_entry_count": len(bodies),
        "covered_instruction_count": len(set().union(*bodies.values()) if bodies else set()),
        "synthetic_region_entries": [f"0x{entry:04x}" for entry in orphans],
        "table_call_count": len(tables),
        "semantic_routines": {
            f"0x{entry:04x}": routine.function
            for entry, routine in sorted(semantic_routines.items())
        },
        "semantic_slices": {
            f"0x{owner:04x}:0x{start:04x}": {
                "function": semantic.function,
                "continuation": f"0x{semantic.continuation:04x}",
                "conditional": semantic.conditional,
            }
            for (owner, start), semantic in sorted(semantic_slices.items())
        },
        "saved_value_report": data_report,
        "runtime_contract": {
            "context": "SmbNeoNativeContext",
            "dispatch": "C switch and static labels",
            "returns": "ordinary C return",
            "rom_data": "not embedded",
            "migration_registers": "A/X/Y/status; balanced saves are scalar C locals",
            "forbidden": ["runtime PC", "fuel", "page dispatcher", "emulated call-return stack"],
        },
    }
    return emit_direct_header(), manifest | {"c_source": direct_source}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prg", type=Path, required=True)
    parser.add_argument("--debug", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    header, result = generate(args.prg, args.debug, args.source_root)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "smbneo_native.h").write_text(header, encoding="utf-8")
    (args.output_dir / "smbneo_native.c").write_text(result.pop("c_source"), encoding="utf-8")
    (args.output_dir / "manifest.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"generated direct-C program in {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
