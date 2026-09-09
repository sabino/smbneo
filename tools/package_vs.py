#!/usr/bin/env python3
"""Package and validate the canonical VS Neo Geo cartridge outputs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import zipfile
from gen_mame_neogeo_software import write_software_list
from build_neosd import build_image, validate_image
from check_vs_package import (
    GNGEO_DATA,
    MANIFEST,
    MANUFACTURER,
    NEOSD_IMAGE,
    NGH,
    REGIONS,
    SHORTNAME,
    TITLE,
    ZIP_EXTERNAL_ATTR,
    ZIP_TIMESTAMP,
    build_manifest,
    validate_package,
)


def _zip_info(filename: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(filename, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = ZIP_EXTERNAL_ATTR
    return info


def write_deterministic_zip(
    output: Path,
    entries: list[tuple[str, bytes]],
) -> None:
    """Write fixed-order, fixed-metadata ZIP bytes."""

    names = [name for name, _ in entries]
    if len(names) != len(set(names)):
        raise ValueError(f"duplicate ZIP member in {output}")
    temporary = output.with_name(output.name + ".tmp")
    with zipfile.ZipFile(
        temporary,
        "w",
        zipfile.ZIP_DEFLATED,
        compresslevel=9,
    ) as archive:
        for name, data in entries:
            archive.writestr(_zip_info(name), data)
    temporary.replace(output)


def canonicalize_zip(path: Path) -> None:
    """Normalize romtool's GnGeo support archive for reproducible releases."""

    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError(f"duplicate ZIP member in {path}")
        entries = [(name, archive.read(name)) for name in sorted(names)]
    write_deterministic_zip(path, entries)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--sound', type=Path, required=True)
    p.add_argument('--samples', type=Path, required=True)
    p.add_argument('--gngeo-data', type=Path, required=True)
    args = p.parse_args()
    output = args.build / 'rom'; output.mkdir(parents=True, exist_ok=True)
    prom = output / 'vssmbneo-p1.p1'
    subprocess.run(['m68k-neogeo-elf-objcopy', '-O', 'binary', '-S', '-R', '.text2',
                    '--gap-fill', '0xff', '--pad-to', '1048576',
                    str(args.build / 'vssmbneo.elf'), str(prom)], check=True)
    data = prom.read_bytes()
    if len(data) != 1048576:
        raise ValueError('VS P-ROM exceeds native fixed 1 MiB layout')
    swapped = bytearray(data); swapped[::2], swapped[1::2] = data[1::2], data[::2]
    prom.write_bytes(swapped)
    for suffix in ('s1.s1', 'c1.c1', 'c2.c2'):
        shutil.copyfile(args.build / 'assets' / ('vssmbneo-' + suffix), output / ('vssmbneo-' + suffix))
    subprocess.run(['z80-neogeo-ihx-sdobjcopy', '-I', 'ihex', '-O', 'binary',
                    str(args.sound), str(output / 'vssmbneo-m1.m1'), '--pad-to', '131072'], check=True)
    shutil.copyfile(args.samples, output / 'vssmbneo-v1.v1')
    write_software_list(
        output,
        args.build / 'mame/hash/neogeo.xml',
        SHORTNAME,
        TITLE,
    )
    names = [region.filename for region in REGIONS]
    region_files = {name: (output / name).read_bytes() for name in names}
    write_deterministic_zip(
        output / f'{SHORTNAME}.zip',
        [(name, region_files[name]) for name in names],
    )
    regions = {region: (output / filename).read_bytes()
               for region, filename in zip(('p', 's', 'm', 'v1', 'c1', 'c2'), names)}
    neo = build_image(
        regions,
        name=TITLE,
        manufacturer=MANUFACTURER,
        ngh=NGH,
    )
    validate_image(
        neo,
        regions,
        name=TITLE,
        manufacturer=MANUFACTURER,
        ngh=NGH,
    )
    (output / NEOSD_IMAGE).write_bytes(neo)
    subprocess.run(['romtool.py', '-b', 'hash', '-f', 'gngeo',
                    '-p', str(prom), '-s', str(output / names[1]), '-m', str(output / names[2]),
                    '-v', str(output / names[3]), '-c', str(output / names[4]), str(output / names[5]),
                    '-n', SHORTNAME, '-l', TITLE,
                    '-x', 'gngeo.data=' + str(args.gngeo_data),
                    '-o', str(output / GNGEO_DATA)], check=True)
    canonicalize_zip(output / GNGEO_DATA)
    (output / MANIFEST).write_text(
        json.dumps(build_manifest(region_files), indent=2) + '\n',
        encoding='utf-8',
    )
    validate_package(args.build)
    print('Canonical VS cartridge:', output / f'{SHORTNAME}.zip')


if __name__ == '__main__': main()
