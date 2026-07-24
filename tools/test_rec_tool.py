#!/usr/bin/env python3

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import hashlib
from io import StringIO
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rec_tool as rec  # noqa: E402


REPO_ROOT = Path(__file__).resolve().parents[1]
WARPLESS_RECORDING = REPO_ROOT / "rec" / "warpless.rec"
WARPLESS_SHA256 = (
    "11ecba771340876fe8e886c60913e29f652abc0d879b281aafed4ae96144b17a"
)


def recording(text: str) -> rec.Recording:
    return rec.parse_recording(text.encode("ascii"))


def fm2_bytes(records: list[str]) -> bytes:
    headers = [
        "version 3",
        "emuVersion 22020",
        "rerecordCount 0",
        "palFlag 0",
        "romFilename test.nes",
        f"romChecksum {rec.SUPPORTED_FM2_ROM_CHECKSUM}",
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
    return ("\n".join(headers + records) + "\n").encode("utf-8")


class RecordingValidationTests(unittest.TestCase):
    def test_current_recording_exact_metadata_and_direction_policies(self) -> None:
        loaded = rec.load_recording(WARPLESS_RECORDING)

        self.assertEqual(len(loaded.transitions), 509)
        self.assertEqual(loaded.end_frame, 7987)
        self.assertEqual(loaded.sha256, WARPLESS_SHA256)
        self.assertGreater(rec.count_opposite_direction_transitions(loaded), 0)

        self.assertIs(
            rec.validate_recording(
                loaded,
                expected_end_frame=7987,
                expected_sha256=WARPLESS_SHA256.upper(),
                expected_transition_count=509,
            ),
            loaded,
        )
        with self.assertRaisesRegex(rec.RecordingError, "not hardware-playable"):
            rec.validate_recording(loaded, hardware_playable=True)

    def test_trailing_comments_and_blank_lines_are_accepted(self) -> None:
        loaded = recording("0:1\n\n4:0\n-- provenance\n-- more context")
        self.assertEqual(
            [(item.frame, item.state) for item in loaded.transitions],
            [(0, 1), (4, 0)],
        )
        rec.validate_recording(loaded)

    def test_transition_after_comment_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            rec.RecordingError, "transition follows a trailing comment"
        ):
            recording("0:1\n-- comment\n2:0\n")

    def test_malformed_records_are_rejected(self) -> None:
        malformed = (
            "1 :2\n2:0\n",
            "1:2 extra\n2:0\n",
            "-1:2\n2:0\n",
            "0x1:2\n2:0\n",
            " \n",
        )
        for content in malformed:
            with self.subTest(content=content):
                with self.assertRaisesRegex(
                    rec.RecordingError, "expected an exact decimal"
                ):
                    recording(content)

    def test_empty_and_non_ascii_recordings_are_rejected(self) -> None:
        with self.assertRaisesRegex(rec.RecordingError, "no transitions"):
            rec.parse_recording(b"-- only a comment")
        with self.assertRaisesRegex(rec.RecordingError, "not ASCII"):
            rec.parse_recording(b"0:1\n\xff")

    def test_frame_and_state_storage_bounds_are_enforced(self) -> None:
        with self.assertRaisesRegex(rec.RecordingError, "exceeds uint32_t"):
            recording(f"{rec.UINT32_MAX + 1}:0\n")
        with self.assertRaisesRegex(rec.RecordingError, "exceeds uint8_t"):
            recording("0:256\n1:0\n")

    def test_frames_must_be_strictly_increasing(self) -> None:
        invalid = (
            "1:1\n1:0\n",
            "2:1\n1:0\n",
        )
        for content in invalid:
            with self.subTest(content=content):
                with self.assertRaisesRegex(
                    rec.RecordingError, "not strictly greater"
                ):
                    rec.validate_recording(recording(content))

    def test_adjacent_duplicate_states_are_rejected(self) -> None:
        with self.assertRaisesRegex(
            rec.RecordingError, "adjacent transitions repeat state"
        ):
            rec.validate_recording(recording("0:1\n2:1\n3:0\n"))

    def test_final_duplicate_zero_is_allowed_as_exclusive_end_sentinel(
        self,
    ) -> None:
        loaded = recording("0:1\n1:0\n3:0\n")
        rec.validate_recording(loaded)
        self.assertEqual(
            rec.build_segments(loaded),
            (
                rec.Segment(duration=1, state=1),
                rec.Segment(duration=2, state=0),
            ),
        )

    def test_opposite_directions_are_an_explicit_hardware_policy(self) -> None:
        cases = (
            0xC0,
            0x30,
            0xC1,
            0x32,
        )
        for state in cases:
            with self.subTest(state=state):
                loaded = recording(f"0:{state}\n1:0\n")
                rec.validate_recording(loaded)
                with self.assertRaisesRegex(
                    rec.RecordingError, "not hardware-playable"
                ):
                    rec.validate_recording(
                        loaded, hardware_playable=True
                    )

    def test_final_transition_must_be_zero_release_sentinel(self) -> None:
        with self.assertRaisesRegex(
            rec.RecordingError, "zero-state release sentinel"
        ):
            rec.validate_recording(recording("0:1\n2:2\n"))

    def test_expected_metadata_mismatches_are_rejected(self) -> None:
        loaded = recording("0:1\n2:0\n")
        checks = (
            (
                {"expected_end_frame": 3},
                "end frame mismatch",
            ),
            (
                {"expected_sha256": "0" * 64},
                "SHA-256 mismatch",
            ),
            (
                {"expected_transition_count": 3},
                "transition count mismatch",
            ),
        )
        for keywords, message in checks:
            with self.subTest(keywords=keywords):
                with self.assertRaisesRegex(rec.RecordingError, message):
                    rec.validate_recording(loaded, **keywords)

    def test_expected_sha256_shape_is_strict(self) -> None:
        loaded = recording("0:1\n2:0\n")
        with self.assertRaisesRegex(rec.RecordingError, "64 hex digits"):
            rec.validate_recording(loaded, expected_sha256="abcd")


class RecordingEmissionTests(unittest.TestCase):
    def test_segments_cover_every_frame_and_merge_initial_zero_state(self) -> None:
        loaded = recording("1:0\n4:8\n5:0\n")
        segments = rec.build_segments(loaded)

        self.assertEqual(
            segments,
            (
                rec.Segment(duration=4, state=0),
                rec.Segment(duration=1, state=8),
            ),
        )
        self.assertEqual(
            sum(segment.duration for segment in segments),
            loaded.end_frame,
        )

    def test_uint16_duration_limit_is_enforced(self) -> None:
        exact_limit = recording(f"{rec.UINT16_MAX}:1\n65536:0\n")
        self.assertEqual(
            rec.build_segments(exact_limit)[0],
            rec.Segment(duration=rec.UINT16_MAX, state=0),
        )

        too_long = recording("65536:1\n65537:0\n")
        with self.assertRaisesRegex(rec.RecordingError, "exceeds uint16_t"):
            rec.build_segments(too_long)

    def test_header_uses_parallel_compact_arrays_without_struct_padding(self) -> None:
        loaded = recording("0:1\n2:2\n5:0\n")
        header = rec.render_c_header(
            loaded,
            symbol_prefix="demo_replay",
            hardware_playable=True,
        )

        self.assertIn("#define DEMO_REPLAY_TRANSITION_COUNT 3u", header)
        self.assertIn("#define DEMO_REPLAY_SEGMENT_COUNT 2u", header)
        self.assertIn("#define DEMO_REPLAY_END_FRAME 5u", header)
        self.assertIn("#define DEMO_REPLAY_HARDWARE_PLAYABLE 1u", header)
        self.assertIn(
            "static const uint16_t demo_replay_durations"
            "[DEMO_REPLAY_SEGMENT_COUNT]",
            header,
        )
        self.assertIn(
            "static const uint8_t demo_replay_states"
            "[DEMO_REPLAY_SEGMENT_COUNT]",
            header,
        )
        self.assertIn("    2, 3,", header)
        self.assertIn("    0x01, 0x02,", header)
        self.assertNotIn("struct", header)

    def test_header_labels_recording_compatible_opposites(self) -> None:
        loaded = recording("0:192\n1:0\n")
        header = rec.render_c_header(loaded)

        self.assertIn(
            "Direction policy: recording-compatible (opposites permitted)",
            header,
        )
        self.assertIn("#define SMB_REPLAY_HARDWARE_PLAYABLE 0u", header)
        self.assertIn(
            "#define SMB_REPLAY_OPPOSITE_DIRECTION_TRANSITIONS 1u",
            header,
        )

    def test_invalid_symbol_prefix_is_rejected(self) -> None:
        with self.assertRaisesRegex(rec.RecordingError, "C identifier"):
            rec.render_c_header(
                recording("0:1\n1:0\n"),
                symbol_prefix="not-valid",
            )


class Fm2ImportTests(unittest.TestCase):
    def test_controller_positions_map_to_repo_replay_bits(self) -> None:
        for position, (button, bit) in enumerate(
            zip(rec.FM2_BUTTON_ORDER, rec.FM2_BUTTON_BITS)
        ):
            field = ["."] * 8
            field[position] = button
            data = fm2_bytes([f"|0|{''.join(field)}|||"])
            with self.subTest(button=button):
                movie = rec.parse_fm2(data)
                self.assertEqual(movie.frame_states, (bit,))

    def test_neutral_and_repeated_lag_frame_records_preserve_time(self) -> None:
        data = fm2_bytes(
            [
                "|0|........|||",
                "|0|........|||",
                "|0|.......A|||",
                "|0|.......A|||",
                "|0|........|||",
                "|0|........|||",
            ]
        )
        movie = rec.parse_fm2(data, "test.fm2")
        content, imported = rec.import_fm2_recording(movie)

        self.assertEqual(movie.frame_count, 6)
        self.assertEqual(
            content.splitlines()[:3],
            ["2:1", "4:0", "6:0"],
        )
        self.assertIn(
            f"-- fm2_source_sha256:{hashlib.sha256(data).hexdigest()}",
            content,
        )
        self.assertIn("-- fm2_frame_count:6", content)
        self.assertIn("-- fm2_ram_init_option:2", content)
        self.assertIn("-- fm2_ram_init_seed:0", content)
        self.assertEqual(imported.end_frame, 6)
        self.assertEqual(
            rec.build_segments(imported),
            (
                rec.Segment(duration=2, state=0),
                rec.Segment(duration=2, state=rec.INPUT_A),
                rec.Segment(duration=2, state=0),
            ),
        )

    def test_all_neutral_movie_uses_only_exclusive_end_sentinel(self) -> None:
        movie = rec.parse_fm2(
            fm2_bytes(
                [
                    "|0|........|||",
                    "|0|........|||",
                    "|0|........|||",
                ]
            )
        )
        content, imported = rec.import_fm2_recording(movie)

        self.assertEqual(content.splitlines()[0], "3:0")
        self.assertEqual(
            rec.build_segments(imported),
            (rec.Segment(duration=3, state=0),),
        )

    def test_all_buttons_map_to_full_byte(self) -> None:
        movie = rec.parse_fm2(fm2_bytes(["|0|RLDUTSBA|||"]))
        self.assertEqual(movie.frame_states, (0xFF,))
        content, _ = rec.import_fm2_recording(movie)
        self.assertEqual(content.splitlines()[:2], ["0:255", "1:0"])

    def test_hardware_policy_rejects_opposites_after_exact_import(self) -> None:
        movie = rec.parse_fm2(fm2_bytes(["|0|RL......|||"]))
        rec.import_fm2_recording(movie)
        with self.assertRaisesRegex(
            rec.RecordingError, "not hardware-playable"
        ):
            rec.import_fm2_recording(movie, hardware_playable=True)

    def test_initial_reset_command_is_declared_fresh_start_equivalence(
        self,
    ) -> None:
        movie = rec.parse_fm2(
            fm2_bytes(
                [
                    "|1|........|||",
                    "|0|.......A|||",
                ]
            )
        )
        content, imported = rec.import_fm2_recording(movie)

        self.assertEqual(movie.initial_command, 1)
        self.assertEqual(imported.end_frame, 2)
        self.assertIn("-- fm2_initial_command:1", content)
        self.assertIn(
            "-- fm2_initial_command_semantics:"
            "fresh_core_reset_equivalent",
            content,
        )

        with self.assertRaisesRegex(
            rec.RecordingError, "supported only on the first"
        ):
            rec.parse_fm2(
                fm2_bytes(
                    [
                        "|0|........|||",
                        "|1|........|||",
                    ]
                )
            )

    def test_binary_fm2_is_rejected_before_binary_payload_decode(self) -> None:
        data = fm2_bytes(["|0|........|||"]).replace(
            b"binary 0", b"binary 1"
        )
        with self.assertRaisesRegex(rec.RecordingError, "binary FM2"):
            rec.parse_fm2(data + b"\xff\x00")

    def test_unsupported_movie_configurations_are_rejected(self) -> None:
        base = fm2_bytes(["|0|........|||"])
        cases = (
            (b"version 3", b"version 2", "unsupported FM2 version"),
            (b"fourscore 0", b"fourscore 1", "multiple-controller"),
            (b"port0 1", b"port0 2", "controller-1 type"),
            (b"port1 0", b"port1 2", "controller-2 type"),
            (b"port2 0", b"port2 1", "expansion-port"),
            (b"microphone 0", b"microphone 1", "microphone"),
            (b"FDS 0", b"FDS 1", "FDS movies"),
            (
                b"startsFromSavestate 0",
                b"startsFromSavestate 1",
                "starting from a savestate",
            ),
        )
        for old, new, message in cases:
            with self.subTest(new=new):
                with self.assertRaisesRegex(rec.RecordingError, message):
                    rec.parse_fm2(base.replace(old, new))

    def test_rom_checksum_is_required_and_exactly_case_sensitive(self) -> None:
        base = fm2_bytes(["|0|........|||"])
        checksum_header = (
            f"romChecksum {rec.SUPPORTED_FM2_ROM_CHECKSUM}\n"
        ).encode("ascii")

        with self.assertRaisesRegex(
            rec.RecordingError, "missing required FM2 header romChecksum"
        ):
            rec.parse_fm2(base.replace(checksum_header, b""))

        mismatches = (
            b"romChecksum base64:AAAAAAAAAAAAAAAAAAAAAA==\n",
            (
                f"romChecksum "
                f"{rec.SUPPORTED_FM2_ROM_CHECKSUM.replace('base64:', 'BASE64:')}"
                f"\n"
            ).encode("ascii"),
        )
        for replacement in mismatches:
            with self.subTest(replacement=replacement):
                with self.assertRaisesRegex(
                    rec.RecordingError, "case-sensitive"
                ):
                    rec.parse_fm2(
                        base.replace(checksum_header, replacement)
                    )

    def test_pal_movies_are_rejected(self) -> None:
        data = fm2_bytes(["|0|........|||"]).replace(
            b"palFlag 0", b"palFlag 1"
        )
        with self.assertRaisesRegex(
            rec.RecordingError, "palFlag must be 0 for NTSC"
        ):
            rec.parse_fm2(data)

    def test_new_ppu_movies_are_rejected(self) -> None:
        data = fm2_bytes(["|0|........|||"]).replace(
            b"NewPPU 0", b"NewPPU 1"
        )
        with self.assertRaisesRegex(
            rec.RecordingError, "NewPPU must be 0"
        ):
            rec.parse_fm2(data)

    def test_missing_ram_option_uses_fceux_221_legacy_default(self) -> None:
        base = fm2_bytes(["|0|........|||"])
        option_header = b"RAMInitOption 2\n"

        legacy = rec.parse_fm2(base.replace(option_header, b""))
        self.assertEqual(legacy.ram_init_option, 0)

        explicit_legacy = rec.parse_fm2(
            base.replace(option_header, b"RAMInitOption 0\n")
        )
        self.assertEqual(explicit_legacy.ram_init_option, 0)

        for option in (1, 3, 4):
            with self.subTest(option=option):
                with self.assertRaisesRegex(
                    rec.RecordingError,
                    "supported deterministic modes are 0.*and 2",
                ):
                    rec.parse_fm2(
                        base.replace(
                            option_header,
                            f"RAMInitOption {option}\n".encode("ascii"),
                        )
                    )

    def test_fill_zero_mode_accepts_any_valid_seed_or_no_seed(self) -> None:
        base = fm2_bytes(["|0|........|||"])
        seed_header = b"RAMInitSeed 0\n"

        for seed in (b"-2147483648", b"0", b"2147483647"):
            with self.subTest(seed=seed):
                movie = rec.parse_fm2(
                    base.replace(
                        seed_header,
                        b"RAMInitSeed " + seed + b"\n",
                    )
                )
                self.assertEqual(movie.frame_count, 1)
                self.assertEqual(movie.ram_init_option, 2)
                self.assertEqual(movie.ram_init_seed, int(seed))

        movie = rec.parse_fm2(base.replace(seed_header, b""))
        self.assertEqual(movie.frame_count, 1)
        self.assertEqual(movie.ram_init_seed, 0)

    def test_ram_initialization_seed_must_be_signed_int32(self) -> None:
        base = fm2_bytes(["|0|........|||"])
        seed_header = b"RAMInitSeed 0\n"
        cases = (
            (b"not-a-number", "signed 32-bit decimal integer"),
            (b"-2147483649", "must fit in a signed 32-bit integer"),
            (b"2147483648", "must fit in a signed 32-bit integer"),
        )

        for seed, message in cases:
            with self.subTest(seed=seed):
                with self.assertRaisesRegex(rec.RecordingError, message):
                    rec.parse_fm2(
                        base.replace(
                            seed_header,
                            b"RAMInitSeed " + seed + b"\n",
                        )
                    )

    def test_header_names_are_case_insensitive_but_values_are_not(
        self,
    ) -> None:
        data = (
            fm2_bytes(["|0|........|||"])
            .replace(b"palFlag 0", b"PALFLAG 0")
            .replace(b"romChecksum ", b"ROMCHECKSUM ")
            .replace(b"NewPPU 0", b"newppu 0")
            .replace(b"RAMInitOption 2", b"raminitoption 2")
            .replace(b"RAMInitSeed 0", b"raminitseed 0")
        )
        movie = rec.parse_fm2(data)
        self.assertEqual(movie.frame_count, 1)

    def test_iso_8859_1_metadata_preserves_ascii_input_and_raw_hash(
        self,
    ) -> None:
        data = (
            b"comment author Ren\xe9; Fran\xe7ais metadata\r\n"
            + fm2_bytes(
                [
                    "|1|.L.U..B.|||",
                    "|0|........|||",
                ]
            ).replace(b"\n", b"\r\n")
        )

        movie = rec.parse_fm2(data, "latin1.fm2")
        header = rec.render_fm2_c_header(movie)

        self.assertEqual(
            movie.frame_states,
            (
                rec.INPUT_LEFT | rec.INPUT_UP | rec.INPUT_B,
                0,
            ),
        )
        self.assertEqual(movie.initial_command, 1)
        self.assertEqual(
            movie.source_sha256, hashlib.sha256(data).hexdigest()
        )
        self.assertIn(
            f'#define SMB_REPLAY_FM2_SOURCE_SHA256 '
            f'"{hashlib.sha256(data).hexdigest()}"',
            header,
        )

    def test_non_ascii_bytes_in_controller_records_are_rejected(self) -> None:
        cases = (
            fm2_bytes(["|0|.......A|||"]).replace(
                b".......A", b".......\xe9"
            ),
            fm2_bytes(["|0|........|.......A||"])
            .replace(b"port1 0", b"port1 1")
            .replace(b".......A", b".......\xe9"),
            fm2_bytes(["|0|........||x|"]).replace(b"||x|", b"||\xe9|"),
            fm2_bytes(["|0|......BA|||"]).replace(
                b"......BA", b"......\xc3\xa9"
            ),
        )

        for data in cases:
            with self.subTest(input_record=data.splitlines()[-1]):
                with self.assertRaisesRegex(
                    rec.RecordingError,
                    "input record must contain only ASCII bytes",
                ):
                    rec.parse_fm2(data, "non-ascii.fm2")

    def test_embedded_state_is_rejected(self) -> None:
        data = fm2_bytes(["|0|........|||"]).replace(
            b"binary 0\n", b"savestate base64:AAAA\nbinary 0\n"
        )
        with self.assertRaisesRegex(
            rec.RecordingError, "embedded FM2 savestate"
        ):
            rec.parse_fm2(data)

    def test_fds_and_other_emulator_commands_are_rejected(self) -> None:
        cases = (
            ("|4|........|||", "FDS command"),
            ("|8|........|||", "FDS command"),
            ("|2|........|||", "emulator command"),
            ("|16|........|||", "emulator command"),
        )
        for input_record, message in cases:
            with self.subTest(input_record=input_record):
                with self.assertRaisesRegex(rec.RecordingError, message):
                    rec.parse_fm2(fm2_bytes([input_record]))

    def test_malformed_input_records_are_rejected(self) -> None:
        cases = (
            "|x|........|||",
            "|0|.......|||",
            "|0|A.......|||",
            "|0|........|........||",
            "|0|........||x|",
            "|0|........||",
        )
        for input_record in cases:
            with self.subTest(input_record=input_record):
                with self.assertRaises(rec.RecordingError):
                    rec.parse_fm2(fm2_bytes([input_record]))

    def test_required_headers_and_frame_records_are_enforced(self) -> None:
        valid = fm2_bytes(["|0|........|||"])
        without_port = valid.replace(b"port0 1\n", b"")
        with self.assertRaisesRegex(rec.RecordingError, "missing.*port0"):
            rec.parse_fm2(without_port)

        headers_only = fm2_bytes([]).decode("utf-8")
        with self.assertRaisesRegex(rec.RecordingError, "no frame input"):
            rec.parse_fm2(headers_only.encode("utf-8"))

        line_after_input = valid + b"comment too late\n"
        with self.assertRaisesRegex(
            rec.RecordingError, "non-input data appears"
        ):
            rec.parse_fm2(line_after_input)

    def test_disabled_ports_must_not_contain_input_fields(self) -> None:
        with self.assertRaisesRegex(
            rec.RecordingError, "controller-2 field must be empty"
        ):
            rec.parse_fm2(fm2_bytes(["|0|........|........||"]))
        with self.assertRaisesRegex(
            rec.RecordingError, "expansion-port input"
        ):
            rec.parse_fm2(fm2_bytes(["|0|........||x|"]))

    def test_configured_controller_two_is_allowed_only_while_neutral(
        self,
    ) -> None:
        neutral = fm2_bytes(
            [
                "|1|........|........||",
                "|0|.......A|........||",
            ]
        ).replace(b"port1 0", b"port1 1")
        movie = rec.parse_fm2(neutral)
        self.assertEqual(movie.frame_states, (0, rec.INPUT_A))

        controlled = neutral.replace(
            b"|0|.......A|........||",
            b"|0|.......A|.......A||",
        )
        with self.assertRaisesRegex(
            rec.RecordingError, "multiple controlled ports"
        ):
            rec.parse_fm2(controlled)


class Fm2EmissionTests(unittest.TestCase):
    def test_header_retains_recording_and_fm2_hashes_with_option_metadata(
        self,
    ) -> None:
        cases = (
            (0, -17, 1),
            (2, 23, 0),
        )
        for ram_option, ram_seed, initial_command in cases:
            with self.subTest(
                ram_option=ram_option,
                ram_seed=ram_seed,
                initial_command=initial_command,
            ):
                data = (
                    fm2_bytes(
                        [
                            f"|{initial_command}|.......A|||",
                            "|0|........|||",
                        ]
                    )
                    .replace(
                        b"RAMInitOption 2",
                        f"RAMInitOption {ram_option}".encode("ascii"),
                    )
                    .replace(
                        b"RAMInitSeed 0",
                        f"RAMInitSeed {ram_seed}".encode("ascii"),
                    )
                )
                movie = rec.parse_fm2(data, "movie.fm2")
                _, imported = rec.import_fm2_recording(movie)
                header = rec.render_fm2_c_header(movie)

                self.assertIn("#define SMB_REPLAY_TRANSITION_COUNT 3u", header)
                self.assertIn("#define SMB_REPLAY_SEGMENT_COUNT 2u", header)
                self.assertIn("#define SMB_REPLAY_END_FRAME 2u", header)
                self.assertIn(
                    "#define SMB_REPLAY_HARDWARE_PLAYABLE 0u", header
                )
                self.assertIn(
                    "#define SMB_REPLAY_OPPOSITE_DIRECTION_TRANSITIONS 0u",
                    header,
                )
                self.assertIn(
                    f'#define SMB_REPLAY_SOURCE_SHA256 "{imported.sha256}"',
                    header,
                )
                self.assertIn(
                    f'#define SMB_REPLAY_FM2_SOURCE_SHA256 '
                    f'"{hashlib.sha256(data).hexdigest()}"',
                    header,
                )
                self.assertIn(
                    "#define SMB_REPLAY_FM2_FRAME_COUNT 2u", header
                )
                self.assertIn(
                    f"#define SMB_REPLAY_FM2_INITIAL_COMMAND "
                    f"{initial_command}u",
                    header,
                )
                self.assertIn(
                    f"#define SMB_REPLAY_FM2_RAM_INIT_OPTION "
                    f"{ram_option}u",
                    header,
                )
                self.assertIn(
                    f"#define SMB_REPLAY_FM2_RAM_INIT_SEED {ram_seed}",
                    header,
                )
                self.assertIn(
                    "SOURCE_SHA256 hashes the canonical imported "
                    "frame:state recording bytes",
                    header,
                )
                self.assertIn(
                    "FM2_SOURCE_SHA256 hashes the original FM2 file bytes",
                    header,
                )
                self.assertNotEqual(imported.sha256, movie.source_sha256)

    def test_custom_symbol_prefix_applies_to_arrays_and_fm2_macros(
        self,
    ) -> None:
        movie = rec.parse_fm2(
            fm2_bytes(
                [
                    "|1|........|||",
                    "|0|.......A|||",
                ]
            )
        )
        header = rec.render_fm2_c_header(
            movie,
            symbol_prefix="full_run",
            hardware_playable=True,
        )

        self.assertIn("#define FULL_RUN_FM2_INITIAL_COMMAND 1u", header)
        self.assertIn("#define FULL_RUN_FM2_RAM_INIT_OPTION 2u", header)
        self.assertIn("static const uint16_t full_run_durations", header)
        self.assertIn("static const uint8_t full_run_states", header)

    def test_hardware_policy_rejects_fm2_opposite_directions(self) -> None:
        movie = rec.parse_fm2(fm2_bytes(["|0|RL......|||"]))

        rec.render_fm2_c_header(movie)
        with self.assertRaisesRegex(
            rec.RecordingError, "not hardware-playable"
        ):
            rec.render_fm2_c_header(movie, hardware_playable=True)


class RecordingCliTests(unittest.TestCase):
    def test_validate_reports_metadata_and_compatibility_policy(self) -> None:
        stdout = StringIO()
        with redirect_stdout(stdout):
            result = rec.main(
                [
                    "validate",
                    str(WARPLESS_RECORDING),
                    "--expect-end-frame",
                    "7987",
                    "--expect-sha256",
                    WARPLESS_SHA256,
                    "--expect-transition-count",
                    "509",
                ]
            )

        self.assertEqual(result, 0)
        output = stdout.getvalue()
        self.assertIn("transitions=509 end_frame=7987", output)
        self.assertIn(
            "direction_policy=recording-compatible (opposites permitted)",
            output,
        )

    def test_validate_hardware_policy_returns_failure(self) -> None:
        stderr = StringIO()
        with redirect_stderr(stderr):
            result = rec.main(
                [
                    "validate",
                    str(WARPLESS_RECORDING),
                    "--hardware-playable",
                ]
            )

        self.assertEqual(result, 1)
        self.assertIn("not hardware-playable", stderr.getvalue())

    def test_emit_c_stdout_contains_only_labeled_header(self) -> None:
        stdout = StringIO()
        with redirect_stdout(stdout):
            result = rec.main(
                ["emit-c", str(WARPLESS_RECORDING)]
            )

        self.assertEqual(result, 0)
        output = stdout.getvalue()
        self.assertTrue(output.startswith("/* Generated by"))
        self.assertIn("opposites permitted", output)
        self.assertNotIn(": wrote ", output)

    def test_import_fm2_reads_local_file_and_reports_source_metadata(
        self,
    ) -> None:
        data = fm2_bytes(
            [
                "|0|........|||",
                "|0|.......A|||",
                "|0|........|||",
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "movie.fm2"
            source.write_bytes(data)
            stdout = StringIO()
            with redirect_stdout(stdout):
                result = rec.main(["import-fm2", str(source)])

        self.assertEqual(result, 0)
        output = stdout.getvalue()
        self.assertEqual(output.splitlines()[:3], ["1:1", "2:0", "3:0"])
        self.assertIn(
            f"-- fm2_source_sha256:{hashlib.sha256(data).hexdigest()}",
            output,
        )
        self.assertIn("-- fm2_frame_count:3", output)
        self.assertIn("-- fm2_ram_init_option:2", output)
        self.assertIn("-- fm2_ram_init_seed:0", output)

    def test_emit_fm2_c_stdout_contains_header_and_reset_metadata(
        self,
    ) -> None:
        data = fm2_bytes(
            [
                "|1|........|||",
                "|0|.......A|||",
                "|0|........|||",
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "movie.fm2"
            source.write_bytes(data)
            stdout = StringIO()
            with redirect_stdout(stdout):
                result = rec.main(
                    [
                        "emit-fm2-c",
                        str(source),
                        "--symbol-prefix",
                        "test_run",
                    ]
                )

        self.assertEqual(result, 0)
        output = stdout.getvalue()
        self.assertTrue(output.startswith("/* Generated by"))
        self.assertIn(
            f'#define TEST_RUN_FM2_SOURCE_SHA256 '
            f'"{hashlib.sha256(data).hexdigest()}"',
            output,
        )
        self.assertIn("#define TEST_RUN_FM2_FRAME_COUNT 3u", output)
        self.assertIn("#define TEST_RUN_FM2_INITIAL_COMMAND 1u", output)
        self.assertIn("#define TEST_RUN_FM2_RAM_INIT_OPTION 2u", output)
        self.assertIn("#define TEST_RUN_FM2_RAM_INIT_SEED 0", output)
        self.assertNotIn(": wrote ", output)

    def test_emit_fm2_c_file_reports_both_source_hashes(self) -> None:
        data = fm2_bytes(
            [
                "|0|.......A|||",
                "|0|........|||",
            ]
        ).replace(b"RAMInitOption 2", b"RAMInitOption 0")
        movie = rec.parse_fm2(data)
        _, imported = rec.import_fm2_recording(movie)

        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "movie.fm2"
            output_path = Path(directory) / "movie_data.h"
            source.write_bytes(data)
            stdout = StringIO()
            with redirect_stdout(stdout):
                result = rec.main(
                    [
                        "emit-fm2-c",
                        str(source),
                        "-o",
                        str(output_path),
                    ]
                )
            header = output_path.read_text(encoding="ascii")

        self.assertEqual(result, 0)
        self.assertIn("#define SMB_REPLAY_FM2_RAM_INIT_OPTION 0u", header)
        self.assertIn("#define SMB_REPLAY_FM2_INITIAL_COMMAND 0u", header)
        self.assertIn(f"recording_sha256={imported.sha256}", stdout.getvalue())
        self.assertIn(
            f"fm2_source_sha256={hashlib.sha256(data).hexdigest()}",
            stdout.getvalue(),
        )

    def test_emit_fm2_c_hardware_policy_returns_failure(self) -> None:
        data = fm2_bytes(["|0|RL......|||"])
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "movie.fm2"
            source.write_bytes(data)
            stderr = StringIO()
            with redirect_stderr(stderr):
                result = rec.main(
                    [
                        "emit-fm2-c",
                        str(source),
                        "--hardware-playable",
                    ]
                )

        self.assertEqual(result, 1)
        self.assertIn("not hardware-playable", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
