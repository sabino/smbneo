# Neo Geo target

This target statically compiles the translated SMB C core for the Neo Geo
MC68000 and replaces the desktop PPU/APU modules with:

- `ppu_direct.c`: 2 KiB nametable, 256-byte OAM, 32-byte palette, and PPU
  register behavior without full CHR/framebuffer storage. Cartridge builds
  retain only the 314-byte CHR read window used to construct the title menu.
- `video.c`: direct C-ROM/S-ROM, palette RAM, FIX-map, and SCB1-4 writes with
  two 161-sprite frame sets, generation caches, and next-VBlank live swaps.
- `apu_bridge.c`: integer-only NES APU register shadowing, envelope/pitch
  approximation, and changed-register coalescing for the YM2610 SSG.
- `apu_neogeo.c`: acknowledged MC68000-to-Z80 transport, startup recovery, and
  the target implementation of the translated core's APU interface.
- `sound_driver.s`: custom nullsound command table and timing-safe YM2610 SSG
  writes for the M1 sound ROM.
- `main.c`: Neo Geo input mapping and the 60 Hz game loop.

## Commands

From the repository root:

```bash
make -C platform/neogeo verify
make -C platform/neogeo cart SMB_ROM="/path/to/owned/smb.nes"
make -C platform/neogeo run SMB_ROM="/path/to/owned/smb.zip"
python3 tools/check_reproducible_cart.py --rom="/path/to/owned/smb.zip"
python3 tools/probe_neogeo_audio.py
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

`verify` does not need an SMB ROM. It builds and tests the integer audio
bridge, links the custom Z80 driver, and rejects a missing or unsafe Z80 linker
map in addition to the MC68000 checks. `cart` and `run` accept either a raw
iNES file or a ZIP with exactly one `.nes` member. The grouped Z80 driver/map
rule requires GNU Make 4.3 or newer. The cadence probe uses an
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
- `build/smbneogeo-sound.ihx` / `.map` and Z80 assembler intermediates
- `build/assets/asset-manifest.json`
- `build/rom/smbneogeo.zip`
- `build/replay-fast/` and `build/replay-rendered/`
- emulator BIOS/hash support files

The repository contains no Nintendo graphics. The local cartridge does, so it
must not be redistributed.

## Native audio MVP

The Neo Geo target does not link the desktop PCM mixer. The translated game
continues writing its normal APU registers, while `apu_bridge.c` shadows the
relevant state and emits only changed YM2610 SSG registers once per rendered
game frame:

- pulse 1 and pulse 2 use SSG tone channels A and B;
- triangle uses SSG tone channel C;
- noise uses SSG noise on channel C; and
- software envelope and integer period conversion avoid audio buffers,
  floating point, and division in the frame loop.

Each target register update is encoded as three commands: register selector,
high data nibble, and low data nibble/commit. The MC68000 waits for the
nullsound `command | 0x80` acknowledgement after each byte. The Z80 command
handler preserves FIFO order and calls `ym2610_write_port_a`, which supplies
the chip's required write delays and interrupt-safe port restoration. Driver
reset uses an eight-frame startup delay and an alternating ready ping. A
failed handshake triggers up to two reset retries; the third consecutive
failure disables transport rather than hanging gameplay.

The cartridge contains the custom 128 KiB M1 image. V1 remains a 512 KiB
zero-filled region because the MVP uses no ADPCM samples. `run` and rendered
`replay-run` start GnGeo with sound enabled. `REPLAY_FAST=1` deliberately skips
both rendering and per-frame sound transport and therefore remains a
core-progression gate, not an audio test.

This bridge is intentionally approximate:

- SSG tones have a fixed 50-percent duty cycle, so source pulse duty is lost;
- pulse sweep, hardware length counters, short-noise mode, and direct DAC/DMC
  behavior are not implemented;
- the triangle voice is represented by a square tone and very low triangle
  periods clamp to the SSG's 12-bit maximum; and
- triangle and noise share SSG C's volume and are AND-gated when simultaneous,
  rather than being independently mixed.

The implementation and ROM packaging have host-side and emulator-oriented
coverage, but audio fidelity has not been validated on physical
AES/MVS-compatible hardware.

The bounded normal-mode smoke probe explicitly avoids GnGeo's `-D` debugger
mode because that emulator mode forcibly disables sound and Z80 execution. It
uses explicit no-debug and stock-clock flags plus an isolated home/config and
X display, retries Start until gameplay produces a signal, then starts a new
exact-duration signed-16-bit stereo evidence segment through SDL's disk
driver. Finite argument ceilings, command timeouts, an overall deadline, and
a child file-size limit bound the live run. It rejects empty/silent output,
retains hashes/logs plus a final-frame screenshot, and deletes the raw PCM
after a successful check unless `--keep-raw` is requested. A failed probe
retains its bounded raw capture for diagnosis:

```bash
python3 tools/probe_neogeo_audio.py \
  --evidence-dir /tmp/smb-neogeo-audio-evidence
```

The debugger cadence probe remains a separate renderer/game-loop timing gate
and intentionally runs with sound disabled.

At this milestone the Z80 linker map reports 10,777 bytes of fixed code,
1,944 bytes of data, and 101 bytes between static data and the stack start.
`tools/check_neogeo_sound_driver.py` requires:

- non-empty code starting at `$0000` and fitting below `$8000`;
- data starting exactly at `$f800` and ending below the `$fffd` stack start;
- at least 64 bytes of stack headroom; and
- one consistent CODE and DATA summary in the linker map.

The host bridge regression covers initial mute, changed-register coalescing,
pitch conversion including A4, envelope decay, master disable/re-enable,
triangle/noise mixer state and representative noise periods, coarse-before-
fine tone writes, and retry behavior after a failed transport write. Python
unit tests exercise all Z80 map rejection paths.

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
