#!/usr/bin/env python3
"""Regression coverage for the interactive GnGeo launch recipe."""

from __future__ import annotations

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "platform" / "neogeo" / "Makefile"
ROOT_MAKEFILE = ROOT / "Makefile"


class InteractiveLaunchRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.makefile = MAKEFILE.read_text()
        cls.root_makefile = ROOT_MAKEFILE.read_text()

    def test_root_forwards_both_public_package_targets(self) -> None:
        forwarding = self.root_makefile.split(
            "# Command-line variables",
            1,
        )[1]
        self.assertIn("cart compat-cart hardware-cart", forwarding)
        self.assertIn("hardware-cart neosd-cart", forwarding)
        self.assertIn("mame-list mame-cart mame-run mame-capture", forwarding)

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

    def test_canonical_default_and_optional_compatibility_targets_are_separate(
        self,
    ) -> None:
        self.assertIn("GAMEROM := smbneo", self.makefile)
        self.assertIn("SMBNEO_NGH := 2026", self.makefile)
        self.assertIn(
            "--defsym=rom_NGH_ID=0x$(SMBNEO_NGH)",
            self.makefile,
        )
        self.assertIn(
            "COMPAT_CART := $(ROMDIR)/puzzledp.zip",
            self.makefile,
        )
        self.assertIn(
            "HARDWARE_CART := $(ROMDIR)/$(GAMEROM).zip",
            self.makefile,
        )
        self.assertIn(
            "NEOSD_CART := $(ROMDIR)/$(GAMEROM).neo",
            self.makefile,
        )
        self.assertIn("cart: hardware-cart neosd-cart", self.makefile)
        self.assertIn("compat-cart: native-roms", self.makefile)
        self.assertIn("hardware-cart: native-roms", self.makefile)
        self.assertIn("--output $(COMPAT_CART)", self.makefile)
        self.assertIn("-o $(HARDWARE_CART)", self.makefile)
        self.assertIn("neosd-cart: native-roms", self.makefile)
        self.assertNotIn("GNGEO_COMPAT_GAME", self.makefile)
        self.assertIn("run: hardware-cart", self.makefile)

        run_recipe = self.makefile.split("run: hardware-cart", 1)[1].split(
            "mame-list:",
            1,
        )[0]
        self.assertIn("-d $(GNGEO_DATA) $(GAMEROM)", run_recipe)
        self.assertNotIn("puzzledp", run_recipe)

        hardware_recipe = self.makefile.split(
            "hardware-cart: native-roms",
            1,
        )[1].split("run: hardware-cart", 1)[0]
        self.assertIn("-n $(GAMEROM)", hardware_recipe)
        self.assertIn('-l "Super Mario Bros. Neo"', hardware_recipe)
        self.assertIn("$(GNGEO_DRIVER_CHECKER)", hardware_recipe)

        neosd_recipe = self.makefile.split(
            "neosd-cart: native-roms",
            1,
        )[1].split("run: hardware-cart", 1)[0]
        self.assertIn("$(ROMTOOL) -b cartridge -f neo", neosd_recipe)
        self.assertIn("neo.genre=Platformer", neosd_recipe)
        self.assertIn("neo.ngh=$(SMBNEO_NGH)", neosd_recipe)
        self.assertIn("--validate $(NEOSD_CART)", neosd_recipe)

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

    def test_mame_capture_is_isolated_and_scripted(self) -> None:
        self.assertIn("mame-list: hardware-cart", self.makefile)
        self.assertIn("mame-cart: mame-list", self.makefile)
        self.assertIn("mame-run: mame-cart", self.makefile)
        self.assertIn("MAME_SYSTEM ?= ng_mv1", self.makefile)
        self.assertIn(
            "$(MAME) $(MAME_SYSTEM) $(GAMEROM)",
            self.makefile,
        )
        self.assertIn(
            "$(PYTHON) $(MAME_SOFTWARE_GENERATOR)",
            self.makefile,
        )

        recipe = self.makefile.split("mame-capture:", 1)[1].split(
            "$(WEB_PROM):",
            1,
        )[0]
        self.assertIn("mame-cart", recipe)
        for option in (
            '-hashpath "$(MAME_HASHDIR)"',
            '-rompath "$(MAME_ROMPATH)"',
            '-cfg_directory "$(MAME_CFG_DIR)"',
            '-nvram_directory "$(MAME_NVRAM_DIR)"',
            '-snapshot_directory "$(MAME_CAPTURE_DIR)"',
            '-autoboot_script "$(MAME_CAPTURE_SCRIPT)"',
        ):
            self.assertIn(option, recipe)
        self.assertIn("$(MAME_SYSTEM) $(GAMEROM)", recipe)
        self.assertNotIn("puzzledp", recipe)

    def test_custom_mame_bios_directory_precedes_local_cartridge(self) -> None:
        self.assertIn(
            "$(if $(strip $(MAME_BIOS_DIR)),$(MAME_BIOS_DIR);)$(ROMDIR)",
            self.makefile,
        )


if __name__ == "__main__":
    unittest.main()
