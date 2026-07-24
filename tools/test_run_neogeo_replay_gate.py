#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_neogeo_replay_gate as runner  # noqa: E402


NM_OUTPUT = """\
00003bce 00000004 T neogeo_replay_pass_trap
00003bd2 00000004 T neogeo_replay_fail_trap
00003bd6 00000004 T neogeo_replay_progress_trap
00100130 00000050 B neogeo_replay_status
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
            80,
            "B",
        ),
        progress_point=runner.ElfSymbol(
            "neogeo_replay_progress_trap",
            0x3BD6,
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
        67117,
        60,
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
    ]


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


class SymbolResolutionTests(unittest.TestCase):
    def test_nm_parser_reads_and_validates_exact_gate_objects(self) -> None:
        symbols = runner.replay_symbols_from_nm(NM_OUTPUT)

        self.assertEqual(symbols.pass_trap.address, 0x3BCE)
        self.assertEqual(symbols.fail_trap.size, 4)
        self.assertEqual(symbols.status.address, 0x100130)
        self.assertEqual(symbols.status.size, runner.STATUS_BYTES)

    def test_nm_parser_rejects_wrong_mailbox_size_and_alignment(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "exactly 80"):
            runner.replay_symbols_from_nm(
                NM_OUTPUT.replace("00000050 B", "0000004c B")
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
    def test_script_breaks_on_both_raw_traps_and_reads_twenty_words(self) -> None:
        script = runner.build_gdb_script(sample_symbols())

        self.assertIn("break *0x00003bce", script)
        self.assertIn("break *0x00003bd2", script)
        self.assertIn("break *0x00003bd6", script)
        self.assertIn("target remote 127.0.0.1:2159", script)
        self.assertEqual(script.count("set $mb"), 20)
        self.assertEqual(script.count("set $progress_mb"), 20)
        self.assertNotIn("ignore 3", script)
        for index in range(20):
            self.assertIn(f"REPLAY_WORD index={index} ", script)
            self.assertIn(
                f"*(unsigned int *)0x{0x100130 + index * 4:08x}",
                script,
            )
        self.assertNotIn("neogeo_replay_status", script)
        self.assertNotIn("neogeo_replay_pass_trap", script)
        self.assertNotIn("neogeo_replay_fail_trap", script)
        self.assertNotIn("neogeo_replay_progress_trap", script)
        self.assertNotIn("unsigned short", script)
        self.assertNotIn("unsigned char", script)

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


class MailboxTests(unittest.TestCase):
    def test_mailbox_parser_maps_exactly_twenty_uint32_values(self) -> None:
        status = runner.parse_mailbox_words(sample_words())

        self.assertEqual(status.magic, runner.STATUS_MAGIC)
        self.assertEqual(status.result, 1)
        self.assertEqual(status.entered_mask, 0xFFFFFFFF)
        self.assertEqual(status.current_stage, 31)
        self.assertEqual(status.ram_init_option, 0)

    def test_mailbox_parser_rejects_size_magic_version_and_range(self) -> None:
        with self.assertRaisesRegex(runner.ReplayGateError, "19 words"):
            runner.parse_mailbox_words(sample_words()[:-1])

        bad_magic = sample_words()
        bad_magic[0] = 0
        with self.assertRaisesRegex(runner.ReplayGateError, "magic"):
            runner.parse_mailbox_words(bad_magic)

        bad_version = sample_words()
        bad_version[1] = 2
        with self.assertRaisesRegex(runner.ReplayGateError, "version"):
            runner.parse_mailbox_words(bad_version)

        bad_range = sample_words()
        bad_range[5] = 1 << 32
        with self.assertRaisesRegex(runner.ReplayGateError, "uint32"):
            runner.parse_mailbox_words(bad_range)

    def test_gdb_output_requires_one_trap_and_all_words_once(self) -> None:
        lines = ["REPLAY_TRAP kind=pass pc=0x00003bce"]
        lines.extend(
            f"REPLAY_WORD index={index} value=0x{value:08x}"
            for index, value in enumerate(sample_words())
        )
        parsed = runner.parse_gdb_output("\n".join(lines), sample_symbols())
        self.assertEqual(parsed.trap_kind, "pass")
        self.assertEqual(parsed.status.frame, 67117)

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


class ClassificationTests(unittest.TestCase):
    def test_only_complete_pass_trap_with_both_full_masks_passes(self) -> None:
        result = runner.classify_result(capture())

        self.assertTrue(result.passed)
        self.assertEqual(result.outcome, "complete")

        cases = [
            capture(trap_kind="fail"),
            capture(result=0),
            capture(entered_mask=0x7FFFFFFF),
            capture(completed_mask=0x7FFFFFFF),
        ]
        for candidate in cases:
            with self.subTest(candidate=candidate):
                self.assertFalse(runner.classify_result(candidate).passed)

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


def replace_namespace(
    namespace: argparse.Namespace,
    **changes: object,
) -> argparse.Namespace:
    values = vars(namespace).copy()
    values.update(changes)
    return argparse.Namespace(**values)


if __name__ == "__main__":
    unittest.main()
