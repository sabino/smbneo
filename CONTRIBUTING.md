# Contributing

Contributions to the game logic, Neo Geo platform code, tests, and
documentation are welcome.

Before opening a pull request:

1. Run `make ci`.
2. If the Neo Geo toolchain is installed, run
   `make verify`.
3. Keep generated C synchronized with its MoonBit source. Do not edit only
   `codegen/lib/` when the generator is the authoritative source.
4. Explain any performance claim with a repeatable stock-clock benchmark and
   a state/image equivalence check.

Do not commit game ROMs, BIOS files, generated cartridge regions, or packaged
cartridges. Tests should use synthetic fixtures; if a test needs local game
assets, document how maintainers can reproduce it from their own input.
