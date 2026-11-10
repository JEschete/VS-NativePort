# Current Handoff

Updated 2026-11-09. Row 28 (battle AI) resumed after the cleanup checkpoint.
The condition-term group below is implemented and verified; once published, its
hash and exact-revision CI belong in #276 and the plan row, not guessed here.

## Checkpoint

- Base revision: `90e5fbfe6d5559e0e79591db722eae2f4dc48092`, the documentation
  cleanup over code checkpoint `f6ba998e` (F01697,
  [Source Quality run 42710468317](https://github.com/JEschete/VS-NativePort/actions/runs/42710468317)).
  Three scoped commits follow it on `imp/01-ma04-gates`, in this order: the CI
  exit-code fix for #333, the F01700-F01703 code checkpoint, then the #335
  quality-gate fix. Use `git log` and #276 for their hashes and exact-revision
  runs. If `git status` still shows them uncommitted, commit them in that order;
  each later change was verified on top of the earlier ones.
- **973 functions remain.** Modernization: 2,771 done of 3,744. C++ promotion:
  2,772 promoted plus one frame-orchestrator WIP. These are separate measures.
- Original 3,479-function cohort: 2,490 promoted, 16 retired, 972 generated,
  one WIP; 2,506 resolved. This continuation resolved 57 more than `6c8ac0b9`.
- Plan row 27, field gimmicks/doors, is done for C++ ownership: its original
  51 IDs audit to 46 promoted and five retired. Row 28, battle AI, is active.
- Issues #325-332 are closed with fixing revisions and verified CI. #333 and
  #335 stay open until their fixing revisions have a real green run (see Latest
  Work). One more quality defect found while verifying is logged and **not yet
  fixed**: #334, the full-scope `quality.py --tidy` gate fails at `HEAD` with 23
  un-baselined findings in five untouched files.
- No owned build, replay, quality run, or CI watcher remains active. The next
  group (F01698, F01699, F01704) has only been located read-only, not edited.

## Sources Of Truth

Follow the owner's decisions in [NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md)
and the ordered [#4 tracker](https://github.com/JEschete/VS-NativePort/issues/4).
[#276](https://github.com/JEschete/VS-NativePort/issues/276) and
[PROMOTION_PLAN.csv](ledgers/PROMOTION_PLAN.csv) track the promotion work.
[FUNCTION_CATALOG.csv](ledgers/FUNCTION_CATALOG.csv),
[CPP_PROMOTION.csv](ledgers/CPP_PROMOTION.csv),
[VERIFY_LEDGER.csv](ledgers/VERIFY_LEDGER.csv), and
[MODERNIZATION_LEDGER.csv](ledgers/MODERNIZATION_LEDGER.csv) retain detailed
per-function evidence. Do not duplicate those ledgers in this file.

The entire previous handoff was preserved verbatim in
[HANDOFF_HISTORY.md](HANDOFF_HISTORY.md), under
`Archived Handoff: 2026-11-09 / f6ba998e`. Its old current/next/pending statements
are historical, not instructions or fresh verification.

## Latest Work

- F01700-F01703, the four condition terms inlined into `EvaluateActionCandidate`,
  are the current code checkpoint. `ActorStatusConditionScore` sums the signed
  weight of each set status bit 0-31; bit 12 counts only when the row's own AI
  state enables action cost. It then adds -128 for zero HP and -84 for HP below
  44/128 of maximum, comparing signed 32-bit products. `ActorHpRatio`,
  `ActorMpRatio` and `ActorEquipmentDpRatio` return `value*128/max`, truncated
  toward zero and not narrowed to 16 bits, zero without a maximum; the equipment
  term is weapon plus shield. The original looks the AI state up by the row's own
  id, not the loop slot, and the native binding does the same.
- The original 99 inlined instructions at `800DCDD4-800DCF5C` were interpreted
  from the actual BATTLE bytes against the real native cores over 20,000 vectors
  (the exact 44/128 boundary, zero maxima, bit 12 on and off, negative HP and
  results past 16 bits) with zero mismatches. The harness stays out of the
  repository because it derives from disc data.
- #333: CI had reported success while `hazards.py --check` failed on a clean
  `HEAD` export. `HAZARD_LEDGER.csv` was last synced in `34cdf8b7`, and a
  multi-line PowerShell `run:` step reports only its last native command's exit
  code, so the later `regression.py static` hid it. Each former multi-command
  step in `quality.yml` is now one command, [test_workflows.py](../tools/lint/test_workflows.py)
  rejects any multi-command step, and the ledger was resynced (763 to 746 sites).
  Thirteen new sites were recorded `open` and are untriaged; 30 rows left with
  rewritten code, all `open` with no notes.
- #335: `quality.py --files X --tidy` silently dropped every diagnostic outside
  the listed files, so a finding in a header reached through the listed source
  printed `0 findings` and exited 0. `main()` aliased `selected = scope` and then
  intersected in place, shrinking the scope that bounds `check_tidy`; it now
  copies the set. `--changed` and full runs were never affected. The new
  `test_selected_files_still_report_header_findings` puts a magic number in a
  header, selects only the including source, and requires that header finding
  and exit 1. It does not repair #334's findings, and whether this defect left
  four of those five files un-baselined is unconfirmed.
- F01637, `RefreshActionCandidateReadiness`, was published in `376c7969`;
  [CI 42705380792](https://github.com/JEschete/VS-NativePort/actions/runs/42705380792)
  passed. It preserves callback rereads, packed candidate bits, non-compacting
  first-survivor promotion, movement ordering, and the 16-bit threshold rules.
- F01697, `ApplySimulatedActionResults`, was the previous code checkpoint
  (`f6ba998e`). It applies cost types 1/3/6 first, then performer and ordered
  target effects. Repeated actor slots apply repeatedly; only operation 2
  restores; status clearing follows setting. Producer evidence identifies HP,
  MP, weapon DP, and shield DP. Native decoding validates the complete flexible
  target tail and 255-target capacity, preserving row IDs, maxima, and reserved
  bytes.
- These policies live in the existing
  [PlanningPrimitives.hpp](../Native/Game/include/Battle/Ai/PlanningPrimitives.hpp),
  [PlanningPrimitives.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp),
  and [PlanningPrimitivesLive.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitivesLive.cpp).
  Production callers and original C/module entries are retained. The
  obsolete function-local adaptations were removed through the copier; the
  generated F01704 caller now calls the owned condition bindings.
- Borrowed outside-arena inputs, broader AI lifecycle, full battle-AI/M8
  gameplay, controller/reference parity, and EC/global/data/type retirement
  remain open. Local function promotion does not satisfy those wider gates.

## Verified

These are results for the working tree on base `90e5fbfe` that holds the three
pending changes (the F01700-F01703 code, the #333 CI fix and the #335 quality
gate fix), not new runtime verification performed by editing this document. The
#335 change touches only `tools/lint`; the build, test and replay results below
were taken before it and do not depend on it. Exact-revision CI is pending until
the commits are pushed.

- Full rebuilt Debug and Release: **5,177 cases / 12,593,621 assertions each**;
  zero failures or skips. Canonical `silver_wolves` (2,400 frames) and
  `first_lever_room` (900 frames) match unchanged baselines; zero regressions.
- F01700-F01703: 10 focused cases / 117 assertions (seven pure-core, three
  live-binding); `[ai][planning]` 86 cases / 137,725 assertions; `[ai]` 525
  cases / 3,131,567 assertions. F01697 was 11 cases / 95 assertions.
- Original MIPS evidence from actual BATTLE bytes: the 99 inlined instructions
  at `800DCDD4-800DCF5C`, SHA256
  `6506a43a5c906fd492cbb9c09cc37ef7510d4a206c026a9b021cfe5badb1d50c`; all of
  F01704, 219 instructions at `800DCC94-800DCFFC`, SHA256
  `f19b68e0b0c641f6fa5a5a53bcdb015009a69dbaadf03a87cc221813fe92237f`; the
  32-byte status table at `0x80069BD0`, SHA256
  `5d33fbf3c199b41f6535b3a0331f542b1e430fb6acff5f0dc218f054a01ea38a`. The
  20,000-vector differential run had zero mismatches.
- All 202 copier controls pass. Changed-scope `quality.py --changed --tidy`:
  48 findings, all baselined, zero new, exit 0; source-scoped quality: zero
  findings. **Full-scope `quality.py --tidy` exits 1**: 267 findings, 23 new,
  all in five files this work does not touch (`NativeContainer.hpp`,
  `SphereTerrain.hpp`, `RadialWarp.hpp`, `Session.cpp`, `Extract/main.cpp`) and
  identical on a pristine export of `90e5fbfe`; this work adds none (#334). No
  baseline or suppression was changed.
- `gates.py`, `modernization.py` and `hazards.py` pass `--sync --check` with
  zero violations (746 hazard sites). `test_quality.py` (31) and
  `test_workflows.py` (5) pass. Two final supported generation runs preserve
  all 1,803 hashed paths (tracked or untracked-not-ignored files under
  `Native`, `tools/port`, `tools/scaffold`, `Docs/ledgers` and the solution;
  this is not the definition behind the older 1,761-path record). Manifest
  SHA256: `df4fcfc298dc8be0cceb20fc04ce96638da67aa0b496c9b38a5de9c808248e9b`.
- #335: the new positive control fails on a pristine `90e5fbfe` export (every
  stage reports 0 findings, exit 0) and passes on the fix. `quality.py --files
  Native/Game/src/Save/NativeContainer.cpp --tidy` printed `0 findings` before
  and now prints the 19 header findings and exits 1. `quality.py --changed`
  reports 0 findings and 0 new; `baseline.json` is byte-identical.

## Next Work

1. Recheck `git status`, HEAD, and the exact-revision CI before resuming.
   Preserve intervening owner edits; archived results do not verify a new tree.
2. Repair the logged maintained-code quality defects as separate scoped
changes. Publish the three prepared changes in order, confirm each
exact-revision CI run, then close #333 and #335 with their fixing commits and
evidence. Then repair the 23 findings in #334: fix the code, do not refresh
the baseline; `--files <source> --tidy` now reports header findings, so use it
to iterate. Quality work precedes the next promotion group.
3. Continue ordered row 28. The prepared next bounded group is F01698
   `SimulateActionCandidate` (`func_800DCAA0`), F01699 `UpdateCandidateHitWeight`
   (`func_800DCBD8`), and F01704 `EvaluateActionCandidate` (`func_800DCC94`) in
   [ActionPlanning.cpp](../Native/Game/src/Battle/Ai/ActionPlanning.cpp).
   F01704 already calls the owned condition terms. This is a recommended next
   group, not a claim that it is already started.
4. Read the actual original `74088.c` routines at lines 513/539/620 and their
   matching assembly (`800DCAA0`, `800DCBD8`, `800DCC94-800DCFFC`) before
   implementing. Establish candidate-weight arithmetic, rounding, clamping,
   signedness, and the order of the condition terms from evidence. Reuse
   `ActorCondition`, the existing planning owner, native bounds helpers,
   and [PlanningPrimitivesTests.cpp](../Native/Tests/src/PlanningPrimitivesTests.cpp).
5. Test the deciding core first, then native/caller/module bindings. Make
   generated changes through the copier or owned registry, never by patching
   generated output. Remove only adaptations made obsolete by the owned body.
6. Run focused checks after each change and shared full gates for the coherent
   milestone. Keep behavior verification, C++ promotion, modernization, and
   compatibility retirement distinct. Log a newly confirmed defect before fixing it.
7. Commit and push scoped verified milestones using the owner's configured
   identity, inspect CI on the exact revision, and close eligible issues only
   with their fixing commit and acceptance evidence. Update #276 promptly.

## Commands

Use native Windows PowerShell, serial builds, and bounded operations.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' VagrantStory.Native.sln /m:1 /p:Configuration=Debug /p:Platform=x64
& .\.venv-3\Scripts\python.exe tools\test\regression.py check --config Debug
& .\.venv-3\Scripts\python.exe -m unittest discover -s tools\port -p test_encounter_fixups.py
& .\.venv-3\Scripts\python.exe tools\lint\quality.py --changed --tidy --jobs 1
& .\.venv-3\Scripts\python.exe tools\ledger\gates.py --sync --check
& .\.venv-3\Scripts\python.exe tools\ledger\modernization.py --sync --check
& .\.venv-3\Scripts\python.exe tools\ledger\hazards.py --sync --check
& .\.venv-3\Scripts\python.exe tools\port\copy_to_native.py
& .\.venv-3\Scripts\python.exe tools\scaffold\gen_solution.py
```

Build and run Release separately. `regression.py check` does **not** rebuild.
Use supported generation twice and compare reviewed source/output hashes.
Run `hazards.py --sync --check` after any source change: CI runs `--check`, and a
stale `HAZARD_LEDGER.csv` fails it (#333). `--sync` keeps each row's triage status
and adds new sites as `open`; triage them separately.
Passing a baseline, compiling, or returning success alone is not gameplay proof.

## Guardrails

- Keep `resources/rood-reverseWin` and upstream references read-only. Source
  listings and actual instruction bytes/consumers establish behavior, not names.
- Never weaken requirements, expected outputs, tests, or quality baselines to
  match delivered scope. Do not declare the remaining 973 functions complete.
- Keep disc assets, BIOS, captures, dumps, logs, and local scratch out of commits.
  Do not force-push, rewrite history, or add attribution trailers/headers.
- Avoid full gameplay JSON, huge terminal histories, and unbounded parsers.
  Pinned clang-format is required. Header-aware tidy can fan out widely;
  use source-only focused checks between milestones, then the full required gate.
- The editor-memory detour was explicitly stopped by the owner. Workspace
  Python-analysis limits remain; Java/Gradle were not disabled. The temporary
  accessibility override was removed. Do not resume that detour as C++ work.
- [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md) contains old failed attempts and
  evidence paths. Consult it only for a specific question; do not reload it wholesale.
