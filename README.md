# SMBNeo

> **Super Mario Bros. for Neo Geo.** This playable work in progress adds a
> pure-C MC68000 target with a direct Neo Geo tile/sprite renderer and a
> native Z80/YM2610 audio bridge.
> It does not allocate a software framebuffer, copy CHR graphics into work RAM,
> or run a host software mixer.

This is an unofficial preservation/engineering project built on
[`nathsou/smb`](https://github.com/nathsou/smb), which statically recompiles
the game from [doppelganger's disassembly](https://www.romhacking.net/documents/344/).
This fork's active branch is intentionally Neo Geo-only. The upstream project
retains its desktop, WebAssembly, and 3DS frontends.

No game ROM, Neo Geo BIOS, or generated cartridge is included. Point the
build at your own local game image; converted graphics and cartridge files
stay under ignored build directories.

See [visual fidelity](docs/VISUAL_FIDELITY.md) for the sprite geometry and
color target, and [third-party notices](THIRD_PARTY_NOTICES.md) for project
credits.

## Controls

- Hardware: joystick, A (jump), B (run/fire), Start, and Select.
- GnGeo defaults: arrow keys, `A`, `S`, `1`, and `2`.
- Press `1` at the title screen before trying to move.
- Impossible left+right or up+down input pairs are neutralized.

## Current status

- Playable cartridge with the original title/menu and all-world progression.
- Direct FIX/sprite/palette renderer with no software framebuffer.
- Native Z80/YM2610 audio bridge.
- Stock-clock interactive pacing and regression coverage for crowded scenes.
- Deterministic replay, audio, cadence, memory, and reproducibility gates.
- Physical AES/MVS-compatible hardware validation remains open.

## Build and run

The optimized playable milestone cross-compiles, links, packages, and boots
in ngdevkit-gngeo. Its direct hardware renderer keeps one persistent
33-strip background ring and double-buffers the object sprites. Light,
steady-state scenes fit one game tick per display VBlank at the stock
emulated 68000 clock; enemy-heavy scenes have separate exact regression
windows instead of being covered by that bounded smoke result. Graphics are
generated locally from a supported user-supplied game image and remain under
the ignored `platform/neogeo/build/` directory.

```bash
# Generator consistency, pure-C/audio tests, ELF/RAM and Z80 map guards
make verify

# Complete cartridge from a raw .nes file or a ZIP containing one .nes file
make cart \
  SMB_ROM="/path/to/smb.zip"

# Prove two isolated cartridge builds are byte-for-byte identical
python3 tools/check_reproducible_cart.py \
  --rom "/path/to/smb.zip"

# Enter gameplay and reject an empty or silent normal-mode PCM capture
python3 tools/probe_neogeo_audio.py

# Bounded stock-clock light/steady-state cadence smoke
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 --sample-vblanks 120 \
  --assert-zero-missed

# Launch the generated cartridge in ngdevkit-gngeo
make run \
  SMB_ROM="/path/to/smb.zip"

# Build a gate-only cartridge from a locally downloaded text FM2 movie
make replay-cart \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1

# Run it to a pass/fail trap and retain a bounded result.json plus logs
python3 tools/run_neogeo_replay_gate.py \
  --68k-overclock 10000 --timeout 2400

# Build the rendered variant and retain every stage transition/settled pair
make replay-rendered-evidence \
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
too. Current linked memory measurements are maintained in
[`docs/NEOGEO_PORT.md`](docs/NEOGEO_PORT.md).
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

Native audio maps the two pulse voices to YM2610 SSG A/B, percussion noise to
SSG C, and the triangle voice to an independently pitched looping ADPCM-B
waveform. A custom Z80 M1 driver receives acknowledged, coalesced port-A
register updates from the MC68000, while a deterministic 512 KiB V1 contains
the generated triangle loop. Pulse levels pass through a source-mixer-derived
curve so jump/fire peaks no longer overload logarithmic SSG volume steps, and
native sweep/envelope counters catch up safely when rendering spans an extra
display period. Music sequencing and gameplay-coupled sound state remain on
ordinary game frames. This is substantially closer to the source mix, but it
is not cycle- or waveform-exact: SSG pulse duty, short-noise mode, direct DAC
behavior, sub-frame register timing, and physical AES/MVS-compatible hardware
validation remain open.
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
