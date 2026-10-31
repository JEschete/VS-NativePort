"""Catch lost behavior while functions are modernized (Docs\\MODERNIZATION.md).

  python tools\\test\\regression.py check [--config Debug]     run the tests and scenario replays against the baseline
  python tools\\test\\regression.py record [--config Debug]    replace that configuration's baseline with a passing run
  python tools\\test\\regression.py static                     no build needed: every baseline test still exists in source
  python tools\\test\\regression.py coverage                   measure what the tests and replays execute, per function

tools\\test\\regression_baseline.json holds test names, the assertion total and per-window hashes of each replay's frame
state: no disc data. A replay steps the frame counter (--fixed-frame-ticks), the wall-clock input to the AI's randomness.
Recording is a reviewed change: its diff is the list of behavior someone chose to give up.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ledger"))
import gates  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "tools" / "test" / "regression_baseline.json"
TESTS = ROOT / "Native" / "Tests" / "src"
SCENARIOS = {
    "silver_wolves": ("tools/test/scenarios/silver_wolves.json", 2400),
    "first_lever_room": ("tools/test/scenarios/first_lever_room.json", 900),
}
# Odd, because the AI masks its random state with a power of two; an even step would freeze the low bits.
FIXED_FRAME_TICKS = "7"
WINDOW_FRAMES = 100
VOLATILE_FIELDS = {"elapsed_us", "sequence"}
HASH_CHARACTERS = 16
TEST_TIMEOUT_SECONDS = 1800
REPLAY_TIMEOUT_SECONDS = 600
COVERAGE_CONFIG = "Debug"
COVERAGE_PROJECTS = [r"Native\Tests\VS.Tests.vcxproj", r"Native\Tools\CombatLab\VS.CombatLab.vcxproj"]


@dataclass(frozen=True)
class Problem:
    rule: str
    subject: str
    message: str


@dataclass
class TestRun:
    passed: set[str] = field(default_factory=set)
    failed: set[str] = field(default_factory=set)
    skipped: set[str] = field(default_factory=set)
    assertions: int = 0


def parse_report(text: str) -> TestRun:
    root = ET.fromstring(text)
    run = TestRun()
    for case in root.findall("TestCase"):
        result = case.find("OverallResult")
        name = case.attrib["name"]
        if result is None or result.attrib.get("success") != "true":
            run.failed.add(name)
        elif int(result.attrib.get("skips", "0")) > 0:
            run.skipped.add(name)
        else:
            run.passed.add(name)
    total = root.find("OverallResults")
    run.assertions = int(total.attrib["successes"]) if total is not None else 0
    return run


def compare_tests(baseline: dict, run: TestRun) -> list[Problem]:
    problems = [Problem("test-failed", name, "test failed") for name in sorted(run.failed)]
    for name in baseline["cases"]:
        if name in run.skipped:
            problems.append(Problem("test-skipped", name, "skipped, so it no longer protects this behavior"))
        elif name not in run.passed and name not in run.failed:
            problems.append(Problem("test-lost", name, "in the baseline but no longer in the test run"))
    if run.assertions < baseline["assertions"]:
        problems.append(Problem(
            "assertions-reduced", "total", f"{run.assertions:,} assertions, baseline {baseline['assertions']:,}"))
    return problems


def declared_tests(directory: Path) -> set[str]:
    names: set[str] = set()
    for path in directory.rglob("*.cpp"):
        code = gates.strip_source(path.read_text(encoding="utf-8-sig", errors="replace"), keep_strings=True)
        names |= {gates.test_case_name(match) for match in gates.TEST_CASE.findall(code)}
    return names


def compare_source(baseline: dict, directory: Path) -> list[Problem]:
    declared = declared_tests(directory)
    return [
        Problem("test-lost", name, "in the baseline but no test declares it")
        for name in baseline["cases"]
        if name not in declared and name.rsplit(" - ", 1)[0] not in declared
    ]


def without_addresses(frame: dict) -> dict:
    """Memory addresses move with every build, so keep the script position as an offset and drop the raw address."""
    event = frame.get("data", {}).get("scene", {}).get("event")
    if isinstance(event, dict) and "script_buffer" in event:
        base = event.pop("script_buffer")
        for thread in event.get("threads", []):
            if thread.get("pc"):
                thread["pc"] -= base
    return frame


def fingerprint(text: str) -> dict:
    """Hash each window of per-tick game state; wall-clock fields and non-frame records do not count."""
    frames = 0
    windows: list[str] = []
    digest = hashlib.sha256()
    for line in text.splitlines():
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        if record.get("category") != "frame":
            continue
        kept = without_addresses({key: value for key, value in record.items() if key not in VOLATILE_FIELDS})
        digest.update(json.dumps(kept, sort_keys=True, separators=(",", ":")).encode())
        frames += 1
        if frames % WINDOW_FRAMES == 0:
            windows.append(digest.hexdigest()[:HASH_CHARACTERS])
            digest = hashlib.sha256()
    if frames % WINDOW_FRAMES:
        windows.append(digest.hexdigest()[:HASH_CHARACTERS])
    return {"frames": frames, "windows": windows}


def compare_replay(name: str, baseline: dict, actual: dict) -> list[Problem]:
    problems = []
    if actual["frames"] != baseline["frames"]:
        problems.append(Problem("replay-frames", name, f"{actual['frames']} frames, baseline {baseline['frames']}"))
    for index, (expected, found) in enumerate(zip(baseline["windows"], actual["windows"], strict=False)):
        if expected != found:
            start = index * WINDOW_FRAMES
            problems.append(Problem(
                "replay-diverged", name, f"game state first differs in frames {start}-{start + WINDOW_FRAMES - 1}"))
            break
    return problems


def compare_configurations(replays: dict) -> list[Problem]:
    """Debug and Release must play the same, so a difference is a build-specific value or a real defect."""
    if len(replays) < 2:
        return []
    first, *others = sorted(replays)
    return [
        Problem("replay-configurations-differ", name, f"{first} and {other} differ")
        for other in others for name in sorted(replays[first].keys() & replays[other].keys())
        if replays[first][name] != replays[other][name]
    ]


def run_tests(executable: Path) -> TestRun:
    with tempfile.TemporaryDirectory() as directory:
        report = Path(directory) / "tests.xml"
        subprocess.run(
            [str(executable), "--reporter", "xml", "--out", str(report)], cwd=ROOT, check=False,
            timeout=TEST_TIMEOUT_SECONDS, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        if not report.is_file():
            raise SystemExit(f"{executable.name} wrote no report")
        return parse_report(report.read_text(encoding="utf-8", errors="replace"))


def run_replay(executable: Path, scenario: str, frames: int) -> dict:
    runs = executable.parent / "combat-lab" / "runs"
    before = set(runs.iterdir()) if runs.is_dir() else set()
    completed = subprocess.run(
        [str(executable), "--scenario", str(ROOT / scenario), "--start", "--scripted", "--frames", str(frames),
         "--fixed-frame-ticks", FIXED_FRAME_TICKS],
        cwd=ROOT, check=False, timeout=REPLAY_TIMEOUT_SECONDS, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    created = sorted(set(runs.iterdir()) - before)
    if completed.returncode != 0 or not created:
        raise SystemExit(f"{scenario}: {executable.name} exited {completed.returncode}")
    return fingerprint((created[-1] / "events.jsonl").read_text(encoding="utf-8", errors="replace"))


def read_baseline() -> dict:
    return json.loads(BASELINE.read_text(encoding="utf-8")) if BASELINE.is_file() else {"tests": {}, "replays": {}}


def write_baseline(baseline: dict) -> None:
    BASELINE.write_text(json.dumps(baseline, indent=1, sort_keys=True) + "\n", encoding="utf-8")


def build_directory(config: str) -> Path:
    return ROOT / "build" / config


def check(config: str) -> list[Problem]:
    baseline = read_baseline()
    if not baseline["tests"]:
        return [Problem("baseline-missing", str(BASELINE), "run regression.py record")]
    directory = build_directory(config)
    run = run_tests(directory / "VS.Tests.exe")
    print(f"{config} tests: {len(run.passed):,} passed, {len(run.failed)} failed, {len(run.skipped)} skipped, "
          f"{run.assertions:,} assertions")
    problems = compare_tests(baseline["tests"], run)
    replays = baseline["replays"].get(config, {})
    for name, (scenario, frames) in SCENARIOS.items():
        if name not in replays:
            problems.append(Problem("baseline-missing", name, f"no {config} replay baseline"))
            continue
        actual = run_replay(directory / "VS.CombatLab.exe", scenario, frames)
        found = compare_replay(name, replays[name], actual)
        print(f"{config} replay {name}: {actual['frames']} frames, {'differs' if found else 'matches baseline'}")
        problems += found
    return problems


def record(config: str) -> list[Problem]:
    directory = build_directory(config)
    run = run_tests(directory / "VS.Tests.exe")
    if run.failed:
        return [Problem("test-failed", name, "a failing run cannot become the baseline") for name in sorted(run.failed)]
    baseline = read_baseline()
    baseline["tests"] = {"cases": sorted(run.passed | run.skipped), "assertions": run.assertions}
    replays = baseline["replays"].setdefault(config, {})
    for name, (scenario, frames) in SCENARIOS.items():
        first = run_replay(directory / "VS.CombatLab.exe", scenario, frames)
        second = run_replay(directory / "VS.CombatLab.exe", scenario, frames)
        repeat = compare_replay(name, first, second)
        if repeat:
            return [Problem("replay-unstable", name, f"two identical runs differ: {repeat[0].message}")]
        replays[name] = first
    differences = compare_configurations(baseline["replays"])
    if differences:
        return differences
    write_baseline(baseline)
    print(f"recorded {config}: {len(baseline['tests']['cases']):,} tests, {run.assertions:,} assertions, "
          f"{len(replays)} replays")
    return []


def tool(name: str) -> str:
    return subprocess.run(
        [os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"), "-latest", "-products",
         "*", "-find", rf"VC\Tools\Llvm\x64\bin\{name}"],
        capture_output=True, text=True, check=True,
    ).stdout.splitlines()[0]


def msbuild() -> str:
    return subprocess.run(
        [os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"), "-latest", "-products",
         "*", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"],
        capture_output=True, text=True, check=True,
    ).stdout.splitlines()[0]


def executed(row: dict[str, str]) -> bool:
    return "yes" in (row["tests_execute"], row["replay_execute"])


def coverage_losses(before: list[dict[str, str]], after: list[dict[str, str]]) -> list[Problem]:
    """A function that ran before and runs no longer: its behavior can now change unnoticed."""
    previous = {row["id"]: row for row in before}
    return [
        Problem("coverage-lost", row["id"], "executed at the last measurement, not now")
        for row in after
        if row["id"] in previous and executed(previous[row["id"]]) and not executed(row)
    ]


def export_coverage(directory: Path, name: str, raw: Path, objects: list[Path]) -> Path:
    merged = directory / f"{name}.profdata"
    subprocess.run([tool("llvm-profdata.exe"), "merge", "-sparse", str(raw), "-o", str(merged)], check=True)
    output = directory / f"{name}.json"
    command = [tool("llvm-cov.exe"), "export", "--format=text", "--skip-expansions", f"-instr-profile={merged}",
               str(objects[0]), *[f"-object={item}" for item in objects[1:]]]
    with output.open("w", encoding="utf-8") as stream:
        subprocess.run(command, check=True, stdout=stream)
    return output


def coverage() -> list[Problem]:
    directory = ROOT / "build" / "Coverage"
    ledger = ROOT / "Docs" / "ledgers" / "MODERNIZATION_LEDGER.csv"
    before = gates.read_csv(ledger)
    directory.mkdir(parents=True, exist_ok=True)
    log = directory / "build.log"
    with log.open("w", encoding="utf-8") as stream:
        for project in COVERAGE_PROJECTS:
            subprocess.run(
                [msbuild(), project, "/m", "/restore", "/nologo", "/v:minimal", f"/p:Configuration={COVERAGE_CONFIG}",
                 "/p:Platform=x64", "/p:VSCoverage=true"],
                cwd=ROOT, check=True, stdout=stream, stderr=subprocess.STDOUT,
            )
    objects = [directory / "VS.Tests.exe", directory / "VS.CombatLab.exe"]
    environment = dict(os.environ)
    raw_tests = directory / "tests.profraw"
    raw_replays = directory / "replays-%p.profraw"
    subprocess.run(
        [str(objects[0]), "--reporter", "xml", "--out", str(directory / "coverage-tests.xml")], cwd=ROOT, check=False,
        env={**environment, "LLVM_PROFILE_FILE": str(raw_tests)}, timeout=TEST_TIMEOUT_SECONDS * 3,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    for scenario, frames in SCENARIOS.values():
        subprocess.run(
            [str(objects[1]), "--scenario", str(ROOT / scenario), "--start", "--scripted", "--frames", str(frames),
             "--fixed-frame-ticks", FIXED_FRAME_TICKS],
            cwd=ROOT, check=False, env={**environment, "LLVM_PROFILE_FILE": str(raw_replays)},
            timeout=REPLAY_TIMEOUT_SECONDS * 3, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
    tests = export_coverage(directory, "tests", raw_tests, objects)
    replays = export_coverage(directory, "replays", directory / "replays-*.profraw", objects)
    subprocess.run(
        [sys.executable, str(ROOT / "tools" / "ledger" / "modernization.py"), "--sync", "--coverage", str(tests),
         "--replay-coverage", str(replays)],
        cwd=ROOT, check=True,
    )
    subprocess.run(
        [sys.executable, str(ROOT / "tools" / "ledger" / "hazards.py"), "--sync", "--compiler-log", str(log)],
        cwd=ROOT, check=True,
    )
    return coverage_losses(before, gates.read_csv(ledger))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["check", "record", "static", "coverage"])
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release"])
    arguments = parser.parse_args()
    if arguments.command == "check":
        problems = check(arguments.config)
    elif arguments.command == "record":
        problems = record(arguments.config)
    elif arguments.command == "coverage":
        problems = coverage()
    else:
        baseline = read_baseline()["tests"]
        problems = compare_source(baseline, TESTS) if baseline else [Problem("baseline-missing", str(BASELINE), "")]
    for problem in problems:
        print(f"{problem.rule}: {problem.subject}: {problem.message}")
    print(f"{len(problems)} regressions")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
