# SMB

> **Neo Geo port in progress.** This fork now has a pure-C MC68000 target with
> a direct Neo Geo tile/sprite renderer and a native Z80/YM2610 audio bridge.
> It does not allocate a software framebuffer, copy CHR graphics into work RAM,
> or run the desktop software mixer. The original desktop and web targets
> remain available.

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
# Generator consistency, pure-C/audio tests, ELF/RAM and Z80 map guards
make -C platform/neogeo verify

# Complete cartridge from a raw .nes file or a ZIP containing one .nes file
make -C platform/neogeo cart \
  SMB_ROM="/path/to/smb.zip"

# Prove two isolated cartridge builds are byte-for-byte identical
python3 tools/check_reproducible_cart.py \
  --rom "/path/to/smb.zip"

# Enter gameplay and reject an empty or silent normal-mode PCM capture
python3 tools/probe_neogeo_audio.py

# Measure stock-clock steady-state cadence without touching a visible run
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 --sample-vblanks 120 \
  --assert-zero-missed

# Launch the generated cartridge in ngdevkit-gngeo
make -C platform/neogeo run \
  SMB_ROM="/path/to/smb.zip"

# Build a gate-only cartridge from a locally downloaded text FM2 movie
make -C platform/neogeo replay-cart \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1

# Run it to a pass/fail trap and retain a bounded result.json plus logs
python3 tools/run_neogeo_replay_gate.py \
  --68k-overclock 10000 --timeout 2400

# Build the rendered variant and retain every stage transition/settled pair
make -C platform/neogeo replay-rendered-evidence \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_HARDWARE_PLAYABLE=1 \
  REPLAY_EVIDENCE_DIR="/tmp/smb-neogeo-rendered-evidence"
```

The interactive `run` target pins both emulated CPUs to their stock clock
adjustments and explicitly enables GnGeo's 60 Hz wall-clock limiter with
`--autoframeskip --sleepidle --no-vsync`. In the installed GnGeo version, the
wait-until-next-frame logic is part of
[`frame_skip()`](https://github.com/dciabrin/gngeo/blob/70121c69accb549f1e9173a41aab46af47619e34/src/frame_skip.c);
`--no-autoframeskip` therefore makes emulation run as fast as the host permits
rather than merely disabling dropped video frames. The debugger-only cadence
and replay gates remain deliberately unpaced because they validate guest
frame/VBlank accounting, not interactive wall-clock speed.

The rendered-evidence target requires `scrot` and the Python Pillow package;
both are checked before the long emulator run begins. It accepts only a
hardware-playable input with no simultaneous opposite directions and uses
exactly two rendered frames between each transition and settled capture.
Before each screenshot-bearing trap, a cartridge-side presentation fence
waits until the VBlank callback has latched the uploaded render generation as
presented; mailbox version 4 exposes both 16-bit generations and the host
requires equality. Because the renderer object is shared, that handshake uses
two 16-bit state words and one 16-bit copy per VBlank in the normal cartridge
too; linker padding absorbs the words, so measured total BSS is unchanged.
After the debugger stops, the host waits another 50 milliseconds by default
only to let the already-issued SDL/X11 presentation settle before `scrot`.
`--display-settle-seconds` can tune this host-only allowance from 0 through
0.25 seconds; the chosen value is recorded in the manifest and adds no further
cartridge cost.
Choose a new external directory below `/tmp` for the evidence rather than a
path in the repository. The resulting 128 KiB-bounded manifest binds the
captures to immutable, hashed artifact snapshots and validation provenance.

The supported ROM revision has SHA-1
`ea343f4e445a9050d4b4fbac2c77d0693b1d0922`. The converter reads only its
8 KiB CHR bank, converts the graphics, and carries the 314-byte title
nametable payload into the P-ROM. It writes no source PRG bytes. The large,
centered original title panel and one/two-player menu are therefore restored
without a tracked bitmap or ROM-derived source file. Do not redistribute
generated cartridge or graphics files.

Current Neo Geo controls are joystick, A (jump), B (run/fire), Start, and
Select. In GnGeo, the defaults are arrow keys, `A`, `S`, `1`, and `2`,
respectively. Press `1` at the title screen before trying to move. Impossible
left+right or up+down keyboard pairs are neutralized.

Native audio maps the two pulse voices to YM2610 SSG A/B, percussion noise to
SSG C, and the triangle voice to an independently pitched looping ADPCM-B
waveform. A custom Z80 M1 driver receives acknowledged, coalesced port-A
register updates from the MC68000, while a deterministic 512 KiB V1 contains
the generated triangle loop. Pulse sweep, hardware length counters, and the
triangle linear counter are modeled in software. This is substantially closer
to the source mix, but it is not cycle- or waveform-exact: SSG pulse duty,
short-noise mode, direct DAC behavior, sub-frame register timing, and physical
AES/MVS-compatible hardware validation remain open.
After building the cartridge, `tools/probe_neogeo_audio.py` supplies a bounded
normal-mode smoke test: it enters gameplay under an isolated emulator
configuration, records an exact post-activation interval through an SDL disk
sink, rejects silence, saves a final-frame screenshot plus hashed JSON
evidence, and removes the raw PCM after a successful check by default. A
failed run retains its bounded capture for diagnosis.

The cartridge replay gate accepts strict local FM2 input logs, preserves their
hashes and startup metadata, and checks sequential entry into all 32 stages
plus the final victory state. Its versioned debugger mailbox also records
bootstrap skips, area-load holds, and the exact number of translated core
frames advanced. A separate rendered lane binds each ordered stage state to
an immediate transition PNG, a PNG after two complete render/VBlank frames,
and renderer/game-frame/VBlank counters, then captures the stable victory
screen. The runner does not download the movie; generated screenshots and
their immutable provenance snapshot stay in the explicitly selected external
evidence directory. This is an endurance and progression proof, not a claim
of pixel-perfect source-console fidelity, audio coverage, or physical-hardware
validation. `REPLAY_FAST=1` skips both the renderer and per-frame APU step. The
preferred no-opposite full-game movie has passed that fast gate through all 32
stages and a stable victory state. It has also passed the stock-clock rendered
lane with 32 transition/settled pairs and the terminal capture. See
[`docs/NEOGEO_PORT.md`](docs/NEOGEO_PORT.md) for the architecture, measured
memory use, recommended published TAS inputs, verification evidence, and
remaining work.

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
