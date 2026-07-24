# Tested toolchain snapshot

The optimized port was last fully verified on 2026-07-24 with:

| Component | Tested version |
| --- | --- |
| Ubuntu | 24.04 |
| GNU Make | 4.3 |
| Host GCC | 13.3.0 |
| Python | 3.12.12 |
| Node.js | 24.11.0 |
| ngdevkit | `0.5+202607191609-17~ubuntu24.04.1` |
| ngdevkit toolchain | `0.1+202606181616-15~ubuntu24.04.1` |
| MC68000 GCC | 15.3.0 |
| ngdevkit GnGeo | `0.8.1+202606021654-11~ubuntu24.04.1` |
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
