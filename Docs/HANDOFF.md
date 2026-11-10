# Current Handoff

Updated 2026-11-09. The owner requested a verified stopping point and a clean
handoff. Do not start another implementation as part of this cleanup.

## Checkpoint

- Code revision: `f6ba998e902c41aa4de4ddbc86a141cbae3f0d37`
  (`Promote typed projected action result application`), pushed on
  `imp/01-ma04-gates`.
- Exact-revision [Source Quality run 42710468317](https://github.com/JEschete/VS-NativePort/actions/runs/42710468317)
  completed successfully. The code worktree was clean at this stopping point;
  this handoff cleanup is a separate documentation-only checkpoint.
- **977 functions remain.** Modernization: 2,767 done of 3,744. C++ promotion:
  2,768 promoted plus one frame-orchestrator WIP. These are separate measures.
- Original 3,479-function cohort: 2,486 promoted, 16 retired, 976 generated,
  one WIP; 2,502 resolved. This continuation resolved 53 more than `6c8ac0b9`.
- Plan row 27, field gimmicks/doors, is done for C++ ownership: its original
  51 IDs audit to 46 promoted and five retired. Row 28, battle AI, is active.
- Issues #325-332 are closed with fixing revisions and verified CI. No
  implementation or investigation from that group remains half-finished.
- No owned build, replay, quality run, or CI watcher remains active. The next
  condition-scoring functions have only been prepared read-only, not edited.

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

- F01637, `RefreshActionCandidateReadiness`, was published in `376c7969`;
  [CI 42705380792](https://github.com/JEschete/VS-NativePort/actions/runs/42705380792)
  passed. It preserves callback rereads, packed candidate bits, non-compacting
  first-survivor promotion, movement ordering, and the 16-bit threshold rules.
- F01697, `ApplySimulatedActionResults`, is the current code checkpoint. It
  applies cost types 1/3/6 first, then performer and ordered target effects.
  Repeated actor slots apply repeatedly; only operation 2 restores; status
  clearing follows setting. Producer evidence identifies HP, MP, weapon DP,
  and shield DP. Native decoding validates the complete flexible target tail
  and 255-target capacity, preserving row IDs, maxima, and reserved bytes.
- Both policies live in the existing
  [PlanningPrimitives.hpp](../Native/Game/include/Battle/Ai/PlanningPrimitives.hpp),
  [PlanningPrimitives.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp),
  and [PlanningPrimitivesLive.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitivesLive.cpp).
  Production callers and original C/module entries are retained. The
  obsolete function-local adaptations were removed through the copier.
- Borrowed outside-arena inputs, broader AI lifecycle, full battle-AI/M8
  gameplay, controller/reference parity, and EC/global/data/type retirement
  remain open. Local function promotion does not satisfy those wider gates.

## Verified

These are results for the code checkpoint above, not new runtime verification
performed by editing this document:

- Full rebuilt Debug and Release: **5,167 cases / 12,593,504 assertions each**;
  zero failures or skips. Canonical `silver_wolves` (2,400 frames) and
  `first_lever_room` (900 frames) match unchanged baselines; zero regressions.
- F01697: 11 focused cases / 95 assertions; combined native result/readiness
  selection: 18 cases / 126 assertions. F01637's original focused total was
  14 cases / 131,141 assertions.
- All 202 copier controls pass. Source-scoped quality has zero findings;
  full header-aware quality has five existing findings and zero new findings,
  actual exit 0. No suppression or baseline was changed to obtain this result.
- Both ledger gates pass with zero violations. Two final supported generation
  runs preserve all 1,761 reviewed paths and raw hashes. Manifest SHA256:
  `bd04d1ec83116ba434b186ddcdd9eec383405da7ba57ed4dd2f6b59b2bbaf8d1`.
- F01697's 134 instructions at `800DC888-800DCA9C` match actual BATTLE bytes:
  SHA256 `342ddca4ea84cdc5aa5bae9d02ee308546f3188b4234f01b3522177a1cd6e467`.
  F01637's 132 instructions at `800D85D8-800D87E4` are separately recorded in
  its verification ledger entry.

## Next Work

1. Recheck `git status`, HEAD, and the exact-revision CI before resuming.
   Preserve intervening owner edits; archived results do not verify a new tree.
2. Continue ordered row 28. The prepared next bounded group is F01700
   `ActorStatusConditionScore`, F01701 `ActorHpRatio`, and F01702 `ActorMpRatio`
   in [ActionPlanning.cpp](../Native/Game/src/Battle/Ai/ActionPlanning.cpp).
   This is a recommended next group, not a claim that it is already started.
3. Read the actual original `74088.c` routines at lines 565/590/598 and their
   matching assembly before implementing. Establish status masks, signedness,
   arithmetic/rounding, clamping, and zero-denominator behavior from evidence.
   Reuse `ActorCondition`, the existing planning owner, native bounds helpers,
   and [PlanningPrimitivesTests.cpp](../Native/Tests/src/PlanningPrimitivesTests.cpp).
4. Test the deciding core first, then native/caller/module bindings. Make
   generated changes through the copier or owned registry, never by patching
   generated output. Remove only adaptations made obsolete by the owned body.
5. Run focused checks after each change and shared full gates for the coherent
   milestone. Keep behavior verification, C++ promotion, modernization, and
   compatibility retirement distinct. Log a newly confirmed defect before fixing it.
6. Commit and push scoped verified milestones using the owner's configured
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
  match delivered scope. Do not declare the remaining 977 functions complete.
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
