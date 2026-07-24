#!/usr/bin/env python3
"""Capture one bounded PNG while a Neo Geo replay is debugger-paused."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys
from typing import Sequence


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DEFAULT_TIMEOUT_SECONDS = 10.0
DEFAULT_MAX_BYTES = 4 * 1024 * 1024
MAX_SEQUENCE_INDEX = 64


class CaptureError(RuntimeError):
    """Raised when a debugger-paused frame cannot be trusted."""


def inspect_png(path: Path, maximum_bytes: int) -> tuple[int, int, int]:
    try:
        size = path.stat().st_size
    except OSError as error:
        raise CaptureError(f"capture is missing: {path}") from error
    if size < 24:
        raise CaptureError(f"capture is too small to be a PNG: {path}")
    if size > maximum_bytes:
        raise CaptureError(
            f"capture is {size} bytes; maximum is {maximum_bytes}: {path}"
        )
    with path.open("rb") as source:
        header = source.read(24)
    if header[:8] != PNG_SIGNATURE:
        raise CaptureError(f"capture has an invalid PNG signature: {path}")
    if header[12:16] != b"IHDR":
        raise CaptureError(f"capture has no leading PNG IHDR: {path}")
    if struct.unpack(">I", header[8:12])[0] != 13:
        raise CaptureError(f"capture has an invalid PNG IHDR length: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width == 0 or height == 0 or width > 4096 or height > 4096:
        raise CaptureError(
            f"capture dimensions are invalid: {width}x{height}: {path}"
        )
    return width, height, size


def next_sequence_path(directory: Path) -> tuple[Path, Path, int]:
    counter = directory / ".next-stage"
    try:
        raw_value = counter.read_text(encoding="ascii").strip()
    except FileNotFoundError:
        index = 1
    except OSError as error:
        raise CaptureError(f"cannot read capture counter: {error}") from error
    else:
        try:
            index = int(raw_value)
        except ValueError as error:
            raise CaptureError("capture counter is not an integer") from error
    if not 1 <= index <= MAX_SEQUENCE_INDEX:
        raise CaptureError(
            f"capture sequence index {index} is outside "
            f"1..{MAX_SEQUENCE_INDEX}"
        )
    output = directory / f"stage-{index:04d}.png"
    return output, counter, index


def capture_frame(
    scrot: str,
    output: Path,
    timeout_seconds: float,
    maximum_bytes: int,
) -> tuple[int, int, int]:
    if output.exists():
        raise CaptureError(f"refusing to overwrite capture: {output}")
    temporary = output.with_name(output.stem + ".partial.png")
    if temporary.exists():
        raise CaptureError(
            f"refusing to overwrite partial capture: {temporary}"
        )
    try:
        completed = subprocess.run(
            [scrot, "-z", "-o", str(temporary)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired as error:
        raise CaptureError(
            f"frame capture timed out after {timeout_seconds:g} seconds"
        ) from error
    if completed.returncode != 0:
        raise CaptureError(
            f"scrot failed with status {completed.returncode}: "
            f"{completed.stdout.strip()}"
        )
    dimensions = inspect_png(temporary, maximum_bytes)
    try:
        os.replace(temporary, output)
    except OSError as error:
        raise CaptureError(f"cannot finalize capture: {error}") from error
    return dimensions


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--sequence-dir", type=Path)
    parser.add_argument("--scrot", required=True)
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
    )
    parser.add_argument(
        "--max-bytes",
        type=int,
        default=DEFAULT_MAX_BYTES,
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    if not 0 < args.timeout <= 60:
        print(
            "frame capture failed: timeout must be in (0, 60] seconds",
            file=sys.stderr,
        )
        return 2
    if not 1024 <= args.max_bytes <= 16 * 1024 * 1024:
        print(
            "frame capture failed: max-bytes must be between 1024 and "
            "16777216",
            file=sys.stderr,
        )
        return 2

    counter: Path | None = None
    index: int | None = None
    try:
        if args.sequence_dir is not None:
            directory = args.sequence_dir.resolve(strict=True)
            if not directory.is_dir():
                raise CaptureError(
                    f"sequence destination is not a directory: {directory}"
                )
            output, counter, index = next_sequence_path(directory)
        else:
            output = args.output.expanduser().resolve()
            if not output.parent.is_dir():
                raise CaptureError(
                    f"capture parent is not a directory: {output.parent}"
                )

        width, height, size = capture_frame(
            args.scrot,
            output,
            args.timeout,
            args.max_bytes,
        )
        if counter is not None and index is not None:
            counter.write_text(f"{index + 1}\n", encoding="ascii")
        print(
            "REPLAY_SCREENSHOT "
            f"index={index or 0} width={width} height={height} "
            f"bytes={size} path={output}",
            flush=True,
        )
        return 0
    except (CaptureError, OSError) as error:
        print(f"frame capture failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
