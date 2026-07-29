#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_neogeo_elf as checker  # noqa: E402


def startup_symbols(
    *,
    bss_start: int = 0x2C64,
    bss_size: int = 0x3758,
    bss_ram: int = 0x100000,
    data_start: int | None = None,
    data_size: int = 0x20,
    data_ram: int | None = None,
) -> dict[str, int]:
    if data_start is None:
        data_start = bss_start
    if data_ram is None:
        data_ram = bss_ram + bss_size
    return {
        "__bss_start": bss_start,
        "__bss_end": bss_start + bss_size,
        "__bss_start_in_ram": bss_ram,
        "__data_start": data_start,
        "__data_end": data_start + data_size,
        "__data_start_in_ram": data_ram,
    }


class NmSymbolTests(unittest.TestCase):
    def test_parses_address_type_and_name_and_ignores_undefined(self) -> None:
        symbols = checker.parse_nm_symbols(
            """\
00100000 R __bss_start_in_ram
00002026 A rom_NGH_ID
         U memset
00100000 b .bss
"""
        )

        self.assertEqual(symbols["__bss_start_in_ram"], 0x100000)
        self.assertEqual(symbols["rom_NGH_ID"], 0x2026)
        self.assertNotIn("memset", symbols)

    def test_rejects_conflicting_duplicate_symbol_values(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "conflicting values"):
            checker.parse_nm_symbols(
                "00002026 A rom_NGH_ID\n0000534d A rom_NGH_ID\n"
            )


class StartupLayoutTests(unittest.TestCase):
    def test_models_ngdevkit_dbf_loop_sizes_exactly(self) -> None:
        self.assertEqual(checker.ngdevkit_bss_clear_size(0), 32)
        self.assertEqual(checker.ngdevkit_bss_clear_size(1), 32)
        self.assertEqual(checker.ngdevkit_bss_clear_size(31), 32)
        self.assertEqual(checker.ngdevkit_bss_clear_size(32), 64)
        self.assertEqual(checker.ngdevkit_bss_clear_size(0x3758), 0x3760)
        self.assertEqual(checker.ngdevkit_data_copy_size(0), 0)
        self.assertEqual(checker.ngdevkit_data_copy_size(31), 31)

    def test_accepts_data_guard_covering_the_bss_overclear(self) -> None:
        self.assertEqual(
            checker.validate_startup_layout(startup_symbols()),
            (0x3758, 0x20, 0x103760, 0x103778),
        )

    def test_rejects_data_that_does_not_restore_bss_overclear(self) -> None:
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "does not restore the ngdevkit BSS over-clear",
        ):
            checker.validate_startup_layout(startup_symbols(data_size=4))

    def test_rejects_noncontiguous_bss_and_data(self) -> None:
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "not immediately after BSS",
        ):
            checker.validate_startup_layout(
                startup_symbols(data_ram=0x104000)
            )

    def test_rejects_startup_below_cartridge_work_ram(self) -> None:
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "does not start at cartridge work RAM",
        ):
            checker.validate_startup_layout(
                startup_symbols(bss_ram=0x0FFFE0)
            )

    def test_rejects_each_startup_write_past_safe_ram(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "BSS clear crosses"):
            checker.validate_startup_layout(
                startup_symbols(
                    bss_size=0xF300,
                    data_size=0x20,
                )
            )

        with self.assertRaisesRegex(checker.ElfCheckError, "data copy crosses"):
            checker.validate_startup_layout(
                startup_symbols(
                    bss_size=0xF2D0,
                    data_size=0x40,
                )
            )

    def test_rejects_missing_or_reversed_linker_ranges(self) -> None:
        missing = startup_symbols()
        del missing["__data_end"]
        with self.assertRaisesRegex(checker.ElfCheckError, "symbols missing"):
            checker.validate_startup_layout(missing)

        reversed_bss = startup_symbols()
        reversed_bss["__bss_end"] = reversed_bss["__bss_start"] - 1
        with self.assertRaisesRegex(checker.ElfCheckError, "precedes"):
            checker.validate_startup_layout(reversed_bss)


class NghIdentityTests(unittest.TestCase):
    def test_accepts_only_the_project_packed_bcd_identity(self) -> None:
        self.assertTrue(checker.is_packed_bcd(0x2026))
        self.assertEqual(
            checker.validate_ngh_id({"rom_NGH_ID": 0x2026}),
            0x2026,
        )

    def test_rejects_missing_non_bcd_and_other_bcd_ids(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "missing"):
            checker.validate_ngh_id({})
        with self.assertRaisesRegex(checker.ElfCheckError, "not four-digit"):
            checker.validate_ngh_id({"rom_NGH_ID": 0x2A26})
        with self.assertRaisesRegex(checker.ElfCheckError, r"expected \$2026"):
            checker.validate_ngh_id({"rom_NGH_ID": 0x2025})


class TitleDataAlignmentTests(unittest.TestCase):
    def test_accepts_word_aligned_title_data(self) -> None:
        self.assertEqual(
            checker.validate_title_data_alignment(
                {"neogeo_title_screen_data": 0x04C2}
            ),
            0x04C2,
        )

    def test_rejects_missing_or_odd_title_data(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "symbol missing"):
            checker.validate_title_data_alignment({})
        with self.assertRaisesRegex(checker.ElfCheckError, "odd P-ROM"):
            checker.validate_title_data_alignment(
                {"neogeo_title_screen_data": 0x04C1}
            )


class LspcStoreTests(unittest.TestCase):
    def test_accepts_absolute_long_vram_writes(self) -> None:
        disassembly = """\
000032e6 <write_vram>:
    32e0: 33c4 003c 0000  movew %d4,3c0000 <REG_VRAMADDR>
    32e6: 33c5 003c 0002  movew %d5,3c0002 <REG_VRAMRW>
    32ec: 4e71            nop
    32ee: 33fc 0000 003c  movew #0,3c0002 <REG_VRAMRW>
    32f4: 0002
    32f6: 33c6 003c 0004  movew %d6,3c0004 <REG_VRAMMOD>
"""
        self.assertEqual(
            checker.validate_lspc_vram_stores(disassembly),
            (1, 2, 1),
        )

    def test_rejects_indirect_write_through_known_vram_pointer(self) -> None:
        disassembly = """\
0000c040 <write_vram>:
    c040: 43f9 003c 0002  lea 3c0002 <REG_VRAMRW>,%a1
    c046: 3280            movew %d0,%a1@
"""
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "unsafe indirect LSPC",
        ):
            checker.validate_lspc_vram_stores(disassembly)

    def test_rejects_offset_write_through_lspc_base_pointer(self) -> None:
        disassembly = """\
0000c040 <write_vram>:
    c040: 43f9 003c 0000  lea 3c0000 <REG_VRAMADDR>,%a1
    c046: 3380 0002       movew %d0,%a1@(2)
"""
        with self.assertRaisesRegex(checker.ElfCheckError, "indirect LSPC"):
            checker.validate_lspc_vram_stores(disassembly)

    def test_rejects_non_word_direct_lspc_write(self) -> None:
        disassembly = """\
0000c040 <write_vram>:
    c040: 23fc 0000 0000 003c 0002  movel #0,3c0002
"""
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "non-word or odd-address LSPC",
        ):
            checker.validate_lspc_vram_stores(disassembly)

    def test_rejects_odd_address_lspc_write(self) -> None:
        disassembly = """\
0000c040 <write_vram>:
    c040: 13fc 0000 003c 0003  moveb #0,3c0003
"""
        with self.assertRaisesRegex(checker.ElfCheckError, "odd-address"):
            checker.validate_lspc_vram_stores(disassembly)

    def test_overwriting_tracked_register_avoids_false_positive(self) -> None:
        disassembly = """\
0000c040 <ordinary_store>:
    c040: 43f9 003c 0002  lea 3c0002 <REG_VRAMRW>,%a1
    c046: 2248            moveal %a0,%a1
    c048: 3280            movew %d0,%a1@
"""
        self.assertEqual(
            checker.validate_lspc_vram_stores(disassembly),
            (0, 0, 0),
        )

    def test_final_audit_requires_at_least_one_absolute_write(self) -> None:
        with self.assertRaisesRegex(
            checker.ElfCheckError,
            "found no absolute-long",
        ):
            checker.validate_lspc_vram_stores(
                "00000000 <empty>:\n    0: 4e75            rts\n",
                require_all_register_stores=True,
            )

    def test_final_audit_requires_each_lspc_register_class(self) -> None:
        stores = {
            "address": (
                "    1000: 33c0 003c 0000  movew %d0,3c0000\n"
            ),
            "data": (
                "    1006: 33c1 003c 0002  movew %d1,3c0002\n"
            ),
            "modifier": (
                "    100c: 33c2 003c 0004  movew %d2,3c0004\n"
            ),
        }
        for missing in stores:
            disassembly = "00001000 <write_vram>:\n" + "".join(
                instruction
                for name, instruction in stores.items()
                if name != missing
            )
            with self.subTest(missing=missing):
                with self.assertRaisesRegex(
                    checker.ElfCheckError,
                    rf"no absolute-long word writes for: {missing}",
                ):
                    checker.validate_lspc_vram_stores(
                        disassembly,
                        require_all_register_stores=True,
                    )


class PaletteReferenceTests(unittest.TestCase):
    GOOD_SEQUENCE = """\
    b0f6: 13fc 0001 003a 000f  moveb #1,3a000f
    b0fe: 33fc 8000 0040 0000  movew #-32768,400000
    b106: 13fc 0001 003a 001f  moveb #1,3a001f
    b10e: 33fc 8000 0040 0000  movew #-32768,400000
"""

    def test_accepts_reference_black_in_both_banks(self) -> None:
        checker.validate_palette_reference_init(self.GOOD_SEQUENCE)

    def test_rejects_missing_or_dynamic_reference_write(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "both banks"):
            checker.validate_palette_reference_init(
                self.GOOD_SEQUENCE.replace(
                    "b10e: 33fc 8000 0040 0000  movew #-32768,400000\n",
                    "",
                )
            )
        with self.assertRaisesRegex(checker.ElfCheckError, "both banks"):
            checker.validate_palette_reference_init(
                self.GOOD_SEQUENCE.replace("#-32768", "%d0", 1)
            )


class MemoryCardSafetyTests(unittest.TestCase):
    def test_accepts_cartridge_without_card_access(self) -> None:
        checker.validate_no_memory_card_access(
            {"main", "rom_backup_data_address", "rom_backup_data_size"},
            "    1000: 33fc 8000 0040 0000  movew #-32768,400000\n",
        )

    def test_rejects_api_bios_unlock_parameter_and_raw_writes(self) -> None:
        with self.assertRaisesRegex(checker.ElfCheckError, "API linked"):
            checker.validate_no_memory_card_access(
                {"main", "ng_memory_card_unlock"},
                "",
            )
        bad_disassemblies = (
            "    1000: 4eb9 00c0 0468  jsr c00468\n",
            "    1000: 13fc 0001 003a 0005  moveb #1,3a0005\n",
            "    1000: 13fc 0003 0010 fdc4  moveb #3,10fdc4\n",
            "    1000: 33fc 1234 0080 0000  movew #4660,800000\n",
            "    1000: 33fc 1234 00bf fffe  movew #4660,bffffe\n",
        )
        for disassembly in bad_disassemblies:
            with self.subTest(disassembly=disassembly):
                with self.assertRaisesRegex(
                    checker.ElfCheckError,
                    "memory-card",
                ):
                    checker.validate_no_memory_card_access(
                        {"main"},
                        disassembly,
                    )


if __name__ == "__main__":
    unittest.main()
