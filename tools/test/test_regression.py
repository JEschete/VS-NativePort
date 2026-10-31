"""Synthetic controls for the behavior-loss check; no build, disc data or live baseline."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import regression  # noqa: E402

REPORT = """<?xml version="1.0" encoding="UTF-8"?>
<Catch2TestRun name="VS.Tests.exe">
  <TestCase name="alpha" tags="[a]"><OverallResult success="true" skips="0"/></TestCase>
  <TestCase name="beta" tags="[b]"><OverallResult success="{beta}" skips="{skips}"/></TestCase>
  <OverallResults successes="{assertions}" failures="0" expectedFailures="0" skips="0"/>
</Catch2TestRun>
"""


def report(beta: str = "true", skips: str = "0", assertions: str = "100") -> str:
    return REPORT.format(beta=beta, skips=skips, assertions=assertions)


def frame(tick: int, hp: int = 50) -> str:
    return json.dumps({
        "sequence": tick * 3, "tick": tick, "elapsed_us": tick * 7919 + 13, "category": "frame",
        "data": {"actors": [{"slot": 0, "hp": hp}]},
    })


def trace(frames: int, change_at: int | None = None) -> str:
    lines = [json.dumps({"sequence": 0, "tick": 0, "elapsed_us": 5, "category": "session.begin", "data": {}})]
    lines += [frame(tick, 49 if change_at is not None and tick >= change_at else 50) for tick in range(frames)]
    return "\n".join(lines) + "\n"


class ReportTests(unittest.TestCase):
    def test_parse_reads_every_case_and_the_run_total(self) -> None:
        result = regression.parse_report(report())
        self.assertEqual(result.passed, {"alpha", "beta"})
        self.assertEqual(result.failed, set())
        self.assertEqual(result.assertions, 100)

    def test_parse_separates_failed_and_skipped_cases(self) -> None:
        self.assertEqual(regression.parse_report(report(beta="false")).failed, {"beta"})
        self.assertEqual(regression.parse_report(report(skips="1")).skipped, {"beta"})


class CompareTests(unittest.TestCase):
    baseline = {"cases": ["alpha", "beta"], "assertions": 100}

    def rules(self, text: str) -> set[str]:
        return {problem.rule for problem in regression.compare_tests(self.baseline, regression.parse_report(text))}

    def test_an_equal_run_is_clean(self) -> None:
        self.assertEqual(self.rules(report()), set())

    def test_a_new_test_and_more_assertions_are_clean(self) -> None:
        text = report(assertions="150").replace(
            "<OverallResults", '<TestCase name="gamma"><OverallResult success="true" skips="0"/></TestCase>'
            "<OverallResults", 1)
        self.assertEqual(self.rules(text), set())

    def test_a_removed_test_is_lost(self) -> None:
        text = report().replace('name="beta"', 'name="renamed"')
        self.assertIn("test-lost", self.rules(text))

    def test_a_failed_test_is_reported(self) -> None:
        self.assertIn("test-failed", self.rules(report(beta="false")))

    def test_a_skipped_test_does_not_count_as_passing(self) -> None:
        self.assertIn("test-skipped", self.rules(report(skips="1")))

    def test_fewer_assertions_are_reported(self) -> None:
        self.assertIn("assertions-reduced", self.rules(report(assertions="99")))


class StaticTests(unittest.TestCase):
    def test_a_test_missing_from_source_is_lost(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            (tests / "A.cpp").write_text('TEST_CASE("alpha", "[a]") {}\nTEST_CASE("be" "ta") {}\n', encoding="utf-8")
            self.assertEqual(regression.compare_source({"cases": ["alpha", "beta"]}, tests), [])
            lost = regression.compare_source({"cases": ["alpha", "gone"]}, tests)
            self.assertEqual([problem.subject for problem in lost], ["gone"])

    def test_a_template_case_matches_by_its_declared_name(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            (tests / "A.cpp").write_text('TEMPLATE_TEST_CASE("sized", "[a]", int) {}\n', encoding="utf-8")
            self.assertEqual(regression.compare_source({"cases": ["sized - int"]}, tests), [])


class CoverageLossTests(unittest.TestCase):
    @staticmethod
    def row(identifier: str, tests: str, replays: str = "no") -> dict[str, str]:
        return {"id": identifier, "tests_execute": tests, "replay_execute": replays}

    def test_code_no_longer_executed_is_reported(self) -> None:
        losses = regression.coverage_losses([self.row("F1", "yes")], [self.row("F1", "no")])
        self.assertEqual([(loss.rule, loss.subject) for loss in losses], [("coverage-lost", "F1")])

    def test_code_still_reached_by_the_other_source_is_clean(self) -> None:
        self.assertEqual(regression.coverage_losses([self.row("F1", "yes")], [self.row("F1", "no", "yes")]), [])

    def test_gaining_or_first_measuring_coverage_is_clean(self) -> None:
        self.assertEqual(regression.coverage_losses([self.row("F1", "no")], [self.row("F1", "yes")]), [])
        self.assertEqual(regression.coverage_losses([self.row("F1", "", "")], [self.row("F1", "no")]), [])

    def test_a_removed_row_is_not_a_coverage_loss(self) -> None:
        self.assertEqual(regression.coverage_losses([self.row("F1", "yes")], []), [])


class ReplayTests(unittest.TestCase):
    def test_fingerprint_ignores_wall_clock_and_sequence_fields(self) -> None:
        other = trace(250).replace('"elapsed_us": 7932', '"elapsed_us": 1').replace('"sequence": 3,', '"sequence": 9,')
        self.assertNotEqual(trace(250), other)
        self.assertEqual(regression.fingerprint(trace(250)), regression.fingerprint(other))

    def test_fingerprint_has_one_hash_per_window_including_the_tail(self) -> None:
        result = regression.fingerprint(trace(250))
        self.assertEqual(result["frames"], 250)
        self.assertEqual(len(result["windows"]), 3)

    def test_fingerprint_ignores_non_frame_records(self) -> None:
        extra = trace(120) + json.dumps({"tick": 5, "category": "engine", "elapsed_us": 1, "data": {"x": 1}}) + "\n"
        self.assertEqual(regression.fingerprint(trace(120)), regression.fingerprint(extra))

    def test_a_state_change_names_its_first_window(self) -> None:
        baseline = regression.fingerprint(trace(350))
        changed = regression.fingerprint(trace(350, change_at=215))
        problems = regression.compare_replay("wolves", baseline, changed)
        self.assertEqual([problem.rule for problem in problems], ["replay-diverged"])
        self.assertIn("200", problems[0].message)

    def test_a_shorter_run_is_reported(self) -> None:
        problems = regression.compare_replay("wolves", regression.fingerprint(trace(350)), regression.fingerprint(trace(200)))
        self.assertIn("replay-frames", {problem.rule for problem in problems})

    def test_an_identical_run_is_clean(self) -> None:
        fingerprint = regression.fingerprint(trace(350))
        self.assertEqual(regression.compare_replay("wolves", fingerprint, fingerprint), [])

    @staticmethod
    def event_trace(buffer: int, position: int) -> str:
        scene = {"event": {"script_buffer": buffer, "threads": [{"pc": buffer + position}, {"pc": 0}]}}
        return json.dumps({"tick": 1, "category": "frame", "data": {"scene": scene}}) + "\n"

    def test_a_build_that_moves_memory_keeps_its_fingerprint(self) -> None:
        # Debug and Release differ only here, and any code change would move these too.
        self.assertEqual(regression.fingerprint(self.event_trace(1000, 16)),
                         regression.fingerprint(self.event_trace(9000, 16)))

    def test_a_script_at_another_position_changes_its_fingerprint(self) -> None:
        self.assertNotEqual(regression.fingerprint(self.event_trace(1000, 16)),
                            regression.fingerprint(self.event_trace(1000, 24)))

    def test_configurations_that_disagree_are_reported(self) -> None:
        same = regression.fingerprint(trace(120))
        other = regression.fingerprint(trace(120, change_at=50))
        self.assertEqual(regression.compare_configurations({"Debug": {"w": same}, "Release": {"w": same}}), [])
        problems = regression.compare_configurations({"Debug": {"w": same}, "Release": {"w": other}})
        self.assertEqual([problem.rule for problem in problems], ["replay-configurations-differ"])

    def test_a_configuration_not_yet_recorded_is_not_a_difference(self) -> None:
        same = regression.fingerprint(trace(120))
        self.assertEqual(regression.compare_configurations({"Debug": {"w": same}}), [])

    def test_a_truncated_last_line_is_ignored(self) -> None:
        text = trace(120) + '{"tick": 120, "category": "fra'
        self.assertEqual(regression.fingerprint(text), regression.fingerprint(trace(120)))


if __name__ == "__main__":
    unittest.main()
