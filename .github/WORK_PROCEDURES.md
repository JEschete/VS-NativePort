# Work Procedures

How work in this repository is carried out, step by step. The rules come from the owner and from
[copilot-instructions.md](copilot-instructions.md); this file is the operating checklist. Current status and the next
group are in [HANDOFF.md](../Docs/HANDOFF.md).

## Ground rules

- Commit and push at verified milestones without asking. Never add AI or assistant attribution: no
  `Co-authored-by` trailers, tool names or model names in commits, issues, PRs, comments or files.
- Never force-push or rewrite history. Preserve unrelated edits in the worktree.
- Never delete Ghidra, `tools\Base` or `tools\PSX`. Keep the repository small: delete scratch files in `build\` when
  done, and keep disc-derived data, captures, dumps and BIOS files out of commits.
- Work in the order set by tracker [#4](https://github.com/JEschete/VS-NativePort/issues/4). Log a newly found defect
  as an issue, with evidence, before fixing it.
- A passing build or baseline is not proof of behavior. Close an issue only with the fixing commit, the exact-revision
  CI run and concrete test or replay evidence.

## Session start

1. Read [HANDOFF.md](../Docs/HANDOFF.md) "Stopping Point" and the active issue (currently #276 row 28, battle AI).
2. Check CI on the last pushed commit: `gh run list --repo JEschete/VS-NativePort --limit 4`. Track and repair a
   failure instead of refreshing a baseline to hide it.
3. Check `git status`; finish or commit any verified work left from the previous session before starting new work.

## C++ promotion: one group per commit

Promote 4-8 related functions per commit, for example the functions of one generated file or one call cluster. Each
function gets its own tests, differential coverage and broken-variant check; the builds, replays, ledgers, commit and
CI are shared by the group. The target is minutes per function, not the old 33-63.

Where the old time went (four single-function cycles, 171 minutes): 55% model round trips (about 210 per function,
mostly one tool call each), 11% builds, 10% replays, 6% quality, 3% CI wait, with every check repeated 4-9 times per
function. So batch independent reads and edits into one step, run each gate once per group, and hand long mechanical
work (splitting tests, writing a differential case) to a background agent while doing something else.

### 1. Choose and analyze the group

- Pick the next group named in the HANDOFF. `python tools\port\promote.py show <ID>` lists a function's
  registration, callers and generated source.
- Read the original source and assembly (decomp `src\`, Ghidra or `differential.py` disassembly) before the generated
  copy. Note every field offset, width, sign and the calls each function makes.
- Group functions that call each other, so one differential range covers them together.

### 2. Implement

- Write a pure typed core (`<Name>.hpp/.cpp`) over a small world interface, and a checked live binding
  (`<Name>Live.hpp/.cpp`) that maps the interface onto the game state. The live binding refuses a missing pointer or
  an out-of-range index instead of reading through it.
- Add `// decomp: <symbol> (<source file>:<line>)` provenance to every promoted function.
- Run `python tools\port\promote.py scaffold|register|prune-adaptations` for the copier registration, `owned.csv`,
  the `gen_solution.py` header list, module bindings and stale `cpp_adaptations.json` hunks. Edit those by hand only
  where the tool says it cannot. Never hand-edit `.vcxproj` files; run `python tools\scaffold\gen_solution.py`.
- Stage the group's new tool files before final project generation: the tools solution inventory uses tracked
  files. Restage the scoped files after fixes before committing; staging does not establish verification.
- Migrate the callers to the live binding so the old generated symbols have no references.

### 3. Test while iterating

- Write Catch2 tests for every function under a group tag, covering each branch and refusal.
- Iterate with `python tools\test\promotion_check.py iterate --focused "[tag]"` (Debug game and tests only).
- Keep each `TEST_CASE` to about 9 assertions. Every Catch2 assertion expands to several statements, and tidy's
  `readability-function-size` limit (80 statements, lambdas included) can still fail callback-heavy cases.
  Guard optional reads in the same function or a guarded value-returning helper; `.value()` or a Catch2 `REQUIRE`
  alone is not an explicit guard for tidy. Do not initialize throwing namespace-scope statics, compare structs by
  value or `std::bit_cast` bytes rather than `memcmp`, and keep LF line endings.

### 4. Compare against the original

- Write `tools\port\differentials\<case>.py` and `<case>_driver.cpp`; copy the structure of an existing case
  (`boss_orbit`, `perception`). Run `.\.venv-3\Scripts\python.exe tools\port\differential.py <case>`.
  - `--no-mutants --vectors 256` while fixing build and link errors (add files to `SOURCES`) and mismatches.
  - Then the full run: all vectors with 0 mismatches and every mutant detected.
- Mutants patch one original instruction each. Choose them to cover every constant, comparison, mask, store and
  call. An undetected mutant is either equivalent (replace it) or a coverage gap (bias the vectors).
- Paths behind several gates are rarely reached by random vectors. Add a biased mode that builds inputs passing the
  gates and sitting on the boundary values (for example a detour step at its wrap or HP at the rule's threshold).
- Compare stores by span. Where the original writes a whole word over several native fields, allow the combined span.
- Run one native broken variant per function:
  `differential.py <case> --no-mutants --vectors 1024 --replace <source> "<old>" "<new>"`. Each must mismatch.

### 5. Group check and ledgers

- Lint first, because the group check spends most of its time in tidy and stops a group for format or style alone.
  Format with the repository's clang-format
  (`.venv-3\Lib\site-packages\clang_format\data\bin\clang-format.exe --style=file -i <file>`), then run the fast
  pass `python tools\lint\quality.py --files <changed first-party files>` (format, cpplint, ruff and clones in
  seconds; differential drivers are not first-party and are rejected by `--files`).
- For new test worlds or callback-heavy cases, run a scoped `--files <test file> --tidy` preflight. The fast pass
  does not catch unchecked optionals or oversized assertion/callback expansions; do not discover those only
  after both full regressions have run.
- Write a ledger spec (record, slice, function notes, verification, gate, plan row text) and apply it before the group
  check, because the copier tests compare the catalog's `final_location` with `owned.csv`:
  `python tools\port\promote.py ledgers <spec.json> --summary build\promotion-check\<run>\summary.json`.
  The gate's before count is the old symbols' code references at the base commit; after is 0. The plan row's text
  replacement runs once, so to record the final numbers run `git restore Docs\ledgers` and apply the spec again with
  the final group summary.
- Run `python tools\test\promotion_check.py group --focused "[tag]"` once (measured at 15-18 minutes, of which
  changed-file tidy is about 12): copier stability, Debug and Release builds, both `regression.py check` runs with
  the canonical replays, focused tests, copier tests and changed-file tidy. Do not replay the CI workflow locally.
  Every new quality finding fails it, test files included (`readability-magic-numbers` in tests is reported
  separately). Changing a widely included header such as `Distance.hpp` widens the scope to every translation unit
  that includes it, so the existing-findings count grows (177 for the perception group) while new findings must
  still be 0. Avoid editing shared headers when a local helper would do.
- Copier tests that assert text of a removed generated copy must be moved to the typed source and live binding, not
  deleted.
- Re-run `python tools\port\test_encounter_fixups.py` and, if the interpreter changed,
  `python -m unittest test_differential` from `tools\port`.
- If a fix after the group check changes code, re-run the differential and `promotion_check.py iterate`; re-run the
  group check before publishing.

### 6. Record, publish and continue

- Keep per-function status in the canonical ledgers and the original
  [REMAINING_FUNCTIONS.csv](../Docs/ledgers/REMAINING_FUNCTIONS.csv). Keep commands, hashes, differential/mutant
  results, group-check results and limits in the group's machine-readable evidence. Do not duplicate that report
  into a prose handoff after every batch.
- Commit only the group's files. Write the message to a UTF-8 file without BOM and use `git commit -F <file>`;
  no attribution. Push, then `gh run watch <id> --exit-status`.
- While checks or CI run, prepare the next related group's source analysis, case and tests. Reuse prepared analysis
  and existing helpers; do not restart an audit without a concrete new correctness question.
- After CI passes, post a concise issue checkpoint with the commit, CI run, cohort counts and evidence location.
  Close an issue only when all its acceptance criteria are verified, then continue the next ordered group.
- Update [HANDOFF.md](../Docs/HANDOFF.md) at a package boundary, an owner pause or a genuine blocker, not every
  batch. Include the exact current revision, remaining count, next action and any unresolved limits.
- Retain referenced verification evidence; clean only disposable scratch intermediates.

## Stopping

When the owner asks to stop: finish or shelve the step in progress, commit and push any verified work with its limits
stated, update the HANDOFF "Stopping Point" with the exact state and the next step, and report the status.
