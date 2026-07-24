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
- Sustained one-game-frame-per-VBlank cadence at stock emulated clock after
  the cold-start cache fill.
- Automated architecture, translated-core reachability, forbidden-symbol,
  and work-RAM guards.

Not completed:

- A deterministic proof of progression through all 32 stages. The checked-in
  smoke recording covers only stages 1-1 through 1-3.
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
`1c2f093adcfa48725efd5b53f2f8b7a2d3f1fc5b63f55ce8a3c17963368317e3`)
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
| MC68000 text + read-only data | 180,534 |
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
`5799b073ae3744dd4153b538db461b90021c0e004281032fb1a3f6029120c38d`
and identical 104,898-byte cartridge ZIPs with SHA-256
`11e12daaa15129daed997839be7d0c6a4afd8dd61e3dd74d715d5f68064b62f6`.

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

## Verification performed

The milestone is checked with:

```bash
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

1. Embed a locally supplied, validated full-game replay in a gate-only
   cartridge and prove all 32 stage transitions plus the final victory state.
2. Add a deterministic core-state transcript so replay drift can be separated
   from renderer or emulator failures.
3. Run long rendered replays across every world and retain transition
   screenshots/state evidence.
4. Implement an integer-only event bridge from NES APU writes to a compact
   YM2610 Z80/68K sound driver.
5. Test on actual AES/MVS-compatible hardware and tune visible-area offsets.
