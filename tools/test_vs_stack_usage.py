import unittest

from check_vs_stack_usage import StackUsageError, parse_stack_usage


class StackUsageTests(unittest.TestCase):
    def test_accepts_gcc_static_and_bounded_rows(self):
        records = parse_stack_usage(
            "build/vs_program.c:10:2:vs_fn_player\t92\tstatic\n"
            "build/vs_program.c:20:2:vs_fn_actor\t96\tdynamic,bounded\n"
        )
        self.assertEqual([record.function for record in records],
                         ["vs_fn_player", "vs_fn_actor"])
        self.assertEqual(max(record.bytes for record in records), 96)

    def test_rejects_malformed_and_unknown_rows(self):
        for report in (
            "not a stack row\n",
            "file.c:1:1:f\t12\tdynamic\n",
            "file.c:1:1:f\t12\tdynamic,unbounded\n",
            "file.c:1:f\t12\tstatic\n",
            "file.c:1:1:f\tnope\tstatic\n",
        ):
            with self.subTest(report=report), self.assertRaises(StackUsageError):
                parse_stack_usage(report)

    def test_rejects_unknown_bound_and_frame_over_limit(self):
        with self.assertRaisesRegex(StackUsageError, "unsupported"):
            parse_stack_usage("file.c:1:1:f\t12\tunknown\n")
        with self.assertRaisesRegex(StackUsageError, "uses 97 bytes"):
            parse_stack_usage("file.c:1:1:f\t97\tstatic\n")
        with self.assertRaisesRegex(StackUsageError, "max-frame"):
            parse_stack_usage("file.c:1:1:f\t0\tstatic\n", -1)

    def test_rejects_empty_report(self):
        with self.assertRaisesRegex(StackUsageError, "empty"):
            parse_stack_usage("\n")


if __name__ == "__main__":
    unittest.main()
