#!/usr/bin/env python3
"""Run the local VS cartridge regression with symbol-resolved diagnostics."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--bios-dir', type=Path, required=True)
    p.add_argument('--system', choices=('ng_mv1', 'aes'), default='ng_mv1')
    p.add_argument('--real-coin', action='store_true', help='exercise MVS Coin 1 rather than AES shortcut')
    p.add_argument('--game-frames', type=int, choices=(600, 1200), default=600,
                   help='1200 also exercises right/run/jump after startup')
    p.add_argument('--reference-frame', type=int, default=0,
                   help='no-input RAM comparison at this game frame, otherwise exercise coin/start')
    a = p.parse_args(); build = a.build.resolve()
    env = os.environ.copy()
    env['VS_END_FRAME'] = str(a.game_frames)
    if a.real_coin:
        if a.system != 'ng_mv1': p.error('--real-coin requires MVS')
        env['VS_REAL_COIN'] = '1'
    symbols = subprocess.check_output(['m68k-neogeo-elf-nm', str(build / 'vssmbneo.elf')], text=True)
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) != 3: continue
        address, _, name = fields
        if name.startswith('vs_debug_'): env[name.upper()] = '0x' + address
        if name == 'machine': env['VS_MACHINE_ADDRESS'] = '0x' + address
    if a.reference_frame:
        env['VS_STOP_GAMEFRAME'] = str(a.reference_frame)
        env['VS_DUMP_PATH'] = str(build / f'neo{a.reference_frame}.ram')
    runs = build / 'mame' / a.system
    runs.mkdir(parents=True, exist_ok=True)
    # Isolate checkpoints so a previous successful run cannot mask a failure.
    root = Path(tempfile.mkdtemp(prefix='run-', dir=runs))
    for name in ('cfg', 'nvram', 'snap'): (root / name).mkdir(parents=True, exist_ok=True)
    script = Path(__file__).resolve().parents[1] / 'variants/vs/mame_test.lua'
    command = ['mame', a.system, 'vssmbneo', '-hashpath', str(build / 'mame/hash'),
               '-rompath', str(build / 'rom') + ';' + str(a.bios_dir.resolve()),
               '-video', 'none', '-sound', 'none', '-nothrottle', '-seconds_to_run', '120',
               '-skip_gameinfo', '-noplugins', '-autoboot_delay', '0', '-autoboot_script', str(script),
               '-cfg_directory', str(root / 'cfg'), '-nvram_directory', str(root / 'nvram'),
               '-snapshot_directory', str(root / 'snap')]
    # Hard wall-clock cap as well as emulator-time cap, including wedged exits.
    with (root / 'run.log').open('w') as log:
        result = subprocess.run(['timeout', '-k', '5s', '55s', *command], env=env, stdout=log, stderr=subprocess.STDOUT)
    print((root / 'run.log').read_text())
    expected = root / 'snap' / ('vs-neo-gameframe.png' if a.reference_frame else f'vs-neo-tick-{a.game_frames:04d}.png')
    if result.returncode or not expected.is_file() or 'VS validation passed' not in (root / 'run.log').read_text():
        raise SystemExit('VS MAME regression did not reach its checkpoint')
    print('Checkpoint:', expected)


if __name__ == '__main__': main()
