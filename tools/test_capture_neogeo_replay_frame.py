#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_neogeo_replay_frame as capture  # noqa: E402


def png_bytes(width: int = 320, height: int = 224) -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return (
            struct.pack(">I", len(payload))
            + body
            + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
        )

    return (
        capture.PNG_SIGNATURE
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(b"\0" + b"\0\0\0" * width))
        + chunk(b"IEND", b"")
    )


class PngInspectionTests(unittest.TestCase):
    def test_valid_png_dimensions_and_size_are_returned(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.png"
            path.write_bytes(png_bytes())

            width, height, size = capture.inspect_png(path, 1_000_000)

        self.assertEqual((width, height), (320, 224))
        self.assertGreater(size, 24)

    def test_invalid_truncated_oversized_and_impossible_pngs_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.png"
            cases = (
                (b"", 1000, "too small"),
                (b"x" * 25, 1000, "signature"),
                (png_bytes(), 24, "maximum"),
                (png_bytes(0, 224), 1000, "dimensions"),
            )
            for payload, limit, message in cases:
                with self.subTest(message=message):
                    path.write_bytes(payload)
                    with self.assertRaisesRegex(
                        capture.CaptureError,
                        message,
                    ):
                        capture.inspect_png(path, limit)


class CaptureTests(unittest.TestCase):
    def test_sequence_is_strict_and_bounded(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output, counter, index = capture.next_sequence_path(directory)
            self.assertEqual(output.name, "stage-0001.png")
            self.assertEqual(index, 1)

            counter.write_text("65\n")
            with self.assertRaisesRegex(capture.CaptureError, "outside"):
                capture.next_sequence_path(directory)

            counter.write_text("broken\n")
            with self.assertRaisesRegex(capture.CaptureError, "integer"):
                capture.next_sequence_path(directory)

    @mock.patch.object(capture.subprocess, "run")
    def test_capture_validates_then_atomically_finalizes(
        self,
        run: mock.Mock,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "stage-0001.png"

            def fake_run(command: list[str], **_kwargs: object) -> object:
                Path(command[-1]).write_bytes(png_bytes())
                return subprocess.CompletedProcess(command, 0, "")

            run.side_effect = fake_run
            dimensions = capture.capture_frame(
                "scrot",
                output,
                3.0,
                1_000_000,
            )

            self.assertEqual(dimensions[:2], (320, 224))
            self.assertTrue(output.is_file())
            self.assertFalse(
                output.with_name("stage-0001.partial.png").exists()
            )

    @mock.patch.object(capture.subprocess, "run")
    def test_capture_rejects_failure_timeout_and_overwrite(
        self,
        run: mock.Mock,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "frame.png"
            run.return_value = subprocess.CompletedProcess([], 2, "bad")
            with self.assertRaisesRegex(capture.CaptureError, "status 2"):
                capture.capture_frame("scrot", output, 1, 1000)

            run.side_effect = subprocess.TimeoutExpired([], 1)
            with self.assertRaisesRegex(capture.CaptureError, "timed out"):
                capture.capture_frame("scrot", output, 1, 1000)

            run.side_effect = None
            output.write_bytes(png_bytes())
            with self.assertRaisesRegex(capture.CaptureError, "overwrite"):
                capture.capture_frame("scrot", output, 1, 1000)


if __name__ == "__main__":
    unittest.main()
