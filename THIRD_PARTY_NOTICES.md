# Third-party notices

This is an unofficial open-source port. It is not affiliated with Nintendo or
SNK, and the repository does not include a game ROM, BIOS, or generated
cartridge.

## nathsou/smb

The repository began as a fork of
[`nathsou/smb`](https://github.com/nathsou/smb). That project includes an
Apache License 2.0 file, which is retained as [LICENSE](LICENSE), and provides
the MoonBit translator, generated C runtime, and project structure. Neo
Geo-specific changes are modifications made after that fork. The upstream
repository retains its desktop, WebAssembly, and 3DS frontends; they are not
part of this target-focused branch.

## SMBDIS and underlying game logic/data

[`src/smb.asm`](src/smb.asm) identifies doppelganger's comprehensive
disassembly as its source and preserves its original notice. The translated
files under `codegen/` are generated from that input. The project license does
not grant rights to Nintendo names, characters, graphics, audio, or other game
assets.

## ngdevkit, NullBIOS, and GnGeo

The Neo Geo target builds against external
[`ngdevkit`](https://github.com/dciabrin/ngdevkit) and uses its GnGeo package
for emulator testing. Those tools are not vendored here and retain their own
licenses and component notices. The web build packages ngdevkit's open-source
NullBIOS into its generated deployment artifact; no commercial BIOS is
included in the repository or site.

## Browser player

The optional web player loads
[`EmulatorJS 4.2.3`](https://github.com/EmulatorJS/EmulatorJS), including its
FBNeo core, and [`fflate 0.8.2`](https://github.com/101arrowz/fflate) from
public CDNs. These projects are not vendored here and retain their own license
terms. EmulatorJS is distributed under GPL-3.0 and fflate under MIT.

Files selected in the browser are processed locally and are not uploaded by
SMBNeo.

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
