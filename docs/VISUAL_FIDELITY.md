# Visual fidelity

## Mario geometry

Mario's tile geometry is preserved at one source pixel per displayed content
pixel:

1. The asset converter decodes each 8x8 two-bit CHR tile without resampling.
2. `expand_2x()` duplicates every pixel into a 2x2 block in a 16x16 C-ROM
   tile.
3. SCB2 horizontal zoom 7 and the in-tile vertical shrink map select the even
   rows and columns, recovering the original 8x8 tile exactly.
4. Lower source OAM indices are assigned higher Neo Geo sprite numbers, which
   preserves overlapping-sprite priority.

`test_hardware_shrink_recovers_every_source_pixel` exercises that complete
tile transform with synthetic data and also locks both renderer zoom words.
The behavior matches the documented
[Neo Geo sprite shrinking](https://wiki.neogeodev.org/index.php?title=Sprite_shrinking)
and [sprite layout](https://wiki.neogeodev.org/index.php?title=Sprites).

The content viewport is 256x224 square pixels centered inside the 320-pixel
Neo Geo display. The source's top and bottom eight overscan lines are
intentionally cropped.

GnGeo's full-frame vertical DDA renders `0x7f` as nine rows for a one-tile
object, duplicating its first row and overlapping the next 8x8 OAM tile.
Single-tile objects therefore use vertical zoom `0x7e`, whose hardware L0
table selects the same eight in-tile source rows but which GnGeo emits as
exactly eight rows. Multi-tile background strips retain `0x7f`; their shrink
table diverges after the first tile. This removes the observed extra hat row
without changing Mario's CHR data or OAM priority.

## Animated sprite orientation

The source game animates some composite objects by swapping their left and
right 8x8 tiles and setting the horizontal-flip bit on both. A follow-up
[MV1C recording](https://streamable.com/2r8pyl) showed the swapped Goomba
halves without the requested mirroring during alternating walk poses. That
isolated the fault to the combination of runtime LSPC flipping and the
16-to-8 shrink path rather than to the Goomba artwork or game animation.

The asset converter now writes four complete C-ROM banks: normal,
horizontally mirrored, vertically mirrored, and mirrored on both axes. The OAM
renderer selects a bank from source attribute bits 6 and 7 and leaves LSPC
SCB1 flip bits 0 and 1 clear. This preserves every source orientation without
additional frame-time work or work RAM. ROM-free conversion tests decode all
four banks back to pixels, and a host renderer test locks the tile selection
and verifies that flip attributes never reach SCB1.

## Full-height background strips

Vertical shrink is applied across an entire Neo Geo sprite chain. It does not
restart at every 16-pixel C-ROM tile. Each native background strip therefore
uses SCB3 height 33, which selects the 32-tile full-height mode, together with
vertical zoom `$7f`. That combination exposes all 32 physical SCB1 rows over
one 256-line period and maps each doubled 16-pixel source tile to one displayed
8-pixel row.

The 28-row playfield occupies physical rows 0 through 27 when the HUD is
hidden. When the three-row FIX HUD is visible, the 26-row playfield starts at
physical row 3. Unused physical rows are transparent. This arrangement avoids
the missing ground and downward black columns produced when a short ordinary
chain is combined with whole-chain shrink on the real LSPC.

FIX tile zero is also permanently transparent. The 512 generated source tiles
start at tile one, followed by dedicated blank and border tiles. BIOS code can
therefore clear the FIX map to tile zero during cartridge handoff without
briefly filling the screen with game graphics.

## Color target

The earlier port converted a different subjective NES RGB table than the one
used by its timing/screenshot reference. Mario's geometry was correct, but
the much brighter red/orange/brown balance made the hat read as heavier or
taller.

The reproducible digital target is now named explicitly: the FCEUX 2.2.1
default six-bit palette used by the existing reference run. There is no
single hardware-independent “actual NES RGB palette”; the original video
signal and display decoder are analog variables.

`tools/gen_neogeo_palette.py` is the single source for:

- the host-reference renderer's 64 RGB triples; and
- the Neo Geo renderer's 64 nearest representable color words.

The encoder tests both states of the Neo Geo's shared low color bit, chooses
the minimum squared-error result with a deterministic tie break, and then
round-trips the word through the target decoder. Every channel is within four
8-bit levels of the named reference.

Mario's principal colors now map as follows:

| Index | Reference RGB | Neo Geo RGB |
| --- | --- | --- |
| `$16` red | `(216, 40, 0)` | `(216, 40, 0)` |
| `$27` orange | `(252, 152, 56)` | `(248, 152, 56)` |
| `$18` olive | `(136, 112, 0)` | `(136, 112, 0)` |

The reference values come from the
[FCEUX 2.2.1 palette sources](https://github.com/TASEmulators/fceux/tree/fceux-2.2.1/src/palettes).

## Intentional differences

The port does not emulate the source hardware's eight-sprites-per-scanline
dropout. Every visible OAM entry is sent to the larger target sprite engine,
while source OAM priority is preserved. Crowded scenes can therefore show
pixels that would flicker or disappear on the source hardware.

Physical display scaling, CRT decoder behavior, composite artifacts, and
AES/MVS analog output are outside a square-pixel screenshot comparison. Two
MV1C runs have informed the renderer fixes, but the latest pre-oriented sprite
build still requires a physical retest.
