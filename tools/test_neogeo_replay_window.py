#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import neogeo_replay_window as replay_window  # noqa: E402


class ReplayWindowTests(unittest.TestCase):
    def test_default_schedule_has_exact_required_boundaries(self) -> None:
        schedule = replay_window.build_schedule(
            replay_window.parse_windows(replay_window.DEFAULT_WINDOW_SPEC),
            replay_window.DEFAULT_WARMUP_FRAMES,
        )

        self.assertEqual(
            schedule.checkpoints,
            (3434, 3554, 25690, 25810),
        )
        self.assertEqual(
            schedule.progress_intervals,
            (3435, 120, 22136, 120),
        )
        self.assertEqual(
            schedule.render_ranges,
            (
                replay_window.Window(3195, 3554),
                replay_window.Window(25451, 25810),
            ),
        )

    def test_parser_accepts_repeated_and_comma_separated_values(self) -> None:
        self.assertEqual(
            replay_window.parse_windows(("10:20,30:40", "50:50")),
            (
                replay_window.Window(10, 20),
                replay_window.Window(30, 40),
                replay_window.Window(50, 50),
            ),
        )

    def test_parser_rejects_bad_reversed_overlapping_and_unsorted_ranges(
        self,
    ) -> None:
        for specification in (
            "",
            "1-2",
            "0:2",
            "3:2",
            "10:20,20:30",
            "30:40,10:20",
        ):
            with self.subTest(specification=specification):
                with self.assertRaises(replay_window.WindowError):
                    replay_window.parse_windows(specification)

    def test_touching_warmup_ranges_are_merged(self) -> None:
        schedule = replay_window.build_schedule(
            replay_window.parse_windows("100:120,130:140"),
            warmup_frames=9,
        )
        self.assertEqual(
            schedule.render_ranges,
            (replay_window.Window(91, 140),),
        )

    def test_rendered_frame_count_includes_warmup_and_measurement(self) -> None:
        schedule = replay_window.build_schedule(
            replay_window.parse_windows(replay_window.DEFAULT_WINDOW_SPEC),
            replay_window.DEFAULT_WARMUP_FRAMES,
        )
        self.assertEqual(
            [
                replay_window.rendered_frames_through(
                    schedule.render_ranges,
                    checkpoint,
                )
                for checkpoint in schedule.checkpoints
            ],
            [240, 360, 600, 720],
        )

    def test_emitted_header_contains_only_schedule_metadata(self) -> None:
        schedule = replay_window.build_schedule(
            replay_window.parse_windows("5:7"),
            warmup_frames=2,
        )
        header = replay_window.emit_c_header(schedule)

        self.assertIn("UINT32_C(4)", header)
        self.assertIn("UINT32_C(3)", header)
        self.assertIn("UINT32_C(7)", header)
        self.assertIn("{ UINT32_C(3), UINT32_C(7) }", header)
        self.assertNotIn("smb_replay_states", header)


if __name__ == "__main__":
    unittest.main()
