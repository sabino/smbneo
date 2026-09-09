import unittest

from run_vs_mame import gameplay_metrics


def log(*markers):
    return "\n".join(f"VS frame={display} stage=4 frames={tick:x}" for tick, display in markers)


class GameplayMetricsTests(unittest.TestCase):
    def test_reports_six_hundred_tick_interval(self):
        metrics = gameplay_metrics(log((180, 10), (300, 20), (600, 100),
                                       (900, 700), (1200, 1300)), "native")
        self.assertEqual(metrics["label"], "native")
        self.assertEqual(metrics["ticks"], 600)
        self.assertEqual(metrics["display_frames"], 1200)
        self.assertEqual(metrics["display_frames_per_tick"], 2.0)

    def test_rejects_missing_marker(self):
        with self.assertRaisesRegex(ValueError, "missing.*1200"):
            gameplay_metrics(log((600, 100)))

    def test_rejects_duplicate_marker(self):
        with self.assertRaisesRegex(ValueError, "duplicated"):
            gameplay_metrics(log((600, 100), (600, 101), (1200, 1300)))

    def test_rejects_out_of_order_marker(self):
        with self.assertRaisesRegex(ValueError, "out of order"):
            gameplay_metrics(log((600, 100), (1200, 1300), (900, 700)))

    def test_rejects_backwards_display_counter_and_tick_range(self):
        with self.assertRaisesRegex(ValueError, "moved backwards"):
            gameplay_metrics(log((600, 1300), (1200, 100)))
        with self.assertRaisesRegex(ValueError, "must be after"):
            gameplay_metrics(log((600, 100)), start_tick=600, end_tick=600)

    def test_rejects_no_markers(self):
        with self.assertRaisesRegex(ValueError, "no VS frame markers"):
            gameplay_metrics("VS validation passed: coin/start/gameplay")


if __name__ == "__main__":
    unittest.main()
