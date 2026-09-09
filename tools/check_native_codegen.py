#!/usr/bin/env python3
"""Static policy guard for the direct-C VS backend prototype."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


# These are architectural constraints, not style preferences.  The new
# backend must not quietly acquire dependencies from the translated oracle.
FORBIDDEN = (
    (r"\bVsCpu\b", "VsCpu dependency"),
    (r"\bvs_(?!native_)[A-Za-z0-9_]*", "translated VS helper/runtime"),
    (r"\b(?:pc|fuel|yielded|fault)\b", "resumable CPU state"),
    (r"\b(?:JSR|RTS|RTI)\b", "emulated return operation"),
    (r"\b(?:vs_push|vs_pop)\b", "emulated stack operation"),
    (
        r"\b(?:data_stack|data_sp|data_underflow|native_push|native_pop)\b",
        "runtime data stack",
    ),
)


def violations(text: str) -> list[str]:
    found = []
    for pattern, description in FORBIDDEN:
        if re.search(pattern, text, flags=re.IGNORECASE):
            found.append(description)
    return found


def check(path: Path) -> None:
    problems = violations(path.read_text(encoding="utf-8"))
    if problems:
        raise ValueError(f"{path}: forbidden direct-C backend constructs: {', '.join(problems)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    check(args.path)
    print(f"direct-C policy passed: {args.path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
