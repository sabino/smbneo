# VS core optimization checkpoints

The core remains experimental. These measurements do **not** establish normal
arcade speed or real AES/MVS hardware compatibility.

## Current changes

- Conservative flag liveness removes calculations whose results are overwritten
  before use. Page exits, calls, stack-status operations and idle returns preserve
  the full status. The unoptimized C path remains available for comparison.
- Native RAM-to-OAM transfers copy a known contiguous RAM page instead of doing
  256 general bus reads. Every RAM mirror and OAM wrap offset is tested. Non-RAM
  transfers keep their original bus semantics, including possible I/O reads.
- Register-state pointers explicitly describe their non-aliasing contract;
  copying the entire register state on page transitions was slower and rejected.

None of these changes alter the reviewed physical VRAM/palette routines, remove
sprites, skip game ticks or overclock the emulated Neo Geo.

## Measured results

MAME 0.289, MVS at its normal hardware clock, identical generated game input,
one NMI per game update. The runner disables host throttling only to finish the
test sooner; performance below counts **simulated display frames**, not wall time.

| Checkpoint | Before this pass | Optimized |
| --- | ---: | ---: |
| Display frames to game frame 600, including startup | 1,537 | 1,176 |

A separate controlled comparison keeps the same optimized core and disables
only the RAM DMA shortcut (`VS_REFERENCE_DMA`). Holding right/run with periodic
jumps from game frames 600 through 1200 gives:

| Moving section | General bus DMA | Native RAM DMA |
| --- | ---: | ---: |
| Display frames for 600 game updates | 2,113 | 1,793 |

That is about 18% higher throughput in this moving sequence, but still roughly
three display periods per game update. More core work is required; do not
advertise the startup result as gameplay FPS or all-stage performance.

## Validation

`make program-compare` hashes the same per-frame CPU/RAM/video/APU-register state
from reference and optimized builds over 24,240 frames: the initial 1,200-frame
sequence, all eight coinage settings, and 600-frame sequences in all 32 stages.
These are stage-load/short-play checks, not completed playthroughs or audio
waveform comparisons. The unchanged fingerprint for the verified local input
is `e25a9b510e964cc7`.

The cartridge also passes the ELF hardware-safety gate, MVS real coin/start,
AES shortcut coin/start, and a 4 KiB target-versus-host RAM comparison at game
frame 180. No generated game data belongs in Git.
