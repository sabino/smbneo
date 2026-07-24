#!/usr/bin/env python3
"""Generate a Neo Geo V1 ROM with a long, looping ADPCM-B triangle."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from typing import Sequence


VROM_SIZE = 512 * 1024
ADPCM_DATA_SIZE = 64 * 1024

ADPCM_B_INITIAL_SAMPLE = 0
ADPCM_B_INITIAL_STEP = 127
ADPCM_B_MINIMUM_SAMPLE = -32768
ADPCM_B_MAXIMUM_SAMPLE = 32767
ADPCM_B_MINIMUM_STEP = 127
ADPCM_B_MAXIMUM_STEP = 24576
ADPCM_B_STEP_FACTORS = (57, 57, 57, 57, 77, 102, 128, 153)

TRIANGLE_PEAK = 10000
TRIANGLE_PERIOD_SAMPLES = 64
TRIANGLE_QUARTER_SAMPLES = TRIANGLE_PERIOD_SAMPLES // 4
TRIANGLE_SLOPE = TRIANGLE_PEAK // TRIANGLE_QUARTER_SAMPLES
TRIANGLE_CYCLE_COUNT = (
    ADPCM_DATA_SIZE * 2 // TRIANGLE_PERIOD_SAMPLES
)


def pack_nibbles(nibbles: Sequence[int]) -> bytes:
    """Pack ADPCM-B codes in hardware playback order, high nibble first."""

    if len(nibbles) % 2 != 0:
        raise ValueError("ADPCM-B data must contain an even number of nibbles")

    output = bytearray(len(nibbles) // 2)
    for index in range(0, len(nibbles), 2):
        high = nibbles[index]
        low = nibbles[index + 1]
        if not 0 <= high <= 0xF or not 0 <= low <= 0xF:
            raise ValueError("ADPCM-B codes must be four-bit values")
        output[index // 2] = (high << 4) | low
    return bytes(output)


def build_triangle_cycle() -> tuple[int, ...]:
    """Return one 64-point triangle period beginning at a zero crossing."""

    samples = []
    for phase in range(TRIANGLE_PERIOD_SAMPLES):
        if phase < TRIANGLE_QUARTER_SAMPLES:
            sample = phase * TRIANGLE_SLOPE
        elif phase < TRIANGLE_QUARTER_SAMPLES * 2:
            sample = (
                TRIANGLE_QUARTER_SAMPLES * 2 - phase
            ) * TRIANGLE_SLOPE
        elif phase < TRIANGLE_QUARTER_SAMPLES * 3:
            sample = -(
                phase - TRIANGLE_QUARTER_SAMPLES * 2
            ) * TRIANGLE_SLOPE
        else:
            sample = -(
                TRIANGLE_PERIOD_SAMPLES - phase
            ) * TRIANGLE_SLOPE
        samples.append(sample)
    return tuple(samples)


def build_triangle_samples() -> tuple[int, ...]:
    """Return all target PCM points for the 64 KiB encoded loop."""

    return build_triangle_cycle() * TRIANGLE_CYCLE_COUNT


def advance_adpcm_b(
    code: int,
    sample: int,
    step: int,
) -> tuple[int, int]:
    """Advance the YM2610 ADPCM-B decoder state by one four-bit code."""

    magnitude = code & 7
    delta = ((magnitude * 2 + 1) * step) >> 3
    sample += -delta if code & 8 else delta
    sample = max(
        ADPCM_B_MINIMUM_SAMPLE,
        min(ADPCM_B_MAXIMUM_SAMPLE, sample),
    )

    step = (step * ADPCM_B_STEP_FACTORS[magnitude]) >> 6
    step = max(
        ADPCM_B_MINIMUM_STEP,
        min(ADPCM_B_MAXIMUM_STEP, step),
    )
    return sample, step


def encode_adpcm_b(samples: Sequence[int]) -> bytes:
    """Greedily quantize signed PCM points into YM2610 ADPCM-B codes."""

    predictor = ADPCM_B_INITIAL_SAMPLE
    step = ADPCM_B_INITIAL_STEP
    codes = []
    for target in samples:
        if not ADPCM_B_MINIMUM_SAMPLE <= target <= ADPCM_B_MAXIMUM_SAMPLE:
            raise ValueError(
                "ADPCM-B target sample exceeds the signed 16-bit range"
            )

        error = target - predictor
        sign = 8 if error < 0 else 0
        magnitude = min(7, (abs(error) * 4) // step)
        code = sign | magnitude
        codes.append(code)
        predictor, step = advance_adpcm_b(code, predictor, step)
    return pack_nibbles(codes)


def build_triangle_adpcm() -> bytes:
    """Return the continuous 64 KiB encoded triangle loop."""

    encoded = encode_adpcm_b(build_triangle_samples())
    if len(encoded) != ADPCM_DATA_SIZE:
        raise AssertionError("triangle ADPCM-B data has an invalid size")
    return encoded


def build_triangle_vrom() -> bytes:
    """Return the complete, deterministically padded 512 KiB V1 image."""

    encoded = build_triangle_adpcm()
    return encoded + bytes(VROM_SIZE - len(encoded))


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="exact V1 output path")
    args = parser.parse_args(argv)

    image = build_triangle_vrom()
    args.output.write_bytes(image)
    digest = hashlib.sha256(image).hexdigest()
    print(f"{args.output}: {len(image)} bytes sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
