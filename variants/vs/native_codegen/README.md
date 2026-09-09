# Direct-C backend prototype

This directory is the first isolated native-C backend milestone for Vs.
Super Mario Bros. It is intentionally separate from the verified translated
oracle in `variants/vs/build/`.

`tools/native_codegen.py` consumes the linked 32 KiB VS PRG, its ca65 debug
map, and the structured source checkout only as verification inputs. It emits
direct C functions for the complete marked instruction graph, including
static labels, direct calls, ordinary returns, and compile-time table
switches. It does not copy ROM bytes, source text, or copyrighted assets into
the repository.

The generated ABI uses `SmbNeoNativeContext` and ordinary C functions. The
context takes `SmbNeoNativeRead8`/`SmbNeoNativeWrite8` callbacks plus an
opaque platform pointer, so a NeoGeo bus/PPU adapter can own memory and I/O.
The prototype has no emulated program counter, instruction budget, page
dispatcher, 6502 JSR/RTS return frames, runtime data stack, or dependency on
`VsCpu`. Balanced source saves are proven during generation and become
ordinary scalar C locals. Title and game mode selection are explicit C
switches. A/X/Y/status remain migration state while individual routine bodies
are replaced incrementally with reviewed semantic C.

Example (using a local reference checkout):

```sh
python3 tools/native_codegen.py \
  --prg /path/to/vsmain.vs.bin \
  --debug /path/to/vs.dbg \
  --source-root /path/to/vs-reference \
  --output-dir variants/vs/native_codegen/generated
python3 tools/check_native_codegen.py \
  variants/vs/native_codegen/generated/smbneo_native.c
cc -std=c99 -Wall -Wextra -Werror \
  -Ivariants/vs/native_codegen/generated -c \
  variants/vs/native_codegen/generated/smbneo_native.c
```

The generated directory is for local experiments and should remain ignored;
the test suite uses synthetic ROM-free fixtures.
