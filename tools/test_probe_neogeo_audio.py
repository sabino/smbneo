#!/usr/bin/env python3

from __future__ import annotations

import argparse
from array import array
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure_neogeo_cadence as cadence  # noqa: E402
import probe_neogeo_audio as audio  # noqa: E402


class AudioAnalysisTests(unittest.TestCase):
    def test_analyze_reads_signed_16_bit_stereo(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            samples = array("h", [0, 0, 4096, -4096, 1024, -1024])
            if sys.byteorder != "little":
                samples.byteswap()
            capture.write_bytes(samples.tobytes())

            result = audio.analyze_s16le_stereo(
                capture,
                sample_rate=3,
            )

        self.assertEqual(result.bytes, 12)
        self.assertEqual(result.samples, 6)
        self.assertEqual(result.frames, 3)
        self.assertEqual(result.nonzero_samples, 4)
        self.assertEqual(result.peak, 4096)
        self.assertEqual(result.estimated_seconds, 1.0)
        self.assertGreater(result.rms, 0.05)
        audio.assert_audible_signal(result)

    def test_empty_capture_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            capture.write_bytes(b"")

            with self.assertRaisesRegex(
                audio.AudioProbeError,
                "empty",
            ):
                audio.analyze_s16le_stereo(capture, sample_rate=22050)

    def test_partial_stereo_frame_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            capture.write_bytes(b"\0\0")

            with self.assertRaisesRegex(
                audio.AudioProbeError,
                "whole 4-byte",
            ):
                audio.analyze_s16le_stereo(capture, sample_rate=22050)

    def test_analysis_can_skip_preactivation_audio(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            samples = array("h", [3000, -3000, 0, 0, 4096, -4096])
            if sys.byteorder != "little":
                samples.byteswap()
            capture.write_bytes(samples.tobytes())

            result = audio.analyze_s16le_stereo(
                capture,
                sample_rate=1,
                start_offset=8,
            )

        self.assertEqual(result.bytes, 4)
        self.assertEqual(result.samples, 2)
        self.assertEqual(result.frames, 1)
        self.assertEqual(result.peak, 4096)

    def test_analysis_can_select_exact_postactivation_segment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            samples = array(
                "h",
                [3000, -3000, 4096, -4096, 0, 0],
            )
            if sys.byteorder != "little":
                samples.byteswap()
            capture.write_bytes(samples.tobytes())

            result = audio.analyze_s16le_stereo(
                capture,
                sample_rate=1,
                start_offset=4,
                byte_count=4,
            )

        self.assertEqual(result.bytes, 4)
        self.assertEqual(result.frames, 1)
        self.assertEqual(result.peak, 4096)

    def test_silence_fails_signal_assertion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.raw"
            capture.write_bytes(b"\0" * 16)
            result = audio.analyze_s16le_stereo(
                capture,
                sample_rate=4,
            )

        with self.assertRaisesRegex(audio.AudioProbeError, "peak"):
            audio.assert_audible_signal(result)


class CommandTests(unittest.TestCase):
    def test_command_enables_sound_without_debugger(self) -> None:
        command = audio.build_gngeo_command(
            "/usr/bin/ngdevkit-gngeo",
            Path("/work/rom"),
            Path("/work/rom/gngeo_data.zip"),
            "smbneo",
            sample_rate=22050,
            scale=2,
        )

        self.assertIn("--sound", command)
        self.assertIn("--samplerate=22050", command)
        self.assertIn("--autoframeskip", command)
        self.assertNotIn("--no-autoframeskip", command)
        self.assertIn("--no-vsync", command)
        self.assertIn("--sleepidle", command)
        self.assertIn("--68kclock=0", command)
        self.assertIn("--z80clock=0", command)
        self.assertIn("--no-debug", command)
        self.assertNotIn("-D", command)
        self.assertEqual(command[-1], "smbneo")
        self.assertIn(cadence.DEFAULT_P1_CONTROLS, command)

    def test_xdotool_timeout_is_reported(self) -> None:
        with mock.patch.object(
            audio.subprocess,
            "run",
            side_effect=subprocess.TimeoutExpired(
                ["xdotool", "search"],
                audio.COMMAND_TIMEOUT_SECONDS,
            ),
        ):
            with self.assertRaisesRegex(
                audio.AudioProbeError,
                "xdotool timed out",
            ):
                audio._run_xdotool(
                    "xdotool",
                    ":99",
                    ("search", "--name", "Gngeo"),
                )

    def test_screenshot_timeout_is_reported(self) -> None:
        with mock.patch.object(
            audio.subprocess,
            "run",
            side_effect=subprocess.TimeoutExpired(
                ["scrot"],
                audio.COMMAND_TIMEOUT_SECONDS,
            ),
        ):
            with self.assertRaisesRegex(
                audio.AudioProbeError,
                "screenshot timed out",
            ):
                audio._capture_screenshot(
                    "scrot",
                    Path("/tmp/frame.png"),
                    {},
                    audio.time.monotonic()
                    + audio.COMMAND_TIMEOUT_SECONDS,
                )


class ProbeLifecycleTests(unittest.TestCase):
    @staticmethod
    def _arguments(
        temporary: Path,
        **overrides: object,
    ) -> argparse.Namespace:
        rom_dir = temporary / "rom"
        rom_dir.mkdir(exist_ok=True)
        (rom_dir / "smbneo.zip").write_bytes(b"cart")
        data_file = rom_dir / "gngeo_data.zip"
        data_file.write_bytes(b"data")
        values: dict[str, object] = {
            "rom_dir": rom_dir,
            "data_file": data_file,
            "rom_set": "smbneo",
            "capture_seconds": 1.0,
            "startup_timeout": 1.0,
            "input_attempts": 1,
            "sample_rate": 22050,
            "scale": 1,
            "evidence_dir": temporary / "evidence",
            "keep_raw": False,
            "gngeo": "/bin/true",
            "xvfb": "/bin/true",
            "xdotool": "/bin/true",
            "scrot": "/bin/true",
        }
        values.update(overrides)
        return argparse.Namespace(**values)

    def test_nonfinite_and_unbounded_arguments_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_text:
            temporary = Path(temporary_text)
            for overrides, message in (
                ({"capture_seconds": float("inf")}, "finite"),
                ({"startup_timeout": float("nan")}, "finite"),
                (
                    {
                        "input_attempts":
                            audio.MAXIMUM_INPUT_ATTEMPTS + 1
                    },
                    "input attempt",
                ),
                (
                    {"sample_rate": audio.MAXIMUM_SAMPLE_RATE + 1},
                    "sample rate",
                ),
            ):
                with self.subTest(overrides=overrides):
                    with self.assertRaisesRegex(
                        audio.AudioProbeError,
                        message,
                    ):
                        audio._validate_probe_arguments(
                            self._arguments(temporary, **overrides)
                        )

    def test_owned_processes_are_cleaned_up_after_startup_failure(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_text:
            temporary = Path(temporary_text)
            owned = mock.Mock()
            with (
                mock.patch.object(
                    audio.cadence,
                    "OwnedProcessGroups",
                    return_value=owned,
                ),
                mock.patch.object(
                    audio.cadence,
                    "_start_xvfb",
                    side_effect=audio.AudioProbeError("synthetic startup"),
                ),
            ):
                with self.assertRaisesRegex(
                    audio.AudioProbeError,
                    "synthetic startup",
                ):
                    audio.run_probe(self._arguments(temporary))

            owned.terminate_all.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
