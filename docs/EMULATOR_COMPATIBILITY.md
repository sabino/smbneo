# Emulator and cartridge packages

SMBNeo has one canonical identity and one optional loader workaround:

- `smbneo` is the project identity. Its visible title is
  **Super Mario Bros. Neo**.
- `puzzledp` is used only when an emulator has a fixed Neo Geo database and
  cannot load external metadata for a new cartridge.

Both packages are generated locally from a legally obtained supported game
image. Neither generated package is committed to this repository.

## Canonical cartridge

Build the normal project output with:

```bash
make cart SMB_ROM="/path/to/owned/smb.zip"
```

The result is:

```text
platform/neogeo/build/rom/smbneo.zip
```

This is the authoritative hardware-native cartridge. It contains:

| File | Size | Purpose |
| --- | ---: | --- |
| `smbneo-p1.p1` | 1 MiB | MC68000 program |
| `smbneo-s1.s1` | 128 KiB | FIX-layer graphics |
| `smbneo-m1.m1` | 128 KiB | Z80 sound program |
| `smbneo-v1.v1` | 512 KiB | YM2610 sample data |
| `smbneo-c1.c1` | 2 MiB | even sprite data |
| `smbneo-c2.c2` | 2 MiB | odd sprite data |

`make hardware-cart` builds the same canonical output. The extra name exists
for scripts and documentation that want to make physical-hardware intent
explicit.

Use `smbneo.zip` for flashcarts, physical hardware, the generated MAME
software list, and the project’s generated GnGeo driver. Do not substitute
the smaller donor archive in a hardware workflow.

## GnGeo with the custom identity

The canonical build also creates:

```text
platform/neogeo/build/rom/gngeo_data.zip
```

That data archive contains a generated `rom/smbneo.drv` entry whose title,
region sizes, filenames, destinations, and CRCs are checked against the six
native ROMs. Launch the supported project path with:

```bash
make run SMB_ROM="/path/to/owned/smb.zip"
```

The launch uses `smbneo.zip`, the generated custom driver data, and the
shortname `smbneo`. It does not pretend the game is another cartridge.

The game archive, GnGeo data, and Neo Geo BIOS remain separate. Because the
project launch uses `--system home`, the local ngdevkit target stages and
loads its open AES replacement from `aes.zip`; it also stages `neogeo.zip`
for Neo Geo parent/common-ROM lookup. Other GnGeo installations must provide
the compatible BIOS archive or archives required by their chosen system
mode. None of those BIOS files belongs inside `smbneo.zip`.

## MAME with the custom identity

MAME supports external software lists, so its canonical path also uses
`smbneo`. Generate the cartridge and local software list with:

```bash
make mame-cart SMB_ROM="/path/to/owned/smb.zip"
```

This creates:

```text
platform/neogeo/build/rom/smbneo.zip
platform/neogeo/build/mame/hash/neogeo.xml
```

The XML contains one `smbneo` software entry titled
**Super Mario Bros. Neo**. It records the full region sizes, generated
CRC32/SHA-1 hashes, and the correct MAME loading behavior: word-swapped P-ROM
at address zero and even/odd C-ROM interleaving.

Launch it directly with:

```bash
mame ng_mv1 smbneo \
  -hashpath "$PWD/platform/neogeo/build/mame/hash" \
  -rompath "$PWD/platform/neogeo/build/rom"
```

The repository wrappers use the same route:

```bash
make mame-run SMB_ROM="/path/to/owned/smb.zip"
make mame-capture SMB_ROM="/path/to/owned/smb.zip"
```

To use a complete MAME BIOS set that you legally own:

```bash
make mame-run SMB_ROM="/path/to/owned/smb.zip" \
  MAME_BIOS=unibios40 MAME_BIOS_DIR="/path/to/mame/roms"
```

The equivalent explicit ROM path is:

```bash
mame ng_mv1 smbneo \
  -hashpath "$PWD/platform/neogeo/build/mame/hash" \
  -rompath "/path/to/mame/roms;$PWD/platform/neogeo/build/rom"
```

MAME needs the generated `neogeo.xml`, the canonical game archive, and a
usable Neo Geo BIOS. A BIOS splash can only be reproduced faithfully with
the corresponding complete BIOS set.

## Optional fixed-database package

Some frontends cannot load a custom software list or GnGeo driver. For those
frontends only, build:

```bash
make compat-cart SMB_ROM="/path/to/owned/smb.zip"
```

The result is:

```text
platform/neogeo/build/rom/puzzledp.zip
```

The archive contains SMBNeo data under filenames and CRCs recognized by the
fixed Puzzle De Pon driver:

| File | Size | CRC32 | Loader behavior |
| --- | ---: | ---: | --- |
| `202-p1.bin` | 512 KiB | `2b61415b` | 16-bit word swap at address zero |
| `202-s1.bin` | 128 KiB | `cd19264f` | linear |
| `202-m1.bin` | 128 KiB | `9c0291ea` | linear |
| `202-v1.bin` | 512 KiB | `debeb8fb` | linear |
| `202-c1.bin` | 1 MiB | `cc0095ef` | even sprite bytes |
| `202-c2.bin` | 1 MiB | `42371307` | odd sprite bytes |

The converter omits only verified zero/`FF` upper padding from the larger
native P/C regions. It never relocates the retained program data. CRC
correction is confined to the final four bytes of a verified padding run; a
live byte in an omitted or patchable range stops the conversion.

A fixed-database frontend will display and launch this archive as
**Puzzle De Pon**. That label is expected for this compatibility workaround,
but it is not the project’s title and should not be used for normal releases.

### Android NEO.emu and similar frontends

This is an optional compatibility path, not the default product. Copy both
files into the frontend’s scanned ROM directory:

```text
puzzledp.zip
neogeo.zip
```

Keep both filenames unchanged, rescan if needed, and launch the Puzzle De Pon
database entry. This arrangement has been confirmed on Android NEO.emu.
Other fixed-database frontends may have different BIOS database requirements.

## Browser player

The browser presents the game only as **Super Mario Bros. Neo**. After the
user selects their own supported game image, conversion happens entirely in
the browser and exposes two explicit downloads:

- **Download `smbneo.zip`** — the full canonical hardware/MAME/GnGeo
  cartridge.
- **Download `puzzledp.zip`** — the optional fixed-database package.

For in-page play, EmulatorJS uses the FBNeo core. FBNeo’s fixed driver
database requires the donor shortname internally, so the player launches
`puzzledp` behind the scenes while keeping the UI and product identity
SMBNeo. The browser keeps a separate FBNeo-specific P-ROM template; the
canonical download is built from the native P-ROM template and its six
regions are regression-checked against the native cartridge build.

The online player ships ngdevkit’s open replacement BIOS as a separate web
asset. It never embeds that BIOS into either downloaded game archive.

## BIOS and redistribution

Standalone emulator setups generally require a separate `neogeo.zip`.
Use a BIOS set that is legal for you to use and compatible with the chosen
emulator. This repository does not include a proprietary BIOS or original
game ROM data.

All generated ROM regions, archives, XML, and GnGeo data live under the
ignored `platform/neogeo/build/` directory. Do not commit or redistribute
generated game data unless you have the necessary rights.
