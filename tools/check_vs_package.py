#!/usr/bin/env python3
"""Validate every representation of the canonical VS Neo Geo cartridge."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import stat
import sys
import xml.etree.ElementTree as ET
import zipfile
import zlib

import build_neosd
import check_gngeo_driver
import gen_mame_neogeo_software as mame_software


SHORTNAME = "vssmbneo"
TITLE = "VS. Super Mario Bros. Neo"
NGH = 0x2027
YEAR = 2026
MANUFACTURER = "Community port"
PACKAGE_FORMAT = "full-hardware-native"
MANIFEST_SCHEMA_VERSION = 1
ZIP_TIMESTAMP = (2026, 1, 1, 0, 0, 0)
ZIP_EXTERNAL_ATTR = (stat.S_IFREG | 0o644) << 16


@dataclass(frozen=True)
class Region:
    key: str
    label: str
    filename: str
    size: int


REGIONS = (
    Region("p", "P", "vssmbneo-p1.p1", 0x100000),
    Region("s", "S", "vssmbneo-s1.s1", 0x020000),
    Region("m", "M", "vssmbneo-m1.m1", 0x020000),
    Region("v1", "V", "vssmbneo-v1.v1", 0x080000),
    Region("c1", "C1", "vssmbneo-c1.c1", 0x200000),
    Region("c2", "C2", "vssmbneo-c2.c2", 0x200000),
)
REGION_NAMES = tuple(region.filename for region in REGIONS)
CANONICAL_ZIP = "vssmbneo.zip"
NEOSD_IMAGE = "vssmbneo.neo"
GNGEO_DATA = "gngeo_data.zip"
MANIFEST = "vs-cart-manifest.json"
MAME_XML = Path("mame") / "hash" / "neogeo.xml"


class PackageError(RuntimeError):
    """A generated VS artifact does not match the canonical cartridge."""


def file_metadata(data: bytes) -> dict[str, int | str]:
    return {
        "size": len(data),
        "crc32": f"{zlib.crc32(data) & 0xFFFFFFFF:08x}",
        "sha1": hashlib.sha1(data).hexdigest(),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def build_manifest(regions: dict[str, bytes]) -> dict[str, object]:
    if tuple(regions) != REGION_NAMES:
        raise PackageError(
            "manifest input must contain the six canonical regions in order"
        )
    return {
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "format": PACKAGE_FORMAT,
        "shortname": SHORTNAME,
        "title": TITLE,
        "ngh": f"0x{NGH:04x}",
        "files": {
            name: file_metadata(data) for name, data in regions.items()
        },
    }


def _read(path: Path, label: str) -> bytes:
    try:
        return path.read_bytes()
    except OSError as error:
        raise PackageError(f"cannot read {label} {path}: {error}") from error


def _load_regions(rom_dir: Path) -> dict[str, bytes]:
    output = {}
    for region in REGIONS:
        data = _read(rom_dir / region.filename, f"{region.label} ROM")
        if len(data) != region.size:
            raise PackageError(
                f"{region.filename} has {len(data)} bytes; "
                f"expected {region.size}"
            )
        output[region.filename] = data
    return output


def _validate_zip_metadata(info: zipfile.ZipInfo, archive: Path) -> None:
    if info.date_time != ZIP_TIMESTAMP:
        raise PackageError(
            f"{info.filename} in {archive} has non-deterministic timestamp "
            f"{info.date_time!r}"
        )
    if info.create_system != 3 or info.external_attr != ZIP_EXTERNAL_ATTR:
        raise PackageError(
            f"{info.filename} in {archive} has non-canonical file metadata"
        )
    if info.compress_type != zipfile.ZIP_DEFLATED:
        raise PackageError(
            f"{info.filename} in {archive} is not deterministically deflated"
        )
    if info.extra or info.comment:
        raise PackageError(
            f"{info.filename} in {archive} has unexpected ZIP metadata"
        )


def _validate_canonical_zip(
    archive_path: Path,
    regions: dict[str, bytes],
) -> None:
    try:
        with zipfile.ZipFile(archive_path) as archive:
            infos = archive.infolist()
            names = [info.filename for info in infos]
            if names != list(REGION_NAMES):
                raise PackageError(
                    f"{archive_path} members are {names!r}; "
                    f"expected {list(REGION_NAMES)!r}"
                )
            if archive.comment:
                raise PackageError(f"{archive_path} has an unexpected ZIP comment")
            for info in infos:
                _validate_zip_metadata(info, archive_path)
                expected = regions[info.filename]
                if info.file_size != len(expected):
                    raise PackageError(
                        f"{info.filename} has the wrong uncompressed ZIP size"
                    )
                if info.CRC != zlib.crc32(expected) & 0xFFFFFFFF:
                    raise PackageError(f"{info.filename} has the wrong ZIP CRC")
                if archive.read(info) != expected:
                    raise PackageError(
                        f"{info.filename} in {archive_path} differs from its "
                        "canonical loose ROM"
                    )
    except PackageError:
        raise
    except (OSError, KeyError, zipfile.BadZipFile, zipfile.LargeZipFile) as error:
        raise PackageError(f"cannot read {archive_path}: {error}") from error


def _validate_manifest(path: Path, regions: dict[str, bytes]) -> None:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PackageError(f"cannot read {path}: {error}") from error
    expected = build_manifest(regions)
    if manifest != expected:
        raise PackageError(
            f"{path} does not exactly describe the canonical cartridge"
        )
    if "experimental" in manifest:
        raise PackageError(f"{path} contains the retired experimental flag")


def _expected_mame_parts() -> list[tuple[str, str, int, int, str | None]]:
    return [
        (
            area,
            filename.replace("smbneo-", f"{SHORTNAME}-", 1),
            size,
            offset,
            loadflag,
        )
        for area, filename, size, offset, loadflag in mame_software.ROM_PARTS
    ]


def _validate_mame_xml(path: Path, regions: dict[str, bytes]) -> None:
    try:
        xml_text = path.read_text(encoding="utf-8")
        root = ET.fromstring(xml_text)
    except (OSError, UnicodeError, ET.ParseError) as error:
        raise PackageError(f"cannot parse {path}: {error}") from error
    if '<!DOCTYPE softwarelist SYSTEM "softwarelist.dtd">' not in xml_text:
        raise PackageError(f"{path} is missing MAME's software-list doctype")
    if root.tag != "softwarelist" or root.attrib != {
        "name": "neogeo",
        "description": "Local Neo Geo cartridge tests",
    }:
        raise PackageError(f"{path} is not the canonical Neo Geo software list")

    software_entries = root.findall("./software")
    if len(software_entries) != 1:
        raise PackageError(f"{path} must contain exactly one software entry")
    software = software_entries[0]
    if software.attrib != {"name": SHORTNAME}:
        raise PackageError(f"{path} does not use the {SHORTNAME!r} identity")
    if software.findtext("description") != TITLE:
        raise PackageError(f"{path} has the wrong visible title")
    if software.findtext("year") != str(YEAR):
        raise PackageError(f"{path} has the wrong publication year")
    if software.findtext("publisher") != MANUFACTURER:
        raise PackageError(f"{path} has the wrong publisher")
    shared_features = software.findall("./sharedfeat")
    if len(shared_features) != 1 or shared_features[0].attrib != {
        "name": "compatibility",
        "value": "MVS,AES",
    }:
        raise PackageError(f"{path} has the wrong MVS/AES compatibility metadata")

    parts = software.findall("./part")
    if len(parts) != 1 or parts[0].attrib != {
        "name": "cart",
        "interface": "neo_cart",
    }:
        raise PackageError(f"{path} has the wrong Neo Geo cartridge interface")
    dataareas = parts[0].findall("./dataarea")
    if [area.get("name") for area in dataareas] != list(
        mame_software.DATA_AREA_SIZES
    ):
        raise PackageError(f"{path} has missing, duplicate, or reordered data areas")

    actual_parts = []
    rom_elements = []
    for area in dataareas:
        area_name = area.get("name")
        expected_size = mame_software.DATA_AREA_SIZES[area_name]
        expected_attributes = {
            "name": area_name,
            "size": f"0x{expected_size:06x}",
        }
        if area_name == "maincpu":
            expected_attributes.update({"width": "16", "endianness": "big"})
        if area.attrib != expected_attributes:
            raise PackageError(f"{path} has incorrect {area_name} load geometry")
        for rom in area.findall("./rom"):
            try:
                part = (
                    area_name,
                    rom.attrib["name"],
                    int(rom.attrib["size"], 0),
                    int(rom.attrib["offset"], 0),
                    rom.get("loadflag"),
                )
            except (KeyError, ValueError) as error:
                raise PackageError(f"{path} contains an invalid ROM entry") from error
            actual_parts.append(part)
            rom_elements.append(rom)

    if actual_parts != _expected_mame_parts():
        raise PackageError(
            f"{path} ROM names, sizes, offsets, or P/C loading semantics "
            "do not match the full cartridge"
        )
    for rom in rom_elements:
        filename = rom.attrib["name"]
        metadata = file_metadata(regions[filename])
        expected_attributes = {
            "name": filename,
            "offset": rom.attrib["offset"],
            "size": rom.attrib["size"],
            "crc": metadata["crc32"],
            "sha1": metadata["sha1"],
        }
        if rom.get("loadflag") is not None:
            expected_attributes["loadflag"] = rom.attrib["loadflag"]
        if rom.attrib != expected_attributes:
            raise PackageError(f"{path} hashes or attributes differ for {filename}")


def _validate_neosd(path: Path, regions: dict[str, bytes]) -> None:
    native_regions = {
        region.key: regions[region.filename] for region in REGIONS
    }
    try:
        header = build_neosd.validate_image(
            _read(path, "NeoSD image"),
            native_regions,
            name=TITLE,
            manufacturer=MANUFACTURER,
            year=YEAR,
            ngh=NGH,
        )
    except build_neosd.NeoSdError as error:
        raise PackageError(f"invalid {path}: {error}") from error
    expected_sizes = (
        len(native_regions["p"]),
        len(native_regions["s"]),
        len(native_regions["m"]),
        len(native_regions["v1"]),
        0,
        len(native_regions["c1"]) + len(native_regions["c2"]),
    )
    actual_sizes = (
        header.p_size,
        header.s_size,
        header.m_size,
        header.v1_size,
        header.v2_size,
        header.c_size,
    )
    if actual_sizes != expected_sizes:
        raise PackageError(f"{path} does not preserve the full native layout")


def _validate_gngeo(path: Path, rom_dir: Path) -> None:
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            names = [info.filename for info in infos]
            if names != sorted(names) or len(names) != len(set(names)):
                raise PackageError(
                    f"{path} members are not unique and deterministically ordered"
                )
            if archive.comment:
                raise PackageError(f"{path} has an unexpected ZIP comment")
            for info in infos:
                _validate_zip_metadata(info, path)
    except PackageError:
        raise
    except (OSError, zipfile.BadZipFile, zipfile.LargeZipFile) as error:
        raise PackageError(f"cannot read {path}: {error}") from error
    try:
        check_gngeo_driver.validate_archive(
            path,
            rom_dir,
            shortname=SHORTNAME,
            title=TITLE,
        )
    except check_gngeo_driver.DriverError as error:
        raise PackageError(f"invalid custom GnGeo data: {error}") from error


def validate_package(build_dir: Path) -> dict[str, bytes]:
    """Validate loose regions and every generated canonical representation."""

    build_dir = build_dir.resolve()
    rom_dir = build_dir / "rom"
    regions = _load_regions(rom_dir)
    _validate_canonical_zip(rom_dir / CANONICAL_ZIP, regions)
    _validate_manifest(rom_dir / MANIFEST, regions)
    _validate_mame_xml(build_dir / MAME_XML, regions)
    _validate_neosd(rom_dir / NEOSD_IMAGE, regions)
    _validate_gngeo(rom_dir / GNGEO_DATA, rom_dir)
    return regions


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build",
        required=True,
        type=Path,
        help="VS build directory containing rom/ and mame/hash/",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        regions = validate_package(args.build)
    except PackageError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"Verified {SHORTNAME}: {len(regions)} full native ROM regions, "
        "canonical ZIP, MAME XML, NeoSD image, and GnGeo driver"
    )
    for filename, data in regions.items():
        metadata = file_metadata(data)
        print(
            f"  {filename}: {metadata['size']} bytes, "
            f"sha256 {metadata['sha256']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
