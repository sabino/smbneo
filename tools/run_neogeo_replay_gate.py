#!/usr/bin/env python3
"""Run a Neo Geo replay-gate cartridge and capture bounded evidence."""

from __future__ import annotations

import argparse
from contextlib import ExitStack
from dataclasses import asdict, dataclass
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from typing import Sequence

import measure_neogeo_cadence as cadence


DEBUG_HOST = cadence.DEBUG_HOST
DEBUG_PORT = cadence.DEBUG_PORT
STATUS_MAGIC = 0x534D4252
STATUS_VERSION = 1
STATUS_WORD_COUNT = 20
STATUS_BYTES = STATUS_WORD_COUNT * 4
COMPLETE_RESULT = 1
INCOMPLETE_RESULT = 0x100
ALL_STAGES_MASK = 0xFFFFFFFF
RESULT_NAMES = {
    0: "running",
    1: "complete",
    2: "invalid_stage",
    3: "skipped_stage",
    4: "backwards_stage",
    5: "game_over",
    6: "returned_to_title",
    INCOMPLETE_RESULT: "incomplete",
}
RESULT_SCHEMA_VERSION = 1
DEFAULT_ROM_SET = "smbneogeo"
DEFAULT_68K_OVERCLOCK = 1000
MIN_DERIVED_TIMEOUT_SECONDS = 180.0
BASELINE_TIMEOUT_SECONDS = 1800.0
MAX_TIMEOUT_SECONDS = 86400.0
MAX_68K_OVERCLOCK = 10000
MAX_DIAGNOSTIC_CHARS = 4096
MAX_RESULT_JSON_BYTES = 65536
DEFAULT_HEARTBEAT_SECONDS = 5.0
DEFAULT_STARTUP_TIMEOUT_SECONDS = 10.0

REQUIRED_SYMBOL_NAMES = (
    "neogeo_replay_pass_trap",
    "neogeo_replay_fail_trap",
    "neogeo_replay_progress_trap",
    "neogeo_replay_status",
)
ROM_SET_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$")
TRAP_RE = re.compile(
    r"^REPLAY_TRAP\s+"
    r"kind=(?P<kind>pass|fail)\s+"
    r"pc=0x(?P<pc>[0-9a-fA-F]{1,8})\s*$"
)
WORD_RE = re.compile(
    r"^REPLAY_WORD\s+"
    r"index=(?P<index>[0-9]+)\s+"
    r"value=0x(?P<value>[0-9a-fA-F]{8})\s*$"
)
PROGRESS_WORD_RE = re.compile(
    r"^REPLAY_PROGRESS_WORD\s+"
    r"sample=(?P<sample>[0-9]+)\s+"
    r"index=(?P<index>[0-9]+)\s+"
    r"value=0x(?P<value>[0-9a-fA-F]{8})\s*$"
)
PROGRESS_END_RE = re.compile(
    r"^REPLAY_PROGRESS_END\s+sample=(?P<sample>[0-9]+)\s*$"
)


class ReplayGateError(RuntimeError):
    """Raised when replay-gate evidence cannot be trusted."""


class ReplayGateTimeout(ReplayGateError):
    """Raised when a finite replay-gate deadline expires."""


ElfSymbol = cadence.ElfSymbol


@dataclass(frozen=True)
class ReplaySymbols:
    pass_trap: ElfSymbol
    fail_trap: ElfSymbol
    status: ElfSymbol
    progress_point: ElfSymbol


@dataclass(frozen=True)
class ReplayStatus:
    magic: int
    version: int
    result: int
    frame: int
    tail_frame: int
    segment_index: int
    controller_state: int
    entered_mask: int
    completed_mask: int
    current_stage: int
    victory_stable_frames: int
    oper_mode: int
    oper_mode_task: int
    world: int
    level: int
    world_end_timer: int
    replay_end_frame: int
    hardware_playable: int
    opposite_direction_transitions: int
    ram_init_option: int


@dataclass(frozen=True)
class DebuggerCapture:
    trap_kind: str
    trap_pc: int
    status: ReplayStatus
    raw_words: tuple[int, ...]


@dataclass(frozen=True)
class ProgressCapture:
    sample: int
    status: ReplayStatus
    raw_words: tuple[int, ...]


@dataclass(frozen=True)
class GateClassification:
    outcome: str
    passed: bool
    detail: str


def parse_nm_symbols(output: str) -> dict[str, ElfSymbol]:
    """Expose the shared strict POSIX ``nm -S`` parser."""

    return cadence.parse_nm_symbols(output)


def replay_symbols_from_nm(output: str) -> ReplaySymbols:
    """Validate the four ELF symbols used by the remote debugger."""

    try:
        parsed = parse_nm_symbols(output)
    except cadence.CadenceError as error:
        raise ReplayGateError(str(error)) from error

    missing = [name for name in REQUIRED_SYMBOL_NAMES if name not in parsed]
    if missing:
        raise ReplayGateError(
            "ELF is missing replay-gate symbols: " + ", ".join(missing)
        )

    pass_trap = parsed["neogeo_replay_pass_trap"]
    fail_trap = parsed["neogeo_replay_fail_trap"]
    status = parsed["neogeo_replay_status"]
    progress_point = parsed["neogeo_replay_progress_trap"]

    for trap in (pass_trap, fail_trap):
        if trap.kind not in "TtWw" or trap.size <= 0:
            raise ReplayGateError(
                f"{trap.name} is not a non-empty code symbol "
                f"(nm type {trap.kind}, size {trap.size})"
            )
    if pass_trap.address == fail_trap.address:
        raise ReplayGateError("pass and fail traps resolve to the same address")

    if status.size != STATUS_BYTES:
        raise ReplayGateError(
            f"{status.name} is {status.size} bytes; expected exactly "
            f"{STATUS_BYTES}"
        )
    if status.address % 4 != 0:
        raise ReplayGateError(
            f"{status.name} address 0x{status.address:08x} is not "
            "32-bit aligned"
        )
    if status.kind in "UuTtWw":
        raise ReplayGateError(
            f"{status.name} is not a defined data object "
            f"(nm type {status.kind})"
        )
    if progress_point.kind not in "TtWw" or progress_point.size <= 0:
        raise ReplayGateError(
            f"{progress_point.name} is not a non-empty code symbol "
            f"(nm type {progress_point.kind}, size {progress_point.size})"
        )

    return ReplaySymbols(
        pass_trap=pass_trap,
        fail_trap=fail_trap,
        status=status,
        progress_point=progress_point,
    )


def resolve_replay_symbols(
    elf: Path,
    nm_executable: str = "m68k-neogeo-elf-nm",
) -> ReplaySymbols:
    completed = subprocess.run(
        [
            nm_executable,
            "-n",
            "-S",
            "--defined-only",
            str(elf),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise ReplayGateError(
            f"{nm_executable} failed with status {completed.returncode}:\n"
            f"{_bounded_text(completed.stdout)}"
        )
    return replay_symbols_from_nm(completed.stdout)


def _gdb_address(address: int) -> str:
    return f"0x{address:08x}"


def build_gdb_script(
    symbols: ReplaySymbols,
    host: str = DEBUG_HOST,
    port: int = DEBUG_PORT,
) -> str:
    """Build a trap probe with exactly twenty raw 32-bit mailbox reads."""

    lines = [
        "set pagination off",
        "set confirm off",
        "set verbose off",
        "set remotetimeout 60",
        "set $gate_trap = 0",
        "set $progress_sample = 0",
        f"target remote {host}:{port}",
        f"break *{_gdb_address(symbols.pass_trap.address)}",
        "commands 1",
        "  silent",
        "  set $gate_trap = 1",
        "end",
        f"break *{_gdb_address(symbols.fail_trap.address)}",
        "commands 2",
        "  silent",
        "  set $gate_trap = 2",
        "end",
        f"break *{_gdb_address(symbols.progress_point.address)}",
        "commands 3",
        "  silent",
        "  set $progress_sample = $progress_sample + 1",
    ]

    for index in range(STATUS_WORD_COUNT):
        address = symbols.status.address + index * 4
        lines.extend(
            [
                (
                    f"  set $progress_mb{index} = "
                    f"*(unsigned int *){_gdb_address(address)}"
                ),
                (
                    '  printf "REPLAY_PROGRESS_WORD '
                    f'sample=%u index={index} value=0x%08x\\n", '
                    f"$progress_sample, $progress_mb{index}"
                ),
            ]
        )

    lines.extend(
        [
            (
                '  printf "REPLAY_PROGRESS_END sample=%u\\n", '
                "$progress_sample"
            ),
            "  continue",
            "end",
            "continue",
            "set $trap_pc = (unsigned int)$pc",
            "if $gate_trap == 1",
            (
                '  printf "REPLAY_TRAP kind=pass pc=0x%08x\\n", '
                "$trap_pc"
            ),
            "else",
            (
                '  printf "REPLAY_TRAP kind=fail pc=0x%08x\\n", '
                "$trap_pc"
            ),
            "end",
        ]
    )

    for index in range(STATUS_WORD_COUNT):
        address = symbols.status.address + index * 4
        lines.extend(
            [
                (
                    f"set $mb{index} = "
                    f"*(unsigned int *){_gdb_address(address)}"
                ),
                (
                    'printf "REPLAY_WORD '
                    f"index={index} value=0x%08x\\n"
                    f'", $mb{index}'
                ),
            ]
        )

    lines.extend(["detach", "quit"])
    return "\n".join(lines) + "\n"


def parse_mailbox_words(words: Sequence[int]) -> ReplayStatus:
    """Parse and validate one complete 80-byte debugger mailbox snapshot."""

    if len(words) != STATUS_WORD_COUNT:
        raise ReplayGateError(
            f"mailbox has {len(words)} words; expected {STATUS_WORD_COUNT}"
        )
    normalized: list[int] = []
    for index, value in enumerate(words):
        if not isinstance(value, int) or not 0 <= value <= 0xFFFFFFFF:
            raise ReplayGateError(
                f"mailbox word {index} is outside the uint32 range"
            )
        normalized.append(value)

    status = ReplayStatus(*normalized)
    if status.magic != STATUS_MAGIC:
        raise ReplayGateError(
            f"mailbox magic is 0x{status.magic:08x}; "
            f"expected 0x{STATUS_MAGIC:08x}"
        )
    if status.version != STATUS_VERSION:
        raise ReplayGateError(
            f"mailbox version is {status.version}; "
            f"expected {STATUS_VERSION}"
        )
    return status


def parse_gdb_output(
    output: str,
    symbols: ReplaySymbols,
) -> DebuggerCapture:
    """Extract exactly one trap and one value for every mailbox word."""

    traps: list[tuple[str, int]] = []
    words: dict[int, int] = {}
    for line in output.splitlines():
        stripped = line.strip()
        trap_match = TRAP_RE.match(stripped)
        if trap_match is not None:
            traps.append(
                (
                    trap_match.group("kind"),
                    int(trap_match.group("pc"), 16),
                )
            )
            continue
        word_match = WORD_RE.match(stripped)
        if word_match is None:
            continue
        index = int(word_match.group("index"))
        if index >= STATUS_WORD_COUNT:
            raise ReplayGateError(
                f"GDB reported out-of-range mailbox word {index}"
            )
        if index in words:
            raise ReplayGateError(
                f"GDB reported mailbox word {index} more than once"
            )
        words[index] = int(word_match.group("value"), 16)

    if len(traps) != 1:
        raise ReplayGateError(
            f"GDB reported {len(traps)} replay traps; expected exactly one"
        )
    missing = [
        str(index)
        for index in range(STATUS_WORD_COUNT)
        if index not in words
    ]
    if missing:
        raise ReplayGateError(
            "GDB output is missing mailbox words: " + ", ".join(missing)
        )

    trap_kind, trap_pc = traps[0]
    expected_pc = (
        symbols.pass_trap.address
        if trap_kind == "pass"
        else symbols.fail_trap.address
    )
    if trap_pc != expected_pc:
        raise ReplayGateError(
            f"{trap_kind} trap stopped at 0x{trap_pc:08x}; "
            f"expected 0x{expected_pc:08x}"
        )

    raw_words = tuple(words[index] for index in range(STATUS_WORD_COUNT))
    return DebuggerCapture(
        trap_kind=trap_kind,
        trap_pc=trap_pc,
        status=parse_mailbox_words(raw_words),
        raw_words=raw_words,
    )


def parse_progress_snapshots(output: str) -> list[ProgressCapture]:
    """Parse only complete, validated intermediate mailbox snapshots."""

    samples: dict[int, dict[int, int]] = {}
    completed_samples: set[int] = set()
    for line in output.splitlines():
        stripped = line.strip()
        word_match = PROGRESS_WORD_RE.match(stripped)
        if word_match is not None:
            sample = int(word_match.group("sample"))
            index = int(word_match.group("index"))
            if sample <= 0:
                raise ReplayGateError(
                    "GDB reported a non-positive progress sample"
                )
            if index >= STATUS_WORD_COUNT:
                raise ReplayGateError(
                    f"GDB reported out-of-range progress word {index}"
                )
            sample_words = samples.setdefault(sample, {})
            if index in sample_words:
                raise ReplayGateError(
                    f"GDB reported progress sample {sample} word {index} "
                    "more than once"
                )
            sample_words[index] = int(word_match.group("value"), 16)
            continue

        end_match = PROGRESS_END_RE.match(stripped)
        if end_match is not None:
            sample = int(end_match.group("sample"))
            if sample in completed_samples:
                raise ReplayGateError(
                    f"GDB ended progress sample {sample} more than once"
                )
            completed_samples.add(sample)

    captures: list[ProgressCapture] = []
    previous_frame = -1
    for sample in sorted(completed_samples):
        sample_words = samples.get(sample, {})
        missing = [
            str(index)
            for index in range(STATUS_WORD_COUNT)
            if index not in sample_words
        ]
        if missing:
            raise ReplayGateError(
                f"progress sample {sample} is missing mailbox words: "
                + ", ".join(missing)
            )
        raw_words = tuple(
            sample_words[index] for index in range(STATUS_WORD_COUNT)
        )
        status = parse_mailbox_words(raw_words)
        if status.frame < previous_frame:
            raise ReplayGateError(
                f"progress frame moved backwards at sample {sample}"
            )
        captures.append(
            ProgressCapture(
                sample=sample,
                status=status,
                raw_words=raw_words,
            )
        )
        previous_frame = status.frame
    return captures


def _status_summary(status: ReplayStatus) -> str:
    result_name = RESULT_NAMES.get(status.result, "unknown")
    return (
        f"result={result_name}(0x{status.result:08x}) frame={status.frame} "
        f"entered=0x{status.entered_mask:08x} "
        f"completed=0x{status.completed_mask:08x} "
        f"stage={status.current_stage} world={status.world + 1} "
        f"level={status.level + 1}"
    )


def classify_result(capture: DebuggerCapture) -> GateClassification:
    """Classify a validated trap/mailbox pair without optimistic inference."""

    status = capture.status
    gate_complete = (
        status.result == COMPLETE_RESULT
        and status.entered_mask == ALL_STAGES_MASK
        and status.completed_mask == ALL_STAGES_MASK
    )
    summary = _status_summary(status)
    if capture.trap_kind == "pass" and gate_complete:
        return GateClassification(
            outcome="complete",
            passed=True,
            detail=f"pass trap reached; {summary}",
        )

    if status.result in (0, INCOMPLETE_RESULT):
        outcome = "incomplete"
    else:
        outcome = "failure"

    if gate_complete:
        detail = (
            "complete mailbox reached the fail trap instead of the pass trap; "
            + summary
        )
    elif capture.trap_kind == "pass":
        detail = (
            "pass trap reached without a complete mailbox; " + summary
        )
    else:
        detail = f"fail trap reached; {summary}"
    return GateClassification(
        outcome=outcome,
        passed=False,
        detail=detail,
    )


def derive_gate_timeout(
    requested_timeout: float | None,
    m68k_overclock: int,
) -> float:
    """Return an explicit or conservative finite replay deadline."""

    if not 0 <= m68k_overclock <= MAX_68K_OVERCLOCK:
        raise ReplayGateError(
            f"68k overclock must be between 0 and {MAX_68K_OVERCLOCK}"
        )
    if requested_timeout is not None:
        if (
            not math.isfinite(requested_timeout)
            or requested_timeout <= 0
            or requested_timeout > MAX_TIMEOUT_SECONDS
        ):
            raise ReplayGateError(
                "timeout must be finite, positive, and no more than "
                f"{MAX_TIMEOUT_SECONDS:g} seconds"
            )
        return float(requested_timeout)

    speed_multiplier = 1.0 + m68k_overclock / 100.0
    return max(
        MIN_DERIVED_TIMEOUT_SECONDS,
        BASELINE_TIMEOUT_SECONDS / speed_multiplier,
    )


def build_gngeo_command(
    executable: str,
    rom_dir: Path,
    data_file: Path,
    rom_set: str,
    m68k_overclock: int,
) -> list[str]:
    return [
        executable,
        "-b",
        "soft",
        "--screen320",
        "--scale=1",
        "--no-resize",
        "--no-sound",
        "--no-autoframeskip",
        "--no-vsync",
        "--no-sleepidle",
        f"--68kclock={m68k_overclock}",
        "--system",
        "home",
        "-D",
        "-i",
        str(rom_dir),
        "-d",
        str(data_file),
        rom_set,
    ]


def build_gdb_command(
    executable: str,
    elf: Path,
    script: Path,
) -> list[str]:
    return [executable, "-q", "-batch", str(elf), "-x", str(script)]


def _wait_for_gate(
    gdb: subprocess.Popen[str],
    gngeo: subprocess.Popen[str],
    heartbeat_seconds: float,
    timeout_seconds: float,
    gdb_log_path: Path,
) -> int:
    start = time.monotonic()
    next_heartbeat = start + heartbeat_seconds
    reported_samples = 0
    latest_progress: ProgressCapture | None = None
    while True:
        try:
            progress = parse_progress_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
        except FileNotFoundError:
            progress = []
        for snapshot in progress[reported_samples:]:
            print(
                "[replay-gate] progress "
                f"sample={snapshot.sample} "
                f"{_status_summary(snapshot.status)}",
                flush=True,
            )
            latest_progress = snapshot
        reported_samples = len(progress)

        status = gdb.poll()
        if status is not None:
            return status
        gngeo_status = gngeo.poll()
        if gngeo_status is not None:
            raise ReplayGateError(
                "GnGeo exited while the replay gate was running "
                f"(status {gngeo_status})"
            )
        now = time.monotonic()
        elapsed = now - start
        if elapsed >= timeout_seconds:
            raise ReplayGateTimeout(
                f"replay gate exceeded its {timeout_seconds:g}-second timeout"
            )
        if now >= next_heartbeat:
            progress_text = (
                _status_summary(latest_progress.status)
                if latest_progress is not None
                else "awaiting first mailbox snapshot"
            )
            print(
                "[replay-gate] still running "
                f"({int(elapsed)}s elapsed, "
                f"{max(0, int(timeout_seconds - elapsed))}s remaining; "
                f"{progress_text})",
                flush=True,
            )
            next_heartbeat = now + heartbeat_seconds
        time.sleep(min(0.25, heartbeat_seconds))


def _bounded_text(value: object, limit: int = MAX_DIAGNOSTIC_CHARS) -> str:
    text = str(value)
    if len(text) <= limit:
        return text
    return text[: limit - 19] + "\n...[truncated]..."


def _artifact_record(path: Path) -> dict[str, object]:
    return {
        "path": str(path),
        "bytes": path.stat().st_size,
        "sha256": cadence._sha256_file(path),
    }


def _tail_bounded(path: Path) -> str:
    return _bounded_text(cadence._tail(path, line_count=80))


def _create_evidence_directory(requested: Path | None) -> Path:
    if requested is None:
        return Path(tempfile.mkdtemp(prefix="smb-neogeo-replay-gate."))
    resolved = requested.expanduser().resolve()
    resolved.mkdir(parents=True, exist_ok=False)
    return resolved


def _write_result(path: Path, result: dict[str, object]) -> None:
    encoded = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode()
    if len(encoded) > MAX_RESULT_JSON_BYTES:
        result.pop("diagnostics", None)
        result["diagnostics_omitted"] = (
            "diagnostics exceeded the bounded evidence limit"
        )
        encoded = (
            json.dumps(result, indent=2, sort_keys=True) + "\n"
        ).encode()
    if len(encoded) > MAX_RESULT_JSON_BYTES:
        raise ReplayGateError(
            "result metadata exceeds the bounded evidence limit"
        )
    path.write_bytes(encoded)


def _progress_result(
    snapshots: Sequence[ProgressCapture],
) -> dict[str, object]:
    if not snapshots:
        return {
            "sample_count": 0,
            "latest": None,
        }
    latest = snapshots[-1]
    return {
        "sample_count": len(snapshots),
        "latest": {
            "sample": latest.sample,
            "raw_words": list(latest.raw_words),
            "fields": asdict(latest.status),
        },
    }


def _require_file(path: Path, description: str) -> Path:
    try:
        return cadence._require_file(path, description)
    except cadence.CadenceError as error:
        raise ReplayGateError(str(error)) from error


def _require_directory(path: Path, description: str) -> Path:
    try:
        return cadence._require_directory(path, description)
    except cadence.CadenceError as error:
        raise ReplayGateError(str(error)) from error


def _resolve_executable(value: str) -> str:
    try:
        return cadence._resolve_executable(value)
    except cadence.CadenceError as error:
        raise ReplayGateError(str(error)) from error


def _argument_record(
    args: argparse.Namespace,
    effective_timeout: float,
    evidence_dir: Path,
) -> dict[str, object]:
    return {
        "argv": list(getattr(args, "_argv", [])),
        "elf": str(Path(args.elf).expanduser().resolve()),
        "rom_dir": str(Path(args.rom_dir).expanduser().resolve()),
        "data_file": str(Path(args.data_file).expanduser().resolve()),
        "rom_set": args.rom_set,
        "timeout_requested_seconds": args.timeout,
        "timeout_effective_seconds": effective_timeout,
        "evidence_dir": str(evidence_dir),
        "m68k_overclock_percent": args.m68k_overclock,
        "heartbeat_seconds": args.heartbeat_seconds,
        "startup_timeout_seconds": args.startup_timeout,
        "gngeo": args.gngeo,
        "gdb": args.gdb,
        "nm": args.nm,
        "xvfb": args.xvfb,
    }


def _validate_runtime_arguments(args: argparse.Namespace) -> float:
    if not ROM_SET_RE.fullmatch(args.rom_set):
        raise ReplayGateError(
            "ROM set must contain 1-64 letters, digits, dots, underscores, "
            "or hyphens and must start with a letter or digit"
        )
    for label, value in (
        ("heartbeat interval", args.heartbeat_seconds),
        ("startup timeout", args.startup_timeout),
    ):
        if not math.isfinite(value) or value <= 0:
            raise ReplayGateError(f"{label} must be finite and positive")
    return derive_gate_timeout(args.timeout, args.m68k_overclock)


def run_probe(args: argparse.Namespace) -> int:
    effective_timeout = _validate_runtime_arguments(args)
    evidence_dir = _create_evidence_directory(args.evidence_dir)
    result_path = evidence_dir / "result.json"
    result: dict[str, object] = {
        "schema_version": RESULT_SCHEMA_VERSION,
        "outcome": "starting",
        "passed": False,
        "arguments": _argument_record(
            args,
            effective_timeout,
            evidence_dir,
        ),
        "artifacts": {},
    }
    _write_result(result_path, result)
    print(f"[replay-gate] evidence directory: {evidence_dir}", flush=True)

    owned = cadence.OwnedProcessGroups()
    gdb_log_path = evidence_dir / "gdb.log"
    gngeo_log_path = evidence_dir / "gngeo.log"
    xvfb_log_path = evidence_dir / "xvfb.log"

    try:
        elf = _require_file(args.elf, "replay-gate ELF")
        rom_dir = _require_directory(args.rom_dir, "ROM directory")
        data_file = _require_file(args.data_file, "GnGeo data file")
        cartridge = _require_file(
            rom_dir / f"{args.rom_set}.zip",
            "replay-gate cartridge",
        )

        nm_executable = _resolve_executable(args.nm)
        gdb_executable = _resolve_executable(args.gdb)
        gngeo_executable = _resolve_executable(args.gngeo)
        xvfb_executable = _resolve_executable(args.xvfb)
        symbols = resolve_replay_symbols(elf, nm_executable)

        result["artifacts"] = {
            "elf": _artifact_record(elf),
            "cartridge": _artifact_record(cartridge),
            "gngeo_data": _artifact_record(data_file),
        }
        result["symbols"] = {
            "pass_trap": asdict(symbols.pass_trap),
            "fail_trap": asdict(symbols.fail_trap),
            "status": asdict(symbols.status),
            "progress_point": asdict(symbols.progress_point),
        }
        result["outcome"] = "preflight"
        _write_result(result_path, result)

        cadence.reject_occupied_debug_port(DEBUG_HOST, DEBUG_PORT)
        script_path = evidence_dir / "replay-gate.gdb"
        script_path.write_text(build_gdb_script(symbols))

        with ExitStack() as stack:
            xvfb_log = stack.enter_context(
                xvfb_log_path.open("w", buffering=1)
            )
            gngeo_log = stack.enter_context(
                gngeo_log_path.open("w", buffering=1)
            )
            gdb_log = stack.enter_context(
                gdb_log_path.open("w", buffering=1)
            )

            _xvfb, display = cadence._start_xvfb(
                xvfb_executable,
                xvfb_log,
                owned,
                args.startup_timeout,
            )
            print(
                f"[replay-gate] owned Xvfb is ready on {display}",
                flush=True,
            )

            gngeo_command = build_gngeo_command(
                gngeo_executable,
                rom_dir,
                data_file,
                args.rom_set,
                args.m68k_overclock,
            )
            environment = os.environ.copy()
            environment["DISPLAY"] = display
            gngeo = owned.add(
                "GnGeo replay gate",
                subprocess.Popen(
                    gngeo_command,
                    stdout=gngeo_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=environment,
                    start_new_session=True,
                ),
            )
            cadence._wait_for_debug_listener(
                gngeo,
                args.startup_timeout,
            )
            print(
                "[replay-gate] owned GnGeo debugger is ready on "
                f"{DEBUG_HOST}:{DEBUG_PORT}",
                flush=True,
            )

            gdb_command = build_gdb_command(
                gdb_executable,
                elf,
                script_path,
            )
            result["commands"] = {
                "gngeo": gngeo_command,
                "gdb": gdb_command,
            }
            result["outcome"] = "running"
            _write_result(result_path, result)

            gdb = owned.add(
                "GDB replay gate",
                subprocess.Popen(
                    gdb_command,
                    stdout=gdb_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    start_new_session=True,
                ),
            )
            gdb_status = _wait_for_gate(
                gdb,
                gngeo,
                args.heartbeat_seconds,
                effective_timeout,
                gdb_log_path,
            )
            gdb_log.flush()
            if gdb_status != 0:
                raise ReplayGateError(
                    f"GDB failed with status {gdb_status}:\n"
                    f"{_tail_bounded(gdb_log_path)}"
                )

            capture = parse_gdb_output(
                gdb_log_path.read_text(errors="replace"),
                symbols,
            )
            progress = parse_progress_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
            classification = classify_result(capture)
            result.update(
                {
                    "outcome": classification.outcome,
                    "passed": classification.passed,
                    "detail": classification.detail,
                    "trap": {
                        "kind": capture.trap_kind,
                        "pc": capture.trap_pc,
                    },
                    "mailbox": {
                        "bytes": STATUS_BYTES,
                        "raw_words": list(capture.raw_words),
                        "fields": asdict(capture.status),
                    },
                    "progress": _progress_result(progress),
                }
            )
            _write_result(result_path, result)
            print(
                "[replay-gate] "
                f"outcome={classification.outcome} "
                f"trap={capture.trap_kind} "
                f"{_status_summary(capture.status)}",
                flush=True,
            )
            if classification.passed:
                print("REPLAY_GATE_COMPLETE passed", flush=True)
                return 0
            print(
                f"replay gate did not pass: {classification.detail}",
                file=sys.stderr,
                flush=True,
            )
            return 1
    except KeyboardInterrupt:
        progress: list[ProgressCapture] = []
        try:
            progress = parse_progress_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
        except (OSError, ReplayGateError):
            pass
        result.update(
            {
                "outcome": "interrupted",
                "passed": False,
                "error": "replay gate interrupted",
                "progress": _progress_result(progress),
                "diagnostics": {
                    "gdb_log_tail": _tail_bounded(gdb_log_path),
                    "gngeo_log_tail": _tail_bounded(gngeo_log_path),
                    "xvfb_log_tail": _tail_bounded(xvfb_log_path),
                },
            }
        )
        _write_result(result_path, result)
        raise
    except Exception as error:
        outcome = (
            "timeout"
            if isinstance(error, ReplayGateTimeout)
            else "error"
        )
        progress: list[ProgressCapture] = []
        try:
            progress = parse_progress_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
        except (OSError, ReplayGateError):
            pass
        result.update(
            {
                "outcome": outcome,
                "passed": False,
                "error": _bounded_text(error),
                "progress": _progress_result(progress),
                "diagnostics": {
                    "gdb_log_tail": _tail_bounded(gdb_log_path),
                    "gngeo_log_tail": _tail_bounded(gngeo_log_path),
                    "xvfb_log_tail": _tail_bounded(xvfb_log_path),
                },
            }
        )
        _write_result(result_path, result)
        print(
            f"replay gate {outcome}: {error}",
            file=sys.stderr,
            flush=True,
        )
        return 1
    finally:
        owned.terminate_all()


def _non_negative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return parsed


def build_argument_parser() -> argparse.ArgumentParser:
    repository = Path(__file__).resolve().parents[1]
    replay_build = (
        repository / "platform" / "neogeo" / "build" / "replay-fast"
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--elf",
        type=Path,
        default=replay_build / "smbneogeo-replay.elf",
    )
    parser.add_argument(
        "--rom-dir",
        type=Path,
        default=replay_build / "rom",
    )
    parser.add_argument(
        "--data-file",
        type=Path,
        default=replay_build / "rom" / "gngeo_data.zip",
    )
    parser.add_argument("--rom-set", default=DEFAULT_ROM_SET)
    parser.add_argument(
        "--timeout",
        type=float,
        help=(
            "finite gate deadline in seconds; defaults to a conservative "
            "deadline derived from --68k-overclock"
        ),
    )
    parser.add_argument(
        "--evidence-dir",
        type=Path,
        help="new directory for logs and result.json; must not already exist",
    )
    parser.add_argument(
        "--68k-overclock",
        dest="m68k_overclock",
        type=_non_negative_int,
        default=DEFAULT_68K_OVERCLOCK,
        metavar="PERCENT",
    )
    parser.add_argument(
        "--heartbeat-seconds",
        type=float,
        default=DEFAULT_HEARTBEAT_SECONDS,
    )
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=DEFAULT_STARTUP_TIMEOUT_SECONDS,
    )
    parser.add_argument("--gngeo", default="ngdevkit-gngeo")
    parser.add_argument("--gdb", default="m68k-neogeo-elf-gdb")
    parser.add_argument("--nm", default="m68k-neogeo-elf-nm")
    parser.add_argument("--xvfb", default="Xvfb")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_argument_parser()
    effective_argv = list(sys.argv[1:] if argv is None else argv)
    args = parser.parse_args(effective_argv)
    args._argv = effective_argv
    try:
        return run_probe(args)
    except (ReplayGateError, cadence.CadenceError, OSError) as error:
        print(f"replay gate failed: {error}", file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("replay gate interrupted", file=sys.stderr, flush=True)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
