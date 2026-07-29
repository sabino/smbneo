#!/usr/bin/env python3
"""Measure exact replay windows with selective Neo Geo rendering enabled."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shlex
import shutil
import signal
import subprocess
import sys
import time
from typing import Sequence, TextIO

import measure_neogeo_cadence as cadence
import neogeo_replay_window as replay_window


STATUS_FIELDS = (
    "magic",
    "version",
    "result",
    "frame",
    "tail_frame",
    "segment_index",
    "controller_state",
    "entered_mask",
    "completed_mask",
    "current_stage",
    "victory_stable_frames",
    "oper_mode",
    "oper_mode_task",
    "world",
    "level",
    "world_end_timer",
    "replay_end_frame",
    "hardware_playable",
    "opposite_direction_transitions",
    "ram_init_option",
    "bootstrap_frames",
    "area_init_hold_frames",
    "area_init_hold_count",
    "core_frames_advanced",
    "rendering_enabled",
    "game_frame_count",
    "vblank_count",
    "stage_settle_frames",
    "render_generation",
    "presented_generation",
)
STATUS_MAGIC = 0x534D4252
STATUS_VERSION = 4
DEFAULT_ROM_SET = "smbneo"
DEFAULT_SYSTEM = "arcade"
PROGRESS_WORD_RE = re.compile(
    r"^REPLAY_PROGRESS_WORD sample=(?P<sample>[0-9]+) "
    r"index=(?P<index>[0-9]+) value=0x(?P<value>[0-9a-fA-F]{8})$"
)
PROGRESS_END_RE = re.compile(
    r"^REPLAY_PROGRESS_END sample=(?P<sample>[0-9]+)$"
)
RAM_RE = re.compile(
    r"^WINDOW_RAM sample=(?P<sample>[0-9]+) "
    r"frame_counter=(?P<frame_counter>[0-9]+) "
    r"active_enemies=(?P<active_enemies>[0-9]+) "
    r"flags=(?P<flags>[0-9a-fA-F]{12}) "
    r"ids=(?P<ids>[0-9a-fA-F]{12}) "
    r"screen=(?P<screen_page>[0-9]+):(?P<screen_x>[0-9]+) "
    r"player=(?P<player_page>[0-9]+):(?P<player_x>[0-9]+)$"
)
RAM_WORD_RE = re.compile(
    r"^WINDOW_RAM_WORD sample=(?P<sample>[0-9]+) "
    r"index=(?P<index>[0-9]+) value=0x(?P<value>[0-9a-fA-F]{8})$"
)
CPU_RE = re.compile(
    r"^WINDOW_CPU sample=(?P<sample>[0-9]+) "
    r"a=(?P<a>[0-9]+) x=(?P<x>[0-9]+) y=(?P<y>[0-9]+) "
    r"sp=(?P<sp>[0-9]+) carry=(?P<carry>[0-9]+) "
    r"nz=(?P<nz>[0-9]+)$"
)


class BenchmarkError(RuntimeError):
    """Raised when benchmark evidence is incomplete or inconsistent."""


@dataclass(frozen=True)
class BenchmarkSymbols:
    progress: cadence.ElfSymbol
    status: cadence.ElfSymbol
    ram: cadence.ElfSymbol
    a: cadence.ElfSymbol
    x: cadence.ElfSymbol
    y: cadence.ElfSymbol
    sp: cadence.ElfSymbol
    carry_flag: cadence.ElfSymbol
    nz_value: cadence.ElfSymbol


@dataclass(frozen=True)
class ParsedEvidence:
    status_mode: str
    statuses: tuple[dict[str, int], ...]
    ram_snapshots: tuple[dict[str, object], ...]
    cpu_snapshots: tuple[dict[str, int], ...]
    ram_images: tuple[bytes, ...]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_executable(name: str) -> str:
    if os.sep in name:
        path = Path(name).expanduser().resolve()
        if path.is_file() and os.access(path, os.X_OK):
            return str(path)
    else:
        resolved = shutil.which(name)
        if resolved is not None:
            return resolved
    raise BenchmarkError(f"required executable is unavailable: {name}")


def resolve_symbols(elf: Path, nm: str) -> BenchmarkSymbols:
    completed = subprocess.run(
        [nm, "-n", "-S", "--defined-only", str(elf)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise BenchmarkError(
            f"nm failed with status {completed.returncode}: "
            f"{completed.stdout[-2000:]}"
        )
    try:
        parsed = cadence.parse_nm_symbols(completed.stdout)
    except cadence.CadenceError as error:
        raise BenchmarkError(str(error)) from error
    required = (
        "neogeo_replay_progress_trap",
        "neogeo_replay_status",
        "ram",
        "a",
        "x",
        "y",
        "sp",
        "carry_flag",
        "nz_value",
    )
    missing = [name for name in required if name not in parsed]
    if missing:
        raise BenchmarkError(
            "ELF is missing window-benchmark symbols: " + ", ".join(missing)
        )
    status = parsed["neogeo_replay_status"]
    ram = parsed["ram"]
    if status.size != len(STATUS_FIELDS) * 4:
        raise BenchmarkError(
            f"status mailbox is {status.size} bytes; "
            f"expected {len(STATUS_FIELDS) * 4}"
        )
    if ram.size != 0x800:
        raise BenchmarkError(f"translated RAM is {ram.size} bytes; expected 2048")
    for name in ("a", "x", "y", "sp", "carry_flag", "nz_value"):
        if parsed[name].size != 1:
            raise BenchmarkError(
                f"translated CPU symbol {name} is {parsed[name].size} bytes; "
                "expected 1"
            )
    return BenchmarkSymbols(
        progress=parsed["neogeo_replay_progress_trap"],
        status=status,
        ram=ram,
        a=parsed["a"],
        x=parsed["x"],
        y=parsed["y"],
        sp=parsed["sp"],
        carry_flag=parsed["carry_flag"],
        nz_value=parsed["nz_value"],
    )


def _gdb_address(address: int) -> str:
    return f"0x{address:08x}"


def _screenshot_command(
    python: str,
    helper: Path,
    scrot: str,
    output: Path,
) -> str:
    return shlex.join(
        [
            python,
            str(helper),
            "--output",
            str(output),
            "--scrot",
            scrot,
            "--timeout",
            "10",
            "--display-settle-seconds",
            "0.05",
            "--max-bytes",
            str(4 * 1024 * 1024),
        ]
    )


def build_gdb_script(
    symbols: BenchmarkSymbols,
    schedule: replay_window.ReplayWindowSchedule,
    screenshot_helper: Path,
    screenshots: dict[int, Path],
    python: str,
    scrot: str,
) -> str:
    """Build an address-pinned GDB script for every generated checkpoint."""

    status_base = symbols.status.address
    ram_base = symbols.ram.address
    lines = [
        "set pagination off",
        "set confirm off",
        "set verbose off",
        "set remotetimeout 60",
        "set $progress_sample = 0",
        f"target remote {cadence.DEBUG_HOST}:{cadence.DEBUG_PORT}",
        f"break *{_gdb_address(symbols.progress.address)}",
        "commands",
        "  silent",
        "  set $progress_sample = $progress_sample + 1",
    ]
    for index in range(len(STATUS_FIELDS)):
        address = status_base + index * 4
        lines.extend(
            [
                f"  set $mb{index} = "
                f"*(unsigned int *){_gdb_address(address)}",
                '  printf "REPLAY_PROGRESS_WORD '
                f'sample=%u index={index} value=0x%08x\\n", '
                f"$progress_sample, $mb{index}",
            ]
        )

    enemy_flags = [ram_base + 0x0F + index for index in range(6)]
    enemy_ids = [ram_base + 0x16 + index for index in range(6)]
    active_expression = " + ".join(
        f"(*(unsigned char *){_gdb_address(address)} != 0)"
        for address in enemy_flags
    )
    ram_arguments = [
        "$progress_sample",
        f"*(unsigned char *){_gdb_address(ram_base + 0x09)}",
        f"({active_expression})",
        *(f"*(unsigned char *){_gdb_address(address)}" for address in enemy_flags),
        *(f"*(unsigned char *){_gdb_address(address)}" for address in enemy_ids),
        f"*(unsigned char *){_gdb_address(ram_base + 0x71A)}",
        f"*(unsigned char *){_gdb_address(ram_base + 0x71C)}",
        f"*(unsigned char *){_gdb_address(ram_base + 0x06D)}",
        f"*(unsigned char *){_gdb_address(ram_base + 0x086)}",
    ]
    lines.append(
        '  printf "WINDOW_RAM sample=%u frame_counter=%u '
        'active_enemies=%u flags=%02x%02x%02x%02x%02x%02x '
        'ids=%02x%02x%02x%02x%02x%02x screen=%u:%u player=%u:%u\\n", '
        + ", ".join(ram_arguments)
    )
    lines.append(
        '  printf "WINDOW_CPU sample=%u a=%u x=%u y=%u sp=%u '
        'carry=%u nz=%u\\n", '
        + ", ".join(
            [
                "$progress_sample",
                f"*(unsigned char *){_gdb_address(symbols.a.address)}",
                f"*(unsigned char *){_gdb_address(symbols.x.address)}",
                f"*(unsigned char *){_gdb_address(symbols.y.address)}",
                f"*(unsigned char *){_gdb_address(symbols.sp.address)}",
                f"*(unsigned char *){_gdb_address(symbols.carry_flag.address)}",
                f"*(unsigned char *){_gdb_address(symbols.nz_value.address)}",
            ]
        )
    )
    for index in range(0x800 // 4):
        address = ram_base + index * 4
        lines.append(
            '  printf "WINDOW_RAM_WORD '
            f'sample=%u index={index} value=0x%08x\\n", '
            f"$progress_sample, *(unsigned int *){_gdb_address(address)}"
        )
    lines.append(
        '  printf "REPLAY_PROGRESS_END sample=%u\\n", $progress_sample'
    )
    for sample, screenshot in sorted(screenshots.items()):
        lines.extend(
            [
                f"  if $progress_sample == {sample}",
                "    shell "
                + _screenshot_command(
                    python,
                    screenshot_helper,
                    scrot,
                    screenshot,
                ),
                "  end",
            ]
        )
    lines.extend(
        [
            f"  if $progress_sample >= {len(schedule.checkpoints)}",
            "    detach",
            "    quit",
            "  else",
            "    continue",
            "  end",
            "end",
            "continue",
        ]
    )
    return "\n".join(lines) + "\n"


def _validate_status_mode(
    statuses: Sequence[dict[str, int]],
    requested_mode: str,
) -> str:
    modes = {
        "full" if status["rendering_enabled"] == 1 else "logic"
        if status["rendering_enabled"] == 0 else "invalid"
        for status in statuses
    }
    if "invalid" in modes or len(modes) != 1:
        raise BenchmarkError(
            "checkpoint mailboxes disagree on full-render/logic status mode"
        )
    detected_mode = next(iter(modes))
    if requested_mode != "auto" and requested_mode != detected_mode:
        raise BenchmarkError(
            f"mailbox reports {detected_mode} mode; "
            f"{requested_mode} was requested"
        )
    return detected_mode


def parse_evidence(
    log: str,
    schedule: replay_window.ReplayWindowSchedule,
    requested_mode: str = "auto",
) -> ParsedEvidence:
    """Parse and validate complete checkpoint mailboxes and RAM snapshots."""

    words: dict[int, dict[int, int]] = {}
    completed: set[int] = set()
    ram_records: dict[int, dict[str, object]] = {}
    cpu_records: dict[int, dict[str, int]] = {}
    ram_words: dict[int, dict[int, int]] = {}
    for raw_line in log.splitlines():
        line = raw_line.strip()
        match = PROGRESS_WORD_RE.fullmatch(line)
        if match is not None:
            sample = int(match.group("sample"))
            index = int(match.group("index"))
            if sample <= 0 or index >= len(STATUS_FIELDS):
                raise BenchmarkError("GDB reported an out-of-range mailbox sample")
            sample_words = words.setdefault(sample, {})
            if index in sample_words:
                raise BenchmarkError(
                    f"sample {sample} repeats mailbox word {index}"
                )
            sample_words[index] = int(match.group("value"), 16)
            continue
        match = PROGRESS_END_RE.fullmatch(line)
        if match is not None:
            sample = int(match.group("sample"))
            if sample in completed:
                raise BenchmarkError(f"sample {sample} has two end markers")
            completed.add(sample)
            continue
        match = RAM_RE.fullmatch(line)
        if match is not None:
            sample = int(match.group("sample"))
            if sample in ram_records:
                raise BenchmarkError(f"sample {sample} repeats its RAM record")
            ram_records[sample] = {
                "frame_counter": int(match.group("frame_counter")),
                "active_enemies": int(match.group("active_enemies")),
                "enemy_flags_hex": match.group("flags").lower(),
                "enemy_ids_hex": match.group("ids").lower(),
                "screen_page": int(match.group("screen_page")),
                "screen_x": int(match.group("screen_x")),
                "player_page": int(match.group("player_page")),
                "player_x": int(match.group("player_x")),
            }
            continue
        match = CPU_RE.fullmatch(line)
        if match is not None:
            sample = int(match.group("sample"))
            if sample in cpu_records:
                raise BenchmarkError(f"sample {sample} repeats its CPU record")
            cpu_records[sample] = {
                name: int(match.group(name))
                for name in ("a", "x", "y", "sp", "carry", "nz")
            }
            continue
        match = RAM_WORD_RE.fullmatch(line)
        if match is not None:
            sample = int(match.group("sample"))
            index = int(match.group("index"))
            if sample <= 0 or index >= 0x800 // 4:
                raise BenchmarkError("GDB reported an out-of-range RAM word")
            sample_words = ram_words.setdefault(sample, {})
            if index in sample_words:
                raise BenchmarkError(
                    f"sample {sample} repeats translated RAM word {index}"
                )
            sample_words[index] = int(match.group("value"), 16)

    expected_samples = list(range(1, len(schedule.checkpoints) + 1))
    if sorted(completed) != expected_samples:
        raise BenchmarkError(
            f"completed samples are {sorted(completed)}; "
            f"expected {expected_samples}"
        )

    statuses: list[dict[str, int]] = []
    ram_snapshots: list[dict[str, object]] = []
    cpu_snapshots: list[dict[str, int]] = []
    ram_images: list[bytes] = []
    for sample, expected_frame in enumerate(schedule.checkpoints, start=1):
        sample_words = words.get(sample, {})
        missing = [
            index
            for index in range(len(STATUS_FIELDS))
            if index not in sample_words
        ]
        if missing:
            raise BenchmarkError(
                f"sample {sample} is missing mailbox words {missing}"
            )
        status = {
            field: sample_words[index]
            for index, field in enumerate(STATUS_FIELDS)
        }
        if status["magic"] != STATUS_MAGIC:
            raise BenchmarkError(f"sample {sample} has invalid mailbox magic")
        if status["version"] != STATUS_VERSION:
            raise BenchmarkError(
                f"sample {sample} has mailbox version {status['version']}"
            )
        if status["frame"] != expected_frame:
            raise BenchmarkError(
                f"sample {sample} stopped at source frame "
                f"{status['frame']}; expected {expected_frame}"
            )
        source_records = expected_frame + 1
        bootstrap_skips = min(source_records, status["bootstrap_frames"])
        available = source_records - bootstrap_skips
        if status["area_init_hold_count"] > available:
            raise BenchmarkError(
                f"sample {sample} area-init holds exceed source records"
            )
        expected_core_frames = available - status["area_init_hold_count"]
        if status["core_frames_advanced"] != expected_core_frames:
            raise BenchmarkError(
                f"sample {sample} core count "
                f"{status['core_frames_advanced']} is not aligned to "
                f"source frame {expected_frame}"
            )
        expected_game_frames = replay_window.rendered_frames_through(
            schedule.render_ranges,
            expected_frame,
        )
        if status["game_frame_count"] != expected_game_frames:
            raise BenchmarkError(
                f"sample {sample} game-frame count is "
                f"{status['game_frame_count']}; expected "
                f"{expected_game_frames} from selective-render ranges"
            )
        if status["vblank_count"] < status["game_frame_count"]:
            raise BenchmarkError(
                f"sample {sample} reports fewer VBlanks than game frames"
            )
        if sample not in ram_records:
            raise BenchmarkError(f"sample {sample} has no RAM record")
        if sample not in cpu_records:
            raise BenchmarkError(f"sample {sample} has no CPU-register record")
        sample_ram_words = ram_words.get(sample, {})
        missing_ram_words = [
            index for index in range(0x800 // 4) if index not in sample_ram_words
        ]
        if missing_ram_words:
            raise BenchmarkError(
                f"sample {sample} is missing translated RAM words "
                f"{missing_ram_words[:8]}"
            )
        ram_image = b"".join(
            sample_ram_words[index].to_bytes(4, "big")
            for index in range(0x800 // 4)
        )
        selected_ram = ram_records[sample]
        selected_matches = (
            ram_image[0x09] == selected_ram["frame_counter"]
            and sum(value != 0 for value in ram_image[0x0F:0x15])
            == selected_ram["active_enemies"]
            and ram_image[0x0F:0x15].hex()
            == selected_ram["enemy_flags_hex"]
            and ram_image[0x16:0x1C].hex()
            == selected_ram["enemy_ids_hex"]
            and ram_image[0x71A] == selected_ram["screen_page"]
            and ram_image[0x71C] == selected_ram["screen_x"]
            and ram_image[0x06D] == selected_ram["player_page"]
            and ram_image[0x086] == selected_ram["player_x"]
        )
        if not selected_matches:
            raise BenchmarkError(
                f"sample {sample} full RAM dump disagrees with selected fields"
            )
        statuses.append(status)
        ram_snapshots.append({"sample": sample, **ram_records[sample]})
        cpu_snapshots.append({"sample": sample, **cpu_records[sample]})
        ram_images.append(ram_image)

    status_mode = _validate_status_mode(statuses, requested_mode)
    for sample, status in enumerate(statuses, start=1):
        if status_mode == "full":
            if status["render_generation"] != (
                status["game_frame_count"] & 0xFFFF
            ):
                raise BenchmarkError(
                    f"sample {sample} render generation is not aligned"
                )
            presentation_lag = (
                status["render_generation"]
                - status["presented_generation"]
            ) & 0xFFFF
            if presentation_lag > 1:
                raise BenchmarkError(
                    f"sample {sample} presentation lag is {presentation_lag}"
                )
        elif (
            status["render_generation"] != 0
            or status["presented_generation"] != 0
        ):
            raise BenchmarkError(
                f"sample {sample} logic mode reports presentation generations"
            )
    return ParsedEvidence(
        status_mode=status_mode,
        statuses=tuple(statuses),
        ram_snapshots=tuple(ram_snapshots),
        cpu_snapshots=tuple(cpu_snapshots),
        ram_images=tuple(ram_images),
    )


def compute_window_results(
    evidence: ParsedEvidence,
    schedule: replay_window.ReplayWindowSchedule,
) -> list[dict[str, int]]:
    by_frame = {status["frame"]: status for status in evidence.statuses}
    results: list[dict[str, int]] = []
    for window in schedule.windows:
        baseline = by_frame[window.start - 1]
        terminal = by_frame[window.end]
        game_frames = (
            terminal["game_frame_count"] - baseline["game_frame_count"]
        ) & 0xFFFFFFFF
        vblanks = (
            terminal["vblank_count"] - baseline["vblank_count"]
        ) & 0xFFFFFFFF
        core_frames = (
            terminal["core_frames_advanced"]
            - baseline["core_frames_advanced"]
        ) & 0xFFFFFFFF
        if core_frames != window.frame_count or game_frames != window.frame_count:
            raise BenchmarkError(
                f"window {window.start}..{window.end} advanced "
                f"source/core/game {window.frame_count}/{core_frames}/{game_frames}"
            )
        if vblanks < game_frames:
            raise BenchmarkError(
                f"window {window.start}..{window.end} reports fewer VBlanks "
                "than game frames"
            )
        results.append(
            {
                "start_frame": window.start,
                "end_frame": window.end,
                "source_frames": window.frame_count,
                "core_frames": core_frames,
                "game_frames": game_frames,
                "display_vblanks": vblanks,
                "missed_display_periods": vblanks - game_frames,
            }
        )
    return results


def build_gngeo_command(
    executable: str,
    rom_dir: Path,
    data_file: Path,
    rom_set: str,
    system: str = DEFAULT_SYSTEM,
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
        "--68kclock=0",
        "--z80clock=0",
        "--system",
        system,
        "-D",
        "-i",
        str(rom_dir),
        "-d",
        str(data_file),
        rom_set,
    ]


def wait_for_gdb(
    gdb: subprocess.Popen[str],
    gngeo: subprocess.Popen[str],
    log_file: TextIO,
    timeout_seconds: float,
) -> int:
    if gdb.stdout is None:
        raise BenchmarkError("GDB stdout pipe was not created")
    selector = selectors.DefaultSelector()
    selector.register(gdb.stdout, selectors.EVENT_READ)
    start = time.monotonic()
    next_heartbeat = start + 10.0
    try:
        while True:
            for key, _mask in selector.select(0.25):
                line = key.fileobj.readline()
                if line:
                    log_file.write(line)
                    log_file.flush()
                    if line.startswith(("REPLAY_PROGRESS_END", "REPLAY_SCREENSHOT")):
                        print(line.rstrip(), flush=True)
            status = gdb.poll()
            if status is not None:
                remainder = gdb.stdout.read()
                if remainder:
                    log_file.write(remainder)
                    log_file.flush()
                return status
            gngeo_status = gngeo.poll()
            if gngeo_status is not None:
                raise BenchmarkError(
                    f"GnGeo exited early with status {gngeo_status}"
                )
            now = time.monotonic()
            if now - start >= timeout_seconds:
                raise BenchmarkError(
                    f"benchmark exceeded {timeout_seconds:g} seconds"
                )
            if now >= next_heartbeat:
                print(
                    f"[window-benchmark] running ({int(now - start)}s elapsed)",
                    flush=True,
                )
                next_heartbeat = now + 10.0
    finally:
        selector.close()


def _required_file(path: Path, description: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise BenchmarkError(f"{description} is missing: {resolved}")
    return resolved


def run(args: argparse.Namespace) -> int:
    repository = Path(__file__).resolve().parents[1]
    schedule = replay_window.build_schedule(
        replay_window.parse_windows(args.window or replay_window.DEFAULT_WINDOW_SPEC),
        args.warmup,
    )
    output = args.output.expanduser().resolve()
    if output.exists():
        raise BenchmarkError(f"refusing to overwrite evidence: {output}")
    output.mkdir(parents=True)

    elf = _required_file(args.elf, "replay ELF")
    rom_dir = args.rom_dir.expanduser().resolve()
    if not rom_dir.is_dir():
        raise BenchmarkError(f"replay ROM directory is missing: {rom_dir}")
    cartridge = _required_file(rom_dir / f"{args.rom_set}.zip", "cartridge")
    data_file = _required_file(rom_dir / "gngeo_data.zip", "GnGeo driver data")
    replay_header = _required_file(args.replay_data_header, "replay data header")
    window_header = _required_file(args.window_header, "window schedule header")
    helper = _required_file(
        repository / "tools" / "capture_neogeo_replay_frame.py",
        "screenshot helper",
    )

    nm = resolve_executable(args.nm)
    gdb_executable = resolve_executable(args.gdb)
    gngeo_executable = resolve_executable(args.gngeo)
    xvfb_executable = resolve_executable(args.xvfb)
    stdbuf = resolve_executable(args.stdbuf)
    scrot = resolve_executable(args.scrot)
    symbols = resolve_symbols(elf, nm)
    screenshots = {
        index * 2: output / f"source-frame-{window.end}.png"
        for index, window in enumerate(schedule.windows, start=1)
    }
    ram_dumps = {
        sample: output / f"source-frame-{frame}.ram.bin"
        for sample, frame in enumerate(schedule.checkpoints, start=1)
    }
    gdb_script_path = output / "window-benchmark.gdb"
    gdb_script_path.write_text(
        build_gdb_script(
            symbols,
            schedule,
            helper,
            screenshots,
            str(Path(sys.executable).resolve()),
            scrot,
        ),
        encoding="ascii",
    )
    gngeo_command = build_gngeo_command(
        gngeo_executable,
        rom_dir,
        data_file,
        args.rom_set,
        args.system,
    )
    gdb_command = [
        stdbuf,
        "-oL",
        "-eL",
        gdb_executable,
        "-nx",
        "-q",
        "-batch",
        str(elf),
        "-x",
        str(gdb_script_path),
    ]
    result: dict[str, object] = {
        "schema_version": 1,
        "outcome": "starting",
        "passed": False,
        "status_mode_requested": args.status_mode,
        "system": args.system,
        "rom_set": args.rom_set,
        "windows": [
            {"start_frame": window.start, "end_frame": window.end}
            for window in schedule.windows
        ],
        "warmup_frames": schedule.warmup_frames,
        "checkpoint_frames": list(schedule.checkpoints),
        "render_ranges": [
            {"start_frame": window.start, "end_frame": window.end}
            for window in schedule.render_ranges
        ],
        "commands": {"gngeo": gngeo_command, "gdb": gdb_command},
        "artifacts": {
            "elf": {"path": str(elf), "sha256": sha256_file(elf)},
            "cartridge": {
                "path": str(cartridge),
                "sha256": sha256_file(cartridge),
            },
            "gngeo_data": {
                "path": str(data_file),
                "sha256": sha256_file(data_file),
            },
            "replay_data_header": {
                "path": str(replay_header),
                "sha256": sha256_file(replay_header),
            },
            "window_header": {
                "path": str(window_header),
                "sha256": sha256_file(window_header),
            },
        },
    }
    result_path = output / "result.json"
    result_path.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    owned = cadence.OwnedProcessGroups()
    xvfb_log_path = output / "xvfb.log"
    gngeo_log_path = output / "gngeo.log"
    gdb_log_path = output / "gdb.log"
    try:
        cadence.reject_occupied_debug_port()
        with (
            xvfb_log_path.open("w", buffering=1) as xvfb_log,
            gngeo_log_path.open("w", buffering=1) as gngeo_log,
            gdb_log_path.open("w", buffering=1) as gdb_log,
        ):
            _xvfb, display = cadence._start_xvfb(
                xvfb_executable,
                xvfb_log,
                owned,
                10.0,
            )
            environment = os.environ.copy()
            environment["DISPLAY"] = display
            gngeo = owned.add(
                "GnGeo window benchmark",
                subprocess.Popen(
                    gngeo_command,
                    stdout=gngeo_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=environment,
                    start_new_session=True,
                ),
            )
            cadence._wait_for_debug_listener(gngeo, 10.0)
            gdb = owned.add(
                "GDB window benchmark",
                subprocess.Popen(
                    gdb_command,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=environment,
                    start_new_session=True,
                    bufsize=1,
                ),
            )
            gdb_status = wait_for_gdb(gdb, gngeo, gdb_log, args.timeout)
            if gdb_status != 0:
                raise BenchmarkError(f"GDB exited with status {gdb_status}")

        evidence = parse_evidence(
            gdb_log_path.read_text(encoding="utf-8"),
            schedule,
            args.status_mode,
        )
        window_results = compute_window_results(evidence, schedule)
        checkpoint_artifacts = []
        for sample, frame in enumerate(schedule.checkpoints, start=1):
            ram_dump = ram_dumps[sample]
            ram_dump.write_bytes(evidence.ram_images[sample - 1])
            checkpoint_artifacts.append(
                {
                    "sample": sample,
                    "frame": frame,
                    "translated_ram": {
                        "path": str(ram_dump),
                        "bytes": ram_dump.stat().st_size,
                        "sha256": sha256_file(ram_dump),
                    },
                    "cpu_registers": evidence.cpu_snapshots[sample - 1],
                }
            )
        for frame, screenshot in (
            (window.end, screenshots[index * 2])
            for index, window in enumerate(schedule.windows, start=1)
        ):
            _required_file(screenshot, f"source-frame {frame} screenshot")

        result.update(
            {
                "outcome": "passed",
                "passed": True,
                "display": display,
                "status_mode": evidence.status_mode,
                "status_snapshots": list(evidence.statuses),
                "ram_snapshots": list(evidence.ram_snapshots),
                "cpu_snapshots": list(evidence.cpu_snapshots),
                "checkpoint_artifacts": checkpoint_artifacts,
                "window_results": window_results,
                "artifacts": {
                    **result["artifacts"],
                    "gdb_log": {
                        "path": str(gdb_log_path),
                        "sha256": sha256_file(gdb_log_path),
                    },
                    "screenshots": [
                        {
                            "path": str(screenshot),
                            "sha256": sha256_file(screenshot),
                            "bytes": screenshot.stat().st_size,
                        }
                        for screenshot in screenshots.values()
                    ],
                },
            }
        )
        result_path.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        for window_result in window_results:
            print(
                "WINDOW_RESULT "
                f"source={window_result['start_frame']}.."
                f"{window_result['end_frame']} "
                f"game={window_result['game_frames']} "
                f"vblank={window_result['display_vblanks']} "
                f"missed={window_result['missed_display_periods']}",
                flush=True,
            )
        print(f"[window-benchmark] evidence: {output}", flush=True)
        return 0
    except BaseException as error:
        result.update(
            {
                "outcome": "failed",
                "passed": False,
                "error": f"{type(error).__name__}: {error}",
            }
        )
        result_path.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        raise
    finally:
        owned.terminate_all()


def argument_parser() -> argparse.ArgumentParser:
    repository = Path(__file__).resolve().parents[1]
    replay = repository / "platform" / "neogeo" / "build" / "replay-rendered"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--window", action="append")
    parser.add_argument(
        "--warmup",
        type=int,
        default=replay_window.DEFAULT_WARMUP_FRAMES,
    )
    parser.add_argument("--elf", type=Path, default=replay / "smbneogeo-replay.elf")
    parser.add_argument("--rom-dir", type=Path, default=replay / "rom")
    parser.add_argument(
        "--replay-data-header",
        type=Path,
        default=replay / "smb_replay_data.h",
    )
    parser.add_argument(
        "--window-header",
        type=Path,
        default=replay / "smb_neogeo_replay_windows.h",
    )
    parser.add_argument("--rom-set", default=DEFAULT_ROM_SET)
    parser.add_argument(
        "--system",
        choices=("arcade", "home"),
        default=DEFAULT_SYSTEM,
    )
    parser.add_argument(
        "--status-mode",
        choices=("auto", "full", "logic"),
        default="auto",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=900.0)
    parser.add_argument("--nm", default="m68k-neogeo-elf-nm")
    parser.add_argument("--gdb", default="m68k-neogeo-elf-gdb")
    parser.add_argument("--gngeo", default="ngdevkit-gngeo")
    parser.add_argument("--xvfb", default="Xvfb")
    parser.add_argument("--stdbuf", default="stdbuf")
    parser.add_argument("--scrot", default="scrot")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = argument_parser()
    args = parser.parse_args(argv)
    if not 30.0 <= args.timeout <= 7200.0:
        parser.error("timeout must be between 30 and 7200 seconds")

    def interrupt(_signum, _frame) -> None:
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupt)
    signal.signal(signal.SIGINT, interrupt)
    try:
        return run(args)
    except (
        BenchmarkError,
        replay_window.WindowError,
        cadence.CadenceError,
        OSError,
        subprocess.SubprocessError,
    ) as error:
        print(f"window benchmark failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
