#!/usr/bin/env python3
"""Build both cartridge identities twice and require byte-identical output."""

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
import xml.etree.ElementTree as ET
import zipfile

import gen_mame_neogeo_software as mame_software
import puzzledp_compat
import check_gngeo_driver


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
    Region("P", "smbneo-p1.p1", 1024 * 1024),
    Region("C1", "smbneo-c1.c1", 2 * 1024 * 1024),
    Region("C2", "smbneo-c2.c2", 2 * 1024 * 1024),
    Region("S", "smbneo-s1.s1", 128 * 1024),
    Region("M", "smbneo-m1.m1", 128 * 1024),
    Region("V", "smbneo-v1.v1", 512 * 1024),
)
ARCHIVES = (
    Archive("Canonical ZIP", "smbneo.zip"),
    Archive("Compatibility ZIP", "puzzledp.zip", compatibility_profile=True),
)
GNGEO_DATA = "gngeo_data.zip"
GNGEO_DATA_LABEL = "GnGeo custom driver data"
MAME_SOFTWARE_LIST = Path("mame") / "hash" / "neogeo.xml"
MAME_SOFTWARE_LIST_LABEL = "MAME software list"
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
    if manifest.get("product_shortname") != "smbneo":
        issues.append(
            f"{build_label} asset manifest has product_shortname="
            f"{manifest.get('product_shortname')!r}; expected 'smbneo'"
        )
    if manifest.get("product_title") != "Super Mario Bros. Neo":
        issues.append(
            f"{build_label} asset manifest has product_title="
            f"{manifest.get('product_title')!r}; "
            "expected 'Super Mario Bros. Neo'"
        )
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


def _validate_hardware_archive(
    build_dir: Path,
    archive_path: Path,
) -> None:
    expected_names = {region.filename for region in REGIONS}
    try:
        with zipfile.ZipFile(archive_path) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise ValueError(f"{archive_path} contains duplicate filenames")
            actual_names = set(names)
            if actual_names != expected_names:
                missing = sorted(expected_names - actual_names)
                extra = sorted(actual_names - expected_names)
                details = []
                if missing:
                    details.append(f"missing {', '.join(missing)}")
                if extra:
                    details.append(f"unexpected {', '.join(extra)}")
                raise ValueError("; ".join(details))

            for region in REGIONS:
                info = archive.getinfo(region.filename)
                if info.file_size != region.expected_size:
                    raise ValueError(
                        f"{region.filename} has {info.file_size} bytes in "
                        f"{archive_path}; expected {region.expected_size}"
                    )
                archived = archive.read(region.filename)
                native = _artifact_path(build_dir, region.filename).read_bytes()
                if archived != native:
                    raise ValueError(
                        f"{region.filename} in {archive_path} does not match "
                        "the canonical native region"
                    )
    except (OSError, zipfile.BadZipFile, zipfile.LargeZipFile) as error:
        raise ValueError(f"cannot read {archive_path}: {error}") from error


def _validate_mame_software_list(
    build_dir: Path,
    software_list_path: Path,
) -> None:
    try:
        root = ET.parse(software_list_path).getroot()
    except (OSError, ET.ParseError) as error:
        raise ValueError(f"cannot parse {software_list_path}: {error}") from error

    if root.tag != "softwarelist" or root.get("name") != "neogeo":
        raise ValueError("MAME XML must be the neogeo software list")

    software_entries = root.findall("./software")
    if len(software_entries) != 1:
        raise ValueError(
            "MAME XML must contain exactly one local software entry"
        )
    software = software_entries[0]
    if software.get("name") != mame_software.GAME_NAME:
        raise ValueError(
            "MAME XML must use the unique smbneo software identity"
        )
    if software.findtext("description") != "Super Mario Bros. Neo":
        raise ValueError("MAME XML must use the Super Mario Bros. Neo title")

    parts = software.findall("./part")
    if len(parts) != 1:
        raise ValueError("MAME XML must contain exactly one cartridge part")
    part = parts[0]
    if part.get("name") != "cart" or part.get("interface") != "neo_cart":
        raise ValueError("MAME XML must use the Neo Geo cartridge interface")

    actual_parts = []
    rom_elements = []
    dataareas = part.findall("./dataarea")
    if [area.get("name") for area in dataareas] != list(
        mame_software.DATA_AREA_SIZES
    ):
        raise ValueError("MAME XML has missing, duplicate, or reordered data areas")

    for dataarea in dataareas:
        area_name = dataarea.get("name")
        expected_area_size = mame_software.DATA_AREA_SIZES.get(area_name)
        if expected_area_size is None:
            raise ValueError(f"MAME XML has unexpected data area {area_name!r}")
        if int(dataarea.get("size", "0"), 0) != expected_area_size:
            raise ValueError(f"MAME XML has the wrong {area_name} area size")
        if area_name == "maincpu":
            if (
                dataarea.get("width") != "16"
                or dataarea.get("endianness") != "big"
            ):
                raise ValueError(
                    "MAME XML maincpu must use 16-bit big-endian loading"
                )
        elif (
            dataarea.get("width") is not None
            or dataarea.get("endianness") is not None
        ):
            raise ValueError(
                f"MAME XML {area_name} must use its native byte loading"
            )
        for rom in dataarea.findall("rom"):
            actual_parts.append(
                (
                    area_name,
                    rom.get("name"),
                    int(rom.get("size", "0"), 0),
                    int(rom.get("offset", "0"), 0),
                    rom.get("loadflag"),
                )
            )
            rom_elements.append(rom)

    if actual_parts != list(mame_software.ROM_PARTS):
        raise ValueError(
            "MAME XML ROM names, sizes, offsets, or loading semantics differ "
            "from the canonical full-size cartridge"
        )

    for rom in rom_elements:
        filename = rom.get("name")
        if filename is None:
            raise ValueError("MAME XML ROM entry is missing its filename")
        native_path = _artifact_path(build_dir, filename)
        expected_crc, expected_sha1 = mame_software.file_hashes(native_path)
        if rom.get("crc") != expected_crc or rom.get("sha1") != expected_sha1:
            raise ValueError(
                f"MAME XML hashes for {filename} do not match the native region"
            )


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
        else:
            try:
                _validate_hardware_archive(build_dir, cart_path)
            except ValueError as error:
                issues.append(
                    f"{build_label} {archive.label} is invalid: {error}"
                )

    gngeo_data_path = _artifact_path(build_dir, GNGEO_DATA)
    if not gngeo_data_path.is_file():
        issues.append(
            f"{build_label} is missing {GNGEO_DATA_LABEL}: {gngeo_data_path}"
        )
    elif gngeo_data_path.stat().st_size == 0:
        issues.append(
            f"{build_label} {GNGEO_DATA_LABEL} is empty: {gngeo_data_path}"
        )
    else:
        try:
            check_gngeo_driver.validate_archive(
                gngeo_data_path,
                build_dir / "rom",
            )
        except check_gngeo_driver.DriverError as error:
            issues.append(
                f"{build_label} {GNGEO_DATA_LABEL} is invalid: {error}"
            )

    software_list_path = build_dir / MAME_SOFTWARE_LIST
    if not software_list_path.is_file():
        issues.append(
            f"{build_label} is missing {MAME_SOFTWARE_LIST_LABEL}: "
            f"{software_list_path}"
        )
    elif software_list_path.stat().st_size == 0:
        issues.append(
            f"{build_label} {MAME_SOFTWARE_LIST_LABEL} is empty: "
            f"{software_list_path}"
        )
    else:
        try:
            _validate_mame_software_list(build_dir, software_list_path)
        except ValueError as error:
            issues.append(
                f"{build_label} {MAME_SOFTWARE_LIST_LABEL} is invalid: {error}"
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
        "compat-cart",
        "mame-cart",
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
    """Build twice and compare canonical, compatibility, GnGeo, and MAME output."""

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

        result, issue = _compare_artifact(
            GNGEO_DATA_LABEL,
            _artifact_path(first_build, GNGEO_DATA),
            _artifact_path(second_build, GNGEO_DATA),
        )
        if result is not None:
            results.append(result)
        if issue is not None:
            issues.append(issue)

        result, issue = _compare_artifact(
            MAME_SOFTWARE_LIST_LABEL,
            first_build / MAME_SOFTWARE_LIST,
            second_build / MAME_SOFTWARE_LIST,
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

    print(
        "Cartridges, GnGeo driver data, and MAME software list are reproducible; "
        "both isolated builds match exactly:"
    )
    for result in results:
        print(
            f"  {result.label}: {result.size} bytes, "
            f"SHA-256 {result.sha256}"
        )
    print("The working platform/neogeo/build directory was not modified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
