# Current Handoff

## E207 reference mode (#507) and ZND zone tables (#517), 2026-11-23

Starting revision: `68c317d04`, clean worktree (the `resources/rood-reverse`
submodule checkout is at `f6bf8402`, not the recorded `c9a869b8`; it was left
alone, so a local `provenance.py --check` reports stale citations that CI does
not).

**#507 (closed, `58c6d497c`)**
- The MAP001 window glow is not drawn too faintly. The DuckStation reference
  was captured in its default True Color mode, which blends at 8 bits. E207's
  ~70 additive layers of a dark FBC palette (vertex colour 63, dither off)
  truncate to almost nothing in a 5-bit frame, on hardware and in the port.
- A replay of the traced primitives reproduces the native frame exactly in 5
  bits and the reference's yellow-green glow at 8 bits.
- The reference setup now uses DuckStation's software renderer with unscaled
  dithering. A new hardware-mode capture of `build\ds_map001` was not made:
  the capture script sends system-wide keys.

**#517 (slice of #199)**
- `bde93eb06` adds the lossless ZND codec. The follow-up adds the zone and
  enemy CSV tables (`Content/ZoneTables.hpp`) and the `znd-tables` converter
  (`VS.Extract tables`, now after `models`).
- The zone texture images live under `textures\zone\`. The room loader's zone
  read rebuilds each zone from the tables (`Content/ConvertedZones.hpp`).
  `EncodeTim`, `OriginOf` and `TimFromImage` moved from VS.Extract to
  VS.Content so the runtime rebuild and the importer share them.
- The launcher pipeline version is 9. ASSET_PIPELINE.md §6.9 has the format
  and limits.
- **Evidence:**
  - All 256 zones convert, rebuild and load back as their exact bytes.
  - A native New Game loads ZONE001 from the tables at tick 1385.
  - With the disc read forced, both replays match the old baseline.
    Converted, every frame after frame 39-40 equals the disc run shifted by
    2 frames. Baselines re-recorded: Debug and Release 9,889 tests /
    13,074,767 assertions, 2 replays, 0 regressions.
- The relational species, variant and loot tables, page depth and palette
  binding, and production enemy-edit evidence remain open on #199.

## Actor request ownership and identity (#48/#55), 2026-11-22

Starting revision: `ff4b3713d`, clean worktree. Commit
`fd50525fad2609929a326acc3818c663982b4712` extends the generation-checked
actor identity into room-enemy asset loading. Each ownable request is ticketed
to that actor; the spawn waits for all results before publishing it, cancels
queued requests when a later submission fails, and releases partial model
resources on load or publication failure. The actor owner is an opaque ID, not
a fixed enum. Follow-up `42a110beb` handles cancellation refusal by draining
the owned requests to completion before releasing the unpublished actor.
These are bounded producer-to-publication slices, not completion of either
issue.

Debug and Release passed `[actors-spawning]` (49 cases / 437 assertions),
`[asset-ownership]` (13 / 132), and `[encyclopedia-preview]` (43 / 684).
`quality.py --since origin/main` reported 0 new findings before the push.
Ledger gates, modernization, hazards, headers and static regression checks
reported 0 violations. Exact-revision Actions run
[45014081945](https://github.com/JEschete/VS-NativePort/actions/runs/45014081945)
passed both Source Quality and Tool and ledger controls.
The cancellation-refusal follow-up passed both jobs in exact-revision run
[45014737849](https://github.com/JEschete/VS-NativePort/actions/runs/45014737849).

The follow-up `quality.py --files Native\Game\src\Actors\ActorSpawning.cpp
Native\Tests\src\ActorSpawningTests.cpp --tidy --jobs 1` passed with 0 new
findings. An earlier serial tidy scan including the changed shared headers found
two diagnostics in unchanged `AnimationControl.cpp` functions about swappable
integer parameters; no new tidy findings were in the edited files. The ordinary
changed-source quality check passed.

Later slices: `ac397a138` owns scripted model requests; `4e1de0464` removes
actors by their instance definitions and releases each claim once. The
ticketed-load slice (`Actors/ActorLoads.hpp`) tracks every actor-owned load
until it ends: `SettleLiveActorLoads` runs each frame after the queue update
and logs each outcome, and removal or the spawn drain releases the actor's
tickets. The untracked owned-request API is retired. Release door run
(`--door-from z009-r000-d000 --door-limit 20`) matched the 15 passed / 5
blocked baseline with 20/20 teardowns, 0 engine errors and 45 settled loads
with no failed, lost or prematurely released tickets.

Record-memory slice: an owned request names no memory.
`SubmitActorAssetRequest` refuses an owned request with a destination; the
owner passes an `ActorRecordMemory` resolver with each live submission, which
the queue keeps per request and calls at bind time with the stored owner. A
spawned actor binds only its own body, second body, weapon and shield records
(`OwnedActorRecordPart`, generation-checked); the enemy preview resolves its
model buffer parts (`EnemyPreviewRecordMemory`); the equipment menu's reloads
bind through Ashley's handle. A kNone memory is a counted bind refusal, so the
load fails visibly. The Release door run again matched 15 passed / 5 blocked,
20/20 teardowns, 0 engine errors and 85 settled loads. Still open for #48
criterion 2: the head-owned `ActorAssetLoadState` parser state is global, the
shared record index rides in `request.data`, immutable sharing waits on
#203/#205, and the unowned menu `ActorPreview` and gimmick model requests
still name destinations.

Shape slice (#55): native readers of the actor's allocation kind word (0x40)
now read the spawn definition (`LiveActorShape`, installed or pending): the
action presentation's bystanders, AI special-actor registration and the AI
creation's actor record lookup. The spawn host still writes the word for the
PS1 layout; no native code reads it. An actor the spawn host did not allocate
has no shape, so it is no bystander, takes no special slot and has no record.
`InstallLiveActorInstance` defines actors whose storage the host does not own.

Both #48 and #55 remain open. Original global waits (`ScriptWaitForObjectLoad`,
the equipment menu's queue-busy check) stay as original timing; banks and
material sequences stay unowned; broader actor/resource migration (slot
addressing, the combined `ActorRecord`) remains. No hardware verification is
claimed.

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
