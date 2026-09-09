import unittest

from run_vs_mame import symbol_environment


class SymbolEnvironmentTests(unittest.TestCase):
    def test_accepts_native_platform_anchor(self):
        result = symbol_environment(
            "00100000 B platform\n00104600 B vs_debug_frames\n"
        )
        self.assertEqual(result["VS_MACHINE_ADDRESS"], "0x00100000")
        self.assertEqual(result["VS_DEBUG_FRAMES"], "0x00104600")

    def test_accepts_legacy_machine_anchor(self):
        result = symbol_environment("00100000 B machine\n")
        self.assertEqual(result["VS_MACHINE_ADDRESS"], "0x00100000")

    def test_rejects_missing_or_ambiguous_anchor(self):
        with self.assertRaisesRegex(ValueError, "found 0"):
            symbol_environment("00100000 B something_else\n")
        with self.assertRaisesRegex(ValueError, "found 2"):
            symbol_environment("00100000 B machine\n00102000 B platform\n")


if __name__ == "__main__":
    unittest.main()
