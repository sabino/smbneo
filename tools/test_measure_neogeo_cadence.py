#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure_neogeo_cadence as cadence  # noqa: E402


NM_OUTPUT = """\
00001234 00000010 T rom_callback_VBlank
00001300 00000028 T unrelated_function
0010a004 00000004 b neogeo_game_frame_count
0010a008 00000004 B neogeo_vblank_count
"""


def sample_symbols() -> cadence.CadenceSymbols:
    return cadence.CadenceSymbols(
        vblank_callback=cadence.ElfSymbol(
            name="rom_callback_VBlank",
            address=0x1234,
            size=0x10,
            kind="T",
        ),
        vblank_count=cadence.ElfSymbol(
            name="neogeo_vblank_count",
            address=0x10FF00,
            size=4,
            kind="B",
        ),
        game_frame_count=cadence.ElfSymbol(
            name="neogeo_game_frame_count",
            address=0x10FF04,
            size=4,
            kind="b",
        ),
    )


class SymbolResolutionTests(unittest.TestCase):
    def test_parse_nm_symbols_reads_hex_address_size_and_kind(self) -> None:
        symbols = cadence.parse_nm_symbols(NM_OUTPUT)

        self.assertEqual(symbols["rom_callback_VBlank"].address, 0x1234)
        self.assertEqual(symbols["neogeo_vblank_count"].size, 4)
        self.assertEqual(symbols["neogeo_vblank_count"].kind, "B")
        self.assertEqual(symbols["neogeo_game_frame_count"].address, 0x10A004)

    @mock.patch.object(cadence.subprocess, "run")
    def test_resolve_uses_current_elf_nm_output(
        self,
        run: mock.Mock,
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=NM_OUTPUT,
        )
        elf = Path("/work/current-build.elf")

        resolved = cadence.resolve_cadence_symbols(elf, "target-nm")

        self.assertEqual(resolved.vblank_callback.address, 0x1234)
        self.assertEqual(resolved.vblank_count.address, 0x10A008)
        self.assertEqual(resolved.game_frame_count.address, 0x10A004)
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

    @mock.patch.object(cadence.subprocess, "run")
    def test_resolve_rejects_non_32_bit_counter(
        self,
        run: mock.Mock,
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=NM_OUTPUT.replace(
                "0010a008 00000004 B neogeo_vblank_count",
                "0010a008 00000002 B neogeo_vblank_count",
            ),
        )

        with self.assertRaisesRegex(
            cadence.CadenceError,
            "only reads 32-bit scalars",
        ):
            cadence.resolve_cadence_symbols(Path("current.elf"), "target-nm")

    def test_duplicate_exact_symbol_is_rejected(self) -> None:
        with self.assertRaisesRegex(cadence.CadenceError, "duplicate"):
            cadence.parse_nm_symbols(NM_OUTPUT + NM_OUTPUT.splitlines()[0])


class CommandConstructionTests(unittest.TestCase):
    def test_gdb_script_uses_resolved_addresses_and_32_bit_reads(self) -> None:
        script = cadence.build_gdb_script(
            sample_symbols(),
            warmup_vblanks=30,
            sample_vblanks=[60, 120],
        )

        self.assertIn("break *0x00001234", script)
        self.assertIn(
            "set $vb0 = *(unsigned int *)0x0010ff00",
            script,
        )
        self.assertIn(
            "set $gf0 = *(unsigned int *)0x0010ff04",
            script,
        )
        self.assertNotIn("neogeo_vblank_count", script)
        self.assertNotIn("neogeo_game_frame_count", script)
        self.assertNotIn("unsigned short", script)
        self.assertNotIn("unsigned char", script)
        self.assertEqual(script.count("*(unsigned int *)"), 8)
        self.assertIn("ignore 1 29", script)
        self.assertIn("ignore 1 59", script)
        self.assertIn("ignore 1 119", script)
        self.assertIn("phase=sample_2", script)

    def test_gdb_script_supports_zero_warmup_and_custom_samples(self) -> None:
        script = cadence.build_gdb_script(
            sample_symbols(),
            warmup_vblanks=0,
            sample_vblanks=[1, 17],
        )

        self.assertNotIn("phase=warmup", script)
        self.assertIn("ignore 1 0", script)
        self.assertIn("ignore 1 16", script)
        self.assertEqual(script.count("*(unsigned int *)"), 6)

    def test_gngeo_command_pins_timing_and_debug_mode(self) -> None:
        command = cadence.build_gngeo_command(
            "/usr/bin/ngdevkit-gngeo",
            Path("/work/rom"),
            Path("/work/rom/gngeo_data.zip"),
            "smbneo",
            scale=2,
        )

        self.assertEqual(command.count("--no-autoframeskip"), 1)
        self.assertEqual(command.count("--68kclock=0"), 1)
        self.assertNotIn("--autoframeskip", command)
        self.assertIn("--no-vsync", command)
        self.assertIn("--no-sleepidle", command)
        self.assertIn("-D", command)
        self.assertIn("--no-sound", command)
        self.assertEqual(command[-1], "smbneo")

    def test_xvfb_command_requests_an_isolated_dynamic_display(self) -> None:
        command = cadence.build_xvfb_command("Xvfb")

        self.assertEqual(command[:3], ["Xvfb", "-displayfd", "1"])
        self.assertIn("-nolisten", command)
        self.assertIn("tcp", command)
        self.assertNotIn(":94", command)

    def test_motion_helper_is_a_separate_owned_child_command(self) -> None:
        command = cadence.build_motion_command(
            "/usr/bin/python3",
            Path("/work/tools/measure_neogeo_cadence.py"),
            ":101",
            "/usr/bin/xdotool",
        )

        self.assertEqual(command[2], "--_motion-child")
        self.assertEqual(command[-4:], [
            "--display",
            ":101",
            "--xdotool",
            "/usr/bin/xdotool",
        ])


class CounterParsingTests(unittest.TestCase):
    def test_parse_and_analyze_handles_32_bit_wrap(self) -> None:
        output = """\
Remote debugging using 127.0.0.1:2159
CADENCE_COUNTER phase=baseline vblank=4294967280 game=100
Breakpoint 1
CADENCE_COUNTER phase=warmup vblank=14 game=115
CADENCE_COUNTER phase=sample_1 vblank=74 game=175
CADENCE_COUNTER phase=sample_2 vblank=194 game=295
"""
        snapshots = cadence.parse_counter_snapshots(output)
        intervals = cadence.analyze_counter_snapshots(
            snapshots,
            warmup_vblanks=30,
            sample_vblanks=[60, 120],
        )

        self.assertEqual(
            intervals,
            [
                cadence.CadenceInterval(
                    phase="warmup",
                    expected_vblanks=30,
                    display_vblanks=30,
                    game_frames=15,
                    missed_frames=15,
                ),
                cadence.CadenceInterval(
                    phase="sample_1",
                    expected_vblanks=60,
                    display_vblanks=60,
                    game_frames=60,
                    missed_frames=0,
                ),
                cadence.CadenceInterval(
                    phase="sample_2",
                    expected_vblanks=120,
                    display_vblanks=120,
                    game_frames=120,
                    missed_frames=0,
                ),
            ],
        )
        cadence.assert_zero_missed_frames(intervals)

    def test_zero_missed_assertion_ignores_warmup_but_rejects_sample(
        self,
    ) -> None:
        intervals = [
            cadence.CadenceInterval("warmup", 30, 30, 10, 20),
            cadence.CadenceInterval("sample_1", 60, 60, 59, 1),
        ]

        with self.assertRaisesRegex(
            cadence.CadenceAssertionError,
            "sample_1=1",
        ):
            cadence.assert_zero_missed_frames(intervals)

    def test_duplicate_checkpoint_is_rejected(self) -> None:
        output = """\
CADENCE_COUNTER phase=baseline vblank=1 game=1
CADENCE_COUNTER phase=baseline vblank=2 game=2
"""
        with self.assertRaisesRegex(cadence.CadenceError, "duplicate"):
            cadence.parse_counter_snapshots(output)

    def test_unexpected_breakpoint_distance_is_rejected(self) -> None:
        snapshots = cadence.parse_counter_snapshots(
            """\
CADENCE_COUNTER phase=baseline vblank=100 game=100
CADENCE_COUNTER phase=sample_1 vblank=159 game=159
"""
        )

        with self.assertRaisesRegex(
            cadence.CadenceError,
            "expected 60",
        ):
            cadence.analyze_counter_snapshots(
                snapshots,
                warmup_vblanks=0,
                sample_vblanks=[60],
            )


class SafetyGuardTests(unittest.TestCase):
    @mock.patch.object(cadence, "_can_bind_debug_port", return_value=False)
    def test_occupied_debug_port_is_rejected_before_launch(
        self,
        can_bind: mock.Mock,
    ) -> None:
        with self.assertRaisesRegex(
            cadence.CadenceError,
            "refusing to disturb the existing listener",
        ):
            cadence.reject_occupied_debug_port()
        can_bind.assert_called_once_with(
            cadence.DEBUG_HOST,
            cadence.DEBUG_PORT,
        )

    def test_zero_missed_assertion_long_alias_is_supported(self) -> None:
        arguments = cadence.build_argument_parser().parse_args(
            ["--assert-zero-missed-frames"]
        )

        self.assertTrue(arguments.assert_zero_missed)

    @mock.patch.object(cadence, "_process_group_socket_inodes")
    @mock.patch.object(cadence, "_listening_socket_inodes")
    @mock.patch.object(cadence, "_can_bind_debug_port", return_value=False)
    def test_debug_listener_must_belong_to_owned_process_group(
        self,
        _can_bind: mock.Mock,
        listening: mock.Mock,
        owned: mock.Mock,
    ) -> None:
        listening.return_value = {"481516"}
        owned.return_value = {"481516"}
        gngeo = mock.Mock(pid=108)
        gngeo.poll.return_value = None

        cadence._wait_for_debug_listener(gngeo, 1.0)

        owned.assert_called_once_with(108)

    @mock.patch.object(cadence, "_process_group_socket_inodes")
    @mock.patch.object(cadence, "_listening_socket_inodes")
    @mock.patch.object(cadence, "_can_bind_debug_port", return_value=False)
    def test_debug_listener_race_is_rejected_when_not_owned(
        self,
        _can_bind: mock.Mock,
        listening: mock.Mock,
        owned: mock.Mock,
    ) -> None:
        listening.return_value = {"481516"}
        owned.return_value = {"2342"}
        gngeo = mock.Mock(pid=108)
        gngeo.poll.return_value = None

        with self.assertRaisesRegex(
            cadence.CadenceError,
            "outside the owned GnGeo process group",
        ):
            cadence._wait_for_debug_listener(gngeo, 1.0)

    def test_default_sampling_timeout_is_finite_and_count_sensitive(
        self,
    ) -> None:
        self.assertEqual(
            cadence.derive_sampling_timeout(120, [120]),
            150.0,
        )
        self.assertEqual(
            cadence.derive_sampling_timeout(0, [1]),
            60.0,
        )

    @mock.patch.object(cadence.os, "killpg")
    @mock.patch.object(cadence.time, "sleep")
    @mock.patch.object(
        cadence.time,
        "monotonic",
        side_effect=[0.0, 0.0, 1.1],
    )
    def test_sampling_timeout_still_allows_owned_process_cleanup(
        self,
        _monotonic: mock.Mock,
        _sleep: mock.Mock,
        killpg: mock.Mock,
    ) -> None:
        process = mock.Mock(pid=321)
        process.poll.return_value = None
        process.wait.return_value = 0
        owned = cadence.OwnedProcessGroups()
        owned.add("GDB", process)

        try:
            with self.assertRaisesRegex(
                cadence.CadenceError,
                "exceeded its 1-second timeout",
            ):
                cadence._wait_with_heartbeats(
                    process,
                    "GDB cadence sample",
                    heartbeat_seconds=5.0,
                    timeout_seconds=1.0,
                )
        finally:
            owned.terminate_all()

        killpg.assert_called_once_with(321, signal.SIGTERM)

    def test_artifact_hash_is_content_addressed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            artifact = Path(temporary) / "artifact.bin"
            artifact.write_bytes(b"abc")

            self.assertEqual(
                cadence._sha256_file(artifact),
                (
                    "ba7816bf8f01cfea414140de5dae2223"
                    "b00361a396177a9cb410ff61f20015ad"
                ),
            )

    def test_explicit_sampling_timeout_argument_is_supported(self) -> None:
        arguments = cadence.build_argument_parser().parse_args(
            ["--sampling-timeout", "12.5"]
        )

        self.assertEqual(arguments.sampling_timeout, 12.5)


if __name__ == "__main__":
    unittest.main()
