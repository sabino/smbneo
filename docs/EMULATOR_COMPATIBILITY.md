# Emulator and cartridge packages

SMBNeo has two deliberately separate cartridge packages. They contain the
same port, but they serve different loaders.

## Default compatibility package

```bash
make cart SMB_ROM="/path/to/owned/smb.zip"
```

This creates:

```text
platform/neogeo/build/rom/puzzledp.zip
```

`puzzledp.zip` is the default emulator release. It uses the Puzzle De Pon
cartridge identity so emulators with a fixed Neo Geo game database can
recognize the generated SMBNeo data. It does not contain Puzzle De Pon data.

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

### MAME

For MAME, use the repository's hardware-validation target:

```bash
make mame-run SMB_ROM="/path/to/owned/smb.zip"
make mame-capture SMB_ROM="/path/to/owned/smb.zip"
```

These commands intentionally use the full native `smbneogeo.zip` through a
generated local software list on the `ng_mv1` system. This exercises the real
P/C region sizes and MAME's Neo Geo video implementation instead of relying
on the compatibility alias. The target uses ngdevkit's open replacement BIOS
unless `MAME_BIOS_DIR` points to another complete, user-owned MAME BIOS set.

MAME can also resolve the six compatibility members by their standard
`puzzledp` CRCs:

```bash
mame puzzledp -rompath "/directory/containing/puzzledp-and-neogeo-zips"
```

That direct built-in-driver route needs a complete MAME-compatible
`neogeo.zip`. The smaller ngdevkit replacement is sufficient for the
repository's custom rendering lane, but it does not satisfy MAME's complete
stock BIOS audit.

### ngdevkit GnGeo

Use:

```bash
make run SMB_ROM="/path/to/owned/smb.zip"
```

GnGeo runs the full native `smbneogeo.zip` with the generated
`gngeo_data.zip` driver. It does not use `puzzledp.zip`; the custom hash entry
lets GnGeo load the actual native sizes and CRCs directly. The target also
places ngdevkit's local open BIOS archives in the ignored build directory.

## Canonical hardware package

```bash
make hardware-cart SMB_ROM="/path/to/owned/smb.zip"
```

This preserves the full cartridge layout as:

```text
platform/neogeo/build/rom/smbneogeo.zip
```

Use this package for the project's GnGeo and MAME paths and for
flash-cartridge or physical-hardware work that expects the canonical SMBNeo
P/C sizes. Do not silently substitute the smaller CRC-identified
`puzzledp.zip` for a hardware workflow.

All generated archives and ROM regions live under the ignored
`platform/neogeo/build/` directory. Do not commit or redistribute generated
game data unless you have the necessary rights.
