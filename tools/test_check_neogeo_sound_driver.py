#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_neogeo_sound_driver as sound_map  # noqa: E402


def map_text(
    code_address: int = 0,
    code_size: int = 0x2A19,
    data_address: int = 0xF800,
    data_size: int = 0x798,
) -> str:
    return f"""\
Area                                    Addr        Size
CODE                                {code_address:08X}    {code_size:08X} =
DATA                                {data_address:08X}    {data_size:08X} =
"""


class SoundDriverMapTests(unittest.TestCase):
    def test_current_layout_has_bounded_stack_headroom(self) -> None:
        areas = sound_map.parse_areas(map_text())

        self.assertEqual(
            sound_map.validate_areas(areas),
            (0x2A19, 0x798, 0x65),
        )

    def test_repeated_identical_area_summaries_are_accepted(self) -> None:
        text = map_text() + map_text()

        self.assertEqual(sound_map.parse_areas(text)["CODE"], (0, 0x2A19))

    def test_inconsistent_or_missing_areas_are_rejected(self) -> None:
        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "inconsistent CODE",
        ):
            sound_map.parse_areas(map_text() + map_text(code_size=1))

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "inconsistent DATA",
        ):
            sound_map.parse_areas(
                map_text() + map_text(data_size=1)
            )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "missing the DATA",
        ):
            sound_map.parse_areas(
                "CODE                                00000000    00000001 =\n"
            )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "missing the CODE",
        ):
            sound_map.parse_areas(
                "DATA                                0000F800    00000001 =\n"
            )

    def test_rom_ram_and_stack_guards_are_enforced(self) -> None:
        bad_maps = (
            (map_text(code_address=0x100), "expected \\$0000"),
            (map_text(code_size=0), "exceeds"),
            (map_text(code_size=0x8001), "exceeds"),
            (map_text(data_address=0xF700), "expected \\$f800"),
            (map_text(data_size=0x800), "beyond stack"),
            (map_text(data_size=0x7C0), "only 61 stack bytes"),
        )
        for text, message in bad_maps:
            with self.subTest(message=message):
                with self.assertRaisesRegex(sound_map.SoundMapError, message):
                    sound_map.validate_areas(sound_map.parse_areas(text))


if __name__ == "__main__":
    unittest.main()
