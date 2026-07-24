#!/usr/bin/env python3
"""Validate, import, and emit compact deterministic replay data."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import sys
from typing import Sequence


UINT8_MAX = 0xFF
UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF
INT32_MIN = -(1 << 31)
INT32_MAX = (1 << 31) - 1

INPUT_RIGHT = 0x80
INPUT_LEFT = 0x40
INPUT_DOWN = 0x20
INPUT_UP = 0x10
INPUT_START = 0x08
INPUT_SELECT = 0x04
INPUT_B = 0x02
INPUT_A = 0x01

FM2_BUTTON_ORDER = "RLDUTSBA"
FM2_BUTTON_BITS = (
    INPUT_RIGHT,
    INPUT_LEFT,
    INPUT_DOWN,
    INPUT_UP,
    INPUT_START,
    INPUT_SELECT,
    INPUT_B,
    INPUT_A,
)
FM2_FDS_COMMAND_MASK = 0x0C
SUPPORTED_FM2_ROM_CHECKSUM = "base64:jjYwGG411HcjG/j9UOVM3Q=="

_RECORD_RE = re.compile(r"([0-9]+):([0-9]+)")
_DECIMAL_RE = re.compile(r"[0-9]+")
_SIGNED_DECIMAL_RE = re.compile(r"-?[0-9]+")
_SHA256_RE = re.compile(r"[0-9a-fA-F]{64}")
_C_IDENTIFIER_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_FM2_BINARY_HEADER_RE = re.compile(
    rb"(?im)^binary[ \t]+([1-9][0-9]*)[ \t]*\r?$"
)


class RecordingError(ValueError):
    """A recording syntax, validation, or C-emission error."""


@dataclass(frozen=True)
class Transition:
    frame: int
    state: int
    line_number: int


@dataclass(frozen=True)
class Recording:
    source: str
    transitions: tuple[Transition, ...]
    sha256: str

    @property
    def end_frame(self) -> int:
        return self.transitions[-1].frame


@dataclass(frozen=True)
class Segment:
    duration: int
    state: int


@dataclass(frozen=True)
class Fm2Movie:
    source: str
    source_sha256: str
    frame_states: tuple[int, ...]
    initial_command: int
    ram_init_option: int
    ram_init_seed: int

    @property
    def frame_count(self) -> int:
        return len(self.frame_states)


def parse_recording(data: bytes, source: str = "<memory>") -> Recording:
    """Parse an ASCII frame:state recording and retain its raw SHA-256."""

    try:
        text = data.decode("ascii")
    except UnicodeDecodeError as error:
        raise RecordingError(
            f"{source}: recording is not ASCII at byte {error.start}"
        ) from error

    transitions: list[Transition] = []
    saw_trailing_comment = False

    for line_number, line in enumerate(text.splitlines(), 1):
        if line == "":
            continue
        if line.startswith("--"):
            saw_trailing_comment = True
            continue
        if saw_trailing_comment:
            raise RecordingError(
                f"{source}:{line_number}: transition follows a trailing comment"
            )

        match = _RECORD_RE.fullmatch(line)
        if match is None:
            raise RecordingError(
                f"{source}:{line_number}: expected an exact decimal frame:state record"
            )

        frame = int(match.group(1), 10)
        state = int(match.group(2), 10)
        if frame > UINT32_MAX:
            raise RecordingError(
                f"{source}:{line_number}: frame {frame} exceeds uint32_t"
            )
        if state > UINT8_MAX:
            raise RecordingError(
                f"{source}:{line_number}: state {state} exceeds uint8_t"
            )

        transitions.append(Transition(frame, state, line_number))

    if not transitions:
        raise RecordingError(f"{source}: recording contains no transitions")

    return Recording(
        source=source,
        transitions=tuple(transitions),
        sha256=hashlib.sha256(data).hexdigest(),
    )


def load_recording(path: Path) -> Recording:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise RecordingError(f"could not read {path}: {error}") from error
    return parse_recording(data, str(path))


def _fm2_header_value(
    headers: dict[str, list[tuple[str, int]]],
    key: str,
    *,
    source: str,
    required: bool,
) -> tuple[str, int] | None:
    entries = headers.get(key.lower(), [])
    if not entries:
        if required:
            raise RecordingError(f"{source}: missing required FM2 header {key}")
        return None
    if len(entries) != 1:
        lines = ", ".join(str(line_number) for _, line_number in entries)
        raise RecordingError(
            f"{source}: duplicate FM2 header {key} on lines {lines}"
        )
    return entries[0]


def _fm2_header_int(
    headers: dict[str, list[tuple[str, int]]],
    key: str,
    *,
    source: str,
    required: bool,
    default: int | None = None,
) -> int | None:
    entry = _fm2_header_value(
        headers, key, source=source, required=required
    )
    if entry is None:
        return default

    value, line_number = entry
    if _DECIMAL_RE.fullmatch(value) is None:
        raise RecordingError(
            f"{source}:{line_number}: FM2 header {key} must be a "
            f"non-negative decimal integer"
        )
    try:
        return int(value, 10)
    except ValueError as error:
        raise RecordingError(
            f"{source}:{line_number}: FM2 header {key} decimal value is too long"
        ) from error


def _fm2_header_signed_int32(
    headers: dict[str, list[tuple[str, int]]],
    key: str,
    *,
    source: str,
    required: bool,
    default: int | None = None,
) -> int | None:
    entry = _fm2_header_value(
        headers, key, source=source, required=required
    )
    if entry is None:
        return default

    value, line_number = entry
    if _SIGNED_DECIMAL_RE.fullmatch(value) is None:
        raise RecordingError(
            f"{source}:{line_number}: FM2 header {key} must be a "
            f"signed 32-bit decimal integer"
        )
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise RecordingError(
            f"{source}:{line_number}: FM2 header {key} decimal value is too long"
        ) from error
    if parsed < INT32_MIN or parsed > INT32_MAX:
        raise RecordingError(
            f"{source}:{line_number}: FM2 header {key} must fit in a "
            f"signed 32-bit integer"
        )
    return parsed


def _validate_fm2_configuration(
    headers: dict[str, list[tuple[str, int]]], source: str
) -> tuple[int, int, int]:
    version = _fm2_header_int(
        headers, "version", source=source, required=True
    )
    if version != 3:
        raise RecordingError(
            f"{source}: unsupported FM2 version {version}; expected version 3"
        )

    pal_flag = _fm2_header_int(
        headers, "palFlag", source=source, required=True
    )
    if pal_flag != 0:
        raise RecordingError(
            f"{source}: PAL FM2 movies are not supported; palFlag must be 0 "
            f"for NTSC"
        )

    checksum_entry = _fm2_header_value(
        headers, "romChecksum", source=source, required=True
    )
    assert checksum_entry is not None
    checksum, checksum_line = checksum_entry
    if checksum != SUPPORTED_FM2_ROM_CHECKSUM:
        raise RecordingError(
            f"{source}:{checksum_line}: romChecksum must exactly match the "
            f"supported ROM payload MD5 (case-sensitive): "
            f"{SUPPORTED_FM2_ROM_CHECKSUM}; found {checksum!r}"
        )

    binary = _fm2_header_int(
        headers,
        "binary",
        source=source,
        required=False,
        default=0,
    )
    if binary != 0:
        raise RecordingError(
            f"{source}: binary FM2 input logs are not supported"
        )

    fourscore = _fm2_header_int(
        headers, "fourscore", source=source, required=True
    )
    if fourscore != 0:
        raise RecordingError(
            f"{source}: Four Score / multiple-controller FM2 movies are "
            f"not supported"
        )

    port0 = _fm2_header_int(
        headers, "port0", source=source, required=True
    )
    if port0 != 1:
        raise RecordingError(
            f"{source}: unsupported controller-1 type {port0}; "
            f"port0 must be a gamepad (1)"
        )

    port1 = _fm2_header_int(
        headers, "port1", source=source, required=True
    )
    if port1 not in (0, 1):
        raise RecordingError(
            f"{source}: unsupported controller-2 type {port1}; "
            f"port1 must be disabled (0) or a neutral gamepad (1)"
        )

    port2 = _fm2_header_int(
        headers, "port2", source=source, required=True
    )
    if port2 != 0:
        raise RecordingError(
            f"{source}: expansion-port controller type {port2} is not supported"
        )

    microphone = _fm2_header_int(
        headers,
        "microphone",
        source=source,
        required=False,
        default=0,
    )
    if microphone != 0:
        raise RecordingError(
            f"{source}: microphone input is not supported"
        )

    fds = _fm2_header_int(
        headers, "fds", source=source, required=False, default=0
    )
    if fds != 0:
        raise RecordingError(f"{source}: FDS movies are not supported")

    new_ppu = _fm2_header_int(
        headers, "NewPPU", source=source, required=False, default=0
    )
    if new_ppu != 0:
        raise RecordingError(
            f"{source}: NewPPU movies are not supported; NewPPU must be 0"
        )

    ram_init_option = _fm2_header_int(
        headers,
        "RAMInitOption",
        source=source,
        required=False,
        default=0,
    )
    assert ram_init_option is not None
    if ram_init_option not in (0, 2):
        raise RecordingError(
            f"{source}: unsupported FM2 RAM initialization option "
            f"{ram_init_option}; supported deterministic modes are 0 "
            f"(legacy 00/FF pattern) and 2 (fill zero)"
        )

    # FCEUX 2.2.1 predates RAMInitOption and always powered CPU RAM with the
    # deterministic 00 00 00 00 FF FF FF FF pattern represented by later
    # option 0. Newer files may explicitly request option 2, which matches
    # fresh zeroed core RAM. Neither mode consumes RAMInitSeed, but validate
    # and retain a supplied seed so provenance is exact and malformed
    # configuration is never accepted.
    ram_init_seed = _fm2_header_signed_int32(
        headers,
        "RAMInitSeed",
        source=source,
        required=False,
        default=0,
    )
    assert ram_init_seed is not None

    starts_from_savestate = _fm2_header_int(
        headers,
        "startsfromsavestate",
        source=source,
        required=False,
        default=0,
    )
    if starts_from_savestate != 0:
        raise RecordingError(
            f"{source}: movies starting from a savestate are not supported"
        )

    for embedded_state in ("savestate", "saveram"):
        if embedded_state in headers:
            _, line_number = headers[embedded_state][0]
            raise RecordingError(
                f"{source}:{line_number}: embedded FM2 {embedded_state} "
                f"state is not supported"
            )
    return port1, ram_init_option, ram_init_seed


def _decode_fm2_gamepad(
    field: str,
    *,
    source: str,
    line_number: int,
    controller_name: str,
) -> int:
    if len(field) != len(FM2_BUTTON_ORDER):
        raise RecordingError(
            f"{source}:{line_number}: {controller_name} field must contain "
            f"exactly 8 RLDUTSBA positions"
        )

    state = 0
    for position, (value, button, bit) in enumerate(
        zip(field, FM2_BUTTON_ORDER, FM2_BUTTON_BITS), 1
    ):
        if value == ".":
            continue
        if value != button:
            raise RecordingError(
                f"{source}:{line_number}: malformed {controller_name} "
                f"position {position}; expected '.' or '{button}', found "
                f"{value!r}"
            )
        state |= bit
    return state


def _parse_fm2_input_record(
    line: str,
    *,
    source: str,
    line_number: int,
    frame_index: int,
    port1_type: int,
) -> tuple[int, int]:
    fields = line.split("|")
    if len(fields) != 6 or fields[0] != "" or fields[-1] != "":
        raise RecordingError(
            f"{source}:{line_number}: malformed FM2 input record; expected "
            f"|commands|controller1|controller2|expansion|"
        )

    command_text, controller1, controller2, expansion = fields[1:5]
    if _DECIMAL_RE.fullmatch(command_text) is None:
        raise RecordingError(
            f"{source}:{line_number}: FM2 command must be a non-negative "
            f"decimal integer"
        )
    try:
        command = int(command_text, 10)
    except ValueError as error:
        raise RecordingError(
            f"{source}:{line_number}: FM2 command decimal value is too long"
        ) from error
    if command > UINT8_MAX:
        raise RecordingError(
            f"{source}:{line_number}: FM2 command {command} exceeds one byte"
        )
    if command & FM2_FDS_COMMAND_MASK:
        raise RecordingError(
            f"{source}:{line_number}: FDS command {command} is not supported"
        )
    if command == 1 and frame_index != 0:
        raise RecordingError(
            f"{source}:{line_number}: reset command 1 is supported only on "
            f"the first FM2 frame"
        )
    if command not in (0, 1):
        raise RecordingError(
            f"{source}:{line_number}: emulator command {command} cannot be "
            f"represented as controller input"
        )

    if port1_type == 0 and controller2 != "":
        raise RecordingError(
            f"{source}:{line_number}: controller-2 field must be empty when "
            f"port1 is disabled"
        )
    if port1_type == 1:
        controller2_state = _decode_fm2_gamepad(
            controller2,
            source=source,
            line_number=line_number,
            controller_name="controller-2",
        )
        if controller2_state != 0:
            raise RecordingError(
                f"{source}:{line_number}: multiple controlled ports are not "
                f"supported; controller-2 must remain neutral"
            )
    if expansion != "":
        raise RecordingError(
            f"{source}:{line_number}: expansion-port input is not supported"
        )

    state = _decode_fm2_gamepad(
        controller1,
        source=source,
        line_number=line_number,
        controller_name="controller-1",
    )
    return state, command


def parse_fm2(data: bytes, source: str = "<memory>") -> Fm2Movie:
    """Parse the deterministic, text, controller-1-only FM2 subset.

    FM2 header names are matched case-insensitively. Header values retain
    their original case; in particular, the Base64 ROM checksum is exact and
    case-sensitive because letter case changes the encoded MD5 bytes. Metadata
    lines may be UTF-8 or ISO-8859-1, as emitted by supported FCEUX versions,
    while every input-record byte is required to be ASCII and decoded without
    Unicode normalization.
    """

    if _FM2_BINARY_HEADER_RE.search(data):
        raise RecordingError(
            f"{source}: binary FM2 input logs are not supported"
        )
    if b"\x00" in data:
        raise RecordingError(
            f"{source}: FM2 must be a text file; binary data was found"
        )

    headers: dict[str, list[tuple[str, int]]] = {}
    input_records: list[tuple[str, int]] = []
    saw_input = False

    # Keep controller records on a byte-oriented path. This accepts legacy
    # Latin-1 author/comment metadata without ever treating a non-ASCII byte
    # in commands or controller fields as a Unicode character. A UTF-8 BOM is
    # meaningful only at the beginning of the metadata section.
    text_bytes = data[3:] if data.startswith(b"\xef\xbb\xbf") else data
    for line_number, raw_line in enumerate(text_bytes.splitlines(), 1):
        if raw_line.startswith(b"|"):
            saw_input = True
            try:
                line = raw_line.decode("ascii")
            except UnicodeDecodeError as error:
                invalid_byte = raw_line[error.start]
                raise RecordingError(
                    f"{source}:{line_number}: FM2 input record must contain "
                    f"only ASCII bytes; found 0x{invalid_byte:02x} at column "
                    f"{error.start + 1}"
                ) from error
            input_records.append((line, line_number))
            continue
        if saw_input:
            raise RecordingError(
                f"{source}:{line_number}: non-input data appears inside the "
                f"FM2 input log"
            )
        if raw_line == b"":
            continue

        try:
            line = raw_line.decode("utf-8")
        except UnicodeDecodeError:
            # Historical FM2 metadata is locale encoded. ISO-8859-1 is a
            # lossless single-byte fallback; required configuration headers
            # remain subject to their existing exact ASCII/numeric checks.
            line = raw_line.decode("iso-8859-1")

        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            raise RecordingError(
                f"{source}:{line_number}: malformed FM2 header"
            )
        key, value = parts
        headers.setdefault(key.lower(), []).append((value, line_number))

    port1_type, ram_init_option, ram_init_seed = (
        _validate_fm2_configuration(headers, source)
    )
    if not input_records:
        raise RecordingError(f"{source}: FM2 contains no frame input records")
    if len(input_records) > UINT32_MAX:
        raise RecordingError(
            f"{source}: FM2 frame count exceeds uint32_t"
        )

    parsed_records = tuple(
        _parse_fm2_input_record(
            line,
            source=source,
            line_number=line_number,
            frame_index=frame_index,
            port1_type=port1_type,
        )
        for frame_index, (line, line_number) in enumerate(input_records)
    )
    return Fm2Movie(
        source=source,
        source_sha256=hashlib.sha256(data).hexdigest(),
        frame_states=tuple(state for state, _ in parsed_records),
        initial_command=parsed_records[0][1],
        ram_init_option=ram_init_option,
        ram_init_seed=ram_init_seed,
    )


def load_fm2(path: Path) -> Fm2Movie:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise RecordingError(f"could not read {path}: {error}") from error
    return parse_fm2(data, str(path))


def import_fm2_recording(
    movie: Fm2Movie, *, hardware_playable: bool = False
) -> tuple[str, Recording]:
    """Collapse equal FM2 states while retaining the exact frame count."""

    lines: list[str] = []
    previous_state = 0
    for frame, state in enumerate(movie.frame_states):
        if state != previous_state:
            lines.append(f"{frame}:{state}")
            previous_state = state

    # The explicit exclusive-end marker preserves trailing neutral/lag frames.
    lines.append(f"{movie.frame_count}:0")
    lines.append(f"-- fm2_source_sha256:{movie.source_sha256}")
    lines.append(f"-- fm2_frame_count:{movie.frame_count}")
    lines.append(f"-- fm2_ram_init_option:{movie.ram_init_option}")
    lines.append(f"-- fm2_ram_init_seed:{movie.ram_init_seed}")
    if movie.initial_command == 1:
        lines.append("-- fm2_initial_command:1")
        lines.append(
            "-- fm2_initial_command_semantics:fresh_core_reset_equivalent"
        )
    content = "\n".join(lines) + "\n"

    imported = parse_recording(
        content.encode("ascii"), f"{movie.source} (imported)"
    )
    validate_recording(
        imported,
        expected_end_frame=movie.frame_count,
        hardware_playable=hardware_playable,
    )
    return content, imported


def has_opposite_directions(state: int) -> bool:
    horizontal = state & (INPUT_LEFT | INPUT_RIGHT)
    vertical = state & (INPUT_UP | INPUT_DOWN)
    return (
        horizontal == (INPUT_LEFT | INPUT_RIGHT)
        or vertical == (INPUT_UP | INPUT_DOWN)
    )


def count_opposite_direction_transitions(recording: Recording) -> int:
    return sum(
        has_opposite_directions(transition.state)
        for transition in recording.transitions
    )


def normalize_sha256(value: str) -> str:
    if _SHA256_RE.fullmatch(value) is None:
        raise RecordingError("expected SHA-256 must contain exactly 64 hex digits")
    return value.lower()


def validate_recording(
    recording: Recording,
    *,
    expected_end_frame: int | None = None,
    expected_sha256: str | None = None,
    expected_transition_count: int | None = None,
    hardware_playable: bool = False,
) -> Recording:
    """Validate transition semantics and optional immutable source metadata."""

    previous: Transition | None = None
    final_index = len(recording.transitions) - 1
    for index, transition in enumerate(recording.transitions):
        if previous is not None:
            if transition.frame <= previous.frame:
                raise RecordingError(
                    f"{recording.source}:{transition.line_number}: frame "
                    f"{transition.frame} is not strictly greater than "
                    f"{previous.frame}"
                )
            is_redundant_end_sentinel = (
                index == final_index
                and transition.state == 0
                and previous.state == 0
            )
            if (
                transition.state == previous.state
                and not is_redundant_end_sentinel
            ):
                raise RecordingError(
                    f"{recording.source}:{transition.line_number}: adjacent "
                    f"transitions repeat state {transition.state}"
                )

        if hardware_playable and has_opposite_directions(transition.state):
            raise RecordingError(
                f"{recording.source}:{transition.line_number}: state "
                f"{transition.state} contains opposite directions and is not "
                f"hardware-playable"
            )
        previous = transition

    sentinel = recording.transitions[-1]
    if sentinel.state != 0:
        raise RecordingError(
            f"{recording.source}:{sentinel.line_number}: final transition must "
            f"be a zero-state release sentinel, found {sentinel.state}"
        )

    if (
        expected_end_frame is not None
        and recording.end_frame != expected_end_frame
    ):
        raise RecordingError(
            f"{recording.source}: end frame mismatch: expected "
            f"{expected_end_frame}, found {recording.end_frame}"
        )

    if expected_sha256 is not None:
        normalized_sha256 = normalize_sha256(expected_sha256)
        if recording.sha256 != normalized_sha256:
            raise RecordingError(
                f"{recording.source}: SHA-256 mismatch: expected "
                f"{normalized_sha256}, found {recording.sha256}"
            )

    if (
        expected_transition_count is not None
        and len(recording.transitions) != expected_transition_count
    ):
        raise RecordingError(
            f"{recording.source}: transition count mismatch: expected "
            f"{expected_transition_count}, found {len(recording.transitions)}"
        )

    return recording


def build_segments(
    recording: Recording, *, hardware_playable: bool = False
) -> tuple[Segment, ...]:
    """Convert frame transitions to compact duration/state runs."""

    validate_recording(recording, hardware_playable=hardware_playable)

    segments: list[Segment] = []
    current_frame = 0
    current_state = 0

    for transition in recording.transitions:
        duration = transition.frame - current_frame
        if duration:
            if segments and segments[-1].state == current_state:
                duration += segments.pop().duration
            if duration > UINT16_MAX:
                raise RecordingError(
                    f"{recording.source}:{transition.line_number}: state "
                    f"{current_state} run has duration {duration}, which exceeds "
                    f"uint16_t"
                )
            segments.append(Segment(duration, current_state))

        current_frame = transition.frame
        current_state = transition.state

    if not segments:
        raise RecordingError(
            f"{recording.source}: recording contains no playable frames"
        )
    if sum(segment.duration for segment in segments) != recording.end_frame:
        raise AssertionError("internal error: emitted durations do not cover recording")

    return tuple(segments)


def direction_policy_label(hardware_playable: bool) -> str:
    if hardware_playable:
        return "hardware-playable (opposites rejected)"
    return "recording-compatible (opposites permitted)"


def _format_c_array(
    values: Sequence[int], *, formatter: str, values_per_line: int = 12
) -> str:
    lines = []
    for offset in range(0, len(values), values_per_line):
        formatted = ", ".join(
            format(value, formatter)
            for value in values[offset : offset + values_per_line]
        )
        lines.append(f"    {formatted},")
    return "\n".join(lines)


def render_c_header(
    recording: Recording,
    *,
    symbol_prefix: str = "smb_replay",
    hardware_playable: bool = False,
    fm2_movie: Fm2Movie | None = None,
) -> str:
    """Render separate uint16_t duration and uint8_t state arrays."""

    if _C_IDENTIFIER_RE.fullmatch(symbol_prefix) is None:
        raise RecordingError(
            f"invalid C symbol prefix {symbol_prefix!r}: expected a C identifier"
        )

    segments = build_segments(
        recording, hardware_playable=hardware_playable
    )
    macro_prefix = symbol_prefix.upper()
    guard = f"{macro_prefix}_DATA_H"
    durations = [segment.duration for segment in segments]
    states = [segment.state for segment in segments]
    policy = direction_policy_label(hardware_playable)

    if fm2_movie is not None and recording.end_frame != fm2_movie.frame_count:
        raise RecordingError(
            "internal FM2 import mismatch: recording end frame "
            f"{recording.end_frame} differs from FM2 frame count "
            f"{fm2_movie.frame_count}"
        )

    source_metadata = (
        "/* SOURCE_SHA256 hashes the input frame:state recording bytes. */\n"
        f'#define {macro_prefix}_SOURCE_SHA256 "{recording.sha256}"\n'
    )
    if fm2_movie is not None:
        source_metadata = (
            "/* SOURCE_SHA256 hashes the canonical imported frame:state "
            "recording bytes. */\n"
            f'#define {macro_prefix}_SOURCE_SHA256 "{recording.sha256}"\n'
            "/* FM2_SOURCE_SHA256 hashes the original FM2 file bytes. */\n"
            f'#define {macro_prefix}_FM2_SOURCE_SHA256 '
            f'"{fm2_movie.source_sha256}"\n'
            f"#define {macro_prefix}_FM2_FRAME_COUNT "
            f"{fm2_movie.frame_count}u\n"
            f"#define {macro_prefix}_FM2_INITIAL_COMMAND "
            f"{fm2_movie.initial_command}u\n"
            f"#define {macro_prefix}_FM2_RAM_INIT_OPTION "
            f"{fm2_movie.ram_init_option}u\n"
            f"#define {macro_prefix}_FM2_RAM_INIT_SEED "
            f"{fm2_movie.ram_init_seed}\n"
        )

    return (
        "/* Generated by tools/rec_tool.py. Do not edit. */\n"
        f"/* Direction policy: {policy}. */\n"
        f"#ifndef {guard}\n"
        f"#define {guard}\n"
        "\n"
        "#include <stdint.h>\n"
        "\n"
        f"#define {macro_prefix}_TRANSITION_COUNT "
        f"{len(recording.transitions)}u\n"
        f"#define {macro_prefix}_SEGMENT_COUNT {len(segments)}u\n"
        f"#define {macro_prefix}_END_FRAME {recording.end_frame}u\n"
        f"#define {macro_prefix}_HARDWARE_PLAYABLE "
        f"{1 if hardware_playable else 0}u\n"
        f"#define {macro_prefix}_OPPOSITE_DIRECTION_TRANSITIONS "
        f"{count_opposite_direction_transitions(recording)}u\n"
        f"{source_metadata}"
        "\n"
        f"static const uint16_t {symbol_prefix}_durations"
        f"[{macro_prefix}_SEGMENT_COUNT] = {{\n"
        f"{_format_c_array(durations, formatter='d')}\n"
        "};\n"
        "\n"
        f"static const uint8_t {symbol_prefix}_states"
        f"[{macro_prefix}_SEGMENT_COUNT] = {{\n"
        f"{_format_c_array(states, formatter='#04x')}\n"
        "};\n"
        "\n"
        f"#endif /* {guard} */\n"
    )


def render_fm2_c_header(
    movie: Fm2Movie,
    *,
    symbol_prefix: str = "smb_replay",
    hardware_playable: bool = False,
) -> str:
    """Import an FM2 movie and render replay arrays with exact provenance."""

    _, imported = import_fm2_recording(
        movie, hardware_playable=hardware_playable
    )
    return render_c_header(
        imported,
        symbol_prefix=symbol_prefix,
        hardware_playable=hardware_playable,
        fm2_movie=movie,
    )


def _nonnegative_int(value: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected a decimal integer") from error
    if parsed < 0:
        raise argparse.ArgumentTypeError("expected a non-negative integer")
    return parsed


def _sha256_argument(value: str) -> str:
    try:
        return normalize_sha256(value)
    except RecordingError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def _add_validation_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("recording", type=Path)
    parser.add_argument(
        "--expect-end-frame",
        type=_nonnegative_int,
        help="reject a recording whose exclusive end frame differs",
    )
    parser.add_argument(
        "--expect-sha256",
        type=_sha256_argument,
        help="reject a recording whose raw file SHA-256 differs",
    )
    parser.add_argument(
        "--expect-transition-count",
        type=_nonnegative_int,
        help="reject a recording whose explicit transition count differs",
    )
    parser.add_argument(
        "--hardware-playable",
        action="store_true",
        help="reject simultaneous left+right or up+down input",
    )


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "validate/import frame input recordings and emit compact C data"
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate_parser = subparsers.add_parser(
        "validate", help="validate a recording"
    )
    _add_validation_arguments(validate_parser)

    emit_parser = subparsers.add_parser(
        "emit-c", help="emit uint16_t durations and uint8_t states"
    )
    _add_validation_arguments(emit_parser)
    emit_parser.add_argument(
        "-o",
        "--output",
        default="-",
        help="output header path, or - for stdout (default)",
    )
    emit_parser.add_argument(
        "--symbol-prefix",
        default="smb_replay",
        help="C identifier prefix (default: smb_replay)",
    )

    import_parser = subparsers.add_parser(
        "import-fm2",
        help="convert a local text FM2 controller-1 log to frame:state",
    )
    import_parser.add_argument(
        "movie",
        type=Path,
        help="local FCEUX FM2 file (never downloaded)",
    )
    import_parser.add_argument(
        "-o",
        "--output",
        default="-",
        help="output recording path, or - for stdout (default)",
    )
    import_parser.add_argument(
        "--hardware-playable",
        action="store_true",
        help="reject simultaneous left+right or up+down input",
    )

    emit_fm2_parser = subparsers.add_parser(
        "emit-fm2-c",
        help=(
            "emit compact C replay data directly from a local text FM2 log"
        ),
    )
    emit_fm2_parser.add_argument(
        "movie",
        type=Path,
        help="local FCEUX FM2 file (never downloaded)",
    )
    emit_fm2_parser.add_argument(
        "-o",
        "--output",
        default="-",
        help="output header path, or - for stdout (default)",
    )
    emit_fm2_parser.add_argument(
        "--symbol-prefix",
        default="smb_replay",
        help="C identifier prefix (default: smb_replay)",
    )
    emit_fm2_parser.add_argument(
        "--hardware-playable",
        action="store_true",
        help="reject simultaneous left+right or up+down input",
    )

    return parser


def _validated_from_arguments(arguments: argparse.Namespace) -> Recording:
    recording = load_recording(arguments.recording)
    return validate_recording(
        recording,
        expected_end_frame=arguments.expect_end_frame,
        expected_sha256=arguments.expect_sha256,
        expected_transition_count=arguments.expect_transition_count,
        hardware_playable=arguments.hardware_playable,
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_argument_parser()
    arguments = parser.parse_args(argv)

    try:
        if arguments.command in ("import-fm2", "emit-fm2-c"):
            movie = load_fm2(arguments.movie)
            if arguments.command == "emit-fm2-c":
                header = render_fm2_c_header(
                    movie,
                    symbol_prefix=arguments.symbol_prefix,
                    hardware_playable=arguments.hardware_playable,
                )
                policy = direction_policy_label(arguments.hardware_playable)
                if arguments.output == "-":
                    sys.stdout.write(header)
                else:
                    output_path = Path(arguments.output)
                    try:
                        output_path.write_text(header, encoding="ascii")
                    except OSError as error:
                        raise RecordingError(
                            f"could not write {output_path}: {error}"
                        ) from error
                    _, imported = import_fm2_recording(
                        movie,
                        hardware_playable=arguments.hardware_playable,
                    )
                    print(
                        f"{output_path}: wrote "
                        f"{len(build_segments(imported))} segments for "
                        f"{movie.frame_count} FM2 frames; "
                        f"recording_sha256={imported.sha256}; "
                        f"fm2_source_sha256={movie.source_sha256}; "
                        f"ram_init_option={movie.ram_init_option}; "
                        f"ram_init_seed={movie.ram_init_seed}; "
                        f"direction_policy={policy}"
                    )
                return 0

            content, imported = import_fm2_recording(
                movie,
                hardware_playable=arguments.hardware_playable,
            )
            policy = direction_policy_label(arguments.hardware_playable)
            if arguments.output == "-":
                sys.stdout.write(content)
            else:
                output_path = Path(arguments.output)
                try:
                    output_path.write_text(content, encoding="ascii")
                except OSError as error:
                    raise RecordingError(
                        f"could not write {output_path}: {error}"
                    ) from error
                print(
                    f"{output_path}: wrote {len(imported.transitions)} "
                    f"transitions for {movie.frame_count} FM2 frames; "
                    f"source_sha256={movie.source_sha256}; "
                    f"ram_init_option={movie.ram_init_option}; "
                    f"ram_init_seed={movie.ram_init_seed}; "
                    f"direction_policy={policy}"
                )
            return 0

        recording = _validated_from_arguments(arguments)
        policy = direction_policy_label(arguments.hardware_playable)

        if arguments.command == "validate":
            print(f"{recording.source}: valid")
            print(
                f"transitions={len(recording.transitions)} "
                f"end_frame={recording.end_frame} sha256={recording.sha256}"
            )
            print(
                f"direction_policy={policy}; "
                f"opposite_direction_transitions="
                f"{count_opposite_direction_transitions(recording)}"
            )
            return 0

        header = render_c_header(
            recording,
            symbol_prefix=arguments.symbol_prefix,
            hardware_playable=arguments.hardware_playable,
        )
        if arguments.output == "-":
            sys.stdout.write(header)
        else:
            output_path = Path(arguments.output)
            try:
                output_path.write_text(header, encoding="ascii")
            except OSError as error:
                raise RecordingError(
                    f"could not write {output_path}: {error}"
                ) from error
            print(
                f"{output_path}: wrote {len(build_segments(recording))} segments; "
                f"direction_policy={policy}"
            )
        return 0
    except RecordingError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
