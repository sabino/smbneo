#!/usr/bin/env python3
"""Test the MoonBit generator and compare isolated output with checked-in C."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


GENERATED_LIBRARY_FILES = (
    "constants.h",
    "data.h",
    "data.c",
    "code.h",
    "code.c",
)
MAX_OUTPUT_CHARACTERS = 8000


class CodegenCheckError(RuntimeError):
    """Raised when generator tests or output verification fails."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_executable(command: str) -> Path:
    candidate = (
        shutil.which(command)
        if os.sep not in command
        else str(Path(command).expanduser())
    )
    if candidate is None:
        raise CodegenCheckError(
            f"MoonBit executable is unavailable: {command}"
        )
    resolved = Path(candidate).resolve()
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise CodegenCheckError(
            f"MoonBit executable is not runnable: {resolved}"
        )
    return resolved


def run_moon(
    moon: Path,
    arguments: list[str],
    repository: Path,
    timeout_seconds: float,
) -> None:
    try:
        completed = subprocess.run(
            [str(moon), *arguments],
            cwd=repository,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired as error:
        raise CodegenCheckError(
            f"MoonBit {' '.join(arguments)} exceeded "
            f"{timeout_seconds:g} seconds"
        ) from error
    if completed.returncode != 0:
        raise CodegenCheckError(
            f"MoonBit {' '.join(arguments)} failed with status "
            f"{completed.returncode}:\n"
            f"{completed.stdout[-MAX_OUTPUT_CHARACTERS:]}"
        )


def verify_codegen(
    repository: Path,
    moon: Path,
    timeout_seconds: float,
) -> None:
    module = repository / "moon.mod.json"
    source = repository / "src"
    expected_library = repository / "codegen" / "lib"
    if not module.is_file() or not source.is_dir():
        raise CodegenCheckError(
            f"repository does not contain the MoonBit project: {repository}"
        )

    with tempfile.TemporaryDirectory(
        prefix="smb-codegen-consistency."
    ) as temporary_name:
        temporary = Path(temporary_name)
        shutil.copy2(module, temporary / module.name)
        shutil.copytree(source, temporary / "src")
        (temporary / "codegen" / "lib").mkdir(parents=True)

        run_moon(
            moon,
            ["test", "src/lower"],
            temporary,
            timeout_seconds,
        )
        run_moon(
            moon,
            ["test", "src/transpile"],
            temporary,
            timeout_seconds,
        )
        run_moon(
            moon,
            ["run", "src/main"],
            temporary,
            timeout_seconds,
        )

        mismatches: list[str] = []
        for name in GENERATED_LIBRARY_FILES:
            expected = expected_library / name
            generated = temporary / "codegen" / "lib" / name
            if not expected.is_file():
                mismatches.append(f"{name}: checked-in output is missing")
                continue
            if not generated.is_file():
                mismatches.append(f"{name}: isolated output is missing")
                continue
            if expected.read_bytes() != generated.read_bytes():
                mismatches.append(
                    f"{name}: checked-in sha256={sha256_file(expected)}, "
                    f"generated sha256={sha256_file(generated)}"
                )
        if mismatches:
            raise CodegenCheckError(
                "checked-in generated C is stale:\n  "
                + "\n  ".join(mismatches)
            )

    print(
        "MoonBit lowering/transpiler tests and isolated code-generation "
        "consistency: OK"
    )


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--moon", default="moon")
    parser.add_argument(
        "--repository",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--timeout", type=float, default=120.0)
    return parser


def main() -> int:
    arguments = build_argument_parser().parse_args()
    if not 0 < arguments.timeout <= 3600:
        print(
            "codegen verification failed: timeout must be between "
            "0 and 3600 seconds",
            file=sys.stderr,
        )
        return 2
    try:
        verify_codegen(
            arguments.repository.expanduser().resolve(),
            resolve_executable(arguments.moon),
            arguments.timeout,
        )
        return 0
    except (CodegenCheckError, OSError) as error:
        print(f"codegen verification failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
