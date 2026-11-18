# Current Handoff

## Connected-controller handover (#438), 2026-11-18

Starting revision: `aa41806e6`, clean worktree. The active-controller removal
handler now closes the old pad, resets its rumble cache, enumerates SDL's
remaining gamepads and opens the first available one. Initial connection and
handover share the same open/logging path; enumeration/open failures are logged.
Inactive-controller removal does not change the selected pad.

The two initial regression cases failed on the old implementation: input and
rumble were not handed over. Final SDL virtual-device tests cover real queued
removal events after both ADDED events have already been consumed, replacement
buttons/stick/edges, inactive removal, last-pad removal, reconnect, unchanged
rumble levels on the replacement and rumble shutdown. The existing removal test
now actually detaches its device: SDL no longer delivers rumble callbacks after
detachment. Connected-pad shutdown still checks the zero-motor callback.
Automatic fixtures allow only their synthetic VID/PID so rescans cannot select
or rumble a physical controller; the opt-in hardware test remains separate.

Validation on the uncommitted changes subsequently committed for #438:

- MSBuild `VagrantStory.Native.sln /t:VS_Tests;VS_Desktop;VS_CombatLab /m
  /p:Configuration=<Debug|Release> /p:Platform=x64 /v:minimal /nologo` passed
  in both configurations. After test-only refinements, rebuilt
  `Native\Tests\VS.Tests.vcxproj` with the same configuration/platform options.
- `build\<Debug|Release>\VS.Tests.exe "[input]" --reporter compact` passed
  **62 tests / 797 assertions** in each configuration.
- `quality-native.cmd --files Native\Engine\src\Input\Pad.cpp
  Native\Engine\include\Input\Pad.hpp Native\Tests\src\VibrationTests.cpp
  --tidy` passed with **zero new findings**, including header dependents.

Evidence: `build\issue-438-before-tests.log`,
`build\issue-438-<debug|release>-build.log`,
`build\issue-438-<debug|release>-final-build.log`,
`build\issue-438-<debug|release>-tests.log`, and
`build\issue-438-quality.log`. Exact-revision CI/closure evidence belongs on #438.
No new physical-controller model or gameplay certification is claimed.

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
  certification was performed. Already-connected pad handover is covered by
  the #438 checkpoint above.

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
