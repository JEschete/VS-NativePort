"""Asset-independent contracts for diagnostic-only VM and flag hooks."""

import unittest

import copy_fixups


class SceneTraceFixupTests(unittest.TestCase):
    def test_instruction_is_captured_before_dispatch_and_result_before_control_flow(
        self,
    ):
        fixups = copy_fixups.FIXES["F01271"]
        capture = next(new for old, new, _ in fixups if "while (1)" in old)
        result = next(new for old, new, _ in fixups if old == "        switch (ret) {")
        self.assertLess(
            capture.index("Diagnostics_RecordSceneCommand"),
            capture.index("if (Event_skipState"),
        )
        self.assertIn("Event_kOpcodeSizes[*script]", capture)
        self.assertIn("Event_scriptBuffer", capture)
        self.assertLess(
            result.index("Diagnostics_RecordSceneResult"), result.index("switch (ret)")
        )
        self.assertIn("(int64_t)ret", result)

    def test_flag_observation_follows_the_original_write(self):
        old, new, _ = copy_fixups.FIXES["F01259"][0]
        self.assertEqual(old, "    flags[id] = value;")
        self.assertLess(new.index("previous_flag ="), new.index("flags[id] = value;"))
        self.assertLess(
            new.index("flags[id] = value;"),
            new.index("Diagnostics_RecordScriptFlagWrite"),
        )
        self.assertNotIn("State_SetScriptFlag", new)

    def test_skip_reason_is_recorded_before_forced_skip_is_cleared(self):
        fixups = copy_fixups.FIXES["F01271"]
        record = next(
            new for old, new, _ in fixups if old == "        Event_isSkipForced = 0;"
        )
        self.assertIn("start=%u forced=%d", record)
        self.assertLess(
            record.index("Core_Debugf"), record.index("Event_isSkipForced = 0;")
        )

    def test_helpers_have_one_c_compatible_owned_diagnostic_header(self):
        for name in (
            "Diagnostics_RecordSceneCommand",
            "Diagnostics_RecordSceneResult",
            "Diagnostics_RecordScriptFlagWrite",
        ):
            self.assertEqual(
                copy_fixups.NATIVE_HELPER_HEADERS[name],
                r"Native\Game\include\Diagnostics\SceneTrace.h",
            )


if __name__ == "__main__":
    unittest.main()
