# Current Handoff

## Transient-tooling retirement (#440), 2026-11-18

Starting revision: `d654bad79`, clean worktree. Retired 255 campaign files:
historical naming inputs/outputs/report/duplicate decisions, 20 batch-specific
generators, three one-off scripts, and the completed 54-slice and 900-function
promotion trackers. Generic review tools remain; their scratch outputs are now
ignored. Shared layout/citation support and its tests moved out of `sym_gen`
into `tools\naming\symbol_support.py` and `test_symbol_support.py`.

The completed queue's scheduling checks are gone, not the canonical ownership,
promotion, behavior or compatibility-retirement gates. All retained native files
and canonical CSV ledgers match their pre-cleanup hashes. `FileMenuLive.cpp`
and other live bindings are unchanged. Only the 62 quality-baseline entries for
deleted tool paths were removed; all 4,995 retained entries are unchanged and
no new findings were accepted.

Local validation: 48 naming tests, 35 modernization tests, 226 encounter/copier
tests and two symbol-ledger tests passed. Modernization and gate checks report
zero violations; changed-source quality reports zero new findings. Both copier
runs reproduce all 486 pre-cleanup output hashes without touching owned files.
Two solution generations are stable. Native builds/replays were not repeated
for this tooling-only cleanup because every native source/header/project hash
is unchanged. Exact-revision CI closure evidence belongs on #440.

Evidence: `build\transient-cleanup\before.json`, `verification.json`,
`removed-baseline-entries.json`, `copy-1.log`, `copy-2.log` and the project/test
logs in the same directory. Retired scripts and snapshots remain in Git history.

## Owner-requested cleanup, 2026-11-18

- Promotion tooling retired in `cb1e04334`; do not run promotion checks.
- Changed-file CI: `185758389`, verified on follow-up `8cd4a839a` in run
  `44235628223`. Exactly two changed Python files were selected: source gate
  approximately 0.6 seconds, source job 53 seconds, whole workflow approximately
  103 seconds versus 407 seconds originally. #437 is closed. Rules/scope changes
  and manual dispatch retain full audits; partial clone checks compare only the
  selected files, not unchanged sources.
- LibDs trim: `cda4b6424`, CI `44236497503`. Six uncalled functions and unused
  declarations are removed. Two referenced compatibility hooks and three layouts
  remain; the sector-addressed loading adapter is still EC-04 debt.
- Portable game state and backup cleanup: `d47e9bc0b` (#439, #436). Debug and
  Release solution builds passed `[launcher],[disc-queue],[save]`: 187 tests,
  6,100 assertions in each configuration. These builds used the final native
  source later committed as `d47e9bc0b`. Changed-source quality and focused
  clang-tidy on `Setup.cpp` and `LauncherTests.cpp` reported no new findings.
  CI found the renamed first-launch test's old inventory name; the follow-up
  changes that single name, preserving all inventory entries and results.
  Exact-revision closure evidence is recorded on #439 and #436.
- The real default launcher migration moved 11,410 save/asset files into
  `build\Debug`, with every SHA-256 unchanged. Assets were reused without
  extraction; old AppData state and its obsolete asset backups are gone.
  Explicit-state and self-check probes created no AppData. A native run using
  the moved assets and a nonexistent disc path reached 1,500 ticks and produced
  its requested frame (`build\portability-smoke\frame_1500.bmp`). This verifies
  startup/loading, not combat or all gameplay.
- Development state: `8cd4a839a`, 208 Player tests and CI passed. 8,829 Player
  files moved to `build\player` and 3,631 archive files to
  `build\evidence-archive`; all hashes matched and SQLite integrity checked OK.
- Controller support remains SDL3 mapping-based; no new physical-controller
  certification was performed. Already-connected pad handover remains #438.

Local evidence is ignored under `build`: `portability-debug-final-build.log`,
`portability-release-build.log`, `portability-release-tests.log`,
`portable-first-launch.log`, `legacy-state-hashes.json`,
`player-migration-hashes.json`, and `evidence-archive-migration-hashes.json`.
Do not delete migrated saves, assets, Player state or retained evidence as build
cache cleanup. Explicit launcher/runtime path overrides remain supported.

Maintained guidance:
- [Native source port plan](NATIVE_SOURCE_PORT_PLAN.md)
- [Ledgers](ledgers/)
- [Ordered work tracker](https://github.com/JEschete/VS-NativePort/issues/4)
