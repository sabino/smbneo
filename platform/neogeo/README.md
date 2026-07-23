# Neo Geo target

This target statically compiles the translated SMB C core for the Neo Geo
MC68000 and replaces the desktop PPU/APU modules with:

- `ppu_direct.c`: 2 KiB nametable, 256-byte OAM, 32-byte palette, and PPU
  register behavior without CHR/framebuffer storage.
- `video.c`: direct C-ROM/S-ROM, palette RAM, FIX-map, and SCB1-4 writes with
  two 161-sprite frame sets.
- `apu_null.c`: a zero-allocation placeholder for a future YM2610 driver.
- `main.c`: Neo Geo input mapping and the 60 Hz game loop.

## Commands

From the repository root:

```bash
make -C platform/neogeo verify
make -C platform/neogeo cart SMB_ROM="/path/to/owned/smb.nes"
make -C platform/neogeo run SMB_ROM="/path/to/owned/smb.zip"
```

`verify` does not need an SMB ROM. `cart` and `run` accept either a raw iNES
file or a ZIP with exactly one `.nes` member.

Generated output is intentionally ignored:

- `build/smbneogeo.elf` and `build/smbneogeo.map`
- `build/assets/asset-manifest.json`
- `build/rom/smbneogeo.zip`
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
demo does not accept movement input.
