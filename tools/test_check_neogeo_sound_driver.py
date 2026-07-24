#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_neogeo_sound_driver as sound_map  # noqa: E402


DRIVER_SOURCE = (
    Path(__file__).resolve().parent.parent
    / "platform"
    / "neogeo"
    / "sound_driver.s"
)


def map_text(
    code_address: int = 0,
    code_size: int = 0x2AD5,
    data_address: int = 0xF800,
    data_size: int = 0x799,
) -> str:
    return f"""\
Area                                    Addr        Size
CODE                                {code_address:08X}    {code_size:08X} =
DATA                                {data_address:08X}    {data_size:08X} =
"""


class SoundDriverSourceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.source = DRIVER_SOURCE.read_text(errors="strict")

    def test_current_command_table_and_data_layout_are_exact(self) -> None:
        commands = sound_map.parse_explicit_commands(self.source)

        self.assertEqual(len(commands), 0x80)
        self.assertEqual(
            commands[0x00:0x06],
            (
                "snd_command_unused",
                "snd_command_01_prepare_for_rom_switch",
                "snd_command_unused",
                "snd_command_03_reset_driver",
                "apu_ready_ping",
                "apu_ready_ping",
            ),
        )
        self.assertEqual(
            commands[0x06:0x80],
            ("apu_packet_byte",) * 122,
        )
        self.assertEqual(
            sound_map.validate_driver_source(self.source),
            (128, 3),
        )

    def test_wrong_packet_dispatch_is_rejected(self) -> None:
        broken = self.source.replace(
            "jp      apu_packet_byte",
            "jp      snd_command_unused",
            1,
        )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            r"command \$06 .* expected apu_packet_byte",
        ):
            sound_map.validate_driver_source(broken)

    def test_ready_ping_resets_packet_state(self) -> None:
        broken = self.source.replace(
            "ld      (apu_previous_symbol), a",
            "ld      (apu_packet_quotient), a",
            1,
        )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "ready-ping reset",
        ):
            sound_map.validate_driver_source(broken)

    def test_packet_decoder_semantics_are_exact(self) -> None:
        broken = self.source.replace(
            "cp      #60",
            "cp      #61",
            1,
        )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "packet decoder",
        ):
            sound_map.validate_driver_source(broken)

    def test_packet_base_table_is_exact(self) -> None:
        broken = self.source.replace(
            "0x0079",
            "0x007a",
            1,
        )

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "packet base table",
        ):
            sound_map.validate_driver_source(broken)

    def test_extra_mutable_data_is_rejected(self) -> None:
        broken = self.source + "\napu_extra_state:\n        .blkb   1\n"

        with self.assertRaisesRegex(
            sound_map.SoundMapError,
            "sound-driver DATA",
        ):
            sound_map.validate_driver_source(broken)


class SoundDriverMapTests(unittest.TestCase):
    def test_current_layout_has_bounded_stack_headroom(self) -> None:
        areas = sound_map.parse_areas(map_text())

        self.assertEqual(
            sound_map.validate_areas(areas),
            (0x2AD5, 0x799, 0x64),
        )

    def test_repeated_identical_area_summaries_are_accepted(self) -> None:
        text = map_text() + map_text()

        self.assertEqual(sound_map.parse_areas(text)["CODE"], (0, 0x2AD5))

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
