# Changelog

## Unreleased

## 0.1.0 - 2026-07-28

- Added a pure-C MC68000 target with direct Neo Geo sprite/FIX/palette output.
- Added native Z80/YM2610 audio transport and bounded sound-cadence recovery.
- Restored the large title/menu presentation from locally generated assets.
- Mapped native controls and stabilized interactive 60 Hz host pacing.
- Replaced source-hardware scanline dropout with all-visible-sprite rendering.
- Added deterministic replay, cadence, audio, memory, cartridge, and
  reproducibility gates.
- Added a MAME validation lane and corrected real-LSPC full-height background
  chains plus BIOS-safe transparent FIX tile zero.
- Replaced runtime LSPC flipping of shrunken OAM tiles with pre-oriented C-ROM
  banks after an MV1C test exposed corrupt alternating composite-sprite poses.
- Added ROM-free host CI.
- Added a ROM-free browser player with local game conversion, arrow-key
  controls, and GitHub Pages deployment.
- Made the full native `smbneo.zip` the canonical hardware, MAME, and custom
  GnGeo output, with the visible title **Super Mario Bros. Neo**.
- Kept `puzzledp.zip` as an optional fixed-database compatibility package and
  as the internal FBNeo/EmulatorJS launch identity only.
- Added local browser downloads for the canonical ZIP, NeoSD/NeoSD Pro image,
  and optional fixed-database package without uploading or committing
  user-supplied game data.
- Added reproducible `smbneo.neo` generation and independent validation for
  command-line and browser workflows.
- Hardened AES/MVS video handoff and frame presentation for physical hardware:
  initialized both palette banks with a valid backdrop entry, forced safe LSPC
  MMIO access widths, double-buffered background/OAM state, and enforced a
  measured VBlank budget.
- Added build-time audits for NeoSD cartridge metadata, unsafe Neo Geo address
  accesses, and memory-card APIs, vectors, parameter blocks, and address ranges.
- Focused the active fork branch on Neo Geo and removed the upstream desktop,
  original WebAssembly, and 3DS frontends.
