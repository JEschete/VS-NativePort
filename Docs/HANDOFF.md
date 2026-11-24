# Current Handoff

## Named recipe domains (#499/#520), 2026-11-23

Base revision: `22b0be713` on `main`. The shared CSV schema/reader now supports
named enum domains with explicit numeric import aliases. Material rules, blade
categories, damage types and cost resources are written by name by the authoring
pass. Readers preserve the original numeric runtime values and normalize keys,
so mixed numeric/named aliases cannot create duplicate material rules.

The cost mapping is now traced end to end:
`EquipmentStats::BuildWeaponStatsFromParts` -> `WeaponLoadout.action_type` ->
`ActionCostsLive::Resources` -> `ActionCosts`: 1 MP, 2 Risk, 3 HP, 4 weapon PP,
or 0/0 for no payment. It is not `ActionDefinition::type`. Unsupported codes
remain losslessly decodable/re-encodable by the import codec but are refused
as authoring values; `none` requires zero cost.

Verification on this slice:
- Full Debug and Release solution builds passed (`VS_JOBS=4`, `/m:2`, x64).
- Debug authoring/label/recipe selection: **35 cases / 680 assertions**.
- Final Release authoring/label/recipe/catalog selection:
  **61 cases / 7,847 assertions**.
- Actual data: **12,897 numeric aliases** became their schema names across
  **15,499 recipe rows**. Every underlying code, quantity, identity and reference
  is unchanged. Repeating authoring changes zero files/values.
- Both Debug production replays match the unchanged baselines (2,400 and 900
  frames). Scoped tidy and changed-file quality report zero new findings.
- A seed-520 five-record cold read records semantic IDs, names, categories,
  damage, resource costs and stats in `build\issue-520-domains-cold-read.log`.
  It is only the known-domain check: remaining import-only/unnamed fields still
  block the full family readability gate.
- Evidence: `build\issue-520-domains-*.log`; original rows are retained locally
  in `build\issue-520-domains-before`.

Still open: the other catalog families, flags and numbered field families,
readable instruction/track structures, the authoritative string pipeline,
one-copy shared item definitions and remaining per-format adoption/verification.
No complete migration issue closes on this bounded step.

## Semantic recipe identity slice (#499/#520), 2026-11-23

Base revision: `a144ae986`, on `main`. This is the first section 4 identity
migration, not completion of the full authoring contract.

- `VS.Extract authoring <datadir>` now migrates generated recipe IDs and all
  directed references; `tables` and full `extract` also run it. Existing custom
  identities, labels and value edits survive. Schemas document legacy aliases,
  which readers normalize through `legacy_id`; ambiguous aliases and duplicate
  resolved directed pairs are errors.
- Real-data inspection found two Footman's Maces: categories 4 (Axe & Mace) and
  7 (Heavy Mace). The owner approved category-qualified IDs. They are now
  `blade.footmans-mace.axe-mace` and `blade.footmans-mace.heavy-mace`.
- The owner also approved quarantining the five stock, unreferenced `untitled`
  tail rows. They are no longer item definitions; their exact record bytes are
  in `catalogs/menu/import/combination-blade.json`, tracked in the conversion
  manifest. Only offline preservation export reads those bytes. Edited,
  referenced or explicitly named records are not silently discarded.
- The original shield/armor copies differ in gem slots for 15 records.
  `combineArmor` does not read model/gem-slot bytes, while `combineShields` does
  (upstream `src/MENU/MENUC.PRG/168.c:5017-5112`, native `Combination.cpp`).
  Their context-specific bytes remain exact; shared category/stats/legacy
  identity are checked, rather than forcing unequal records to be identical.
- Normal incremental table conversion retains the import file and authored
  edits. Missing tracked import provenance is refused unless the user explicitly
  requests source reconversion with `--force`.

**Evidence on the uncommitted slice based on `a144ae986`:**
- Full Release and Debug solution builds passed, `VS_JOBS=4`, `/m:2`, x64.
- Release final selection
  `[workshop-authoring],[asset-labels],[content-tables][workshop],[asset-catalog]`:
  **58 cases / 7,814 assertions passed**.
- Debug identity/recipe selection: **20 cases / 464 assertions passed**.
  An earlier Debug schema comparison used the old committed description; the
  schemas were refreshed from the native formatter and that selection passed.
- Both Debug production replays match their unchanged baselines after migration:
  `silver_wolves` 2,400 frames and `first_lever_room` 900 frames.
- Actual data: **189 semantic result IDs, 37,356 directed references**, and
  **five archived records**. Every retained result value/label and material table
  is unchanged; all directed pairs resolve to the same original result IDs.
  Archived bytes equal their exact source records. A repeated `tables` run
  converts **0** sources, keeps **3** recipe filesets and **256** zones, removes
  **0** files, and changes **0** IDs/references/labels.
- Scoped tidy and complete changed-file quality: **0 new findings**.
  Generated projects are stable on their second generation.
- Logs: `build\issue-520-authoring-*.log`, `build\issue-520-semantic-*.log`,
  `build\issue-520-import-*.log`; before-state data stays local under
  `build\issue-520-authoring-before`.

**Still open:** machine-readable named enum/flag domains, meaningful numbered
column families, authoritative readable text/markup, removal of remaining
import-only fields from authoring rows, and one-copy shared item definitions.
The field currently called `cost_type` needs its actual consumers traced; do
not equate it with `ActionDefinition::type` just because another intermediate
field is named `action_type`. Other asset families retain their section 7.2
readability and original runtime/preservation gates. #499/#520 stay open.

## Readable-label preparation and semantic authoring decision (#520), 2026-11-23

Base revision: `242ae3e2b` on `main`. The owner explicitly replaced the PR
workflow with scoped commits/pushes on `main`; that documentation-only commit
passed CI `45259252641`.

**Destination: ASSET_PIPELINE.md section 4, not labels over opaque dumps.**
The owner clarified this while the label pass was in progress. D26 now requires
semantic IDs, schema-backed named values/flags, meaningful fields/arguments and
explicit provenance for legacy identities/unused bytes. The new section 7.2
readability gate was appended to all 21 open asset-format issues. #520 was
expanded and remains open. The proposed table generator and charmap are now
accurately marked unimplemented; no new implementation is claimed for them.

**Preparatory code delivered**
- `VS.Extract labels <datadir> [--force]` and successful conversion commands
  add optional `display_name` metadata. CSV puts it second without shifting
  runtime semantic columns; JSON uses an early member and glTF uses
  `asset.extras.vs`. Other fields/IDs remain unchanged in this command.
- Bounded English names decode accents/punctuation, stop before terminator
  padding and retain unknown codes as visible markers. Item names label recipe
  results and parts; verified archetype references propagate shared aliases.
- Existing labels/author edits survive; `labels --force` refreshes only labels.
  Unknown fields and old numeric identities still present are migration debt.

**Measured local data evidence**
- All **21,709 rows in 15 CSV tables** preserve every original cell; the new
  label is column 2. Examples include Guildenstern, Muellenkamp Soldier (with
  the source accent), Battle Knife and Scramasax.
- **333 glTF and 2,751 JSON documents** are labeled. Across **13,525 non-disc
  files**, canonical non-label JSON/CSV values and every binary hash are
  unchanged. Schemas differ only by the optional annotation column.
- The repeat command reports **0 updated / 3,114 unchanged files**.
- No disc-derived data was committed. Native data was annotated only after
  rebuilding consumers and confirming no native application was running.

**Validation**
- Full Debug and Release solution builds passed (`VS_JOBS=4`, `/m:2`,
  `/p:Configuration=<Debug|Release> /p:Platform=x64`).
- Release `[asset-labels]`: **12 cases / 183 assertions**.
- The broader Release asset selection passed 132/133 cases; its only failure
  compared catalog text before/after optional annotations. The test now compares
  every non-label JSON field and verifies annotation types, not byte formatting.
  That exact case then passed **5,568 assertions**.
- Debug `[asset-labels],[asset-catalog]`: **38 cases / 7,350 assertions**.
- Both existing Debug production replays, run through `regression.run_replay`
  and `compare_replay`, match the unchanged baseline: `silver_wolves` 2,400
  frames and `first_lever_room` 900 frames. The full suite was not repeated for
  this annotation-only checkpoint.
- Scoped tidy and the mixed changed-file quality pass report **0 new findings**.
  The generated projects are stable on a second generation; all 15 CSV schemas
  were produced by the native formatter and are stable on repeated annotation.
- Evidence: `build\issue-520-*.log`, `build\issue-520-asset-before.json`.
  Exact pushed-revision CI is recorded on the issue after publication.

**Next:** implement the first actual section 4 authoring slice in workshop
catalogs: semantic result IDs/references and verified named domains. Preserve
directed combinations and original mappings, and classify unknown bytes from
their real consumers rather than guessing. This checkpoint does not close #520,
#499, #199 or any complete asset-format issue.

**Owner playtest feedback:** positive FMV/demo/rain/facial/manor-fire observations
are recorded on #149. New reports are #521 (VKP cigarette flicker), #522 (Debug
logging choppiness), #523 (window drag pauses playback). They are owner evidence;
tested revision and causes remain unverified. No speculative fixes were made.

## Converted actor-asset read failures (#518), 2026-11-23

Starting revision: `0530c5dc8` on `imp/asset-migration`, PR #519. The owner
finished the Debug playtest before Debug rebuilding and regression began.

The converted-read failure was logged in #518 before implementation.
`BeginFileRead` now distinguishes a started read, a full queue and a failed
read. Only a full queue can use the existing synchronous fallback, whose
result is also checked. Failed SHP, WEP, SEQ/ESQ and ETM reads free the
unpublished allocation from the correct heap end and return a failed request
before parsing or texture upload. A failed player sequence does not queue its
dependent weapon sequence.

Tests cover all four loaders, high/low sequence allocations, failed synchronous
fallback, recovery and live owned SHP/WEP ticket failure followed by a valid
queued request. The preview cancellation fixtures now load actual converted
models with the real event-stride table instead of relying on a missing store
being accepted as a zero-filled model. The shared model fixture reuses its
existing checked-result helper to satisfy tidy without weakening assertions.

Validation on the nine-file dirty changeset based on `0530c5dc8`:
- Debug and Release MSBuild `VagrantStory.Native.sln
  /t:VS_Tests;VS_CombatLab;VS_Desktop /m:2 /p:Configuration=<configuration>
  /p:Platform=x64 /v:minimal /nologo`, `VS_JOBS=4`: passed.
- `python tools\test\regression.py check --config Debug`: **9,896 tests /
  13,074,943 assertions; 2 replays; 0 regressions**. `silver_wolves` (2,400
  frames) and `first_lever_room` (900 frames) match the unchanged baseline.
- Final Release `VS.Tests.exe
  "[actor-assets],[asset-ownership],[actors-spawning],[encyclopedia-preview],[actors][sequence],[actors][texture],[content-models]"
  --reporter compact`: **302 cases / 19,405 assertions passed**.
- Scoped quality/tidy on the changed loader and test translation units found
  two test-size findings; both were corrected. The affected translation unit
  was rechecked with tidy: **0 new findings**. No baseline was changed.
- Evidence: `build\issue-518-debug-build.log`,
  `build\issue-518-debug-regression.log`,
  `build\issue-518-release-final-build.log`,
  `build\issue-518-release-final-tests.log`,
  `build\issue-518-release-final-console.log`, and
  `build\issue-518-quality{,-retest}.log`.

This also supplies fresh Debug regression evidence for the preceding #48
request-state milestone. #48 still has the limits below; neither change
completes asset modernization. Exact fixing-commit/branch-CI evidence belongs
on #518 and PR #519 after publication. The PR closes #518 only on owner merge.

## Per-request actor parser state (#48), 2026-11-23

Starting revision: `23279779c`, clean worktree. `imp/asset-migration` was
fast-forwarded from the merged branch to `origin/main`; work continues through
an owner-reviewed PR, not a direct push to `main`.

- Actor, weapon and sequence parser phases, buffers and cursors now live with
  each queued request's identity and record-memory resolver. Queue movement,
  cancellation and clearing carry or reset them together.
- Module parser globals are output-only diagnostic snapshots for those loaders.
  Tests overwrite the snapshots before and during a real preview parse and
  before cancellation: the parse still finishes or frees its partial geometry.
- The battle initializer relies on the queue reset rather than clearing
  individual parser globals.
- This is not #48 completion: the unowned ETM phase/buffer, serialized file
  streams, in-band shared-record index, unowned producers and deliberate
  immutable sharing remain. G00064's ledger note records a write-only snapshot,
  not compatibility retirement.

Validation on the seven-file dirty changeset based on `23279779c`:
- `VS_JOBS=2`; MSBuild `VagrantStory.Native.sln /t:VS_Tests /m:2
  /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`: passed.
- `build\Release\VS.Tests.exe
  "[actor-assets],[asset-ownership],[actors-spawning],[encyclopedia-preview],[actors][sequence],[actors][texture]"
  --reporter compact`: **240 cases / 2,197 assertions passed**.
- `quality.py --files` the changed `ActorAssetsLive.cpp/.hpp`,
  `ModelRenderingLive.cpp` and `EncyclopediaPreviewTests.cpp`: **0 new findings**.
- Evidence: `build\issue-48-release-build.log`,
  `build\issue-48-release-tests.log`, `build\issue-48-quality.log`.

At that checkpoint the owner was playtesting the Debug build. Its binaries,
logs and `Native\data` were not changed; Debug builds and gameplay replays
were deferred. Fresh integrated Debug evidence is now recorded above.
The latest starting CI
`45208337293` passed both jobs; exact-revision branch CI belongs in the issue
checkpoint after publication.

Follow-up: #518 records a source-confirmed error-propagation defect found while
tracing these loaders: `BeginFileRead` ignores a converted read's failure and
lets zero-filled content continue. Its fix and verification are recorded above.

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
