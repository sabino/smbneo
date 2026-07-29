#!/usr/bin/env python3
"""Build deterministic replay checkpoints and selective-render ranges."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
from typing import Iterable, Sequence


DEFAULT_WINDOW_SPEC = "3435:3554,25691:25810"
DEFAULT_WARMUP_FRAMES = 240
WINDOW_RE = re.compile(r"^(?P<start>[0-9]+):(?P<end>[0-9]+)$")


class WindowError(ValueError):
    """Raised when a replay-window schedule would be ambiguous or invalid."""


@dataclass(frozen=True, order=True)
class Window:
    start: int
    end: int

    @property
    def frame_count(self) -> int:
        return self.end - self.start + 1


@dataclass(frozen=True)
class ReplayWindowSchedule:
    windows: tuple[Window, ...]
    warmup_frames: int
    checkpoints: tuple[int, ...]
    progress_intervals: tuple[int, ...]
    render_ranges: tuple[Window, ...]


def parse_windows(specifications: str | Iterable[str]) -> tuple[Window, ...]:
    """Parse ordered ``START:END`` ranges, allowing comma-separated values."""

    if isinstance(specifications, str):
        raw_specifications = (specifications,)
    else:
        raw_specifications = tuple(specifications)

    tokens: list[str] = []
    for specification in raw_specifications:
        tokens.extend(part.strip() for part in specification.split(","))
    if not tokens or any(not token for token in tokens):
        raise WindowError("at least one non-empty START:END window is required")

    windows: list[Window] = []
    for token in tokens:
        match = WINDOW_RE.fullmatch(token)
        if match is None:
            raise WindowError(
                f"invalid replay window {token!r}; expected START:END"
            )
        window = Window(
            start=int(match.group("start")),
            end=int(match.group("end")),
        )
        if window.start < 1:
            raise WindowError(
                f"window {token!r} must start at frame 1 or later"
            )
        if window.end < window.start:
            raise WindowError(f"window {token!r} ends before it starts")
        if window.end > 0xFFFFFFFF:
            raise WindowError(f"window {token!r} exceeds uint32 frame indices")
        if windows and window.start <= windows[-1].end:
            raise WindowError(
                "replay windows must be strictly ordered and non-overlapping"
            )
        windows.append(window)
    return tuple(windows)


def _merge_ranges(ranges: Sequence[Window]) -> tuple[Window, ...]:
    merged: list[Window] = []
    for candidate in ranges:
        if not merged or candidate.start > merged[-1].end + 1:
            merged.append(candidate)
            continue
        merged[-1] = Window(
            start=merged[-1].start,
            end=max(merged[-1].end, candidate.end),
        )
    return tuple(merged)


def build_schedule(
    windows: Sequence[Window],
    warmup_frames: int = DEFAULT_WARMUP_FRAMES,
) -> ReplayWindowSchedule:
    """Derive exact trap intervals and merged selective-render ranges."""

    validated = parse_windows(
        ",".join(f"{window.start}:{window.end}" for window in windows)
    )
    if not 0 <= warmup_frames <= 0xFFFFFFFF:
        raise WindowError("warmup frames must fit in uint32")
    if warmup_frames > validated[0].start:
        raise WindowError(
            "warmup begins before replay frame zero for the first window"
        )

    checkpoints = tuple(
        frame
        for window in validated
        for frame in (window.start - 1, window.end)
    )
    intervals: list[int] = []
    processed_records = 0
    for checkpoint in checkpoints:
        next_processed_records = checkpoint + 1
        interval = next_processed_records - processed_records
        if not 1 <= interval <= 0xFFFFFFFF:
            raise WindowError("checkpoint interval does not fit in uint32")
        intervals.append(interval)
        processed_records = next_processed_records

    render_ranges = _merge_ranges(
        tuple(
            Window(window.start - warmup_frames, window.end)
            for window in validated
        )
    )
    return ReplayWindowSchedule(
        windows=validated,
        warmup_frames=warmup_frames,
        checkpoints=checkpoints,
        progress_intervals=tuple(intervals),
        render_ranges=render_ranges,
    )


def rendered_frames_through(
    render_ranges: Sequence[Window],
    frame: int,
) -> int:
    """Count selectively rendered source records through ``frame`` inclusive."""

    count = 0
    for render_range in render_ranges:
        if frame < render_range.start:
            break
        count += max(
            0,
            min(frame, render_range.end) - render_range.start + 1,
        )
    return count


def emit_c_header(schedule: ReplayWindowSchedule) -> str:
    """Return a self-contained generated C header for ``replay_main.c``."""

    checkpoints = ",\n    ".join(
        f"UINT32_C({value})" for value in schedule.checkpoints
    )
    intervals = ",\n    ".join(
        f"UINT32_C({value})" for value in schedule.progress_intervals
    )
    ranges = ",\n    ".join(
        "{ UINT32_C(%d), UINT32_C(%d) }" % (window.start, window.end)
        for window in schedule.render_ranges
    )
    return f"""\
#ifndef SMB_NEOGEO_REPLAY_WINDOWS_H
#define SMB_NEOGEO_REPLAY_WINDOWS_H

/* Generated benchmark metadata. No replay input is embedded here. */
#include <stdint.h>

#define SMB_NEOGEO_REPLAY_WINDOW_COUNT UINT32_C({len(schedule.windows)})
#define SMB_NEOGEO_REPLAY_CHECKPOINT_COUNT UINT32_C({len(schedule.checkpoints)})
#define SMB_NEOGEO_REPLAY_RENDER_RANGE_COUNT UINT32_C({len(schedule.render_ranges)})
#define SMB_NEOGEO_REPLAY_WINDOW_WARMUP_FRAMES UINT32_C({schedule.warmup_frames})

static const uint32_t smb_neogeo_replay_checkpoint_frames[] = {{
    {checkpoints}
}};

static const uint32_t smb_neogeo_replay_progress_intervals[] = {{
    {intervals}
}};

static const uint32_t smb_neogeo_replay_render_ranges[][2] = {{
    {ranges}
}};

#endif
"""


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    emit = subparsers.add_parser("emit-header")
    emit.add_argument("--windows", default=DEFAULT_WINDOW_SPEC)
    emit.add_argument("--warmup", type=int, default=DEFAULT_WARMUP_FRAMES)
    emit.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _argument_parser().parse_args(argv)
    try:
        schedule = build_schedule(parse_windows(args.windows), args.warmup)
    except WindowError as error:
        _argument_parser().error(str(error))
    args.output.write_text(emit_c_header(schedule), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
