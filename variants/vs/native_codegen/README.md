# Direct-C backend development

The VS production cartridge uses the generated direct-C core checked in at
`../native/`. This directory contains a small ROM-free host harness for working
on that backend; it is not a second release runtime.

`tools/native_codegen.py` consumes a locally reconstructed 32 KiB VS program,
its ca65 debug map, and the structured source checkout as verification inputs.
It emits ordinary C functions, direct calls, static labels, and compile-time
table switches. It does not copy program bytes, source text, graphics, palette
data, or any other copyrighted asset into the generated C.

The generated ABI uses `SmbNeoNativeContext` with platform read/write
callbacks. The release core has no `VsCpu`, runtime program counter,
instruction fuel, page dispatcher, emulated JSR/RTS return frames, or runtime
6502 data stack. A/X/Y/status remain explicit migration state, while balanced
source save/restore sequences become bounded scalar C locals. Reviewed semantic
C modules replace hot routines only when their complete instruction
fingerprints match.

## Regenerate and verify

Use the pinned local reference checkout documented in the
[VS edition guide](../README.md):

```sh
make -C variants/vs native-reproducible \
  VS_PRG=/path/to/vs-reference/vsmain.vs.bin \
  VS_DEBUG=/path/to/vs-reference/vs.dbg \
  VS_SOURCE=/path/to/vs-reference
```

That command generates into the ignored `variants/vs/build/` tree, runs the
architecture checker, and compares all generated files with `../native/`.
Only reviewed, reproducible generator output should update the checked-in core.

The ROM-free generator and harness checks are:

```sh
make -C variants/vs native-test
make -C variants/vs/native_codegen test
```

The former also checks that the production generated file contains none of the
forbidden runtime machinery. The complete owned-ROM behavioral comparison is
`make -C variants/vs native-differential`; the instruction-level translated
runtime exists only as its development oracle.
