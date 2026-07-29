#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import neogeo_replay_window as replay_window  # noqa: E402
import run_neogeo_window_benchmark as runner  # noqa: E402


def default_schedule() -> replay_window.ReplayWindowSchedule:
    return replay_window.build_schedule(
        replay_window.parse_windows(replay_window.DEFAULT_WINDOW_SPEC),
        replay_window.DEFAULT_WARMUP_FRAMES,
    )


def status_words(
    frame: int,
    game_frames: int,
    *,
    logic: bool = False,
    hold_count: int = 1,
) -> list[int]:
    core_frames = frame + 1 - 7 - hold_count
    generation = 0 if logic else game_frames & 0xFFFF
    return [
        runner.STATUS_MAGIC,
        runner.STATUS_VERSION,
        0,
        frame,
        0,
        123,
        0,
        1,
        0,
        0,
        0,
        1,
        3,
        0,
        0,
        0,
        67677,
        0,
        0,
        0,
        7,
        1,
        hold_count,
        core_frames,
        0 if logic else 1,
        game_frames,
        game_frames + 20,
        2,
        generation,
        generation,
    ]


def evidence_log(*, logic: bool = False) -> str:
    schedule = default_schedule()
    lines: list[str] = []
    for sample, frame in enumerate(schedule.checkpoints, start=1):
        game_frames = replay_window.rendered_frames_through(
            schedule.render_ranges,
            frame,
        )
        for index, value in enumerate(
            status_words(frame, game_frames, logic=logic)
        ):
            lines.append(
                "REPLAY_PROGRESS_WORD "
                f"sample={sample} index={index} value=0x{value:08x}"
            )
        lines.append(
            "WINDOW_RAM "
            f"sample={sample} frame_counter={frame & 0xff} "
            "active_enemies=3 flags=010001010000 ids=010203040506 "
            "screen=12:34 player=11:200"
        )
        lines.append(
            f"WINDOW_CPU sample={sample} a=1 x=2 y=3 sp=253 carry=1 nz=128"
        )
        ram_image = bytearray(0x800)
        ram_image[0x09] = frame & 0xff
        ram_image[0x0F:0x15] = bytes.fromhex("010001010000")
        ram_image[0x16:0x1C] = bytes.fromhex("010203040506")
        ram_image[0x71A] = 12
        ram_image[0x71C] = 34
        ram_image[0x06D] = 11
        ram_image[0x086] = 200
        for index in range(0x800 // 4):
            value = int.from_bytes(ram_image[index * 4:index * 4 + 4], "big")
            lines.append(
                "WINDOW_RAM_WORD "
                f"sample={sample} index={index} value=0x{value:08x}"
            )
        lines.append(f"REPLAY_PROGRESS_END sample={sample}")
    return "\n".join(lines)


class WindowBenchmarkParserTests(unittest.TestCase):
    def test_full_render_parser_and_window_deltas(self) -> None:
        schedule = default_schedule()
        parsed = runner.parse_evidence(evidence_log(), schedule)
        results = runner.compute_window_results(parsed, schedule)

        self.assertEqual(parsed.status_mode, "full")
        self.assertEqual(
            [result["game_frames"] for result in results],
            [120, 120],
        )
        self.assertEqual(
            [status["game_frame_count"] for status in parsed.statuses],
            [240, 360, 600, 720],
        )
        self.assertEqual(parsed.cpu_snapshots[0]["sp"], 253)
        self.assertEqual(len(parsed.ram_images[0]), 2048)

    def test_logic_bench_accepts_paced_frames_but_zero_generations(self) -> None:
        schedule = default_schedule()
        parsed = runner.parse_evidence(
            evidence_log(logic=True),
            schedule,
            requested_mode="logic",
        )

        self.assertEqual(parsed.status_mode, "logic")
        self.assertEqual(
            runner.compute_window_results(parsed, schedule)[1]["game_frames"],
            120,
        )

    def test_parser_rejects_incomplete_duplicate_and_wrong_frame_samples(
        self,
    ) -> None:
        schedule = default_schedule()
        log = evidence_log()
        with self.assertRaisesRegex(runner.BenchmarkError, "completed samples"):
            runner.parse_evidence(
                "\n".join(log.splitlines()[:-1]),
                schedule,
            )
        with self.assertRaisesRegex(runner.BenchmarkError, "repeats mailbox"):
            runner.parse_evidence(
                log + "\n" + log.splitlines()[0],
                schedule,
            )
        with self.assertRaisesRegex(runner.BenchmarkError, "expected 3434"):
            runner.parse_evidence(
                log.replace(
                    "index=3 value=0x00000d6a",
                    "index=3 value=0x00000d6b",
                    1,
                ),
                schedule,
            )

    def test_parser_rejects_full_render_generation_mismatch(self) -> None:
        schedule = default_schedule()
        log = evidence_log().replace(
            "index=28 value=0x000000f0",
            "index=28 value=0x000000ef",
            1,
        )
        with self.assertRaisesRegex(runner.BenchmarkError, "render generation"):
            runner.parse_evidence(log, schedule)

    def test_gngeo_defaults_to_current_mvs_identity(self) -> None:
        command = runner.build_gngeo_command(
            "/usr/bin/ngdevkit-gngeo",
            Path("/work/rom"),
            Path("/work/rom/gngeo_data.zip"),
            runner.DEFAULT_ROM_SET,
        )

        self.assertEqual(command[-1], "smbneo")
        self.assertEqual(command[command.index("--system") + 1], "arcade")
        self.assertIn("--no-sound", command)
        self.assertIn("--z80clock=0", command)


if __name__ == "__main__":
    unittest.main()
