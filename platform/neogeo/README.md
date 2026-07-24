# Neo Geo target

This target statically compiles the translated SMB C core for the Neo Geo
MC68000 and replaces the desktop PPU/APU modules with:

- `ppu_direct.c`: 2 KiB nametable, 256-byte OAM, 32-byte palette, and PPU
  register behavior without full CHR/framebuffer storage. Cartridge builds
  retain only the 314-byte CHR read window used to construct the title menu.
- `video.c`: direct C-ROM/S-ROM, palette RAM, FIX-map, and SCB1-4 writes with
  two 161-sprite frame sets, generation caches, and next-VBlank live swaps.
- `apu_null.c`: a zero-allocation placeholder for a future YM2610 driver.
- `main.c`: Neo Geo input mapping and the 60 Hz game loop.

## Commands

From the repository root:

```bash
make -C platform/neogeo verify
make -C platform/neogeo cart SMB_ROM="/path/to/owned/smb.nes"
make -C platform/neogeo run SMB_ROM="/path/to/owned/smb.zip"
python3 tools/check_reproducible_cart.py --rom="/path/to/owned/smb.zip"
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 --sample-vblanks 120 \
  --assert-zero-missed

# Use a local text FM2 as a deterministic all-stage cartridge gate
make -C platform/neogeo replay-cart \
  SMB_ROM="/path/to/owned/smb.zip" \
  REPLAY_FM2="/path/to/full-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1
python3 tools/run_neogeo_replay_gate.py --timeout 900
```

`verify` does not need an SMB ROM. `cart` and `run` accept either a raw iNES
file or a ZIP with exactly one `.nes` member. The cadence probe uses an
isolated X display, positively verifies ownership of the emulator's fixed
debugger listener, and applies a finite sampling deadline. `replay-cart`
requires a local text FM2; it does not download or track the movie. The
`REPLAY_HARDWARE_PLAYABLE=1` policy rejects simultaneous opposite joystick
directions. The runner owns an isolated display and emulator process group,
resolves the cartridge's raw trap/mailbox addresses from its ELF, applies a
finite deadline, and writes a bounded evidence directory.

Generated output is intentionally ignored:

- ROM-less `build/smbneogeo.elf` / `.map` verification outputs
- title-enabled `build/smbneogeo-cart.elf` / `.map` cartridge outputs
- `build/assets/asset-manifest.json`
- `build/rom/smbneogeo.zip`
- `build/replay-fast/` and `build/replay-rendered/`
- emulator BIOS/hash support files

The repository contains no Nintendo graphics. The local cartridge does, so it
must not be redistributed.

## Controls

The port reads the active-low Neo Geo controller registers directly. Its
desktop bindings are:

- arrow keys: directional joystick
- `A`: Neo Geo A / NES A (jump)
- `S`: Neo Geo B / NES B (run/fire)
- `Q`, `W`: Neo Geo C/D (currently unused by SMB)
- `1`: player-one Start / NES Start
- `2`: player-two Start / NES Select

Press `1` at the title screen before trying to move Mario. The attract-mode
demo does not accept movement input. The original large title panel,
copyright line, top score, and one/two-player menu are reconstructed through
the game's own nametable writes from locally generated title data.
