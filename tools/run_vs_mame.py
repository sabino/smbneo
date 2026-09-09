#!/usr/bin/env python3
"""Run the local VS cartridge regression with symbol-resolved diagnostics."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


_DEBUG_MARKER = re.compile(r"^VS frame=(?P<display>\d+).*?frames=(?P<game>[0-9a-fA-F]+)\s*$")


def gameplay_metrics(log: str, label: str = "vs", start_tick: int = 600,
                     end_tick: int = 1200) -> dict:
    """Extract the bounded gameplay interval from the Lua debug markers."""
    if end_tick <= start_tick:
        raise ValueError("end game frame must be after start game frame")
    markers = []
    for line_number, line in enumerate(log.splitlines(), 1):
        match = _DEBUG_MARKER.fullmatch(line)
        if match:
            markers.append((int(match["game"], 16), int(match["display"]), line_number))
    if not markers:
        raise ValueError("MAME log has no VS frame markers")
    for previous, current in zip(markers, markers[1:]):
        if current[0] <= previous[0]:
            raise ValueError(f"game-frame markers out of order or duplicated at line {current[2]}")
    selected = {tick: [] for tick in (start_tick, end_tick)}
    for tick, display, line_number in markers:
        if tick in selected:
            selected[tick].append((display, line_number))
    for tick, values in selected.items():
        if not values:
            raise ValueError(f"MAME log is missing game-frame marker {tick}")
        if len(values) != 1:
            raise ValueError(f"MAME log has duplicate game-frame marker {tick}")
    start_display = selected[start_tick][0][0]
    end_display = selected[end_tick][0][0]
    display_frames = end_display - start_display
    if display_frames < 0:
        raise ValueError("display-frame counter moved backwards")
    ticks = end_tick - start_tick
    return {
        "label": label,
        "ticks": ticks,
        "display_frames": display_frames,
        "display_frames_per_tick": display_frames / ticks,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--bios-dir', type=Path, required=True)
    p.add_argument('--system', choices=('ng_mv1', 'aes'), default='ng_mv1')
    p.add_argument('--real-coin', action='store_true', help='exercise MVS Coin 1 rather than AES shortcut')
    p.add_argument('--game-frames', type=int, choices=(600, 1200), default=600,
                   help='1200 also exercises right/run/jump after startup')
    p.add_argument('--label', default='vs', help='benchmark label (used with 1200 frames)')
    p.add_argument('--metrics-json', type=Path,
                   help='write the 600-to-1200 gameplay metric as JSON')
    p.add_argument('--reference-frame', type=int, default=0,
                   help='no-input RAM comparison at this game frame, otherwise exercise coin/start')
    a = p.parse_args(); build = a.build.resolve()
    if a.metrics_json and a.game_frames != 1200:
        p.error('--metrics-json requires --game-frames 1200')
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
    if a.game_frames == 1200:
        try:
            metrics = gameplay_metrics((root / 'run.log').read_text(), a.label)
        except ValueError as exc:
            raise SystemExit(f'VS MAME benchmark did not reach both gameplay markers: {exc}')
        print('VS benchmark: ' + ' '.join(
            f'{key}={value:.6f}' if isinstance(value, float) else f'{key}={value}'
            for key, value in metrics.items()))
        if a.metrics_json:
            a.metrics_json.parent.mkdir(parents=True, exist_ok=True)
            a.metrics_json.write_text(json.dumps(metrics, indent=2) + '\n')


if __name__ == '__main__': main()
