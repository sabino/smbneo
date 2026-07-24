#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_neogeo_palette as palette  # noqa: E402


class NeoGeoPaletteTests(unittest.TestCase):
    def test_generated_includes_are_current(self) -> None:
        palette.check_generated()

    def test_palette_has_sixty_four_bounded_colors(self) -> None:
        rgb = palette.rgb8_palette()
        words = palette.neogeo_palette()
        self.assertEqual(len(rgb), 64)
        self.assertEqual(len(words), 64)
        for source, word in zip(rgb, words):
            decoded = palette.decode_neogeo_color(word)
            self.assertLessEqual(
                max(abs(a - b) for a, b in zip(source, decoded)),
                4,
            )

    def test_mario_reference_colors(self) -> None:
        words = palette.neogeo_palette()
        self.assertEqual(words[0x16], 0xED20)
        self.assertEqual(words[0x27], 0xFF93)
        self.assertEqual(words[0x18], 0xC870)
        self.assertEqual(
            palette.decode_neogeo_color(words[0x16]),
            (216, 40, 0),
        )
        self.assertEqual(
            palette.decode_neogeo_color(words[0x27]),
            (248, 152, 56),
        )
        self.assertEqual(
            palette.decode_neogeo_color(words[0x18]),
            (136, 112, 0),
        )

    def test_black_and_white_anchors(self) -> None:
        words = palette.neogeo_palette()
        self.assertEqual(words[0x0D], 0x8000)
        self.assertEqual(words[0x20], 0x7FFF)
        self.assertEqual(palette.decode_neogeo_color(words[0x0D]), (0, 0, 0))
        self.assertEqual(
            palette.decode_neogeo_color(words[0x20]),
            (252, 252, 252),
        )


if __name__ == "__main__":
    unittest.main()
