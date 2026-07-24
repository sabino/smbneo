#!/usr/bin/env python3
"""Regression coverage for the interactive GnGeo launch recipe."""

from __future__ import annotations

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "platform" / "neogeo" / "Makefile"


class InteractiveLaunchRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.makefile = MAKEFILE.read_text()

    def test_realtime_flags_enable_pacing_at_stock_clocks(self) -> None:
        match = re.search(
            r"^GNGEO_REALTIME_FLAGS\s*\?=\s*\\\n"
            r"(?P<body>(?:\t.*(?:\\\n|\n))+)",
            self.makefile,
            flags=re.MULTILINE,
        )
        self.assertIsNotNone(match)
        flags = match.group("body").replace("\\\n", " ").split()

        self.assertIn("--autoframeskip", flags)
        self.assertNotIn("--no-autoframeskip", flags)
        self.assertIn("--no-vsync", flags)
        self.assertIn("--sleepidle", flags)
        self.assertIn("--68kclock=0", flags)
        self.assertIn("--z80clock=0", flags)

    def test_both_interactive_recipes_use_realtime_flags(self) -> None:
        for target in ("run", "replay-run"):
            match = re.search(
                rf"^{re.escape(target)}:.*?\n"
                rf"(?P<recipe>(?:\t.*\n)+)",
                self.makefile,
                flags=re.MULTILINE,
            )
            self.assertIsNotNone(match, target)
            self.assertIn(
                "$(GNGEO_REALTIME_FLAGS)",
                match.group("recipe"),
                target,
            )


if __name__ == "__main__":
    unittest.main()
