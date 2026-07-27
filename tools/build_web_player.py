#!/usr/bin/env python3
"""Build the ROM-free SMBNeo browser player for GitHub Pages."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


PROJECT_NAME = "SMBNeo"
PRODUCT_SHORTNAME = "smbneo"
PRODUCT_TITLE = "Super Mario Bros. Neo"
FBNEO_DRIVER = "puzzledp"
EXPECTED_NES_SHA1 = "ea343f4e445a9050d4b4fbac2c77d0693b1d0922"
TITLE_SYMBOL = "neogeo_title_screen_data"
TITLE_BYTES = 0x013A
FIXED_ZIP_TIME = (2026, 1, 1, 0, 0, 0)

TEMPLATE_ENTRIES = {
    "smbneo-p1.p1": 0x100000,
    "smbneo-web-p1.p1": 0x100000,
    "smbneo-m1.m1": 0x020000,
    "smbneo-v1.v1": 0x080000,
}

BIOS_ENTRIES = {
    "sp-s3.sp1": (
        ("sp-s3.sp1", "sp-s2.sp1", "neo-epo.bin", "aes-bios.bin"),
        0x91B64BE3,
    ),
    "sm1.sm1": (("sm1.sm1",), 0x94416D67),
    "sfix.sfix": (("sfix.sfix",), 0xC2EA0CFD),
    "000-lo.lo": (("000-lo.lo",), 0x5A86CFF2),
}


class BuildError(RuntimeError):
    """A browser-player input or generated artifact is invalid."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def crc32(data: bytes | bytearray) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def force_crc32(data: bytes, desired: int, patch_offset: int) -> bytes:
    """Adjust four padding bytes so the complete blob has the requested CRC32."""

    if patch_offset < 0 or patch_offset + 4 > len(data):
        raise BuildError(
            f"invalid CRC patch offset {patch_offset} for {len(data)} bytes"
        )

    base = bytearray(data)
    base[patch_offset : patch_offset + 4] = b"\0\0\0\0"
    base_crc = crc32(base)
    delta = desired ^ base_crc

    columns: list[int] = []
    probe = bytearray(base)
    for bit in range(32):
        probe[patch_offset : patch_offset + 4] = b"\0\0\0\0"
        probe[patch_offset + bit // 8] = 1 << (bit % 8)
        columns.append(crc32(probe) ^ base_crc)

    rows: list[tuple[int, int]] = []
    for bit in range(32):
        mask = 0
        for column, value in enumerate(columns):
            if (value >> bit) & 1:
                mask |= 1 << column
        rows.append((mask, (delta >> bit) & 1))

    rank = 0
    for column in range(32):
        pivot = next(
            (
                row
                for row in range(rank, 32)
                if (rows[row][0] >> column) & 1
            ),
            None,
        )
        if pivot is None:
            continue
        rows[rank], rows[pivot] = rows[pivot], rows[rank]
        pivot_mask, pivot_rhs = rows[rank]
        for row in range(32):
            if row != rank and ((rows[row][0] >> column) & 1):
                rows[row] = (
                    rows[row][0] ^ pivot_mask,
                    rows[row][1] ^ pivot_rhs,
                )
        rank += 1

    if rank != 32:
        raise BuildError("CRC patch matrix is singular")

    patch_value = 0
    for mask, rhs in rows:
        if mask and rhs:
            patch_value |= mask & -mask

    patched = bytearray(base)
    patched[patch_offset : patch_offset + 4] = patch_value.to_bytes(4, "little")
    actual = crc32(patched)
    if actual != desired:
        raise BuildError(
            f"CRC patch failed: wanted {desired:08x}, got {actual:08x}"
        )
    return bytes(patched)


def find_padding_patch_offset(data: bytes, label: str) -> tuple[int, int, int]:
    """Find a zero/FF run so CRC correction never replaces live BIOS data."""

    candidates: list[tuple[int, int, int]] = []
    for value in (0x00, 0xFF):
        pattern = re.compile(bytes((value,)) + b"{8,}")
        for match in pattern.finditer(data):
            candidates.append(
                (match.end() - match.start(), match.start(), value)
            )
    if not candidates:
        raise BuildError(f"{label} has no safe zero/FF run for CRC correction")
    length, start, value = max(candidates, key=lambda item: (item[0], item[1]))
    return start + length - 4, value, length


def patch_for_fbneo(
    data: bytes,
    desired_crc: int,
    label: str,
) -> tuple[bytes, dict[str, int | str | None]]:
    original_crc = crc32(data)
    if original_crc == desired_crc:
        return data, {
            "original_crc32": f"{original_crc:08x}",
            "target_crc32": f"{desired_crc:08x}",
            "patch_offset": None,
            "padding_byte": None,
            "padding_run": 0,
        }

    patch_offset, padding_byte, padding_run = find_padding_patch_offset(
        data,
        label,
    )
    patched = force_crc32(data, desired_crc, patch_offset)
    return patched, {
        "original_crc32": f"{original_crc:08x}",
        "target_crc32": f"{desired_crc:08x}",
        "patch_offset": patch_offset,
        "padding_byte": f"{padding_byte:02x}",
        "padding_run": padding_run,
    }


def build_bios_entries(
    source_zip: Path,
) -> tuple[dict[str, bytes], list[dict[str, object]]]:
    output: dict[str, bytes] = {}
    records: list[dict[str, object]] = []
    with zipfile.ZipFile(source_zip) as archive:
        available = set(archive.namelist())
        for target, (aliases, target_crc) in BIOS_ENTRIES.items():
            source_name = next(
                (name for name in aliases if name in available),
                None,
            )
            if source_name is None:
                raise BuildError(
                    f"NullBIOS entry is missing: {' or '.join(aliases)}"
                )
            data = archive.read(source_name)
            if len(data) > 0x20000:
                raise BuildError(
                    f"{source_name} is unexpectedly large: {len(data)} bytes"
                )
            data += b"\0" * (0x20000 - len(data))
            patched, correction = patch_for_fbneo(data, target_crc, target)
            output[target] = patched
            records.append(
                {
                    "name": target,
                    "size": len(patched),
                    "sha256": sha256_bytes(patched),
                    "source": source_name,
                    **correction,
                }
            )
    return output, records


def write_zip(path: Path, entries: dict[str, bytes]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as archive:
        for name in sorted(entries):
            info = zipfile.ZipInfo(name, FIXED_ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(
                info,
                entries[name],
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9,
            )


def verify_zip(
    path: Path,
    expected: dict[str, tuple[int, int | None]],
) -> None:
    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        if names != set(expected):
            missing = set(expected) - names
            extra = names - set(expected)
            details = []
            if missing:
                details.append(f"missing {', '.join(sorted(missing))}")
            if extra:
                details.append(f"unexpected {', '.join(sorted(extra))}")
            raise BuildError(f"{path.name}: {'; '.join(details)}")
        for name, (size, expected_crc) in expected.items():
            info = archive.getinfo(name)
            if info.file_size != size:
                raise BuildError(
                    f"{path.name}:{name} is {info.file_size}, expected {size}"
                )
            if expected_crc is not None and info.CRC != expected_crc:
                raise BuildError(
                    f"{path.name}:{name} CRC {info.CRC:08x}, "
                    f"expected {expected_crc:08x}"
                )


def find_title_offset(elf: Path, nm: str) -> int:
    try:
        result = subprocess.run(
            [nm, "-a", str(elf)],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise BuildError(f"could not inspect {elf.name} with {nm}: {error}") from error

    pattern = re.compile(
        rf"^([0-9a-fA-F]+)\s+\S\s+{re.escape(TITLE_SYMBOL)}$",
        re.MULTILINE,
    )
    matches = pattern.findall(result.stdout)
    if len(matches) != 1:
        raise BuildError(
            f"expected one {TITLE_SYMBOL} symbol in {elf.name}, found {len(matches)}"
        )
    offset = int(matches[0], 16)
    if (offset & 1) != 0:
        raise BuildError(f"{TITLE_SYMBOL} has odd P-ROM offset {offset:#x}")
    return offset


def template_records(entries: dict[str, bytes]) -> list[dict[str, object]]:
    return [
        {
            "name": name,
            "size": len(data),
            "sha256": sha256_bytes(data),
        }
        for name, data in sorted(entries.items())
    ]


def copy_templates(
    templates: Path,
    output: Path,
    title_image: Path,
    build_id: str,
) -> None:
    if not templates.is_dir():
        raise BuildError(f"web templates not found: {templates}")
    if not title_image.is_file():
        raise BuildError(f"title image not found: {title_image}")

    if output.exists():
        shutil.rmtree(output)
    shutil.copytree(templates, output)
    shutil.copy2(title_image, output / "title.png")

    index_path = output / "index.html"
    index = index_path.read_text(encoding="utf-8")
    index = index.replace("__BUILD_ID__", build_id)
    if "__BUILD_ID__" in index:
        raise BuildError("unresolved web build ID")
    index_path.write_text(index, encoding="utf-8")
    (output / ".nojekyll").write_text("", encoding="ascii")


def build(args: argparse.Namespace) -> None:
    native_elf = args.native_elf.resolve()
    native_prom = args.native_prom.resolve()
    web_elf = args.web_elf.resolve()
    web_prom = args.web_prom.resolve()
    mrom = args.mrom.resolve()
    vrom = args.vrom.resolve()
    source_bios = args.bios.resolve()
    templates = args.templates.resolve()
    title_image = args.title_image.resolve()
    output = args.output.resolve()

    for path, label in (
        (native_elf, "native ROM-free ELF"),
        (native_prom, "native template P-ROM"),
        (web_elf, "FBNeo ROM-free ELF"),
        (web_prom, "FBNeo template P-ROM"),
        (mrom, "template M-ROM"),
        (vrom, "template V-ROM"),
        (source_bios, "NullBIOS"),
    ):
        if not path.is_file():
            raise BuildError(f"{label} not found: {path}")

    template_entries = {
        "smbneo-p1.p1": native_prom.read_bytes(),
        "smbneo-web-p1.p1": web_prom.read_bytes(),
        "smbneo-m1.m1": mrom.read_bytes(),
        "smbneo-v1.v1": vrom.read_bytes(),
    }
    for name, expected_size in TEMPLATE_ENTRIES.items():
        actual_size = len(template_entries[name])
        if actual_size != expected_size:
            raise BuildError(
                f"{name} is {actual_size} bytes, expected {expected_size}"
            )

    title_offsets = {
        "native": find_title_offset(native_elf, args.nm),
        "web": find_title_offset(web_elf, args.nm),
    }
    for profile, entry_name in (
        ("native", "smbneo-p1.p1"),
        ("web", "smbneo-web-p1.p1"),
    ):
        title_offset = title_offsets[profile]
        if title_offset + TITLE_BYTES > len(template_entries[entry_name]):
            raise BuildError(
                f"{profile} title range {title_offset:#x}.."
                f"{title_offset + TITLE_BYTES:#x} falls outside the P-ROM"
            )
        title_region = template_entries[entry_name][
            title_offset : title_offset + TITLE_BYTES
        ]
        if any(title_region):
            raise BuildError(
                f"{profile} ROM-free P-ROM title placeholder is not zero-filled"
            )

    version_digest = hashlib.sha256()
    for name, data in sorted(template_entries.items()):
        version_digest.update(name.encode("utf-8"))
        version_digest.update(data)
    version_digest.update(source_bios.read_bytes())
    version_digest.update(title_image.read_bytes())
    for path in sorted(templates.rglob("*")):
        if path.is_file():
            version_digest.update(path.relative_to(templates).as_posix().encode("utf-8"))
            version_digest.update(path.read_bytes())
    build_id = version_digest.hexdigest()[:12]

    copy_templates(templates, output, title_image, build_id)
    assets = output / "assets"
    template_zip = assets / "smbneo-template.zip"
    # EmulatorJS 4.2.3 writes this name directly into its virtual root when
    # EJS_dontExtractBIOS is enabled. Keep it slash-free for that code path.
    bios_zip = output / "neogeo.zip"

    write_zip(template_zip, template_entries)
    verify_zip(
        template_zip,
        {
            name: (size, None)
            for name, size in TEMPLATE_ENTRIES.items()
        },
    )

    bios_entries, bios_records = build_bios_entries(source_bios)
    write_zip(bios_zip, bios_entries)
    verify_zip(
        bios_zip,
        {
            name: (0x20000, target_crc)
            for name, (_, target_crc) in BIOS_ENTRIES.items()
        },
    )

    manifest = {
        "project": PROJECT_NAME,
        "product": {
            "shortname": PRODUCT_SHORTNAME,
            "title": PRODUCT_TITLE,
            "canonical_archive": "smbneo.zip",
        },
        "profile": "ROM-free SMBNeo browser player",
        "fbneo_driver": FBNEO_DRIVER,
        "downloads": {
            "canonical": {
                "filename": "smbneo.zip",
                "shortname": "smbneo",
                "layout": "full hardware-native P/S/M/V/C",
            },
            "neosd": {
                "filename": "smbneo.neo",
                "shortname": "smbneo",
                "layout": "NeoSD v1 single-file native cartridge",
            },
            "compatibility": {
                "filename": "puzzledp.zip",
                "shortname": "puzzledp",
                "layout": "fixed-database donor compatibility",
            },
        },
        "build_id": build_id,
        "source_commit": os.environ.get("GITHUB_SHA"),
        "expected_nes_sha1": EXPECTED_NES_SHA1,
        "title_patch_offsets": title_offsets,
        "title_patch_bytes": TITLE_BYTES,
        "template": {
            "path": template_zip.relative_to(output).as_posix(),
            "bytes": template_zip.stat().st_size,
            "sha256": sha256_file(template_zip),
            "entries": template_records(template_entries),
        },
        "bios": {
            "path": bios_zip.relative_to(output).as_posix(),
            "provider": "ngdevkit NullBIOS",
            "bytes": bios_zip.stat().st_size,
            "sha256": sha256_file(bios_zip),
            "entries": bios_records,
        },
        "runtime": {
            "emulatorjs": "4.2.3",
            "archive_library": "fflate 0.8.2",
        },
        "privacy": {
            "game_image_included": False,
            "generated_graphics_included": False,
            "generated_cartridge_included": False,
        },
    }
    (output / "build-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    print(f"Built {PROJECT_NAME} browser player: {output}")
    print(f"Template: {template_zip.stat().st_size} bytes")
    print(f"NullBIOS: {bios_zip.stat().st_size} bytes")
    print(
        "Title patch offsets: "
        f"native={title_offsets['native']:#x}, web={title_offsets['web']:#x}"
    )
    print(f"Build ID: {build_id}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-elf", type=Path, required=True)
    parser.add_argument("--native-prom", type=Path, required=True)
    parser.add_argument("--web-elf", type=Path, required=True)
    parser.add_argument("--web-prom", type=Path, required=True)
    parser.add_argument("--mrom", type=Path, required=True)
    parser.add_argument("--vrom", type=Path, required=True)
    parser.add_argument("--bios", type=Path, required=True)
    parser.add_argument("--nm", default="m68k-neogeo-elf-nm")
    parser.add_argument("--output", type=Path, default=Path("dist/pages"))
    parser.add_argument("--templates", type=Path, default=Path("web"))
    parser.add_argument(
        "--title-image",
        type=Path,
        default=Path("docs/screenshots/title-screen.png"),
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        build(parse_args(sys.argv[1:] if argv is None else argv))
    except (
        BuildError,
        OSError,
        subprocess.SubprocessError,
        zipfile.BadZipFile,
    ) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
