"""Protect reviewed hardware fixes while extending the shared VS renderer.

These guards intentionally require review when safety-critical bus routines
change. They are not a substitute for hardware testing or ELF inspection.
"""
import hashlib
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
# Normalized function bodies from 6a76271 (includes earlier hardware fixes).
REVIEWED = {
    'write_vram_address': 'ad5cb546d3efc1a9df7349edc83543a269ddf46c0c3e31c1d4233fdc8215caab',
    'write_vram_data': '78474eff0a5eb2e1a213fd59d6f45f1d7aca235a2cba12ad56e24c84a1829d42',
    'write_vram_mod': '7495bc209d50b2b7342b5f5f8fc59370db61d4d9f6223e39ccd9afac0d550ece',
    'hide_sprite_set': 'f4735b638b4a99579c496128a9a0214e2ae34fddd0e68030a2dd6d3d9ba658ec',
    'show_next_sprite_set': 'e8a7401656d6500785f251ac3d929c5b9b76ff11915ef27357b2cbf0f0c3fcb9',
    'clear_hardware_sprites': 'b675eae9155ab2459cd641c0b60e549860480c6d5d8c0afc3f3916ca89d0ca8e',
    'initialize_fix_map': '7e9b6c179886a8196968214a8cd914b86974d0c04a1768eac05cfb4f98448c4b',
    'neogeo_video_init': '78f7b5e0f60c47ad164690a6f7597c960051b5873c7b3564fcbea4800fd99330',
}


class HardwareContractTests(unittest.TestCase):
    def test_reviewed_lspc_routines_unchanged(self):
        source = (ROOT / 'platform/neogeo/video.c').read_text()
        for name, digest in REVIEWED.items():
            with self.subTest(function=name):
                match = re.search(r'\b' + name + r'\([^)]*\)\s*\{', source)
                self.assertIsNotNone(match)
                start = match.end() - 1
                depth = 0
                for end in range(start, len(source)):
                    depth += (source[end] == '{') - (source[end] == '}')
                    if depth == 0: break
                body = re.sub(r'/\*.*?\*/|//[^\n]*', '', source[start:end + 1], flags=re.S)
                body = re.sub(r'\s+', '', body)
                self.assertEqual(hashlib.sha256(body.encode()).hexdigest(), digest,
                                 'Review the real-hardware fixes before updating this guard')

    def test_vs_target_keeps_elf_safety_gate(self):
        make = (ROOT / 'variants/vs/neogeo.mk').read_text()
        self.assertIn('check_neogeo_elf.py $(BUILD)/vssmbneo.elf --variant vs', make)
        self.assertIn('rom_NGH_ID=0x2027', make)
        main = (ROOT / 'variants/vs/neogeo_main.c').read_text()
        self.assertIn('bss_restore_guard[32]', main)
        self.assertIn('(void)bss_restore_guard[0]', main)

    def test_vs_credit_owner_and_hardware_coin_gate(self):
        main = (ROOT / 'variants/vs/neogeo_main.c').read_text()
        self.assertIn('*(volatile uint8_t *)BIOS_USER_MODE = 2;', main)
        self.assertIn('if (*(volatile uint8_t *)BIOS_MVS_FLAG & 0x80u)', main)
        self.assertLess(main.index('BIOS_USER_MODE = 2'), main.index('vs_machine_init(&machine'))


if __name__ == '__main__': unittest.main()
