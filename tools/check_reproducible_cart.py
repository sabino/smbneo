#!/usr/bin/env python3
"""Build the Neo Geo cartridge twice and require byte-identical output."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from typing import Iterable, Sequence

import puzzledp_compat


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
NEOGEO_DIR = REPOSITORY_ROOT / "platform" / "neogeo"
BUFFER_SIZE = 64 * 1024


@dataclass(frozen=True)
class Region:
    label: str
    filename: str
    expected_size: int


@dataclass(frozen=True)
class ArtifactResult:
    label: str
    size: int
    sha256: str


@dataclass(frozen=True)
class Archive:
    label: str
    filename: str
    compatibility_profile: bool = False


REGIONS = (
    Region("P", "smbneogeo-p1.p1", 1024 * 1024),
    Region("C1", "smbneogeo-c1.c1", 2 * 1024 * 1024),
    Region("C2", "smbneogeo-c2.c2", 2 * 1024 * 1024),
    Region("S", "smbneogeo-s1.s1", 128 * 1024),
    Region("M", "smbneogeo-m1.m1", 128 * 1024),
    Region("V", "smbneogeo-v1.v1", 512 * 1024),
)
ARCHIVES = (
    Archive("Compatibility ZIP", "puzzledp.zip", compatibility_profile=True),
    Archive("Hardware ZIP", "smbneogeo.zip"),
)
MANIFEST_FILENAME = "asset-manifest.json"


class CartCheckError(RuntimeError):
    """One or more cartridge build or comparison checks failed."""

    def __init__(self, issues: Iterable[str]):
        self.issues = tuple(issues)
        super().__init__("\n".join(self.issues))


def _artifact_path(build_dir: Path, filename: str) -> Path:
    return build_dir / "rom" / filename


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(BUFFER_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def _first_difference(first: Path, second: Path) -> tuple[int, int | None, int | None] | None:
    """Return offset and differing bytes, using None to represent EOF."""

    offset = 0
    with first.open("rb") as first_file, second.open("rb") as second_file:
        while True:
            first_chunk = first_file.read(BUFFER_SIZE)
            second_chunk = second_file.read(BUFFER_SIZE)
            if first_chunk == second_chunk:
                if not first_chunk:
                    return None
                offset += len(first_chunk)
                continue

            common_size = min(len(first_chunk), len(second_chunk))
            for index in range(common_size):
                if first_chunk[index] != second_chunk[index]:
                    return (
                        offset + index,
                        first_chunk[index],
                        second_chunk[index],
                    )

            return (
                offset + common_size,
                first_chunk[common_size] if len(first_chunk) > common_size else None,
                second_chunk[common_size] if len(second_chunk) > common_size else None,
            )


def _format_byte(value: int | None) -> str:
    return "EOF" if value is None else f"0x{value:02x}"


def _compare_artifact(
    label: str,
    first: Path,
    second: Path,
) -> tuple[ArtifactResult | None, str | None]:
    if not first.is_file() or not second.is_file():
        return None, None

    difference = _first_difference(first, second)
    if difference is None:
        return ArtifactResult(label, first.stat().st_size, _sha256(first)), None

    offset, first_byte, second_byte = difference
    first_size = first.stat().st_size
    second_size = second.stat().st_size
    issue = (
        f"{label} bytes differ at offset 0x{offset:x}: "
        f"build 1 has {_format_byte(first_byte)}, "
        f"build 2 has {_format_byte(second_byte)}; "
        f"sizes are {first_size} and {second_size} bytes; "
        f"SHA-256 values are {_sha256(first)} and {_sha256(second)}"
    )
    return None, issue


def _validate_manifest(build_dir: Path, build_label: str) -> list[str]:
    manifest_path = build_dir / "assets" / MANIFEST_FILENAME
    if not manifest_path.exists():
        return [
            f"{build_label} is missing asset manifest: {manifest_path}"
        ]
    if not manifest_path.is_file():
        return [f"{build_label} asset manifest is not a file: {manifest_path}"]

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return [f"{build_label} asset manifest is invalid: {error}"]

    if not isinstance(manifest, dict):
        return [f"{build_label} asset manifest must contain a JSON object"]

    issues = []
    if manifest.get("verified_revision") is not True:
        issues.append(
            f"{build_label} asset manifest has verified_revision="
            f"{manifest.get('verified_revision')!r}; expected true"
        )

    prg_bytes_written = manifest.get("prg_bytes_written")
    if type(prg_bytes_written) is not int or prg_bytes_written != 0:
        issues.append(
            f"{build_label} asset manifest has prg_bytes_written="
            f"{prg_bytes_written!r}; expected 0"
        )
    return issues


def _validate_build(build_dir: Path, build_label: str) -> list[str]:
    issues = []
    for region in REGIONS:
        path = _artifact_path(build_dir, region.filename)
        if not path.is_file():
            issues.append(f"{build_label} is missing {region.label} region: {path}")
            continue

        actual_size = path.stat().st_size
        if actual_size != region.expected_size:
            issues.append(
                f"{build_label} {region.label} region has {actual_size} bytes; "
                f"expected {region.expected_size} ({path})"
            )

    for archive in ARCHIVES:
        cart_path = _artifact_path(build_dir, archive.filename)
        if not cart_path.is_file():
            issues.append(
                f"{build_label} is missing {archive.label}: {cart_path}"
            )
        elif cart_path.stat().st_size == 0:
            issues.append(
                f"{build_label} {archive.label} is empty: {cart_path}"
            )
        elif archive.compatibility_profile:
            try:
                puzzledp_compat.validate_archive(cart_path)
            except puzzledp_compat.CompatibilityError as error:
                issues.append(
                    f"{build_label} {archive.label} is invalid: {error}"
                )

    issues.extend(_validate_manifest(build_dir, build_label))
    return issues


def _run_build(
    rom: Path,
    build_dir: Path,
    neogeo_dir: Path,
    make_program: str,
    build_number: int,
) -> None:
    command = [
        make_program,
        "--no-print-directory",
        "-C",
        str(neogeo_dir),
        "cart",
        "hardware-cart",
        f"BUILD={build_dir}",
        f"SMB_ROM={rom}",
    ]
    print(
        f"Building isolated cartridge pass {build_number}/2: "
        f"{shlex.join(command)}",
        flush=True,
    )
    try:
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as error:
        raise CartCheckError(
            [
                f"build {build_number} failed with exit status "
                f"{error.returncode}; see the build output above"
            ]
        ) from error
    except OSError as error:
        raise CartCheckError(
            [f"could not start build {build_number} with {make_program!r}: {error}"]
        ) from error


def check_reproducible_cart(
    rom: Path,
    *,
    neogeo_dir: Path = NEOGEO_DIR,
    make_program: str = "make",
    temp_parent: Path | None = None,
) -> tuple[ArtifactResult, ...]:
    """Build twice in owned temporary directories and compare all cartridge output."""

    rom = rom.expanduser().resolve()
    neogeo_dir = neogeo_dir.resolve()
    if not rom.is_file():
        raise CartCheckError([f"ROM input does not exist or is not a file: {rom}"])
    if not (neogeo_dir / "Makefile").is_file():
        raise CartCheckError([f"Neo Geo Makefile was not found in {neogeo_dir}"])

    temporary_parent = None
    if temp_parent is not None:
        temporary_parent = str(temp_parent.resolve())

    with tempfile.TemporaryDirectory(
        prefix="smb-cart-repro-",
        dir=temporary_parent,
    ) as temporary_root_text:
        temporary_root = Path(temporary_root_text)
        first_build = temporary_root / "build-1"
        second_build = temporary_root / "build-2"

        _run_build(rom, first_build, neogeo_dir, make_program, 1)
        _run_build(rom, second_build, neogeo_dir, make_program, 2)

        issues = []
        issues.extend(_validate_build(first_build, "build 1"))
        issues.extend(_validate_build(second_build, "build 2"))

        results = []
        for region in REGIONS:
            result, issue = _compare_artifact(
                region.label,
                _artifact_path(first_build, region.filename),
                _artifact_path(second_build, region.filename),
            )
            if result is not None:
                results.append(result)
            if issue is not None:
                issues.append(issue)

        for archive in ARCHIVES:
            result, issue = _compare_artifact(
                archive.label,
                _artifact_path(first_build, archive.filename),
                _artifact_path(second_build, archive.filename),
            )
            if result is not None:
                results.append(result)
            if issue is not None:
                issues.append(issue)

        if issues:
            raise CartCheckError(issues)
        return tuple(results)


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rom",
        required=True,
        type=Path,
        help="legally obtained input .nes file or ZIP containing one .nes file",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        results = check_reproducible_cart(
            args.rom,
            make_program=os.environ.get("MAKE", "make"),
        )
    except CartCheckError as error:
        print("Cartridge reproducibility check failed:", file=sys.stderr)
        for issue in error.issues:
            print(f"  - {issue}", file=sys.stderr)
        return 1

    print("Cartridge is reproducible; both isolated builds match exactly:")
    for result in results:
        print(
            f"  {result.label}: {result.size} bytes, "
            f"SHA-256 {result.sha256}"
        )
    print("The working platform/neogeo/build directory was not modified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
