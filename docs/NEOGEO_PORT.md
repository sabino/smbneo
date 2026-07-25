# Neo Geo port architecture

## Milestone status

The current milestone is a bootable, playable, performance-characterized Neo Geo
cartridge. It runs the upstream translated C game core on the MC68000, uses
Neo Geo graphics hardware directly, reads active-low Neo Geo controller
registers directly, and packages a cartridge from a user-owned SMB dump.

Completed:

- Pure C target; no C++ runtime or desktop framework in the Neo Geo link.
- MC68000 code generation (`-m68000 -mlra`) with `-O3`, LTO, and measured
  inlining of the translated core's hot instruction helpers.
- Direct background, OAM sprite, palette, FIX HUD, and input backends.
- One persistent 33-strip background ring with generation-tracked columns,
  sparse changed-row uploads, a 64-row bulk-rebuild threshold, and one or two
  sticky chain drivers.
- Double-buffered OAM around that persistent background, sparse FIX/palette
  updates, and next-VBlank live-state swaps.
- Local, asset-clean CHR-to-C-ROM/S-ROM converter with unit tests.
- Full P/M/V/S/C ROM packaging and ngdevkit-gngeo boot.
- Byte-for-byte reproducible cartridge checker using two isolated builds.
- Deterministic recording validation, compact replay emission, and a strict
  local FM2 input-log importer.
- A separate FM2-driven cartridge, sequential 32-stage/final-victory tracker,
  raw debugger mailbox, and bounded isolated runner for progression evidence.
- A passing hardware-direction-safe full-game replay through all 32 stages
  and a 60-frame stable final-victory condition.
- Measured FM2 source-frame bootstrap/area-load timing, versioned replay
  accounting, and exact FCEUX-versus-C state transcript tooling.
- Restored carry semantics for every translated assembly jump dispatcher,
  with generated-code and gameplay-state regression tests.
- Neutralized impossible opposite keyboard directions before they can corrupt
  the original game's single-stick direction state.
- Exact native-reference and port measurements for ordinary and enemy-heavy
  scenes, including dedicated crowd regression windows.
- Integer-only NES APU-to-YM2610 SSG bridge with changed-register coalescing,
  acknowledged MC68000/Z80 transport, and a custom M1 sound driver.
- Per-channel software pulse sweep with exact target overflow muting,
  divider/reload ordering, and pulse-1/pulse-2 negate asymmetry.
- Host regressions for pitch, sweep, envelope, mixer, noise, write ordering,
  and transport retry plus an enforced Z80 ROM/RAM/stack linker-map budget.
- Automated architecture, translated-core reachability, forbidden-symbol,
  and work-RAM guards.

Not completed:

- Full-fidelity audio synthesis. The current native SSG bridge intentionally
  approximates source waveforms and omits several APU behaviors.
- Physical retest of the corrected background/FIX handoff on MV1C hardware,
  plus broader AES and flash-cartridge validation.
- Cycle/scanline-accurate PPU behavior. This port intentionally follows the
  upstream frame-at-a-time timing model.
- Exact NES left-edge masking in every fine-scroll case.
- One game tick per stock-clock VBlank throughout the enemy-heavy regression
  windows.
- Save state or memory-card support.

## Why this renderer fits Neo Geo

The desktop PPU turns every NES frame into a 256x240 RGB bitmap. That is a poor
fit for the Neo Geo's roughly 64 KiB user work-RAM window and would also spend
the 12 MHz 68000 on work already handled by the sprite hardware.

The port converts each NES 8x8 2bpp CHR tile in two ways at build time:

1. C-ROM: pixels are expanded 2x to a 16x16 4bpp Neo Geo tile. Single-tile
   OAM uses SCB2 `$077e`; background chains use `$077f`. Both select the same
   eight in-tile rows, while `$077e` avoids a ninth-row GnGeo DDA artifact on
   one-tile objects.
2. S-ROM: tile zero remains transparent for BIOS FIX-map clears. The original
   8x8 pixels are encoded directly for FIX tiles 1 through 512.

The 2x expansion followed by the in-tile SCB2 shrink map is a lossless
sampling pair: the hardware selects one pixel from each duplicated 2x2 block.
A ROM-free
regression locks that transform and the zoom word. Color indices are also
preserved. `tools/gen_neogeo_palette.py` converts the named FCEUX 2.2.1
reference profile into the nearest representable Neo Geo color words, with at
most four 8-bit levels of per-channel error. See
[visual fidelity](VISUAL_FIDELITY.md) for the exact geometry and color gates.

The game also reads 314 bytes at CHR `$1ec0-$1ff9` to construct its title
nametable. Cartridge builds generate a small read-only C array for that
window from the user-supplied ROM. The normal ROM-less verification ELF links
a zero stub instead, and the Makefile uses distinct ELF outputs so one mode
cannot accidentally reuse the other's link product.

At runtime:

- 33 vertical SCB strips cover the scrolling 256-pixel background, including
  the incoming fine-scroll column. Each strip uses SCB3 height 33 to select
  the LSPC's 32-tile full-height mode; HUD-visible playfields are padded by
  three transparent physical rows so whole-chain `$7f` shrink preserves the
  intended 8-pixel row geometry.
- One persistent circular strip bank keeps the world-column/generation cache.
  Fine scrolling only changes one or two chain-driver SCB4 words, and crossing
  an eight-pixel boundary normally rebuilds one entering strip instead of all
  33.
- The stationary three-row SMB status bar uses FIX tiles.
- NES OAM entries use one-tile Neo Geo sprites.
- OAM priority-behind-background sprites are below the background strips;
  normal sprites are above them.
- Lower NES OAM indices receive higher Neo Geo sprite numbers so their
  priority remains in front.
- OAM is evaluated from entry zero upward and every in-range entry is sent to
  the Neo Geo in the same priority order. The port intentionally does not
  emulate the source hardware's eight-sprites-per-scanline dropout.
- Opaque FIX tiles mask the 32-pixel side borders, producing a centered
  256x224 viewport with the usual eight-pixel top/bottom overscan crop.

The Neo Geo global backdrop supplies NES background color zero. Because C-ROM
pen zero is transparent, a behind-background sprite naturally appears through
transparent NES background pixels and disappears under opaque ones.

## Object double buffering with a persistent background

The physical hardware layout is:

| Purpose | Hardware slots |
| --- | ---: |
| Behind-background OAM bank A | 64 |
| Behind-background OAM bank B | 64 |
| Persistent scrolling background | 33 |
| Front OAM bank A | 64 |
| Front OAM bank B | 64 |
| Total | 289 of 381 |

Only OAM is double-buffered. The 33 background strips remain resident and
form one circular sticky chain, split into two chains only when the ring wraps
around its physical slot zero. Its one or two driver SCB3 words briefly hide
the bank while changed SCB1 rows and SCB4 positions are committed, then reveal
it again in the same VBlank.

The next OAM bank's SCB1 and SCB4 state is prepared while its SCB3 entries are
hidden. The renderer stages its live SCB3 words in work RAM, then hides only
the old bank's active entries and reveals only the new bank's in-range/live
entries. SCB2 zoom values are initialized once for all 289 slots. Hidden-bank
SCB1/SCB4 data is written directly instead of compared against a work-RAM OAM
cache, and descending same-plane SCB3 runs share one address setup.

The renderer waits for a display interrupt observed after hidden-set
construction finishes. If construction crosses an earlier VBlank, it waits
for the following one rather than writing live state during active display.
The wait uses an atomic 16-bit signal while separate 32-bit counters retain
long-running cadence evidence.

Palette, sparse FIX changes, sparse background rows, OAM SCB3 state, and
background driver movement begin only after that fresh interrupt. A large
screen/configuration change affecting more than 64 background rows uses a
bounded bulk path: hide the background, upload it while invisible, wait for
one more VBlank, and then reveal it. The repeated display frame is preferable
to writing a visible strip during active scanout.

The following table belongs to the older, pre-audio, double-background
renderer milestone ELF (SHA-256
`8a3894ea0cf378d33eee451893fdce76c53e051f71252d6b3926a56fe2fda7d0`)
and is retained as historical evidence only. It was audited instruction by
instruction with these worst-case MC68000 cycle bounds, including a successful
VBlank-poll iteration:

| Live-update path | Worst-case cycles |
| --- | ---: |
| Clean palette, five changed HUD cells, and maximum SCB swap | 24,952 |
| All 51 palette words changed, no pending HUD scan, and maximum SCB swap | 24,842 |
| Split phase 1: all palette words plus all 96 HUD cells | 22,782 |
| Split phase 2: maximum SCB swap | 18,344 |

All paths stay below the 25,000-cycle post-handler working ceiling. A raw
NTSC VBlank is approximately 30,720 68000 cycles, so the ceiling reserves
more than 5,700 cycles for interrupt/BIOS work, recognition latency, and
hardware wait-state uncertainty. It does not characterize the current
persistent renderer or its enemy-heavy cadence.

## Native reference cadence and crowd behavior

An exact 67,677-record NTSC reference run was measured with FCEUX 2.2.1 and
cross-checked against FCEUX 2.6.5. On all 57,812 live gameplay frames
(`OperMode == 1` and task 3), the NMI count advanced once, the game's
`FrameCounter` advanced once, and FCEUX reported no lagged frame. The full run
contained 53 lag markers, all during boot or transitions; 46 were in
game-mode area-loading task 1. Mean CPU time was 29,780.500007 cycles per
video frame, approximately 60.0988 Hz.

Therefore native crowd pressure does not throttle gameplay logic in the
measured run. It causes first-eight-per-scanline sprite dropout and flicker.
The port instead sends every in-range OAM entry to the substantially larger
Neo Geo sprite engine: the target is one translated game tick per hardware
VBlank without intentionally reproducing that flicker. The nominal Neo Geo
and source NTSC refresh rates are slightly different, so this target
prioritizes stable hardware pacing over fractional double-tick jitter.

Enemy/object-buffer occupancy and source PPU rejection were:

| Active enemy slots | Live frames | Frames with rejected OAM entries |
| ---: | ---: | ---: |
| 0 | 5,683 | 0 |
| 1 | 6,713 | 1 |
| 2 | 19,713 | 3 |
| 3 | 13,179 | 131 |
| 4 | 7,902 | 487 |
| 5 | 4,530 | 386 |
| 6 | 92 | 0 |

Six active object slots do not imply overflow: the source hardware limit is
eight sprites intersecting one scanline, not six objects anywhere on screen.

The zero-based FM2 crowd regression windows are:

| Source frames | Stage | Reference behavior |
| --- | --- | --- |
| `3707..3712` | 1-2 | First early overflow onset |
| `3739..3751` | 1-2 | Five active slots and continuous rotating dropout |
| `19780..19809` | 3-2 | 30 consecutive overflow frames |
| `30384..30488` | 4-3 | 105 consecutive overflow frames |
| `32587..32620` | 4-4 | 34 consecutive overflow frames |

At frames 3739 through 3742, the rejected whole OAM-entry indices rotate as
`[56,57]`, `[36,37]`, `[58,59]`, and `[56,57]`. Every one of those frames
still advances exactly one native game tick.

The committed persistent-background baseline was measured at the stock
emulated MC68000 clock with host pacing, sound, VSync, and overclock disabled.
Each interval followed a 240-frame complete renderer/audio warmup, and three
independent launches reproduced the early values exactly:

| Inclusive source window | Native ticks / frames | Port ticks / VBlanks |
| --- | ---: | ---: |
| `3435..3554` | 120 / 120 | 120 / 226 |
| `11806..11925` | 120 / 120 | 120 / 225 |
| `3707..3751` | 45 / 45 | 45 / 90 |
| `3739..3751` | 13 / 13 | 13 / 26 |

Those ratios are the explicit optimization gate for the old software-culling
baseline. A light-load 120/120 cadence smoke does not override them. The
renderer screenshot at source frame 3742 was identical across all three
baseline runs.

The cacheless all-sprite renderer was then measured over the exact
`3435..3554` World 1-2 pipe window:

| Renderer configuration | Game ticks / VBlanks | Missed VBlanks |
| --- | ---: | ---: |
| Persistent-background baseline with software culling | 120 / 226 | 106 |
| All in-range sprites with the old OAM cache | 120 / 203 | 83 |
| Cacheless all-sprite renderer with batched SCB3 runs | 120 / 177 | 57 |
| Current integrated renderer and flattened collision scan | 120 / 174 | 54 |

The endpoint screenshot was byte-identical in all four runs, and the
translated game/RAM endpoint matched. The integrated result removes 52 of the
baseline's 106 missed display periods in this window. With the established
53-hold source
schedule, the CSV header and all 67,677 non-comment state rows also matched
byte-for-byte between baseline and all-sprite builds; the complete files
differ only in scheduling metadata. The new policy deliberately restores
sprites that the source PPU would reject on overflow frames while leaving the
translated gameplay state unchanged.

A field-major SCB1/SCB4 prototype reduced the modeled OAM register-store count
from 25,895 to 20,762 over this window but regressed real stock-clock cadence
to 120 / 197. It was rejected and is not present in the renderer. No assembly
replacement has been accepted without a measured stock-clock win.

Native hardware behavior is documented by the
[NES PPU frame timing](https://www.nesdev.org/wiki/PPU_frame_timing),
[sprite evaluation](https://www.nesdev.org/wiki/PPU_sprite_evaluation), and
[OAM](https://www.nesdev.org/wiki/PPU_OAM) references; the target display rate
is documented by the
[Neo Geo development wiki](https://wiki.neogeodev.org/index.php?title=Framerate).

## Native audio bridge

The Neo Geo link replaces the desktop PCM mixer with a compact native bridge.
The translated game still performs its normal APU register writes; the bridge
shadows the relevant channel state and derives YM2610 SSG and ADPCM-B
registers on ordinary game/audio frames plus bounded native-hardware catch-up
periods:

| Source voice | Native target |
| --- | --- |
| Pulse 1 | SSG tone A |
| Pulse 2 | SSG tone B |
| Triangle | Variable-rate looping ADPCM-B |
| Noise | SSG noise C |

Only changed target registers are sent. Pulse periods use 8.8 fixed-point
multiplication and shifts, while a 16-entry integer table maps noise periods.
Triangle timer writes calculate ADPCM-B Delta-N from a 64-sample period;
division therefore occurs only when a note timer byte changes, never in the
60 Hz emission loop. The bridge adds no runtime PCM buffers or floating
point.

Pulse amplitude also uses a 16-entry integer curve. The source pulse mixer
compresses its 0..15 range, while each fixed SSG step is approximately 3 dB;
direct passthrough therefore exaggerated the gap between music around 4..8 and
jump/fire effects at 14..15. The curve retains target level 8 for source level
8, maps 14/15 to 10, and leaves noise independent. This keeps capture headroom
while reducing pulse-effect peaks by up to five SSG steps.

Writes to the two source pulse-sweep registers are retained as compact
MC68000 state. Each 60 Hz bridge step clocks both sweep units twice, applies
the source divider/reload order, continuously mutes invalid targets, and
distinguishes pulse 1's one's-complement negate from pulse 2's two's-
complement negate. Hardware length counters also receive two half-frame
clocks; envelopes and the triangle linear counter receive four quarter-frame
clocks. Source halt/control bits, disabled length loads, immediate disable
clears, linear reload, and triangle timers zero through two are modeled. The
resulting state still passes through the changed-register coalescer and
generic Z80 command path.

The renderer may occasionally consume more than one display period.
`audio_cadence.c` detects that with an atomic 16-bit VBlank snapshot and clocks
only the native bridge units for the missed periods before the next ordinary
game frame. It leaves the newest period for that normal frame, handles counter
wrap, and caps catch-up at four steps; older debt increments an exported
diagnostic instead of forming a transport/recovery spiral. It intentionally
does not call the translated `SoundEngine()` out of band: that routine consumes
queues and changes `EventMusicBuffer`, which end-of-level gameplay also reads.
Music-note and software-effect duration counters therefore remain tied to
completed game frames.

The YM2610 is driven by the Z80 rather than directly by the MC68000. Each
13-bit register/value payload uses two commands from the 122-symbol
`$06-$7f` alphabet. The payload is split into base-121 quotient/remainder
digits and each digit is rotated relative to the preceding symbol. Adjacent
commands therefore always differ, including across packet boundaries, so an
old echoed acknowledgement cannot satisfy the next wait. Bit 7 remains
exclusive to acknowledgements. The Z80's 64-entry FIFO preserves order,
decodes the pair before the deferred YM write, and a full 18-register initial
flush uses 36 commands.

Command 3 resets the sound driver without an acknowledgement. The MC68000
allows eight game frames for startup and then sends one of two alternating
ready pings. A transport timeout invalidates the changed-register cache and
restarts this sequence. Two reset retries are allowed; a third consecutive
failure disables audio transport instead of hanging gameplay. The Z80 commit
handler calls nullsound's `ym2610_write_port_a`, retaining its required YM2610
delays and interrupt-safe port restoration.

The triangle sample is generated from source code rather than stored as a
copyrighted asset. A 64-point, DC-centered, +/-10,000 PCM triangle is repeated
2,048 times and deterministically encoded into the first 64 KiB of V1. The
remaining 448 KiB are zero-reserved. Independent decoding measures
`-10,078..10,081`, 47.534 PCM units of RMS error, and a repeat-reset seam one
PCM unit from the ideal adjacent slope. Its fixed V1 SHA-256 is checked before
normal and replay packaging. The bridge sets start block `$0000`, inclusive
stop block `$00ff`, stereo pan, volume `$70`, and START|REPEAT. It changes
Delta-N without retriggering while notes remain audible.

This is a major fidelity improvement, not a claim of source-chip exactness:

- SSG tones have a fixed 50-percent duty cycle, so pulse duty is not retained.
- The source short-noise mode bit is retained, but the SSG cannot reproduce
  that alternate sequence and slow noise periods saturate at 31.
- Direct DAC/DMC mixer bias is not modeled.
- Sweep control is advanced with two half-frame clocks, but changed SSG tone
  periods are emitted at the next 60 Hz bridge boundary rather than at a
  source-chip sub-frame instant.
- Music-note and software-effect duration sequencing remains game-frame-bound;
  only native hardware units receive missed-display-period catch-up.
- Frame-sequencer units are aggregated at the 60 Hz bridge boundary rather
  than exact source-chip sub-frame instants.
- The startup handshake proves that the command FIFO accepts input after the
  fixed delay; it is not a processed-ready response from the driver main loop.
- Physical hardware still needs to confirm the cartridge board exposes V1 to
  the YM2610 ADPCM-B bus and that the emulator-tuned level transfers cleanly.

The custom sound driver is packaged as a 128 KiB M1 region and the generated
triangle as a 512 KiB V1 region. Normal and rendered-replay GnGeo commands
enable sound explicitly.
`REPLAY_FAST=1` skips both hardware rendering and `apu_step_frame()`, so the
fast all-stage lane remains a core-progression test rather than audio
evidence.

GnGeo's remote-debug mode forcibly disables its sound/Z80 path, even when
`--sound` is requested. The debugger cadence and replay gates therefore
cannot observe audio transport. `tools/probe_neogeo_audio.py` launches a
separate normal-mode instance with fixed stock-clock and 60 Hz host-pacing
flags (`--autoframeskip --sleepidle --no-vsync`), an isolated
home/configuration and X display, explicit debugger disablement, active
gameplay input, and SDL's disk-audio driver. Signal detection is separate from
evidence: the hashed PCM interval starts only after gameplay activation is
accepted and contains exactly the requested number of audio frames. Finite
argument ceilings, per-command timeouts, an overall deadline, and a
child-process file-size limit bound the run while it is active. The probe
rejects empty or silent signed-16-bit stereo, records cartridge, GnGeo-data,
PCM-segment, and screenshot hashes in `result.json`, and deletes the raw PCM
after a successful check by default. A failed probe retains its bounded raw
capture with the logs for diagnosis.

### Emulator wall-clock pacing

The cartridge itself advances from the Neo Geo VBlank signal. On physical
hardware that signal is periodic, but the host emulator still has to throttle
its emulated frame loop. In the installed GnGeo revision, the wait for the
next 60 Hz deadline is inside `frame_skip()`, guarded by the
`autoframeskip` option. Consequently, `--no-autoframeskip` removes wall-clock
throttling entirely: gameplay speeds up when the host is idle and slows down
when it is busy even though both emulated CPU clock adjustments remain zero.

The interactive Make targets and normal-mode audio probe therefore use
`--autoframeskip --sleepidle --no-vsync --68kclock=0 --z80clock=0`.
Automatic frame skipping keeps game time at 60 Hz and may omit a host draw
only if the machine falls behind; `--sleepidle` avoids a busy wait, and
`--no-vsync` avoids adding a second display-dependent limiter. The debugger
cadence and replay tools use the opposite policy intentionally: their
unpaced, sound-disabled emulator validates guest work per emulated VBlank and
can finish deterministic evidence faster than real time. That evidence must
not be described as a wall-clock-speed measurement.

The current Z80 linker map reports:

| Measurement | Bytes |
| --- | ---: |
| Fixed Z80 code | 10,965 |
| Z80 static data | 1,945 |
| Data-to-stack headroom | 100 |

`tools/check_neogeo_sound_driver.py` runs as part of `verify`. It rejects
missing or inconsistent CODE/DATA summaries, code that does not start at
`$0000` or extends beyond the fixed `$8000` window, data that does not start
at `$f800` or reaches the `$fffd` stack start, and less than 64 bytes of stack
headroom. It also verifies the exact 128-entry packet command dispatch,
ready-ping state reset, decoder instructions, base-121 lookup table, and
exactly three bytes of driver-owned mutable DATA. Its parser and rejection
paths have Python unit coverage.

`tools/test_neogeo_apu_bridge.c` verifies initial mute, changed-register
coalescing, A4 and general period conversion, positive and negative sweep
targets, per-channel negate behavior, divider/reload cadence, sweep overflow
muting, envelope decay, hardware length/halt semantics, triangle linear reload
and low-timer muting, master disable/re-enable behavior, independent
ADPCM-B/noise registers, representative noise periods, coarse-before-fine
tone updates, the exact flagpole sweep start/first update, normalized
music/effect pulse levels, and dirty-register retry after a transport failure.
`tools/test_neogeo_audio_cadence.c` verifies no-op/single/multiple display
periods, 16-bit wrap, a catch-up step that crosses another VBlank, and the
bounded debt-drop path.
`tools/test_gen_neogeo_triangle_vrom.py` independently decodes the waveform
and fixes its dimensions, error bound, seam, padding, and SHA-256. These host
tests and emulator-oriented packaging checks do not replace listening tests
or electrical/timing validation on physical AES/MVS-compatible hardware.

## Measured memory and ROM size

`make -C platform/neogeo verify` currently reports:

| Measurement | Bytes |
| --- | ---: |
| MC68000 text + read-only data | 181,086 |
| Initialized work RAM (`.data`) | 4 |
| Zeroed work RAM (`.bss`) | 14,100 |
| Static user work RAM total | 14,104 |
| User-RAM limit below `$10f300` | 62,208 |
| Remaining stack/heap headroom | 48,104 |

For comparison, the unmodified desktop link's measured BSS was 545,556 bytes.
Most of that was its RGB framebuffer, opacity mask, decoded-tile cache, audio
buffers, save-state storage, and runtime CHR copy. None of those symbols is
present in the Neo Geo ELF.

The verifier fails if:

- the ELF is not MC68000;
- the actual translated `main`, program `data`, `ram`, or `nametable` has been
  optimized out;
- the ROM image is implausibly small;
- static user work RAM exceeds 48 KiB;
- framebuffer, CHR-copy, desktop, or software-float symbols are linked.

The 48 KiB guard is intentionally below the linker limit, preserving more
than 13 KiB for stack and future runtime state even if the port grows.

## Asset and cartridge pipeline

`tools/gen_neogeo_assets.py` accepts a raw iNES ROM or a ZIP containing
exactly one `.nes` member. It:

1. verifies SHA-1
   `ea343f4e445a9050d4b4fbac2c77d0693b1d0922`;
2. validates mapper 0, vertical mirroring, 2x16 KiB PRG, and 1x8 KiB CHR;
3. reads the CHR bank only;
4. generates 512 C-ROM and 512 S-ROM NES tiles plus transparent/solid helper
   tiles;
5. emits the 314-byte CHR-resident title nametable payload as an ignored C
   translation unit for the cartridge-only ELF;
6. pads C1/C2/S1 to cartridge sizes; and
7. records a manifest confirming that zero source PRG bytes were written.

The Makefile then builds the one-megabyte native P1, custom Z80 sound-driver
M1, generated triangle V1, and full C/S regions. `make cart` packages those
full regions as the authoritative `smbneo.zip`; `make hardware-cart` is an
explicit alias for that same physical-cartridge layout.

The canonical build also generates `gngeo_data.zip` with one custom
`rom/smbneo.drv` entry. A validator checks the title, full region sizes,
filenames, destinations, and CRCs against the native ROM files. The ordinary
`make run` path therefore launches the project as `smbneo`, not through a
donor database entry. The cartridge header also uses the project-specific
unofficial NGH value `0x534d` instead of ngdevkit's generic default.

`make mame-cart` pairs `smbneo.zip` with a generated
`build/mame/hash/neogeo.xml` containing the unique `smbneo` software entry,
the visible title **Super Mario Bros. Neo**, full region sizes, generated
hashes, and explicit MAME loading semantics. MAME therefore never needs to
describe the canonical build as another game.

`make compat-cart` separately generates the optional `puzzledp.zip` profile
for fixed-database frontends. It retains the first 512 KiB of P1 and first
1 MiB of each C ROM only after proving that every omitted byte is native
`FF`/zero padding. The six expected driver CRCs are reached by changing only
the last four bytes of verified padding tails; P1 remains at offset zero for
`load16_word_swap`. Exact loader paths and the separate `neogeo.zip`
requirement are documented in
[`EMULATOR_COMPATIBILITY.md`](EMULATOR_COMPATIBILITY.md).

A shared V1 target encodes the 64 KiB ADPCM-B loop, pads the image to 512 KiB,
and checks SHA-256
`c52017058a226a44506a5d94fc1f692b42fe818302761da64d4f7adc5e5928a7`.
Normal and replay recipes copy and recheck that identical artifact before
packaging, so a correctly sized stale or zero-filled V1 cannot survive an
incremental build. All derived assets live below the ignored
`platform/neogeo/build/` directory.

For the renderer/replay milestone before native audio, two isolated builds
produced identical
1 MiB P regions with SHA-256
`d8ea97f3e05846467d298e9287bbf49567cc2799c8d16d8cf2aeac1153046b50`
and identical 105,220-byte cartridge ZIPs with SHA-256
`352e4e0272a59d10e7f52014196ed2a35dac5a90a5a08e56ac1a58f3818b98fb`.

`tools/check_reproducible_cart.py` performs two complete cartridge builds in
different owned temporary directories. It validates the P/C1/C2/S/M/V region
sizes, the asset-clean manifest, the exact optional compatibility profile,
the full native contents of `smbneo.zip`, the generated custom GnGeo driver,
and the unique canonical MAME software-list semantics. It requires
byte-for-byte equality of every region, both cartridge ZIPs,
`gngeo_data.zip`, and the generated `neogeo.xml`. It never cleans or writes
the normal `platform/neogeo/build/` directory.

`tools/rec_tool.py` retains the input movie's exact frame count, source hash,
initial reset command, and RAM-initialization provenance. Movies from
[FCEUX 2.2.1](https://github.com/TASEmulators/fceux/blob/fceux-2.2.1/src/fceu.cpp)
without a `RAMInitOption` are labeled as legacy option 0, matching that
version's deterministic `00 00 00 00 ff ff ff ff` power-on pattern; explicit
zero-fill option 2 is also supported. Fill-FF, random, malformed, or ambiguous
initialization is rejected instead of being silently imported.

## Core-state transcript and source-frame scheduling

An FM2 row is a source video-frame record, not a promise that the game CPU
received an NMI on that row. The original reset path also spends several video
frames before its first game NMI, while the translated `Start()` routine runs
synchronously. An exact FCEUX 2.2.1 comparison measured the resulting adapter:

- translated row 0 aligns with reference/FM2 row 7;
- the first seven FM2 inputs are retained without advancing the C core; and
- one no-NMI input hold is inserted on each continuous game-mode,
  area-initialization task-0 entry.

The replay cartridge defaults are therefore
`REPLAY_BOOTSTRAP_FRAMES=7` and `REPLAY_AREA_INIT_HOLD_FRAMES=1`. They are
compile-time parameters rather than hidden constants. Debugger mailbox version
4 reports both parameters, the number of area holds actually consumed, and the
number of core frames actually advanced. The host runner rejects internally
inconsistent frame, tail, bootstrap, hold, or core-advance accounting before
classifying a pass.

The cartridge adapter is intentionally small. For instruction-level drift
diagnosis, `tools/core_state_trace.py` uses the reference emulator's explicit
lag markers instead of inferring them from already-divergent C state. FCEUX
marks the row after an extra source boundary as lagged, so the tool holds the
immediately preceding source row. It consumes one optional reference
lookahead row to make the final hold decision.

The original 6502 program also contains a few indexed table reads whose valid
indices extend into physically adjacent tables or instruction bytes. Packing
only declared `.db` data into C silently changed those reads. The
`.rom_fallthrough` lowering directive now retains such bytes as physical
storage without pretending they enlarge the logical table. Generator and
native regressions cover the fireball direction byte, full-byte bubble
scratch index, firebar mirror index, and flying-enemy random windows.

A bounded comparison can be produced with local ROM and movie files:

```bash
# Print, but do not execute, the exact reference environment and argv.
python3 tools/core_state_trace.py fceux-command \
  --fceux /path/to/fceux-2.2.1 \
  --rom /path/to/owned/smb.nes \
  --fm2 /path/to/no-opposite-warpless.fm2 \
  --output /tmp/smb-reference-000000-004101.csv \
  --frames 4102

# After executing that printed reference command:
python3 tools/core_state_trace.py emit-translated \
  --fm2 /path/to/no-opposite-warpless.fm2 \
  --output /tmp/smb-translated-000007-004100.csv \
  --input-frame-offset 7 \
  --frames 4094 \
  --hold-schedule-reference /tmp/smb-reference-000000-004101.csv

python3 tools/core_state_trace.py compare \
  --translated /tmp/smb-translated-000007-004100.csv \
  --reference /tmp/smb-reference-000000-004101.csv \
  --reference-frame-offset 7 \
  --skip-scheduled-holds \
  --result-json /tmp/smb-state-comparison.json
```

Every output path is create-only: the Python command, Lua extractor, and
optional native RAM dump refuse to overwrite an existing file. A complete
CSV carries an explicit schema, contiguous frame numbers, frame semantics,
source metadata, and a final completion marker. The pass domain compares
controller input, OAM, and selected persistent gameplay fields. Whole-RAM,
zero-page, stack, and work-buffer hashes remain diagnostic because the static
C translation deliberately does not model the instruction stack and retains
some transient buffers differently. Reference traces, RAM dumps, ROMs, and
FM2 files are external evidence and are not tracked.

The printed reference command records the emulator executable's SHA-256 and
runs it through `exec-fceux-verified`. That wrapper reopens and hashes the
binary immediately before executing the same verified file descriptor, so a
later pathname replacement cannot be mislabeled as the measured build. The
Lua side requires and embeds the matching label and digest. If a streamed
write itself fails, its exclusively created partial file is deliberately
retained; the missing completion marker makes it invalid evidence and avoids
unsafe pathname cleanup.

## Published TAS regression lanes

FM2 is a useful interchange format here because its text input log contains
one `RLDUTSBA` controller record per emulated frame. The importer accepts
UTF-8 or legacy single-byte metadata, but requires every input record to be
strict ASCII and validates the exact game checksum, startup mode, controller
ports, emulator commands, frame count, and raw file hash.

Four external movies provide complementary regression lanes:

| Lane | Source and authors | Local FM2 records | Intended gate |
| --- | --- | ---: | --- |
| Published warpless | [HappyLee and Mars608, 18:36.78](https://tasvideos.org/3728M) | 67,117 | Definitive published all-32-stage core oracle |
| No-opposite warpless user file | [yizhihongzhunan, 18:36.877 RTA](https://tasvideos.org/UserFiles/Info/637779016362867321) | 67,677 | Preferred stock-controller-expressible all-stage oracle |
| Published warp run | [HappyLee, 04:57.31](https://tasvideos.org/1715M) | 17,868 | Famous short synchronization smoke; intentionally cannot pass the sequential-stage gate |
| No-opposite warp user file | [zdoroviy_antony, 17,882 frames](https://tasvideos.org/UserFiles/Info/56655800614738568) | 17,882 | Short stock-controller-expressible synchronization smoke |

The published warpless movie is the strongest provenance/reference run and
was replayed on an original console, but its documented 6-2 route includes
simultaneous `L+D+R`. The no-opposite warpless user file is therefore the
better input for the Neo Geo joystick policy. It is still a frame-perfect
tool-assisted movie, not evidence of ordinary human execution.

The exact locally inspected downloads had these SHA-256 values:

| Movie | Raw FM2 SHA-256 |
| --- | --- |
| Published warpless | `a9a3403b639cd30bd06d721ebb44449555ac64f979d95b3ce1c77642bc4ba423` |
| No-opposite warpless | `c9afd9d1d6ee7abbeacf1ae32a74cd26fae2b42109815bf7bbbdccc253111f9b` |
| Published warp run | `66f28af696f95f642ae962829d51a4d7071ee3255cf39b277c84c6f3ff6e191b` |
| No-opposite warp run | `fc5fde2e256b3a6c3765d317a648099537ff860bdd890160efd61104f11dcff4` |

TASVideos asks callers to link to the publication/user-file page instead of
hotlinking the download. The repository follows that rule and does not
contain any of these movies. Download a text `.fm2` manually from its page,
retain author attribution, and build the local gate with:

```bash
make -C platform/neogeo replay-cart \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1

python3 tools/run_neogeo_replay_gate.py \
  --68k-overclock 10000 \
  --timeout 2400

# Direct-renderer endurance lane. This target defaults to stock MC68000
# timing and a conservative 7,200-second host deadline.
make -C platform/neogeo replay-rendered-evidence \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_HARDWARE_PLAYABLE=1 \
  REPLAY_EVIDENCE_DIR="/tmp/smb-neogeo-rendered-evidence"
```

The rendered lane requires `scrot` and the Python Pillow package. The
documented Make target checks both dependencies, selects the rendered build,
and enforces the hardware-playable/no-opposite input policy before beginning
the long emulator run. `REPLAY_EVIDENCE_DIR` should name a new external
directory; a path below `/tmp` keeps ROM-derived captures and replay evidence
outside the repository.

The generated header stores compact `uint16_t` durations and `uint8_t`
controller states rather than 67,000 padded structures. It also embeds the raw
FM2 SHA-256, canonical imported-recording SHA-256, exact input frame count,
initial command, RAM initialization option/seed, direction policy, and
opposite-direction count. The ignored gate build has its own ELF, map,
cartridge, and asset directory, so it cannot contaminate the playable build.

`REPLAY_FAST=1` skips both the hardware renderer and `apu_step_frame()` and is
appropriate for the translated-core progression gate. Omitting it exercises
normal rendering and emulated VBlank accounting but is not a substitute for
the separate guest VBlank-budget measurement or an interactively paced run.
The runner accepts a result only
when it reaches the pass trap with both 32-stage masks complete. Failure,
incomplete playback, invalid mailbox data, debugger/emulator exit, occupied
debug port, and timeout all remain non-passing and produce a bounded
`result.json` plus logs. A cartridge-side checkpoint exposes an intermediate
mailbox every 1,800 frames; the debugger therefore does not need to stop and
round-trip on every frame.

The rendered-evidence lane adds two distinct traps per newly entered stage.
The first records the immediate transition state. The second fires after two
additional calls have completed the direct renderer and its VBlank swap. Its
version-4 mailbox identifies the rendered build and records the direct
renderer game-frame count, VBlank count, configured settle interval, and
16-bit uploaded/presented render generations.
Before a rendered pass is accepted, the host requires:

- a hardware-playable FM2 with zero opposite-direction transitions;
- exactly 32 ordered transition/settled pairs with prefix entry/completion
  masks and matching world/level coordinates;
- renderer game frames equal to translated core frames at every checkpoint;
- at least one VBlank per rendered frame, with an exact two-rendered-frame
  source/core/game-frame and modulo-65,536 render-generation delta between
  each transition and settled pair;
- equal uploaded and presented generations at every screenshot-bearing trap;
- 32 valid diagnostic transition PNGs plus 32 non-blank settled-stage PNGs,
  with every settled playfield distinct from its paired transition and no two
  consecutive settled stages pixel-identical; and
- a valid terminal PNG tied to the existing 60-frame stable victory state.

The synchronous screenshot helper writes through a temporary PNG, validates
its signature, dimensions, and per-file size bound, then atomically publishes
it. The correctness boundary is inside the cartridge: after uploading the
live sprite/FIX/palette state, the renderer increments a 16-bit generation;
the following VBlank callback latches it as presented, and screenshot traps
wait for equality without advancing the translated core or renderer frame.
Those two shared words and one 16-bit callback copy also exist in normal
cartridges and are included in the measured 14,100-byte BSS above.

Immediately before invoking `scrot`, the host also applies a bounded display
settling allowance: 50 milliseconds by default, configurable from 0 through
0.25 seconds with `--display-settle-seconds`. This does not establish frame
identity; it only lets the already-issued SDL/X11 presentation reach the X
window while the debugger is stopped. The selected allowance is validated and
recorded in `result.json` and adds no cartridge work or memory. The final
manifest records both file and centered 320x224 pixel hashes.
It also records a playfield-only hash below the 32-pixel HUD region and
requires the terminal playfield to differ from the settled 8-4 entrance.
The evidence directory also contains immutable, hashed snapshots of the
exercised artifacts and the runner/capture/debugger provenance needed to bind
the result to what actually ran. `result.json` has a hard 128 KiB bound.
Repository-local generated builds remain ignored; ROM-derived screenshots and
the evidence snapshot instead stay in the explicitly selected external
directory. GnGeo debugger mode disables its Z80/audio execution, so this lane
proves direct-renderer endurance plus progression, not audio, pixel-perfect
equivalence to the source console, or operation on physical hardware.

### Current replay result

On 2026-07-24, the preferred no-opposite 67,677-frame warpless movie passed
the fast cartridge gate through every stage and the final victory state:

| Measurement | Terminal value |
| --- | ---: |
| Cartridge frame | 68,631 |
| Source replay tail frame | 954 |
| Stages entered | 32 (`0xffffffff`) |
| Stages completed | 32 (`0xffffffff`) |
| Stable victory frames | 60 |
| Bootstrap frames skipped | 7 |
| Area-initialization holds | 46 |
| Translated core frames advanced | 68,579 |
| Opposite-direction transitions | 0 |

The terminal mailbox had valid version-3 metadata, identified the
rendering-disabled fast build, retained the exact source frame count and
hardware-playable direction policy, passed all host-side accounting checks,
and reached the dedicated pass trap. The bounded 6,816-byte `result.json` had
SHA-256
`223bd9f04b6f4b3842dc0c6a880e307622007a526347185bcba8d4b84afed393`.
The exercised replay ELF, cartridge ZIP, and P region had SHA-256 values
`559fa54a5896c754dbc12da2ed1e33895a5cecfd0b9b0a96c7534b2ec7727e67`,
`b65b03858dcc7e108dcef89869534c95630dc52311f869933a1a2869c86484f4`,
and
`409b8234902d48387d8edd08430925d1a71992be6071dfa1c6f64752468ce04f`,
respectively.

The same movie also completed the direct-renderer lane with the stock emulated
MC68000 cycle budget under the earlier version-3 protocol. That run remains evidence
of full-game renderer endurance and ordered progression, but its screenshots
were captured one host presentation behind their mailbox state. The following
figures are therefore retained as historical endurance measurements, not as
current state-bound image evidence:

| Measurement | Terminal value |
| --- | ---: |
| Cartridge frame | 68,631 |
| Source replay tail frame | 954 |
| Stages entered | 32 (`0xffffffff`) |
| Stages completed | 32 (`0xffffffff`) |
| Translated core frames advanced | 68,579 |
| Direct-renderer game frames | 68,579 |
| VBlanks | 101,178 |
| Transition captures | 32 |
| Two-frame settled captures | 32 |
| Stable-victory captures | 1 |
| Opposite-direction transitions | 0 |

All 32 ordered transition/settled pairs had exact source/core/renderer frame
deltas of two and a VBlank delta of seven. The 65 decoded PNGs and all nine
immutable provenance artifacts passed the then-current version-3 validator.
The bounded 78,356-byte `result.json` had SHA-256
`7a67f2fb11bd646e7f160c3b8522ba853be819bcaff96b4bff899a90d1ebfd65`.
The exercised rendered ELF and cartridge had SHA-256 values
`d4d87d31520f72aafcfe27f3e59c32419a682f4498e92a4f3a0b9fad559c62d7`
and
`09f21de1da091bf871b9cd931cd5389123c7f5da1ece6c7e94c23f214afea77d`,
respectively.

The corrected version-4 presentation fence then passed a bounded first-stage
regression. Its transition mailbox reported source/core/render generation
550/543/543 and its settled mailbox reported 552/545/545. Uploaded and
presented generations were equal at both traps, the two-frame modulo-65,536
delta passed, and the settled PNG visibly contained the player sprite that
the stale version-3 image omitted. The corrected PNG has SHA-256
`6966034a78426eea51d7b3e162af2e95970d8f32e480a1fa0548232e79429e36`.
The bounded run intentionally timed out after collecting that pair, so a new
32-stage version-4 rendered pass remains outstanding.

The fast lane is a translated-core progression proof. The historical rendered
run adds stock-clock renderer endurance; the version-4 smoke proves the new
state-bound presentation mechanism at the first stage. Neither is a
pixel-perfect source-console, audio, or physical-hardware claim. The normal
rendered cartridge is covered separately by the stock-clock cadence probes,
and the corrected renderer still awaits its follow-up MV1C run.

## Verification performed

The milestone is checked with:

```bash
# Uses MAME's LSPC implementation and captures title/gameplay frames.
make mame-capture SMB_ROM="/path/to/smb.zip"

# Includes isolated MoonBit lowering/transpiler tests, generated-C comparison,
# native audio bridge tests, and MC68000 plus Z80 link/map guards.
make -C platform/neogeo verify
make -C platform/neogeo cart \
  SMB_ROM="/path/to/smb.zip"
make -C platform/neogeo hardware-cart \
  SMB_ROM="/path/to/smb.zip"
make -C platform/neogeo mame-cart \
  SMB_ROM="/path/to/smb.zip"
python3 tools/check_reproducible_cart.py \
  --rom "/path/to/smb.zip"
# Must run without GnGeo debugger mode so its Z80/audio path executes.
python3 tools/probe_neogeo_audio.py \
  --evidence-dir /tmp/smb-neogeo-audio-evidence
python3 tools/rec_tool.py validate rec/warpless.rec \
  --expect-end-frame 7987 \
  --expect-transition-count 509
python3 tools/measure_neogeo_cadence.py \
  --warmup-vblanks 120 \
  --sample-vblanks 120 \
  --assert-zero-missed
python3 tools/measure_neogeo_cadence.py \
  --active-motion \
  --warmup-vblanks 300 \
  --sample-vblanks 120 \
  --assert-zero-missed

make -C platform/neogeo replay-cart \
  SMB_ROM="/path/to/smb.zip" \
  REPLAY_FM2="/path/to/no-opposite-warpless.fm2" \
  REPLAY_FAST=1 REPLAY_HARDWARE_PLAYABLE=1
python3 tools/run_neogeo_replay_gate.py \
  --68k-overclock 10000 --timeout 2400
```

The current `verify` result includes a passing native audio bridge regression
and Z80 map-checker regression. It also links the custom sound driver and
reports 10,965 bytes of Z80 fixed code, 1,945 bytes of static data, and 100
bytes of data-to-stack headroom. Both cartridge profiles package that driver
as M1 and the hash-checked generated triangle as V1; the reproducibility lane
checks the native regions and both final archives. The M1 image and linker map
are grouped build outputs;
changes to the nullsound library or included command helper trigger a relink,
and the map checker runs on every verification or cartridge invocation.

The cadence probe owns an isolated X display and process groups, verifies
that the fixed debugger listener belongs to its launched emulator, and has a
finite sampling deadline. It disables host pacing so its counters measure
whether one guest game frame fits each emulated VBlank budget rather than how
fast the host happens to run. Its `result.json` records that policy plus the
ELF, cartridge, and GnGeo-data SHA-256 values together with the full
timing/sample arguments, so the measurements remain tied to the binaries that
were exercised.

The supplied ZIP contained one 40,976-byte iNES file with the supported
SHA-1. The generated canonical hardware cartridge loaded all P/M/V/S/C
regions in ngdevkit-gngeo, initialized the AES BIOS and 68000, and produced
stable 640x448 captures (2x scale) showing:

- the large centered title panel and complete one/two-player menu;
- the centered/cropped 256x224 viewport;
- the fixed `MARIO / WORLD / TIME` HUD;
- sky, cloud, mountain, bush, and ground background tiles;
- Mario and other OAM sprites; and
- live palette/scroll updates across successive frames.

The earlier bounded light-load probe completed 89 game frames during its
first 120 emulated display periods while caches warmed, followed by 120/120.
An active-input smoke completed 269/300 during warmup and then 120/120. These
remain useful build and ordinary-scene checks, but they predate the exact
enemy-heavy windows above and must not be generalized into a full-game
one-tick-per-VBlank claim. All cadence probes measure guest work per emulated
VBlank rather than host wall-clock rate; interactive and audio runs exercise
the separate real-time pacing policy.

Emulator captures and generated ROMs are verification artifacts only and are
not tracked. MAME reproduced the two failures visible in the initial MV1C
footage: a tiled FIX screen during BIOS handoff and truncated background strips
that left the ground missing and extended black columns below solid tiles.
Reserving transparent FIX tile zero and using the LSPC 32-tile full-height
chain removed both failures in the same MAME lane. The corrected cartridge
still requires an MV1C retest.

The normal-mode audio probe establishes non-silent emulator PCM from active
gameplay in addition to the sound-state, linkage, and packaging gates. It does
not establish subjective fidelity, electrical timing, or operation on
physical hardware.

## Next engineering steps

1. Reach one tick per stock-clock VBlank in every enemy-heavy regression
   window while rendering every in-range OAM entry.
2. Retest the corrected cartridge on the MV1C, then broaden audio/video testing
   to other AES/MVS-compatible hardware, confirm the ADPCM-B V1 bus mapping,
   and tune output level plus visible-area offsets.
3. Evaluate pulse-duty and short-noise approximations without consuming the
   remaining Z80 stack margin.
4. Tighten the remaining fine-scroll left-edge masking cases.
5. Extend the ROM-free pixel-transform and palette checks to more composite
   sprite poses without tracking game-derived captures.
6. Add memory-card or save-state support.
