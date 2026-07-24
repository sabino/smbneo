#!/usr/bin/env python3

from __future__ import annotations

import argparse
from dataclasses import asdict, replace
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_neogeo_replay_gate as runner  # noqa: E402


NM_OUTPUT = """\
00003bce 00000004 T neogeo_replay_pass_trap
00003bd2 00000004 T neogeo_replay_fail_trap
00003bd6 00000004 T neogeo_replay_progress_trap
00003bda 00000004 T neogeo_replay_stage_trap
00003bde 00000004 T neogeo_replay_transition_trap
00100130 00000070 B neogeo_replay_status
"""


def sample_symbols() -> runner.ReplaySymbols:
    return runner.ReplaySymbols(
        pass_trap=runner.ElfSymbol(
            "neogeo_replay_pass_trap",
            0x3BCE,
            4,
            "T",
        ),
        fail_trap=runner.ElfSymbol(
            "neogeo_replay_fail_trap",
            0x3BD2,
            4,
            "T",
        ),
        status=runner.ElfSymbol(
            "neogeo_replay_status",
            0x100130,
            112,
            "B",
        ),
        progress_point=runner.ElfSymbol(
            "neogeo_replay_progress_trap",
            0x3BD6,
            4,
            "T",
        ),
        stage_point=runner.ElfSymbol(
            "neogeo_replay_stage_trap",
            0x3BDA,
            4,
            "T",
        ),
        transition_point=runner.ElfSymbol(
            "neogeo_replay_transition_trap",
            0x3BDE,
            4,
            "T",
        ),
    )


def sample_words(
    *,
    result: int = 1,
    entered_mask: int = 0xFFFFFFFF,
    completed_mask: int = 0xFFFFFFFF,
) -> list[int]:
    return [
        runner.STATUS_MAGIC,
        runner.STATUS_VERSION,
        result,
        67116,
        0,
        9118,
        0,
        entered_mask,
        completed_mask,
        31,
        60,
        2,
        4,
        7,
        3,
        0,
        67117,
        0,
        419,
        0,
        7,
        1,
        6,
        67104,
        1,
        67104,
        67120,
        2,
    ]


def stage_words(stage: int, *, frame: int) -> list[int]:
    entered_mask = (
        runner.ALL_STAGES_MASK
        if stage == runner.FINAL_STAGE
        else (1 << (stage + 1)) - 1
    )
    completed_mask = 0 if stage == 0 else (1 << stage) - 1
    words = sample_words(
        result=0,
        entered_mask=entered_mask,
        completed_mask=completed_mask,
    )
    words[3] = frame
    words[4] = 0
    words[9] = stage
    words[10] = 0
    words[11] = 1
    words[12] = 3
    words[13] = stage // 4
    words[14] = stage % 4
    words[15] = 0
    words[17] = 1
    words[18] = 0
    words[23] = frame + 1 - words[20] - words[22]
    words[25] = words[23]
    words[26] = words[25] + 2
    return words


def capture(
    *,
    trap_kind: str = "pass",
    result: int = 1,
    entered_mask: int = 0xFFFFFFFF,
    completed_mask: int = 0xFFFFFFFF,
) -> runner.DebuggerCapture:
    symbols = sample_symbols()
    words = sample_words(
        result=result,
        entered_mask=entered_mask,
        completed_mask=completed_mask,
    )
    return runner.DebuggerCapture(
        trap_kind=trap_kind,
        trap_pc=(
            symbols.pass_trap.address
            if trap_kind == "pass"
            else symbols.fail_trap.address
        ),
        status=runner.parse_mailbox_words(words),
        raw_words=tuple(words),
    )


def rendered_capture() -> runner.DebuggerCapture:
    symbols = sample_symbols()
    words = sample_words()
    words[17] = 1
    words[18] = 0
    return runner.DebuggerCapture(
        trap_kind="pass",
        trap_pc=symbols.pass_trap.address,
        status=runner.parse_mailbox_words(words),
        raw_words=tuple(words),
    )


def stage_captures(count: int = 32) -> list[runner.StageCapture]:
    captures: list[runner.StageCapture] = []
    for stage in range(count):
        words = stage_words(stage, frame=1000 + stage * 1000)
        captures.append(
            runner.StageCapture(
                sample=stage + 1,
                status=runner.parse_mailbox_words(words),
                raw_words=tuple(words),
            )
        )
    return captures


def transition_captures(
    count: int = 32,
) -> list[runner.TransitionCapture]:
    captures: list[runner.TransitionCapture] = []
    for stage in range(count):
        words = stage_words(stage, frame=998 + stage * 1000)
        captures.append(
            runner.TransitionCapture(
                sample=stage + 1,
                status=runner.parse_mailbox_words(words),
                raw_words=tuple(words),
            )
        )
    return captures


def write_visual_png(path: Path, variant: int) -> None:
    from PIL import Image, ImageDraw

    image = Image.new("RGB", (1024, 768), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    left, top = (1024 - 320) // 2, (768 - 224) // 2
    base = (
        20 + (variant * 29) % 180,
        40 + (variant * 17) % 180,
        60 + (variant * 11) % 180,
    )
    draw.rectangle((left, top, left + 319, top + 223), fill=base)
    draw.rectangle(
        (left + 8, top + 8, left + 120, top + 24),
        fill=(255, 255, 255),
    )
    draw.rectangle(
        (left + 32 + variant, top + 100, left + 80 + variant, top + 180),
        fill=(255, 80, 0),
    )
    draw.rectangle(
        (left + 160, top + 140, left + 300, top + 220),
        fill=(0, 180, 40),
    )
    image.save(path)


def write_transition_png(path: Path) -> None:
    from PIL import Image, ImageDraw

    image = Image.new("RGB", (1024, 768), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    left, top = (1024 - 320) // 2, (768 - 224) // 2
    draw.rectangle(
        (left, top, left + 319, top + 223),
        fill=(100, 160, 230),
    )
    image.save(path)


class SymbolResolutionTests(unittest.TestCase):
    def test_nm_parser_reads_and_validates_exact_gate_objects(self) -> None:
        symbols = runner.replay_symbols_from_nm(NM_OUTPUT)

        self.assertEqual(symbols.pass_trap.address, 0x3BCE)
        self.assertEqual(symbols.fail_trap.size, 4)
        self.assertEqual(symbols.status.address, 0x100130)
        self.assertEqual(symbols.status.size, runner.STATUS_BYTES)

    def test_nm_parser_rejects_wrong_mailbox_size_and_alignment(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "exactly 112"):
            runner.replay_symbols_from_nm(
                NM_OUTPUT.replace("00000070 B", "0000006c B")
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "aligned"):
            runner.replay_symbols_from_nm(
                NM_OUTPUT.replace("00100130", "00100132")
            )

    def test_nm_parser_rejects_missing_or_duplicate_symbols(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "missing"):
            runner.replay_symbols_from_nm(
                NM_OUTPUT.replace(
                    "00003bd2 00000004 T neogeo_replay_fail_trap\n",
                    "",
                )
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "duplicate"):
            runner.replay_symbols_from_nm(NM_OUTPUT + NM_OUTPUT.splitlines()[0])

    @mock.patch.object(runner.subprocess, "run")
    def test_resolver_runs_nm_against_the_requested_elf(
        self,
        run: mock.Mock,
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=NM_OUTPUT,
        )
        elf = Path("/work/replay.elf")

        resolved = runner.resolve_replay_symbols(elf, "target-nm")

        self.assertEqual(resolved.status.address, 0x100130)
        run.assert_called_once_with(
            [
                "target-nm",
                "-n",
                "-S",
                "--defined-only",
                str(elf),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )


class GdbScriptTests(unittest.TestCase):
    def test_script_breaks_on_both_raw_traps_and_reads_all_words(self) -> None:
        script = runner.build_gdb_script(sample_symbols())

        self.assertIn("break *0x00003bce", script)
        self.assertIn("break *0x00003bd2", script)
        self.assertIn("break *0x00003bd6", script)
        self.assertIn("break *0x00003bda", script)
        self.assertIn("break *0x00003bde", script)
        self.assertIn("target remote 127.0.0.1:2159", script)
        self.assertEqual(script.count("set $mb"), runner.STATUS_WORD_COUNT)
        self.assertEqual(
            script.count("set $progress_mb"),
            runner.STATUS_WORD_COUNT,
        )
        self.assertEqual(
            script.count("set $stage_mb"),
            runner.STATUS_WORD_COUNT,
        )
        self.assertEqual(
            script.count("set $transition_mb"),
            runner.STATUS_WORD_COUNT,
        )
        self.assertNotIn("ignore 3", script)
        for index in range(runner.STATUS_WORD_COUNT):
            self.assertIn(f"REPLAY_WORD index={index} ", script)
            self.assertIn(
                f"*(unsigned int *)0x{0x100130 + index * 4:08x}",
                script,
            )
        self.assertNotIn("neogeo_replay_status", script)
        self.assertNotIn("neogeo_replay_pass_trap", script)
        self.assertNotIn("neogeo_replay_fail_trap", script)
        self.assertNotIn("neogeo_replay_progress_trap", script)
        self.assertNotIn("neogeo_replay_stage_trap", script)
        self.assertNotIn("neogeo_replay_transition_trap", script)
        self.assertNotIn("unsigned short", script)
        self.assertNotIn("unsigned char", script)

    def test_rendered_script_captures_stage_and_terminal_frames(self) -> None:
        config = runner.ScreenshotConfig(
            python="/usr/bin/python3",
            helper=Path("/work/helper with space.py"),
            scrot="/usr/bin/scrot",
            directory=Path("/work/evidence frames"),
            timeout_seconds=7.5,
            maximum_bytes=123456,
        )

        script = runner.build_gdb_script(
            sample_symbols(),
            screenshot=config,
        )

        self.assertEqual(script.count("--sequence-dir"), 2)
        self.assertEqual(script.count("--output"), 2)
        self.assertEqual(script.count("terminal.png"), 2)
        self.assertIn("'/work/helper with space.py'", script)
        self.assertIn("'/work/evidence frames/stages'", script)
        self.assertIn("'/work/evidence frames/transitions'", script)
        self.assertIn("--timeout 7.5", script)
        self.assertIn("--max-bytes 123456", script)

        with self.assertRaisesRegex(runner.ReplayGateError, "newlines"):
            runner._gdb_shell_command(["ok", "bad\nargument"])

    def test_gngeo_command_uses_fixed_debug_port_mode_and_overclock(self) -> None:
        command = runner.build_gngeo_command(
            "/usr/bin/ngdevkit-gngeo",
            Path("/work/rom"),
            Path("/work/rom/gngeo_data.zip"),
            "smbneogeo",
            750,
        )

        self.assertIn("-D", command)
        self.assertIn("--68kclock=750", command)
        self.assertIn("--no-autoframeskip", command)
        self.assertIn("--no-vsync", command)
        self.assertEqual(command[-1], "smbneogeo")

    def test_gdb_command_ignores_local_initialization(self) -> None:
        command = runner.build_gdb_command(
            "target-gdb",
            Path("/work/replay.elf"),
            Path("/work/probe.gdb"),
        )

        self.assertEqual(command[0:3], ["target-gdb", "-nx", "-q"])
        self.assertIn("-batch", command)


class MailboxTests(unittest.TestCase):
    def test_mailbox_parser_maps_every_uint32_value(self) -> None:
        status = runner.parse_mailbox_words(sample_words())

        self.assertEqual(status.magic, runner.STATUS_MAGIC)
        self.assertEqual(status.result, 1)
        self.assertEqual(status.entered_mask, 0xFFFFFFFF)
        self.assertEqual(status.current_stage, 31)
        self.assertEqual(status.ram_init_option, 0)
        self.assertEqual(status.bootstrap_frames, 7)
        self.assertEqual(status.area_init_hold_frames, 1)
        self.assertEqual(status.area_init_hold_count, 6)
        self.assertEqual(status.core_frames_advanced, 67104)
        self.assertEqual(status.rendering_enabled, 1)
        self.assertEqual(status.game_frame_count, 67104)
        self.assertEqual(status.vblank_count, 67120)
        self.assertEqual(status.stage_settle_frames, 2)

    def test_mailbox_parser_rejects_size_magic_version_and_range(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "27 words"):
            runner.parse_mailbox_words(sample_words()[:-1])

        bad_magic = sample_words()
        bad_magic[0] = 0
        with self.assertRaisesRegex(runner.ReplayGateError, "magic"):
            runner.parse_mailbox_words(bad_magic)

        bad_version = sample_words()
        bad_version[1] = 1
        with self.assertRaisesRegex(runner.ReplayGateError, "version"):
            runner.parse_mailbox_words(bad_version)

        bad_range = sample_words()
        bad_range[5] = 1 << 32
        with self.assertRaisesRegex(runner.ReplayGateError, "uint32"):
            runner.parse_mailbox_words(bad_range)

    def test_mailbox_parser_rejects_impossible_scheduler_accounting(
        self,
    ) -> None:
        invalid_cases = [
            (20, 67118, "bootstrap"),
            (21, 256, "hold interval"),
            (22, 67117, "hold count"),
            (23, 0xFFFFFFFF, "core-frame accounting"),
        ]
        for index, value, message in invalid_cases:
            with self.subTest(index=index, value=value):
                words = sample_words()
                words[index] = value
                with self.assertRaisesRegex(
                    runner.ReplayGateError,
                    message,
                ):
                    runner.parse_mailbox_words(words)

    def test_mailbox_parser_requires_consistent_renderer_counters(
        self,
    ) -> None:
        invalid_cases = [
            (24, 2, "rendering_enabled"),
            (25, 0, "rendered-frame accounting"),
            (26, 10, "VBlank count"),
            (27, 256, "stage-settle"),
        ]
        for index, value, message in invalid_cases:
            with self.subTest(index=index):
                words = sample_words()
                words[index] = value
                with self.assertRaisesRegex(
                    runner.ReplayGateError,
                    message,
                ):
                    runner.parse_mailbox_words(words)

        fast = sample_words()
        fast[24] = 0
        fast[25] = 0
        parsed = runner.parse_mailbox_words(fast)
        self.assertEqual(parsed.rendering_enabled, 0)

        fast[25] = 1
        with self.assertRaisesRegex(
            runner.ReplayGateError,
            "non-rendered",
        ):
            runner.parse_mailbox_words(fast)

    def test_mailbox_parser_accepts_pre_stage_and_invalid_stage_states(
        self,
    ) -> None:
        pre_stage = sample_words(result=0, entered_mask=0, completed_mask=0)
        pre_stage[9] = runner.NO_STAGE
        pre_stage[10] = 0
        pre_stage[11] = 0
        pre_stage[12] = 0
        pre_stage[13] = 0
        pre_stage[14] = 0
        parsed = runner.parse_mailbox_words(pre_stage)
        self.assertEqual(parsed.current_stage, runner.NO_STAGE)

        invalid_stage = pre_stage.copy()
        invalid_stage[2] = runner.INVALID_STAGE_RESULT
        invalid_stage[13] = 8
        parsed = runner.parse_mailbox_words(invalid_stage)
        self.assertEqual(parsed.result, runner.INVALID_STAGE_RESULT)

        invalid_stage[13] = 0
        with self.assertRaisesRegex(
            runner.ReplayGateError,
            "valid world/level",
        ):
            runner.parse_mailbox_words(invalid_stage)

    def test_mailbox_parser_validates_tail_frame_accounting(self) -> None:
        tail_pass = sample_words()
        tail_pass[3] = tail_pass[16] + 2
        tail_pass[4] = 2
        tail_pass[23] = 67104 + 3
        tail_pass[25] = tail_pass[23]
        tail_pass[26] = tail_pass[25] + 2
        parsed = runner.parse_mailbox_words(tail_pass)
        self.assertEqual(parsed.tail_frame, 2)

        incomplete = tail_pass.copy()
        incomplete[2] = runner.INCOMPLETE_RESULT
        incomplete[23] = 67104 + 2
        incomplete[25] = incomplete[23]
        incomplete[26] = incomplete[25] + 2
        parsed = runner.parse_mailbox_words(incomplete)
        self.assertEqual(parsed.result, runner.INCOMPLETE_RESULT)

        inconsistent = tail_pass.copy()
        inconsistent[3] += 1
        with self.assertRaisesRegex(
            runner.ReplayGateError,
            "frame/tail accounting",
        ):
            runner.parse_mailbox_words(inconsistent)

    def test_gdb_output_requires_one_trap_and_all_words_once(self) -> None:
        lines = ["REPLAY_TRAP kind=pass pc=0x00003bce"]
        lines.extend(
            f"REPLAY_WORD index={index} value=0x{value:08x}"
            for index, value in enumerate(sample_words())
        )
        parsed = runner.parse_gdb_output("\n".join(lines), sample_symbols())
        self.assertEqual(parsed.trap_kind, "pass")
        self.assertEqual(parsed.status.frame, 67116)

        with self.assertRaisesRegex(runner.ReplayGateError, "missing"):
            runner.parse_gdb_output("\n".join(lines[:-1]), sample_symbols())
        with self.assertRaisesRegex(runner.ReplayGateError, "more than once"):
            runner.parse_gdb_output(
                "\n".join(lines + [lines[-1]]),
                sample_symbols(),
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "expected"):
            runner.parse_gdb_output(
                "\n".join(
                    [
                        "REPLAY_TRAP kind=pass pc=0x00003bd2",
                        *lines[1:],
                    ]
                ),
                sample_symbols(),
            )

    def test_progress_parser_keeps_complete_validated_mailboxes(self) -> None:
        lines = []
        for sample, frame in ((1, 1799), (2, 3599)):
            words = sample_words()
            words[3] = frame
            words[23] = frame + 1 - words[20] - words[22]
            words[25] = words[23]
            words[26] = words[25] + 2
            lines.extend(
                "REPLAY_PROGRESS_WORD "
                f"sample={sample} index={index} value=0x{value:08x}"
                for index, value in enumerate(words)
            )
            lines.append(f"REPLAY_PROGRESS_END sample={sample}")
        # A currently printing sample has no end marker and is ignored.
        lines.append(
            "REPLAY_PROGRESS_WORD sample=3 index=0 value=0x534d4252"
        )

        progress = runner.parse_progress_snapshots("\n".join(lines))

        self.assertEqual([item.sample for item in progress], [1, 2])
        self.assertEqual(progress[-1].status.frame, 3599)
        recorded = runner._progress_result(progress)
        self.assertEqual(recorded["sample_count"], 2)
        self.assertEqual(recorded["latest"]["fields"]["frame"], 3599)

    def test_progress_parser_rejects_completed_partial_mailbox(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "missing"):
            runner.parse_progress_snapshots(
                "\n".join(
                    [
                        (
                            "REPLAY_PROGRESS_WORD "
                            "sample=1 index=0 value=0x534d4252"
                        ),
                        "REPLAY_PROGRESS_END sample=1",
                    ]
                )
            )

    def test_stage_parser_requires_contiguous_complete_ordered_samples(
        self,
    ) -> None:
        lines: list[str] = []
        for sample, stage in ((1, 0), (2, 1)):
            words = stage_words(stage, frame=1000 + stage * 100)
            lines.extend(
                "REPLAY_STAGE_WORD "
                f"sample={sample} index={index} value=0x{value:08x}"
                for index, value in enumerate(words)
            )
            lines.append(f"REPLAY_STAGE_END sample={sample}")

        captures = runner.parse_stage_snapshots("\n".join(lines))

        self.assertEqual([item.sample for item in captures], [1, 2])
        self.assertEqual(captures[1].status.current_stage, 1)

        skipped = "\n".join(line.replace("sample=2", "sample=3") for line in lines)
        with self.assertRaisesRegex(runner.ReplayGateError, "contiguous"):
            runner.parse_stage_snapshots(skipped)

        with self.assertRaisesRegex(runner.ReplayGateError, "missing"):
            runner.parse_stage_snapshots(
                "\n".join(
                    [
                        (
                            "REPLAY_STAGE_WORD "
                            "sample=1 index=0 value=0x534d4252"
                        ),
                        "REPLAY_STAGE_END sample=1",
                    ]
                )
            )

    def test_transition_parser_maps_complete_samples(self) -> None:
        words = stage_words(0, frame=998)
        lines = [
            (
                "REPLAY_TRANSITION_WORD "
                f"sample=1 index={index} value=0x{value:08x}"
            )
            for index, value in enumerate(words)
        ]
        lines.append("REPLAY_TRANSITION_END sample=1")

        captures = runner.parse_transition_snapshots("\n".join(lines))

        self.assertEqual(len(captures), 1)
        self.assertEqual(captures[0].status.current_stage, 0)


class RenderedEvidenceTests(unittest.TestCase):
    def populate_images(self, directory: Path, count: int = 32) -> None:
        stages = directory / "stages"
        transitions = directory / "transitions"
        stages.mkdir()
        transitions.mkdir()
        for index in range(1, count + 1):
            write_visual_png(
                stages / f"stage-{index:04d}.png",
                index,
            )
            write_transition_png(
                transitions / f"stage-{index:04d}.png"
            )
        write_visual_png(directory / "terminal.png", 100)

    def test_complete_rendered_evidence_binds_all_stage_images(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)

            result = runner._rendered_evidence_result(
                stage_captures(),
                transition_captures(),
                rendered_capture(),
                directory,
                runner.MAX_SCREENSHOT_BYTES,
                True,
            )

        self.assertEqual(result["stage_count"], 32)
        self.assertTrue(result["complete_stage_set"])
        self.assertEqual(result["stages"][0]["world"], 1)
        self.assertEqual(result["stages"][-1]["level"], 4)
        self.assertEqual(
            result["stages"][5]["game_frame_count"],
            result["stages"][5]["core_frames_advanced"],
        )
        self.assertEqual(result["terminal"]["image"]["width"], 1024)

    def test_complete_result_manifest_fits_its_hard_bound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            rendered = runner._rendered_evidence_result(
                stage_captures(),
                transition_captures(),
                rendered_capture(),
                directory,
                runner.MAX_SCREENSHOT_BYTES,
                True,
            )
            result = {
                "schema_version": runner.RESULT_SCHEMA_VERSION,
                "outcome": "complete",
                "passed": True,
                "arguments": {
                    "argv": ["--rendered-evidence"],
                    "evidence_dir": str(directory),
                    "timeout_effective_seconds": 7200,
                    "m68k_overclock_percent": 0,
                },
                "artifacts": {
                    f"artifact_{index}": {
                        "path": f"inputs/artifact-{index}.bin",
                        "bytes": 1048576,
                        "sha256": "a" * 64,
                    }
                    for index in range(9)
                },
                "mailbox": {
                    "bytes": runner.STATUS_BYTES,
                    "raw_words": sample_words(),
                    "fields": asdict(rendered_capture().status),
                },
                "progress": {
                    "sample_count": 38,
                    "latest": {
                        "sample": 38,
                        "raw_words": sample_words(),
                        "fields": asdict(rendered_capture().status),
                    },
                },
                "rendered_evidence": rendered,
            }
            result_path = directory / "result.json"
            runner._write_result(result_path, result)

            self.assertLessEqual(
                result_path.stat().st_size,
                runner.MAX_RESULT_JSON_BYTES,
            )

    def test_missing_stale_black_and_nonrendered_evidence_fail(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            (directory / "stages" / "stage-0032.png").unlink()
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "do not match",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            (directory / "stages" / "stage-0002.png").write_bytes(
                (directory / "stages" / "stage-0001.png").read_bytes()
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "pixel-identical",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            (directory / "transitions" / "stage-0001.png").write_bytes(
                (directory / "stages" / "stage-0001.png").read_bytes()
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "immediate transition",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            from PIL import Image

            Image.new("RGB", (1024, 768), (0, 0, 0)).save(
                directory / "stages" / "stage-0001.png"
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "visible game content",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            (directory / "terminal.png").write_bytes(
                (directory / "stages" / "stage-0032.png").read_bytes()
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "terminal playfield",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            stages = stage_captures()
            stages[0] = replace(
                stages[0],
                status=replace(
                    stages[0].status,
                    stage_settle_frames=3,
                ),
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "two settling",
            ):
                runner._rendered_evidence_result(
                    stages,
                    transition_captures(),
                    rendered_capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "hardware-playable",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    capture(),
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.populate_images(directory)
            terminal = rendered_capture()
            terminal = replace(
                terminal,
                status=replace(
                    terminal.status,
                    rendering_enabled=0,
                    game_frame_count=0,
                ),
            )
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "non-rendered",
            ):
                runner._rendered_evidence_result(
                    stage_captures(),
                    transition_captures(),
                    terminal,
                    directory,
                    runner.MAX_SCREENSHOT_BYTES,
                    True,
                )


class ClassificationTests(unittest.TestCase):
    def test_only_complete_pass_trap_with_both_full_masks_passes(self) -> None:
        result = runner.classify_result(capture())

        self.assertTrue(result.passed)
        self.assertEqual(result.outcome, "complete")

        cases = [
            capture(trap_kind="fail"),
            capture(result=0),
            capture(
                entered_mask=0x7FFFFFFF,
                completed_mask=0x7FFFFFFF,
            ),
            capture(completed_mask=0x7FFFFFFF),
        ]
        for candidate in cases:
            with self.subTest(candidate=candidate):
                self.assertFalse(runner.classify_result(candidate).passed)

    def test_complete_result_requires_the_final_stable_victory_state(
        self,
    ) -> None:
        valid = capture()
        fields = {
            "current_stage": 0,
            "victory_stable_frames": 0,
            "oper_mode": 1,
            "oper_mode_task": 3,
            "world": 0,
            "level": 0,
            "world_end_timer": 1,
        }
        for field, value in fields.items():
            with self.subTest(field=field):
                status = replace(valid.status, **{field: value})
                malformed = replace(valid, status=status)
                self.assertFalse(
                    runner.classify_result(malformed).passed
                )

    def test_incomplete_and_gate_failures_are_distinguished(self) -> None:
        incomplete = runner.classify_result(
            capture(trap_kind="fail", result=runner.INCOMPLETE_RESULT)
        )
        failed = runner.classify_result(
            capture(trap_kind="fail", result=3)
        )

        self.assertEqual(incomplete.outcome, "incomplete")
        self.assertIn("result=incomplete(0x00000100)", incomplete.detail)
        self.assertEqual(failed.outcome, "failure")
        self.assertIn("entered=", failed.detail)


class ArtifactProvenanceTests(unittest.TestCase):
    def test_frozen_input_is_recorded_and_change_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            evidence = Path(temporary)
            source = evidence / "source.bin"
            source.write_bytes(b"original replay input")
            frozen = runner._snapshot_file(
                source,
                evidence / "inputs" / "source.bin",
            )
            artifacts = {
                "source": runner._artifact_record(
                    frozen,
                    relative_to=evidence,
                )
            }

            runner._verify_frozen_artifacts(artifacts, evidence)
            source.write_bytes(b"changed workspace input")
            runner._verify_frozen_artifacts(artifacts, evidence)

            frozen.chmod(0o644)
            frozen.write_bytes(b"mutated frozen input")
            with self.assertRaisesRegex(
                runner.ReplayGateError,
                "changed during replay",
            ):
                runner._verify_frozen_artifacts(artifacts, evidence)


class TimeoutAndArgumentsTests(unittest.TestCase):
    def test_explicit_timeout_is_preserved_and_default_is_finite(self) -> None:
        self.assertEqual(runner.derive_gate_timeout(42.5, 0), 42.5)
        normal = runner.derive_gate_timeout(None, 0)
        accelerated = runner.derive_gate_timeout(None, 1000)

        self.assertTrue(math.isfinite(normal))
        self.assertGreater(normal, accelerated)
        self.assertGreaterEqual(
            accelerated,
            runner.MIN_DERIVED_TIMEOUT_SECONDS,
        )

    def test_timeout_and_overclock_ranges_are_enforced(self) -> None:
        for timeout in (0.0, -1.0, math.inf, math.nan):
            with self.subTest(timeout=timeout):
                with self.assertRaisesRegex(
                    runner.ReplayGateError,
                    "timeout",
                ):
                    runner.derive_gate_timeout(timeout, 0)
        with self.assertRaisesRegex(runner.ReplayGateError, "overclock"):
            runner.derive_gate_timeout(None, -1)
        with self.assertRaisesRegex(runner.ReplayGateError, "overclock"):
            runner.derive_gate_timeout(
                None,
                runner.MAX_68K_OVERCLOCK + 1,
            )

    def test_parser_accepts_required_surface_and_default_rom_set(self) -> None:
        args = runner.build_argument_parser().parse_args(
            [
                "--elf",
                "/work/gate.elf",
                "--rom-dir",
                "/work/rom",
                "--data-file",
                "/work/gngeo_data.zip",
                "--timeout",
                "90",
                "--evidence-dir",
                "/work/evidence",
                "--68k-overclock",
                "500",
            ]
        )

        self.assertEqual(args.rom_set, "smbneogeo")
        self.assertEqual(args.timeout, 90)
        self.assertEqual(args.m68k_overclock, 500)
        self.assertFalse(args.rendered_evidence)

    def test_rendered_mode_selects_rendered_paths_and_records_clock(
        self,
    ) -> None:
        args = runner.build_argument_parser().parse_args(
            [
                "--rendered-evidence",
            ]
        )
        runner._apply_default_paths(args)

        self.assertIn("build/replay-rendered/", str(args.elf))
        self.assertIn("build/replay-rendered/rom", str(args.rom_dir))
        self.assertEqual(
            runner._validate_runtime_arguments(args),
            runner.DEFAULT_RENDERED_TIMEOUT_SECONDS,
        )
        self.assertEqual(args.m68k_overclock, 0)

        args.timeout = 42
        args.m68k_overclock = 1000
        self.assertEqual(
            runner._validate_runtime_arguments(args),
            42,
        )

    def test_runtime_validation_rejects_unsafe_romset_and_bad_intervals(
        self,
    ) -> None:
        args = runner.build_argument_parser().parse_args([])

        with self.assertRaisesRegex(runner.ReplayGateError, "ROM set"):
            runner._validate_runtime_arguments(
                replace_namespace(args, rom_set="../other")
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "heartbeat"):
            runner._validate_runtime_arguments(
                replace_namespace(args, heartbeat_seconds=0.0)
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "startup"):
            runner._validate_runtime_arguments(
                replace_namespace(args, startup_timeout=float("inf"))
            )
        with self.assertRaisesRegex(runner.ReplayGateError, "screenshot"):
            runner._validate_runtime_arguments(
                replace_namespace(args, screenshot_timeout=61)
            )


def replace_namespace(
    namespace: argparse.Namespace,
    **changes: object,
) -> argparse.Namespace:
    values = vars(namespace).copy()
    values.update(changes)
    return argparse.Namespace(**values)


if __name__ == "__main__":
    unittest.main()
