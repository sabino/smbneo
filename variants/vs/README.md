# VS. Super Mario Bros. Neo

This edition ports the arcade game to a native Neo Geo cartridge. It keeps the
arcade stages, title and attract flow, credits, coinage, two-player selection,
and VS-specific rules. It is separate from the home SMBNeo edition and writes
its own `vssmbneo.*` artifacts, so building one never replaces the other.

The cartridge is playable in MAME and the project's GnGeo setup. Its inherited
hardware safeguards are validated at build time, but this edition still needs
broad testing on real AES/MVS boards and flashcarts.

## Native C runtime

The normal cartridge target uses the checked-in direct-C core under `native/`.
The MC68000 release binary contains ordinary C functions and direct calls; it
does not contain a 6502 interpreter, runtime program counter, instruction-fuel
loop, page dispatcher, emulated call/return stack, or translated `VsCpu`
machine.

The generator uses the verified arcade program and Matthew Gilmore's
[meta-disassembly](https://gitlab.com/segaloco/smb) as build-time evidence. Its
generated C preserves the game behavior while reviewed semantic C modules
replace hot movement, actor, collision, video, game-loop, and NMI paths. ROM
bytes are never stored in the generated source.

The older instruction-level translation remains available as
`neogeo-legacy-elf`. It is a development oracle for differential tests, not a
release cartridge and not part of `neogeo-cart`.

## Required game data

Provide your own canonical MAME `suprmrio.zip` set for **SM4-4 E**. A home NES
ROM is not interchangeable with this edition. A cartridge build requires these
seven verified members:

- four 8 KiB program chips;
- two 8 KiB graphics chips; and
- `rp2c04-0004.pal`, the 192-byte RP2C04-0004 palette data.

The loader verifies every filename, size, CRC32, and SHA-1 before producing
anything. All extracted data and generated cartridge files stay in ignored
`build/` directories. Do not commit a source ROM, generated game data, or a
BIOS.

## Build the cartridge

With ngdevkit and its MC68000/Z80 tools available, run from the repository root:

```sh
make -C variants/vs neogeo-cart \
  VS_ROM="/path/to/suprmrio.zip"
```

The authoritative outputs are:

```text
variants/vs/build/rom/vssmbneo.zip
variants/vs/build/rom/vssmbneo.neo
variants/vs/build/rom/gngeo_data.zip
variants/vs/build/mame/hash/neogeo.xml
```

`vssmbneo.zip` contains the full hardware-native layout:

| Region | File | Size |
| --- | --- | ---: |
| P | `vssmbneo-p1.p1` | 1 MiB |
| S | `vssmbneo-s1.s1` | 128 KiB |
| M | `vssmbneo-m1.m1` | 128 KiB |
| V | `vssmbneo-v1.v1` | 512 KiB |
| C1 | `vssmbneo-c1.c1` | 2 MiB |
| C2 | `vssmbneo-c2.c2` | 2 MiB |

This full P/S/M/V/C layout is the source of truth for MAME, GnGeo, physical
cartridge/flashcart workflows, and the `.neo` image. The cartridge identity is
`vssmbneo`, the visible title is **VS. Super Mario Bros. Neo**, and the packed-
BCD NGH is `0x2027`.

The `.neo` file contains the same full cartridge payload in NeoSD/NeoSD Pro
format. It does not contain a Neo Geo BIOS.

## Run in MAME

MAME supports an external software list, so this path uses the real `vssmbneo`
identity rather than pretending to be an existing game. After building, launch
from the repository root with:

```sh
mame ng_mv1 vssmbneo \
  -hashpath "$PWD/variants/vs/build/mame/hash" \
  -rompath "/path/to/mame/roms;$PWD/variants/vs/build/rom" \
  -noplugins
```

`/path/to/mame/roms` must contain a compatible, legally obtained `neogeo.zip`.
The SDK's open replacement BIOS is sufficient for cartridge-side testing, but
MAME may warn that its checksums differ from the proprietary BIOS entries in
MAME's database. That warning is about the selected BIOS, not `vssmbneo.zip`.

The bounded integration runner exercises the real MVS coin input, credit
consumption, Start, movement, run, and jump:

```sh
python3 tools/run_vs_mame.py \
  --build variants/vs/build \
  --bios-dir /path/to/mame/roms \
  --system ng_mv1 --real-coin --game-frames 1200 \
  --label local-check
```

Use `--system aes` without `--real-coin` to exercise the AES coin shortcut.

## Run with the custom GnGeo driver

`gngeo_data.zip` contains the generated `rom/vssmbneo.drv` entry. Keep it
separate from the game archive and launch the custom shortname explicitly:

```sh
ngdevkit-gngeo -b soft --screen320 --scale 3 --no-resize --sound \
  --system home \
  -i "$PWD/variants/vs/build/rom" \
  -B "/path/to/your/gngeo-bios-directory" \
  -d "$PWD/variants/vs/build/rom/gngeo_data.zip" \
  vssmbneo
```

GnGeo still needs a compatible BIOS supplied separately. Depending on the
chosen `--system` mode and GnGeo build, that normally means `neogeo.zip` and
the corresponding AES/MVS BIOS archive in the directory passed with `-B`.
Neither BIOS belongs inside `vssmbneo.zip` or `gngeo_data.zip`.

## Controls, credits, and DIP rules

| Action | Neo Geo control |
| --- | --- |
| Move | Joystick |
| Jump / swim | A or B |
| Run / fire | C or D |
| One player | P1 Start |
| Two players | P2 Start |

On MVS, either physical coin slot adds credits according to the arcade coinage
rule. Service credit is handled separately. On AES, press P1 Start together
with C or D to insert one credit, release the buttons, then press Start normally.

The current cartridge fixes the decoded DSW byte to `0x10`: 1 coin/1 credit,
three starting lives, a bonus life at 200 coins, the slow timer, and four lives
after continue. The native rules tests cover all 256 DSW bytes and all eight
coinage encodings; exposing a user-selectable cabinet DIP UI is separate work.

## Browser build

The web player accepts the same user-owned `suprmrio.zip` only after
**VS Arcade Edition** is selected. Validation and conversion happen locally in
the browser; the selected archive and generated game data are never uploaded.

The VS page offers three clearly named downloads:

- `vssmbneo.zip`, the full canonical cartridge for hardware, MAME, and the
  custom GnGeo driver; and
- `vssmbneo.neo`, the same full cartridge in NeoSD/NeoSD Pro format; and
- `puzzledp.zip`, an optional reduced package for emulators whose fixed
  database cannot discover `vssmbneo`.

For in-page play, EmulatorJS/FBNeo uses the same known donor identity internally
because its database cannot discover a custom driver. That loader workaround
does not change the page title, canonical artifacts, or project identity. The
page supplies ngdevkit's open replacement BIOS as a separate emulator asset; it
does not embed a BIOS in any download. No proprietary BIOS or VS game data is
committed or uploaded.

## Tests and development oracle

ROM-free checks run without a source ROM or BIOS:

```sh
make -C variants/vs test native-test
```

With an owned source set and the pinned reference checkout, the full
differential validates the direct-C runtime against the instruction-level
oracle across every DIP byte, all eight coinage modes, all 32 stages, gameplay
input sequences, PPU/APU state, and audio write ordering:

```sh
make -C variants/vs native-differential \
  VS_ROM="/path/to/suprmrio.zip" \
  VS_REFERENCE="/path/to/vs-reference"
```

Normal users do not need `VS_REFERENCE` to build or play the cartridge. It is
required only to regenerate or differentially verify the checked-in native C.
The expected reference revision is
`238f61930e2b5024cb17cf70975e637ee0305f8f`.

To build the legacy oracle explicitly:

```sh
make -C variants/vs translate neogeo-legacy-elf \
  VS_ROM="/path/to/suprmrio.zip" \
  VS_REFERENCE="/path/to/vs-reference"
```

## Hardware safeguards

The VS cartridge uses the same reviewed physical renderer as the home edition:

- full-height LSPC background chains and explicit palette-latch setup;
- pre-oriented C-ROM tiles instead of shrink-sensitive hardware flip bits;
- word-sized, timing-safe VRAM writes and bounded live updates;
- `$8000` analog palette reference in both physical banks;
- independent address/data pairs for OAM visibility writes;
- aligned cartridge-resident data and guarded startup RAM restoration; and
- no linked memory-card API or direct backup-RAM/control write.

`neogeo-cart` inspects the final ELF for those contracts, RAM bounds, native
identity, required data, and the absence of legacy CPU/dispatcher symbols.
Emulator success cannot replace a physical AES/MVS and flashcart retest, so
hardware results should always include the board, BIOS, and cartridge device.
