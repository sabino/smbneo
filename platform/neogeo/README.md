# SMBNeo target

This target statically compiles the translated SMB C core for the Neo Geo
MC68000 and replaces the desktop PPU/APU modules with:

- `ppu_direct.c`: 2 KiB nametable, 256-byte OAM, 32-byte palette, and PPU
  register behavior without full CHR/framebuffer storage. Cartridge builds
  retain only the 314-byte CHR read window used to construct the title menu.
- `video.c`: direct C-ROM/S-ROM, palette RAM, FIX-map, and SCB1-4 writes with
  one persistent 33-strip background ring, two 64-entry OAM banks on each
  priority plane, generation caches, sparse uploads, and next-VBlank swaps.
- `apu_bridge.c`: NES APU register shadowing, envelope/sweep/length/linear
  state, and changed-register coalescing for YM2610 SSG plus ADPCM-B.
- `apu_neogeo.c`: acknowledged MC68000-to-Z80 transport, startup recovery, and
  the target implementation of the translated core's APU interface.
- `audio_cadence.c`: bounded display-period catch-up for native APU hardware
  state without advancing translated music queues or gameplay RAM.
- `sound_driver.s`: custom nullsound command table and timing-safe YM2610
  port-A writes for the M1 sound ROM.
- `tools/gen_neogeo_triangle_vrom.py`: deterministic 64 KiB encoded triangle
  loop inside a padded 512 KiB V1 image.
- `main.c`: Neo Geo input mapping and the VBlank-paced game loop.

## Commands

From the repository root:

```bash
make -C platform/neogeo verify
make -C platform/neogeo cart SMB_ROM="/path/to/owned/smb.nes"
make -C platform/neogeo hardware-cart SMB_ROM="/path/to/owned/smb.nes"
make -C platform/neogeo run SMB_ROM="/path/to/owned/smb.zip"
python3 tools/check_reproducible_cart.py --rom="/path/to/owned/smb.zip"
python3 tools/probe_neogeo_audio.py
# Bounded light/steady-state smoke; see docs for crowded regression windows
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 --sample-vblanks 120 \
  --assert-zero-missed

# Use a local text FM2 as a deterministic all-stage cartridge gate
make -C platform/neogeo replay-cart \
  SMB_ROM="/path/to/owned/smb.zip" \
  REPLAY_FM2="/path/to/full-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1
python3 tools/run_neogeo_replay_gate.py --timeout 900

# Exercise the direct renderer through every stage and retain 65 PNGs
make -C platform/neogeo replay-rendered-evidence \
  SMB_ROM="/path/to/owned/smb.zip" \
  REPLAY_FM2="/path/to/full-warpless.fm2" \
  REPLAY_HARDWARE_PLAYABLE=1 \
  REPLAY_EVIDENCE_DIR="/tmp/smb-neogeo-rendered-evidence"
```

`verify` does not need an SMB ROM. It builds and tests the integer audio
bridge, links the custom Z80 driver, and rejects a missing or unsafe Z80 linker
map in addition to the MC68000 checks. `cart`, `hardware-cart`, and `run`
accept either a raw iNES file or a ZIP with exactly one `.nes` member.
`cart` produces the default fixed-database emulator set as
`build/rom/puzzledp.zip`. `hardware-cart` preserves the full P/C layout as
`build/rom/smbneogeo.zip` and generates the custom GnGeo hash data. `run`
uses that full native package rather than the compatibility alias. See
[`docs/EMULATOR_COMPATIBILITY.md`](../../docs/EMULATOR_COMPATIBILITY.md) for
the exact FBNeo, NEO.emu, MAME, GnGeo, and BIOS paths.

Interactive `run` and `replay-run` explicitly use stock CPU-clock adjustments
plus `--autoframeskip --sleepidle --no-vsync`. GnGeo implements its 60 Hz host
wait inside the automatic-frame-skip path, so disabling that option also
removes wall-clock throttling and lets gameplay speed follow host load. The
grouped Z80 driver/map rule requires GNU Make 4.3 or newer. The cadence probe
uses an isolated X display, positively verifies ownership of the emulator's
fixed debugger listener, and applies a finite sampling deadline. It
intentionally disables host pacing to measure guest game frames per emulated
VBlank; it is not the interactive wall-clock-speed gate. `replay-cart`
requires a local text FM2; it does not download or track the movie. The
`REPLAY_HARDWARE_PLAYABLE=1` policy rejects simultaneous opposite joystick
directions. The runner owns an isolated display and emulator process group,
resolves the cartridge's raw trap/mailbox addresses from its ELF, applies a
finite deadline, and writes a bounded evidence directory.

`replay-rendered-evidence` selects `build/replay-rendered`, uses stock
MC68000 timing by default, and retains an immediate plus two-rendered-frame
settled screenshot for each of the 32 ordered stages, followed by the stable
victory screen. It preflights `scrot` and Python Pillow before launching the
emulator, requires a hardware-playable movie with zero opposite-direction
transitions, and rejects any settling interval other than exactly two rendered
frames. The runner requires renderer and VBlank counters to match the
mailbox's translated-frame accounting and rejects missing, blank, malformed,
or consecutively stale settled-stage images. A new external evidence directory
below `/tmp` retains immutable, hashed snapshots of the exercised artifacts,
capture/validation provenance, the 65 PNGs, logs, and a manifest bounded to
128 KiB. A cartridge-side fence first waits for its uploaded and presented
16-bit generations to match, and mailbox version 4 makes that binding
host-verifiable. The two shared generation words and one 16-bit copy per
VBlank also exist in the normal cartridge and are included in the current
linked-memory measurements in `docs/NEOGEO_PORT.md`. The host then waits 50
milliseconds by default only for the already-issued SDL/X11 presentation to
settle before capture; the bounded `--display-settle-seconds` range is 0
through 0.25 seconds and is recorded in the manifest without adding cartridge
cost. GnGeo debugger mode disables its Z80/audio path, so this lane proves
renderer endurance and full-game progression, not audio, pixel-perfect
source-console fidelity, or physical-hardware behavior; the normal-mode audio
probe remains separate.

The following repository-local build output is intentionally ignored:

- ROM-less `build/smbneogeo.elf` / `.map` verification outputs
- title-enabled `build/smbneogeo-cart.elf` / `.map` cartridge outputs
- `build/smbneogeo-sound.ihx` / `.map` and Z80 assembler intermediates
- `build/assets/asset-manifest.json`
- `build/rom/puzzledp.zip` (default emulator compatibility package)
- `build/rom/smbneogeo.zip` (canonical hardware/MAME/GnGeo package)
- `build/replay-fast/` and `build/replay-rendered/`
- emulator BIOS/hash support files

Rendered evidence is deliberately written to the caller-selected external
`/tmp` directory; it is not described as repository-ignored output.

The repository contains no Nintendo graphics. The local cartridge does, so it
must not be redistributed.

## Native audio

The Neo Geo target does not link the desktop PCM mixer. The translated game
continues writing its normal APU registers. `apu_bridge.c` shadows the
relevant state and emits only changed YM2610 registers on ordinary sound
frames and bounded native-hardware catch-up periods:

- pulse 1 and pulse 2 use SSG tone channels A and B;
- noise uses SSG noise on channel C;
- triangle uses a separately pitched, looping ADPCM-B waveform; and
- software envelopes, pulse sweep, length counters, the triangle linear
  counter, and integer period conversion avoid runtime PCM buffers and
  floating point.

Source pulse levels do not pass directly into the logarithmic SSG ladder. A
16-entry curve models the source pulse mixer's compressed relative amplitude,
keeps the common source level 8 at target level 8, and caps source levels
13..15 at target level 10. This reduces the jump/fire-to-music gap while
preserving headroom. Noise remains independent because its source mixer path
differs.

The bridge clocks both pulse sweep units twice per game frame, including
divider/reload ordering, continuous target-overflow muting, and the distinct
negate arithmetic of the two source pulse channels. Changed target periods
continue through the same coalesced register transport. Length counters clock
twice per game frame, while envelopes and the triangle linear counter receive
four quarter-frame clocks. Channel-disable writes clear lengths immediately,
and disabled timer-high writes cannot revive a channel.

If rendering consumes more than one display period, `audio_cadence.c` advances
only these native sweep/envelope/length/linear units for the missed periods
before the next ordinary frame. It reserves the latest period for the normal
game/audio update and caps recovery at four steps, with a dropped-period
diagnostic, so transport trouble cannot create an unbounded catch-up loop.
Translated music/SFX queues are deliberately not advanced out of band because
their event-music state is also read by end-of-level gameplay logic.

Each 13-bit target register/value payload is encoded as two commands from the
122-symbol `$06-$7f` alphabet. Base-121 quotient/remainder digits are rotated
relative to the preceding symbol, guaranteeing distinct adjacent commands
even across packet boundaries. This prevents a stale
`command | 0x80` echo from satisfying the next acknowledgement wait, while
leaving bit 7 reserved for acknowledgements. The Z80 FIFO preserves order;
its main loop decodes the pair and calls `ym2610_write_port_a`, which supplies
the chip's required write delays and interrupt-safe port restoration. Driver
reset uses an eight-frame startup delay and an alternating ready ping that
also resets packet state. A failed handshake triggers up to two reset retries;
the third consecutive failure disables transport rather than hanging
gameplay.

The cartridge contains the custom 128 KiB M1 image and a deterministic
512 KiB V1 image. The first 64 KiB of V1 encode 2,048 periods of a generated,
DC-centered 64-point triangle; the remainder is reserved as zeroes. Its
predictor-reset seam differs from the ideal adjacent sample by one PCM unit,
and its fixed SHA-256 is checked before both normal and replay packaging.
`run` and rendered `replay-run` start GnGeo with sound enabled.
`REPLAY_FAST=1` deliberately skips both rendering and per-frame sound
transport and therefore remains a core-progression gate, not an audio test.

Remaining approximations are explicit:

- SSG tones have a fixed 50-percent duty cycle, so source pulse duty is lost;
- the noise mode bit is retained in bridge state, but the SSG cannot reproduce
  the source short-noise sequence and slow periods saturate at 31;
- direct DAC/DMC mixer bias is not modeled;
- translated music-note and SFX-duration sequencing remains tied to completed
  game frames, while native sweep/envelope/length units additionally follow
  bounded elapsed-display-period catch-up;
- changed SSG periods are transported on the next 60 Hz bridge boundary;
- APU units are aggregated at the 60 Hz bridge boundary instead of exact
  source-chip sub-frame instants; and
- physical-cartridge exposure of V1 to the ADPCM-B bus still needs validation.

The implementation and ROM packaging have host-side and emulator-oriented
coverage, but audio fidelity has not been validated on physical
AES/MVS-compatible hardware.

The bounded normal-mode smoke probe explicitly avoids GnGeo's `-D` debugger
mode because that emulator mode forcibly disables sound and Z80 execution. It
uses explicit no-debug, stock-clock, and 60 Hz wall-clock-pacing flags plus an
isolated home/config and X display, retries Start until gameplay produces a
signal, then starts a new exact-duration signed-16-bit stereo evidence segment
through SDL's disk driver. Finite argument ceilings, command timeouts, an
overall deadline, and a child file-size limit bound the live run. It rejects
empty/silent output, retains hashes/logs plus a final-frame screenshot, and
deletes the raw PCM after a successful check unless `--keep-raw` is requested.
A failed probe retains its bounded raw capture for diagnosis:

```bash
python3 tools/probe_neogeo_audio.py \
  --evidence-dir /tmp/smb-neogeo-audio-evidence
```

The debugger cadence probe remains a separate renderer/game-loop VBlank-budget
gate and intentionally runs with sound and wall-clock pacing disabled.

At this milestone the Z80 linker map reports 10,965 bytes of fixed code,
1,945 bytes of data, and 100 bytes between static data and the stack start.
`tools/check_neogeo_sound_driver.py` requires:

- non-empty code starting at `$0000` and fitting below `$8000`;
- data starting exactly at `$f800` and ending below the `$fffd` stack start;
- at least 64 bytes of stack headroom; and
- one consistent CODE and DATA summary in the linker map;
- the exact 128-entry packet command dispatch and decoder semantics; and
- exactly the three locally owned packet-state bytes in Z80 DATA.

The host bridge regression covers initial mute, changed-register coalescing,
pitch conversion including A4, envelope decay, master disable/re-enable,
length-table/halt behavior, triangle linear reload and timer muting,
independent ADPCM-B triangle/noise state, representative noise periods,
the flagpole sweep, pulse-volume normalization, coarse-before-fine tone writes,
and retry behavior after a failed transport write. A separate cadence
regression covers single/multiple missed periods, counter wrap, catch-up that
itself crosses a display boundary, and bounded debt dropping. Python tests
exercise the Z80 protocol/map rejection paths and verify the generated V1
waveform, decode error, loop seam, padding, and hash.

## Controls

The port reads the active-low Neo Geo controller registers directly. Its
default GnGeo keyboard bindings are:

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
