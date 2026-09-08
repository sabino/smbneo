"""Verified, local-only input for the canonical arcade VS variant.

Chip identities follow MAME's suprmrio (SM4-4 E) declaration. No game bytes
are distributed here. This module must not accept the home NES ROM as VS.
"""
from dataclasses import dataclass
import hashlib
from pathlib import Path
import zipfile
import zlib


@dataclass(frozen=True)
class Chip:
    name: str
    size: int
    crc32: str
    sha1: str


CHIPS = (
    Chip("mds-sm4-4__1dor6d_e.1d or 6d", 8192, "be4d5436", "08162a7c987f1939d09bebdb676f596c86abf465"),
    Chip("mds-sm4-4__1cor6c_e.1c or 6c", 8192, "5e3fb550", "de4494e4dd52f7f7b04cf1d9019fd89fb90eaca9"),
    Chip("mds-sm4-4__1bor6b_e.1b or 6b", 8192, "b1b87893", "8563ceaca664cf4495ef1020c07179ca7e4af9f3"),
    Chip("mds-sm4-4__1aor6a_e.1a or 6a", 8192, "1abf053c", "f17db88ce0c9bf1ed88dc16b9650f11d10835cec"),
    Chip("mds-sm4-4__2bor8b_e.2b or 8b", 8192, "42418d40", "22ab61589742cfa4cc6856f7205d7b4b8310bc4d"),
    Chip("mds-sm4-4__2aor8a_e.2a or 8a", 8192, "15506b86", "69ecf7a3cc8bf719c1581ec7c0d68798817d416f"),
)
PALETTE = Chip("rp2c04-0004.pal", 192, "0c2e8e4d", "0f9090225eb1f08ae5072d40af3e95547cbce05f")
# Upstream reconstruction container, NOT an emulator's VS mapper declaration.
REFERENCE_HEADER = b"NES\x1a" + bytes((2, 2, 0x31, 0)) + bytes(8)


def verify_chip(data: bytes, chip: Chip) -> None:
    if (len(data) != chip.size or f"{zlib.crc32(data):08x}" != chip.crc32
            or hashlib.sha1(data).hexdigest() != chip.sha1):
        raise ValueError(f"Wrong canonical VS chip: {chip.name}")


@dataclass(frozen=True)
class VsRom:
    prg: bytes
    chr: bytes
    palette: bytes | None = None

    def reference_image(self) -> bytes:
        return REFERENCE_HEADER + self.prg + self.chr


def load_vs_rom(path: Path) -> VsRom:
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        required = {chip.name for chip in CHIPS}
        if len(names) != len(set(names)):
            raise ValueError("Duplicate VS archive members")
        if set(names) not in (required, required | {PALETTE.name}):
            raise ValueError("Expected canonical suprmrio chip names (SM4-4 E)")
        data = []
        palette = None
        for chip in (*CHIPS, PALETTE):
            if chip.name not in names:
                continue
            if archive.getinfo(chip.name).file_size != chip.size:
                raise ValueError(f"Wrong VS chip size: {chip.name}")
            payload = archive.read(chip.name)
            verify_chip(payload, chip)
            if chip == PALETTE:
                palette = payload
            else:
                data.append(payload)
    return VsRom(b"".join(data[:4]), b"".join(data[4:]), palette)
