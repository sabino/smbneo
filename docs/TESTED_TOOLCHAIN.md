# Tested toolchain snapshot

## Current host and generator (2026-09-08)

The host regression suite and code generator have been validated with:

| Component | Tested version |
| --- | --- |
| Host GCC | 16.2.1 |
| Python / Pillow | 3.14.7 / 12.3.0 |
| Node.js | 26.8.1 |
| MoonBit | `moon 0.1.20260904`, `moonc v0.10.12+1634b282e` |
| MoonBit extra library | `moonbitlang/x@0.5.1` |

All 79 MoonBit tests pass, and isolated regeneration matches all five
checked-in generated C/header files byte-for-byte. The migration updates
parsing/error-display APIs and removes unused debug-print derivations on the
recursive control-flow graph; it does not change generated game code.

After a fresh MoonBit installation, initialize its registry before testing:

```bash
moon update
moon test
make -C platform/neogeo verify-codegen PKG_CONFIG=true
make ci
```

This host/generator result is separate from cross-compiler and physical-hardware
validation.

## Earlier full workstation snapshot (2026-07-25)

The previous full cross-toolchain and emulator validation used:

| Component | Tested version |
| --- | --- |
| Ubuntu | 24.04 |
| GNU Make | 4.3 |
| Host GCC | 13.3.0 |
| Python | 3.12.12 |
| Node.js | 24.11.0 |
| MoonBit | `moon 0.1.20260713`, `moonc v0.10.4+2cc641edf` |
| ngdevkit | `0.5+202607191609-17~ubuntu24.04.1` |
| ngdevkit toolchain | `0.1+202606181616-15~ubuntu24.04.1` |
| MC68000 GCC | 15.3.0 |
| ngdevkit GnGeo | `0.8.1+202606021654-11~ubuntu24.04.1` |
| MAME | 0.264 |
| Z80 assembler | SDCC `sdas` V02.00 |

These historical versions are not a strict dependency lock or the required OS.
Use the newer MoonBit/compiler-library pair above for the current generator.

The ROM-free host lane needs GNU Make, a C compiler, Python 3, Pillow, and
Node.js:

```bash
make ci
```

The full cross-target lane additionally needs MoonBit, ngdevkit, its MC68000
and Z80 toolchains, and the local GnGeo package:

```bash
make verify
```

The generated browser site also needs ngdevkit and its cross-toolchain:

```bash
make web
```

The VS arcade cartridge uses the same cross-toolchain. Its normal build uses
the checked-in direct-C runtime and needs only the user-owned, verified arcade
set; cc65 and the reference checkout are not runtime or release-build
dependencies:

```bash
make -C variants/vs neogeo-cart \
  VS_ROM="/path/to/suprmrio.zip"
# variants/vs/build/rom/vssmbneo.zip
# variants/vs/build/rom/vssmbneo.neo
# variants/vs/build/rom/gngeo_data.zip
# variants/vs/build/mame/hash/neogeo.xml
```

cc65 plus the pinned source/debug reference are needed only when regenerating
or differentially validating that direct-C core:

```bash
make -C variants/vs native-differential \
  VS_ROM="/path/to/suprmrio.zip" \
  VS_REFERENCE="/path/to/vs-reference"
```

See the [VS edition guide](../variants/vs/README.md) for the exact source-set
requirements and emulator commands.

The normal cartridge target creates the canonical full native package:

```bash
make cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/smbneo.zip
# platform/neogeo/build/rom/smbneo.neo
# platform/neogeo/build/rom/gngeo_data.zip
```

The ZIP and `.neo` outputs contain the same full P/S/M/V/C cartridge data.
The former serves MAME, GnGeo, and six-ROM hardware workflows; the latter is
the single-file NeoSD/NeoSD Pro image. To build only that image:

```bash
make neosd-cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/smbneo.neo
```

`make run` launches that archive as `smbneo` using the generated custom GnGeo
driver. The explicit hardware target produces the same canonical cartridge:

```bash
make hardware-cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/smbneo.zip
```

The donor package is optional:

```bash
make compat-cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/puzzledp.zip
```

Use it only for NEO.emu, FBNeo, EmulatorJS, or another frontend whose fixed
database cannot discover `smbneo`. Standalone frontends need a compatible
`neogeo.zip` BIOS alongside the game archive. A NeoSD `.neo` image likewise
contains only cartridge data; the BIOS is not embedded in either format.
See
[Emulator and cartridge packages](EMULATOR_COMPATIBILITY.md).

The canonical MAME lane additionally generates a local software list:

```bash
make mame-cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/smbneo.zip
# platform/neogeo/build/mame/hash/neogeo.xml
```

MAME launches that pair under `smbneo`; its `puzzledp` definition is only
the optional fixed-database compatibility fallback. The hardware-accurate
capture lane uses the canonical identity:

```bash
make mame-capture SMB_ROM="/path/to/smb.zip"
```

An exact BIOS test also needs the corresponding user-owned MAME BIOS archive.
The repository and build do not download or distribute BIOS images.
The ngdevkit replacement BIOS remains sufficient for cartridge-side rendering
checks, but its boot presentation is not an exact substitute.
