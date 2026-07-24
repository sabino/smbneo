#!/usr/bin/env python3

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))
import core_state_trace as trace  # noqa: E402
import rec_tool  # noqa: E402


def fm2_bytes(frame_count: int) -> bytes:
    headers = [
        "version 3",
        "emuVersion 22020",
        "rerecordCount 0",
        "palFlag 0",
        "romFilename test.nes",
        f"romChecksum {rec_tool.SUPPORTED_FM2_ROM_CHECKSUM}",
        "fourscore 0",
        "microphone 0",
        "port0 1",
        "port1 0",
        "port2 0",
        "FDS 0",
        "NewPPU 0",
        "RAMInitOption 2",
        "RAMInitSeed 0",
        "startsFromSavestate 0",
        "binary 0",
    ]
    records = ["|0|........|||"] * frame_count
    return ("\n".join(headers + records) + "\n").encode()


def base_values() -> dict[str, int]:
    return {
        "input": 0,
        "semantic_hash": 0x11111111,
        "full_ram_hash": 0x22222222,
        "zero_page_hash": 0x33333333,
        "stack_hash": 0x44444444,
        "oam_hash": 0x55555555,
        "work_hash": 0x66666666,
        "oper_mode": 1,
        "oper_mode_task": 3,
        "world": 0,
        "level": 0,
        "engine_subroutine": 8,
        "player_state": 0,
        "player_page": 1,
        "player_x": 64,
        "player_y": 160,
        "screen_page": 0,
        "screen_x": 0,
        "world_end_timer": 0,
        "lagged": 0,
        "lag_count": 0,
    }


def write_trace(
    path: Path,
    rows: list[dict[str, int]],
    *,
    complete: bool = True,
) -> None:
    lines = [
        f"# schema={trace.TRACE_SCHEMA}",
        "# source=test",
        "# frame_semantics=post_input_nmi",
        f"# frames={len(rows)}",
        ",".join(trace.TRACE_COLUMNS),
    ]
    for frame, values in enumerate(rows):
        cells = []
        all_values = {"frame": frame, **values}
        for column in trace.TRACE_COLUMNS:
            value = all_values[column]
            cells.append(
                f"{value:08x}"
                if column in trace.HASH_COLUMNS
                else str(value)
            )
        lines.append(",".join(cells))
    if complete:
        lines.append("# complete=1")
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


class TranscriptParsingTests(unittest.TestCase):
    def test_parser_requires_schema_columns_contiguous_frames_and_marker(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            valid = temporary / "valid.csv"
            write_trace(valid, [base_values(), base_values()])

            transcript = trace.load_transcript(valid)

            self.assertEqual(len(transcript.rows), 2)
            self.assertEqual(
                transcript.rows[0].values["semantic_hash"],
                0x11111111,
            )
            self.assertEqual(len(transcript.sha256), 64)

            incomplete = temporary / "incomplete.csv"
            write_trace(incomplete, [base_values()], complete=False)
            with self.assertRaisesRegex(trace.TraceError, "incomplete"):
                trace.load_transcript(incomplete)

            discontinuous = temporary / "discontinuous.csv"
            write_trace(discontinuous, [base_values(), base_values()])
            content = discontinuous.read_text().replace(
                "\n1,0,",
                "\n7,0,",
            )
            discontinuous.write_text(content)
            with self.assertRaisesRegex(trace.TraceError, "contiguous"):
                trace.load_transcript(discontinuous)


class ComparisonTests(unittest.TestCase):
    def test_first_semantic_divergence_reports_exact_input_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            translated_path = temporary / "translated.csv"
            reference_path = temporary / "reference.csv"
            translated_rows = [base_values() for _ in range(4)]
            reference_rows = [base_values() for _ in range(4)]
            translated_rows[2] = {
                **base_values(),
                "input": 0x81,
                "semantic_hash": 0xABCDEF01,
                "zero_page_hash": 0xABCDEF02,
                "player_x": 65,
            }
            reference_rows[2] = {
                **base_values(),
                "input": 0x81,
            }
            write_trace(translated_path, translated_rows)
            write_trace(reference_path, reference_rows)

            comparison = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
            )

            self.assertFalse(comparison.matches)
            self.assertEqual(comparison.outcome, "state_divergence")
            self.assertEqual(comparison.first_divergent_frame, 2)
            self.assertEqual(comparison.previous_matching_frame, 1)
            self.assertEqual(comparison.input_state, 0x81)
            self.assertNotIn("semantic_hash", comparison.differing_fields)
            self.assertEqual(
                comparison.first_semantic_hash_difference,
                2,
            )
            self.assertIn("player_x", comparison.differing_fields)

    def test_instruction_stack_differences_are_diagnostic_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            translated_path = temporary / "translated.csv"
            reference_path = temporary / "reference.csv"
            translated = base_values()
            reference = {
                **base_values(),
                "stack_hash": 0xDEADBEEF,
                "full_ram_hash": 0xFEEDFACE,
            }
            write_trace(translated_path, [translated])
            write_trace(reference_path, [reference])

            comparison = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
            )

            self.assertTrue(comparison.matches)
            self.assertEqual(comparison.first_full_ram_difference, 0)

    def test_reference_offset_maps_divergence_to_source_input_frame(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            translated_path = temporary / "translated.csv"
            reference_path = temporary / "reference.csv"
            reference_rows = [base_values() for _ in range(5)]
            translated_rows = [base_values() for _ in range(3)]
            translated_rows[1] = {
                **base_values(),
                "input": 8,
                "work_hash": 0x12345678,
                "player_x": 65,
            }
            reference_rows[3] = {
                **base_values(),
                "input": 8,
                "work_hash": 0x87654321,
                "player_x": 64,
            }
            write_trace(translated_path, translated_rows)
            write_trace(reference_path, reference_rows)

            comparison = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
                reference_frame_offset=2,
            )

            self.assertFalse(comparison.matches)
            self.assertEqual(comparison.first_divergent_frame, 3)
            self.assertEqual(
                comparison.first_divergent_translated_frame,
                1,
            )
            self.assertEqual(comparison.previous_matching_frame, 2)
            self.assertEqual(comparison.reference_frame_offset, 2)

    def test_hold_schedule_uses_aligned_reference_lag_flags(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            path = Path(temporary_name) / "reference.csv"
            rows = [base_values() for _ in range(5)]
            rows[2] = {**base_values(), "lagged": 1, "lag_count": 1}
            rows[4] = {**base_values(), "lagged": 1, "lag_count": 2}
            write_trace(path, rows)

            schedule = trace.build_hold_schedule(
                trace.load_transcript(path),
                reference_frame_offset=1,
                frame_count=4,
            )

            self.assertEqual(schedule, bytes([1, 0, 1, 0]))

    def test_hold_schedule_uses_one_reference_row_of_lookahead(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            path = Path(temporary_name) / "reference.csv"
            rows = [base_values() for _ in range(6)]
            rows[5] = {**base_values(), "lagged": 1, "lag_count": 1}
            write_trace(path, rows)

            schedule = trace.build_hold_schedule(
                trace.load_transcript(path),
                reference_frame_offset=1,
                frame_count=4,
            )

            self.assertEqual(schedule, bytes([0, 0, 0, 1]))

    def test_scheduled_hold_rows_validate_input_but_skip_atomic_state(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            translated_path = temporary / "translated.csv"
            reference_path = temporary / "reference.csv"
            translated_rows = [base_values(), base_values()]
            reference_rows = [base_values(), base_values()]
            translated_rows[0] = {
                **base_values(),
                "lagged": 1,
                "oam_hash": 0xAAAAAAAA,
                "player_x": 99,
            }
            reference_rows[0] = {
                **base_values(),
                "oam_hash": 0xBBBBBBBB,
                "player_x": 44,
            }
            write_trace(translated_path, translated_rows)
            write_trace(reference_path, reference_rows)

            strict = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
            )
            scheduled = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
                skip_scheduled_holds=True,
            )

            self.assertFalse(strict.matches)
            self.assertTrue(scheduled.matches)
            self.assertEqual(scheduled.skipped_hold_frames, (0,))
            self.assertEqual(scheduled.compared_frames, 1)

    def test_comparison_accepts_one_schedule_lookahead_row(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            translated_path = temporary / "translated.csv"
            reference_path = temporary / "reference.csv"
            translated_rows = [
                base_values(),
                {**base_values(), "lagged": 1, "lag_count": 1},
            ]
            reference_rows = [
                base_values(),
                base_values(),
                {**base_values(), "lagged": 1, "lag_count": 1},
            ]
            write_trace(translated_path, translated_rows)
            write_trace(reference_path, reference_rows)

            comparison = trace.compare_transcripts(
                trace.load_transcript(translated_path),
                trace.load_transcript(reference_path),
                skip_scheduled_holds=True,
            )

            self.assertTrue(comparison.matches)
            self.assertEqual(comparison.compared_frames, 1)
            self.assertEqual(comparison.skipped_hold_frames, (1,))


class CommandTests(unittest.TestCase):
    def test_exclusive_text_output_preserves_an_existing_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            output = Path(temporary_name) / "result.json"
            output.write_bytes(b"preserve")

            with self.assertRaisesRegex(trace.TraceError, "already exists"):
                trace._write_text_exclusive(
                    output,
                    '{"replacement": true}\n',
                    "result output",
                )

            self.assertEqual(output.read_bytes(), b"preserve")

    def test_failed_copy_does_not_unlink_an_external_replacement(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            source = temporary / "source.csv"
            output = temporary / "output.csv"
            source.write_bytes(b"trace")

            def replace_then_fail(*_args, **_kwargs) -> None:
                output.unlink()
                output.write_bytes(b"replacement")
                raise OSError("injected copy failure")

            with mock.patch.object(
                shutil,
                "copyfileobj",
                side_effect=replace_then_fail,
            ):
                with self.assertRaisesRegex(
                    OSError,
                    "injected copy failure",
                ):
                    trace._copy_file_exclusive(
                        source,
                        output,
                        "output",
                    )

            self.assertEqual(output.read_bytes(), b"replacement")

    def test_host_build_contains_only_translated_core_and_host_stubs(
        self,
    ) -> None:
        command = trace.build_host_compile_command(
            "clang",
            Path("/repo"),
            Path("/tmp/generated"),
            Path("/tmp/trace"),
        )
        joined = " ".join(command)

        self.assertIn("/repo/tools/core_state_trace.c", joined)
        self.assertIn("/repo/codegen/lib/ppu.c", joined)
        self.assertIn("/repo/platform/neogeo/apu_null.c", joined)
        self.assertNotIn("/repo/platform/neogeo/video.c", joined)
        self.assertNotIn("/repo/platform/neogeo/replay_main.c", joined)

    def test_fceux_command_uses_lua_without_broken_nogui_flag(self) -> None:
        command = trace.build_fceux_command(
            "/tmp/fceux",
            Path("/tmp/game.nes"),
            Path("/tmp/movie.fm2"),
            Path("/repo/tools/fceux_ram_trace.lua"),
        )
        lua = (
            Path(__file__).resolve().parent / "fceux_ram_trace.lua"
        ).read_text()

        self.assertIn("--playmov", command)
        self.assertIn("--loadlua", command)
        self.assertNotIn("--nogui", command)
        self.assertIn('emu.speedmode("maximum")', lua)
        self.assertIn("after the first emu.frameadvance", lua)
        self.assertIn('io.open(output_path, "wx")', lua)
        self.assertIn("cannot exclusively create trace output", lua)
        self.assertIn("SMB_TRACE_EMULATOR_SHA256", lua)

    def test_fceux_command_records_the_executable_fingerprint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            rom = temporary / "game.nes"
            fm2 = temporary / "movie.fm2"
            lua = temporary / "trace.lua"
            output = temporary / "reference.csv"
            for path in (rom, fm2, lua):
                path.write_bytes(b"test")
            stdout = io.StringIO()

            with redirect_stdout(stdout):
                result = trace.main(
                    [
                        "fceux-command",
                        "--fceux",
                        "/bin/true",
                        "--rom",
                        str(rom),
                        "--fm2",
                        str(fm2),
                        "--lua",
                        str(lua),
                        "--output",
                        str(output),
                        "--frames",
                        "1",
                    ]
                )

            payload = json.loads(stdout.getvalue())
            environment = payload["environment"]
            self.assertEqual(result, 0)
            self.assertEqual(
                environment["SMB_TRACE_EMULATOR_LABEL"],
                "true",
            )
            self.assertEqual(
                environment["SMB_TRACE_EMULATOR_SHA256"],
                trace._sha256_file(Path("/bin/true")),
            )
            command = payload["command"]
            self.assertIn("exec-fceux-verified", command)
            self.assertIn(
                environment["SMB_TRACE_EMULATOR_SHA256"],
                command,
            )

    def test_verified_executable_descriptor_survives_path_replacement(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            executable = Path(temporary_name) / "emulator"
            original = b"original executable"
            executable.write_bytes(original)
            executable.chmod(0o755)
            expected = trace._sha256_file(executable)

            descriptor = trace._open_verified_executable(
                executable,
                expected,
            )
            try:
                executable.unlink()
                executable.write_bytes(b"different executable")
                executable.chmod(0o755)
                self.assertEqual(
                    os.read(descriptor, len(original)),
                    original,
                )
            finally:
                os.close(descriptor)

    def test_verified_wrapper_binds_lua_metadata_to_opened_binary(
        self,
    ) -> None:
        executable = Path("/bin/true").resolve()
        expected = trace._sha256_file(executable)

        with mock.patch.dict(
            os.environ,
            {
                "SMB_TRACE_EMULATOR_LABEL": "wrong-build",
                "SMB_TRACE_EMULATOR_SHA256": "0" * 64,
            },
        ):
            with mock.patch.object(os, "execve") as execve:
                with mock.patch.object(
                    os,
                    "supports_fd",
                    {*os.supports_fd, execve},
                ):
                    trace.execute_verified_fceux(
                        executable,
                        expected,
                        Path("/tmp/game.nes"),
                        Path("/tmp/movie.fm2"),
                        Path("/tmp/trace.lua"),
                    )

        environment = execve.call_args.args[2]
        self.assertEqual(
            environment["SMB_TRACE_EMULATOR_LABEL"],
            executable.name,
        )
        self.assertEqual(
            environment["SMB_TRACE_EMULATOR_SHA256"],
            expected,
        )

    def test_fceux_command_refuses_an_existing_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            rom = temporary / "game.nes"
            fm2 = temporary / "movie.fm2"
            lua = temporary / "trace.lua"
            output = temporary / "reference.csv"
            for path in (rom, fm2, lua, output):
                path.write_bytes(b"test")
            stderr = io.StringIO()

            with redirect_stderr(stderr):
                result = trace.main(
                    [
                        "fceux-command",
                        "--fceux",
                        "/bin/true",
                        "--rom",
                        str(rom),
                        "--fm2",
                        str(fm2),
                        "--lua",
                        str(lua),
                        "--output",
                        str(output),
                        "--frames",
                        "1",
                    ]
                )

            self.assertEqual(result, 2)
            self.assertIn("already exists", stderr.getvalue())


class GeneratedDispatcherTests(unittest.TestCase):
    def test_every_jump_dispatch_restores_the_assembly_asl_carry(self) -> None:
        repository = Path(__file__).resolve().parents[1]
        generated = (
            repository / "codegen" / "lib" / "code.c"
        ).read_text(encoding="ascii")
        lines = generated.splitlines()
        switch_lines = [
            index
            for index, line in enumerate(lines)
            if line.strip() == "switch (a) {"
        ]

        self.assertGreater(len(switch_lines), 0)
        for index in switch_lines:
            self.assertEqual(
                lines[index - 1].strip(),
                "carry_flag = (a & 0x80u) != 0;",
            )


@unittest.skipUnless(shutil.which("clang"), "clang is required")
class HostEmitterIntegrationTests(unittest.TestCase):
    def test_four_frame_fm2_compiles_and_runs_host_natively(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            fm2 = temporary / "movie.fm2"
            output = temporary / "translated.csv"
            fm2.write_bytes(fm2_bytes(4))

            transcript = trace.emit_translated_trace(
                fm2,
                output,
                frames=4,
                input_frame_offset=0,
                cc="clang",
                timeout_seconds=60,
            )

            self.assertEqual(len(transcript.rows), 4)
            self.assertTrue(output.is_file())
            self.assertEqual(
                transcript.metadata["frame_semantics"],
                "post_input_nmi",
            )
            self.assertEqual(
                [row.frame for row in transcript.rows],
                [0, 1, 2, 3],
            )

    def test_ram_diagnostic_dump_refuses_an_existing_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = Path(temporary_name)
            fm2 = temporary / "movie.fm2"
            output = temporary / "translated.csv"
            dump = temporary / "ram.bin"
            fm2.write_bytes(fm2_bytes(4))
            dump.write_bytes(b"preserve")

            with mock.patch.dict(
                os.environ,
                {
                    "SMB_TRACE_RAM_DUMP_FRAME": "0",
                    "SMB_TRACE_RAM_DUMP_OUTPUT": str(dump),
                },
            ):
                with self.assertRaisesRegex(
                    trace.TraceError,
                    "exclusively create RAM dump",
                ):
                    trace.emit_translated_trace(
                        fm2,
                        output,
                        frames=4,
                        input_frame_offset=0,
                        cc="clang",
                        timeout_seconds=60,
                    )

            self.assertEqual(dump.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
