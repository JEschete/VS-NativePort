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

Use the project's Python environment and Node.js 22 or newer:

```powershell
python -m pip install -r tools/requirements.txt
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
owned C++ minimap implementation that consumes the existing C ABI.
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

## Scope And Baseline

The default scope includes hand-maintained native sources and first-party Python
tooling. It reads the copier manifest and ownership registry to exclude generated
native output. Reference repositories, extracted data, vendor packages, and tool
collections under `tools/PSX` and `tools/Base` are not project-style targets.

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

The CI workflow runs the fast gate and tool positive controls without game data.
Run the full `--tidy` gate locally for native changes; the fast CI job is not a
substitute for compilation-based analysis or game testing.

```powershell
python -m unittest discover -s tools/lint -p test_quality.py -v
```
