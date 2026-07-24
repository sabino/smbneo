# Third-party notices

This is an unofficial open-source port. It is not affiliated with Nintendo or
SNK, and it does not include a game ROM, BIOS, or generated cartridge.

## nathsou/smb

The repository began as a fork of
[`nathsou/smb`](https://github.com/nathsou/smb). That project includes an
Apache License 2.0 file, which is retained as [LICENSE](LICENSE), and provides
the MoonBit translator, generated C runtime, desktop/web targets, and project
structure. Neo Geo-specific changes are modifications made after that fork.

## SMBDIS and underlying game logic/data

[`src/smb.asm`](src/smb.asm) identifies doppelganger's comprehensive
disassembly as its source and preserves its original notice. The translated
files under `codegen/` are generated from that input. The project license does
not grant rights to Nintendo names, characters, graphics, audio, or other game
assets.

## raylib-quickstart

The optional desktop target references
[`raylib-extras/raylib-quickstart`](https://github.com/raylib-extras/raylib-quickstart)
as a Git submodule pinned at
`759d3ff61a88381fbb063bb3c8d9393ce330d278`. Its own repository contains the
applicable license and bundled-dependency notices. GitHub-generated source
archives do not include submodule contents.

## ngdevkit and GnGeo

The Neo Geo target builds against external
[`ngdevkit`](https://github.com/dciabrin/ngdevkit) and uses its GnGeo package
for emulator testing. Those tools are not vendored here and retain their own
licenses and component notices. BIOS/support archives are local runtime
dependencies and are not part of this project.

## FCEUX visual-reference palette

The named RGB target in `tools/gen_neogeo_palette.py` reproduces the default
six-bit palette values from
[`FCEUX 2.2.1`](https://github.com/TASEmulators/fceux/tree/fceux-2.2.1/src/palettes).
FCEUX is distributed under the GNU General Public License version 2 or later;
the source link and profile version are retained in the generator.

## User-supplied game image

The asset generator accepts one specifically identified local game-image
revision and reads its CHR bank. Generated C-ROM/S-ROM/title data and packaged
cartridges remain local build outputs.
