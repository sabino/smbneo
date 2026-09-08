#!/usr/bin/env python3
"""Package the local experimental VS cartridge without touching home outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile
from gen_mame_neogeo_software import write_software_list
from build_neosd import build_image, validate_image


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
    write_software_list(output, args.build / 'mame/hash/neogeo.xml', 'vssmbneo', 'VS. Super Mario Bros. Neo')
    names = ['vssmbneo-' + suffix for suffix in ('p1.p1', 's1.s1', 'm1.m1', 'v1.v1', 'c1.c1', 'c2.c2')]
    with zipfile.ZipFile(output / 'vssmbneo.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for name in names:
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, (output / name).read_bytes())
    regions = {region: (output / filename).read_bytes()
               for region, filename in zip(('p', 's', 'm', 'v1', 'c1', 'c2'), names)}
    neo = build_image(regions, name='VS. Super Mario Bros. Neo', ngh=0x2027)
    validate_image(neo, regions, name='VS. Super Mario Bros. Neo', ngh=0x2027)
    (output / 'vssmbneo.neo').write_bytes(neo)
    subprocess.run(['romtool.py', '-b', 'hash', '-f', 'gngeo',
                    '-p', str(prom), '-s', str(output / names[1]), '-m', str(output / names[2]),
                    '-v', str(output / names[3]), '-c', str(output / names[4]), str(output / names[5]),
                    '-n', 'vssmbneo', '-l', 'VS. Super Mario Bros. Neo',
                    '-x', 'gngeo.data=' + str(args.gngeo_data), '-o', str(output / 'gngeo_data.zip')], check=True)
    (output / 'vs-cart-manifest.json').write_text(json.dumps({
        'experimental': True, 'shortname': 'vssmbneo',
        'files': {name: {'size': (output / name).stat().st_size,
                         'sha256': hashlib.sha256((output / name).read_bytes()).hexdigest()} for name in names}}, indent=2) + '\n')
    print('Experimental VS cartridge:', output / 'vssmbneo.zip')


if __name__ == '__main__': main()
