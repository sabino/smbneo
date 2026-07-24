#!/usr/bin/env python3
"""Capture and validate bounded Neo Geo audio in normal GnGeo mode."""

from __future__ import annotations

import argparse
from array import array
from dataclasses import asdict, dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import subprocess
import sys
import time
from typing import Sequence

import measure_neogeo_cadence as cadence


DEFAULT_SAMPLE_RATE = 22050
DEFAULT_CHANNELS = 2
BYTES_PER_SAMPLE = 2
DEFAULT_CAPTURE_SECONDS = 8.0
DEFAULT_INPUT_ATTEMPTS = 5
INPUT_SETTLE_SECONDS = 2.0
INPUT_MOTION_SECONDS = 1.0
COMMAND_TIMEOUT_SECONDS = 5.0
OVERALL_SLACK_SECONDS = 20.0
MAXIMUM_CAPTURE_SECONDS = 30.0
MAXIMUM_STARTUP_TIMEOUT = 60.0
MAXIMUM_INPUT_ATTEMPTS = 8
MAXIMUM_SAMPLE_RATE = 48000
MAXIMUM_SCALE = 8
MINIMUM_PEAK = 256
MINIMUM_RMS = 0.001
RESULT_SCHEMA_VERSION = 1


class AudioProbeError(RuntimeError):
    """Raised when normal-mode audio evidence is unavailable or invalid."""


@dataclass(frozen=True)
class AudioStatistics:
    bytes: int
    samples: int
    frames: int
    nonzero_samples: int
    peak: int
    rms: float
    estimated_seconds: float
    sha256: str


def build_gngeo_command(
    executable: str,
    rom_dir: Path,
    data_file: Path,
    rom_set: str,
    sample_rate: int,
    scale: int,
) -> list[str]:
    if sample_rate <= 0:
        raise ValueError("sample rate must be positive")
    if scale <= 0:
        raise ValueError("GnGeo scale must be positive")
    return [
        executable,
        "-b",
        "soft",
        "--screen320",
        f"--scale={scale}",
        "--no-resize",
        "--sound",
        f"--samplerate={sample_rate}",
        "--no-autoframeskip",
        "--68kclock=0",
        "--z80clock=0",
        "--system",
        "home",
        "--no-debug",
        "--p1control",
        cadence.DEFAULT_P1_CONTROLS,
        "--p2control",
        cadence.DEFAULT_P2_CONTROLS,
        "-i",
        str(rom_dir),
        "-d",
        str(data_file),
        rom_set,
    ]


def analyze_s16le_stereo(
    path: Path,
    sample_rate: int,
    channels: int = DEFAULT_CHANNELS,
    start_offset: int = 0,
    byte_count: int | None = None,
) -> AudioStatistics:
    if sample_rate <= 0:
        raise ValueError("sample rate must be positive")
    if channels <= 0:
        raise ValueError("channel count must be positive")

    file_size = path.stat().st_size
    frame_bytes = channels * BYTES_PER_SAMPLE
    if start_offset < 0:
        raise ValueError("audio start offset must be non-negative")
    if start_offset % frame_bytes != 0:
        raise AudioProbeError(
            f"audio start offset {start_offset} is not frame-aligned"
        )
    if file_size < start_offset:
        raise AudioProbeError(
            f"audio start offset {start_offset} exceeds "
            f"{file_size}-byte capture"
        )
    available = file_size - start_offset
    if byte_count is None:
        size = available
    else:
        if byte_count <= 0:
            raise ValueError("audio byte count must be positive")
        if byte_count > available:
            raise AudioProbeError(
                f"requested {byte_count}-byte audio segment exceeds "
                f"{available} available bytes"
            )
        size = byte_count
    if size == 0:
        raise AudioProbeError("audio capture is empty")
    if size % frame_bytes != 0:
        raise AudioProbeError(
            f"audio capture is {size} bytes, not whole {frame_bytes}-byte "
            "stereo frames"
        )

    digest = hashlib.sha256()
    sample_count = 0
    nonzero_samples = 0
    peak = 0
    sum_squares = 0

    with path.open("rb") as source:
        source.seek(start_offset)
        remaining = size
        while remaining:
            chunk = source.read(min(1024 * 1024, remaining))
            if not chunk:
                raise AudioProbeError(
                    "audio capture ended before the requested segment"
                )
            remaining -= len(chunk)
            digest.update(chunk)
            values = array("h")
            values.frombytes(chunk)
            if sys.byteorder != "little":
                values.byteswap()
            sample_count += len(values)
            for value in values:
                magnitude = abs(value)
                if magnitude != 0:
                    nonzero_samples += 1
                if magnitude > peak:
                    peak = magnitude
                sum_squares += value * value

    rms = (sum_squares / sample_count) ** 0.5 / 32768.0
    frames = sample_count // channels
    return AudioStatistics(
        bytes=size,
        samples=sample_count,
        frames=frames,
        nonzero_samples=nonzero_samples,
        peak=peak,
        rms=rms,
        estimated_seconds=frames / sample_rate,
        sha256=digest.hexdigest(),
    )


def assert_audible_signal(statistics: AudioStatistics) -> None:
    if statistics.peak < MINIMUM_PEAK:
        raise AudioProbeError(
            f"audio peak {statistics.peak} is below {MINIMUM_PEAK}"
        )
    if statistics.rms < MINIMUM_RMS:
        raise AudioProbeError(
            f"audio RMS {statistics.rms:.6f} is below {MINIMUM_RMS:.6f}"
        )


def _run_xdotool(
    executable: str,
    display: str,
    arguments: Sequence[str],
    capture: bool = False,
    deadline: float | None = None,
) -> subprocess.CompletedProcess[str]:
    timeout = COMMAND_TIMEOUT_SECONDS
    if deadline is not None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AudioProbeError(
                f"audio probe deadline expired before xdotool "
                f"{' '.join(arguments)}"
            )
        timeout = min(timeout, remaining)

    environment = os.environ.copy()
    environment["DISPLAY"] = display
    try:
        return subprocess.run(
            [executable, *arguments],
            env=environment,
            stdout=subprocess.PIPE if capture else subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise AudioProbeError(
            f"xdotool timed out after {timeout:.1f}s: "
            f"{' '.join(arguments)}"
        ) from error


def _check_runtime(
    gngeo: subprocess.Popen[str],
    raw_path: Path,
    maximum_bytes: int,
    deadline: float,
    stage: str,
) -> None:
    status = gngeo.poll()
    if status is not None:
        raise AudioProbeError(
            f"GnGeo exited during {stage} (status {status})"
        )
    if time.monotonic() >= deadline:
        raise AudioProbeError(f"audio probe deadline expired during {stage}")
    if raw_path.is_file() and raw_path.stat().st_size > maximum_bytes:
        raise AudioProbeError(
            f"audio capture exceeded its live {maximum_bytes}-byte "
            f"safety bound during {stage}"
        )


def _wait_with_runtime_checks(
    seconds: float,
    gngeo: subprocess.Popen[str],
    raw_path: Path,
    maximum_bytes: int,
    deadline: float,
    stage: str,
) -> None:
    wait_deadline = time.monotonic() + seconds
    while time.monotonic() < wait_deadline:
        _check_runtime(
            gngeo,
            raw_path,
            maximum_bytes,
            deadline,
            stage,
        )
        remaining = min(
            wait_deadline - time.monotonic(),
            deadline - time.monotonic(),
        )
        if remaining <= 0:
            raise AudioProbeError(
                f"audio probe deadline expired during {stage}"
            )
        time.sleep(min(0.1, remaining))


def _make_file_size_limiter(maximum_bytes: int):
    def apply_limit() -> None:
        resource.setrlimit(
            resource.RLIMIT_FSIZE,
            (maximum_bytes, maximum_bytes),
        )

    return apply_limit


def _activate_gameplay(
    xdotool: str,
    display: str,
    gngeo: subprocess.Popen[str],
    timeout_seconds: float,
    raw_path: Path,
    maximum_bytes: int,
    overall_deadline: float,
) -> str:
    deadline = min(
        overall_deadline,
        time.monotonic() + timeout_seconds,
    )
    window = ""
    while time.monotonic() < deadline:
        _check_runtime(
            gngeo,
            raw_path,
            maximum_bytes,
            overall_deadline,
            "audio-probe startup",
        )
        completed = _run_xdotool(
            xdotool,
            display,
            ("search", "--name", "Gngeo"),
            capture=True,
            deadline=deadline,
        )
        if completed.returncode == 0:
            window = next(
                (
                    line.strip()
                    for line in completed.stdout.splitlines()
                    if line.strip()
                ),
                "",
            )
        if window:
            break
        _wait_with_runtime_checks(
            0.1,
            gngeo,
            raw_path,
            maximum_bytes,
            overall_deadline,
            "window discovery",
        )
    if not window:
        raise AudioProbeError("timed out waiting for the GnGeo window")

    for arguments in (
        ("windowfocus", "--sync", window),
        ("mousemove", "--window", window, "320", "224", "click", "1"),
    ):
        completed = _run_xdotool(
            xdotool,
            display,
            arguments,
            deadline=overall_deadline,
        )
        if completed.returncode != 0:
            raise AudioProbeError(
                f"xdotool command failed: {' '.join(arguments)}"
            )

    # The window can appear during the BIOS handoff. Give the title enough
    # time to begin accepting controls before the first bounded input attempt.
    _wait_with_runtime_checks(
        0.75,
        gngeo,
        raw_path,
        maximum_bytes,
        overall_deadline,
        "title startup",
    )
    return window


def _send_gameplay_input(
    xdotool: str,
    display: str,
    window: str,
    gngeo: subprocess.Popen[str],
    raw_path: Path,
    maximum_bytes: int,
    deadline: float,
) -> None:
    for arguments, delay in (
        (("windowfocus", "--sync", window), 0.0),
        (("key", "1"), INPUT_SETTLE_SECONDS),
        (("keydown", "Right"), 0.0),
        (("keydown", "s"), 0.0),
        (("key", "a"), INPUT_MOTION_SECONDS),
        (("keyup", "s"), 0.0),
        (("keyup", "Right"), 0.0),
    ):
        completed = _run_xdotool(
            xdotool,
            display,
            arguments,
            deadline=deadline,
        )
        if completed.returncode != 0:
            raise AudioProbeError(
                f"xdotool command failed: {' '.join(arguments)}"
            )
        if delay:
            _wait_with_runtime_checks(
                delay,
                gngeo,
                raw_path,
                maximum_bytes,
                deadline,
                "gameplay activation input",
            )


def _capture_segment(
    gngeo: subprocess.Popen[str],
    raw_path: Path,
    start_offset: int,
    required_bytes: int,
    maximum_bytes: int,
    deadline: float,
) -> None:
    target_size = start_offset + required_bytes
    if target_size > maximum_bytes:
        raise AudioProbeError(
            f"requested audio segment reaches {target_size} bytes, beyond "
            f"the {maximum_bytes}-byte live safety bound"
        )
    while not raw_path.is_file() or raw_path.stat().st_size < target_size:
        _wait_with_runtime_checks(
            0.05,
            gngeo,
            raw_path,
            maximum_bytes,
            deadline,
            "post-activation audio capture",
        )


def _capture_screenshot(
    executable: str,
    path: Path,
    environment: dict[str, str],
    deadline: float,
) -> None:
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise AudioProbeError(
            "audio probe deadline expired before screenshot"
        )
    timeout = min(COMMAND_TIMEOUT_SECONDS, remaining)
    try:
        completed = subprocess.run(
            [executable, "-z", "-o", str(path)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise AudioProbeError(
            f"final-frame screenshot timed out after {timeout:.1f}s"
        ) from error
    if completed.returncode != 0:
        raise AudioProbeError(
            "failed to capture the final emulator frame: "
            f"{completed.stdout.strip()}"
        )


def _new_evidence_directory(requested: Path | None) -> Path:
    return cadence._create_evidence_directory(requested)


def _tail(path: Path, line_count: int = 80) -> str:
    return cadence._tail(path, line_count)


def _validate_probe_arguments(args: argparse.Namespace) -> None:
    for value, label, maximum in (
        (
            args.capture_seconds,
            "capture duration",
            MAXIMUM_CAPTURE_SECONDS,
        ),
        (
            args.startup_timeout,
            "startup timeout",
            MAXIMUM_STARTUP_TIMEOUT,
        ),
    ):
        if not math.isfinite(value) or value <= 0:
            raise AudioProbeError(f"{label} must be finite and positive")
        if value > maximum:
            raise AudioProbeError(
                f"{label} exceeds the {maximum:g}s safety bound"
            )
    if (
        args.input_attempts <= 0
        or args.input_attempts > MAXIMUM_INPUT_ATTEMPTS
    ):
        raise AudioProbeError(
            f"input attempt count must be between 1 and "
            f"{MAXIMUM_INPUT_ATTEMPTS}"
        )
    if args.sample_rate <= 0 or args.sample_rate > MAXIMUM_SAMPLE_RATE:
        raise AudioProbeError(
            f"sample rate must be between 1 and {MAXIMUM_SAMPLE_RATE}"
        )
    if args.scale <= 0 or args.scale > MAXIMUM_SCALE:
        raise AudioProbeError(
            f"GnGeo scale must be between 1 and {MAXIMUM_SCALE}"
        )


def _overall_duration_seconds(args: argparse.Namespace) -> float:
    return (
        2.0 * args.startup_timeout
        + args.input_attempts
        * (
            INPUT_SETTLE_SECONDS
            + INPUT_MOTION_SECONDS
            + 1.0
        )
        + args.capture_seconds
        + OVERALL_SLACK_SECONDS
    )


def run_probe(args: argparse.Namespace) -> int:
    _validate_probe_arguments(args)

    rom_dir = cadence._require_directory(args.rom_dir, "ROM directory")
    data_file = cadence._require_file(args.data_file, "GnGeo data file")
    cartridge = cadence._require_file(
        rom_dir / f"{args.rom_set}.zip",
        "GnGeo cartridge",
    )
    gngeo_executable = cadence._resolve_executable(args.gngeo)
    xvfb_executable = cadence._resolve_executable(args.xvfb)
    xdotool_executable = cadence._resolve_executable(args.xdotool)
    scrot_executable = cadence._resolve_executable(args.scrot)

    evidence_dir = _new_evidence_directory(args.evidence_dir)
    raw_path = evidence_dir / "audio-s16le-stereo.raw"
    gngeo_log_path = evidence_dir / "gngeo.log"
    xvfb_log_path = evidence_dir / "xvfb.log"
    input_log_path = evidence_dir / "input.log"
    screenshot_path = evidence_dir / "final-frame.png"
    command = build_gngeo_command(
        gngeo_executable,
        rom_dir,
        data_file,
        args.rom_set,
        args.sample_rate,
        args.scale,
    )
    frame_bytes = DEFAULT_CHANNELS * BYTES_PER_SAMPLE
    required_frames = math.ceil(args.capture_seconds * args.sample_rate)
    required_audio_bytes = required_frames * frame_bytes
    overall_duration = _overall_duration_seconds(args)
    maximum_bytes = math.ceil(
        overall_duration
        * args.sample_rate
        * frame_bytes
        * 2.0
    )
    overall_deadline = time.monotonic() + overall_duration
    runtime_home = evidence_dir / "isolated-home"
    runtime_config = runtime_home / ".config"
    runtime_cache = runtime_home / ".cache"
    runtime_config.mkdir(parents=True)
    runtime_cache.mkdir()

    print(f"[audio] evidence directory: {evidence_dir}", flush=True)
    owned = cadence.OwnedProcessGroups()
    statistics: AudioStatistics | None = None
    activation_audio_offset = 0
    evidence_audio_offset = 0
    input_attempts = 0

    try:
        with (
            xvfb_log_path.open("w", buffering=1) as xvfb_log,
            gngeo_log_path.open("w", buffering=1) as gngeo_log,
            input_log_path.open("w", buffering=1) as input_log,
        ):
            _xvfb, display = cadence._start_xvfb(
                xvfb_executable,
                xvfb_log,
                owned,
                args.startup_timeout,
            )
            print(f"[audio] owned Xvfb is ready on {display}", flush=True)

            environment = os.environ.copy()
            environment["DISPLAY"] = display
            environment["HOME"] = str(runtime_home)
            environment["XDG_CONFIG_HOME"] = str(runtime_config)
            environment["XDG_CACHE_HOME"] = str(runtime_cache)
            environment["SDL_AUDIODRIVER"] = "disk"
            environment["SDL_DISKAUDIOFILE"] = str(raw_path)
            environment.pop("SDL_DISKAUDIODELAY", None)

            gngeo = owned.add(
                "GnGeo",
                subprocess.Popen(
                    command,
                    stdout=gngeo_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=environment,
                    start_new_session=True,
                    preexec_fn=_make_file_size_limiter(maximum_bytes),
                ),
            )
            window = _activate_gameplay(
                xdotool_executable,
                display,
                gngeo,
                args.startup_timeout,
                raw_path,
                maximum_bytes,
                overall_deadline,
            )
            if raw_path.is_file():
                activation_audio_offset = (
                    raw_path.stat().st_size // frame_bytes
                ) * frame_bytes

            for input_attempts in range(1, args.input_attempts + 1):
                _send_gameplay_input(
                    xdotool_executable,
                    display,
                    window,
                    gngeo,
                    raw_path,
                    maximum_bytes,
                    overall_deadline,
                )
                if gngeo.poll() is not None:
                    raise AudioProbeError(
                        "GnGeo exited after the Start input"
                    )
                try:
                    candidate = analyze_s16le_stereo(
                        raw_path,
                        args.sample_rate,
                        start_offset=activation_audio_offset,
                    )
                    assert_audible_signal(candidate)
                except (AudioProbeError, OSError):
                    input_log.write(
                        f"INPUT_ATTEMPT attempt={input_attempts} "
                        "signal=pending\n"
                    )
                    continue
                input_log.write(
                    f"GAMEPLAY_ACTIVE window={window} "
                    f"detection_offset={activation_audio_offset} "
                    f"input_attempt={input_attempts}\n"
                )
                evidence_audio_offset = (
                    raw_path.stat().st_size // frame_bytes
                ) * frame_bytes
                input_log.write(
                    f"EVIDENCE_SEGMENT offset={evidence_audio_offset} "
                    f"bytes={required_audio_bytes}\n"
                )
                break
            else:
                raise AudioProbeError(
                    f"no audible gameplay signal after "
                    f"{args.input_attempts} input attempts"
                )

            print(
                f"[audio] gameplay reached; capturing "
                f"{args.capture_seconds:g}s",
                flush=True,
            )
            _capture_segment(
                gngeo,
                raw_path,
                evidence_audio_offset,
                required_audio_bytes,
                maximum_bytes,
                overall_deadline,
            )
            _capture_screenshot(
                scrot_executable,
                screenshot_path,
                environment,
                overall_deadline,
            )
    except Exception:
        gngeo_tail = _tail(gngeo_log_path)
        input_tail = _tail(input_log_path)
        if gngeo_tail:
            print(
                f"[audio] GnGeo log tail:\n{gngeo_tail}",
                file=sys.stderr,
                flush=True,
            )
        if input_tail:
            print(
                f"[audio] input log tail:\n{input_tail}",
                file=sys.stderr,
                flush=True,
            )
        raise
    finally:
        owned.terminate_all()

    if not raw_path.is_file():
        raise AudioProbeError("SDL disk driver did not create an audio file")
    if raw_path.stat().st_size > maximum_bytes:
        raise AudioProbeError(
            f"audio capture exceeded its {maximum_bytes}-byte safety bound"
        )

    statistics = analyze_s16le_stereo(
        raw_path,
        args.sample_rate,
        start_offset=evidence_audio_offset,
        byte_count=required_audio_bytes,
    )
    assert_audible_signal(statistics)
    gngeo_log = gngeo_log_path.read_text(errors="replace")
    if (
        "SDL disk i/o audio driver" not in gngeo_log
        or str(raw_path) not in gngeo_log
    ):
        raise AudioProbeError(
            "GnGeo did not confirm the isolated SDL disk audio sink"
        )

    result = {
        "schema_version": RESULT_SCHEMA_VERSION,
        "mode": "normal",
        "debugger_enabled": False,
        "capture_seconds_after_active_input": args.capture_seconds,
        "input_attempts": input_attempts,
        "activation_detection_audio_offset": activation_audio_offset,
        "analyzed_audio_offset": evidence_audio_offset,
        "analyzed_audio_bytes": required_audio_bytes,
        "maximum_raw_capture_bytes": maximum_bytes,
        "audio_format": {
            "encoding": "signed-16-bit-little-endian",
            "channels": DEFAULT_CHANNELS,
            "sample_rate": args.sample_rate,
        },
        "signal_thresholds": {
            "minimum_peak": MINIMUM_PEAK,
            "minimum_rms": MINIMUM_RMS,
        },
        "statistics": asdict(statistics),
        "artifacts": {
            "cartridge": {
                "path": str(cartridge),
                "bytes": cartridge.stat().st_size,
                "sha256": cadence._sha256_file(cartridge),
            },
            "gngeo_data": {
                "path": str(data_file),
                "bytes": data_file.stat().st_size,
                "sha256": cadence._sha256_file(data_file),
            },
            "final_frame": {
                "path": str(screenshot_path),
                "bytes": screenshot_path.stat().st_size,
                "sha256": cadence._sha256_file(screenshot_path),
            },
        },
        "gngeo_command": command,
        "raw_capture_retained": args.keep_raw,
    }
    (evidence_dir / "result.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )

    if not args.keep_raw:
        raw_path.unlink()

    print(
        f"AUDIO_SIGNAL peak={statistics.peak} "
        f"rms={statistics.rms:.6f} "
        f"nonzero_samples={statistics.nonzero_samples} "
        f"captured_seconds={statistics.estimated_seconds:.3f}",
        flush=True,
    )
    print("ASSERT_AUDIBLE_SIGNAL passed", flush=True)
    return 0


def build_argument_parser() -> argparse.ArgumentParser:
    repository = Path(__file__).resolve().parents[1]
    rom = repository / "platform" / "neogeo" / "build" / "rom"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom-dir", type=Path, default=rom)
    parser.add_argument(
        "--data-file",
        type=Path,
        default=rom / "gngeo_data.zip",
    )
    parser.add_argument("--rom-set", default="smbneogeo")
    parser.add_argument(
        "--capture-seconds",
        type=float,
        default=DEFAULT_CAPTURE_SECONDS,
    )
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=15.0,
    )
    parser.add_argument(
        "--input-attempts",
        type=int,
        default=DEFAULT_INPUT_ATTEMPTS,
        help=(
            "bounded Start/gameplay attempts before declaring audio silent"
        ),
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        default=DEFAULT_SAMPLE_RATE,
    )
    parser.add_argument("--scale", type=int, default=1)
    parser.add_argument(
        "--evidence-dir",
        type=Path,
        help="new directory for logs and results; must not already exist",
    )
    parser.add_argument(
        "--keep-raw",
        action="store_true",
        help="retain the bounded raw capture alongside result.json",
    )
    parser.add_argument("--gngeo", default="ngdevkit-gngeo")
    parser.add_argument("--xvfb", default="Xvfb")
    parser.add_argument("--xdotool", default="xdotool")
    parser.add_argument("--scrot", default="scrot")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    try:
        return run_probe(build_argument_parser().parse_args(argv))
    except (
        AudioProbeError,
        cadence.CadenceError,
        OSError,
        ValueError,
    ) as error:
        print(f"audio probe failed: {error}", file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("audio probe interrupted", file=sys.stderr, flush=True)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
