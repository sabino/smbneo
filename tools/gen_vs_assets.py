#!/usr/bin/env python3
"""Generate private VS Neo Geo graphics and C-resident banked data."""
import argparse
import hashlib
import json
from pathlib import Path
from vs_rom import load_vs_rom
from gen_neogeo_assets import decode_nes_tile, expand_2x, orient_tile, encode_crom_tile, encode_srom_tile
from gen_neogeo_palette import encode_neogeo_color


def generate(rom, output: Path) -> None:
    if rom.palette is None:
        raise ValueError('VS Neo Geo assets require rp2c04-0004.pal from the user set')
    c1, c2 = bytearray(0x200000), bytearray(0x200000)
    s1 = bytearray(0x20000)
    for tile in range(1024):
        pixels = decode_nes_tile(rom.chr[(tile // 512) * 8192:][:8192], tile % 512)
        for orientation in range(4):
            planes = encode_crom_tile(expand_2x(orient_tile(pixels, orientation)))
            offset = (257 + orientation * 1024 + tile) * 64
            c1[offset:offset + 64], c2[offset:offset + 64] = planes
        s1[(1 + tile) * 32:(2 + tile) * 32] = encode_srom_tile(pixels)
    s1[1026 * 32:1027 * 32] = encode_srom_tile(bytes([1] * 64))
    words = []
    for i in range(64):
        rgb = tuple((v << 5) | (v << 2) | (v >> 1) for v in rom.palette[i * 3:i * 3 + 3])
        words.append(encode_neogeo_color(rgb))
    source = ['/* Private generated VS data; never commit. */', '#include <stdint.h>']
    for name, data in (('vs_prg', rom.prg), ('vs_chr', rom.chr)):
        source.append(f'_Alignas(2) const uint8_t {name}[{len(data)}] = {{')
        source.extend(','.join(f'0x{v:02x}' for v in data[i:i + 16]) + ',' for i in range(0, len(data), 16))
        source.append('};')
    source += ['const uint16_t vs_palette_neogeo[64] = {', ','.join(f'0x{v:04x}' for v in words), '};']
    files = {'vssmbneo-c1.c1': bytes(c1), 'vssmbneo-c2.c2': bytes(c2),
             'vssmbneo-s1.s1': bytes(s1), 'vs_data.c': ('\n'.join(source) + '\n').encode()}
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (output / name).write_bytes(data)
    (output / 'vs-neogeo-assets.json').write_text(json.dumps({
        'shortname': 'vssmbneo', 'title': 'VS. Super Mario Bros. Neo',
        'tiles_per_orientation': 1024, 'orientations': 4,
        'outputs': {name: {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                    for name, data in files.items()}}, indent=2) + '\n')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    a = p.parse_args()
    generate(load_vs_rom(a.input), a.output_dir)
