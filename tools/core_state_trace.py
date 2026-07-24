#!/usr/bin/env python3
"""Emit and compare deterministic translated-core/FCEUX state transcripts."""

from __future__ import annotations

import argparse
import csv
from dataclasses import asdict, dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Sequence

import rec_tool


TRACE_SCHEMA = "smb-core-state-trace-v1"
TRACE_COLUMNS = (
    "frame",
    "input",
    "semantic_hash",
    "full_ram_hash",
    "zero_page_hash",
    "stack_hash",
    "oam_hash",
    "work_hash",
    "oper_mode",
    "oper_mode_task",
    "world",
    "level",
    "engine_subroutine",
    "player_state",
    "player_page",
    "player_x",
    "player_y",
    "screen_page",
    "screen_x",
    "world_end_timer",
    "lagged",
    "lag_count",
)
HASH_COLUMNS = frozenset(
    {
        "semantic_hash",
        "full_ram_hash",
        "zero_page_hash",
        "stack_hash",
        "oam_hash",
        "work_hash",
    }
)
DIAGNOSTIC_ONLY_COLUMNS = frozenset(
    {
        "semantic_hash",
        "full_ram_hash",
        "zero_page_hash",
        "stack_hash",
        "work_hash",
        "lagged",
        "lag_count",
    }
)
COMPARABLE_COLUMNS = tuple(
    column
    for column in TRACE_COLUMNS
    if column not in {"frame", *DIAGNOSTIC_ONLY_COLUMNS}
)
MAX_TRACE_FRAMES = 10_000_000
MAX_TIMEOUT_SECONDS = 3600.0
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class TraceError(RuntimeError):
    """Raised when a transcript cannot be generated or trusted."""


@dataclass(frozen=True)
class TraceRow:
    frame: int
    values: dict[str, int]

    @property
    def input(self) -> int:
        return self.values["input"]


@dataclass(frozen=True)
class Transcript:
    source: Path
    metadata: dict[str, str]
    rows: tuple[TraceRow, ...]
    sha256: str


@dataclass(frozen=True)
class TraceComparison:
    matches: bool
    outcome: str
    compared_frames: int
    first_divergent_frame: int | None
    first_divergent_translated_frame: int | None
    previous_matching_frame: int | None
    reference_frame_offset: int
    input_state: int | None
    differing_fields: dict[str, dict[str, int]]
    first_full_ram_difference: int | None
    first_semantic_hash_difference: int | None
    first_work_hash_difference: int | None
    skipped_hold_frames: tuple[int, ...]
    translated_frames: int
    reference_frames: int


def _sha256_stream(source) -> str:
    digest = hashlib.sha256()
    for chunk in iter(lambda: source.read(1024 * 1024), b""):
        digest.update(chunk)
    return digest.hexdigest()


def _sha256_file(path: Path) -> str:
    with path.open("rb") as source:
        return _sha256_stream(source)


def _open_verified_executable(
    path: Path,
    expected_sha256: str,
) -> int:
    if SHA256_RE.fullmatch(expected_sha256) is None:
        raise TraceError("expected executable SHA-256 is malformed")
    try:
        descriptor = os.open(path, os.O_RDONLY)
    except OSError as error:
        raise TraceError(f"cannot open executable {path}: {error}") from error
    try:
        mode = os.fstat(descriptor).st_mode
        if not stat.S_ISREG(mode) or mode & 0o111 == 0:
            raise TraceError(f"executable is not a runnable file: {path}")
        with os.fdopen(os.dup(descriptor), "rb") as source:
            actual_sha256 = _sha256_stream(source)
        os.lseek(descriptor, 0, os.SEEK_SET)
        if actual_sha256 != expected_sha256:
            raise TraceError(
                f"executable SHA-256 changed: expected "
                f"{expected_sha256}, got {actual_sha256}"
            )
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _open_exclusive_output(path: Path, label: str):
    try:
        descriptor = os.open(
            path,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL,
            0o666,
        )
    except FileExistsError as error:
        raise TraceError(f"{label} already exists: {path}") from error
    except OSError as error:
        raise TraceError(f"cannot create {label} {path}: {error}") from error
    return os.fdopen(descriptor, "wb")


def _copy_file_exclusive(
    source: Path,
    destination: Path,
    label: str,
) -> None:
    # Keep a partial destination on an exceptional write. The transcript
    # parser rejects it because it lacks the final completion marker, and
    # retaining our exclusively created inode avoids ever unlinking a path
    # that another process may have replaced during cleanup.
    with _open_exclusive_output(destination, label) as output:
        with source.open("rb") as input_file:
            shutil.copyfileobj(input_file, output, 1024 * 1024)


def _write_text_exclusive(path: Path, text: str, label: str) -> None:
    with _open_exclusive_output(path, label) as output:
        output.write(text.encode("utf-8"))


def _parse_uint(value: str, column: str, line_number: int) -> int:
    base = 16 if column in HASH_COLUMNS else 10
    try:
        parsed = int(value, base)
    except ValueError as error:
        raise TraceError(
            f"line {line_number}: invalid {column} value {value!r}"
        ) from error
    limit = 0xFFFFFFFF if column in HASH_COLUMNS else 0xFFFFFFFF
    if not 0 <= parsed <= limit:
        raise TraceError(
            f"line {line_number}: {column} is outside the uint32 range"
        )
    if column == "input" and parsed > 0xFF:
        raise TraceError(
            f"line {line_number}: input is outside the uint8 range"
        )
    return parsed


def load_transcript(path: Path) -> Transcript:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise TraceError(f"transcript does not exist: {resolved}")

    metadata: dict[str, str] = {}
    rows: list[TraceRow] = []
    columns: tuple[str, ...] | None = None
    complete = False
    try:
        lines = resolved.read_text(encoding="ascii").splitlines()
    except (OSError, UnicodeError) as error:
        raise TraceError(f"cannot read transcript {resolved}: {error}") from error

    for line_number, line in enumerate(lines, start=1):
        if not line:
            continue
        if line.startswith("# "):
            item = line[2:]
            if "=" not in item:
                raise TraceError(
                    f"line {line_number}: malformed metadata line"
                )
            key, value = item.split("=", 1)
            if key in metadata:
                raise TraceError(
                    f"line {line_number}: duplicate metadata key {key!r}"
                )
            metadata[key] = value
            if key == "complete":
                complete = value == "1"
            continue

        parsed = next(csv.reader([line]))
        if columns is None:
            columns = tuple(parsed)
            if columns != TRACE_COLUMNS:
                raise TraceError(
                    f"line {line_number}: unexpected transcript columns"
                )
            continue
        if len(parsed) != len(columns):
            raise TraceError(
                f"line {line_number}: expected {len(columns)} columns, "
                f"found {len(parsed)}"
            )

        values = {
            column: _parse_uint(value, column, line_number)
            for column, value in zip(columns, parsed, strict=True)
        }
        expected_frame = len(rows)
        if values["frame"] != expected_frame:
            raise TraceError(
                f"line {line_number}: frame is {values['frame']}; "
                f"expected contiguous frame {expected_frame}"
            )
        rows.append(
            TraceRow(
                frame=values.pop("frame"),
                values=values,
            )
        )
        if len(rows) > MAX_TRACE_FRAMES:
            raise TraceError(
                f"transcript exceeds {MAX_TRACE_FRAMES} frame safety limit"
            )

    if columns is None:
        raise TraceError("transcript has no CSV header")
    if metadata.get("schema") != TRACE_SCHEMA:
        raise TraceError(
            f"transcript schema is {metadata.get('schema')!r}; "
            f"expected {TRACE_SCHEMA!r}"
        )
    if metadata.get("frame_semantics") != "post_input_nmi":
        raise TraceError(
            "transcript frame_semantics must be post_input_nmi"
        )
    if not complete:
        raise TraceError("transcript is incomplete")

    declared_frames = metadata.get("frames")
    if declared_frames is not None:
        try:
            parsed_frames = int(declared_frames, 10)
        except ValueError as error:
            raise TraceError("invalid frames metadata") from error
        if parsed_frames != len(rows):
            raise TraceError(
                f"transcript declares {parsed_frames} frames but contains "
                f"{len(rows)}"
            )

    return Transcript(
        source=resolved,
        metadata=metadata,
        rows=tuple(rows),
        sha256=_sha256_file(resolved),
    )


def compare_transcripts(
    translated: Transcript,
    reference: Transcript,
    *,
    reference_frame_offset: int = 0,
    skip_scheduled_holds: bool = False,
) -> TraceComparison:
    if not 0 <= reference_frame_offset <= len(reference.rows):
        raise TraceError(
            "reference frame offset is outside the reference transcript"
        )
    available_reference_frames = (
        len(reference.rows) - reference_frame_offset
    )
    common_frames = min(len(translated.rows), available_reference_frames)
    first_full_ram_difference: int | None = None
    first_semantic_hash_difference: int | None = None
    first_work_hash_difference: int | None = None
    skipped_hold_frames: list[int] = []
    compared_frames = 0
    previous_matching_frame: int | None = None

    for translated_frame in range(common_frames):
        reference_frame = translated_frame + reference_frame_offset
        translated_row = translated.rows[translated_frame]
        reference_row = reference.rows[reference_frame]
        if (
            first_full_ram_difference is None
            and translated_row.values["full_ram_hash"]
            != reference_row.values["full_ram_hash"]
        ):
            first_full_ram_difference = reference_frame
        if (
            first_semantic_hash_difference is None
            and translated_row.values["semantic_hash"]
            != reference_row.values["semantic_hash"]
        ):
            first_semantic_hash_difference = reference_frame
        if (
            first_work_hash_difference is None
            and translated_row.values["work_hash"]
            != reference_row.values["work_hash"]
        ):
            first_work_hash_difference = reference_frame

        if (
            skip_scheduled_holds
            and translated_row.values["lagged"] == 1
        ):
            if translated_row.input != reference_row.input:
                return TraceComparison(
                    matches=False,
                    outcome="input_mismatch",
                    compared_frames=compared_frames,
                    first_divergent_frame=reference_frame,
                    first_divergent_translated_frame=translated_frame,
                    previous_matching_frame=previous_matching_frame,
                    reference_frame_offset=reference_frame_offset,
                    input_state=translated_row.input,
                    differing_fields={
                        "input": {
                            "translated": translated_row.input,
                            "reference": reference_row.input,
                        }
                    },
                    first_full_ram_difference=(
                        first_full_ram_difference
                    ),
                    first_semantic_hash_difference=(
                        first_semantic_hash_difference
                    ),
                    first_work_hash_difference=(
                        first_work_hash_difference
                    ),
                    skipped_hold_frames=tuple(skipped_hold_frames),
                    translated_frames=len(translated.rows),
                    reference_frames=len(reference.rows),
                )
            skipped_hold_frames.append(reference_frame)
            continue

        compared_frames += 1
        differences = {
            column: {
                "translated": translated_row.values[column],
                "reference": reference_row.values[column],
            }
            for column in COMPARABLE_COLUMNS
            if translated_row.values[column] != reference_row.values[column]
        }
        if differences:
            outcome = (
                "input_mismatch"
                if "input" in differences
                else "state_divergence"
            )
            return TraceComparison(
                matches=False,
                outcome=outcome,
                compared_frames=compared_frames,
                first_divergent_frame=reference_frame,
                first_divergent_translated_frame=translated_frame,
                previous_matching_frame=previous_matching_frame,
                reference_frame_offset=reference_frame_offset,
                input_state=translated_row.input,
                differing_fields=differences,
                first_full_ram_difference=first_full_ram_difference,
                first_semantic_hash_difference=(
                    first_semantic_hash_difference
                ),
                first_work_hash_difference=first_work_hash_difference,
                skipped_hold_frames=tuple(skipped_hold_frames),
                translated_frames=len(translated.rows),
                reference_frames=len(reference.rows),
            )
        previous_matching_frame = reference_frame

    has_schedule_lookahead = (
        skip_scheduled_holds
        and available_reference_frames == len(translated.rows) + 1
    )
    if (
        len(translated.rows) != available_reference_frames
        and not has_schedule_lookahead
    ):
        translated_frame = common_frames
        reference_frame = translated_frame + reference_frame_offset
        input_state = (
            translated.rows[translated_frame].input
            if translated_frame < len(translated.rows)
            else reference.rows[reference_frame].input
        )
        return TraceComparison(
            matches=False,
            outcome="length_mismatch",
            compared_frames=compared_frames,
            first_divergent_frame=reference_frame,
            first_divergent_translated_frame=translated_frame,
            previous_matching_frame=previous_matching_frame,
            reference_frame_offset=reference_frame_offset,
            input_state=input_state,
            differing_fields={},
            first_full_ram_difference=first_full_ram_difference,
            first_semantic_hash_difference=first_semantic_hash_difference,
            first_work_hash_difference=first_work_hash_difference,
            skipped_hold_frames=tuple(skipped_hold_frames),
            translated_frames=len(translated.rows),
            reference_frames=len(reference.rows),
        )

    return TraceComparison(
        matches=True,
        outcome="match",
        compared_frames=compared_frames,
        first_divergent_frame=None,
        first_divergent_translated_frame=None,
        previous_matching_frame=previous_matching_frame,
        reference_frame_offset=reference_frame_offset,
        input_state=None,
        differing_fields={},
        first_full_ram_difference=first_full_ram_difference,
        first_semantic_hash_difference=first_semantic_hash_difference,
        first_work_hash_difference=first_work_hash_difference,
        skipped_hold_frames=tuple(skipped_hold_frames),
        translated_frames=len(translated.rows),
        reference_frames=len(reference.rows),
    )


def build_hold_schedule(
    reference: Transcript,
    *,
    reference_frame_offset: int,
    frame_count: int,
) -> bytes:
    if reference_frame_offset < 0 or frame_count < 0:
        raise TraceError("hold schedule bounds must be non-negative")
    end = reference_frame_offset + frame_count
    if end > len(reference.rows):
        raise TraceError(
            "reference transcript is too short for the hold schedule"
        )
    schedule = bytearray(frame_count)
    # FCEUX marks the row after the extra source-frame boundary as lagged:
    # translated state F(n-1) catches up at reference F(n). Therefore hold
    # source frame n-1, not the lagged row n itself. The bootstrap marker at
    # the first aligned frame is deliberately ignored.
    lookahead_end = min(end + 1, len(reference.rows))
    for row in reference.rows[
        reference_frame_offset + 1 : lookahead_end
    ]:
        lagged = row.values["lagged"]
        if lagged not in (0, 1):
            raise TraceError(
                f"reference frame {row.frame} has invalid lagged={lagged}"
            )
        if lagged:
            schedule[row.frame - 1 - reference_frame_offset] = 1
    return bytes(schedule)


def build_host_compile_command(
    cc: str,
    repository: Path,
    generated_include: Path,
    output: Path,
) -> list[str]:
    library = repository / "codegen" / "lib"
    return [
        cc,
        "-std=c11",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-I",
        str(library),
        "-I",
        str(generated_include),
        "-o",
        str(output),
        str(repository / "tools" / "core_state_trace.c"),
        str(library / "instructions.c"),
        str(library / "code.c"),
        str(library / "data.c"),
        str(library / "cpu.c"),
        str(library / "ppu.c"),
        str(repository / "platform" / "neogeo" / "apu_null.c"),
    ]


def emit_translated_trace(
    fm2: Path,
    output: Path,
    *,
    frames: int | None,
    input_frame_offset: int,
    cc: str,
    timeout_seconds: float,
    hold_schedule_reference: Path | None = None,
    hold_source_frames: Sequence[int] = (),
) -> Transcript:
    if not math.isfinite(timeout_seconds) or not (
        0 < timeout_seconds <= MAX_TIMEOUT_SECONDS
    ):
        raise TraceError(
            f"timeout must be finite and between 0 and "
            f"{MAX_TIMEOUT_SECONDS:g} seconds"
        )
    resolved_cc = shutil.which(cc) if os.sep not in cc else cc
    if resolved_cc is None or not os.access(resolved_cc, os.X_OK):
        raise TraceError(f"C compiler is unavailable: {cc}")

    movie = rec_tool.load_fm2(fm2.expanduser().resolve())
    if not 0 <= input_frame_offset <= movie.frame_count:
        raise TraceError(
            f"input frame offset must be between 0 and {movie.frame_count}"
        )
    available_frames = movie.frame_count - input_frame_offset
    frame_limit = available_frames if frames is None else frames
    if not 0 <= frame_limit <= available_frames:
        raise TraceError(
            f"frame count must be between 0 and {available_frames} after "
            f"the {input_frame_offset}-frame input offset"
        )

    resolved_output = output.expanduser().resolve()
    if resolved_output.exists():
        raise TraceError(f"output already exists: {resolved_output}")
    resolved_output.parent.mkdir(parents=True, exist_ok=True)
    repository = Path(__file__).resolve().parents[1]

    with tempfile.TemporaryDirectory(
        prefix="smb-core-state-trace."
    ) as temporary_name:
        temporary = Path(temporary_name)
        header = temporary / "smb_trace_replay_data.h"
        executable = temporary / "core-state-trace"
        temporary_output = temporary / "translated.csv"
        schedule_path = temporary / "hold-schedule.bin"
        header.write_text(
            rec_tool.render_fm2_c_header(
                movie,
                symbol_prefix="smb_trace",
                hardware_playable=False,
            ),
            encoding="ascii",
        )

        compile_command = build_host_compile_command(
            resolved_cc,
            repository,
            temporary,
            executable,
        )
        completed = subprocess.run(
            compile_command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout_seconds,
        )
        if completed.returncode != 0:
            raise TraceError(
                f"host trace compilation failed with status "
                f"{completed.returncode}:\n{completed.stdout[-8000:]}"
            )

        execution_environment = os.environ.copy()
        schedule = bytearray(frame_limit)
        if hold_schedule_reference is not None:
            reference = load_transcript(hold_schedule_reference)
            schedule[:] = build_hold_schedule(
                reference,
                reference_frame_offset=input_frame_offset,
                frame_count=frame_limit,
            )
        for source_frame in hold_source_frames:
            if not (
                input_frame_offset
                <= source_frame
                < input_frame_offset + frame_limit
            ):
                raise TraceError(
                    f"hold source frame {source_frame} is outside the "
                    "emitted source-frame window"
                )
            schedule[source_frame - input_frame_offset] = 1
        if any(schedule):
            schedule_path.write_bytes(schedule)
            execution_environment["SMB_TRACE_HOLD_SCHEDULE"] = str(
                schedule_path
            )

        with temporary_output.open("w", encoding="ascii") as destination:
            try:
                executed = subprocess.run(
                    [
                        str(executable),
                        str(frame_limit),
                        str(input_frame_offset),
                    ],
                    stdout=destination,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                    timeout=timeout_seconds,
                    env=execution_environment,
                )
            except subprocess.TimeoutExpired as error:
                raise TraceError(
                    f"translated trace exceeded its "
                    f"{timeout_seconds:g}-second timeout"
                ) from error
        if executed.returncode != 0:
            raise TraceError(
                f"translated trace failed with status "
                f"{executed.returncode}:\n{executed.stderr[-8000:]}"
            )

        transcript = load_transcript(temporary_output)
        _copy_file_exclusive(
            temporary_output,
            resolved_output,
            "output",
        )

    return Transcript(
        source=resolved_output,
        metadata=transcript.metadata,
        rows=transcript.rows,
        sha256=_sha256_file(resolved_output),
    )


def build_fceux_command(
    executable: str,
    rom: Path,
    fm2: Path,
    lua_script: Path,
) -> list[str]:
    return [
        executable,
        "--no-config",
        "1",
        "--sound",
        "0",
        "--playmov",
        str(fm2),
        "--loadlua",
        str(lua_script),
        str(rom),
    ]


def build_verified_fceux_command(
    python_executable: Path,
    script: Path,
    executable: Path,
    expected_sha256: str,
    rom: Path,
    fm2: Path,
    lua_script: Path,
) -> list[str]:
    return [
        str(python_executable),
        str(script),
        "exec-fceux-verified",
        "--fceux",
        str(executable),
        "--expected-sha256",
        expected_sha256,
        "--rom",
        str(rom),
        "--fm2",
        str(fm2),
        "--lua",
        str(lua_script),
    ]


def execute_verified_fceux(
    executable: Path,
    expected_sha256: str,
    rom: Path,
    fm2: Path,
    lua_script: Path,
) -> None:
    if os.execve not in os.supports_fd:
        raise TraceError(
            "this platform cannot execute a verified file descriptor"
        )
    descriptor = _open_verified_executable(
        executable,
        expected_sha256,
    )
    command = build_fceux_command(
        str(executable),
        rom,
        fm2,
        lua_script,
    )
    environment = os.environ.copy()
    environment["SMB_TRACE_EMULATOR_LABEL"] = executable.name
    environment["SMB_TRACE_EMULATOR_SHA256"] = expected_sha256
    try:
        # Executing the already-hashed descriptor closes the pathname race
        # between command generation, verification, and process replacement.
        os.execve(descriptor, command, environment)
    finally:
        os.close(descriptor)


def _comparison_payload(
    comparison: TraceComparison,
    translated: Transcript,
    reference: Transcript,
) -> dict[str, object]:
    return {
        "schema_version": 1,
        "comparison": asdict(comparison),
        "artifacts": {
            "translated": {
                "path": str(translated.source),
                "sha256": translated.sha256,
                "frames": len(translated.rows),
            },
            "reference": {
                "path": str(reference.source),
                "sha256": reference.sha256,
                "frames": len(reference.rows),
            },
        },
        "comparison_domain": {
            "frame_semantics": "post_input_nmi",
            "pass_domain": (
                "input, OAM page hash, and selected persistent gameplay "
                "state"
            ),
            "work_hash_note": (
                "$0300-$07ff includes transient VRAM buffers and is "
                "diagnostic-only"
            ),
            "diagnostic_only": sorted(DIAGNOSTIC_ONLY_COLUMNS),
        },
    }


def _positive_timeout(value: str) -> float:
    parsed = float(value)
    if not math.isfinite(parsed) or not 0 < parsed <= MAX_TIMEOUT_SECONDS:
        raise argparse.ArgumentTypeError(
            f"must be finite, positive, and at most {MAX_TIMEOUT_SECONDS:g}"
        )
    return parsed


def _nonnegative_frames(value: str) -> int:
    parsed = int(value, 10)
    if not 0 <= parsed <= MAX_TRACE_FRAMES:
        raise argparse.ArgumentTypeError(
            f"must be between 0 and {MAX_TRACE_FRAMES}"
        )
    return parsed


def _sha256_digest(value: str) -> str:
    normalized = value.lower()
    if SHA256_RE.fullmatch(normalized) is None:
        raise argparse.ArgumentTypeError(
            "must be exactly 64 hexadecimal digits"
        )
    return normalized


def build_argument_parser() -> argparse.ArgumentParser:
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    emit = commands.add_parser(
        "emit-translated",
        help="compile the translated core and emit its post-frame transcript",
    )
    emit.add_argument("--fm2", type=Path, required=True)
    emit.add_argument("--output", type=Path, required=True)
    emit.add_argument("--frames", type=_nonnegative_frames)
    emit.add_argument(
        "--input-frame-offset",
        type=_nonnegative_frames,
        default=0,
        help=(
            "start controller playback at this FM2 source frame while "
            "translated execution starts at row 0"
        ),
    )
    emit.add_argument(
        "--hold-schedule-reference",
        type=Path,
        help=(
            "derive no-NMI source rows immediately before post-bootstrap "
            "FCEUX lag markers"
        ),
    )
    emit.add_argument(
        "--hold-source-frame",
        type=_nonnegative_frames,
        action="append",
        default=[],
        help="explicit absolute FM2 source frame that must not advance NMI",
    )
    emit.add_argument("--cc", default="clang")
    emit.add_argument("--timeout", type=_positive_timeout, default=120.0)

    compare = commands.add_parser(
        "compare",
        help="report the first divergent FM2 input frame",
    )
    compare.add_argument("--translated", type=Path, required=True)
    compare.add_argument("--reference", type=Path, required=True)
    compare.add_argument(
        "--reference-frame-offset",
        type=_nonnegative_frames,
        default=0,
        help="compare translated row i with reference row i plus this offset",
    )
    compare.add_argument(
        "--skip-scheduled-holds",
        action="store_true",
        help=(
            "validate input but skip state comparison on translated rows "
            "explicitly marked as no-NMI holds"
        ),
    )
    compare.add_argument("--result-json", type=Path)

    fceux = commands.add_parser(
        "fceux-command",
        help="print the exact environment and command for the Lua reference",
    )
    fceux.add_argument("--fceux", default="fceux")
    fceux.add_argument("--rom", type=Path, required=True)
    fceux.add_argument("--fm2", type=Path, required=True)
    fceux.add_argument("--output", type=Path, required=True)
    fceux.add_argument("--frames", type=_nonnegative_frames, required=True)
    fceux.add_argument(
        "--lua",
        type=Path,
        default=repository / "tools" / "fceux_ram_trace.lua",
    )

    verified = commands.add_parser(
        "exec-fceux-verified",
        help=argparse.SUPPRESS,
    )
    verified.add_argument("--fceux", type=Path, required=True)
    verified.add_argument(
        "--expected-sha256",
        type=_sha256_digest,
        required=True,
    )
    verified.add_argument("--rom", type=Path, required=True)
    verified.add_argument("--fm2", type=Path, required=True)
    verified.add_argument("--lua", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = build_argument_parser().parse_args(argv)
    try:
        if arguments.command == "exec-fceux-verified":
            rom = arguments.rom.expanduser().resolve()
            fm2 = arguments.fm2.expanduser().resolve()
            lua_script = arguments.lua.expanduser().resolve()
            for label, path in (
                ("ROM", rom),
                ("FM2", fm2),
                ("Lua script", lua_script),
            ):
                if not path.is_file():
                    raise TraceError(f"{label} does not exist: {path}")
            execute_verified_fceux(
                arguments.fceux.expanduser().resolve(),
                arguments.expected_sha256,
                rom,
                fm2,
                lua_script,
            )
            raise AssertionError("verified executable unexpectedly returned")

        if arguments.command == "emit-translated":
            transcript = emit_translated_trace(
                arguments.fm2,
                arguments.output,
                frames=arguments.frames,
                input_frame_offset=arguments.input_frame_offset,
                cc=arguments.cc,
                timeout_seconds=arguments.timeout,
                hold_schedule_reference=arguments.hold_schedule_reference,
                hold_source_frames=arguments.hold_source_frame,
            )
            print(
                f"{transcript.source}: wrote {len(transcript.rows)} frames; "
                f"sha256={transcript.sha256}"
            )
            return 0

        if arguments.command == "compare":
            translated = load_transcript(arguments.translated)
            reference = load_transcript(arguments.reference)
            comparison = compare_transcripts(
                translated,
                reference,
                reference_frame_offset=arguments.reference_frame_offset,
                skip_scheduled_holds=arguments.skip_scheduled_holds,
            )
            payload = _comparison_payload(
                comparison,
                translated,
                reference,
            )
            if arguments.result_json is not None:
                result_path = arguments.result_json.expanduser().resolve()
                _write_text_exclusive(
                    result_path,
                    json.dumps(payload, indent=2, sort_keys=True) + "\n",
                    "result output",
                )
            if comparison.matches:
                print(
                f"STATE_TRACE_MATCH frames={comparison.compared_frames} "
                f"reference_offset={comparison.reference_frame_offset} "
                f"full_ram_first_difference="
                    f"{comparison.first_full_ram_difference}"
                )
                return 0
            print(
                "STATE_TRACE_DIVERGENCE "
                f"frame={comparison.first_divergent_frame} "
                f"translated_frame="
                f"{comparison.first_divergent_translated_frame} "
                f"previous={comparison.previous_matching_frame} "
                f"input=0x{comparison.input_state or 0:02x} "
                f"outcome={comparison.outcome} "
                f"fields={','.join(comparison.differing_fields)}"
            )
            return 1

        output = arguments.output.expanduser().resolve()
        if output.exists():
            raise TraceError(f"reference output already exists: {output}")
        rom = arguments.rom.expanduser().resolve()
        fm2 = arguments.fm2.expanduser().resolve()
        lua_script = arguments.lua.expanduser().resolve()
        for label, path in (
            ("ROM", rom),
            ("FM2", fm2),
            ("Lua script", lua_script),
        ):
            if not path.is_file():
                raise TraceError(f"{label} does not exist: {path}")
        fceux = (
            shutil.which(arguments.fceux)
            if os.sep not in arguments.fceux
            else arguments.fceux
        )
        if fceux is None or not os.access(fceux, os.X_OK):
            raise TraceError(
                f"FCEUX executable is unavailable: {arguments.fceux}"
            )
        fceux_path = Path(fceux).resolve()
        fceux_sha256 = _sha256_file(fceux_path)
        payload = {
            "environment": {
                "SMB_TRACE_OUTPUT": str(output),
                "SMB_TRACE_FRAMES": str(arguments.frames),
                "SMB_TRACE_EMULATOR_LABEL": fceux_path.name,
                "SMB_TRACE_EMULATOR_SHA256": fceux_sha256,
            },
            "command": build_verified_fceux_command(
                Path(sys.executable).resolve(),
                Path(__file__).resolve(),
                fceux_path,
                fceux_sha256,
                rom,
                fm2,
                lua_script,
            ),
            "frame_semantics": (
                "row 0 is sampled after the first emu.frameadvance; "
                "FM2 record 0 command and controller input have been applied"
            ),
        }
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0
    except (
        TraceError,
        rec_tool.RecordingError,
        OSError,
        subprocess.SubprocessError,
    ) as error:
        print(f"state trace failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
