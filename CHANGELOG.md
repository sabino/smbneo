# Changelog

## Unreleased

- Added a pure-C MC68000 target with direct Neo Geo sprite/FIX/palette output.
- Added native Z80/YM2610 audio transport and bounded sound-cadence recovery.
- Restored the large title/menu presentation from locally generated assets.
- Mapped native controls and stabilized interactive 60 Hz host pacing.
- Replaced source-hardware scanline dropout with all-visible-sprite rendering.
- Added deterministic replay, cadence, audio, memory, cartridge, and
  reproducibility gates.
- Added ROM-free host CI.
- Focused the active fork branch on Neo Geo and removed the upstream desktop,
  WebAssembly, and 3DS frontends.
