# Emulator and cartridge packages

SMBNeo has two deliberately separate cartridge identities. They contain the
same port, but they serve different loaders:

- `puzzledp` is the zero-configuration compatibility identity for frontends
  whose bundled Neo Geo database cannot discover a new game.
- `smbneogeo` is the canonical project identity for MAME, custom software
  lists, and hardware-oriented packaging.

The compatibility identity must not be used to describe SMBNeo in the
canonical MAME lane.

## Default compatibility package

```bash
make cart SMB_ROM="/path/to/owned/smb.zip"
```

This creates:

```text
platform/neogeo/build/rom/puzzledp.zip
```

`puzzledp.zip` is the default compatibility release for NEO.emu, GnGeo,
FBNeo, and EmulatorJS. It uses the Puzzle De Pon cartridge identity so
emulators with a fixed Neo Geo game database can recognize the generated
SMBNeo data. It does not contain Puzzle De Pon data.

The archive is validated immediately after it is built:

| File | Size | CRC32 | Loader behavior |
| --- | ---: | ---: | --- |
| `202-p1.bin` | 512 KiB | `2b61415b` | 16-bit word swap at address zero |
| `202-s1.bin` | 128 KiB | `cd19264f` | linear |
| `202-m1.bin` | 128 KiB | `9c0291ea` | linear |
| `202-v1.bin` | 512 KiB | `debeb8fb` | linear |
| `202-c1.bin` | 1 MiB | `cc0095ef` | even sprite bytes |
| `202-c2.bin` | 1 MiB | `42371307` | odd sprite bytes |

The full native P1 is 1 MiB and each native C ROM is 2 MiB. The compatibility
builder omits only their verified `FF`/zero upper padding. It leaves the
retained program bytes in place—there is no half-ROM relocation—and confines
each CRC correction to the final four bytes of a verified padding run. A
non-padding byte in either an omitted range or a correction range stops the
build.

## BIOS requirement

The game archive and the Neo Geo BIOS are separate files. Standalone
emulators generally need both:

```text
puzzledp.zip
neogeo.zip
```

Keep those filenames unchanged and place both files in a ROM directory
scanned by the emulator. Use a BIOS set that is legal for you to use and
compatible with that emulator. The build does not add a BIOS to
`puzzledp.zip`, and the repository contains neither a proprietary BIOS nor
original game ROM data.

The online player ships ngdevkit's open replacement as a separate web asset.
That does not make it a drop-in replacement for every standalone emulator's
BIOS database.

## Emulator paths

### NEO.emu on Android

Copy the generated `puzzledp.zip` and a compatible `neogeo.zip` into the
directory NEO.emu scans, rescan if necessary, and start the Puzzle De Pon
entry. This exact two-file arrangement has been confirmed on a real Android
NEO.emu installation.

### FBNeo and EmulatorJS

Standalone FBNeo uses the same `puzzledp.zip` identity and also needs its
supported `neogeo.zip` BIOS set.

The browser player performs the same native-to-`puzzledp` conversion in
JavaScript and starts EmulatorJS with the FBNeo core and game name
`puzzledp`. It can accept the supported `.nes` input, a ZIP containing that
input, the canonical hardware archive, or a generated `puzzledp.zip`.
Conversion tests compare every generated web entry byte-for-byte with the
native Python builder.

### ngdevkit GnGeo

Use:

```bash
make run SMB_ROM="/path/to/owned/smb.zip"
```

The normal GnGeo launch uses `puzzledp.zip`, the installed GnGeo
`puzzledp` database entry, and the separately copied open ngdevkit BIOS. It
does not generate or require a custom SMBNeo database entry for ordinary
play.

### MAME: canonical `smbneogeo` identity

MAME supports local software lists, so its official project path does not use
the donor identity. Generate the full cartridge and local `neogeo.xml` with:

```bash
make mame-cart SMB_ROM="/path/to/owned/smb.zip"
```

This creates:

```text
platform/neogeo/build/rom/smbneogeo.zip
platform/neogeo/build/mame/hash/neogeo.xml
```

Launch the unique `smbneogeo` software entry directly with:

```bash
mame ng_mv1 smbneogeo \
  -hashpath "$PWD/platform/neogeo/build/mame/hash" \
  -rompath "$PWD/platform/neogeo/build/rom"
```

The repository wrappers generate those same artifacts and options:

```bash
make mame-run SMB_ROM="/path/to/owned/smb.zip"
make mame-capture SMB_ROM="/path/to/owned/smb.zip"
```

These commands intentionally load the full native `smbneogeo.zip` through
the generated software list on the `ng_mv1` system. The XML names the game
`smbneogeo`, preserves the 1 MiB P and 2 MiB-per-chip C regions, and records
their generated CRC32/SHA-1 values and MAME loading semantics.

The target uses ngdevkit's open replacement BIOS unless `MAME_BIOS_DIR`
points to another complete, user-owned MAME BIOS set. For a separate BIOS
directory, the equivalent explicit ROM path is:

```bash
mame ng_mv1 smbneogeo \
  -hashpath "$PWD/platform/neogeo/build/mame/hash" \
  -rompath "/path/to/mame/roms;$PWD/platform/neogeo/build/rom"
```

### Donor compatibility launch

In a fixed-database frontend, scan `puzzledp.zip` and launch the entry shown
as **Puzzle De Pon**. That is the intended compatibility behavior for
NEO.emu, GnGeo, FBNeo, and EmulatorJS even though the archive contains
SMBNeo.

MAME can also resolve the compatibility archive through its built-in donor
definition:

```bash
mame puzzledp \
  -rompath "/directory/containing/rom-zips"
```

That command is a compatibility fallback, not the canonical MAME identity.
It needs a complete MAME-compatible `neogeo.zip`. Prefer the generated
`smbneogeo` software-list command above whenever using MAME.

## Canonical hardware package

```bash
make hardware-cart SMB_ROM="/path/to/owned/smb.zip"
```

This preserves the full cartridge layout as:

```text
platform/neogeo/build/rom/smbneogeo.zip
```

Use this package for the canonical MAME path and for flash-cartridge or
physical-hardware work that expects the full SMBNeo P/C sizes. Normal GnGeo
play uses `puzzledp.zip`; internal replay and diagnostic tooling may still
generate a custom GnGeo hash for purpose-built cartridges. Do not silently
substitute the smaller CRC-identified `puzzledp.zip` for a hardware workflow,
and do not present the donor name as SMBNeo's canonical MAME identity.

All generated archives and ROM regions live under the ignored
`platform/neogeo/build/` directory. Do not commit or redistribute generated
game data unless you have the necessary rights.
