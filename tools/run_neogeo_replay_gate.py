#!/usr/bin/env python3
"""Run a Neo Geo replay-gate cartridge and capture bounded evidence."""

from __future__ import annotations

import argparse
from contextlib import ExitStack
from dataclasses import asdict, dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Sequence

import measure_neogeo_cadence as cadence
import capture_neogeo_replay_frame as frame_capture


DEBUG_HOST = cadence.DEBUG_HOST
DEBUG_PORT = cadence.DEBUG_PORT
STATUS_MAGIC = 0x534D4252
STATUS_VERSION = 4
STATUS_WORD_COUNT = 30
STATUS_BYTES = STATUS_WORD_COUNT * 4
COMPLETE_RESULT = 1
INVALID_STAGE_RESULT = 2
INCOMPLETE_RESULT = 0x100
ALL_STAGES_MASK = 0xFFFFFFFF
FINAL_STAGE = 31
NO_STAGE = 0xFF
FINAL_STABLE_FRAMES = 60
VICTORY_MODE = 2
VICTORY_TASK = 4
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
RESULT_SCHEMA_VERSION = 4
DEFAULT_ROM_SET = "smbneo"
DEFAULT_68K_OVERCLOCK = 1000
DEFAULT_RENDERED_68K_OVERCLOCK = 0
DEFAULT_RENDERED_TIMEOUT_SECONDS = 7200.0
MIN_DERIVED_TIMEOUT_SECONDS = 180.0
BASELINE_TIMEOUT_SECONDS = 1800.0
MAX_TIMEOUT_SECONDS = 86400.0
MAX_68K_OVERCLOCK = 10000
MAX_DIAGNOSTIC_CHARS = 4096
MAX_RESULT_JSON_BYTES = 128 * 1024
DEFAULT_HEARTBEAT_SECONDS = 5.0
DEFAULT_STARTUP_TIMEOUT_SECONDS = 10.0
DEFAULT_SCREENSHOT_TIMEOUT_SECONDS = 10.0
MAX_SCREENSHOT_TIMEOUT_SECONDS = 60.0
DEFAULT_DISPLAY_SETTLE_SECONDS = (
    frame_capture.DEFAULT_DISPLAY_SETTLE_SECONDS
)
MAX_DISPLAY_SETTLE_SECONDS = frame_capture.MAX_DISPLAY_SETTLE_SECONDS
MAX_SCREENSHOT_BYTES = 4 * 1024 * 1024
EXPECTED_STAGE_CAPTURES = 32
REQUIRED_BIOS_FILES = ("aes.zip", "neogeo.zip")

REQUIRED_SYMBOL_NAMES = (
    "neogeo_replay_pass_trap",
    "neogeo_replay_fail_trap",
    "neogeo_replay_progress_trap",
    "neogeo_replay_stage_trap",
    "neogeo_replay_transition_trap",
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
STAGE_WORD_RE = re.compile(
    r"^REPLAY_STAGE_WORD\s+"
    r"sample=(?P<sample>[0-9]+)\s+"
    r"index=(?P<index>[0-9]+)\s+"
    r"value=0x(?P<value>[0-9a-fA-F]{8})\s*$"
)
STAGE_END_RE = re.compile(
    r"^REPLAY_STAGE_END\s+sample=(?P<sample>[0-9]+)\s*$"
)
STAGE_IMAGE_RE = re.compile(r"^stage-(?P<index>[0-9]{4})\.png$")
TRANSITION_WORD_RE = re.compile(
    r"^REPLAY_TRANSITION_WORD\s+"
    r"sample=(?P<sample>[0-9]+)\s+"
    r"index=(?P<index>[0-9]+)\s+"
    r"value=0x(?P<value>[0-9a-fA-F]{8})\s*$"
)
TRANSITION_END_RE = re.compile(
    r"^REPLAY_TRANSITION_END\s+sample=(?P<sample>[0-9]+)\s*$"
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
    stage_point: ElfSymbol
    transition_point: ElfSymbol


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
    bootstrap_frames: int
    area_init_hold_frames: int
    area_init_hold_count: int
    core_frames_advanced: int
    rendering_enabled: int
    game_frame_count: int
    vblank_count: int
    stage_settle_frames: int
    render_generation: int
    presented_generation: int


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
class StageCapture:
    sample: int
    status: ReplayStatus
    raw_words: tuple[int, ...]


@dataclass(frozen=True)
class TransitionCapture:
    sample: int
    status: ReplayStatus
    raw_words: tuple[int, ...]


@dataclass(frozen=True)
class ScreenshotConfig:
    python: str
    helper: Path
    scrot: str
    directory: Path
    timeout_seconds: float
    display_settle_seconds: float
    maximum_bytes: int


@dataclass(frozen=True)
class GateClassification:
    outcome: str
    passed: bool
    detail: str


def parse_nm_symbols(output: str) -> dict[str, ElfSymbol]:
    """Expose the shared strict POSIX ``nm -S`` parser."""

    return cadence.parse_nm_symbols(output)


def replay_symbols_from_nm(output: str) -> ReplaySymbols:
    """Validate the ELF symbols used by the remote debugger."""

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
    stage_point = parsed["neogeo_replay_stage_trap"]
    transition_point = parsed["neogeo_replay_transition_trap"]

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
    for point in (progress_point, stage_point, transition_point):
        if point.kind not in "TtWw" or point.size <= 0:
            raise ReplayGateError(
                f"{point.name} is not a non-empty code symbol "
                f"(nm type {point.kind}, size {point.size})"
            )
    trap_addresses = {
        pass_trap.address,
        fail_trap.address,
        progress_point.address,
        stage_point.address,
        transition_point.address,
    }
    if len(trap_addresses) != 5:
        raise ReplayGateError("replay trap symbols do not have unique addresses")

    return ReplaySymbols(
        pass_trap=pass_trap,
        fail_trap=fail_trap,
        status=status,
        progress_point=progress_point,
        stage_point=stage_point,
        transition_point=transition_point,
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


def _gdb_shell_command(arguments: Sequence[object]) -> str:
    values = [str(value) for value in arguments]
    if any("\n" in value or "\r" in value for value in values):
        raise ReplayGateError(
            "GDB shell-command arguments must not contain newlines"
        )
    return "  shell " + shlex.join(values)


def _screenshot_command(
    config: ScreenshotConfig,
    *,
    sequence: str | None,
) -> str:
    destination = (
        ("--sequence-dir", config.directory / sequence)
        if sequence is not None
        else ("--output", config.directory / "terminal.png")
    )
    return _gdb_shell_command(
        [
            config.python,
            config.helper,
            destination[0],
            destination[1],
            "--scrot",
            config.scrot,
            "--timeout",
            f"{config.timeout_seconds:g}",
            "--display-settle-seconds",
            f"{config.display_settle_seconds:g}",
            "--max-bytes",
            config.maximum_bytes,
        ]
    )


def build_gdb_script(
    symbols: ReplaySymbols,
    host: str = DEBUG_HOST,
    port: int = DEBUG_PORT,
    screenshot: ScreenshotConfig | None = None,
) -> str:
    """Build a trap probe with every raw 32-bit mailbox word."""

    lines = [
        "set pagination off",
        "set confirm off",
        "set verbose off",
        "set remotetimeout 60",
        "set $gate_trap = 0",
        "set $progress_sample = 0",
        "set $stage_sample = 0",
        "set $transition_sample = 0",
        f"target remote {host}:{port}",
        f"break *{_gdb_address(symbols.pass_trap.address)}",
        "commands",
        "  silent",
        "  set $gate_trap = 1",
    ]
    if screenshot is not None:
        lines.append(_screenshot_command(screenshot, sequence=None))
    lines.extend(
        [
            "end",
            f"break *{_gdb_address(symbols.fail_trap.address)}",
            "commands",
            "  silent",
            "  set $gate_trap = 2",
        ]
    )
    if screenshot is not None:
        lines.append(_screenshot_command(screenshot, sequence=None))
    lines.extend(
        [
            "end",
            f"break *{_gdb_address(symbols.progress_point.address)}",
            "commands",
            "  silent",
            "  set $progress_sample = $progress_sample + 1",
        ]
    )

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
            f"break *{_gdb_address(symbols.stage_point.address)}",
            "commands",
            "  silent",
            "  set $stage_sample = $stage_sample + 1",
        ]
    )

    for index in range(STATUS_WORD_COUNT):
        address = symbols.status.address + index * 4
        lines.extend(
            [
                (
                    f"  set $stage_mb{index} = "
                    f"*(unsigned int *){_gdb_address(address)}"
                ),
                (
                    '  printf "REPLAY_STAGE_WORD '
                    f'sample=%u index={index} value=0x%08x\\n", '
                    f"$stage_sample, $stage_mb{index}"
                ),
            ]
        )

    if screenshot is not None:
        lines.append(_screenshot_command(screenshot, sequence="stages"))
    lines.extend(
        [
            (
                '  printf "REPLAY_STAGE_END sample=%u\\n", '
                "$stage_sample"
            ),
            "  continue",
            "end",
            f"break *{_gdb_address(symbols.transition_point.address)}",
            "commands",
            "  silent",
            "  set $transition_sample = $transition_sample + 1",
        ]
    )

    for index in range(STATUS_WORD_COUNT):
        address = symbols.status.address + index * 4
        lines.extend(
            [
                (
                    f"  set $transition_mb{index} = "
                    f"*(unsigned int *){_gdb_address(address)}"
                ),
                (
                    '  printf "REPLAY_TRANSITION_WORD '
                    f'sample=%u index={index} value=0x%08x\\n", '
                    f"$transition_sample, $transition_mb{index}"
                ),
            ]
        )

    if screenshot is not None:
        lines.append(
            _screenshot_command(screenshot, sequence="transitions")
        )
    lines.extend(
        [
            (
                '  printf "REPLAY_TRANSITION_END sample=%u\\n", '
                "$transition_sample"
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
    """Parse and validate one complete debugger mailbox snapshot."""

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
    if status.result not in RESULT_NAMES:
        raise ReplayGateError(
            f"mailbox result 0x{status.result:08x} is unknown"
        )
    byte_fields = {
        "controller_state": status.controller_state,
        "oper_mode": status.oper_mode,
        "oper_mode_task": status.oper_mode_task,
        "world": status.world,
        "level": status.level,
        "world_end_timer": status.world_end_timer,
        "victory_stable_frames": status.victory_stable_frames,
    }
    for name, value in byte_fields.items():
        if value > 0xFF:
            raise ReplayGateError(
                f"mailbox {name}={value} is outside the uint8 range"
            )
    if status.current_stage > FINAL_STAGE and status.current_stage != NO_STAGE:
        raise ReplayGateError(
            f"mailbox current_stage={status.current_stage} is invalid"
        )
    invalid_coordinates = status.world >= 8 or status.level >= 4
    if status.result == INVALID_STAGE_RESULT:
        if not invalid_coordinates:
            raise ReplayGateError(
                "invalid-stage mailbox has valid world/level coordinates"
            )
    elif invalid_coordinates:
        raise ReplayGateError(
            f"mailbox world/level is {status.world}/{status.level}; "
            "expected zero-based values below 8/4"
        )
    if status.victory_stable_frames > FINAL_STABLE_FRAMES:
        raise ReplayGateError(
            "mailbox victory-stable interval exceeds the gate limit"
        )
    if status.hardware_playable not in (0, 1):
        raise ReplayGateError(
            "mailbox hardware_playable must be zero or one"
        )
    if status.rendering_enabled not in (0, 1):
        raise ReplayGateError(
            "mailbox rendering_enabled must be zero or one"
        )
    if status.stage_settle_frames > 0xFF:
        raise ReplayGateError(
            "mailbox stage-settle interval exceeds uint8"
        )
    if (
        status.hardware_playable == 1
        and status.opposite_direction_transitions != 0
    ):
        raise ReplayGateError(
            "hardware-playable mailbox reports opposite directions"
        )
    if status.ram_init_option not in (0, 2):
        raise ReplayGateError(
            f"mailbox RAM initialization option "
            f"{status.ram_init_option} is unsupported"
        )
    if status.replay_end_frame == 0:
        raise ReplayGateError("mailbox replay_end_frame must be positive")
    if status.bootstrap_frames > status.replay_end_frame:
        raise ReplayGateError(
            "mailbox bootstrap interval exceeds the replay"
        )
    if status.area_init_hold_frames > 0xFF:
        raise ReplayGateError(
            "mailbox area-init hold interval exceeds uint8"
        )
    if status.completed_mask & ~status.entered_mask:
        raise ReplayGateError(
            "mailbox completed stages were never entered"
        )

    if status.frame < status.replay_end_frame:
        if status.tail_frame != 0:
            raise ReplayGateError(
                "mailbox reports tail frames before replay input ended"
            )
        source_records = status.frame + 1
        tail_core_frames = 0
    else:
        if status.frame != status.replay_end_frame + status.tail_frame:
            raise ReplayGateError(
                "mailbox frame/tail accounting is inconsistent"
            )
        source_records = status.replay_end_frame
        tail_core_frames = (
            status.tail_frame
            if status.result == INCOMPLETE_RESULT
            else status.tail_frame + 1
        )

    bootstrap_skips = min(source_records, status.bootstrap_frames)
    available_source_frames = source_records - bootstrap_skips
    if status.area_init_hold_count > available_source_frames:
        raise ReplayGateError(
            "mailbox area-init hold count exceeds available source frames"
        )
    expected_core_frames = (
        available_source_frames
        - status.area_init_hold_count
        + tail_core_frames
    )
    if status.core_frames_advanced != expected_core_frames:
        raise ReplayGateError(
            f"mailbox core-frame accounting is "
            f"{status.core_frames_advanced}; expected "
            f"{expected_core_frames}"
        )
    if status.rendering_enabled == 0:
        if status.game_frame_count != 0:
            raise ReplayGateError(
                "non-rendered mailbox reports rendered game frames"
            )
        if (
            status.render_generation != 0
            or status.presented_generation != 0
        ):
            raise ReplayGateError(
                "non-rendered mailbox reports presentation generations"
            )
    else:
        if status.game_frame_count != status.core_frames_advanced:
            raise ReplayGateError(
                f"mailbox rendered-frame accounting is "
                f"{status.game_frame_count}; expected "
                f"{status.core_frames_advanced}"
            )
        if status.render_generation > 0xFFFF:
            raise ReplayGateError(
                "mailbox render generation exceeds uint16"
            )
        if status.presented_generation > 0xFFFF:
            raise ReplayGateError(
                "mailbox presented generation exceeds uint16"
            )
        expected_render_generation = status.game_frame_count & 0xFFFF
        if status.render_generation != expected_render_generation:
            raise ReplayGateError(
                f"mailbox render generation is "
                f"{status.render_generation}; expected "
                f"{expected_render_generation}"
            )
        presentation_lag = (
            status.render_generation - status.presented_generation
        ) & 0xFFFF
        if presentation_lag > 1:
            raise ReplayGateError(
                "mailbox presented generation is more than one render "
                "behind"
            )
        minimum_vblanks = status.game_frame_count + 1
        if status.vblank_count < minimum_vblanks:
            raise ReplayGateError(
                f"mailbox VBlank count is {status.vblank_count}; expected "
                f"at least {minimum_vblanks}"
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


def parse_stage_snapshots(output: str) -> list[StageCapture]:
    """Parse complete stage-linked mailbox snapshots in capture order."""

    samples: dict[int, dict[int, int]] = {}
    completed_samples: set[int] = set()
    for line in output.splitlines():
        stripped = line.strip()
        word_match = STAGE_WORD_RE.match(stripped)
        if word_match is not None:
            sample = int(word_match.group("sample"))
            index = int(word_match.group("index"))
            if sample <= 0:
                raise ReplayGateError(
                    "GDB reported a non-positive stage sample"
                )
            if index >= STATUS_WORD_COUNT:
                raise ReplayGateError(
                    f"GDB reported out-of-range stage word {index}"
                )
            sample_words = samples.setdefault(sample, {})
            if index in sample_words:
                raise ReplayGateError(
                    f"GDB reported stage sample {sample} word {index} "
                    "more than once"
                )
            sample_words[index] = int(word_match.group("value"), 16)
            continue

        end_match = STAGE_END_RE.match(stripped)
        if end_match is not None:
            sample = int(end_match.group("sample"))
            if sample in completed_samples:
                raise ReplayGateError(
                    f"GDB ended stage sample {sample} more than once"
                )
            completed_samples.add(sample)

    ordered_samples = sorted(completed_samples)
    expected_samples = list(range(1, len(ordered_samples) + 1))
    if ordered_samples != expected_samples:
        raise ReplayGateError(
            "GDB stage samples are not contiguous from one"
        )

    captures: list[StageCapture] = []
    previous_frame = -1
    for sample in ordered_samples:
        sample_words = samples.get(sample, {})
        missing = [
            str(index)
            for index in range(STATUS_WORD_COUNT)
            if index not in sample_words
        ]
        if missing:
            raise ReplayGateError(
                f"stage sample {sample} is missing mailbox words: "
                + ", ".join(missing)
            )
        raw_words = tuple(
            sample_words[index] for index in range(STATUS_WORD_COUNT)
        )
        status = parse_mailbox_words(raw_words)
        if status.frame <= previous_frame:
            raise ReplayGateError(
                f"stage frame did not advance at sample {sample}"
            )
        captures.append(
            StageCapture(
                sample=sample,
                status=status,
                raw_words=raw_words,
            )
        )
        previous_frame = status.frame
    return captures


def parse_transition_snapshots(output: str) -> list[TransitionCapture]:
    """Parse complete immediate-transition mailbox snapshots."""

    samples: dict[int, dict[int, int]] = {}
    completed_samples: set[int] = set()
    for line in output.splitlines():
        stripped = line.strip()
        word_match = TRANSITION_WORD_RE.match(stripped)
        if word_match is not None:
            sample = int(word_match.group("sample"))
            index = int(word_match.group("index"))
            if sample <= 0:
                raise ReplayGateError(
                    "GDB reported a non-positive transition sample"
                )
            if index >= STATUS_WORD_COUNT:
                raise ReplayGateError(
                    f"GDB reported out-of-range transition word {index}"
                )
            sample_words = samples.setdefault(sample, {})
            if index in sample_words:
                raise ReplayGateError(
                    f"GDB reported transition sample {sample} word {index} "
                    "more than once"
                )
            sample_words[index] = int(word_match.group("value"), 16)
            continue

        end_match = TRANSITION_END_RE.match(stripped)
        if end_match is not None:
            sample = int(end_match.group("sample"))
            if sample in completed_samples:
                raise ReplayGateError(
                    f"GDB ended transition sample {sample} more than once"
                )
            completed_samples.add(sample)

    ordered_samples = sorted(completed_samples)
    if ordered_samples != list(range(1, len(ordered_samples) + 1)):
        raise ReplayGateError(
            "GDB transition samples are not contiguous from one"
        )

    captures: list[TransitionCapture] = []
    previous_frame = -1
    for sample in ordered_samples:
        sample_words = samples.get(sample, {})
        missing = [
            str(index)
            for index in range(STATUS_WORD_COUNT)
            if index not in sample_words
        ]
        if missing:
            raise ReplayGateError(
                f"transition sample {sample} is missing mailbox words: "
                + ", ".join(missing)
            )
        raw_words = tuple(
            sample_words[index] for index in range(STATUS_WORD_COUNT)
        )
        status = parse_mailbox_words(raw_words)
        if status.frame <= previous_frame:
            raise ReplayGateError(
                f"transition frame did not advance at sample {sample}"
            )
        captures.append(
            TransitionCapture(
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
        and status.current_stage == FINAL_STAGE
        and status.victory_stable_frames == FINAL_STABLE_FRAMES
        and status.oper_mode == VICTORY_MODE
        and status.oper_mode_task == VICTORY_TASK
        and status.world == 7
        and status.level == 3
        and status.world_end_timer == 0
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
    return [
        executable,
        "-nx",
        "-q",
        "-batch",
        str(elf),
        "-x",
        str(script),
    ]


def _wait_for_gate(
    gdb: subprocess.Popen[str],
    gngeo: subprocess.Popen[str],
    heartbeat_seconds: float,
    timeout_seconds: float,
    gdb_log_path: Path,
    require_rendered: bool = False,
    screenshot_dir: Path | None = None,
) -> int:
    start = time.monotonic()
    next_heartbeat = start + heartbeat_seconds
    reported_samples = 0
    reported_stages = 0
    reported_transitions = 0
    latest_progress: ProgressCapture | None = None
    while True:
        try:
            log_text = gdb_log_path.read_text(errors="replace")
        except FileNotFoundError:
            log_text = ""
        progress = parse_progress_snapshots(log_text)
        stages = parse_stage_snapshots(log_text)
        transitions = parse_transition_snapshots(log_text)
        for snapshot in progress[reported_samples:]:
            print(
                "[replay-gate] progress "
                f"sample={snapshot.sample} "
                f"{_status_summary(snapshot.status)}",
                flush=True,
            )
            latest_progress = snapshot
        reported_samples = len(progress)

        for snapshot in transitions[reported_transitions:]:
            if require_rendered:
                if screenshot_dir is None:
                    raise ReplayGateError(
                        "rendered evidence has no screenshot directory"
                    )
                transition_image = (
                    screenshot_dir
                    / "transitions"
                    / f"stage-{snapshot.sample:04d}.png"
                )
                _screenshot_content_record(
                    transition_image,
                    MAX_SCREENSHOT_BYTES,
                    require_visible_content=False,
                )
            print(
                "[replay-gate] transition "
                f"sample={snapshot.sample} "
                f"{_status_summary(snapshot.status)}",
                flush=True,
            )
        reported_transitions = len(transitions)

        for snapshot in stages[reported_stages:]:
            if require_rendered:
                if screenshot_dir is None:
                    raise ReplayGateError(
                        "rendered evidence has no screenshot directory"
                    )
                if snapshot.sample > len(transitions):
                    raise ReplayGateError(
                        f"stage sample {snapshot.sample} has no paired "
                        "transition"
                    )
                transition = transitions[snapshot.sample - 1]
                _validate_rendered_stage_pair(
                    snapshot,
                    transition,
                    snapshot.sample - 1,
                )
                stage_image = (
                    screenshot_dir
                    / "stages"
                    / f"stage-{snapshot.sample:04d}.png"
                )
                transition_image = (
                    screenshot_dir
                    / "transitions"
                    / f"stage-{snapshot.sample:04d}.png"
                )
                stage_record = _screenshot_content_record(
                    stage_image,
                    MAX_SCREENSHOT_BYTES,
                )
                transition_record = _screenshot_content_record(
                    transition_image,
                    MAX_SCREENSHOT_BYTES,
                    require_visible_content=False,
                )
                if (
                    stage_record["viewport"]["playfield"]["pixel_sha256"]
                    == transition_record["viewport"]["playfield"][
                        "pixel_sha256"
                    ]
                ):
                    raise ReplayGateError(
                        f"stage {snapshot.sample - 1} settled playfield is "
                        "pixel-identical to its immediate transition"
                    )
            print(
                "[replay-gate] stage checkpoint "
                f"sample={snapshot.sample} "
                f"{_status_summary(snapshot.status)} "
                f"game_frames={snapshot.status.game_frame_count} "
                f"vblanks={snapshot.status.vblank_count}",
                flush=True,
            )
        reported_stages = len(stages)

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


def _artifact_record(
    path: Path,
    *,
    relative_to: Path | None = None,
) -> dict[str, object]:
    recorded_path = path
    if relative_to is not None:
        try:
            recorded_path = path.relative_to(relative_to)
        except ValueError as error:
            raise ReplayGateError(
                f"artifact is outside its evidence directory: {path}"
            ) from error
    return {
        "path": str(recorded_path),
        "bytes": path.stat().st_size,
        "sha256": cadence._sha256_file(path),
    }


def _image_color_stats(
    image: object,
    maximum_colors: int,
) -> tuple[int, int]:
    colors = image.getcolors(maxcolors=maximum_colors)
    if colors is None:
        pixels = image.tobytes()
        unique_colors = maximum_colors
        nonblack_pixels = sum(
            1
            for offset in range(0, len(pixels), 3)
            if pixels[offset : offset + 3] != b"\0\0\0"
        )
        return unique_colors, nonblack_pixels
    return (
        len(colors),
        sum(
            count
            for count, color in colors
            if color != (0, 0, 0)
        ),
    )


def _screenshot_content_record(
    path: Path,
    maximum_bytes: int,
    *,
    require_visible_content: bool = True,
    relative_to: Path | None = None,
) -> dict[str, object]:
    try:
        width, height, size = frame_capture.inspect_png(
            path,
            maximum_bytes,
        )
    except frame_capture.CaptureError as error:
        raise ReplayGateError(str(error)) from error
    if width < 320 or height < 224:
        raise ReplayGateError(
            f"screenshot is {width}x{height}; expected at least 320x224: "
            f"{path}"
        )

    try:
        from PIL import Image, UnidentifiedImageError
    except ImportError as error:
        raise ReplayGateError(
            "rendered evidence requires the Pillow Python package"
        ) from error

    try:
        with Image.open(path) as source:
            source.load()
            image = source.convert("RGB")
    except (OSError, UnidentifiedImageError) as error:
        raise ReplayGateError(
            f"cannot decode screenshot pixels: {path}: {error}"
        ) from error
    if image.size != (width, height):
        raise ReplayGateError(
            f"decoded screenshot dimensions changed for {path}"
        )

    left = (width - 320) // 2
    top = (height - 224) // 2
    viewport = image.crop((left, top, left + 320, top + 224))
    pixels = viewport.tobytes()
    playfield = viewport.crop((0, 32, 320, 224))
    playfield_pixels = playfield.tobytes()
    unique_colors, nonblack_pixels = _image_color_stats(
        viewport,
        320 * 224,
    )
    playfield_unique_colors, playfield_nonblack_pixels = (
        _image_color_stats(playfield, 320 * 192)
    )
    if (
        require_visible_content
        and (
            unique_colors < 4
            or nonblack_pixels < 256
            or playfield_unique_colors < 3
            or playfield_nonblack_pixels < 256
        )
    ):
        raise ReplayGateError(
            f"screenshot playfield lacks visible game content "
            f"(viewport: {unique_colors} colors, "
            f"{nonblack_pixels} non-black pixels; playfield: "
            f"{playfield_unique_colors} colors, "
            f"{playfield_nonblack_pixels} non-black pixels): "
            f"{path}"
        )

    recorded_path = path
    if relative_to is not None:
        try:
            recorded_path = path.relative_to(relative_to)
        except ValueError as error:
            raise ReplayGateError(
                f"screenshot is outside its evidence directory: {path}"
            ) from error
    return {
        "path": str(recorded_path),
        "bytes": size,
        "sha256": cadence._sha256_file(path),
        "width": width,
        "height": height,
        "viewport": {
            "left": left,
            "top": top,
            "width": 320,
            "height": 224,
            "unique_colors": unique_colors,
            "nonblack_pixels": nonblack_pixels,
            "pixel_sha256": hashlib.sha256(pixels).hexdigest(),
            "playfield": {
                "top": 32,
                "width": 320,
                "height": 192,
                "unique_colors": playfield_unique_colors,
                "nonblack_pixels": playfield_nonblack_pixels,
                "pixel_sha256":
                    hashlib.sha256(playfield_pixels).hexdigest(),
            },
        },
    }


def _validate_rendered_stage_pair(
    snapshot: StageCapture,
    transition: TransitionCapture,
    expected_stage: int,
) -> None:
    status = snapshot.status
    transition_status = transition.status
    if (
        snapshot.sample != expected_stage + 1
        or transition.sample != expected_stage + 1
    ):
        raise ReplayGateError(
            "stage or transition samples are out of order"
        )
    if (
        status.rendering_enabled != 1
        or transition_status.rendering_enabled != 1
    ):
        raise ReplayGateError(
            f"stage {expected_stage} did not execute the renderer"
        )
    if (
        status.hardware_playable != 1
        or transition_status.hardware_playable != 1
        or status.opposite_direction_transitions != 0
        or transition_status.opposite_direction_transitions != 0
    ):
        raise ReplayGateError(
            f"stage {expected_stage} is not hardware-playable"
        )
    if (
        status.stage_settle_frames != 2
        or transition_status.stage_settle_frames != 2
    ):
        raise ReplayGateError(
            f"stage {expected_stage} was captured without two settling "
            "rendered frames"
        )
    expected_entered = (
        ALL_STAGES_MASK
        if expected_stage == FINAL_STAGE
        else (1 << (expected_stage + 1)) - 1
    )
    expected_completed = (
        0 if expected_stage == 0 else (1 << expected_stage) - 1
    )
    for label, candidate in (
        ("transition", transition_status),
        ("settled", status),
    ):
        if candidate.presented_generation != candidate.render_generation:
            raise ReplayGateError(
                f"stage {expected_stage} {label} screenshot is not bound "
                "to its rendered generation"
            )
        if (
            candidate.current_stage != expected_stage
            or candidate.world != expected_stage // 4
            or candidate.level != expected_stage % 4
            or candidate.entered_mask != expected_entered
            or candidate.completed_mask != expected_completed
            or candidate.oper_mode != 1
            or candidate.oper_mode_task != 3
        ):
            raise ReplayGateError(
                f"{label} sample {snapshot.sample} does not describe "
                f"ordered active stage {expected_stage}"
            )
    settle_delta = status.stage_settle_frames
    render_generation_delta = (
        status.render_generation - transition_status.render_generation
    ) & 0xFFFF
    if (
        status.frame - transition_status.frame != settle_delta
        or status.core_frames_advanced
        - transition_status.core_frames_advanced
        != settle_delta
        or status.game_frame_count
        - transition_status.game_frame_count
        != settle_delta
        or render_generation_delta != settle_delta
        or status.vblank_count - transition_status.vblank_count
        < settle_delta
    ):
        raise ReplayGateError(
            f"stage {expected_stage} settling counters are inconsistent"
        )


def _rendered_evidence_result(
    stages: Sequence[StageCapture],
    transitions: Sequence[TransitionCapture],
    terminal: DebuggerCapture,
    screenshot_dir: Path,
    maximum_bytes: int,
    require_complete: bool,
) -> dict[str, object]:
    def sequence_paths(directory: Path, label: str) -> dict[int, Path]:
        paths: dict[int, Path] = {}
        for path in directory.iterdir():
            match = STAGE_IMAGE_RE.fullmatch(path.name)
            if match is None:
                continue
            index = int(match.group("index"))
            if index in paths:
                raise ReplayGateError(
                    f"duplicate {label} screenshot index {index}"
                )
            paths[index] = path
        return paths

    stage_image_paths = sequence_paths(
        screenshot_dir / "stages",
        "stage",
    )
    transition_image_paths = sequence_paths(
        screenshot_dir / "transitions",
        "transition",
    )
    expected_stage_indices = list(range(1, len(stages) + 1))
    if sorted(stage_image_paths) != expected_stage_indices:
        raise ReplayGateError(
            "stage screenshots do not match complete stage snapshots"
        )
    expected_transition_indices = list(range(1, len(transitions) + 1))
    if sorted(transition_image_paths) != expected_transition_indices:
        raise ReplayGateError(
            "transition screenshots do not match complete transition "
            "snapshots"
        )
    if len(stages) != len(transitions):
        raise ReplayGateError(
            "settled stage and immediate transition counts differ"
        )
    if terminal.status.rendering_enabled != 1:
        raise ReplayGateError(
            "rendered evidence was requested from a non-rendered replay"
        )
    if (
        terminal.status.presented_generation
        != terminal.status.render_generation
    ):
        raise ReplayGateError(
            "terminal screenshot is not bound to its rendered generation"
        )
    if (
        terminal.status.hardware_playable != 1
        or terminal.status.opposite_direction_transitions != 0
    ):
        raise ReplayGateError(
            "rendered evidence requires a hardware-playable replay "
            "without opposite directions"
        )

    records: list[dict[str, object]] = []
    previous_pixel_hash: str | None = None
    for expected_stage, (snapshot, transition) in enumerate(
        zip(stages, transitions, strict=True)
    ):
        status = snapshot.status
        transition_status = transition.status
        _validate_rendered_stage_pair(
            snapshot,
            transition,
            expected_stage,
        )

        image = _screenshot_content_record(
            stage_image_paths[snapshot.sample],
            maximum_bytes,
            relative_to=screenshot_dir.parent,
        )
        transition_image = _screenshot_content_record(
            transition_image_paths[transition.sample],
            maximum_bytes,
            require_visible_content=False,
            relative_to=screenshot_dir.parent,
        )
        pixel_hash = str(image["viewport"]["pixel_sha256"])
        if (
            image["viewport"]["playfield"]["pixel_sha256"]
            == transition_image["viewport"]["playfield"]["pixel_sha256"]
        ):
            raise ReplayGateError(
                f"stage {expected_stage} settled playfield is "
                "pixel-identical to its immediate transition"
            )
        if pixel_hash == previous_pixel_hash:
            raise ReplayGateError(
                f"stage {expected_stage} screenshot is pixel-identical to "
                "the preceding stage"
            )
        previous_pixel_hash = pixel_hash
        records.append(
            {
                "sample": snapshot.sample,
                "stage": expected_stage,
                "world": status.world + 1,
                "level": status.level + 1,
                "source_frame": status.frame,
                "core_frames_advanced": status.core_frames_advanced,
                "game_frame_count": status.game_frame_count,
                "vblank_count": status.vblank_count,
                "render_generation": status.render_generation,
                "presented_generation": status.presented_generation,
                "entered_mask": f"0x{status.entered_mask:08x}",
                "completed_mask": f"0x{status.completed_mask:08x}",
                "transition": {
                    "source_frame": transition_status.frame,
                    "game_frame_count":
                        transition_status.game_frame_count,
                    "vblank_count": transition_status.vblank_count,
                    "render_generation":
                        transition_status.render_generation,
                    "presented_generation":
                        transition_status.presented_generation,
                    "image": transition_image,
                },
                "image": image,
            }
        )

    if require_complete and len(records) != EXPECTED_STAGE_CAPTURES:
        raise ReplayGateError(
            f"rendered pass has {len(records)} stage captures; expected "
            f"{EXPECTED_STAGE_CAPTURES}"
        )

    terminal_image = _screenshot_content_record(
        screenshot_dir / "terminal.png",
        maximum_bytes,
        relative_to=screenshot_dir.parent,
    )
    if (
        records
        and terminal_image["viewport"]["playfield"]["pixel_sha256"]
        == records[-1]["image"]["viewport"]["playfield"]["pixel_sha256"]
    ):
        raise ReplayGateError(
            "terminal playfield is pixel-identical to the final settled stage"
        )
    return {
        "stage_count": len(records),
        "transition_count": len(transitions),
        "complete_stage_set": len(records) == EXPECTED_STAGE_CAPTURES,
        "stages": records,
        "terminal": {
            "source_frame": terminal.status.frame,
            "core_frames_advanced": terminal.status.core_frames_advanced,
            "game_frame_count": terminal.status.game_frame_count,
            "vblank_count": terminal.status.vblank_count,
            "render_generation": terminal.status.render_generation,
            "presented_generation":
                terminal.status.presented_generation,
            "image": terminal_image,
        },
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


def _require_pillow() -> None:
    try:
        from PIL import Image
    except ImportError as error:
        raise ReplayGateError(
            "rendered evidence requires the Pillow Python package"
        ) from error
    if not hasattr(Image, "open"):
        raise ReplayGateError(
            "rendered evidence requires a usable Pillow installation"
        )


def _snapshot_file(source: Path, destination: Path) -> Path:
    if destination.exists():
        raise ReplayGateError(
            f"frozen input destination already exists: {destination}"
        )
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        with source.open("rb") as reader:
            with destination.open("xb") as writer:
                shutil.copyfileobj(reader, writer, length=1024 * 1024)
        destination.chmod(0o444)
    except OSError as error:
        raise ReplayGateError(
            f"cannot freeze replay input {source}: {error}"
        ) from error
    return destination.resolve()


def _verify_frozen_artifacts(
    artifacts: dict[str, object],
    evidence_dir: Path,
) -> None:
    for name, value in artifacts.items():
        if not isinstance(value, dict):
            raise ReplayGateError(
                f"artifact record {name} is malformed"
            )
        relative_path = value.get("path")
        expected_bytes = value.get("bytes")
        expected_sha256 = value.get("sha256")
        if (
            not isinstance(relative_path, str)
            or not isinstance(expected_bytes, int)
            or not isinstance(expected_sha256, str)
        ):
            raise ReplayGateError(
                f"artifact record {name} is incomplete"
            )
        candidate = (evidence_dir / relative_path).resolve()
        try:
            candidate.relative_to(evidence_dir)
        except ValueError as error:
            raise ReplayGateError(
                f"artifact record {name} escapes the evidence directory"
            ) from error
        if (
            not candidate.is_file()
            or candidate.stat().st_size != expected_bytes
            or cadence._sha256_file(candidate) != expected_sha256
        ):
            raise ReplayGateError(
                f"frozen artifact changed during replay: {name}"
            )


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
        "rendered_evidence": args.rendered_evidence,
        "screenshot_timeout_seconds": args.screenshot_timeout,
        "display_settle_seconds": args.display_settle_seconds,
        "maximum_screenshot_bytes": MAX_SCREENSHOT_BYTES,
        "gngeo": args.gngeo,
        "gdb": args.gdb,
        "nm": args.nm,
        "xvfb": args.xvfb,
        "scrot": args.scrot,
    }


def _apply_default_paths(args: argparse.Namespace) -> None:
    repository = Path(__file__).resolve().parents[1]
    variant = "replay-rendered" if args.rendered_evidence else "replay-fast"
    replay_build = repository / "platform" / "neogeo" / "build" / variant
    if args.elf is None:
        args.elf = replay_build / "smbneogeo-replay.elf"
    if args.rom_dir is None:
        args.rom_dir = replay_build / "rom"
    if args.data_file is None:
        args.data_file = replay_build / "rom" / "gngeo_data.zip"


def _validate_runtime_arguments(args: argparse.Namespace) -> float:
    if args.m68k_overclock is None:
        args.m68k_overclock = (
            DEFAULT_RENDERED_68K_OVERCLOCK
            if args.rendered_evidence
            else DEFAULT_68K_OVERCLOCK
        )
    if not ROM_SET_RE.fullmatch(args.rom_set):
        raise ReplayGateError(
            "ROM set must contain 1-64 letters, digits, dots, underscores, "
            "or hyphens and must start with a letter or digit"
        )
    for label, value in (
        ("heartbeat interval", args.heartbeat_seconds),
        ("startup timeout", args.startup_timeout),
        ("screenshot timeout", args.screenshot_timeout),
    ):
        if not math.isfinite(value) or value <= 0:
            raise ReplayGateError(f"{label} must be finite and positive")
    if args.screenshot_timeout > MAX_SCREENSHOT_TIMEOUT_SECONDS:
        raise ReplayGateError(
            f"screenshot timeout must be no more than "
            f"{MAX_SCREENSHOT_TIMEOUT_SECONDS:g} seconds"
        )
    if (
        not math.isfinite(args.display_settle_seconds)
        or args.display_settle_seconds < 0
        or args.display_settle_seconds > MAX_DISPLAY_SETTLE_SECONDS
    ):
        raise ReplayGateError(
            "display-settle delay must be finite and between 0 and "
            f"{MAX_DISPLAY_SETTLE_SECONDS:g} seconds inclusive"
        )
    requested_timeout = args.timeout
    if requested_timeout is None and args.rendered_evidence:
        requested_timeout = DEFAULT_RENDERED_TIMEOUT_SECONDS
    return derive_gate_timeout(requested_timeout, args.m68k_overclock)


def run_probe(args: argparse.Namespace) -> int:
    _apply_default_paths(args)
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
    screenshot_dir = evidence_dir / "screenshots"

    try:
        source_elf = _require_file(args.elf, "replay-gate ELF")
        source_rom_dir = _require_directory(
            args.rom_dir,
            "ROM directory",
        )
        source_data_file = _require_file(
            args.data_file,
            "GnGeo data file",
        )
        source_cartridge = _require_file(
            source_rom_dir / f"{args.rom_set}.zip",
            "replay-gate cartridge",
        )
        source_bios = {
            name: _require_file(
                source_rom_dir / name,
                f"replay-gate {name} BIOS archive",
            )
            for name in REQUIRED_BIOS_FILES
        }

        nm_executable = _resolve_executable(args.nm)
        gdb_executable = _resolve_executable(args.gdb)
        gngeo_executable = _resolve_executable(args.gngeo)
        xvfb_executable = _resolve_executable(args.xvfb)
        scrot_executable: str | None = None
        source_screenshot_helper: Path | None = None
        if args.rendered_evidence:
            _require_pillow()
            scrot_executable = _resolve_executable(args.scrot)
            source_screenshot_helper = _require_file(
                Path(frame_capture.__file__),
                "replay screenshot helper",
            )

        frozen_input_dir = evidence_dir / "inputs"
        frozen_rom_dir = frozen_input_dir / "rom"
        frozen_elf = _snapshot_file(
            source_elf,
            frozen_input_dir / "smbneogeo-replay.elf",
        )
        frozen_cartridge = _snapshot_file(
            source_cartridge,
            frozen_rom_dir / f"{args.rom_set}.zip",
        )
        frozen_data_file = _snapshot_file(
            source_data_file,
            frozen_rom_dir / "gngeo_data.zip",
        )
        frozen_bios = {
            name: _snapshot_file(source, frozen_rom_dir / name)
            for name, source in source_bios.items()
        }
        frozen_runner = _snapshot_file(
            _require_file(Path(__file__), "replay validator"),
            frozen_input_dir / "run_neogeo_replay_gate.py",
        )
        frozen_cadence_helper = _snapshot_file(
            _require_file(
                Path(cadence.__file__),
                "replay process helper",
            ),
            frozen_input_dir / "measure_neogeo_cadence.py",
        )

        screenshot: ScreenshotConfig | None = None
        if args.rendered_evidence:
            screenshot_dir.mkdir()
            (screenshot_dir / "stages").mkdir()
            (screenshot_dir / "transitions").mkdir()
            assert source_screenshot_helper is not None
            assert scrot_executable is not None
            frozen_screenshot_helper = _snapshot_file(
                source_screenshot_helper,
                frozen_input_dir / "capture_neogeo_replay_frame.py",
            )
            screenshot = ScreenshotConfig(
                python=_resolve_executable(sys.executable),
                helper=frozen_screenshot_helper,
                scrot=scrot_executable,
                directory=screenshot_dir,
                timeout_seconds=args.screenshot_timeout,
                display_settle_seconds=args.display_settle_seconds,
                maximum_bytes=MAX_SCREENSHOT_BYTES,
            )
        symbols = resolve_replay_symbols(frozen_elf, nm_executable)

        result["artifacts"] = {
            "elf": _artifact_record(
                frozen_elf,
                relative_to=evidence_dir,
            ),
            "cartridge": _artifact_record(
                frozen_cartridge,
                relative_to=evidence_dir,
            ),
            "gngeo_data": _artifact_record(
                frozen_data_file,
                relative_to=evidence_dir,
            ),
            "validator": _artifact_record(
                frozen_runner,
                relative_to=evidence_dir,
            ),
            "process_helper": _artifact_record(
                frozen_cadence_helper,
                relative_to=evidence_dir,
            ),
        }
        for name, path in frozen_bios.items():
            result["artifacts"][f"bios_{name.removesuffix('.zip')}"] = (
                _artifact_record(path, relative_to=evidence_dir)
            )
        if screenshot is not None:
            result["artifacts"]["screenshot_helper"] = _artifact_record(
                screenshot.helper,
                relative_to=evidence_dir,
            )
        result["symbols"] = {
            "pass_trap": asdict(symbols.pass_trap),
            "fail_trap": asdict(symbols.fail_trap),
            "status": asdict(symbols.status),
            "progress_point": asdict(symbols.progress_point),
            "stage_point": asdict(symbols.stage_point),
            "transition_point": asdict(symbols.transition_point),
        }

        script_path = evidence_dir / "replay-gate.gdb"
        script_path.write_text(build_gdb_script(symbols, screenshot=screenshot))
        script_path.chmod(0o444)
        result["artifacts"]["gdb_script"] = _artifact_record(
            script_path,
            relative_to=evidence_dir,
        )
        result["outcome"] = "preflight"
        _write_result(result_path, result)

        cadence.reject_occupied_debug_port(DEBUG_HOST, DEBUG_PORT)
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
                frozen_rom_dir,
                frozen_data_file,
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
                frozen_elf,
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
                    env=environment,
                    start_new_session=True,
                ),
            )
            gdb_status = _wait_for_gate(
                gdb,
                gngeo,
                args.heartbeat_seconds,
                effective_timeout,
                gdb_log_path,
                require_rendered=args.rendered_evidence,
                screenshot_dir=(
                    screenshot_dir if args.rendered_evidence else None
                ),
            )
            gdb_log.flush()
            if gdb_status != 0:
                raise ReplayGateError(
                    f"GDB failed with status {gdb_status}:\n"
                    f"{_tail_bounded(gdb_log_path)}"
                )
            _verify_frozen_artifacts(
                result["artifacts"],
                evidence_dir,
            )

            capture = parse_gdb_output(
                gdb_log_path.read_text(errors="replace"),
                symbols,
            )
            progress = parse_progress_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
            stages = parse_stage_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
            transitions = parse_transition_snapshots(
                gdb_log_path.read_text(errors="replace")
            )
            classification = classify_result(capture)
            rendered_evidence: dict[str, object] | None = None
            if args.rendered_evidence:
                rendered_evidence = _rendered_evidence_result(
                    stages,
                    transitions,
                    capture,
                    screenshot_dir,
                    MAX_SCREENSHOT_BYTES,
                    classification.passed,
                )
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
            if rendered_evidence is not None:
                result["rendered_evidence"] = rendered_evidence
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--elf",
        type=Path,
        help="replay-gate ELF; defaults to the selected replay variant",
    )
    parser.add_argument(
        "--rom-dir",
        type=Path,
        help="ROM directory; defaults to the selected replay variant",
    )
    parser.add_argument(
        "--data-file",
        type=Path,
        help="GnGeo data file; defaults to the selected replay variant",
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
        default=None,
        metavar="PERCENT",
        help=(
            "MC68000 overclock percentage; defaults to 0 for rendered "
            f"evidence and {DEFAULT_68K_OVERCLOCK} otherwise"
        ),
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
    parser.add_argument(
        "--rendered-evidence",
        action="store_true",
        help=(
            "select the rendered replay and capture 32 immediate/settled "
            "stage pairs plus the terminal frame"
        ),
    )
    parser.add_argument(
        "--screenshot-timeout",
        type=float,
        default=DEFAULT_SCREENSHOT_TIMEOUT_SECONDS,
    )
    parser.add_argument(
        "--display-settle-seconds",
        type=float,
        default=DEFAULT_DISPLAY_SETTLE_SECONDS,
        help=(
            "bounded host-display settling delay applied after each debugger "
            "break and immediately before screenshot capture"
        ),
    )
    parser.add_argument("--gngeo", default="ngdevkit-gngeo")
    parser.add_argument("--gdb", default="m68k-neogeo-elf-gdb")
    parser.add_argument("--nm", default="m68k-neogeo-elf-nm")
    parser.add_argument("--xvfb", default="Xvfb")
    parser.add_argument("--scrot", default="scrot")
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
