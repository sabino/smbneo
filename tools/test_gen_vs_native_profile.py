import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from gen_vs_native_profile import EXPECTED_DESCRIPTORS, extract_profile, emit


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.prg = bytearray(0x8000)
        self.chr = bytearray(0x2000)
        addresses = {
            "courses_world_offsets": 0x8000,
            "courses_area_offsets": 0x8010,
            "course_actor_pages": 0x8050,
            "course_actor_addr_lo": 0x8060,
            "course_scenery_pages": 0x80A0,
            "course_scenery_addr_lo": 0x80B0,
        }
        for index, value in enumerate((0, 5, 10, 14, 19, 23, 27, 32)):
            self.prg[addresses["courses_world_offsets"] - 0x8000 + index] = value
        for index in range(36):
            self.prg[addresses["courses_area_offsets"] - 0x8000 + index] = index & 0x1F
        for index in range(4):
            self.prg[addresses["course_actor_pages"] - 0x8000 + index] = 0
            self.prg[addresses["course_scenery_pages"] - 0x8000 + index] = 0
            actor = addresses["course_actor_addr_lo"] - 0x8000 + 2 * index
            scenery = addresses["course_scenery_addr_lo"] - 0x8000 + 2 * index
            self.prg[actor:actor + 2] = (0x10).to_bytes(2, "little")
            self.prg[scenery:scenery + 2] = (0x20).to_bytes(2, "little")
        self.chr[0x10] = 0xFF
        self.chr[0x20] = 0xF0
        self.lbl = "\n".join(
            f"al {value:06X} .{name}" for name, value in addresses.items()
        ) + "\n"
        self.prg_sha = hashlib.sha256(self.prg).hexdigest()
        self.lbl_sha = hashlib.sha256(self.lbl.encode()).hexdigest()

    def profile(self):
        return extract_profile(
            self.prg, self.chr, self.lbl, self.lbl,
            self.prg_sha, self.lbl_sha, self.lbl_sha,
        )

    def test_decodes_pointer_tables_and_terminators(self):
        profile = self.profile()
        self.assertEqual(profile["course_count"], 32)
        self.assertEqual(profile["courses"][31]["world"], 8)
        self.assertEqual(profile["courses"][0]["actor"]["terminator"], 0xFF)
        self.assertEqual(
            [row["descriptor"] for row in profile["courses"]],
            [0, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 16,
             17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 29, 30,
             31, 0, 1, 2, 3],
        )

    def test_rejects_missing_labels_table_overflow_and_bad_streams(self):
        with self.assertRaises(ValueError):
            extract_profile(
                self.prg,
                self.chr,
                self.lbl.replace("courses_area_offsets", "missing"),
                self.lbl,
                self.prg_sha,
                self.lbl_sha,
                self.lbl_sha,
            )

        overflow = bytearray(self.prg)
        overflow[0x8050 - 0x8000] = 0x30
        with self.assertRaises(ValueError):
            extract_profile(
                overflow,
                self.chr,
                self.lbl,
                self.lbl,
                hashlib.sha256(overflow).hexdigest(),
                self.lbl_sha,
                self.lbl_sha,
            )

        with self.assertRaises(ValueError):
            extract_profile(self.prg, bytes(0x2000), self.lbl, self.lbl,
                            self.prg_sha, self.lbl_sha, self.lbl_sha)
        with self.assertRaises(ValueError):
            extract_profile(self.prg, self.chr[:-1], self.lbl, self.lbl,
                            self.prg_sha, self.lbl_sha, self.lbl_sha)

    def test_emits_deterministic_metadata_only_files(self):
        profile = self.profile()
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            first_path, second_path = Path(first), Path(second)
            emit(profile, first_path)
            emit(profile, second_path)
            for name in ("vs_native_profile.json", "vs_native_profile.h",
                         "vs_native_profile.c"):
                self.assertEqual((first_path / name).read_bytes(),
                                 (second_path / name).read_bytes())
            self.assertNotIn(b"0x8000",
                             (first_path / "vs_native_profile.json").read_bytes())
            self.assertNotIn(b"bytes",
                             (first_path / "vs_native_profile.json").read_bytes())

    @unittest.skipUnless(shutil.which("cc"), "host C compiler unavailable")
    def test_emitted_c_compiles(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            emit(self.profile(), output)
            subprocess.run(
                ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
                 "-I", str(output), "-c", str(output / "vs_native_profile.c"),
                 "-o", str(output / "profile.o")],
                check=True,
            )

    @unittest.skipUnless(os.environ.get("SMB_VS_REAL_PROFILE"),
                         "opt-in owned reference assertion")
    def test_real_reference(self):
        root = Path(os.environ["SMB_VS_REAL_PROFILE"])
        profile = extract_profile(
            (root / "vsmain.vs.bin").read_bytes(),
            (root / "vschar1.vs.bin").read_bytes(),
            (root / "vs.dbg").read_text(encoding="utf-8"),
            (root / "vs.lbl").read_text(encoding="utf-8"),
        )
        self.assertEqual(profile["course_count"], 32)
        self.assertEqual(
            profile["chr_sha256"],
            "b26ea7836d3bb370a9d4551471c096671ac074c7329b29076cec5ab366303cc0",
        )
        self.assertEqual(
            tuple(row["descriptor"] for row in profile["courses"]),
            EXPECTED_DESCRIPTORS,
        )
        self.assertEqual(profile["courses"][0]["actor"]["offset"], 532)
        self.assertEqual(profile["courses"][-1]["scenery"]["length"], 1849)


if __name__ == "__main__":
    unittest.main()
