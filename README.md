# SMB

> **Neo Geo port in progress.** This fork now has a pure-C MC68000 target with
> a direct Neo Geo tile/sprite renderer. It does not allocate a software
> framebuffer or copy CHR graphics into work RAM. The original desktop and web
> targets remain available.

[Play it online](https://nathsou.github.io/smb/)

Static recompilation of Super Mario Bros. using [doppelganger's disassembly](https://www.romhacking.net/documents/344/)

[![SMB C port running in the browser](res/smb-demo.png)](https://nathsou.github.io/smb)

## Controls

- D-Pad: WASD
- B: K
- A: L
- start: Enter
- select: Space
- z: Save state
- x: Load state

## Checkpoints

- [x] Static translation of the disassembly to low-level C
- [x] PPU & APU emulation layers
- [x] Convert subroutines to C functions
- [x] Convert most gotos to if statements
- [ ] Remove unused flag updates
- [ ] Replace PPU with direct draw calls
- [ ] Manually rewrite portions of the code to higher level C

## Building

### Neo Geo (ngdevkit)

The optimized playable milestone cross-compiles, links, packages, and boots
in ngdevkit-gngeo. The direct hardware renderer sustains one game frame per
display VBlank after its cold-start cache fill at the stock emulated 68000
clock. Graphics are generated locally from a legally obtained Super Mario
Bros. (World) dump and remain under the ignored `platform/neogeo/build/`
directory.

```bash
# Pure-C cross-build, unit tests, ELF architecture check, and RAM guard
make -C platform/neogeo verify

# Complete cartridge from a raw .nes file or a ZIP containing one .nes file
make -C platform/neogeo cart \
  SMB_ROM="/path/to/smb.zip"

# Prove two isolated cartridge builds are byte-for-byte identical
python3 tools/check_reproducible_cart.py \
  --rom "/path/to/smb.zip"

# Measure stock-clock steady-state cadence without touching a visible run
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 --sample-vblanks 120 \
  --assert-zero-missed

# Launch the generated cartridge in ngdevkit-gngeo
make -C platform/neogeo run \
  SMB_ROM="/path/to/smb.zip"
```

The supported ROM revision has SHA-1
`ea343f4e445a9050d4b4fbac2c77d0693b1d0922`. The converter reads only its
8 KiB CHR bank and writes no PRG bytes. Do not redistribute generated
cartridge or graphics files.

Current Neo Geo controls are joystick, A (jump), B (run/fire), Start, and
Select. In GnGeo, the defaults are arrow keys, `A`, `S`, `1`, and `2`,
respectively. Press `1` at the title screen before trying to move. Audio is
intentionally silent in this milestone, and deterministic verification of all
32 stages is still in progress; see
[`docs/NEOGEO_PORT.md`](docs/NEOGEO_PORT.md) for the architecture, measured
memory use, verification evidence, and remaining work.

### Linux & MacOS

1. Fetch the submodules:
```bash
$ git submodule update --init --recursive
```

2. Build raylib, follow the instructions [here](https://github.com/raylib-extras/raylib-quickstart)

3. Run `make build` in the root folder:
```bash
$ make build
```

4. Place a legally obtained dump/ROM of SMB called `smb.nes` in the root folder to extract graphics data from
5. You can now run `./smb`

## WebAssembly

1. Install a recent version of `clang` with support for the `wasm32` target
2. Run `make wasm`
3. Run an HTTP server in the `web/` folder and open `index.html` in your browser
4. Select a legally obtained dump/ROM of SMB to extract graphics data from

## Codegen

The output of the code generator is in the `codegen/` folder. To regenerate it:

1. Install [Moonbit](https://www.moonbitlang.com/):

```bash
$ curl -fsSL https://cli.moonbitlang.com/install/unix.sh | bash -s '0.7.1+c0b22a8b0'
```

2. Run `make codegen`

## References & Resources

- [doppelganger's disassembly](https://www.romhacking.net/documents/344/)
- [SuperMarioBros-C by MitchellSternke](https://github.com/MitchellSternke/SuperMarioBros-C)
- [Nesdev Wiki](https://www.nesdev.org/wiki/Nesdev_Wiki)
- [nessy](https://github.com/nathsou/nessy)
- [An Overview of NES Rendering by Austin Morlan](https://austinmorlan.com/posts/nes_rendering_overview/)
