#!/usr/bin/env python3
"""Prepare verified user-owned VS data; generated files must remain outside Git."""
import argparse
import hashlib
import json
from pathlib import Path
from vs_rom import CHIPS, load_vs_rom


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    rom = load_vs_rom(args.input)  # Validate everything before creating outputs.
    outputs = {"vs-program.bin": rom.prg, "vs-chr.bin": rom.chr,
               "vs-reference.nes": rom.reference_image()}
    if rom.palette is not None:
        outputs["vs-palette.bin"] = rom.palette
    manifest = {
        "variant": "vssmbneo", "reference_set": "suprmrio",
        "revision": "SM4-4 E", "ppu": "RP2C04-0004",
        "chips": [chip.__dict__ for chip in CHIPS],
        "outputs": {name: {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                    for name, data in outputs.items()},
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        (args.output_dir / name).write_bytes(data)
    (args.output_dir / "vs-asset-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print("Verified canonical VS input: 32 KiB PRG, 16 KiB banked CHR/data")


if __name__ == "__main__":
    main()
