# Neo Geo port architecture

## Milestone status

The current milestone is a bootable, visually verified Neo Geo cartridge
proof of concept. It runs the upstream translated C game core on the MC68000,
uses Neo Geo graphics hardware directly, reads player-one input through the
BIOS, and packages a cartridge from a user-owned SMB dump.

Completed:

- Pure C target; no C++ runtime or desktop framework in the Neo Geo link.
- MC68000 code generation (`-m68000`) with size-oriented LTO.
- Direct background, OAM sprite, palette, FIX HUD, and input backends.
- Hidden sprite-set construction and VBlank-only reveal.
- Local, asset-clean CHR-to-C-ROM/S-ROM converter with unit tests.
- Full P/M/V/S/C ROM packaging and ngdevkit-gngeo boot.
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

At runtime:

- 33 vertical SCB strips cover the scrolling 256-pixel background, including
  the incoming fine-scroll column.
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
renderer updates live palettes/FIX entries, hides the old set, and uploads the
staged SCB3 words to reveal the new set.

This is the reusable lesson taken from the local Doom64KB work: organize data
in the hardware's upload order, precompute control words, update a hidden
display set, and keep the live swap bounded to VBlank. The Mario
implementation is independent C code for a tile renderer; no GPL Doom source
was copied into this Apache-2.0 project.

## Measured memory and ROM size

`make -C platform/neogeo verify` currently reports:

| Measurement | Bytes |
| --- | ---: |
| MC68000 text + read-only data | 113,902 |
| Initialized work RAM (`.data`) | 4 |
| Zeroed work RAM (`.bss`) | 4,992 |
| Static user work RAM total | 4,996 |
| User-RAM limit below `$10f300` | 62,208 |
| Remaining stack/heap headroom | 57,212 |

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
5. pads C1/C2/S1 to cartridge sizes; and
6. records a manifest confirming that zero PRG bytes were written.

The Makefile then builds the one-megabyte P1, null-sound M1, empty V1,
cartridge ZIP, and GnGeo hash data. All derived assets live below the ignored
`platform/neogeo/build/` directory.

## Verification performed

The milestone was checked with:

```bash
make -C platform/neogeo verify
make -C platform/neogeo cart \
  SMB_ROM="/path/to/smb.zip"
```

The supplied ZIP contained one 40,976-byte iNES file with the supported
SHA-1. The generated cartridge loaded all P/M/V/S/C regions in
ngdevkit-gngeo, initialized the AES BIOS and 68000, and produced stable
640x448 captures (2x scale) showing:

- the centered/cropped 256x224 viewport;
- the fixed `MARIO / WORLD / TIME` HUD;
- sky, cloud, mountain, bush, and ground background tiles;
- Mario and other OAM sprites; and
- live palette/scroll updates across successive frames.

Emulator captures and generated ROMs are verification artifacts only and are
not tracked.

## Next engineering steps

1. Add a deterministic render-command/state hash alongside the upstream
   7,987-frame recording test.
2. Exercise Start/A/B/joystick input through an automated GnGeo run and add
   longer gameplay captures.
3. Implement an integer-only event bridge from NES APU writes to a compact
   YM2610 Z80/68K sound driver.
4. Profile SCB upload time before promoting only measured hot functions from
   `-Os` to `-O3` or adding any 68000 assembly.
5. Test on actual AES/MVS-compatible hardware and tune visible-area offsets.
