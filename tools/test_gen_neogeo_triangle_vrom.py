#!/usr/bin/env python3

from __future__ import annotations

from contextlib import redirect_stdout
import hashlib
from io import StringIO
import math
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_neogeo_triangle_vrom as triangle_vrom  # noqa: E402


EXPECTED_VROM_SHA256 = (
    "c52017058a226a44506a5d94fc1f692b42fe818302761da64d4f7adc5e5928a7"
)
ADPCM_B_STEP_SCALES = (57, 57, 57, 57, 77, 102, 128, 153)
ADPCM_B_MINIMUM_STEP = 127
ADPCM_B_MAXIMUM_STEP = 24576


def unpack_nibbles(data: bytes) -> list[int]:
    return [
        nibble
        for value in data
        for nibble in (value >> 4, value & 0x0F)
    ]


def decode_adpcm_b(
    nibbles: list[int],
) -> tuple[list[int], list[int]]:
    """Decode codes with the YM2610 ADPCM-B accumulator/step equations."""

    accumulator = 0
    step = ADPCM_B_MINIMUM_STEP
    samples = []
    steps = []
    for code in nibbles:
        magnitude = code & 7
        delta = ((magnitude * 2 + 1) * step) >> 3
        accumulator += -delta if code & 8 else delta
        accumulator = max(-32768, min(32767, accumulator))

        step = (step * ADPCM_B_STEP_SCALES[magnitude]) >> 6
        step = max(
            ADPCM_B_MINIMUM_STEP,
            min(ADPCM_B_MAXIMUM_STEP, step),
        )
        samples.append(accumulator)
        steps.append(step)
    return samples, steps


class TriangleVromTests(unittest.TestCase):
    def test_target_is_centered_ideal_triangle_at_the_loop_phase(self) -> None:
        cycle = triangle_vrom.build_triangle_cycle()

        self.assertEqual(len(cycle), 64)
        self.assertEqual(cycle[0], 0)
        self.assertEqual(cycle[16], 10000)
        self.assertEqual(cycle[32], 0)
        self.assertEqual(cycle[48], -10000)
        self.assertEqual(cycle[-1], -625)
        self.assertEqual(sum(cycle), 0)
        self.assertEqual(
            triangle_vrom.build_triangle_samples(),
            cycle * 2048,
        )

    def test_encoded_loop_has_bounded_level_error_and_clean_seam(self) -> None:
        targets = triangle_vrom.build_triangle_samples()
        encoded = triangle_vrom.build_triangle_adpcm()
        decoded, steps = decode_adpcm_b(unpack_nibbles(encoded))

        self.assertEqual(len(encoded), 64 * 1024)
        self.assertEqual(len(decoded), 131072)
        self.assertEqual((min(decoded), max(decoded)), (-10078, 10081))
        squared_error = sum(
            (actual - target) ** 2
            for actual, target in zip(decoded, targets)
        )
        rms_error = math.sqrt(squared_error / len(targets))
        self.assertAlmostEqual(rms_error, 47.53386444786648)
        self.assertLess(rms_error, 50)

        # Repeat resets the decoder, so decoded[0] is also the first output
        # after decoded[-1] when the YM2610 loops back to address zero.
        ideal_seam_delta = targets[0] - targets[-1]
        decoded_seam_delta = decoded[0] - decoded[-1]
        self.assertEqual(decoded[0], 15)
        self.assertEqual(decoded[-1], -609)
        self.assertEqual(targets[-1], -625)
        self.assertEqual(
            abs(decoded_seam_delta - ideal_seam_delta),
            1,
        )
        self.assertLessEqual(
            abs(decoded[0] - targets[0]),
            16,
        )
        self.assertLessEqual(
            abs(decoded[-1] - targets[-1]),
            16,
        )
        self.assertEqual((min(steps), max(steps)), (127, 1153))
        self.assertTrue(
            all(
                ADPCM_B_MINIMUM_STEP <= step <= ADPCM_B_MAXIMUM_STEP
                for step in steps
            )
        )

    def test_complete_vrom_layout_and_sha256_are_fixed(self) -> None:
        image = triangle_vrom.build_triangle_vrom()
        encoded = triangle_vrom.build_triangle_adpcm()

        self.assertEqual(len(image), 512 * 1024)
        self.assertEqual(image[: 64 * 1024], encoded)
        self.assertEqual(
            image[64 * 1024:],
            bytes(448 * 1024),
        )
        self.assertEqual(
            hashlib.sha256(image).hexdigest(),
            EXPECTED_VROM_SHA256,
        )

    def test_cli_writes_only_the_exact_output_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory)
            output = output_dir / "custom-triangle-name.v1"
            stdout = StringIO()

            with redirect_stdout(stdout):
                result = triangle_vrom.main([str(output)])

            self.assertEqual(result, 0)
            self.assertEqual(output.read_bytes(), triangle_vrom.build_triangle_vrom())
            self.assertEqual(
                [path.name for path in output_dir.iterdir()],
                [output.name],
            )
            self.assertIn(EXPECTED_VROM_SHA256, stdout.getvalue())


if __name__ == "__main__":
    unittest.main()
