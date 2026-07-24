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
- Automated architecture, translated-core reachability, forbidden-symbol,
  and work-RAM guards.

Not completed:

- YM2610 sound/music. `apu_null.c` currently discards NES APU writes.
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

The final linked ELF (SHA-256
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

## Measured memory and ROM size

`make -C platform/neogeo verify` currently reports:

| Measurement | Bytes |
| --- | ---: |
| MC68000 text + read-only data | 180,830 |
| Initialized work RAM (`.data`) | 4 |
| Zeroed work RAM (`.bss`) | 16,112 |
| Static user work RAM total | 16,116 |
| User-RAM limit below `$10f300` | 62,208 |
| Remaining stack/heap headroom | 46,092 |

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

The Makefile then builds the one-megabyte P1, null-sound M1, empty V1,
cartridge ZIP, and GnGeo hash data. All derived assets live below the ignored
`platform/neogeo/build/` directory.

For the final audited program, the two isolated builds produced identical
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
2 reports both parameters, the number of area holds actually consumed, and the
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
```

The generated header stores compact `uint16_t` durations and `uint8_t`
controller states rather than 67,000 padded structures. It also embeds the raw
FM2 SHA-256, canonical imported-recording SHA-256, exact input frame count,
initial command, RAM initialization option/seed, direction policy, and
opposite-direction count. The ignored gate build has its own ELF, map,
cartridge, and asset directory, so it cannot contaminate the playable build.

`REPLAY_FAST=1` skips only the hardware renderer and is appropriate for the
translated-core progression gate. Omitting it exercises normal rendering but
takes display time and is not a substitute for the separate stock-clock
cadence measurement. The runner accepts a result only when it reaches the
pass trap with both 32-stage masks complete. Failure, incomplete playback,
invalid mailbox data, debugger/emulator exit, occupied debug port, and timeout
all remain non-passing and produce a bounded `result.json` plus logs. A
cartridge-side checkpoint exposes an intermediate mailbox every 1,800 frames;
the debugger therefore does not need to stop and round-trip on every frame.

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

The terminal mailbox had valid version-2 metadata, retained the exact source
frame count and hardware-playable direction policy, passed all host-side
accounting checks, and reached the dedicated pass trap. The bounded
`result.json` had SHA-256
`c93dee25ee767ca22cdb0680f47570a04e678af000d6ff806e288d1516b3f731`.
The exercised replay ELF, cartridge ZIP, and P region had SHA-256 values
`de4fc801831ad5fae803d45bf73f917c6d91bb49109009d56cfc106e5b377caa`,
`0897a187323b6cd9696804270c0a887167f53e5d62514b50d2bc3455d8b0a3b1`,
and
`d2f22e4c5dce90a1102676a6e46ea06c7cc09a7522c66a7112c1a1bf295cb86b`,
respectively.

This is a translated-core progression proof, not a rendered-performance or
physical-hardware claim. The normal rendered cartridge is covered separately
by the stock-clock cadence probes, and real AES/MVS-compatible hardware
validation remains pending.

## Verification performed

The milestone is checked with:

```bash
# Includes isolated MoonBit lowering/transpiler tests and a comparison of
# freshly generated C against every checked-in generator output.
make -C platform/neogeo verify
make -C platform/neogeo cart \
  SMB_ROM="/path/to/smb.zip"
python3 tools/check_reproducible_cart.py \
  --rom "/path/to/smb.zip"
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
not tracked.

## Next engineering steps

1. Run long rendered replays across every world and retain transition
   screenshots/state evidence.
2. Implement an integer-only event bridge from NES APU writes to a compact
   YM2610 Z80/68K sound driver.
3. Test on actual AES/MVS-compatible hardware and tune visible-area offsets.
4. Tighten the remaining fine-scroll left-edge masking cases.
5. Add memory-card or save-state support.
