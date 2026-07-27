# SMBNeo

**Super Mario Bros. running as a native Neo Geo cartridge.**

SMBNeo is a playable, open-source Neo Geo port built around a pure C game
core. It runs on the MC68000, draws the game with the Neo Geo's tile and
sprite hardware, and plays music and sound through the Z80 and YM2610.

This repository is a Neo Geo-focused fork of
[`nathsou/smb`](https://github.com/nathsou/smb). It is still a work in
progress, but the game is playable from the title screen through the ending.

## Play online

**[Launch SMBNeo in your browser](https://sabino.pro/smbneo/)**

Choose your own supported `.nes` file, ZIP, or locally built
`smbneo.zip`/`puzzledp.zip`. The file stays in your browser and is never
uploaded. After conversion, the page offers three local downloads:

- `smbneo.zip`, the canonical cartridge archive;
- `smbneo.neo`, the single-file NeoSD/NeoSD Pro image; and
- `puzzledp.zip`, the optional package for fixed-database emulators.

Use the arrow keys to move, `A` to jump, `S` to run or throw fireballs, `1`
to start, and `2` to select.

## Screenshots

<p align="center">
  <img src="docs/screenshots/title-screen.png" width="31%" alt="SMBNeo title screen">
  <img src="docs/screenshots/world-1-1.png" width="31%" alt="Mario running through World 1-1">
  <img src="docs/screenshots/world-1-1-goomba.png" width="31%" alt="Mario facing a Goomba in World 1-1">
</p>

<p align="center"><sub>The title screen and World 1-1 from the current Neo Geo build.</sub></p>

## What works

- The original title screen, menu, stages, enemies, power-ups, and ending.
- Neo Geo joystick and button input.
- Backgrounds, objects, palettes, and the HUD rendered directly with Neo Geo
  graphics hardware.
- Music and sound effects played through the Neo Geo sound hardware.
- Full progression through all 32 stages.
- More on-screen objects without reproducing the original console's small
  sprite-per-scanline limit.

## Controls

| Action | Neo Geo | Default keyboard |
| --- | --- | --- |
| Move | Joystick | Arrow keys |
| Jump / swim | A | `A` |
| Run / fire | B | `S` |
| Start | Start | `1` |
| Select | Select | `2` |

Press Start on the title screen before trying to move Mario.

## Build and play

You will need the
[ngdevkit](https://github.com/dciabrin/ngdevkit) toolchain and a legally
obtained game image. The input may be a raw `.nes` file or a ZIP containing
one `.nes` file.

```bash
git clone https://github.com/sabino/smbneo.git
cd smbneo

make cart SMB_ROM="/path/to/smb.zip"
make run SMB_ROM="/path/to/smb.zip"
```

The supported game revision has SHA-1
`ea343f4e445a9050d4b4fbac2c77d0693b1d0922`.

`make cart` creates both canonical full-size cartridge packages:

```text
platform/neogeo/build/rom/smbneo.zip
platform/neogeo/build/rom/smbneo.neo
```

Both preserve the native 1 MiB P, 128 KiB S/M, 512 KiB V, and two 2 MiB C
regions used by real hardware. The ZIP is used by MAME and the project's
GnGeo path. The `.neo` file is the single-file format used by TerraOnion
NeoSD and NeoSD Pro flashcarts; it does not contain a BIOS. The same build also creates
custom GnGeo driver data under the shortname `smbneo`; `make run` launches
that identity. `make hardware-cart` remains an explicit alias for callers
that want to emphasize the six-ROM physical-cartridge lane.

To generate only the NeoSD image:

```bash
make neosd-cart SMB_ROM="/path/to/smb.zip"
```

The generated format, metadata, and payload have been checked byte for byte
against ngdevkit and an independent open-source converter. A boot on physical
NeoSD/NeoSD Pro hardware is still awaiting confirmation from a device owner.

For an emulator whose built-in database cannot discover a custom game, build
the optional compatibility archive:

```bash
make compat-cart SMB_ROM="/path/to/smb.zip"
```

That produces `platform/neogeo/build/rom/puzzledp.zip`. The donor name is a
loader workaround only and is never SMBNeo's public identity. Standalone
emulators also require a separate compatible `neogeo.zip` BIOS. Generated
graphics and cartridge files are ignored by Git and must not be
redistributed.

The versions used for the current build are listed in
[Tested toolchain](docs/TESTED_TOOLCHAIN.md).
Exact FBNeo, NEO.emu, MAME, GnGeo, BIOS, and hardware package instructions are
in [Emulator and cartridge packages](docs/EMULATOR_COMPATIBILITY.md).

### Hardware-accurate emulator check

MAME exercises Neo Geo sprite-chain behavior that can be missed by lighter
emulators. Because MAME supports custom software lists, the canonical lane
generates `platform/neogeo/build/mame/hash/neogeo.xml` and launches the full
`smbneo.zip` as `smbneo` on MAME's one-slot MV-1 configuration.

Build the canonical MAME package with:

```bash
make mame-cart SMB_ROM="/path/to/smb.zip"
```

The direct equivalent command is:

```bash
mame ng_mv1 smbneo \
  -hashpath "$PWD/platform/neogeo/build/mame/hash" \
  -rompath "$PWD/platform/neogeo/build/rom"
```

To collect a scripted set of title and gameplay frames instead:

```bash
make mame-capture SMB_ROM="/path/to/smb.zip"
```

The PNG files are written to
`platform/neogeo/build/mame/captures/`. For an interactive run, use
`make mame-run` instead. To test a BIOS that you legally own and have already
installed in a MAME ROM directory:

```bash
make mame-run SMB_ROM="/path/to/smb.zip" \
  MAME_BIOS=unibios40 MAME_BIOS_DIR="/path/to/mame/roms"
```

No BIOS image is included in this repository.
Without `MAME_BIOS_DIR`, the target uses ngdevkit's open replacement from the
local build. That is enough to test the cartridge renderer, but matching a
specific BIOS splash requires the corresponding complete MAME BIOS set.

## How it works

The upstream project translates the original game logic into C. SMBNeo
compiles that code for the Neo Geo's MC68000 and supplies new platform code
for video, sound, controls, timing, and cartridge startup.

Instead of drawing a complete image in work RAM every frame, the renderer
maps the game's background tiles and objects onto Neo Geo sprites and uses
the FIX layer for the status display. This keeps memory use low and lets the
graphics hardware do most of the drawing.

During a local build or browser launch, the graphics needed by the port are
converted from the user-supplied game image. No game ROM, generated graphics,
or packaged cartridge is included in this repository.

## Project status

The complete game is playable in the emulator, including enemy-heavy stages
and the final ending. Performance, sound balance, input, scrolling, title
screen rendering, and crowded scenes have all received target-specific work.

MV1C flash-cartridge tests exposed BIOS-handoff, background-chain, and
animated-sprite orientation errors that lighter emulation had hidden. The
renderer now uses full-height native sprite chains and pre-oriented graphics
banks instead of runtime flip bits for 8x8 objects. The latest orientation fix
still needs a physical-hardware retest. Sound is adapted to the Neo Geo
hardware rather than reproduced waveform-for-waveform, and further performance
and fidelity improvements are welcome.

## Development

Run the ROM-free test suite before submitting a change:

```bash
make ci
```

For contribution guidelines and deeper technical information, see:

- [Contributing](CONTRIBUTING.md)
- [Neo Geo port architecture](docs/NEOGEO_PORT.md)
- [Emulator and cartridge packages](docs/EMULATOR_COMPATIBILITY.md)
- [Visual fidelity](docs/VISUAL_FIDELITY.md)
- [Changelog](CHANGELOG.md)

## Credits

- [`nathsou/smb`](https://github.com/nathsou/smb), the static-recompilation
  project on which this fork is based.
- [doppelganger's SMBDIS
  disassembly](https://www.romhacking.net/documents/344/), used by the
  upstream translator.
- The ngdevkit contributors and the wider Neo Geo development community.

See [Third-party notices](THIRD_PARTY_NOTICES.md) for full attribution.

SMBNeo is an unofficial fan project and is not affiliated with Nintendo or
SNK. Game names, characters, graphics, audio, and related trademarks belong
to their respective owners. The repository source code is provided under the
[Apache License 2.0](LICENSE); that license does not grant rights to the
underlying game or its assets.
