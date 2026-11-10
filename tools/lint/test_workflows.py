"""Controls for workflow exit codes: each `run` step must hold one command (#333)."""

from __future__ import annotations

import textwrap
import unittest
from pathlib import Path

import yaml

WORKFLOWS = Path(__file__).resolve().parents[2] / ".github" / "workflows"
CONTINUATIONS = ("`", "^", "\\")


def commands(script: str) -> list[str]:
    """Split a run script into commands, joining lines that continue the previous."""
    found: list[str] = []
    continued = False
    for line in script.splitlines():
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        if continued:
            found[-1] += " " + text
        else:
            found.append(text)
        continued = text.endswith(CONTINUATIONS)
    return found


def multi_command_steps(workflow: dict) -> list[str]:
    """Name the steps whose `run` holds several commands.

    A Windows PowerShell step reports only the last native command's exit code, so a
    failing command followed by a passing one is reported as success.
    """
    found = []
    for job_name, job in (workflow.get("jobs") or {}).items():
        for number, step in enumerate(job.get("steps") or [], start=1):
            script = step.get("run")
            if script is not None and len(commands(str(script))) > 1:
                found.append(f"{job_name}: {step.get('name', f'step {number}')}")
    return found


class WorkflowExitCodeTests(unittest.TestCase):
    def load(self, text: str) -> dict:
        return yaml.safe_load(textwrap.dedent(text))

    def test_a_command_before_another_command_in_one_step_is_rejected(self) -> None:
        workflow = self.load(
            r"""
            jobs:
              quality:
                steps:
                  - name: Check ledgers
                    run: |
                      python tools\ledger\hazards.py --check
                      python tools\test\regression.py static
                  - name: Check gates
                    run: python tools\ledger\gates.py --check
            """
        )
        self.assertEqual(multi_command_steps(workflow), ["quality: Check ledgers"])

    def test_an_unnamed_step_is_reported_by_position(self) -> None:
        workflow = self.load(
            """
            jobs:
              quality:
                steps:
                  - uses: actions/checkout@v4
                  - run: |
                      first
                      second
            """
        )
        self.assertEqual(multi_command_steps(workflow), ["quality: step 2"])

    def test_one_command_per_step_passes(self) -> None:
        workflow = self.load(
            """
            jobs:
              quality:
                steps:
                  - uses: actions/checkout@v4
                  - name: Check
                    run: python check.py --all
                  - name: Wrapped
                    run: |
                      python check.py `
                        --all `
                        --strict
            """
        )
        self.assertEqual(multi_command_steps(workflow), [])

    def test_blank_lines_and_comments_do_not_count(self) -> None:
        self.assertEqual(
            commands("\n# explain\n\npython check.py\n  # trailing note\n"),
            ["python check.py"],
        )

    def test_every_repository_workflow_step_runs_one_command(self) -> None:
        paths = sorted([*WORKFLOWS.glob("*.yml"), *WORKFLOWS.glob("*.yaml")])
        self.assertTrue(paths, "no workflows found to check")
        for path in paths:
            with self.subTest(workflow=path.name):
                workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
                self.assertEqual(multi_command_steps(workflow), [])


if __name__ == "__main__":
    unittest.main()
