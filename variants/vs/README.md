# VS. Super Mario Bros. Neo — development variant

This is a separate arcade-game port. The normal SMBNeo game and its default
cartridges remain unchanged. This variant is experimental, not a hardware-tested
release.

The arcade program is statically translated into C using the named instruction
boundaries in Matthew Gilmore's [meta-disassembly](https://gitlab.com/segaloco/smb).
It retains the VS game logic rather than adding arcade-looking menus to the home
game. No runtime opcode interpreter is used.

The target generator recovers named C functions and direct calls from the
source's call graph and explicit state tables. Handwritten C replacements live
in `vs_fast_paths.h`; the generator selects them only after checking the complete
routine's instruction fingerprint. Improving a generated routine also means
updating its generator or replacement, so rebuilding does not discard the work.
Generated game source remains a local build product.

## Build locally

Use your own canonical `suprmrio.zip` (SM4-4 E). A home NES ROM is not accepted.
The archive must contain the six canonical chips; the Neo Geo graphics build also
requires its `rp2c04-0004.pal`. All generated game data stays under ignored `build/`
directories. Never commit it or a BIOS.

The current reference revision is `238f61930e2b5024cb17cf70975e637ee0305f8f`.
Build it in an isolated checkout with cc65 and the explicit VS definitions below;
`make smb.vs` alone does not select the VS program. After preparing the input with
the first command, pass `build/assets/vs-reference.nes` to the reference checkout's
`tools/extract.vs.sh`, then build the reference there:

```sh
# From this directory:
make reference VS_ROM=/path/to/suprmrio.zip

# In the isolated reference checkout, after extracting the prepared input:
make -B -j2 smb.vs check_vs \
  ASFLAGS_TGT='-DOG_PAD -DOG_FALL -DVS -DVS_2C04_0004 -DVS_SMB -DSMBV1' \
  APPFLAGS_TGT='-g -U -I . -I ./include' \
  LDFLAGS_VS='-C link.vs.ld --dbgfile vs.dbg -Ln vs.lbl -m vs.map'

# Back in this directory, with the ngdevkit toolchain on PATH:
make translate neogeo-assets \
  VS_ROM=/path/to/suprmrio.zip VS_REFERENCE=/path/to/reference
make -j2 neogeo-cart
```

The reference must reconstruct the user's PRG exactly before translation is
allowed. Generated C and data are private build products, not source to add to Git.

Outputs are `build/rom/vssmbneo.zip` and `build/rom/vssmbneo.neo`. Both retain the
full native cartridge layout; the NeoSD header and P-ROM use packed-BCD ID `2027`.
The original `smbneo.zip` and `smbneo.neo` are not overwritten.

## Run and test

MAME uses the generated custom software list, not a donor identity:

```sh
mame ng_mv1 vssmbneo \
  -hashpath build/mame/hash \
  -rompath 'build/rom;/path/to/your/bios' -noplugins
```

Supply your own appropriate Neo Geo BIOS separately. The SDK's replacement BIOS
files may produce checksum warnings against MAME's original BIOS database.
The generated `build/rom/gngeo_data.zip` supplies the custom GnGeo driver; fixed
database emulators do not automatically recognize this new archive.

Neo Geo A/B jump, C/D run or fire. Start selects one player; player-two Start
selects two players. MVS coin inputs add credits. On AES, hold Start and C or D
to insert a coin, release them, then press Start. DIP settings currently default
to the canonical arcade configuration.

There is also a silent SDL diagnostic player for testing the translated C without
the Neo Geo target: `make host`, then `build/vs_host build/assets 0`. Use arrows,
Z/Space to jump, X/Left Shift to run, 5 for coin, Enter for one player, 2 for two
players, and Escape to close. It is a development aid, not the Neo Geo emulator.

`make test` runs ROM-free tests. `make program-test` additionally exercises the
owned-ROM translation: all DIP combinations, eight coinage settings, startup,
coin/start, and all 32 stage loads with 600-frame input sequences per stage.
`make program-test-native` runs those scenarios on the optimized C core.
`make kernel-compare` checks individual semantic replacements against their
original instruction-level implementations. ROM-free generated-C tests cover
nested calls, shared tails, changed return addresses, table dispatch, interrupt
returns, bounded recursion, and resuming with very small execution budgets.
Passing these does not mean every stage and ending has been played through.

For a bounded Neo Geo integration test, from the repository root:

```sh
python3 tools/run_vs_mame.py --build variants/vs/build \
  --bios-dir /path/to/your/bios --real-coin --game-frames 1200 \
  --label my-checkpoint
```

This tests the actual MVS coin input, credit consumption, and right/run/jump in
gameplay. It rejects unexpected game restarts and uses a fresh output directory
for each run. A 1200-tick run also reports the isolated 600-to-1200 gameplay
interval as display frames per game tick; `--metrics-json path.json` can save
that result. Omit `--real-coin` to test the AES-friendly shortcut; add
`--system aes` for the AES machine. The VS integration keeps the BIOS in game
mode while VS handles its own attract screen and credits, preventing the BIOS
from restarting the port on coin insertion.
Physical coin bits are read only in MVS mode; unused AES bits must not be
interpreted as permanently pressed coin switches.

Performance is still work in progress. Measure simulated display frames per
game frame, not MAME's unthrottled host-speed percentage. The shorter 600-frame
test includes attract/title screens and is not a standalone gameplay FPS test.

The C core batches complete background columns and display-list transfers,
enemy and player sprite composition, and moving-platform rendering. Player
physics and animation select the arcade game's existing tables directly;
acceleration, braking, scrolling, gravity and common collision routines use C
arithmetic. Common-enemy states share one C movement policy, including falling,
shells, defeat and recovery; cannon scheduling uses a single slot scan. These
follow the regular port's optimization approach while retaining the arcade game's
movement and clipping rules. The physical Neo Geo renderer is shared with SMBNeo.

Known calls and state-table selections execute as ordinary C calls, without
returning through a page dispatcher each time. The original RAM stack remains
authoritative: unusual/computed transfers safely unwind to the fallback path.
Native call chains are limited to 16 executing C levels. The cross build emits
and validates the compiler's `build/vs_program.m68k.su` report, rejecting
unknown/unbounded records or frames above 96 bytes. The fallback can be
selected for diagnostics with `translate_vs.py --no-native-functions` or
`-DVS_PAGE_DISPATCH_ONLY`; neither changes the game's frame cadence.

## Hardware safeguards inherited from SMBNeo

The VS renderer reuses the hardware-correct path, including these reviewed fixes:

- `edcab1b`: full-height LSPC background strips and explicit palette-latch setup.
- `6369caa`: pre-oriented C-ROM tiles; no shrink-dependent hardware flip bits.
- `38af616`: timed word-sized VRAM accesses, hidden-buffer updates, bounded live
  commits, analog palette reference, packed-BCD identity and startup RAM guard.
- `6a76271`: independent address/data pairs for every OAM visibility write.
- `9ae243f`: alignment requirements for cartridge-resident patchable data.

The cartridge build inspects the linked ELF for RAM bounds, VRAM access forms,
palette initialization and absence of memory-card access. Regression tests guard
the reviewed bus routines and both VS graphics banks. Real AES/MVS testing is
still required; emulator screenshots cannot establish hardware compatibility.
