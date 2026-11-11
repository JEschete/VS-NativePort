"""Unit tests for promotion_check.py's parsing, staging and copier-stability snapshot (#340).

  python -m unittest discover -s tools\\test -p test_promotion_check.py -v
"""

from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import promotion_check

REGRESSION_OUTPUT = """Debug tests: 4,321 passed, 0 failed, 2 skipped, 1,234,567 assertions
Debug replay silver_wolves: 2400 frames, matches baseline
Debug replay first_lever_room: 900 frames, differs
1 regressions
"""


class Parsing(unittest.TestCase):
    def test_regression_counts_and_replays(self) -> None:
        values = promotion_check.parse_regression(REGRESSION_OUTPUT)
        self.assertEqual(values["debug_cases"], 4323)
        self.assertEqual(values["debug_passed"], 4321)
        self.assertEqual(values["debug_failed"], 0)
        self.assertEqual(values["debug_assertions"], 1234567)
        self.assertEqual(values["debug_replay_silver_wolves"], "2400 frames, matches baseline")
        self.assertEqual(values["debug_replay_first_lever_room"], "900 frames, differs")

    def test_catch_summaries(self) -> None:
        self.assertEqual(promotion_check.parse_catch("All tests passed (1,024 assertions in 12 test cases)\n", "f"),
                         {"f_cases": 12, "f_assertions": 1024, "f_failed": 0})
        self.assertEqual(promotion_check.parse_catch("All tests passed (1 assertion in 1 test case)", "f"),
                         {"f_cases": 1, "f_assertions": 1, "f_failed": 0})
        failed = "test cases: 12 | 11 passed | 1 failed\nassertions: 99 | 98 passed | 1 failed\n"
        self.assertEqual(promotion_check.parse_catch(failed, "f"), {"f_cases": 12, "f_failed": 1})
        self.assertEqual(promotion_check.parse_catch("No test cases matched", "f"), {})


class Snapshot(unittest.TestCase):
    def test_changes_are_found_and_skipped_trees_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "Game").mkdir()
            (root / "data").mkdir()
            (root / "Game" / "a.cpp").write_text("a", encoding="utf-8")
            (root / "data" / "disc.bin").write_text("x", encoding="utf-8")
            before = promotion_check.snapshot(root)
            self.assertEqual(list(before), [str(Path("Game") / "a.cpp")])
            (root / "data" / "disc.bin").write_text("y", encoding="utf-8")
            self.assertEqual(promotion_check.changed_paths(before, promotion_check.snapshot(root)), [])
            (root / "Game" / "a.cpp").write_text("b", encoding="utf-8")
            (root / "Game" / "b.cpp").write_text("new", encoding="utf-8")
            self.assertEqual(promotion_check.changed_paths(before, promotion_check.snapshot(root)),
                             [str(Path("Game") / "a.cpp"), str(Path("Game") / "b.cpp")])


class Staging(unittest.TestCase):
    def options(self, **overrides) -> argparse.Namespace:
        return argparse.Namespace(**({"generate": False, "no_generate": False, "no_tidy": False} | overrides))

    def names(self, mode: str, **overrides) -> list[list[str]]:
        with mock.patch.object(promotion_check, "msbuild", return_value="msbuild.exe"):
            stages = promotion_check.plan(mode, "[tag]", self.options(**overrides))
        return [[step.name for step in stage] for stage in stages]

    def test_iterate_builds_only_the_tests(self) -> None:
        self.assertEqual(self.names("iterate"), [["build-debug-tests"], ["focused-debug"]])
        self.assertEqual(self.names("iterate", generate=True)[0], ["generate"])
        with mock.patch.object(promotion_check, "msbuild", return_value="msbuild.exe"):
            build = promotion_check.plan("iterate", "[tag]", self.options())[0][0].action
        self.assertIn("/t:VS_Tests", build)
        self.assertIn("/p:Configuration=Debug", build)

    def test_group_overlaps_the_release_build_and_runs_release_once(self) -> None:
        self.assertEqual(self.names("group"), [
            ["generate"], ["build-debug"], ["regression-debug", "focused-debug", "build-release"],
            ["regression-release", "focused-release"], ["copier-tests", "quality-tidy"]])
        flattened = [name for stage in self.names("group") for name in stage]
        self.assertEqual(flattened.count("build-release"), 1)
        self.assertNotIn("quality-tidy", [name for stage in self.names("group", no_tidy=True) for name in stage])
        self.assertNotIn(["generate"], self.names("group", no_generate=True))

    def test_a_failing_stage_stops_the_run(self) -> None:
        calls = []

        def step(code: int):
            def action(log: Path) -> int:
                calls.append(log.stem)
                log.write_text(f"exit {code}\n", encoding="utf-8")
                return code
            return action

        stages = [[promotion_check.Step("one", step(0)), promotion_check.Step("two", step(3))],
                  [promotion_check.Step("three", step(0))]]
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(promotion_check, "ROOT", Path(folder)), \
                mock.patch("builtins.print"):
            results, passed = promotion_check.run_stages(stages, Path(folder))
        self.assertFalse(passed)
        self.assertEqual(sorted(calls), ["one", "two"])
        self.assertEqual([result.code for result in results], [0, 3])


if __name__ == "__main__":
    unittest.main()
