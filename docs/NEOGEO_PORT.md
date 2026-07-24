# Neo Geo port architecture

## Milestone status

The current milestone is a bootable, playable, performance-verified Neo Geo
cartridge. It runs the upstream translated C game core on the MC68000, uses
Neo Geo graphics hardware directly, reads active-low Neo Geo controller
registers directly, and packages a cartridge from a user-owned SMB dump.

Completed:

- Pure C target; no C++ runtime or desktop framework in the Neo Geo link.
- MC68000 code generation (`-m68000 -mlra`) with `-O3`, LTO, and measured
  inlining of the translated core's hot instruction helpers.
- Direct background, OAM sprite, palette, FIX HUD, and input backends.
- Generation-tracked background/HUD/palette caches, rotating hidden
  background strips, batched SCB4 movement, and active-entry SCB3 swaps.
- Hidden sprite-set construction and cycle-budgeted, next-VBlank-only
  live-state updates.
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
- Sustained one-game-frame-per-VBlank cadence at stock emulated clock after
  the cold-start cache fill.
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
- Real AES/MVS or flash-cartridge validation.
- Cycle/scanline-accurate PPU behavior. This port intentionally follows the
  upstream frame-at-a-time timing model.
- Exact NES left-edge masking in every fine-scroll case.
- Save state or memory-card support.

## Why this renderer fits Neo Geo

The desktop PPU turns every NES frame into a 256x240 RGB bitmap. That is a poor
fit for the Neo Geo's roughly 64 KiB user work-RAM window and would also spend
the 12 MHz 68000 on work already handled by the sprite hardware.

The port converts each NES 8x8 2bpp CHR tile in two ways at build time:

1. C-ROM: pixels are expanded 2x to a 16x16 4bpp Neo Geo tile. SCB2 shrink
   value `$077f` displays it at 8x8.
2. S-ROM: the original 8x8 pixels are encoded directly for the FIX layer.

The game also reads 314 bytes at CHR `$1ec0-$1ff9` to construct its title
nametable. Cartridge builds generate a small read-only C array for that
window from the user-supplied ROM. The normal ROM-less verification ELF links
a zero stub instead, and the Makefile uses distinct ELF outputs so one mode
cannot accidentally reuse the other's link product.

At runtime:

- 33 vertical SCB strips cover the scrolling 256-pixel background, including
  the incoming fine-scroll column.
- Each hidden set keeps a world-column/generation cache. Fine scrolling only
  changes SCB4, and crossing an eight-pixel boundary normally rebuilds one
  entering strip instead of all 33.
- The stationary three-row SMB status bar uses FIX tiles.
- NES OAM entries use one-tile Neo Geo sprites.
- OAM priority-behind-background sprites are below the background strips;
  normal sprites are above them.
- Lower NES OAM indices receive higher Neo Geo sprite numbers so their
  priority remains in front.
- A small per-scanline counter keeps accepted OAM sprites at the NES limit of
  eight, leaving ample room under the Neo Geo's 96-strip scanline limit.
- Opaque FIX tiles mask the 32-pixel side borders, producing a centered
  256x224 viewport with the usual eight-pixel top/bottom overscan crop.

The Neo Geo global backdrop supplies NES background color zero. Because C-ROM
pen zero is transparent, a behind-background sprite naturally appears through
transparent NES background pixels and disappears under opaque ones.

## Double buffering without a framebuffer

Each hardware frame set contains:

| Purpose | Sprite slots |
| --- | ---: |
| Behind-background OAM bank | 64 |
| Scrolling background strips | 33 |
| Front OAM bank | 64 |
| Total per set | 161 |

Two sets consume 322 of the Neo Geo's 381 displayable sprite slots. SCB1,
SCB2, and SCB4 for the next set are written while its SCB3 height is zero.
Only 161 SCB3 words (322 bytes) are staged in work RAM. During VBlank the
renderer uploads changed palette/FIX entries, hides only the old set's active
background and OAM entries, and reveals only the new set's active entries.
SCB2 zoom values are initialized once; all 33 background X positions use one
sequential SCB4 transfer when they change.

The renderer waits for a display interrupt observed after hidden-set
construction finishes. If construction crosses an earlier VBlank, it waits
for the following one rather than writing live state during active display.
The wait uses an atomic 16-bit signal while separate 32-bit counters retain
long-running cadence evidence.

The live-update scheduler also bounds the work performed after that wait.
Palette-only updates share the sprite-set swap. With a clean palette, a HUD
update affecting at most five FIX cells also shares it. A palette update
coinciding with a HUD scan, or a HUD update affecting six or more cells,
uploads the palette/FIX state in one display period and waits for the next
VBlank before swapping SCB3. The repeated display frame is preferable to
crossing into active scanout.

The renderer milestone ELF from before native audio was added (SHA-256
`8a3894ea0cf378d33eee451893fdce76c53e051f71252d6b3926a56fe2fda7d0`)
was audited instruction by instruction with these worst-case MC68000 cycle
bounds, including a successful VBlank-poll iteration:

| Live-update path | Worst-case cycles |
| --- | ---: |
| Clean palette, five changed HUD cells, and maximum SCB swap | 24,952 |
| All 51 palette words changed, no pending HUD scan, and maximum SCB swap | 24,842 |
| Split phase 1: all palette words plus all 96 HUD cells | 22,782 |
| Split phase 2: maximum SCB swap | 18,344 |

All paths stay below the 25,000-cycle post-handler working ceiling. A raw
NTSC VBlank is approximately 30,720 68000 cycles, so the ceiling reserves
more than 5,700 cycles for interrupt/BIOS work, recognition latency, and
hardware wait-state uncertainty. This is a hardware-oriented bound, not a
substitute for the still-pending measurement on real AES/MVS-compatible
hardware.

## Native audio bridge

The Neo Geo link replaces the desktop PCM mixer with an integer-only bridge.
The translated game still performs its normal APU register writes; the bridge
shadows the relevant channel state and, once per rendered game frame, derives
the YM2610 SSG registers:

| Source voice | Native target |
| --- | --- |
| Pulse 1 | SSG tone A |
| Pulse 2 | SSG tone B |
| Triangle | SSG tone C |
| Noise | SSG noise gated through channel C |

Only changed target registers are sent. Pulse and triangle timer periods use
8.8 fixed-point multiplication and shifts, while a 16-entry integer table
maps noise periods. Software envelope state is clocked four quarter-frame
times per game frame. The bridge therefore adds no PCM buffers, floating
point, or division to the MC68000 frame loop.

Writes to the two source pulse-sweep registers are retained as compact
MC68000 state. Each 60 Hz bridge step clocks both sweep units twice, applies
the source divider/reload order, continuously mutes invalid targets, and
distinguishes pulse 1's one's-complement negate from pulse 2's two's-
complement negate. The resulting tone-period changes still pass through the
existing changed-register coalescer and generic Z80 command path.

The YM2610 is driven by the Z80 rather than directly by the MC68000. Each SSG
register update uses three commands below `$80`: a register selector, a high
data nibble, and a low data nibble that commits the write. The MC68000 waits
for the nullsound `command | $80` acknowledgement after every byte. Distinct
command classes prevent the previous acknowledgement from satisfying the
next wait. The Z80's 64-entry FIFO preserves order, and a full initial flush
uses 33 commands.

Command 3 resets the sound driver without an acknowledgement. The MC68000
allows eight game frames for startup and then sends one of two alternating
ready pings. A transport timeout invalidates the changed-register cache and
restarts this sequence. Two reset retries are allowed; a third consecutive
failure disables audio transport instead of hanging gameplay. The Z80 commit
handler calls nullsound's `ym2610_write_port_a`, retaining its required YM2610
delays and interrupt-safe port restoration.

This is the native audio MVP, not a claim of source-chip fidelity:

- SSG tones have a fixed 50-percent duty cycle, so pulse duty is not retained.
- Hardware length counters, the triangle linear counter, short-noise mode,
  and direct DAC/DMC behavior are not implemented.
- Sweep control is advanced with two half-frame clocks, but changed SSG tone
  periods are emitted at the next 60 Hz bridge boundary rather than at a
  source-chip sub-frame instant.
- Triangle is represented by a square tone. Very low triangle periods clamp to
  the SSG's 12-bit maximum.
- Triangle and noise share SSG C and one volume. When both are enabled, the SSG
  AND-gates them rather than mixing them as independent voices.
- The startup handshake proves that the command FIFO accepts input after the
  fixed delay; it is not a processed-ready response from the driver main loop.

The custom sound driver is packaged as a 128 KiB M1 region. V1 is still a
512 KiB zero-filled region because this milestone uses no ADPCM samples.
Normal and rendered-replay GnGeo commands enable sound explicitly.
`REPLAY_FAST=1` skips both hardware rendering and `apu_step_frame()`, so the
fast all-stage lane remains a core-progression test rather than audio
evidence.

GnGeo's remote-debug mode forcibly disables its sound/Z80 path, even when
`--sound` is requested. The debugger cadence and replay gates therefore
cannot observe audio transport. `tools/probe_neogeo_audio.py` launches a
separate normal-mode instance with fixed stock-clock/no-autoframeskip flags,
an isolated home/configuration and X display, explicit debugger disablement,
active gameplay input, and SDL's disk-audio driver. Signal detection is
separate from evidence: the hashed PCM interval starts only after gameplay
activation is accepted and contains exactly the requested number of audio
frames. Finite argument ceilings, per-command timeouts, an overall deadline,
and a child-process file-size limit bound the run while it is active. The
probe rejects empty or silent signed-16-bit stereo, records cartridge,
GnGeo-data, PCM-segment, and screenshot hashes in `result.json`, and deletes
the raw PCM after a successful check by default. A failed probe retains its
bounded raw capture with the logs for diagnosis.

The current Z80 linker map reports:

| Measurement | Bytes |
| --- | ---: |
| Fixed Z80 code | 10,777 |
| Z80 static data | 1,944 |
| Data-to-stack headroom | 101 |

`tools/check_neogeo_sound_driver.py` runs as part of `verify`. It rejects
missing or inconsistent CODE/DATA summaries, code that does not start at
`$0000` or extends beyond the fixed `$8000` window, data that does not start
at `$f800` or reaches the `$fffd` stack start, and less than 64 bytes of stack
headroom. Its parser and every rejection path have Python unit coverage.

`tools/test_neogeo_apu_bridge.c` verifies initial mute, changed-register
coalescing, A4 and general period conversion, positive and negative sweep
targets, per-channel negate behavior, divider/reload cadence, sweep overflow
muting, envelope decay, master disable/re-enable behavior, triangle/noise
mixer state, representative noise periods, coarse-before-fine tone updates,
and dirty-register retry after a transport failure. These host tests and the
emulator-oriented packaging checks do not replace listening tests or
electrical/timing validation on physical AES/MVS-compatible hardware.

## Measured memory and ROM size

`make -C platform/neogeo verify` currently reports:

| Measurement | Bytes |
| --- | ---: |
| MC68000 text + read-only data | 214,614 |
| Initialized work RAM (`.data`) | 4 |
| Zeroed work RAM (`.bss`) | 16,172 |
| Static user work RAM total | 16,176 |
| User-RAM limit below `$10f300` | 62,208 |
| Remaining stack/heap headroom | 46,032 |

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

The Makefile then builds the one-megabyte P1, custom Z80 sound-driver M1,
zero-filled V1, cartridge ZIP, and GnGeo hash data. The V1 region is present
for a complete cartridge layout but carries no samples in the SSG-only audio
MVP. Each cartridge recipe recreates V1 from zero bytes and checks its fixed
SHA-256 before packaging, so a correctly sized stale file cannot survive an
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
sizes, the asset-clean manifest, and byte-for-byte equality of every region
and the final cartridge ZIP. It never cleans or writes the normal
`platform/neogeo/build/` directory.

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
normal rendering but takes display time and is not a substitute for the
separate stock-clock cadence measurement. The runner accepts a result only
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
cartridges. Existing linker padding absorbs the words, so the measured final
BSS remains 16,172 bytes.

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

The same movie also completed the direct-renderer lane at the stock emulated
MC68000 clock under the earlier version-3 protocol. That run remains evidence
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
and real AES/MVS-compatible hardware validation remains pending.

## Verification performed

The milestone is checked with:

```bash
# Includes isolated MoonBit lowering/transpiler tests, generated-C comparison,
# native audio bridge tests, and MC68000 plus Z80 link/map guards.
make -C platform/neogeo verify
make -C platform/neogeo cart \
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
reports 10,777 bytes of Z80 fixed code, 1,944 bytes of static data, and 101
bytes of data-to-stack headroom. The cartridge pipeline packages that driver
as M1, preserves the zero-filled V1 region, and includes M1/V1 in
reproducibility size and hash checks. The M1 image and linker map are grouped
build outputs;
changes to the nullsound library or included command helper trigger a relink,
and the map checker runs on every verification or cartridge invocation.

The cadence probe owns an isolated X display and process groups, verifies
that the fixed debugger listener belongs to its launched emulator, and has a
finite sampling deadline. Its `result.json` records the ELF, cartridge, and
GnGeo-data SHA-256 values together with the full timing/sample arguments, so
the measurements remain tied to the binaries that were exercised.

The supplied ZIP contained one 40,976-byte iNES file with the supported
SHA-1. The generated cartridge loaded all P/M/V/S/C regions in
ngdevkit-gngeo, initialized the AES BIOS and 68000, and produced stable
640x448 captures (2x scale) showing:

- the large centered title panel and complete one/two-player menu;
- the centered/cropped 256x224 viewport;
- the fixed `MARIO / WORLD / TIME` HUD;
- sky, cloud, mountain, bush, and ground background tiles;
- Mario and other OAM sprites; and
- live palette/scroll updates across successive frames.

At stock emulated 68000 clock with automatic frameskip disabled, the
blanking-budgeted renderer completed 89 game frames during its first 120
display periods while the cartridge and renderer caches warmed. The
following 120-display-period sample completed 120 game frames with zero
misses. A separate active-input run pressed Start, held Right+B, and repeated
jumps. Its 300-display-period warmup completed 269 game frames, and the
following 120-display-period gameplay sample completed 120 game frames with
zero misses. This keeps title-screen idling from standing in for gameplay
performance.

Emulator captures and generated ROMs are verification artifacts only and are
not tracked. The normal-mode audio probe establishes non-silent emulator PCM
from active gameplay in addition to the sound-state, linkage, and packaging
gates. It does not establish subjective fidelity, electrical timing, or
operation on physical hardware.

## Next engineering steps

1. Improve audio fidelity with an ADPCM-B triangle voice so SSG C can
   represent noise independently, then add length/linear-counter gating.
2. Test on actual AES/MVS-compatible hardware and tune visible-area offsets.
3. Tighten the remaining fine-scroll left-edge masking cases.
4. Add pixel-level reference captures for the remaining renderer fidelity
   differences.
5. Add memory-card or save-state support.
