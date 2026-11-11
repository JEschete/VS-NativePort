"""Build and test a promotion without replaying CI locally (#340).

  python tools\\test\\promotion_check.py iterate --focused "[ai-actor-creation]" [--generate]
  python tools\\test\\promotion_check.py group --focused "[actor-creation],[actor-settings]"

iterate is the edit-compile-test loop: the Debug VS.Tests build (and the projects it links), then the focused
tests. --generate first runs group step 1 (about 30 s).

group runs once per group of 4-8 promoted functions, after each function has its own tests, differential and
mutants. Steps on one line run in parallel:

  1. gen_solution.py; copy_to_native.py twice (the second run must change nothing under Native\\)
  2. the Debug solution build
  3. regression.py check --config Debug, the Debug focused tests  |  the Release solution build
  4. regression.py check --config Release, the Release focused tests
  5. the copier and promotion tool tests  |  quality.py --changed --tidy

GitHub CI runs the tool tests and the source-quality checks on the pushed revision, so the full workflow is not
repeated here: only the copier tests the registration edits touch, and clang-tidy, which CI does not run. CI
cannot build or replay (no disc data), so this is the only place those run. promote.py ledgers syncs and checks
the ledgers afterwards. Every step logs to build\\promotion-check\\<mode>-<timestamp>\\<step>.log; summary.json
holds the counts that promote.py ledgers --summary fills into the ledgers.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "build" / "promotion-check"
SOLUTION = ROOT / "VagrantStory.Native.sln"
GENERATED = ROOT / "Native"
SKIPPED_TREES = {"data"}  # extracted disc data under Native\
COPIER_TESTS = ("test_encounter_fixups.py", "test_scene_trace_fixups.py", "test_module_symbol_extents.py",
                "test_differential.py", "test_promote.py")
TESTS_LINE = re.compile(r"^(Debug|Release) tests: ([\d,]+) passed, (\d+) failed, (\d+) skipped, ([\d,]+) assertions")
REPLAY_LINE = re.compile(r"^(Debug|Release) replay (\w+): (\d+) frames, (.+)$")
CATCH_PASSED = re.compile(r"All tests passed \(([\d,]+) assertions? in ([\d,]+) test cases?\)")
CATCH_FAILED = re.compile(r"test cases:\s*([\d,]+) \|\s*([\d,]+) passed \|\s*([\d,]+) failed")


def number(text: str) -> int:
    return int(text.replace(",", ""))


@dataclass
class Step:
    name: str
    action: list[str] | Callable[[Path], int]
    cwd: Path = ROOT


@dataclass
class Result:
    name: str
    code: int
    seconds: float
    log: Path
    values: dict[str, object] = field(default_factory=dict)


# ---------------------------------------------------------------------- parsing (unit-tested)
def parse_regression(text: str) -> dict[str, object]:
    values: dict[str, object] = {}
    for line in text.splitlines():
        if match := TESTS_LINE.match(line.strip()):
            config = match.group(1).lower()
            values |= {f"{config}_cases": number(match.group(2)) + number(match.group(4)),
                       f"{config}_passed": number(match.group(2)), f"{config}_failed": number(match.group(3)),
                       f"{config}_assertions": number(match.group(5))}
        elif match := REPLAY_LINE.match(line.strip()):
            values[f"{match.group(1).lower()}_replay_{match.group(2)}"] = (
                f"{match.group(3)} frames, {match.group(4)}")
    return values


def parse_catch(text: str, prefix: str) -> dict[str, object]:
    if match := CATCH_PASSED.search(text):
        return {f"{prefix}_cases": number(match.group(2)), f"{prefix}_assertions": number(match.group(1)),
                f"{prefix}_failed": 0}
    if match := CATCH_FAILED.search(text):
        return {f"{prefix}_cases": number(match.group(1)), f"{prefix}_failed": number(match.group(3))}
    return {}


def snapshot(root: Path, skipped: set[str] = SKIPPED_TREES) -> dict[str, str]:
    """SHA-256 of every file under root except the skipped top-level trees."""
    hashes = {}
    for directory, folders, files in os.walk(root):
        if Path(directory) == root:
            folders[:] = [folder for folder in folders if folder not in skipped]
        for name in files:
            path = Path(directory) / name
            hashes[str(path.relative_to(root))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return hashes


def changed_paths(before: dict[str, str], after: dict[str, str]) -> list[str]:
    return sorted(path for path in before.keys() | after.keys() if before.get(path) != after.get(path))


# ---------------------------------------------------------------------- running
def msbuild() -> str:
    return subprocess.run(
        [os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"), "-latest",
         "-products", "*", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"],
        capture_output=True, text=True, check=True).stdout.splitlines()[0]


def build(config: str, target: str | None = None) -> list[str]:
    command = [msbuild(), str(SOLUTION), "/m", "/restore", "/nologo", "/v:minimal", "/clp:Summary",
               f"/p:Configuration={config}", "/p:Platform=x64"]
    return command + [f"/t:{target}"] if target else command


def python(*arguments: str) -> list[str]:
    return [sys.executable, *arguments]


def tests(config: str, selection: str) -> list[str]:
    return [str(ROOT / "build" / config / "VS.Tests.exe"), selection]


def generate(log: Path) -> int:
    """gen_solution.py, then the copier twice; the second copier run must leave Native\\ unchanged."""
    with log.open("a", encoding="utf-8") as stream:
        for command in (python(r"tools\scaffold\gen_solution.py"), python(r"tools\port\copy_to_native.py")):
            stream.write(f"$ {' '.join(command)}\n")
            stream.flush()
            if subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT).returncode:
                return 1
        first = snapshot(GENERATED)
        stream.write("$ copy_to_native.py (stability run)\n")
        stream.flush()
        if subprocess.run(python(r"tools\port\copy_to_native.py"), cwd=ROOT, stdout=stream,
                          stderr=subprocess.STDOUT).returncode:
            return 1
        changed = changed_paths(first, snapshot(GENERATED))
        stream.write(f"second copier run changed {len(changed)} files under Native\\\n")
        stream.writelines(f"  {path}\n" for path in changed)
        stream.write(f"Native\\ fingerprint {fingerprint(first)}\n")
        return 1 if changed else 0


def fingerprint(hashes: dict[str, str]) -> str:
    return hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()[:16]


def copier_tests(log: Path) -> int:
    with log.open("a", encoding="utf-8") as stream:
        failed = 0
        for name in COPIER_TESTS:
            stream.write(f"$ python tools\\port\\{name}\n")
            stream.flush()
            failed |= subprocess.run(python(str(ROOT / "tools" / "port" / name)), cwd=ROOT,
                                     stdout=stream, stderr=subprocess.STDOUT).returncode != 0
        return int(failed)


def run_step(step: Step, directory: Path) -> Result:
    log = directory / f"{step.name}.log"
    start = time.monotonic()
    if callable(step.action):
        code = step.action(log)
    else:
        with log.open("w", encoding="utf-8") as stream:
            stream.write(f"$ {' '.join(step.action)}\n")
            stream.flush()
            code = subprocess.run(step.action, cwd=step.cwd, stdout=stream, stderr=subprocess.STDOUT).returncode
    result = Result(step.name, code, time.monotonic() - start, log)
    text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
    if step.name.startswith("regression-"):
        result.values = parse_regression(text)
    elif step.name.startswith("focused-"):
        result.values = parse_catch(text, step.name.replace("-", "_"))
    return result


def run_stages(stages: list[list[Step]], directory: Path) -> tuple[list[Result], bool]:
    results: list[Result] = []
    for stage in stages:
        print(" | ".join(step.name for step in stage), flush=True)
        with ThreadPoolExecutor(max_workers=len(stage)) as pool:
            finished = list(pool.map(lambda step: run_step(step, directory), stage))
        results += finished
        for result in finished:
            print(f"  {result.name}: {'ok' if result.code == 0 else f'FAILED ({result.code})'} "
                  f"{result.seconds:.0f}s {result.log.relative_to(ROOT)}", flush=True)
        if any(result.code for result in finished):
            for result in finished:
                if result.code:
                    lines = result.log.read_text(encoding="utf-8", errors="replace").splitlines()
                    print(f"--- {result.name} (last 25 lines)\n" + "\n".join(lines[-25:]))
            return results, False
    return results, True


def plan(mode: str, focused: str, options: argparse.Namespace) -> list[list[Step]]:
    if mode == "iterate":
        stages = [[Step("generate", generate)]] if options.generate else []
        return stages + [[Step("build-debug-tests", build("Debug", "VS_Tests"))],
                         [Step("focused-debug", tests("Debug", focused))]]
    stages = [] if options.no_generate else [[Step("generate", generate)]]
    stages += [[Step("build-debug", build("Debug"))],
               [Step("regression-debug", python(r"tools\test\regression.py", "check", "--config", "Debug")),
                Step("focused-debug", tests("Debug", focused)),
                Step("build-release", build("Release"))],
               [Step("regression-release", python(r"tools\test\regression.py", "check", "--config", "Release")),
                Step("focused-release", tests("Release", focused))]]
    final = [Step("copier-tests", copier_tests)]
    if not options.no_tidy:
        final.append(Step("quality-tidy", python(r"tools\lint\quality.py", "--changed", "--tidy", "--jobs", "4")))
    return stages + [final]


def git(*arguments: str) -> str:
    return subprocess.run(["git", *arguments], cwd=ROOT, capture_output=True, text=True, check=True).stdout


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("mode", choices=["iterate", "group"])
    parser.add_argument("--focused", required=True, help='Catch2 selection, e.g. "[ai-actor-creation],[ai-x]"')
    parser.add_argument("--generate", action="store_true", help="iterate: run gen_solution and the copier first")
    parser.add_argument("--no-generate", action="store_true", help="group: the copier output is already current")
    parser.add_argument("--no-tidy", action="store_true", help="group: skip quality.py --changed --tidy")
    options = parser.parse_args(argv)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    directory = OUTPUT / f"{options.mode}-{stamp}"
    directory.mkdir(parents=True)
    start = time.monotonic()
    results, passed = run_stages(plan(options.mode, options.focused, options), directory)
    values: dict[str, object] = {"base": git("rev-parse", "HEAD").strip(), "focused": options.focused,
                                 "dirty_files": len(git("status", "--porcelain").splitlines())}
    for result in results:
        values |= result.values
    if "focused_debug_cases" in values:
        values["focused_tests"] = values["focused_debug_cases"]
    summary = {"mode": options.mode, "passed": passed, "seconds": round(time.monotonic() - start),
               "steps": {result.name: {"code": result.code, "seconds": round(result.seconds),
                                       "log": str(result.log.relative_to(ROOT))} for result in results},
               "values": values}
    (directory / "summary.json").write_text(json.dumps(summary, indent=1) + "\n", encoding="utf-8")
    print(f"{'passed' if passed else 'FAILED'} in {summary['seconds']}s; {directory.relative_to(ROOT)}\\summary.json")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
