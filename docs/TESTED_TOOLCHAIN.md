# Tested toolchain snapshot

The current workstation validation snapshot, last refreshed on 2026-07-25,
uses:

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

These versions describe one validated workstation; they are not a strict
dependency lock.

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

The normal cartridge target creates the fixed-database emulator package:

```bash
make cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/puzzledp.zip
```

The full-size hardware/MAME/GnGeo package remains a separate target:

```bash
make hardware-cart SMB_ROM="/path/to/smb.zip"
# platform/neogeo/build/rom/smbneogeo.zip
```

Standalone FBNeo and NEO.emu need a compatible `neogeo.zip` BIOS alongside
`puzzledp.zip`; it is not embedded in the game archive. See
[Emulator and cartridge packages](EMULATOR_COMPATIBILITY.md).

The hardware-accurate video lane additionally needs MAME:

```bash
make mame-capture SMB_ROM="/path/to/smb.zip"
```

An exact BIOS test also needs the corresponding user-owned MAME BIOS archive.
The repository and build do not download or distribute BIOS images.
The ngdevkit replacement BIOS remains sufficient for cartridge-side rendering
checks, but its boot presentation is not an exact substitute.
