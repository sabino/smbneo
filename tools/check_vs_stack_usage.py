#!/usr/bin/env python3
"""Validate GCC -fstack-usage output for the VS native C core."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


class StackUsageError(ValueError):
    """Raised when a stack-usage report is unsafe or not understood."""


@dataclass(frozen=True)
class StackRecord:
    location: str
    function: str
    bytes: int
    kind: str


_ROW = re.compile(r"^(?P<location>[^\t]+)\t(?P<size>\d+)\t(?P<kind>[^\t]+)$")
_LOCATION = re.compile(r"^.+:\d+:\d+:(?P<function>.+)$")


def parse_stack_usage(text: str, max_frame: int = 96) -> list[StackRecord]:
    """Parse GCC rows and enforce a finite, bounded per-frame limit.

    GCC emits ``static`` for leaf frames and ``dynamic,bounded`` for functions
    containing calls whose dynamic stack use it can still bound.  Everything
    else is rejected so a changed compiler report cannot silently weaken the
    hardware safety review.
    """
    if max_frame < 0:
        raise StackUsageError("max-frame must be non-negative")
    records: list[StackRecord] = []
    for number, raw in enumerate(text.splitlines(), 1):
        if not raw.strip():
            continue
        match = _ROW.fullmatch(raw)
        if not match:
            raise StackUsageError(f"line {number}: malformed .su row")
        location = match["location"]
        location_match = _LOCATION.fullmatch(location)
        if not location_match or not location_match["function"]:
            raise StackUsageError(f"line {number}: malformed function location")
        kind = match["kind"]
        if kind not in ("static", "dynamic,bounded"):
            raise StackUsageError(f"line {number}: unsupported stack bound {kind!r}")
        size = int(match["size"])
        if size > max_frame:
            raise StackUsageError(
                f"line {number}: {location_match['function']} uses {size} bytes "
                f"(limit {max_frame})"
            )
        records.append(StackRecord(location, location_match["function"], size, kind))
    if not records:
        raise StackUsageError("empty .su report")
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--max-frame", type=int, default=96)
    args = parser.parse_args()
    try:
        records = parse_stack_usage(args.report.read_text(encoding="utf-8"), args.max_frame)
    except (OSError, StackUsageError) as exc:
        parser.error(str(exc))
    maximum = max(record.bytes for record in records)
    print(f"{len(records)} stack records valid; maximum frame {maximum} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
