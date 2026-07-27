# Emulator and cartridge packages

SMBNeo has one canonical identity, available in two native package formats,
and one optional loader workaround:

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

The results are:

```text
platform/neogeo/build/rom/smbneo.zip
platform/neogeo/build/rom/smbneo.neo
```

These contain the same authoritative hardware-native cartridge data. The ZIP
keeps the six native regions as separate files:

| File | Size | Purpose |
| --- | ---: | --- |
| `smbneo-p1.p1` | 1 MiB | MC68000 program |
| `smbneo-s1.s1` | 128 KiB | FIX-layer graphics |
| `smbneo-m1.m1` | 128 KiB | Z80 sound program |
| `smbneo-v1.v1` | 512 KiB | YM2610 sample data |
| `smbneo-c1.c1` | 2 MiB | even sprite data |
| `smbneo-c2.c2` | 2 MiB | odd sprite data |

`make hardware-cart` builds the canonical six-ROM ZIP without also packaging
the NeoSD image. The extra name exists for scripts and documentation that want
to make the physical-chip/ZIP intent explicit.

Use `smbneo.zip` for physical EPROM/flashcart workflows, the generated MAME
software list, and the project's generated GnGeo driver. Use `smbneo.neo`
with a TerraOnion NeoSD or NeoSD Pro. Do not substitute the smaller donor
archive in either hardware workflow.

## NeoSD and NeoSD Pro

Generate only the single-file NeoSD image with:

```bash
make neosd-cart SMB_ROM="/path/to/owned/smb.zip"
```

The result is:

```text
platform/neogeo/build/rom/smbneo.neo
```

Copy that file to the NeoSD card and select **Super Mario Bros. Neo** from the
cartridge menu. No Neo Geo BIOS is stored inside the file; the console or
emulator supplies its own BIOS.

The format, metadata, and complete payload have been checked byte for byte
against ngdevkit's writer and the pinned open-source `neosdconv` implementation.
That establishes converter interoperability, but it is not a substitute for a
physical-device test. Booting this image on an actual NeoSD or NeoSD Pro is
still awaiting confirmation from a device owner.

The image follows the documented NeoSD v1 layout: a 4 KiB `NEO\x01` header,
followed by P, S, M, V, and byte-interleaved C data. Its metadata records the
project title, 2026, the Platformer genre, `Community port`, and the project's
unofficial NGH value `0x534d`. P data remains in native byte order and the
full hardware-sized regions are preserved. The final image is 6,033,408
bytes. A validator checks the header, exact end of file, reserved bytes, and
canonical title/year/genre/NGH metadata, plus every transformed payload byte
after generation.

The format itself has no embedded checksum field. TerraOnion's separate
NeoValidator catalog may report an unknown homebrew file; that is not a
malformed-image result. The project instead verifies the generated payload
against its source regions and checks reproducibility byte for byte.

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
the browser and exposes three explicit downloads:

- **Download `smbneo.zip`** — the full canonical hardware/MAME/GnGeo
  cartridge.
- **Download `smbneo.neo`** — the same full native cartridge in the
  single-file NeoSD/NeoSD Pro format.
- **Download `puzzledp.zip`** — the optional fixed-database package.

For in-page play, EmulatorJS uses the FBNeo core. FBNeo’s fixed driver
database requires the donor shortname internally, so the player launches
`puzzledp` behind the scenes while keeping the UI and product identity
SMBNeo. The browser keeps a separate FBNeo-specific P-ROM template; the
canonical download is built from the native P-ROM template and its six
regions are regression-checked against the native cartridge build.

The online player ships ngdevkit's open replacement BIOS as a separate web
asset. It never embeds that BIOS into any downloaded game package. The `.neo`
file is assembled in browser memory directly from the canonical regions,
before the separate FBNeo donor conversion occurs.

## NeoSD format references

- [TerraOnion NeoBuilder guide](https://wiki.terraonion.com/index.php/Neobuilder_Guide)
  documents the header, payload order, and region transformations.
- [ngdevkit](https://github.com/dciabrin/ngdevkit) supplies the native
  command-line packer used by `make neosd-cart`.
- [neosdconv](https://github.com/city41/neosdconv) is the independent,
  open-source converter used for the manual byte-for-byte cross-check. It is
  not a project build dependency.

## BIOS and redistribution

Standalone emulator setups generally require a separate `neogeo.zip`.
Use a BIOS set that is legal for you to use and compatible with the chosen
emulator. This repository does not include a proprietary BIOS or original
game ROM data.

All generated ROM regions, archives, `.neo` images, XML, and GnGeo data live under the
ignored `platform/neogeo/build/` directory. Do not commit or redistribute
generated game data unless you have the necessary rights.
