# Changelog

## Unreleased

- Added a pure-C MC68000 target with direct Neo Geo sprite/FIX/palette output.
- Added native Z80/YM2610 audio transport and bounded sound-cadence recovery.
- Restored the large title/menu presentation from locally generated assets.
- Mapped native controls and stabilized interactive 60 Hz host pacing.
- Replaced source-hardware scanline dropout with all-visible-sprite rendering.
- Added deterministic replay, cadence, audio, memory, cartridge, and
  reproducibility gates.
- Added a MAME validation lane and corrected real-LSPC full-height background
  chains plus BIOS-safe transparent FIX tile zero.
- Added ROM-free host CI.
- Added a ROM-free browser player with local game conversion, arrow-key
  controls, and GitHub Pages deployment.
- Focused the active fork branch on Neo Geo and removed the upstream desktop,
  original WebAssembly, and 3DS frontends.
