# VS native-core optimization

The production VS cartridge runs a CPU-free direct-C game core. Performance
work therefore targets ordinary C control flow, data access, actor scheduling,
collision, composition, and Neo Geo presentation. The older instruction-level
translation is retained only as an exact behavioral oracle.

## Rules for an optimization

An accepted change must:

- preserve arcade logic, credits, DIP behavior, level data, PPU-visible state,
  APU write ordering, and source fixed-point arithmetic;
- keep the release ELF free of `VsCpu`, runtime PC/fuel fields, page dispatch,
  and emulated call/return-stack machinery;
- keep the reviewed hardware renderer, palette reference, startup guard, and
  memory-card prohibition intact;
- avoid frame skipping, game-tick skipping, Neo Geo overclocking, sprite-limit
  emulation, or removal of live actors; and
- pass an exact differential before replacing a generated routine with a
  semantic C implementation.

The generator activates a semantic replacement only when the complete source
routine or slice fingerprint matches. This prevents a future source change
from silently selecting an implementation that was reviewed against different
instructions.

## What is native today

The checked-in core uses direct C functions and compile-time table selections.
Reviewed semantic modules cover hot paths including:

- the main game/NMI coordination;
- actor scheduling, common movement policies, and rendering;
- fixed-point movement and gravity;
- collision-box and position calculations;
- background/meta-tile work and display-list writes; and
- OAM initialization/shuffling and other repeated video work.

Unconverted routines still execute as generated direct C. They do not fall
back to a runtime CPU interpreter or page dispatcher in the release cartridge.

## Validation

Run the ROM-free architecture and semantic-module tests with:

```sh
make -C variants/vs native-test
```

Run the complete owned-ROM comparison with:

```sh
make -C variants/vs native-differential \
  VS_ROM="/path/to/suprmrio.zip" \
  VS_REFERENCE="/path/to/vs-reference"
```

That comparison covers all eight coinage modes, every DSW byte, all 32 stage
loads, long input sequences, exact RAM outside the intentionally unused source
processor-stack page, PPU/APU state, registers/status, and ordered audio writes.

The final cross build also runs `tools/check_neogeo_elf.py --variant vs-native`.
It checks ROM/RAM bounds, cartridge identity, required data, safe LSPC access,
palette setup, startup restoration, no memory-card access, and absence of the
legacy CPU/dispatcher symbols.

## Measuring cadence

Use MAME at the stock emulated Neo Geo clock and count simulated display
periods per completed game tick. Host-speed percentage and an unthrottled
wall-clock benchmark do not measure cartridge cadence. The reproducible runner
is:

```sh
python3 tools/run_vs_mame.py \
  --build variants/vs/build \
  --bios-dir /path/to/mame/roms \
  --system ng_mv1 --real-coin --game-frames 1200 \
  --metrics-json /tmp/vssmbneo-metrics.json \
  --label cadence
```

The 600-to-1200 interval excludes most boot/title cost and is more useful than
the cumulative startup number. Retest crowded stages, moving platforms, and
audio-active scenes separately: a light World 1-1 sample is not a whole-game
performance result.

MAME and GnGeo are complementary checks. MAME exercises the generated custom
software list and stricter hardware behavior; the project GnGeo path checks its
generated custom driver and interactive cadence. Neither replaces a stock-clock
AES/MVS flashcart test.
