# Source Quality

Native code follows the Google C++ style conventions. The tools report existing
debt separately and reject new findings; a passing baseline check does not mean
the entire port is clean or that gameplay is complete.

## Tools

| Tool | Purpose |
| --- | --- |
| clang-format 22.1.8 | Google formatting: two spaces, 80 columns, no tabs |
| cpplint 2.0.2 | Google style checks, including includes and header conventions |
| clang-tidy 22.1.8 | Static analysis, Google naming, magic numbers, large functions, and include cycles |
| Ruff 0.14.0 | Python correctness, imports, simplification, and modernization |
| jscpd 4.0.5 | Potential copy/paste duplication across first-party code |

The copyright-header requirement and obsolete C++17-header ban are disabled
globally in cpplint: this is a C++23 project. Tests also
disable its logging-style CHECK_EQ recommendation because they use Catch2 CHECK.
Do not add copyright headers merely to satisfy a tool. Preserve existing published C interfaces and
decompilation provenance when migrating code; naming changes require caller review.

## Setup

Use the retained `.venv-3` Python environment and Node.js 22 or newer.
Create the environment only on a fresh clone; see [BUILDING.md](BUILDING.md).
Activate it in each new shell before running Python-based commands:

```powershell
.\.venv-3\Scripts\Activate.ps1
python -m pip install -r tools\requirements.txt
npm ci --prefix tools/lint --ignore-scripts --no-audit --no-fund
```

The runner prefers the pinned Python-distributed LLVM binaries. Visual Studio's
LLVM installation provides clang-cl for compilation-based checks. Full clang-tidy
analysis also needs the native vcpkg headers installed by the normal build.

## Commands

Run from the repository root:

```cmd
quality-native.cmd
quality-native.cmd --changed
quality-native.cmd --changed --tidy
quality-native.cmd --files Native/Engine/src/Core/FrameClock.cpp --tidy
quality-native.cmd --tidy
```

The default command runs formatting, cpplint, Ruff, and clone detection. `--tidy`
adds compilation-based analysis. Header changes trigger analysis of all owned
translation units. Clone detection always scans the complete first-party scope
so duplication with an unchanged file is still caught.

The runner generates compile commands from the solution generator's project table
and shared compiler properties; do not hand-maintain a second include-path list.
The generator and analyzer share the C-header interop predicate, including the
owned C++ minimap and actor sound-override implementations that consume the
existing C ABI.
Exit codes are 0 for no new findings, 1 for new findings, and 2 for tool/configuration
failures. Commands never rewrite source or update the baseline automatically.

## Running CI explicitly

The Source Quality workflow runs on pushes and pull requests and can also be
dispatched manually. If a published commit has no run, request the same checks
without rewriting history or creating an empty commit:

```powershell
gh workflow run quality.yml --ref work
gh run list --workflow quality.yml --branch work --event workflow_dispatch --limit 5 --json databaseId,headSha,status,conclusion
gh run watch <run-id> --exit-status
```

Check that `headSha` is the intended published revision before using the result
to close an issue. A previous revision's passing run is not a substitute.
Manual dispatch uses the same Windows job, permissions and quality baselines
as the automatic triggers.

The Windows job also checks symbol-ledger merge and member-pairing contracts:

```powershell
.\.venv-3\Scripts\python.exe -m unittest discover -s tools\naming -p test_*.py -v
```

These tests use isolated fixtures, not disc data, reference indexes or live
ledgers. Actual generator-output correctness, ownership preservation and repeat
generation are separate required checks when changing generator behavior.

## What a green run establishes

A green Source Quality run establishes only that, at that exact revision, the
default scope has no findings beyond the reviewed baseline, the synthetic tool
controls pass, and the ledgers satisfy the gate check below.

It does **not** establish that:

- generated copied C is clean: it is outside the default quality scope;
- maintained `.c` code is strict: it is in scope but compiles relaxed;
- any code is modernized, C++-promoted or behaviorally verified: those are
  separate gates, and this job builds nothing and runs no native test;
- `--tidy` analysis passed: the fast CI job does not run it;
- gameplay works: no game data is available to CI.

## Separately reported gates

`tools/ledger/gates.py` reports five gates that do not read each other's result
as evidence (#46):

```powershell
.\.venv-3\Scripts\python.exe tools\ledger\gates.py          # write build\quality\gates.{json,md}, gates-rows.csv
.\.venv-3\Scripts\python.exe tools\ledger\gates.py --check  # also exit 1 on any violation (runs in CI)
.\.venv-3\Scripts\python.exe tools\ledger\gates.py --sync   # refresh machine columns, catalog cpp and the summary
.\.venv-3\Scripts\python.exe -m unittest discover -s tools\ledger -p test_gates.py -v
```

| Gate | Inputs | `--check` rejects |
| --- | --- | --- |
| Ownership | catalog, `owned.csv`, copy manifest | unknown/duplicate owned IDs, missing paths, owned paths in the manifest, location mismatches, unregistered rows in owned files, invalid `copied`, `retired` without `copy=no` and `retired-by:` |
| C++ promotion | `CPP_PROMOTION.csv`, source, projects | a §8.2 claim that does not verify, a missing record for an owned C++ function, catalog `cpp` differing from the derived value, remaining consumers of the old symbol |
| Quality | generated `.vcxproj` files, `baseline.json` | owned C++ or any promotion claim compiled relaxed, `strict_clean` on a file with baseline debt |
| Behavior | `VERIFY_LEDGER.csv`, Catch2 sources | `verified=x` without a passing record, unresolved references, missing revision, configuration or limits, invalid values |
| Retirement | `EC_BRIDGES.csv`, `GATE_SLICES.csv`, include graph | `modernized` on unowned rows, `modernized=x` reaching a bridge directly or indirectly, a listed bridge that no longer exists, a slice whose measured consumers differ from its record |

Source classes come from each file's actual compiler settings in the generated
projects, not its extension: `generated-c` (relaxed, outside quality scope),
`maintained-c` (relaxed, in scope), `strict-cpp` and `replacement-legacy`
(`RETIRING_NATIVE` and `Legacy/`). The report lists maintained relaxation as debt
per file, with owned-function and baseline-finding counts. Whether maintained C
should compile more strictly before promotion remains an owner decision.

The tool reads the baseline and never writes it. Moving or renaming a bridge
without updating `EC_BRIDGES.csv` fails the check instead of lowering a consumer
count. Include-graph consumers are a static measure over every preprocessor
branch, so they can overcount a conditional include. Stored records are
references, not fresh runs; rerun what a closure claim depends on.

## Event dispatch contract audit

After generation, validate the event-handler C ABI and both 256-entry dispatch
tables with:

```powershell
.\.venv-3\Scripts\python.exe tools\port\check_event_dispatch.py
```

This local check requires the extracted US `Native\data\disc\BATTLE\BATTLE.PRG`
module; `--module <path>` can select another copy. It parses generated headers
plus the registered headers of owned opcode handlers from `tools\port\owned.csv`,
since those headers deliberately do not appear in the copier manifest.
An owned opcode entry without a header is an error. Unrelated owned headers are
not added to the C translation unit. Canonical function signatures, pointer
table types, entry counts and the complete normal/skip target set are still
checked; ownership is not an ABI exemption.

Asset-independent header-selection, deduplication and missing-contract controls
run in the existing encounter-fixup suite and CI:

```powershell
.\.venv-3\Scripts\python.exe -m unittest discover -s tools\port -p test_encounter_fixups.py
```

The actual disc-backed audit remains a separate local verification; its module
is not committed or uploaded. Passing it is not gameplay or compatibility-
retirement verification.

## Scope And Baseline

The default scope includes hand-maintained native sources and first-party Python
tooling. It reads the copier manifest and ownership registry to exclude generated
native output. Reference repositories, extracted data, vendor packages, and tool
collections under `tools/PSX` and `tools/Base` are not project-style targets.
Python replay validators under `tools/test` are maintained tooling too; native
sequence fixtures are exercised by their runtime and semantic verification steps.
The standalone external player under `Native/Player` is also maintained Python
tooling. Its asset-independent contracts run in Source Quality CI:

```powershell
.\.venv-3\Scripts\python.exe -m unittest discover -s Native\Player\tests -v
```

These controls cover observation validation, timing boundaries, useful-effect
attribution, stale plans, menu transactions and owned evidence retention. They
do not replace live gameplay; see [the player guide](../Native/Player/README.md)
for local model setup and the separate combat/physical-door acceptance gates.

```cmd
quality-native.cmd --list
```

Findings and clone details are written under `build/quality`. The
report separates native production, legacy directories, tests, and tooling. Exact
test-fixture literal warnings are listed separately as `intentional_test_fixtures`:
known binary bytes and expected results should remain inspectable in tests.
This exception does not apply to production headers analyzed through test files,
nor to safety, bounds, type, lifetime, or other test diagnostics. The checked-in
baseline is debt tracking, not a blanket suppression: changed code or additional
occurrences of a finding are rejected. Formatting debt is tied to a file's current
content, ignoring CRLF/LF differences; edit such a file into Google format rather
than extending its old formatting.

Style-only debt in `Legacy` directories and the explicit EC-01/EC-02/EC-03/EC-05
page-space, SPU-register reset, GTE-state and scratchpad replacements is reported separately in
`deferred_replacement_style`. The exact paths are in `RETIRING_NATIVE`; owned C
gameplay code is not automatically a replacement exception. Safety and compiler
diagnostics from replacement code remain checked. See
[EMULATION_COMPATIBILITY_AUDIT.md](EMULATION_COMPATIBILITY_AUDIT.md).

Existing `VS` subsystem namespaces and decomp-linked C exports/types retain their
published names. Local identifiers and private C++ state use Google naming.
The naming configuration lists the existing public logging/input/mode values,
sector/timing constants, generated module-table symbols and native-view field
spellings that are preserved. These are explicit spelling exceptions, not a
blanket exemption for new public or private names; a real local-parameter naming
control remains in the quality tests.

Positional-parameter suggestions for exact function/file pairs in the ownership
registry are reported as `fixed_interface_advisories`: their published C call
contracts must be preserved. Private helpers, conversions, bounds and lifetime
diagnostics are not covered by this exception. Regression controls check both
the registered path/name match and continued safety reporting.

`language_advisories` retains cpplint's inapplicable C++ `<string>` suggestion
for a direct `.string`/`->string` member assignment in a native C17 source.
That field name does not make a C structure use `std::string`, and a C++
standard header cannot repair C code. This classification is limited to that
exact tool/rule/message and C member expression; C++ sources, headers, other
include advice and safety diagnostics remain actionable. Real C/C++ tool
controls verify both sides. The diagnostic stays visible in the report and
does not require a baseline or source suppression.

The positional-argument heuristic requires at least three identical-type
arguments and does not group implicit conversions. This avoids redesigning
ordinary two-value APIs solely for lint while still flagging ambiguous private
argument groups; a real-tool positive control verifies that coverage.

The analyzer's blanket `DeprecatedOrUnsafeBufferHandling` API advisory is disabled:
bounded standard-library copies are permitted, and Annex K `_s` functions are not
the project's portability contract. Actual null, bounds, allocation and lifetime
analysis remains enabled and has an executable null-dereference positive control.

Baseline changes must be deliberate and reviewed with the full report:

```cmd
quality-native.cmd --tidy --update-baseline
```

Do not run this command to make an unexplained failure green. Compiler errors and
missing/invalid tool reports cannot be baselined. No repository-wide automatic
formatting or symbol renaming is part of setup.

For an intentional formatting cleanup, preserve an accepted full report before
editing. The format-only reconciliation command uses libclang tokenization to
match the same diagnostic in unchanged C/C++ tokens, including string contents:

```cmd
quality-native.cmd --tidy --reconcile-formatting build/quality/cleanup-before.json
```

This explicitly rewrites matching baseline fingerprints, not the source. It does
not accept changed expressions, changed literals, or copied code. If a linter
previously collapsed multiple warnings on one line, they may expand only within
the same unchanged token region. Unmatched findings still fail the gate. Ordinary
scans remain read-only. Review the report and baseline diff after reconciliation.

Local, rule-specific lint exceptions document compiler-required include order,
C generic selection, or a formatter/linter disagreement. Do not disable an entire
tool or category to conceal native behavior or layout problems.

## Review Rules

- Prefer existing menu, rendering, math, memory, and asset helpers before adding one.
- Use named constants or enums for states, limits, units, and format dimensions.
- Keep exact source bytes and expected values in focused tests where they are evidence.
- Investigate clone reports before extracting an abstraction; similar syntax alone
  does not establish identical behavior or ownership.
- Fix copied-code defects in the copier or a registered owned implementation.
- Keep behavior tests, native-layout checks, and build verification alongside lint.

The CI workflow runs the fast gate, the ledger gate check and tool positive
controls without game data. Run the full `--tidy` gate locally for native changes;
the fast CI job is not a substitute for compilation-based analysis or game testing.

```powershell
python -m unittest discover -s tools/lint -p test_quality.py -v
```
