#!/usr/bin/env python3
"""Measure Neo Geo game-frame cadence with GnGeo's remote debugger."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from contextlib import ExitStack
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from typing import Sequence, TextIO


DEBUG_HOST = "127.0.0.1"
DEBUG_PORT = 2159
UINT32_MODULUS = 1 << 32
DEFAULT_WARMUP_VBLANKS = 30
DEFAULT_SAMPLE_VBLANKS = (60, 120)
RESULT_SCHEMA_VERSION = 1
DEFAULT_P1_CONTROLS = (
    "A=K97,B=K115,C=K113,D=K119,START=K49,"
    "UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79"
)
DEFAULT_P2_CONTROLS = "START=K50"
REQUIRED_SYMBOL_NAMES = (
    "rom_callback_VBlank",
    "neogeo_vblank_count",
    "neogeo_game_frame_count",
)

NM_SYMBOL_RE = re.compile(
    r"^\s*"
    r"(?P<address>[0-9a-fA-F]+)\s+"
    r"(?P<size>[0-9a-fA-F]+)\s+"
    r"(?P<kind>[A-Za-z])\s+"
    r"(?P<name>\S+)\s*$"
)
COUNTER_RE = re.compile(
    r"^CADENCE_COUNTER\s+"
    r"phase=(?P<phase>[a-z0-9_]+)\s+"
    r"vblank=(?P<vblank>[0-9]+)\s+"
    r"game=(?P<game>[0-9]+)\s*$"
)


class CadenceError(RuntimeError):
    """Raised when the cadence probe cannot produce trustworthy evidence."""


class CadenceAssertionError(CadenceError):
    """Raised when a requested cadence assertion fails."""


@dataclass(frozen=True)
class ElfSymbol:
    name: str
    address: int
    size: int
    kind: str


@dataclass(frozen=True)
class CadenceSymbols:
    vblank_callback: ElfSymbol
    vblank_count: ElfSymbol
    game_frame_count: ElfSymbol


@dataclass(frozen=True)
class CounterSnapshot:
    phase: str
    vblank: int
    game: int


@dataclass(frozen=True)
class CadenceInterval:
    phase: str
    expected_vblanks: int
    display_vblanks: int
    game_frames: int
    missed_frames: int


def parse_nm_symbols(output: str) -> dict[str, ElfSymbol]:
    """Parse POSIX nm ``-S`` output into symbols keyed by exact name."""

    symbols: dict[str, ElfSymbol] = {}
    for line in output.splitlines():
        match = NM_SYMBOL_RE.match(line)
        if match is None:
            continue
        name = match.group("name")
        symbol = ElfSymbol(
            name=name,
            address=int(match.group("address"), 16),
            size=int(match.group("size"), 16),
            kind=match.group("kind"),
        )
        if name in symbols:
            raise CadenceError(f"ELF contains duplicate symbol {name!r}")
        symbols[name] = symbol
    return symbols


def resolve_cadence_symbols(
    elf: Path,
    nm_executable: str = "m68k-neogeo-elf-nm",
) -> CadenceSymbols:
    """Resolve and validate every address used by the debugger probe."""

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
        raise CadenceError(
            f"{nm_executable} failed with status {completed.returncode}:\n"
            f"{completed.stdout}"
        )

    parsed = parse_nm_symbols(completed.stdout)
    missing = [name for name in REQUIRED_SYMBOL_NAMES if name not in parsed]
    if missing:
        raise CadenceError(
            "ELF is missing cadence symbols: " + ", ".join(missing)
        )

    callback = parsed["rom_callback_VBlank"]
    vblank_count = parsed["neogeo_vblank_count"]
    game_frame_count = parsed["neogeo_game_frame_count"]

    if callback.kind not in "TtWw":
        raise CadenceError(
            f"{callback.name} is not a code symbol (nm type {callback.kind})"
        )
    for symbol in (vblank_count, game_frame_count):
        if symbol.size != 4:
            raise CadenceError(
                f"{symbol.name} is {symbol.size} bytes; "
                "the probe only reads 32-bit scalars"
            )
        if symbol.kind in "UuTtWw":
            raise CadenceError(
                f"{symbol.name} is not a defined data scalar "
                f"(nm type {symbol.kind})"
            )

    return CadenceSymbols(
        vblank_callback=callback,
        vblank_count=vblank_count,
        game_frame_count=game_frame_count,
    )


def validate_counts(
    warmup_vblanks: int,
    sample_vblanks: Sequence[int],
) -> None:
    if warmup_vblanks < 0:
        raise ValueError("warmup VBlank count must be non-negative")
    if not sample_vblanks:
        raise ValueError("at least one sample count is required")
    if any(count <= 0 for count in sample_vblanks):
        raise ValueError("sample VBlank counts must be positive")


def _gdb_address(address: int) -> str:
    return f"0x{address:08x}"


def build_gdb_script(
    symbols: CadenceSymbols,
    warmup_vblanks: int,
    sample_vblanks: Sequence[int],
    host: str = DEBUG_HOST,
    port: int = DEBUG_PORT,
) -> str:
    """Construct a batch GDB script containing only 32-bit scalar reads."""

    validate_counts(warmup_vblanks, sample_vblanks)
    vblank_address = _gdb_address(symbols.vblank_count.address)
    game_address = _gdb_address(symbols.game_frame_count.address)

    lines = [
        "set pagination off",
        "set confirm off",
        "set verbose off",
        f"target remote {host}:{port}",
        f"break *{_gdb_address(symbols.vblank_callback.address)}",
        "continue",
    ]
    checkpoint_index = 0

    def append_checkpoint(phase: str) -> None:
        nonlocal checkpoint_index
        variable_suffix = str(checkpoint_index)
        lines.extend(
            [
                (
                    f"set $vb{variable_suffix} = "
                    f"*(unsigned int *){vblank_address}"
                ),
                (
                    f"set $gf{variable_suffix} = "
                    f"*(unsigned int *){game_address}"
                ),
                (
                    'printf "CADENCE_COUNTER '
                    f"phase={phase} vblank=%u game=%u\\n"
                    f'", $vb{variable_suffix}, $gf{variable_suffix}'
                ),
            ]
        )
        checkpoint_index += 1

    append_checkpoint("baseline")
    if warmup_vblanks:
        lines.extend(
            [
                f"ignore 1 {warmup_vblanks - 1}",
                "continue",
            ]
        )
        append_checkpoint("warmup")

    for sample_index, count in enumerate(sample_vblanks, start=1):
        lines.extend(
            [
                f"ignore 1 {count - 1}",
                "continue",
            ]
        )
        append_checkpoint(f"sample_{sample_index}")

    lines.extend(["detach", "quit"])
    return "\n".join(lines) + "\n"


def build_gdb_command(
    gdb_executable: str,
    elf: Path,
    script: Path,
) -> list[str]:
    return [
        gdb_executable,
        "-q",
        "-batch",
        str(elf),
        "-x",
        str(script),
    ]


def build_xvfb_command(xvfb_executable: str) -> list[str]:
    return [
        xvfb_executable,
        "-displayfd",
        "1",
        "-screen",
        "0",
        "1024x768x24",
        "-nolisten",
        "tcp",
        "-ac",
    ]


def build_gngeo_command(
    gngeo_executable: str,
    rom_dir: Path,
    data_file: Path,
    rom_set: str,
    scale: int,
) -> list[str]:
    """Build the fixed-timing GnGeo command used for cadence evidence."""

    if scale <= 0:
        raise ValueError("GnGeo scale must be positive")
    return [
        gngeo_executable,
        "-b",
        "soft",
        "--screen320",
        f"--scale={scale}",
        "--no-resize",
        "--no-sound",
        "--no-autoframeskip",
        "--68kclock=0",
        "--system",
        "home",
        "-D",
        "--p1control",
        DEFAULT_P1_CONTROLS,
        "--p2control",
        DEFAULT_P2_CONTROLS,
        "-i",
        str(rom_dir),
        "-d",
        str(data_file),
        rom_set,
    ]


def build_motion_command(
    python_executable: str,
    script_path: Path,
    display: str,
    xdotool_executable: str,
) -> list[str]:
    return [
        python_executable,
        str(script_path),
        "--_motion-child",
        "--display",
        display,
        "--xdotool",
        xdotool_executable,
    ]


def parse_counter_snapshots(output: str) -> dict[str, CounterSnapshot]:
    """Extract debugger checkpoints while ignoring normal GDB chatter."""

    snapshots: dict[str, CounterSnapshot] = {}
    for line in output.splitlines():
        match = COUNTER_RE.match(line.strip())
        if match is None:
            continue
        phase = match.group("phase")
        if phase in snapshots:
            raise CadenceError(f"duplicate debugger checkpoint {phase!r}")
        vblank = int(match.group("vblank"))
        game = int(match.group("game"))
        if vblank >= UINT32_MODULUS or game >= UINT32_MODULUS:
            raise CadenceError(
                f"checkpoint {phase!r} is outside the 32-bit scalar range"
            )
        snapshots[phase] = CounterSnapshot(
            phase=phase,
            vblank=vblank,
            game=game,
        )
    return snapshots


def _uint32_delta(after: int, before: int) -> int:
    return (after - before) % UINT32_MODULUS


def analyze_counter_snapshots(
    snapshots: dict[str, CounterSnapshot],
    warmup_vblanks: int,
    sample_vblanks: Sequence[int],
) -> list[CadenceInterval]:
    """Turn ordered 32-bit checkpoints into validated cadence intervals."""

    validate_counts(warmup_vblanks, sample_vblanks)
    expected_phases = ["baseline"]
    if warmup_vblanks:
        expected_phases.append("warmup")
    expected_phases.extend(
        f"sample_{index}"
        for index in range(1, len(sample_vblanks) + 1)
    )
    missing = [phase for phase in expected_phases if phase not in snapshots]
    if missing:
        raise CadenceError(
            "GDB output is missing checkpoints: " + ", ".join(missing)
        )

    intervals: list[CadenceInterval] = []
    previous = snapshots["baseline"]

    def append_interval(phase: str, expected: int) -> None:
        nonlocal previous
        current = snapshots[phase]
        display_vblanks = _uint32_delta(current.vblank, previous.vblank)
        game_frames = _uint32_delta(current.game, previous.game)
        if display_vblanks != expected:
            raise CadenceError(
                f"{phase} stopped after {display_vblanks} VBlanks; "
                f"expected {expected}"
            )
        if game_frames > display_vblanks:
            raise CadenceError(
                f"{phase} advanced {game_frames} game frames during only "
                f"{display_vblanks} VBlanks"
            )
        intervals.append(
            CadenceInterval(
                phase=phase,
                expected_vblanks=expected,
                display_vblanks=display_vblanks,
                game_frames=game_frames,
                missed_frames=display_vblanks - game_frames,
            )
        )
        previous = current

    if warmup_vblanks:
        append_interval("warmup", warmup_vblanks)
    for index, expected in enumerate(sample_vblanks, start=1):
        append_interval(f"sample_{index}", expected)
    return intervals


def assert_zero_missed_frames(
    intervals: Sequence[CadenceInterval],
) -> None:
    failed = [
        interval
        for interval in intervals
        if interval.phase.startswith("sample_")
        and interval.missed_frames != 0
    ]
    if failed:
        details = ", ".join(
            f"{interval.phase}={interval.missed_frames}"
            for interval in failed
        )
        raise CadenceAssertionError(
            f"zero-missed-frame assertion failed: {details}"
        )


def _can_bind_debug_port(host: str, port: int) -> bool:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind((host, port))
    except OSError as error:
        if error.errno in (errno.EADDRINUSE, errno.EACCES):
            return False
        raise
    finally:
        sock.close()
    return True


def reject_occupied_debug_port(
    host: str = DEBUG_HOST,
    port: int = DEBUG_PORT,
) -> None:
    if not _can_bind_debug_port(host, port):
        raise CadenceError(
            f"debug port {host}:{port} is occupied; "
            "refusing to disturb the existing listener"
        )


@dataclass
class _OwnedProcess:
    label: str
    process: subprocess.Popen[str]


class OwnedProcessGroups:
    """Track and terminate only process groups created by this invocation."""

    def __init__(self) -> None:
        self._processes: list[_OwnedProcess] = []

    def add(
        self,
        label: str,
        process: subprocess.Popen[str],
    ) -> subprocess.Popen[str]:
        self._processes.append(_OwnedProcess(label, process))
        return process

    def terminate_all(self) -> None:
        for owned in reversed(self._processes):
            process = owned.process
            if process.poll() is not None:
                continue
            print(
                f"[cadence] stopping owned {owned.label} process "
                f"{process.pid}",
                flush=True,
            )
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                continue
            try:
                process.wait(timeout=2.0)
                continue
            except subprocess.TimeoutExpired:
                pass
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                continue
            try:
                process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                print(
                    f"[cadence] owned {owned.label} process "
                    f"{process.pid} did not exit",
                    file=sys.stderr,
                    flush=True,
                )


def _start_xvfb(
    executable: str,
    log: TextIO,
    owned: OwnedProcessGroups,
    timeout_seconds: float,
) -> tuple[subprocess.Popen[str], str]:
    process = owned.add(
        "Xvfb",
        subprocess.Popen(
            build_xvfb_command(executable),
            stdout=subprocess.PIPE,
            stderr=log,
            text=True,
            start_new_session=True,
        ),
    )
    if process.stdout is None:
        raise CadenceError("Xvfb display-number pipe was not created")

    selector = selectors.DefaultSelector()
    try:
        selector.register(process.stdout, selectors.EVENT_READ)
        events = selector.select(timeout_seconds)
    finally:
        selector.close()
    if not events:
        if process.poll() is not None:
            raise CadenceError(
                f"Xvfb exited before selecting a display "
                f"(status {process.returncode})"
            )
        raise CadenceError("timed out waiting for Xvfb to select a display")

    display_number = process.stdout.readline().strip()
    process.stdout.close()
    if not display_number.isdigit():
        raise CadenceError(
            f"Xvfb returned invalid display number {display_number!r}"
        )
    return process, f":{display_number}"


def _wait_for_debug_listener(
    gngeo: subprocess.Popen[str],
    timeout_seconds: float,
) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        status = gngeo.poll()
        if status is not None:
            raise CadenceError(
                f"GnGeo exited before opening debug port {DEBUG_PORT} "
                f"(status {status})"
            )
        if not _can_bind_debug_port(DEBUG_HOST, DEBUG_PORT):
            listener_inodes = _listening_socket_inodes(DEBUG_PORT)
            owned_inodes = _process_group_socket_inodes(gngeo.pid)

            if listener_inodes & owned_inodes:
                return
            raise CadenceError(
                f"debug port {DEBUG_HOST}:{DEBUG_PORT} became occupied by "
                "a listener outside the owned GnGeo process group"
            )
        time.sleep(0.1)
    raise CadenceError(
        f"timed out waiting for GnGeo debug port {DEBUG_PORT}"
    )


def _wait_with_heartbeats(
    process: subprocess.Popen[str],
    label: str,
    heartbeat_seconds: float,
    timeout_seconds: float,
    watched_process: subprocess.Popen[str] | None = None,
) -> int:
    start = time.monotonic()
    next_heartbeat = start + heartbeat_seconds
    while True:
        status = process.poll()
        if status is not None:
            return status
        if watched_process is not None:
            watched_status = watched_process.poll()
            if watched_status is not None:
                raise CadenceError(
                    f"GnGeo exited during {label} "
                    f"(status {watched_status})"
                )
        now = time.monotonic()
        if now - start >= timeout_seconds:
            raise CadenceError(
                f"{label} exceeded its {timeout_seconds:g}-second timeout"
            )
        if now >= next_heartbeat:
            elapsed = int(now - start)
            print(
                f"[cadence] {label} still running "
                f"({elapsed}s elapsed)",
                flush=True,
            )
            next_heartbeat = now + heartbeat_seconds
        time.sleep(min(0.25, heartbeat_seconds))


def _listening_socket_inodes(port: int) -> set[str]:
    """Return Linux socket inodes listening on ``port``.

    The cadence harness is already Linux/X11-specific. Reading procfs lets it
    distinguish the debugger opened by its owned GnGeo process group from a
    listener that wins the small race after the preflight bind check.
    """

    inodes: set[str] = set()
    for table in (Path("/proc/net/tcp"), Path("/proc/net/tcp6")):
        try:
            lines = table.read_text().splitlines()[1:]
        except OSError as error:
            raise CadenceError(
                f"cannot inspect debugger listener ownership via {table}: "
                f"{error}"
            ) from error
        for line in lines:
            fields = line.split()
            if len(fields) < 10:
                continue
            try:
                local_port = int(fields[1].rsplit(":", 1)[1], 16)
            except (IndexError, ValueError):
                continue
            if local_port == port and fields[3] == "0A":
                inodes.add(fields[9])
    return inodes


def _process_group_socket_inodes(process_group: int) -> set[str]:
    """Return socket inodes held by members of an owned process group."""

    inodes: set[str] = set()
    proc = Path("/proc")
    try:
        entries = list(proc.iterdir())
    except OSError as error:
        raise CadenceError(
            f"cannot inspect owned process group via {proc}: {error}"
        ) from error

    for entry in entries:
        if not entry.name.isdigit():
            continue
        pid = int(entry.name)
        try:
            if os.getpgid(pid) != process_group:
                continue
            descriptors = list((entry / "fd").iterdir())
        except (OSError, ProcessLookupError):
            continue
        for descriptor in descriptors:
            try:
                target = os.readlink(descriptor)
            except OSError:
                continue
            if target.startswith("socket:[") and target.endswith("]"):
                inodes.add(target[8:-1])
    return inodes


def derive_sampling_timeout(
    warmup_vblanks: int,
    sample_vblanks: Sequence[int],
) -> float:
    """Return a generous but finite deadline for debugger sampling."""

    validate_counts(warmup_vblanks, sample_vblanks)
    total_vblanks = warmup_vblanks + sum(sample_vblanks)
    return max(60.0, 30.0 + total_vblanks * 0.5)


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _tail(path: Path, line_count: int = 80) -> str:
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return ""
    return "\n".join(lines[-line_count:])


def _resolve_executable(value: str) -> str:
    if os.sep in value:
        path = Path(value).expanduser().resolve()
        if not path.is_file() or not os.access(path, os.X_OK):
            raise CadenceError(f"executable is unavailable: {path}")
        return str(path)
    resolved = shutil.which(value)
    if resolved is None:
        raise CadenceError(f"executable is unavailable on PATH: {value}")
    return resolved


def _require_file(path: Path, description: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise CadenceError(f"{description} does not exist: {resolved}")
    return resolved


def _require_directory(path: Path, description: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        raise CadenceError(f"{description} does not exist: {resolved}")
    return resolved


def _create_evidence_directory(
    requested: Path | None,
) -> Path:
    if requested is None:
        return Path(tempfile.mkdtemp(prefix="smb-neogeo-cadence."))
    resolved = requested.expanduser().resolve()
    resolved.mkdir(parents=True, exist_ok=False)
    return resolved


def _print_intervals(intervals: Sequence[CadenceInterval]) -> None:
    for interval in intervals:
        label = interval.phase.upper()
        print(
            f"{label} expected_vblanks={interval.expected_vblanks} "
            f"display_vblanks={interval.display_vblanks} "
            f"game_frames={interval.game_frames} "
            f"missed={interval.missed_frames}",
            flush=True,
        )


def run_probe(args: argparse.Namespace) -> int:
    reject_occupied_debug_port()

    elf = _require_file(args.elf, "ELF")
    rom_dir = _require_directory(args.rom_dir, "ROM directory")
    data_file = _require_file(args.data_file, "GnGeo data file")
    cartridge = _require_file(
        rom_dir / f"{args.rom_set}.zip",
        "GnGeo cartridge",
    )
    sampling_timeout = (
        args.sampling_timeout
        if args.sampling_timeout is not None
        else derive_sampling_timeout(
            args.warmup_vblanks,
            args.sample_vblanks,
        )
    )

    nm_executable = _resolve_executable(args.nm)
    gdb_executable = _resolve_executable(args.gdb)
    gngeo_executable = _resolve_executable(args.gngeo)
    xvfb_executable = _resolve_executable(args.xvfb)
    xdotool_executable = None
    if args.active_motion:
        xdotool_executable = _resolve_executable(args.xdotool)

    symbols = resolve_cadence_symbols(elf, nm_executable)
    gdb_script_text = build_gdb_script(
        symbols,
        args.warmup_vblanks,
        args.sample_vblanks,
    )
    evidence_dir = _create_evidence_directory(args.evidence_dir)
    gdb_script_path = evidence_dir / "cadence.gdb"
    gdb_script_path.write_text(gdb_script_text)

    print(
        "[cadence] resolved "
        f"VBlank callback={_gdb_address(symbols.vblank_callback.address)} "
        f"vblank_count={_gdb_address(symbols.vblank_count.address)} "
        f"game_frame_count={_gdb_address(symbols.game_frame_count.address)}",
        flush=True,
    )
    print(f"[cadence] evidence directory: {evidence_dir}", flush=True)

    owned = OwnedProcessGroups()
    gdb_log_path = evidence_dir / "gdb.log"
    gngeo_log_path = evidence_dir / "gngeo.log"
    xvfb_log_path = evidence_dir / "xvfb.log"
    motion_log_path = evidence_dir / "motion.log"

    try:
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
            motion_log = None
            motion = None
            if args.active_motion:
                motion_log = stack.enter_context(
                    motion_log_path.open("w", buffering=1)
                )

            _xvfb, display = _start_xvfb(
                xvfb_executable,
                xvfb_log,
                owned,
                args.startup_timeout,
            )
            print(
                f"[cadence] owned Xvfb is ready on {display}",
                flush=True,
            )

            gngeo_env = os.environ.copy()
            gngeo_env["DISPLAY"] = display
            gngeo_command = build_gngeo_command(
                gngeo_executable,
                rom_dir,
                data_file,
                args.rom_set,
                args.scale,
            )
            gngeo = owned.add(
                "GnGeo",
                subprocess.Popen(
                    gngeo_command,
                    stdout=gngeo_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=gngeo_env,
                    start_new_session=True,
                ),
            )

            if args.active_motion:
                assert xdotool_executable is not None
                assert motion_log is not None
                motion = owned.add(
                    "motion input",
                    subprocess.Popen(
                        build_motion_command(
                            sys.executable,
                            Path(__file__).resolve(),
                            display,
                            xdotool_executable,
                        ),
                        stdout=motion_log,
                        stderr=subprocess.STDOUT,
                        text=True,
                        start_new_session=True,
                    ),
                )
                if motion.poll() is not None:
                    raise CadenceError(
                        "active-motion helper exited during startup"
                    )
                print(
                    "[cadence] active motion input enabled",
                    flush=True,
                )

            _wait_for_debug_listener(gngeo, args.startup_timeout)
            print(
                f"[cadence] owned GnGeo debug listener is ready on "
                f"{DEBUG_HOST}:{DEBUG_PORT}",
                flush=True,
            )

            gdb = owned.add(
                "GDB",
                subprocess.Popen(
                    build_gdb_command(
                        gdb_executable,
                        elf,
                        gdb_script_path,
                    ),
                    stdout=gdb_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    start_new_session=True,
                ),
            )
            gdb_status = _wait_with_heartbeats(
                gdb,
                "GDB cadence sample",
                args.heartbeat_seconds,
                sampling_timeout,
                watched_process=gngeo,
            )
            gdb_log.flush()
            gdb_output = gdb_log_path.read_text(errors="replace")
            if gdb_status != 0:
                raise CadenceError(
                    f"GDB failed with status {gdb_status}:\n"
                    f"{_tail(gdb_log_path)}"
                )
            if motion is not None:
                motion_status = motion.poll()
                motion_output = motion_log_path.read_text(errors="replace")
                if motion_status is not None:
                    raise CadenceError(
                        "active-motion helper exited before sampling ended "
                        f"(status {motion_status}):\n"
                        f"{_tail(motion_log_path)}"
                    )
                if "MOTION_ACTIVE" not in motion_output:
                    raise CadenceError(
                        "active-motion helper did not reach active input; "
                        "increase --warmup-vblanks or inspect motion.log"
                    )

            snapshots = parse_counter_snapshots(gdb_output)
            intervals = analyze_counter_snapshots(
                snapshots,
                args.warmup_vblanks,
                args.sample_vblanks,
            )
            _print_intervals(intervals)

            result = {
                "schema_version": RESULT_SCHEMA_VERSION,
                "elf": str(elf),
                "rom_set": args.rom_set,
                "artifacts": {
                    "elf": {
                        "path": str(elf),
                        "bytes": elf.stat().st_size,
                        "sha256": _sha256_file(elf),
                    },
                    "cartridge": {
                        "path": str(cartridge),
                        "bytes": cartridge.stat().st_size,
                        "sha256": _sha256_file(cartridge),
                    },
                    "gngeo_data": {
                        "path": str(data_file),
                        "bytes": data_file.stat().st_size,
                        "sha256": _sha256_file(data_file),
                    },
                },
                "fixed_timing": {
                    "autoframeskip": False,
                    "m68k_clock_percent_adjustment": 0,
                },
                "probe_arguments": {
                    "warmup_vblanks": args.warmup_vblanks,
                    "sample_vblanks": list(args.sample_vblanks),
                    "active_motion": args.active_motion,
                    "assert_zero_missed": args.assert_zero_missed,
                    "scale": args.scale,
                    "heartbeat_seconds": args.heartbeat_seconds,
                    "startup_timeout_seconds": args.startup_timeout,
                    "sampling_timeout_seconds": sampling_timeout,
                    "gngeo_command": gngeo_command,
                },
                "symbols": {
                    name: asdict(symbol)
                    for name, symbol in (
                        ("vblank_callback", symbols.vblank_callback),
                        ("vblank_count", symbols.vblank_count),
                        ("game_frame_count", symbols.game_frame_count),
                    )
                },
                "intervals": [asdict(interval) for interval in intervals],
                "active_motion": args.active_motion,
            }
            (evidence_dir / "result.json").write_text(
                json.dumps(result, indent=2, sort_keys=True) + "\n"
            )

            if args.assert_zero_missed:
                assert_zero_missed_frames(intervals)
                print(
                    "ASSERT_ZERO_MISSED_FRAMES passed",
                    flush=True,
                )
    except Exception:
        gngeo_tail = _tail(gngeo_log_path)
        motion_tail = _tail(motion_log_path)
        if gngeo_tail:
            print(
                f"[cadence] GnGeo log tail:\n{gngeo_tail}",
                file=sys.stderr,
                flush=True,
            )
        if motion_tail:
            print(
                f"[cadence] motion log tail:\n{motion_tail}",
                file=sys.stderr,
                flush=True,
            )
        raise
    finally:
        owned.terminate_all()

    return 0


def _positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return parsed


def _non_negative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return parsed


def build_argument_parser() -> argparse.ArgumentParser:
    repository = Path(__file__).resolve().parents[1]
    build = repository / "platform" / "neogeo" / "build"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--elf",
        type=Path,
        default=build / "smbneogeo.elf",
        help="linked MC68000 ELF used to resolve all debugger addresses",
    )
    parser.add_argument(
        "--rom-dir",
        type=Path,
        default=build / "rom",
        help="GnGeo ROM directory",
    )
    parser.add_argument(
        "--data-file",
        type=Path,
        default=build / "rom" / "gngeo_data.zip",
        help="GnGeo game-data archive",
    )
    parser.add_argument("--rom-set", default="smbneogeo")
    parser.add_argument(
        "--warmup-vblanks",
        type=_non_negative_int,
        default=DEFAULT_WARMUP_VBLANKS,
    )
    parser.add_argument(
        "--sample-vblanks",
        type=_positive_int,
        nargs="+",
        default=list(DEFAULT_SAMPLE_VBLANKS),
        metavar="COUNT",
    )
    parser.add_argument(
        "--active-motion",
        action="store_true",
        help=(
            "press Start, hold Right+B, and jump repeatedly; "
            "use enough warmup to enter gameplay"
        ),
    )
    parser.add_argument(
        "--assert-zero-missed",
        "--assert-zero-missed-frames",
        dest="assert_zero_missed",
        action="store_true",
        help="exit nonzero if any measured sample misses a game frame",
    )
    parser.add_argument("--scale", type=_positive_int, default=1)
    parser.add_argument(
        "--heartbeat-seconds",
        type=float,
        default=5.0,
        help="stdout heartbeat interval while GDB is sampling",
    )
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=10.0,
        help="seconds to wait for Xvfb and the GnGeo debugger",
    )
    parser.add_argument(
        "--sampling-timeout",
        type=float,
        help=(
            "maximum seconds for GDB sampling; defaults to a finite "
            "deadline derived from the requested VBlank count"
        ),
    )
    parser.add_argument(
        "--evidence-dir",
        type=Path,
        help="new directory for logs and results; must not already exist",
    )
    parser.add_argument("--gngeo", default="ngdevkit-gngeo")
    parser.add_argument("--gdb", default="m68k-neogeo-elf-gdb")
    parser.add_argument("--nm", default="m68k-neogeo-elf-nm")
    parser.add_argument("--xvfb", default="Xvfb")
    parser.add_argument("--xdotool", default="xdotool")
    return parser


def _validate_runtime_arguments(args: argparse.Namespace) -> None:
    validate_counts(args.warmup_vblanks, args.sample_vblanks)
    if args.heartbeat_seconds <= 0:
        raise CadenceError("heartbeat interval must be positive")
    if args.startup_timeout <= 0:
        raise CadenceError("startup timeout must be positive")
    if args.sampling_timeout is not None and args.sampling_timeout <= 0:
        raise CadenceError("sampling timeout must be positive")


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)
    try:
        _validate_runtime_arguments(args)
        return run_probe(args)
    except (CadenceError, OSError, ValueError) as error:
        print(f"cadence probe failed: {error}", file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("cadence probe interrupted", file=sys.stderr, flush=True)
        return 130


def motion_child_main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--display", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args(argv)

    environment = os.environ.copy()
    environment["DISPLAY"] = args.display
    stopping = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)

    def xdotool(*arguments: str, capture: bool = False) -> str:
        completed = subprocess.run(
            [args.xdotool, *arguments],
            env=environment,
            stdout=(
                subprocess.PIPE if capture else subprocess.DEVNULL
            ),
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            return ""
        return completed.stdout if capture else ""

    window = ""
    for _attempt in range(50):
        if stopping:
            return 0
        output = xdotool("search", "--name", "Gngeo", capture=True)
        window = next(
            (line.strip() for line in output.splitlines() if line.strip()),
            "",
        )
        if window:
            break
        time.sleep(0.2)
    if not window:
        print("motion helper could not find the GnGeo window", flush=True)
        return 1

    xdotool("windowfocus", "--sync", window)
    xdotool("mousemove", "--window", window, "320", "224", "click", "1")
    time.sleep(0.5)
    xdotool("key", "1")
    time.sleep(2.0)
    xdotool("keydown", "Right")
    xdotool("keydown", "s")
    print(f"MOTION_ACTIVE window={window}", flush=True)
    try:
        while not stopping:
            time.sleep(0.6)
            if stopping:
                break
            xdotool("keydown", "a")
            time.sleep(0.18)
            xdotool("keyup", "a")
    finally:
        xdotool("keyup", "a")
        xdotool("keyup", "s")
        xdotool("keyup", "Right")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--_motion-child":
        raise SystemExit(motion_child_main(sys.argv[2:]))
    raise SystemExit(main())
