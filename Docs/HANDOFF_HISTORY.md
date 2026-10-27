# Historical handoff evidence

Archived on 2026-10-21 from the local worktree based on
`30f5369504d9438bc15b91f5a440383a4c6fba44`, including its uncommitted audio
checkpoint. The original handoff follows unchanged. Its "current", "next",
open/closed and validation statements describe their individual checkpoints,
not necessarily the latest worktree.

Use [HANDOFF.md](HANDOFF.md) for the current checkpoint and
[#4](https://github.com/JEschete/VS-NativePort/issues/4) for work order.
Keep the commands, revisions, limits and local evidence paths below; archival
does not establish fresh verification or authorize deleting the referenced data.

## Final seven assembly-gap resolution (2026-10-26)

The resumed base was `15d290bc`; during the pause the owner published
`506f750be767d159e537b6c937468aca6a205f3d`, including the verified room APIs
and their unrelated asset/solution/reference edits. That history is retained,
not rewritten. Its exact Source Quality40425198955 succeeded. F01805/F01806
have exact green40421506975 at15d290bc.

F00745/F00747 now share the actual renderer in RoomDraw.c. Native bounded
batch/frame spans and explicit projection outputs replace register inputs
and pipeline lookahead without reading past the final polygon. Existing
quarter-depth, culling, material, subdivision and texture-LOD behavior is
preserved. M2C output and diagnostics for all three assembly-only entries
remain in ignored `build\quality\issue42-final`. `.nop` was normalized only
in a local assembly copy; GTE operations still required instruction review,
and M2C cannot infer F00766's executable-data jump table.

The room milestone passes batch3/140 and coupled30/726,890 assertions;
full Debug/Release998/9,506,403 each. Truncation and overlap controls exit3.
Focused tidy has1 existing/0 new; default quality0 new;130 port controls.
Two generations are identical across1,028 outputs and preserve117 owned
paths. Before/after isolated normal-speed1800-tick New Game replays reach
MAP001 with0 errors and complete tracing. Their frame1790 BMPs both hash
`F2309B847B77D40C32B31A2BF69DC230670251F996075BB3AEB56917E029BE01`.
This is selected-frame/component evidence, not all-room visual parity.

The final three entries are resolved as follows:

- F01769 retains original `7BDB4.s:7-16` elapsed-unit/16-bit-wrap semantics,
  backed by native steady-clock frame-work duration. The baseline resets
  after each host yield, before callback work. The15,734 units/second match
  the read-only PC reference; imported AI budget490 remains unchanged.
  Tests cover sub-unit boundaries,65,537 whole-second samples, very large
  durations, real elapsed work and VSync baseline reset. This does not claim
  deterministic wall-time-independent AI or retire all scheduling bridges.
- F03575's original `overlay.c:6` stack capture has no native consumer:
  Boot_RunSessionLoop already replaces the original capture/jump paths.
  Remove the generated stub/prototype and original symbol registration.
  Repeated Title/Battle/Title C-boundary tests verify32 complete cycles;
  no host stack pointer or no-op replacement is stored.
- F00766's `2EA3C.s:1770` jumps to sectionE+20. There are no original-source
  or native callers and no aligned absolute/j/jal references in the scanned
  executable/PRG/BIN files. Every one of512 MPDs has zero sectionE length,
  including91 short64-byte room headers (the first421-only inspection
  unnecessarily excluded these; the final audit covers them).
  Retire the unused stub/prototype/module registration, rather than invent
  a successful executable-data dispatcher. Per-file hashes and lengths
  remain in `section-e-complete-audit.json`. Future authored executable
  payloads are not supported or authorized by this retirement.

Both retirements retain original catalog identity/provenance and explicit
`copy=no`/`copied=retired` states. The audit reports them as excluded, not
implemented bodies. The original186 cohort is184 implementations plus2
justified retirements. Thirty SDK/library assembly labels remain outside
the game-entry cohort. This is not a zero-compatibility or whole-game claim.

Fresh final validation on dirty506f750b plus this scoped work:

- `test-native.cmd Debug "[frame-budget]"` and direct
  `build\Debug\VS.Tests.exe "[frame-budget],[retired-entry],[session-reset],[scripted-override]"`:
  2/65,546 and20/262,416 respectively. The wrapper's earlier multi-tag
  attempt split its filter and ran no tests; only direct combined runs count.
- Full `build\Debug\VS.Tests.exe` and `test-native.cmd Release` each pass
  1,001 cases/9,572,047 assertions.
- Port unittest discovery131, naming32, native-view check621/0 mismatches.
  Two generations preserve119 owned paths and repeat1,028 outputs.
- Focused `quality-native.cmd --files ... --tidy` has1 existing/0 new;
  the full `quality-native.cmd --tidy` is still running at this checkpoint.
  Existing Catch macro identities were preserved by appending new tests,
  rather than renumbering old generated test functions or changing baselines.
- Frozen Debug/Release hosts under `final-Debug`/`final-Release` use private
  `--save-dir`, `--scripted --speed 1 --frames 6000 --press
  1000:Start,1300:Circle`. Sessions1793061408406775-0/
  1793061408406128-0 reach MAP001 at1415, execute4,586 scripted-control
  frames, and end with exit0/engine_errors0/trace_failed=false.
  Earlier Up1200 attempts stayed on title because the private save folders
  were empty; those clean exits are explicitly not room-entry evidence.
- Real Lab AI runs use existing
  `build\Release\combat-lab\runs\1792906154560166-0\encounter-1.json`,
  normal speed1800 ticks and controller presses140/220/280/800/880:Circle.
  Debug1793061567198447-0/Release1793061567197798-0 both exit0 with complete
  tracing and no engine errors,1,566 ordinary-control frames, live target0,
  and enemy movement from[1088,0,576] toward Ashley. No combat-result/damage
  acceptance is claimed. The first stale last-scenario attempted an
  unavailable zone1 hostile and was correctly rejected; it is not acceptance.

All local evidence is under ignored `build\quality\issue42-final`; no
disc-derived data or captures are committed. Final publication, exact CI,
full-tidy result and #42 closure are separate remaining checkpoint gates.

## Scripted-navigation draft verification (2026-10-26)

Resumed from `d692407e08e462fe4048401ee7802c91c5ae55e7` with the owner's
existing F01805/F01806 draft and unrelated asset-pipeline, solution, ignore
and reference edits present. The draft's native implementations, callers,
original module bindings, generator ownership, field alias and tests were
reviewed against `7C7E0.s:216-421` and the read-only PC reference. The original
repeat-request path copies onto the base state rather than the old route;
this is deliberately retained. Guest pointer tag bits and scratch-stack
switching are not applied to native pointers.

Fresh commands/results:

- `test-native.cmd Debug "[scripted-override],[navigation-override],[route-availability]"`
  built all hosts, but the wrapper split the selection: no tests ran.
  This attempt is not counted as test evidence.
- `build\Debug\VS.Tests.exe "[scripted-override],[navigation-override],[route-availability]"`
  passes19 cases/197,756 assertions. The middle tag has no matching cases;
  the scripted-override and route-availability selections ran.
- `build\Debug\VS.Tests.exe` and `test-native.cmd Release` each pass
  993 cases/9,506,238 assertions.
- Hidden `[.override-actor-identity]`, `[.override-missing-map]` and
  `[.override-cyclic-list]` each emit the intended diagnostic and exit3.
  An earlier misspelled cycle selector ran no tests and is excluded.
- `quality-native.cmd --files Native\Game\src\Battle\Ai\NavigationPrimitives.cpp
  Native\Tests\src\NavigationOverrideTests.cpp --tidy` and the default
  `quality-native.cmd` both report0 findings/0 new.
- Port unittest discovery passes130; naming discovery passes32;
  `tools\port\check_native_views.py` checks621 fields with0 mismatches.
- Two `copy_to_native.py` runs produce identical hashes across1,030 native
  source/header files, preserving the60 distinct owned implementation paths
  and the unrelated solution/ignore/asset-pipeline files.

The matching Debug executable/PDB/DLLs were copied to the isolated ignored
`build\quality\issue42-final\nav-native` directory. Actual arguments were
`--disc <local cue> --data <Native\data> --scripted --speed 1 --frames 6000
--press 1000:Start,1200:Up,1300:Circle`. The GUI launch initially returned
before process termination; the existing PID was awaited, not relaunched.
Session1793057955081624-0 reaches zone1/room0 at1415, executes4,586 frames
in scripted-control mode3 and ends at6000 with exit0,0 engine errors and
`trace_failed=false`. No live owner process or log was modified.

Evidence stays under `build\quality\issue42-final`. This verifies the
component and production-loop slice, not all-scene movement, visual parity
or compatibility retirement. F01769's timing dependency remains separately
open. The audit has181/186 implemented entries and5 gaps; issue checkboxes
still require publication and exact-revision CI.

## Moving-platform route availability (2026-10-26)

F01694 continues the owner-directed non-MA work after
`d3216ef1d6161218de3b9d0cd9886bfbe043c9f3`, exact green40397305088.
Every open non-MA issue body/comment history was pulled: #4/#18/#42/#148/
#149/#151/#160/#167. This checkpoint does not conclude that larger assignment.

Original `73C84.s:149-243` calls `Field_CollectMovingPlatformPositions`, not a
room-exit collector. It visits256 route words and8 endpoint pairs, modifies
the two closed bits, and mutates selector registers across pair iterations.
The native implementation uses the current room's bounded auxiliary section,
typed platform output and copied endpoint records. It preserves the actual
variable-shift/mask mutations instead of applying a simplified clean mask.

The source's initial register value can be a stack address for a first-word
second-only gate with active platforms. The read-only512-MPD audit classifies
418 structurally valid auxiliary sections,3 rooms without auxiliary data and
91 structurally unavailable/non-room tails. None of the418 uses that
first-word case; the maximum original platform count is4. Native code
diagnoses that unsupported stack-dependent input, rather than inventing a
guest stack address. The original bounded thirteen-point output capacity,
auxiliary ownership/length and endpoint binding are checked before use.

Red1/3 has2 availability failures; a later original-entry control exposes a
missing symbol-map binding before it is added. Final7/1,033 tests cover all16
platform visibility masks and4 selectors, both endpoints, operand-mutation
goldens, all256 words, visibility changes/re-entry, original13-point capacity,
unbound auxiliary data and the original module entry. Three isolated hidden
controls diagnose/exit3 for stack-dependent input, short room storage and
excess platform output. Full Debug and Release suites each pass
981/9,309,515, including actual room-AI initialization.

Fresh compilation also exposed four incompatible-pointer warnings introduced
by the earlier typed occupancy ABI: F01013 passes a native Gimmick physics
view of the same owning record. A source-registered fixup now makes that
owning-record conversion explicit; compile-time tile-prefix offset assertions
and the existing621-field native-view check protect the alias. No generated
output is manually patched and no opaque input or integer-address workaround
is introduced.

Focused `quality-native.cmd --files ... --tidy` and the default check have
0 findings/0 new. The129 port controls and32 naming controls pass; two
generations repeat across746 outputs and preserve117 owned paths. Pylance
syntax validation of the source fixups is clean.

The first standard smoke helper did not launch because the owner's current
Desktop held `build\Debug\vs-native.log`. It is not counted as a run and
that process was not terminated. Instead the current executable, matching
PDB and runtime DLLs were frozen under ignored
`build\quality\routegate42-native`; that copy has independent durable logs.
Normal-speed/scripted Start1000/Up1200/Circle1300 reaches MAP001 at1403,
captures1790 and exits1800 with0 errors. No input is injected into the live
owner session. This is room-entry smoke, not platform traversal or audiovisual
parity.

Private evidence uses `routegate42-*` in the session files directory.
The audit advances178->179/186, leaving7 gaps (4 C-backed/3 assembly-only),
3134 ordinary implementations and161 aliases. The fixing revision/exact CI
are on #42; owner edits and reference directories remain untouched.

## Gimmick and actor contact occupancy (2026-10-26)

Continued #42 after publishing Ashley-alliance revision
`8639dc81e532b477c75ea2c22a559e8be80c4799` and verifying its successful
Source Quality run40394287382. Owner asset/solution/ignore/reference edits
remain excluded from this checkpoint.

F01826/F01827 own the two directions of the shared `7E05C.s:38-69` height-grid
update. They take `const Field_GimmickInstance*`, use the named native
placement fields and checked native map storage, and add/subtract1024 with
defined unsigned-halfword wrap. The original byte-offset expression includes
the upper layer bits: it is not simply X/Z, and it must not be replaced by
the distinct tile-flag `OR0x400` obstacle setter. Out-of-range/unaligned
placements and missing active height storage produce an explicit error and
terminate instead of writing unrelated memory. Disabled navigation maps retain
the source no-update gate.

F01836 owns `7E05C.s:510-542` in `ActorCleanup.cpp`. It checks native actor
identity, the command-byte shift/mask, byte1 of `nextTile`, and the model's
typed `supportLayer`, then reuses the established room-contact-bit helper.
Both missing actor/state records are an allowed no-op; inconsistent partial
records are diagnosed rather than reading source low RAM. The original
bit31/sign-test behavior of the shared room-bit helper is retained.
The original contact symbol was absent from the generated symbol map;
its explicit binding was added after the new binding control failed.

The original missing-behavior controls failed2 cases/2 assertions before
implementation. Final normal selection
`build\Debug\VS.Tests.exe "[gimmick-occupancy],[contact-release]" --reporter compact`
passes **10 cases /1,313,815 assertions**: all65,536 halfword inputs with
both directions,32,768 aligned placement/layer combinations, all1,024 map
cells, room rebinding, actual production gimmick release/re-entry, original
bindings, and all256 command bytes x16 support layers x256 contact bytes.
The one coupled compile failure from generator dependency cleanup was repaired
by explicitly including the owning `Field/Gimmicks.h` in SpecialTerrainTests;
no generated include was manually retained.

Three explicitly selected hidden tests exercise invalid halfword alignment,
the first offset beyond the2048-byte map, and an unavailable active height
map. Each logs its required diagnostic and exits3 in an isolated child.
The first child initially waited on the Windows CRT abort dialog; the
existing `Platform::InstallCrashHandler` now disables dialogs and records the
abort stack only for these fatal controls. The timed-out child was stopped;
no default tests or runtime error policy were weakened.

Full solution builds and suites pass **974 cases /9,308,482 assertions**
each in Debug and Release. Focused `--tidy` and default quality each have
**0 findings/0 new**; port controls **127 pass**, naming controls **32 pass**,
native views **621 checked/0 mismatches**. Two generations preserve117 owned
files and repeat identically across746 outputs. Pylance syntax validation of
the changed port-control file is clean.

One normal-speed Debug smoke reaches MAP001 at tick1403, MAP008 at3127,
MAP009 at4057, captures tick5990 and exits normally at6000 with0 errors:

`tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 6000 -At 5990 -Press "1200:Up,1300:Circle,3000:Start,4000:Start" -Cap build\occupancy42-newgame-debug`

The preceding attempt omitted Up, selected Continue because native saves are
present, and stayed in the load picker. It is not counted as room verification.
All logs/captures remain local under the private session `files\occupancy42-*`
and the named ignored build directories. The successful replay is a room-entry
smoke only; skipping the prologue is not combat or moving-platform acceptance.

The audit advances the fixed cohort175->178 of186, leaving8 gaps
(5 C-backed/3 assembly-only),3133 ordinary implementations and161 aliases.
Original source declarations and .cpp promotion/whole-state modernization
remain distinct from these behavior checks. Revision/exact CI are on #42.

## Ashley alliance entry and exit (2026-10-26)

Resumed the earlier unfinished F01681/F01682 work after owner direction to
check issues and continue. Parent `2a2186bdcbfb97f0f575a975909a33c01632c4fa`
had successful Source Quality run40389100383. The open non-MA issue list still
contains #42/#148/#149/#151/#160/#167 and the separate #18 limits. This
checkpoint follows the existing #42 work rather than opening a duplicate.

Original `7399C.s:11-85` and the read-only `rood-boi` C body establish:

- Every exit clears the favorable-reaction counter; inactive exit otherwise
  preserves the whole actor state and both random streams.
- Ordinary entry consumes one BIOS random value, accepts masked values0..75,
  increments the counter and enters at two favorable reactions. The original
  stores the low byte but compares the full register. At count255 it enters
  while storing0; the resumed draft incorrectly compared the wrapped byte.
  The regression failed with3 assertions before this correction.
- Forced entry bypasses the BIOS draw. Both paths save a native function
  pointer and install the existing typed engagement-approach callback.
- Active exit clears only the alliance marker bit, rebuilds classifications,
  clears/restores the saved callback, and queues marker6/40-frame recovery and
  command2/range128 only when settingsBit25 permits. The existing actor RNG,
  navigation override and decision-delay gates are reused unchanged.

The owned implementation stays in `Battle\Ai\Relations.c`. Generator-created
aliases expose the flag, counter and saved pointer in existing view storage;
the callback is not added to every AI view's pointer slots, which would shift
the canonical native layout. C++ offset/capacity assertions cover the saved
pointer and counter. The native-view checker confirms621 existing fields
without mismatches. Fresh AI creation still clears the native complete
record. Original module bindings were already generated correctly by the
ownership mechanism and are now tested; no duplicate binding was needed.

Tests cover all128 masked BIOS rolls against all256 counter values, inactive
exit preservation, native pointer restoration/null behavior,12 recovery/busy
combinations, original entries, and actual helpful/harmful action-reaction and
saved-relation callers. The initial production-caller fixture checked the
classification timer instead of the distinct five-bit reaction timer; its
expectation was corrected to the source's `timer=28`, without changing code.

Commands from the repository root:

- Debug and Release solution MSBuild with `/m /p:Platform=x64`; all native
  desktop, Combat Lab and test targets link.
- `build\Debug\VS.Tests.exe "[ashley-alliance]" --reporter compact`:
  **10 cases /230,618 assertions**. Full Debug and Release suites each:
  **964 cases /7,994,667 assertions**.
- `python -m unittest discover -s tools\port -p test_*.py`: **126 pass**;
  corresponding naming selection: **32 pass**.
- Focused `quality-native.cmd --files` for Relations, lifecycle tests,
  native-view producer and port controls, with `--tidy`: **0 findings/0 new**.
  Repository default quality also has **0 findings/0 new**, without baseline
  changes.
- Two copier generations repeat identically across746 outputs and preserve
  all117 owned paths. `check_native_views.py`: **621 fields/0 mismatches**.
  Pylance reports no syntax errors in the native-view producer.

Evidence uses `alliance42-*` under the private session `files` directory.
The audit retains11 gaps (8 C-backed/3 assembly-only),3130 ordinary
implementations and161 aliases. The fixed cohort advances173->175 of186.
Revision/exact CI are on #42. Owner asset-pipeline, solution, ignore-file and
reference edits remain uncommitted. This does not establish a new live combat
alliance acceptance, C++ promotion or EC retirement.

## PLG074 template integration and ring-arc import (2026-10-26)

Owner scope: integrate F01986-F01995 individually, check regressions between
entries, retain native naming/provenance, and mark the ten off in #42.
Publication was explicitly requested again during this work. Source parent:
`87d6858a91365f7478e0230869a3542ad22034ef`; verification used the dirty native
worktree, while publication excludes the earlier unfinished Ashley-alliance
changes, owner asset documentation, solution edits and reference submodule.

The original PLG073 and PLG074 images both contain 22,400 bytes and have SHA-256
`542228ed077cedbbfd70b3f96f965a6d63cc67f9a5f43f7e31d7e981ce83f0dd`.
Every instruction and data byte agrees. Updated read-only `rood-reverse`
`99bf0429` binds the ten entries through `tiledRings.h`, `rings.h`,
`trails5.h`/`trailEffect.h` and `rotatingSurfaces.h`. Native code uses the
existing renamed native templates, never includes or links reference code.

- F01986/F01987 and F01989-F01993/F01995 reuse the corresponding typed PLG073
  implementation through the existing `dup-of` mechanism.
- F01988 and F01994 are owned native template instances. Their origin cursor
  is imported separately from PLG074 offset `0x577C`, address `0x800FEF7C`;
  it never aliases PLG073 mutable state. The emitter explicitly calls this
  PLG074 origin stage.
- All ten original module-address bindings and production renderer slots
  87-90 are verified. Catalog `INCLUDE_ASM` provenance is retained; the audit
  validates owned `native-template` bindings against the real shared body.
  It still rejects missing bindings and unported template bodies.
- The two `.inl` includes have narrow `build/include` annotations: cpplint
  compares repository-relative package names against project-relative include
  roots and incorrectly treats these same-directory templates as foreign
  implementation files. No quality baseline or safety diagnostic was disabled.

The ten missing entry contracts failed first (10 cases/10 assertions).
After each individual integration, generation, Debug build, all existing
effects/module regressions and all already-integrated PLG074 cases passed.
Coverage includes exact ring/surface packet geometry, seams, UVs and colors;
three anchor spaces; independent corner cursors; target and spline controls;
zero/maximum durations; exact source recurrence and circular histories;
attached/world ribbon rendering and clipping; the sixteen-trail limit;
finite/infinite lifetimes, cancellation, repeat entry and complete heap
reclamation through the production renderer list.

Final source-data inspection found and logged #196 before repair. G00659
was size `-1`, producing a tentative one-row ring-color array and no module
import. The source selects eight four-byte rows. Its proven original extent
is `0x575C..0x577B`, directly before the cursor. A new import/capacity test
failed (1 case/1 assertion), then passed after resolving the producer ledger
to 32 bytes / `u_char[8][4]`. Native rendering exercises all eight modes, and
the content test checks original bytes and reloads. The intermediate mode
fixture initially wrote the 26-bit `flags` field rather than the adjacent
`transparencyCurve` bitfield; correcting that fixture preserved the original
packed definition contract, rather than changing the renderer.

Final commands and evidence:

- `MSBuild.exe VagrantStory.Native.sln /m /p:Configuration=Debug /p:Platform=x64`
  and the corresponding Release build; desktop, Combat Lab and tests link.
- `build\Debug\VS.Tests.exe "[plg074]" --reporter compact`:
  **13 cases / 3,642 assertions**.
- `build\Debug\VS.Tests.exe --reporter compact` and the Release equivalent:
  **957 cases / 7,764,092 assertions each**, including pre-existing local AI
  tests. These are dirty-worktree results, not a claim that AI edits shipped.
- `python -m unittest discover -s tools\port -p test_*.py`:
  **124 controls pass on the exact staged source**. The ordinary dirty tree
  retains two pre-existing F01681/F01682 catalog/ownership failures.
- `python -m unittest discover -s tools\naming -p test_*.py`: **32 pass**.
- Focused `quality-native.cmd --files` selection covers the owned native
  source, both changed test files and the four changed port/audit tools,
  with `--tidy`: **3 existing test findings / 0 new**.
- Default quality on the isolated staged worktree: **0 findings / 0 new**.
  Dirty-tree default findings belong only to the excluded AI edits.
- Two successive generator runs preserve **117 owned files** and repeat
  identically across **746 generated files**. Only the Game projects are
  regenerated; the owner's solution remains untouched.

Private evidence is under
`C:\Users\judee\.copilot\session-state\b7c4a10d-7c4c-4355-a674-dd8737f2f0ec\files\`,
using `plg074-*` and `ring196-*`. The isolated `plg074-scoped` checkout verifies
the exact staged code without moving or reverting owner changes. Its audit
retains **13 gaps** (10 C-backed / 3 assembly-only), **3,128 ordinary
implementations**, **161 aliases**, **432 template instances**, and the
original ten PLG074 coverage records. The reference snapshot fingerprint in
that report reflects the isolated checkout's line endings; the original
reference directories were not changed. The two unfinished AI implementations
are not counted as completed by this checkpoint.

The ten advance the fixed #42 cohort from **163/186 to 173/186**. Publication
revision, exact CI run and eligible #196 closure are recorded on #42/#196.
The broader unresolved ring-template data warnings are not silently repaired.
No full Radial Surge controller/visual-parity sweep, whole-state C++ promotion,
hardware-shaped bridge retirement or broader particle completion is claimed.

## Learned spell and rank acceptance (2026-10-22)

The owner resumed autonomous work after the earlier unavailable approval form.
Native Catch2 execution was recovered using Start-Process with separate
stdout/stderr files and checked exit codes: the combined selection passes
55 cases /2,098 assertions in both Debug and Release. Expected stderr warnings
in rejection tests must not be promoted into a PowerShell invocation failure.

[The learned-spell checkpoint](COMBAT_LAB.md#learned-spell-acceptance-checkpoint-2026-10-22)
records18 accepted real-input sessions, source-backed costs/entry differences,
exact effect boundaries, all four Flame Sphere ranks, five restriction/cancel
cases and explicitly uncovered direct-entry IDs. Native source remains exactly
ad6b2adb, published under9b76aabf; unrelated worktree changes remain preserved.
No new production defect was established in this acceptance pass.
Acceptance documentation was published as
`60c5bd3dcc7c50f0fb648a4c92ddea8a4473e871`; exact-revision Source Quality
run39802591754 passed and #9 was closed with evidence. #4 now records #10 as
the next ordered feature. The handoff/archive remain local mixed edits, not
part of that single-file publication.

Diagnostic failures retained in local run logs:

- `1792679698075646-0`: incorrect Shaman shortcut key. Native Triangle is S,
  Square is Z, not guessed keys.
- `1792679858395081-0`: incorrect assumption of one area preview; Flame Sphere
  includes eligible limbs and Ashley, so select the actual enemy/limb.
- `1792679981118833-0` and `1792680142555874-0`: idle snapshots can precede an
  enemy action which wins the subsequent F8/shortcut input race. Explicit
  rejection/timeout leads to a bounded wait for real idle control, not a bypass.
- `1792680337423574-0`: rankII impact4/hp0 is an original failed cast. Waiting
 45 seconds for nonexistent positive damage lets the enemy kill Ashley;
  complete the failed cast and, when needed, try another ordinary cast.
- `1792680802469680-0`:20ms horizontal probe absent from sampled input;
 40ms probes cover the original two-tick input/update cadence.
- `1792680989478735-0`: two simultaneous limb results of5 each reduced actual
  enemy HP677 ->667. Sum the results, rather than compare one message with
  the whole actor's10HP loss.
- `1792681720365461-0`: original no-target confirmation returns to the spell
  menu. It does not remain in targeting; MP and progression were unchanged.

These are corrected acceptance-driver assumptions, not tickets for game fixes.
The18 final traces are the acceptance set; earlier partial/failed drivers are
not presented as completed runs. The #17 story-boundary audit, #76 particle
fidelity and remaining64 direct spell action IDs are explicitly not closed.

## Environmental recovery and progression acceptance (2026-10-22)

The final bounded #8 effects/eligibility observations use published native
`ad6b2adbbff6c56cf9874665392d64e7a226383c`, with unrelated dirty documentation,
solution/tool-project and reference state preserved. No production code changed
after the Ghost full-suite/generator validation. Acceptance documentation is
committed and pushed as `9b76aabffeb674f55c29ad446a4cdb76545324ba`; its Source
Quality run39793715858 passed every step on that exact revision. #8's six
acceptance checkboxes are complete and the issue is closed with fixing commits,
concrete measurements and explicit remaining limits. #4 records #9 as the next
ordered feature. The unrelated handoff/archive rewrites remain local rather
than being blanket-staged into the acceptance commit.

Complete measured criteria and commands are in
[COMBAT_LAB.md](COMBAT_LAB.md#item-use-acceptance-checkpoint-2026-10-22).
Eight final traces all have zero error events and `trace_failed=false`:

| Check | Debug | Release |
| --- | --- | --- |
| Fixate444/action79, real moving MAP014 platform | `1792676535074920-0` | `1792676584085327-0` |
| Eureka447/action93, actual MAP015 trap | `1792676277768454-0` | `1792676304258865-0` |
| Uncapped329/335 and344 mixed-condition cures | `1792677272197860-0` | `1792677331620738-0` |
| First/repeated Fireball and unchanged story gates | `1792678085128728-0` | `1792678132098137-0` |

Fixate's platform moves before casting, holds its exact position with both
room/platform pause flags0 while the positive freeze timer decreases, and moves
again after timer0. Debug samples go178 ->161 while stationary, then resume.
This is not mere menu/cast pausing. The original platform is in zone9 room5;
its original position/velocity records are not modified. Eureka changes the
real trap at15,9 from activation0/fade0 to activation1/fade128, costing6MP.
Both books consume one, learn their real action and return.

Recovery effect-boundary values:
- Debug329: MP800 ->900; Release329:801 ->900.
- Debug335: HP401 ->501, MP705 ->805; Release335: HP401 ->500, MP705 ->806.
- Item344 restores INT90 ->100 while preserving Poison, then separately
  restores AGI90 ->100 and removes Equipment Down while still preserving Poison.
  The original STR/INT/AGI modifiers cancel the entire0x7E0 group; they cannot
  coexist as three independent ailments.

Fixture corrections, not native defects:
- Initial recovery run `1792676962660066-0` expected fixed100 but observed
  exact800 ->899MP. Original `_itemRecovery` at146C.c:12109 and calculator15
  prove `factor*5 + smoothed(11)-5`; factor20 permits95..105. The corrected
  diagnostic matches the actual reported amount to adjacent-frame gains.
- `1792677132485015-0` completed both recovery items, then awaited an impossible
  combined STR/INT/AGI mask. Actual condition logs show each new attribute
  modifier cancels the previous one; the two source-valid344 cases replace it.
- The first gate diagnostic had a nested local helper and did not use the book.
  A later shortcut was attempted before action recovery ended. The driver now
  waits for real recovery0. Another repeat legitimately produced impact4/zero
  damage: that is valid use-result/consumption evidence, not positive-damage proof.

Final gate runs consume Fireball10 ->8 and25MP each time. Only action99 changes
from unlearned to learned, then remains learned; all unrelated action flags stay
unchanged. Read-only native observations preserve intro3, Teleport0 and unused
ability/Break Art counters0. The original magic-used flag changes0 ->1 and stays1.
Debug's repeat has impact4/no damage; Release's repeat deals positive damage.
This distinguishes a committed no-effect cast from cancellation. Earlier
`1792654884015949-0` supplies cancellation, repeated unranked/non-stacking STR
and an unusable-key rejection. No story or actor memory was patched.

The original item shortcut gates actor/resource availability, and the original
commit path consumes only after cancellation has returned. It does not add
per-book story unlocks. Teleport's separate story/save-point gate is not a
grimoire grant. Existing prologue/profile tests preserve normal startup
restrictions; comprehensive learned-spell/progression fidelity remains #9.
Matching keys/sigils and save/load remain separate follow-ups, not invented
acceptance criteria for #8.

An optional final combined `test-native.cmd Debug` selector lost its commas and
selected no tests; it is not passing evidence. A direct Catch2 retry could not
obtain tool approval and was not run. The already completed Debug/Release full
suites (416 cases /1,060,702 assertions each),23 copier tests and exact-revision
quality remain the validation for unchanged native code. No test expectation,
baseline or suppression was weakened.
After closeout, the owner was unavailable to answer whether to approve that
focused retry and continue #9 or pause. Continuation is held at the actual tool
approval boundary, not treated as an owner refusal or a fresh requirement for
GitHub authorization. No #9 implementation was started. The intended test is
the existing `VS.Tests.exe` in each configuration's build directory with the
single Catch2 OR filter `[items],[profile],[status],[stats],[encounter]`.

Final read-only remote refresh confirms upstream `ser-pounce/rood-reverse`
main5163208b, noprompt's verified-functions39821e4f and rood-boi b2c3fede remain
the reviewed revisions. The local `rood-reverse` origin is the JEschete fork:
its main e0875ad8 is38 commits behind the local/upstream revision, not newer
particle work. No reference fetch, checkout or edit was performed.

## Positive book effects, caster identity and Ghost loading (2026-10-22)

Published `work`: `ad6b2adbbff6c56cf9874665392d64e7a226383c`.
Unrelated documentation/reference edits and the pre-existing staged submodule
deletion remain preserved. Local presets, observations and logs are not committed.

### Caster trace identity (#117)

Published fix `b6e0a84ac7ceaac6f1a5ddbcfa79e90c42ff3494` changes the diagnostic
producer to the primary action request's actual acting slot. The old
`sourceActorId` contains targeting mode, not caster identity; modes2/3 do not
simply mean spell/item. Original `146C.c:4489` writes the acting actor into
`unk8.unk4.unk0.targetActor`. Gameplay and the JSON field shape are unchanged.
The source-producer regression failed before repair and passes afterward.
Both builds, 22 copier tests and focused `[encounter]` selections (25 cases /
205 assertions each) pass. Debug `1792672809694256-0` / Release
`1792672843712798-0` identify Ashley0; five-encounter restart checks Debug
`1792672877419529-0` / Release `1792672901515987-0` identify the actual enemy2.
Source Quality run39780595702 passed on exact b6e0a84a; #117 is closed.

### Nine positive offensive/negative book effects

Each uses normal shortcut, targeting and confirmation inputs, consumes10 ->9,
learns the mapped action, charges the original cost and returns normally.
These are bounded gap checks, not another broad all-item sweep.

| Book / action | Cost | Debug run | Release run | Positive result |
| --- | --- | --- | --- | --- |
| 388 /99 Fireball | 25 | `1792672809694256-0` | `1792672843712798-0` | Real Minotaur Zombie damage |
| 423 /94 Drain Heart | 12 | `1792673351295983-0` | `1792673552369320-0` | Enemy HP loss and Ashley HP gain |
| 424 /95 Drain Mind | 2 | `1792673386523391-0` | `1792673587171088-0` | Enemy MP loss and Ashley MP gain |
| 432 /54 Degenerate | 7 | `1792673421873792-0` | `1792673622353166-0` | STR91 ->82, status0x20 |
| 441 /82 Stun Cloud | 7 | `1792673444615055-0` | `1792673645040861-0` | Paralysis0x2000 |
| 442 /83 Poison Mist | 11 | `1792673469634297-0` | `1792673669741563-0` | Poison0x4000 |
| 443 /85 Curse | 17 | `1792673494809373-0` | `1792673694722601-0` | Status0x10000, attributes91/89/101 ->82/81/91 |
| 393 /104 Exorcism | 22 | `1792674665668194-0` | `1792674749081138-0` | Mandel146 ->0; normal loot confirmation |
| 394 /105 Banish | 25 | `1792675471010886-0` | `1792675506604847-0` | Ghost67 ->0; normal loot confirmation |

First-positive drain snapshots (849 ->867HP and922 ->934MP) are not necessarily
final totals: resource animations can continue across frames. Do not report a
first-positive Fireball1HP sample as the entire spell's damage.

Fixtures use999 starting HP/MP, default base attributes and original enemy
definitions. Fireball uses zone12 room0, Minotaur Zombie1, Ashley9,10/enemy9,14.
Drains/negative books use zone9 room0, Sackheim6, Ashley4,9/enemy4,11;
three real F1/F2 inputs supply the drain preconditions. Exorcism uses Mandel0,
Ashley4,9/enemy4,10. Banish uses zone13 room0, Ghost0,
Ashley9,13/enemy9,14. No live HP, position, event or completion memory was edited.

Read-only ZND class decoding is checked against the record stride0x464 and
catalog HP/MP. Mandel is Undead, not a positive drain fixture; Sackheim is Human
with MP. Minotaur Zombie is Undead with0MP. Ghost is Phantom, but Banish is not
restricted solely to that class. Exorcism requires Undead; drains reject Undead.

Area books need three confirmations: height, horizontal position, final targets.
Earlier failed diagnostics omitted later confirmations or ordinary loot input;
they are not spell defects. Initial Ghost layout Ashley9,14/enemy9,13 selected
centerZ1856 but previewZ1727, one unit outside the128-unit half-area. Reversing
the validated adjacent layout retained the actual preview point without changing
targeting rules. Both final Banish runs consume at910, defeat at1214 and return
at1685, with MP999 ->974 and learned action105. All four new Ghost traces have
zero error events and `trace_failed=false`.

### Ghost secondary-body table alias (#118)

Fresh pre-fix Debug run `1792674705041076-0` crashed at tick48, reading0x2 in
`Actors_SpawnRoomEnemy` before item/F10 input. Original F00176/`func_800775C0`
at `146C.c:6578`, secondary read at6676, uses `D_800F188C[zudIndex].unk2+1`.
`D_800F188C` aliases `_zoneContext(0x800F1880)+0xC`, the live enemy-table pointer.
The registered copier now uses `Field_zoneContext.zndEnemies[zudIndex].recordBytes[2]`
for the companion model, matching the existing primary path. Both original
views retain stride0x464 and character-kind byte2; the ZUD index is not replaced
with the enemy-definition index. Models26/27 load successfully in the real runs.
G00247/F00176 notes record the alias; no promotion/retirement flag was advanced.
The inert independent declaration remains tracked compatibility debt.

Debug `1792675542067688-0` and Release `1792675556299153-0` each complete three
fresh live-Ghost encounters and three F10 stops. Debug stop/end ticks:
58/70,325/336,396/407; Release58/70,327/339,399/411. This establishes Ghost
repeat entry, not a root-cause fix for unreproduced #82.

Commands: `build\quality\live-restart-check.ps1 -Configuration Debug -Book 394`
and Release; repeat with `-GhostRestart` instead of `-Book 394`. The same local
driver's `-Book <id>` selects the other bounded cases above. Observations:
`build\quality\live-book-<id>-<Configuration>.json`; normal logs:
`build\<Configuration>\combat-lab\runs\<run-id>`.

Both x64 MSBuild configurations pass. `test-native.cmd Debug` and Release each
pass416 cases /1,060,702 assertions. The new alias regression fails before repair;
`python -m unittest discover -s tools\port -p "test_*.py"` passes23 tests afterward.
Two copier runs retain751 stable generated files and86 unchanged owned files.
`quality-native.cmd --files tools\port\copy_fixups.py tools\port\test_encounter_fixups.py`
reports zero findings. Evidence: `build\quality\ghost-alias-*`. Existing generated
actor-view pointer warnings remain; no baseline/suppression was changed.

The seven-file fix is committed and pushed as ad6b2adb. Exact-revision Source
Quality run39787313511 succeeded; its completed logs confirm24 positive controls,
10 encounter-adaptation tests and195 existing findings /zero new. #118 is closed.
#8 remains open for environmental Fixate/Eureka effects and remaining recovery,
already-learned/progression cases. Keys/sigils remain owner-requested follow-up.
Consumption is84/84, not full effect acceptance. No save persistence, all-content
spell fidelity, #82 root cause or #76 particle fidelity is claimed.

## Scoped item follow-up publication (2026-10-22)

Six separate commits were pushed normally to `work`: `af771c69` (#32),
`d33b3f43` (#84), `fa794fab` (#115 and the item/Lab fixtures), `65f3c8f1`
(#85), `5aa6adab` (#86), and `9745ac35` (#116). The six issues were closed
only after Source Quality run `39777257457` passed every step on exact head
`9745ac3540a7afc4697f552bcb4f93c82932ed1a`. Each closure records its individual
fixing commit, actual controller/visual/mechanics evidence and remaining limits.
The original ongoing-task publication authorization was used; the earlier
unavailable confirmation forms are not an outstanding blocker or owner refusal.

Fresh checks: both x64 configurations build; both full native suites pass
416 cases / 1,060,702 assertions; all 21 copier tests pass; full source quality
reports 195 existing findings, zero new. The two configurations used the
documented MSBuild command and `test-native.cmd Debug` / `test-native.cmd Release`.
Copier selection: `python -m unittest discover -s tools\port -p "test_*.py"`.
Quality selection: `quality-native.cmd`, without baseline or suppression changes.
Evidence: `build\quality\item-closeout-{debug,release}-{build,tests}.log` and
`build\quality\item-closeout-quality.log`.

An isolated staging index kept the pre-existing staged reference-submodule
deletion out of every commit. Committed native code, generated projects,
copier/scaffold changes and affected ledgers match the tested working files
exactly. Unrelated documentation rewrites, local reference changes and extracted
assets/captures were not published. #8's broader item effects and persistence,
#82's unreproduced live-enemy restart crash, and #76 particle fidelity remain open.
The following earlier checkpoint retains its original pre-publication status.

## Item follow-up closeout and reference review (local, 2026-10-22)

Source remains dirty `work` at `4777775f5d16163fc4d0f2740170bdb02fb4d175`.
No publication or issue closure occurred in this checkpoint. Last green CI
`39707982956` validates the published base, not these local fixes. Preserve
unrelated edits and the staged reference-submodule removal.

### Confirmed follow-up results

- #116: original `vs_battle_wepModels=0x800F4588` and
  `D_800F4590=0x800F4590` establish the latter as weapon pointer slots2/3.
  Native F02465/F02637 incorrectly used independent one-element storage.
  Registered copier fixes now set the canonical weapon visibility bit and
  reset the canonical fade-start field. Both consumers are covered by a
  failing-then-passing generator regression. G00409/T00254 and associated
  function/field/trace metadata now describe the alias correctly; no
  behavior/promotion/retirement status was advanced.
- Before repair, Debug `1792667259518061-0` read-only observation found the
  correct enemy model68 in preview slot2, owner1, bones3, attachment30,
  but drawing disabled. After repair, Debug `1792667966428084-0` visibly
  shows Ashley's equipped blade.
- Release `1792668067114499-0` shows the Minotaur Zombie axe after actor
  switching, cancellation and re-entry. Inputs:
  `130:Triangle,300:Down,400:Circle,900:R1,1200:L1,1450:R1,1700:Cross,1850:Circle,2050:R1`;
  2300 frames, speed1. Menu returns at1721 and re-enters Status at1851.
  Capture: `build\quality\status-enemy-weapons-fixed.png`.
- Release `1792668480343311-0` also shows the Skeleton's held weapon:
  zone13 room0, enemy definition5; inputs through the first R1 above,
  1500 frames. Capture: `build\quality\status-skeleton-fixed.png`.
  Both Release runs have zero error events and `trace_failed=false`.
  No shield-equipped visual or live equipment-reload acceptance is claimed.
- #85/#86 additional Release `1792668279428641-0` confirms rank IV action117
  at1115, elevation-selection cancellation1601, rank I action114 at2065,
  MP999 ->963 at2303 and return to idle2955. It uses the documented Debug
  IV-cancel-to-I input sequence, 3400 frames, speed1. No error events;
  normal trace finalization.
- Clef: corrected-facing Debug `1792668279581682-0`, zone37 room0/MAP176,
  profileB, Ashley9,10, chest9,11, one `200:Up` press turns to facing2048
  without leaving the adjacent tile. Read-only observation records actual
  lock2 ->0 after the first cast. Two uses consume item446 from10 ->8,
  learn action92, cost3MP per cast and return to idle. No memory or progression
  flags were written. Evidence:
  `build\quality\clef-correct-facing-observation.json` and `clef-replay.ps1`.
  The earlier away-facing attempts are not effect-failure evidence; the
  longer Up hold reached a different tile and consumed a book without
  unlocking. This checkpoint proves the actual lock change, not just use.
- This raises named-consumable/book consumption coverage to84/84, not
  complete effect acceptance. Correcting the previous fixture conclusion:
  ordinary `lockTriggers` and `containers` survive Lab event isolation.
  Keys/sigils still need matching-lock replays; a blanket event-policy
  expansion is not justified.

After the preview repair, `test-native.cmd Debug` and `test-native.cmd Release`
each pass **416 cases / 1,060,702 assertions**. Logs:
`build\quality\preview-{Debug,Release}-tests.log`; both builds also pass.
`python -m unittest discover -s tools\port -p "test_*.py"` passes21 tests.
Two copier runs retain751 identical generated files and86 unchanged owned
sources/headers. `quality-native.cmd --files tools\port\copy_fixups.py
tools\port\test_encounter_fixups.py` reports zero findings.
Updated ledger CSV column counts were validated. Earlier full changed-header
quality remains328 existing /zero new; no baseline was changed.

### Additional preview and restart acceptance

The stock new-game inventory has no equipped shield. Real Equip UI inspection
shows `SHIELD NONE`; no shield was invented or granted to make a capture pass.
Read-only ZND decoding, checked against `Actors_CharacterInit` and
`Items_ShieldSpawnLoadout`, instead identifies supported zone9 enemy6 Sackheim
with shield item127/model96 from MAP014. The normal source-backed Lab
configuration places that enemy in zone9 room0, Ashley4,9 and enemy4,11.

- Debug `1792669671662902-0` and Release `1792669671662303-0` visibly show
  Sackheim holding both sword and shield in Status. Inputs:
  `130:Triangle,300:Down,400:Circle,900:R1`; 1500 frames, speed1.
  Status enters at401 in both. Captures:
  `build\quality\status-shield-{Debug,Release}.png`; preset
  `build\quality\status-shield.json`.
- Actual equipment-model reload: Debug `1792669514001197-0` selects
  Fandango and unequips it; the Equip screen shows `WEAPON NONE` and empty
  hands. Debug `1792669402558979-0` and Release `1792669514000617-0` then
  re-enter the slot, select owned Fandango and restore its visible model.
  Full sequence:
  `130:Triangle,300:Circle,700:Circle,1050:Circle,1450:Circle,1900:Circle,2150:Down,2300:Circle`;
  2700 frames, speed1. The intermediate unequipped capture stops at1800
  after the1450 input. Captures: `build\quality\equip-unequipped-Debug.png`
  and `equip-reload-{Debug,Release}.png`.
- All five preview/reload runs above have zero error events and
  `trace_failed=false`. This supersedes the earlier shield/reload limit
  for #116; it is not comprehensive #14 equipment acceptance.

Additional #82 reproduction attempts use actual host F10/F6/F7 and builder
navigation, not direct calls to reset functions. Debug `1792669132748917-0`
and Release `1792669185688328-0` each finish five encounters. At every stop,
Ashley has999 HP and the Minotaur Zombie has681 HP. Stops cover idle,
enemy action1 phase7, paused after exactly one F7 tick, profileA -> B and
another action1 phase7, then fresh re-entry. Initial/restarted player
HP/MP/statuses match; profileB receives58 fresh stacks of10.
Both hosts exit with `encounter_interrupted=false`, zero error events and
`trace_failed=false`.

Commands: `build\quality\live-restart-check.ps1 -Configuration Debug` and
the same with `Release`. Driver and detailed sampled results remain local:
`build\quality\live-restart-check.ps1`,
`build\quality\live-restart-{Debug,Release}.json`.
No intermittent fault was reproduced, no speculative lifecycle code was
changed, and #82 is not fixed by this evidence. The next useful observation
is the actual failing setup/phase and crash stack, not more identical
successful restarts. Publication and fixing-revision CI remain outstanding
for the locally verified fixes; no ticket closure is claimed here.

### Positive cure-book effects with real ailments

Debug `1792670335675466-0` and Release `1792670431199842-0` close the earlier
four-cure-book eligibility gap. Solo profileA, zone9 room0, startup HP/MP999;
Lab F4/F5 establish the actual production ailments and ordinary L2+Up/Circle
inputs select and confirm each book. No effect/result/learning fields were
written directly.

The original resident action table is at SLUS file offset0x3C1DC with52-byte
records. Its first-hit mediator indices27/28/29/32 resolve through the
BATTLE table at0x7FB78 to paralysis cure, poison cure, curse cure and the
negative-status cure mask0xE000 respectively.

| Item / action | Actual effect | Debug consume / effect / idle | Release consume / effect / idle | MP cost |
|---|---|---|---|---|
| 426 / 87, Restoration | Paralysis0x2000 ->0 | 1016 /1298 /1634 | 1008 /1290 /1626 | 3 |
| 427 / 88, Antidote | Poison0x4000 ->0 | 1910 /2192 /2528 | 1898 /2180 /2516 | 3 |
| 428 / 90, Blessing | Curse0x10000 ->0; current STR/INT/AGI90 ->100 | 2738 /3020 /3356 | 2722 /3004 /3340 | 17 |
| 429 / 91, Clearance | Paralysis+poison+numbness0xE000 ->0; walk/run4/9 ->9/18 | 3662 /3964 /4312 | 3640 /3942 /4290 | 15 |

All removals coincide with action phase7 and a real `combat.result`.
Paralysis still has15 timer units just before Restoration removes it;
Clearance's timed ailments still have14/15 units in Debug and14/14 in
Release. These are actual cures, not timer expiry. Every book consumes10 ->9,
changes its learned flag from absent to present and returns normally.
Both traces contain zero error events and finalize with `trace_failed=false`.
Total attribute fields are the unchanged baseline; the active
`strength/intelligence/agility` fields are the values restored by Blessing.

Commands: `build\quality\live-restart-check.ps1 -Configuration Debug -Cures`
and the same with `Release`. Preset: `build\quality\cure-books.json`.
Per-case before/consumption/effect/return snapshots:
`build\quality\live-cures-{Debug,Release}.json`.
Two initial driver attempts are not accepted: one sent Down before the
shortcut opening animation finished; the next used X, which is native Cross,
and cancelled instead of confirming. The driver now waits for the menu and
uses native C/Circle; the complete runs above pass. Neither attempt exposed
a game defect.

Other previously recorded offensive/environmental eligibility gaps, keys,
sigils and save persistence remain unverified. These results do not establish
complete #8 or #9 acceptance.

### Read-only particle and trail reference review

Inspected local references without fetch, checkout or file edits:

| Reference | Inspected revision | Relevant result |
|---|---|---|
| `resources\rood-reverse` | `5163208b363660881f3139612ed042897ce95b10` | Same revision as upstream main; PR288 unifies PLG057's 19 shared particle/nine-sample/five-sample stages and separates trail types. |
| `resources\rood-reverse-noprompt` | `39821e4fc0353dc89855f5f29fd678d71545daa2` | Current remote `decomp/verified-functions`; merged PR284-287 and retains additional effect implementations/type views. |
| `resources\rood-boi` | `b2c3fedefbeb05e912ef281688598a963def6a39` | Current remote HEAD; recent effect-request initialization repair is relevant to deterministic state comparisons, not demonstrated particle-space correctness. |

Upstream review:
[ser-pounce/rood-reverse#288](https://github.com/ser-pounce/rood-reverse/pull/288).
The native copier uses the read-only `rood-reverseWin` source snapshot, not
the parent repository revision reported by running git inside that non-repository
directory. Its PLG057 includes already supply all19 functions, catalog
F01930-F01948, to native generated template instances. Native
`Effects_Trail9` already has nine samples and the0x78 size assertion;
five-sample trails are separate. PR288 is useful corroboration, not proof
that importing it would cure #76.

The noprompt-only effect families inspected (PLG077/128/162/236/263/300)
also have native catalog entries/implementations. Bounded PLG057 comparison
shows the fork's local macro/type workaround for mixed sample counts.
PLG162 differences expose named world/view matrices and nested child-motion
fields; native type/layout contracts, not source text equality, need checking.
Changed hashes and decompiled function counts are not behavioral evidence.

Rood-boi commit `27c618b90ba86506d3b43979d76143867e9a85ec` zeros the complete
action effect request before filling it. Its report concerns unused/reserved
host-stack bytes and state-oracle divergence. Native F00109 still declares
the request without whole-object initialization. No native trajectory defect
or consumer of those unused bytes has been demonstrated; do not present this
as the fire/smoke fix.

Next #76 observation: an aligned concrete fire/smoke or trail reproduction,
recording emission origin/target, transform selection, four control points,
sampled world positions and rendered positions. Compare native world/view,
fixed-point and parent/child layout contracts with the original before
changing them. Existing template coverage, this review and passing tests do
not close the scattered-particle issue.

## Spell rendering and status mechanics (local, 2026-10-22)

Source: dirty `work` based on `4777775f5d16163fc4d0f2740170bdb02fb4d175`.
These changes remain uncommitted. Preserve unrelated instruction, documentation,
solution and reference edits and the pre-existing staged submodule deletion.
The last published CI remains successful run `39707982956` on that base.

The completed owner run `1792659407129820-0` has 462,588 consecutive events,
66 item uses / 49 distinct items in two encounters, no error-level events and
normal `trace_failed=false` finalization. Each use consumes one copy and
returns to idle. Combined with earlier runs, 83/84 named consumables/books
have consumption evidence and all seven ranked grimoires have four uses in
one encounter. Meteor IV: action 133, consumption 47631, effect 48115
(enemy HP 527 -> 518), return 48447, MP cost 62, copies 7 -> 6.
Clef (446) was not consumed: the owner confirms no chest was available.
Positive-effect gaps remain for items 388, 393, 394, 423, 424, 432, 441, 442
and 443. Cure books 426-429 had no matching ailment. Fixate 444 had no
moving-platform target; Eureka 447 had no trap. Review:
`build\quality\items-owner-1792659407129820-0-review.json`.
F10 was requested with living actors at 60482 and 92392; profile B entered
at 63174 after A. This is successful living-enemy re-entry evidence for #82,
not proof that the intermittent crash is fixed.

### Rendering implementation and verification

- F02604 / #86: final rank-menu packet used
  `((u_int)prim << 8) >> 8`. A failing copier regression preceded replacing
  it with `LegacyGpu_Handle(prim)` in `copy_fixups.py`. Generated output was
  not manually patched. ASCII LV strings were already correct.
- F01309 / #85: original `BATTLE/BATTLE.PRG/5A624.s` was an unported stub.
  `SphereTerrain.cpp` is registered as owned, with the original 9x9 terrain
  border, wall/corner occlusion and 32-point cross-section scanline passes.
  Native vectors replace scratch-RAM interval storage; existing GTE and
  GPU packet interfaces remain. Function ledger metadata was corrected
  without advancing behavior, C++ promotion or EC retirement gates.
- `all-spells` learns exactly 71 actions from the four production spell
  classes (50 names plus higher ranks), retaining ordinary inventory,
  equipment, resources and chain/Break Art mappings. It is not a Teleport or
  story-event grant. Regressions cover missing tables, state preservation,
  repeat entry and preset round trips.

Final source/project generation used the retained Python interpreter:
`tools\port\copy_to_native.py` then `tools\scaffold\gen_solution.py`, twice.
All **773 generated/project files** are identical on the second pass and
all **86 owned sources/headers** are preserved. The full copier unittest
discovery passes **20 tests**. Logs:
`build\quality\spell-status-generation-{1,2}.log`.

Full x64 solution MSBuild builds followed by `test-native.cmd Debug` and
`test-native.cmd Release` each pass **416 cases / 1,060,702 assertions**.
Evidence: `build\quality\spell-status-{Debug,Release}-build.log` and
`spell-status-{Debug,Release}-full-tests.log`.
The focused `quality-native.cmd --files ... --tidy` selection of
`SphereTerrain.cpp`, `Boot/Encounter.cpp`, `SphereTerrainTests.cpp`,
`EncounterProfileTests.cpp`, `StatusBehaviorTests.cpp` and Combat Lab `main.cpp`
passes **zero findings / zero new**. See
`spell-status-final-focused-quality.log`. The full `--changed --tidy` rerun
passes **328 existing findings / zero new**; evidence:
`spell-status-final-quality.log` and `spell-status-full-quality-report.json`.
The subsequent test-only refinement sets current HP100 versus max HP1000
to distinguish poison's current-HP scaling, and separates its last-HP case.
Both full suites were rerun; targeted quality again passes zero findings in
`status-final-quality.log`. No baseline or suppression was changed.

Live replays use `--start --scripted --speed 1` and existing real controller
inputs. GUI launches explicitly wait for process completion and verify the
run finalization; a shell exit alone is not acceptance. Fixture:
`build\quality\all-spells-enemy.json`, zone 9 room 0, seed 1, Ashley (4,9),
non-attacking dummy definition 20 at (4,11), HP/MP 999.
The dummy blocks magic; these are selection/rendering/cost tests, not positive
enemy-damage acceptance.

| Configuration / purpose | Run | Demonstrated result |
| --- | --- | --- |
| Debug rank menu before / after | `1792661824265037-0` / `1792662044576447-0` | Blank/truncated UI becomes visible ranks/arrows/costs/HUD |
| Debug plane before / after | `1792662706232663-0` / `1792662745832026-0` | Action 114 phase 2; unported warning removed; visible clipped plane |
| Debug IV cancel/re-entry/cast | `1792662922644833-0` | Action 117, MP 999 -> 939, return idle; not rank-I evidence |
| Debug IV cancel then I | `1792664170918807-0` | 117 at1115; cancel1601; 114 at2065; MP999 ->963 at2303; idle2955 |
| Debug II / III | `1792665578508636-0` / `1792665633065464-0` | Actions115/116; MP999 ->955/947 at1503; idle by2800 |
| Release I / II | `1792664688101263-0` / `1792664743686554-0` | Actions114/115; costs36/44; idle by2800 |
| Release III / IV | `1792664798267399-0` / `1792664852870888-0` | Actions116/117; costs52/60; idle by2800 |
| Debug two-rank learning/UI | `1792664995941793-0` | Two real item403 uses, stock10 ->8, only114/115 learned; LV1/LV2 and locked placeholders visible |

All cost replays above finalized without error-level events. Release Up inputs
move area center Y through -163/-155/-147/-139 before confirmation.
Screenshots remain local in `build\quality`: `spell-rank-{before,after}.png`,
`spell-plane-{before,after}.png`, `spell-rank-IV-Release.png`,
`spell-plane-raised-IV-Release.png` and `spell-partial-two-ranks.png`.
One book use (`1792664688190148-0`) correctly skips the rank selector because
only rank I is learned; its image shows targeting, not the partial selector.
The initial adjacent-Minotaur plane attempt never reached targeting and is
not reproduction evidence.

Rendering limits: no emulator frame-aligned pixel differential; synthetic
tests do not establish every near-plane/degenerate camera case. The wall
projection invariant still reports/throws if a non-flat edge reverses. No
issue closure, full spell fidelity or compatibility retirement is claimed.

### Negative statuses and remaining fixtures

Ten new mechanics tests call the production status queue/apply/remove/timer
and action-restriction functions. Original resident parameters were checked
at SLUS file offset **0x3F6CC**, not 0x3F6D0.
They verify:

- Attribute reductions by floor(base/10), timer expiry and cure restoration;
  Curse lowers all three, replaces buffs and blocks ordinary stat changes.
- Numbness halves walk/run and AI steps, cancels Quicken and expires;
  Quicken increases speeds 25%. Injured-leg weakness persists only while
  the mobility limbs are at 1 HP.
- Silence blocks the spell action class; paralysis blocks physical actions
  while spells/items retain their separate policy.
- Poison's two-update interval, actual HP reduction, lethal lower bound and
  cure stopping damage; regeneration's five-update interval, HP cap, limb
  restoration and cancellation by poison.
- Equipment Down's actual weapon/shield/armor strength changes and restoration,
  status immunity and the original excluded-actor rule.

This is mechanics-unit evidence with synthetic actors/parameters, not live
visual acceptance of every spell/item or floating label. The misleading Lab
SLOW fixture was logged as #115 before removal: parameter4 is injured-leg
weakness, which clears immediately on healthy legs. F4 now lists nine real
ailments and retains Numbness for timed half-speed movement. Main-game rules
were not altered.

At this checkpoint the door/script isolation was incorrectly treated as a
blanket key/chest blocker. The closeout above corrects that conclusion:
ordinary lock/container sections remain available and Clef unlocks a real
chest. Keys/sigils still need matching-lock evidence. No progression flags
were bypassed.

Enemy preview investigation is now tracked by #116.
Original `MAINMENU.PRG/224.c` clones weapon models and conditionally renders
them in Status. Native clone/draw paths retain those checks. A weapon's
animation draw bit and model projection matter; source support alone does
not establish every enemy's original appearance. Minotaur-Zombie attempts
with Triangle at300 and90 did not open Status. The latter trace
`Release/1792665526546323-0` shows idle control and the real Triangle input
at90/91 without a menu transition; no missing-weapon fix is inferred from
those attempts. Obtain an actual Status reproduction/reference comparison.

## Combat Lab item profiles and starting-stat overrides (local, 2026-10-22)

Base revision: `4777775f5d16163fc4d0f2740170bdb02fb4d175`; dirty `work` tree.
No item-work commit or push yet. Preserve the unrelated documentation,
instruction, solution and reference edits, including the staged submodule
removal. Reference source trees and disc-derived files were not modified.

The new `all-items-a` and `all-items-b` profiles contain 58 distinct stacks of
ten items each: 34 ordinary consumables, 50 grimoires and 32 keys/sigils across
the two profiles. The stock capacity remains 64; equipment and learned spells
are not expanded. Quantity zero and empty presets support solo Ashley.

Lab-only controls prepare HP/MP/Risk/status conditions, with readable F11 help
and before/after or rejection logs. F8 restores current MP to its existing
maximum. The owner's subsequent setup requirement adds optional starting
HP/MP/STR/INT/AGI overrides to the editor and preset format. Each accepts only
integer 1..999, can inherit DEFAULT independently, and applies once after
normal actor initialization. HP/MP start full. Invalid inputs are rejected;
older presets without `ashley_stats` retain their behavior. Main-game startup,
inventory, equipment and spell-learning flags are unchanged.

Verification:

- Final Debug/Release builds use `MSBuild VagrantStory.Native.sln /m
  /t:VS_CombatLab,VS_Tests /p:Configuration=<configuration> /p:Platform=x64`.
  Both complete `build\<configuration>\VS.Tests.exe` suites pass
  **396 cases / 993,045 assertions**. Logs:
  `build\quality\items-overrides-{Debug,Release}-checked-{build,tests}.log`.
- Focused `[stats],[items],[profile],[encounter]` selection passes
  **38 cases / 1,689 assertions** in Debug. Coverage includes every field's
  range, malformed preset values, unsigned overflow, omitted defaults,
  editor steps/reset, resource fill and current/base attribute consistency.
- The session-local replay harness's `-Mode stats -Configuration Debug`
  and `Release` runs use the existing `--scripted --press` controller path.
  Runs `1792658234597087-0` and `1792658396711193-0` enter with HP/MP 999
  and all three base/current attributes 1 at tick 585; lower resources without
  continuous reapplication; refill MP 977 -> 999; re-enter with the same
  overrides at 1245; reset every override and observe HP 250 / MP 50 /
  attributes 100 at 1825. Both exit 0 with normal trace finalization.
  Logs: `build\quality\items-overrides-scripted-{Debug,Release}.log`.
  Readable editor capture: `build\quality\items-stats-verified.png`.
- Ordinary non-scripted UI attempts received extra live device input and are
  not acceptance evidence. The successful editor tests isolate device input
  using the host's existing scripted mode.
- Explosion replay: `-Mode ranked -Configuration Debug -StartSlot 43
  -ItemLimit 44`, profile A, zone 12 room 0, seed 1, Ashley (9,10),
  Minotaur Zombie definition 1 at (9,14), HP/MP overrides 999. Run
  `1792658645709510-0` uses book 395 four times without an encounter reset:

  | Rank | Action | Consumed / returned tick | Copies left | MP at use | Enemy HP before / after |
  | --- | --- | --- | --- | --- | --- |
  | I | 106 | 1619 / 2523 | 9 | 963 | 681 / 677 |
  | II | 107 | 4025 / 4913 | 8 | 955 | 677 / 674 |
  | III | 108 | 5923 / 6839 | 7 | 947 | 674 / 671 |
  | IV | 109 | 8245 / 9167 | 6 | 939 | 671 / 660 |

  All four learned flags persist. Real area-selection confirmations and
  learning-result acknowledgements are controller inputs; target/result
  snapshots are read-only. F8 restores MP between uses. Evidence:
  `build\quality\items-ranked-overrides-harness.log` and
  `items-ranked-Debug-20261022-044405.results.json`.

Do not repeat the earlier max-MP preparation approach. Ranked spells cost
36-62 MP, exceeding the unmodified 50 maximum. The training dummy explicitly
blocks magic (`statuses & 0x80040000`), while the nearby Minotaur Zombie killed
normal Ashley during consumable preparation. These were unsuitable fixtures,
not proofs of item failure. The new startup overrides address setup directly.

Earlier local item repairs and evidence remain relevant:

- #32: F02553's shortcut scratch allocation now reserves native pointer width
  for 128 text pointers, rather than the original 512-byte PS1 table.
- #84: the source copier honors per-offset EFFPURGE plugin bindings before
  ambiguous shared-address labels. Elixir of Queens no longer calls the
  development dispatcher; a failing native table regression demonstrated
  the original defect. Generated C was updated through the copier.
- All ten permanent-stat items (346-355) changed exactly their intended base
  attribute or maximum by 1-4, left the other base/max values unchanged and
  consumed one item. Debug and Release both passed real actions. Complete
  Release run: `1792654498738975-0`.
- Eye of Argon revealed the actual MAP015 trap (zone 9 room 6, tile 15,9):
  activation 0 -> 1 and marker fade 0 -> positive, run `1792654798094151-0`.
- Grimoire 431 cancellation, first/repeated use, non-stacking STR, an invalid
  Bronze Key, menu-state adjustment rejection and F10 from the menu passed in
  `1792654884015949-0`. This is an unranked book, not four-rank proof.

### Owner-driven item evidence and numerical recovery (2026-10-22)

The owner stopped automated input sweeps and took over gameplay to test faster.
Do not resume the session-local input harness without a new request. Its
interrupted final-five attempt, `1792659253378245-0`, is not accepted as a
completed ranked run. The owner's separate Debug process writes run
`1792659407129820-0`; it was inspected read-only without sending inputs,
restarting gameplay, rebuilding the running executable or terminating it.
It records the same dirty base revision above, speed 1, profile A, HP/MP 999,
zone 12 room 0, seed 1, Ashley (9,10), Minotaur Zombie definition 1 at (9,11).

The bounded snapshot ends at tick **25048**, byte offset **104613433**.
There is one encounter beginning at tick 2213, with no restart in the inspected
interval. This is partial live-run evidence, not normal session-exit proof.

| Book / action | Observed outcome |
| --- | --- |
| 386 / 97 | One consumed, learned, 4 HP damage, returned at 4015 |
| 387 / 98 | One consumed, learned, 1 HP damage, returned at 4901 |
| 388 / 99 | One consumed and learned; impact 4 with zero HP effect; returned at 5831 |
| 389 / 100 | One consumed, learned, 3 HP damage, returned at 7569 |
| 390 / 101 | One consumed, learned, 2 HP damage, returned at 8645 |
| 391 / 102 | One consumed, learned, 2 HP damage, returned at 9703 |
| 392 / 103 | One consumed, learned, 1 HP damage, returned at 10891 |
| 393 / 104 | One consumed and learned; impact 4 with zero HP effect; returned at 12097 |
| 394 / 105 | One consumed and learned; impact 4 with zero HP effect; returned at 13587 |

These observations separate successful learning/return from positive offensive
effects; the three zero-effect casts are not evidence of damage or a confirmed
defect. They need an eligible positive hit before effect acceptance.

Flame Sphere, book 403, completes all four ranks in the same encounter:

| Rank / action | Consumed / returned tick | Copies left | MP before / at use | Enemy HP after return |
| --- | --- | --- | --- | --- |
| I / 114 | 14127 / 14985 | 9 | 773 / 737 | 666 |
| II / 115 | 15979 / 16837 | 8 | 738 / 694 | 659 |
| III / 116 | 17279 / 18135 | 7 | 694 / 642 | 646 |
| IV / 117 | 18735 / 19587 | 6 | 642 / 582 | 637 |

Each rank produces positive `combat.result` HP damage and adds its corresponding
learned action; all four flags remain in the snapshot. Multi-limb higher ranks
also record some zero-effect hits; those are not counted as extra successes.
Gaea Strike, book 407, has completed actions 118-120 at return ticks
21275/22667/23949, consumed 10 -> 7 and retained those three learned ranks.
Its rank IV is not in this snapshot. Item 336 is resolving at the snapshot
boundary; do not treat its missing return as a failure.

A subsequent incremental read through tick **40286**, sequence **228600**,
byte offset **178637855**, resolves those pending observations and covers more
of the same encounter:

| Book / rank / action | Consumed / returned tick | Copies left | MP before / at use | Positive enemy damage |
| --- | --- | --- | --- | --- |
| Gaea Strike / I / 118 | 20499 / 21275 | 9 | 582 / 546 | HP 637 -> 634 at effect tick 20963 |
| Gaea Strike / II / 119 | 21903 / 22667 | 8 | 547 / 503 | HP 635 -> 634 at effect tick 22359 |
| Gaea Strike / III / 120 | 23115 / 23949 | 7 | 503 / 451 | HP 634 -> 624 at effect tick 23575 |
| Gaea Strike / IV / 121 | 26005 / 26765 | 6 | 999 / 939 | HP 624 -> 613 at effect tick 26467 |
| Avalanche / I / 122 | 27447 / 28157 | 9 | 939 / 903 | HP 613 -> 611 at effect tick 27855 |
| Avalanche / II / 123 | 28565 / 29277 | 8 | 903 / 859 | HP 611 -> 608 at effect tick 28991 |
| Avalanche / III / 124 | 29877 / 30593 | 7 | 859 / 807 | HP 608 -> 589 at effect tick 30303 |
| Avalanche / IV / 125 | 31131 / 31841 | 6 | 808 / 748 | HP 589 -> 577 at effect tick 31557 |
| Radial Surge / I / 126 | 32735 / 33487 | 9 | 748 / 710 | HP 577 -> 576 at effect tick 33203 |
| Radial Surge / II / 127 | 33849 / 34611 | 8 | 710 / 664 | HP 576 -> 563 at effect tick 34315 |
| Radial Surge / III / 128 | 36437 / 37187 | 7 | 999 / 945 | 10 HP damage across four positive limb results at tick 36909 |
| Radial Surge / IV / 129 | 37547 / 38413 | 6 | 945 / 883 | 3 HP damage across two positive limb results at tick 38013 |

All sixteen learned flags for actions 114-129 persist. Gaea Strike II is an
important readout distinction: the enemy regenerates 1 HP between casts, so
comparing only the two field-return snapshots would hide its genuine 1 HP
damage. Effect-adjacent snapshots establish the actual reduction.
Meteor I (book 419/action 130) also consumes one, costs 38 MP, deals 5 HP damage,
learns and returns at tick 39917; ranks II-IV remain outside this snapshot.
No encounter reset or error-level engine event occurs in these incremental
reads. This still does not establish final session teardown.

The next read ends at tick **46448**, sequence **264198**, byte offset
**207527643**. Meteor II consumes at 41215, selects action 131, costs
846 -> 800 MP, reports 6 HP damage and returns/learns at 42045 with eight
copies left. Meteor III consumes at 42591, selects action 132, costs
801 -> 747 MP, reports 13 HP damage and returns/learns at 43409 with seven
copies left. All prior ranks remain learned; IV is not yet consumed in this
snapshot. Keep later events pending rather than infer completion.

The owner uses item 336 to heal/refill between ranks rather than a control
override. Its first effect at tick 25095 changes HP 351 -> 999 and MP
451 -> 999, consuming 10 -> 9 and returning at 25233. A second use consumes
9 -> 8, reports 805 HP / 334 MP recovery and returns at 35851 with both
resources at 999. Thus full restoration also works with the larger setup
maxima; this is separate from uncapped fixed-amount Mana Tonic coverage.

Earlier sweep JSON snapshots were reconciled with adjacent frames around each
`combat.result` in the original run logs, rather than attributing all changes
between menu open and return to the item. Each completed sweep use below
consumed exactly one copy, 10 -> 9, and returned to field control:

| Items / actions | Measured effect at the result boundary |
| --- | --- |
| 323-326 / 224-227 | HP 51 -> 101/151/200/250 respectively; reported recovery 50/100/149/199 |
| 327-328 / 228-229 | MP 2 -> 27 and 1 -> 50; reported recovery 25/49 |
| 329 / 230 | Reported MP recovery 100, actual MP 2 -> 50 at the existing cap |
| 330 / 231 | MP 1 -> 50; reported recovery 49 |
| 331 / 243 | Risk 98 -> 73 exactly at tick 3055; earlier 99 -> 98 occurred before the effect |
| 332-334 / 244-246 | Risk 98 -> 48/23/0 respectively |
| 335 / 232 | HP 51 -> 152, MP 1 -> 50; reported recovery 101 HP / 100 MP with MP capped |
| 336 / 233 | HP 51 -> 250, MP 1 -> 50; reported recovery 199 HP / 49 MP |
| 337 / 234 | HP 51 -> 76 and Risk 98 -> 72 in the effect frame |
| 338 / 235 | HP 51 -> 101 and Risk 98 -> 48 in the effect frame |
| 339 / 236 | Paralysis bit 8192 and its remaining timer clear |
| 340 / 237 | Poison bit 16384 and its remaining timer clear |
| 341 / 239 | Numbness bit 32768/timer clear; walk/run speed 4/9 -> 9/18 |
| 342 / 238 | Curse bit 65536 clears; current STR/INT/AGI 90 -> 100 |
| 343 / 240 | Combined paralysis/poison/numbness bits 57344 and timers clear; speeds restore |
| 344 / 242 | STR-down bit 32/timer clears; current STR 90 -> 100 |
| 345 / 247 | Speed status 2048 appears with timer 20; walk/run speed 9/18 -> 11/22 |

Raw sources are Debug runs `1792652242990814-0`, `1792652393632179-0`,
`1792652583367277-0` and `1792652807264261-0`. The earlier independent
`1792651896838111-0` also records Cure Root recovery 49, HP 200 -> 249.
Snapshot files are `build\quality\items-sweep-Debug-20261022-025722.results.json`,
`025953.results.json`, `030303.results.json` and `030647.results.json` with
the same prefix. Read-only inline JSON inspection produced the tables; no
new analysis program or capture utility was installed.

These are observed values, not assertions that randomized recovery always
equals a nominal guide amount. Higher fixed MP recovery still needs an uncapped
example with enough missing MP. Item 344 has only demonstrated STR-down removal,
not every condition it may support; mixed unaffected-status preservation and
other eligibility cases remain open. Permanent-stat items 346-355 and the real
trap test for 356 retain the separate evidence above.

Fresh noninteractive checks during the owner's run:

- `build\Debug\VS.Tests.exe "[item-plugin],[items],[stats]"` and the Release
  equivalent each pass **16 cases / 1,531 assertions**. Existing executables
  were used; the owner's running host was not rebuilt or replaced. Logs:
  `build\quality\items-manual-review-{Debug,Release}-tests.log`.
- `.venv-3\Scripts\python.exe -m unittest discover -s tools\port
  -p test_encounter_fixups.py` passes **6 tests**; this is the focused file,
  not a rerun of all 19 generator tests. Log:
  `build\quality\items-manual-review-generator-tests.log`.
- Scoped documentation/ledger `git diff --check` passes. CSV parsing retains
  all 4,207 function rows and confirms that only notes for F01889/F02269/F02553
  changed; no copied/compiled/verified/C++/modernized gate was advanced.

Remaining: other ranked/unranked books and progression restrictions, the
uncovered recovery/condition eligibility cases, broader living-enemy F10
re-entry (#82), speed-4 voice command queue exhaustion (#83), final publication
and exact-revision CI. #8, #32 and #84 stay open until their respective gates
are met. Native save/load, comprehensive spell balance and visual fidelity,
and compatibility retirement are separate unverified gates.

## Native audio publication and closure (2026-10-21)

Fixing revision: `4777775f5d16163fc4d0f2740170bdb02fb4d175`, pushed to `work`.
Exact-revision Source Quality run `39707982956` completed successfully, with
all positive controls, source-adaptation tests and quality checks passing.
#7 is closed against its five documented functional criteria; #4 records #8
as the next ordered implementation. EC-02 remains open.

Publication contains 92 scoped files using the configured owner identity and
no additional trailers. An isolated Git index preserved unrelated staging;
mixed documentation was published as audio-only hunks/additive checkpoints.
Unrelated documentation rewrites, reference changes and the pre-existing
`resources/rood-reverse-noprompt` staged removal remain local. Disc-derived
assets/captures were not committed. Publication used the original ongoing-task
authorization; the earlier requests for redundant publication approval did not
establish a new gate. No subjective listening approval is inferred.

Fresh solution builds and full suites after commit each pass **379 cases /
991,535 assertions** in Debug and Release. Logs:
`build\quality\audio-published-{Debug,Release}-{build,tests}.log`.
The staged/native code matched the tested worktree exactly before publication
and remains unchanged after these checks. The complete local quality result
remains **313 existing / zero new**; pushed CI is a separate verified result.

Fresh speed-1 Release Combat Lab run **1792633974201305-0** repeats the
2,500-tick command and scenario below. Its manifest records the fixing SHA
and unrelated dirty state. E044 binds at 515, starts voices 22/23 at 607,
stops at 691 and reuses them for menu audio at 1143. Only one cast occurred.
Final PCM is [-1, 0.999969], with no NaNs/Infs and no shortages after the six
initial pre-attachment requests/2,880 frames. Evidence:
`build\quality\audio-published-solid-shock-{native,astats}.log`, adjacent
`.f32`/`.bmp`, and the per-run Combat Lab directory.

Full credits navigation/listening, every effect/track in context, subjective
or bit-exact parity, general discontinuous/multichannel XA and 5.1 remain
explicit limits. They are not silently promoted by functional issue closure.
The pre-publication checkpoints below retain their historical state and
evidence; their open/uncommitted wording is superseded by this entry.

## Native audio menu, streams and output acceptance (local, 2026-10-21)

Base revision: `43fb192633130720d0db7ec7cbf31ced45f810e9`; audio, dependency,
generator, test, ledger and documentation changes remain dirty/uncommitted.
Unrelated edits, the staged reference-submodule removal and read-only references
are preserved. No commit/push or new custom waveform tool was made. Pushed Source
Quality run `39655769092` covers the base only. #7 and EC-02 remain open.

**Implementation and source-backed corrections**

- The five owned in-game menu wrappers dispatch open/invalid/leave/select/change
  IDs 10/7/6/5/4 with the original channel tag, gain and pan.
- Auditing 290 effect packages found 113 block-13 streams: 112 mono, one stereo.
  Stable native WAV/SFZ/metadata imports preserve separate channel predictor
  histories, pitch and logical endpoints. Borrowed effect slot 3, native-width
  command pointers, typed callbacks, reserved voices, pause/resume, fades, EOF,
  unbind/reset and re-entry are connected. One-shots do not mistake source DMA
  buffer-end markers for logical endpoints.
- Ending XA uses existing FLAC decoding and an independent SDL source. Four
  repeated raw sectors form one actual payload; 8,316 payloads yield 16,765,056
  stereo frames at 37,800 Hz, **443.52 seconds**, not 1774.08. Initial CD volume
  is not duration. Decode/preload occurs on the control thread (about 67 MB PCM);
  callback controls are atomic, with no callback loading/logging. EOF drain,
  stop/reset/reopen and explicit error reporting have tests.
- Original F03198 disassembly defines saved-layer voice conflicts. Native
  whole-object save/restore/swap replaces PS1 byte extents. Regressions cover
  1,536 allocation/key/reservation combinations and all 32 channel records.
- Missing/unknown/extra `streams` CLI arguments now exit 2 before creating an
  output directory. The defect was recorded on #7 before repair.
- Final combat capture exposed **+6.774167 dBFS** output. Original mono streams
  intentionally duplicate their gains across two voices; halving them is not
  justified. A new coherent-voice regression failed at **-1.814161062** (Catch2
  exit 42). The renderer now restores the original final signed-PCM saturation
  boundary after dry+wet/Mono mixing. Stereo/Mono bounds, both limits and quiet
  additive behavior have regressions. This is not a limiter/normalizer or proof
  that intentional source overload distortion disappears.
- Original output semantics were checked against the SPU documentation at
  <https://psx-spx.consoledev.net/soundprocessingunitspu/> and the public
  DuckStation final-mix behavior. No reference implementation was copied/linked.
- A final buffering check confirms `Sound_Cutscene_OnInitialTransferComplete`
  starts both mono voices at the same `0x1100` sample with the same `0x2100`
  repeat address (`SLUS_010.40/25AC.c:6426`). The native pair is not incorrectly
  duplicating two alternating source channels; its original gains remain intact.
- An installed-asset regression now plays the actual ending FLAC through the
  production disc queue and SDL postmix. It confirms audible stereo at sector
  270000 and after a 30-second seek to 272250, with stop/re-entry between them,
  then natural final-sector EOF/drain from 303214. Each start decodes the full
  **16,765,056 frames / 37,800 Hz / stereo** asset. This regression explicitly
  skips when the converted asset is absent; synthetic tests remain independent
  of disc data. It does not navigate the credits or claim full-track listening.

**Final automated verification**

```powershell
# Each configuration was built, then its complete executable suite was run.
MSBuild VagrantStory.Native.sln /m /p:Configuration=Debug /p:Platform=x64
build\Debug\VS.Tests.exe
MSBuild VagrantStory.Native.sln /m /p:Configuration=Release /p:Platform=x64
build\Release\VS.Tests.exe
build\Release\VS.Tests.exe '[audio],[clock]'
quality-native.cmd --changed --tidy
```

- Both final full suites: **379 cases / 991,535 assertions**.
  Logs: `build\quality\audio-ending-final-{Debug,Release}-tests.log`, with
  test-target rebuilds in `audio-ending-{Debug,Release}-build.log`.
  Earlier complete solution builds are in
  `audio-output-final-{Debug,Release}-build.log`; production code is unchanged
  by the additional installed-asset regression.
- Final focused Release audio/clock: **72 cases / 163,008 assertions**.
  Log: `audio-ending-final-focused-tests.log`; failing output-boundary control:
  `audio-output-bounds-before-tests.log`.
- Final complete changed-header quality: **313 existing findings, zero new**.
  `audio-ending-final-quality.log` and `audio-ending-final-quality-report.json`
  include the installed-asset regression; the earlier
  `audio-output-final-quality*` reports are retained.
  The added installed-XA test also passes `audio-ending-quality.log` with
  zero findings/new findings after extracting its shared capture operation;
  no test assertions were removed to satisfy complexity limits.
  The earlier interrupted `audio-closure-quality` run was not a pass.
  No baselines or suppressions were changed.
- Missing-data control: an isolated copy of the Release executable ran
  `[xa-playback][disc],[clock]` with no relative installed asset directory:
  **4 passed / 55 assertions, 1 explicitly skipped**, exit 0. The temporary
  executable/directories were removed without changing installed user data.
  Evidence: `audio-ending-no-assets-tests.log`.
- Before the final mixer-only repair, all **17** port-generator tests passed;
  two source/project generations after ledger reconciliation retained **966**
  identical files, and two Release effect/XA imports retained **250** identical
  files. Evidence: `audio-final-generator-tests.log`,
  `audio-ledger-generation-stability.json`, `audio-final-import-stability.json`.
  The importer used both explicit and PATH-discovered FFmpeg successfully.
- CLI negative controls: `audio-streams-cli-{before,after}.log`; no disc fixture
  was needed. The synthetic original test FLAC provenance is in BUILDING.md.

**Real-input final Release replays**

All speed 1, scripted controller actions, with ordinary production playback.
No HP/event edits or forced completion. Local captures/disc data are not commits
or issue attachments.

1. Menu: `run_native.ps1 -NoCopy -Config Release -SkipIntro -Frames 5800`,
   `-At '4380,4620,5500' -Cap build\audio-output-final-menu`, debug
   `audio,boot,content` logs and the following `-Press` value:

   ```text
   1300:Circle,3000:Start,4000:Start,4300:Triangle,4420:Down,4480:Up,
   4540:Circle,4700:Down,4760:Up,4820:Cross,4950:Cross,5100:Triangle,
   5220:Cross,5400:L2,5401:L2,5402:L2+Circle,5403:L2+Circle,5404:L2
   ```

   All five sound actions occur. The unavailable spell shortcut starts
   instruments 145/141 on voices 22/23 at tick 5404; the field remains visible.
   Float peak **-5.826359 dBFS**, no NaNs/Infs. Only the initial seven
   pre-attachment requests/3,360 frames are missing; no later shortages.
   Maximum callback **1.2869 ms**, maximum control gap **56.5983 ms**.

2. Opening: same helper/config/debug channels, `-Frames 5600 -SkipIntro`,
   only `-Press '1300:Circle'`, `-At '2000,2600,3200,4400,5500'`,
   `-Cap build\audio-output-final-prologue`. Only the intro movie is skipped,
   not the prologue. Float peak **-2.462624 dBFS**, no NaNs/Infs. Only six
   initial requests/2,880 frames are missing; no later shortages.
   Maximum callback **1.21 ms**, maximum control gap **48.8426 ms**.

3. Isolated Solid Shock run **1792631613651010-0**, 2,500 ticks, scenario
   `build\quality\audio-stream-lab.json`: zone 9/room 0, seed 1, post-Minotaur
   Ashley (4,9), SilverWolf definition 4 at (4,10). Command:

   ```text
   VS.CombatLab.exe --scenario build\quality\audio-stream-lab.json --start
   --scripted --speed 1 --frames 2500
   --press 140:Triangle,220:Circle,300:Circle,380:Circle,460:Circle,540:Circle,1140:Triangle,1220:Circle,1300:Circle,1380:Circle,1460:Circle,1540:Circle
   --capture build\quality\audio-output-isolated-solid-shock.bmp
   ```

   The launcher owns/waits for the actual process handle. E044 binds at 515,
   both stream voices start at 607, stop at 691, and become menu voices at 1143.
   Only **one** cast occurs despite the later attempted input sequence.
   FFmpeg reports minimum **-1**, maximum **0.999969**, no NaNs/Infs and a
   **0 dBFS** peak: bounded saturation, not an unclipped-signal claim.
   The only shortage is seven initial requests/3,360 frames.
   `build\quality\audio-output-isolated-solid-shock-{native,astats}.log`
   and adjacent `.f32` are the canonical combat evidence.

Set `VS_AUDIO_CAPTURE` to the corresponding absolute `build\quality\*.f32`
path before each run. The final menu/opening files use `audio-output-final-*`.
An earlier combined launch did not await the GUI Combat Lab before starting the
desktop replay; its correct run is `1792631275979452-0`. Its primary captures
are separate, but it is not isolated audible-output evidence. The independent
repeat above supersedes its combat acceptance. No owner processes were stopped.

**Listening preparation and remaining acceptance**

- Portable reference recordings:
  `build\duckstation\videos\Vagrant Story 2026-10-21-20-42-56.m4a` (menus) and
  `Vagrant Story 2026-10-21-20-52-47.m4a` (unskipped opening).
  Existing MediaFoundation AAC was used; these are not lossless references.
- Screenshots match the manor card (native tick 2000/reference second 65),
  Ashley entering (2600/75), the later hall view (3200/85) and the guard's
  "Have you found Sydney?" dialogue (4400/110). The reference screenshot's
  recorder timer, rather than wall time alone, anchors the excerpt.
- Existing FFmpeg made PCM16 audition WAVs without gain normalization:
  `audio-listen-menu-native.wav` (native 71-92 s),
  `audio-listen-menu-reference.wav` (recording 94-115 s),
  `audio-listen-opening-native.wav` (32-57 s),
  `audio-listen-opening-reference.wav` (58-85 s), and
  `audio-listen-solid-shock-native.wav` (isolated combat 8-20 s).
  All are under `build\quality`; FFprobe verifies 21/21/25/27/12-second durations.
  These cover matching scene/action windows, not sample-exact synchronization.
  Earlier approximate/clipped listening excerpts are not final acceptance.
- **Owner listening feedback is pending.** The owner was unavailable when
  prompted with the five audition files; acceptance was not inferred.
  No subjective parity is claimed.
  Primary capture excludes independent XA/movie sources and physical device
  gaps. Full ending-credit navigation/listening and all 113 streams in context
  are not verified. General discontinuous/multichannel XA, 5.1, unused actor
  transition APIs and DMA/device-shaped compatibility retirement remain outside
  the verified scope; the producer/cleanup audit lists these explicitly.
- Obtain current scoped commit/push authorization and verify CI on the fixing
  revision before closing #7. Do not use the base run or this local checkpoint
  as pushed acceptance. The owner was unavailable when publication was requested;
  authorization was not inferred. EC-02 retirement remains a separate gate.

## Native audio fidelity (local, 2026-10-21)

Base revision: `43fb192633130720d0db7ec7cbf31ced45f810e9`, with the audio,
generator, ledger and documentation changes dirty/uncommitted. Unrelated edits
and read-only references are preserved. No commit or push was made. Pushed
Source Quality run `39655769092` covers the base only. #7 and EC-02 remain open.

**Implemented scope**

- All **83 nonempty WAVE banks** import; the other 18 files are one-byte
  placeholders. The nine previously rejected mapped banks contain exact-end
  or erased final DMA-padding records, not corrupt active ADPCM. Those records
  remain explicitly unavailable. Equal sample/loop offsets inside valid data
  still decode, and malformed/truncated active samples still fail.
- WAVE0032 uses 64-byte AKAO articulations: twelve Q20 pitch entries and an
  unpacked envelope. Its base pitch/root and ADSR are converted; complete source
  articulation bytes survive metadata export/load. All-zero pitch tables are
  unavailable records. No missing waveform is fabricated.
- Native shared noise uses the 44.1-kHz clock/parity rules independently of voice
  pitch. FM drives unsmoothed sfizz pitch from the preceding voice's envelope
  output before speaker gains, with voice-zero exclusion, original pitch bounds
  and held sample position at zero pitch. Sample/noise toggles preserve the
  prepared program and envelope. The AKAO sequencer remains the sole sequencer.
- Reverb implements all ten original preset choices with a native float
  reflection/comb/all-pass network at 22.05 kHz, documented 39-tap filters,
  signed wet gains and clear/reset semantics. The SDK preset/depth/noise calls
  and boot enable/disable consumers migrate through copier fixups to typed
  APIs. Seven SDK ledger rows are `[M]`, not retired/deleted `[x]`.
- Mono applies after dry+wet mixing. This was necessary to preserve the existing
  setting once stereo reverb was enabled; stereo tail state survives re-entry.
  Whole-tick publication, off-callback program preparation/destruction and the
  demand-driven stall-safe boundary remain intact.

**Regressions caught before acceptance**

- Both new exact-end/padding tests failed before the importer repair
  (`fidelity-padding-before-{build,tests}.log`).
- The first real Sound-menu replay exposed stereo reverb leaking through Mono.
  Two mono windows had difference energies **284.315 / 292.059**. The production
  SDL-postmix regression also failed (`fidelity-mono-before-tests.log`).
  The defect was recorded on #7 before the final-mix correction. The final
  replay has exact channel equality with nonzero energy in both mono windows.
- An integration tail test caught processing reverb inside the voice loop;
  processing now runs once per output frame, including when no voice remains.
  Earlier quality findings were fixed without baseline changes or suppressions.

**Final checks**

- Debug and Release solution builds:
  `MSBuild VagrantStory.Native.sln /m /p:Configuration=<config> /p:Platform=x64`.
  `test-native.cmd Debug` and `test-native.cmd Release` each pass
  **355 cases / 830,955 assertions**. Each
  `build\<config>\VS.Tests.exe "[audio],[clock]" --reporter compact` passes
  **48 cases / 2,428 assertions**.
  Logs: `build\quality\fidelity-final-{Debug,Release}-{build,tests,focused}.log`.
- `quality-native.cmd --changed --tidy`: **313 existing findings, zero new**.
  Final report: `build\quality\fidelity-verified-full-quality.log`.
  Scoped whitespace checks and all four `test_encounter_fixups.py` tests pass.
- Two `copy_to_native.py` / `gen_solution.py` runs leave **956 source/project
  hashes unchanged**, including owned implementations. Evidence:
  `build\quality\fidelity-final-generation-stability.json`.
- Two unfiltered `VS.Extract audio Native\data\disc Native\data` runs produce
  **5,993 identical native asset files**. Evidence:
  `fidelity-final-all-bank-import-{1,2}.log` and
  `fidelity-final-bank-stability.json` under `build\quality`.

**Actual playback**

1. Final speed-1 Release replay, 8,100 ticks: MUSIC016/WAVE0069 ->
   MUSIC110/WAVE0091 -> MUSIC054/WAVE0011, with two menu open/close cycles.
   There are six FM note-ons and 622 reverb-routed note-ons, no audio errors,
   and no stream shortages after startup. Seven pre-attachment startup requests
   remain. Maximum callback **1.1046 ms**, maximum control gap **49.3567 ms**.
   Captured **6,480,000 frames** are finite and unclipped; peak **-5.826359 dBFS**.
   This run also overlapped the final source-quality analysis.
2. Final speed-1 Release Sound-menu replay, 3,800 ticks: actual
   Stereo -> Mono -> Stereo choices. Both 96,000-frame mono windows have
   **zero channel-difference energy**; restored stereo windows have
   difference energies **356.100 / 371.779**. Captured **3,035,200 frames**,
   peak **0.472805**, no nonfinite or clipped samples. Maximum callback
   **0.2982 ms**. Images show the selected Mono/Stereo labels.
   Evidence: `fidelity-final-release-settings-{native.log,metrics.json}`,
   adjacent `.f32`, `fidelity-final-{mono,stereo}-label.png` under
   `build\quality`, and `build\fidelity-final-release-settings`.
3. Earlier in this pass, before the final mono-only correction, a speed-1
   Debug replay kept the full prologue (only the intro movie was skipped).
   It reached 5,100 ticks with ten FM and 885 reverb note-ons, no audio errors
   or post-startup shortages, a **111.7357-ms** control gap and **8.1576-ms**
   maximum callback. Its 4,079,600 frames peak at **-2.462681 dBFS**, with no
   clipping/nonfinite samples. Evidence: `fidelity-debug-prologue-*`.
   The final Debug build/full suite and production postmix regressions were
   rerun after the mono correction; this earlier replay is not relabeled as
   a final-tree Debug replay.
4. These scene replays did not request noise. Noise is verified separately
   through the production channel/mask/key-on bridge and actual SDL postmix
   output over a one-second control-thread stall, plus deterministic clock,
   pitch independence, live toggle, release and reset tests.

Final transition replay:

```powershell
$env:VS_AUDIO_CAPTURE = Join-Path $PWD "build\quality\fidelity-final-release-transitions.f32"
tools\test\run_native.ps1 -NoCopy -Config Release -SkipIntro -Frames 8100 `
  -Press '1300:Circle,3000:Start,4000:Start,7000:Triangle,7300:Cross,7600:Triangle,7900:Cross' `
  -At '4190,7100,7400,7700,8000' -Cap build\fidelity-final-release-transitions `
  -LogLevel debug -LogChannels 'audio,boot,content'
Remove-Item Env:\VS_AUDIO_CAPTURE
```

The final Sound-menu replay uses the same 3,800-tick input sequence documented
in the first-sound checkpoint below, with captures at `1750,1900,2550,2750,3350`.
The Debug prologue command uses `-Frames 5100 -Press '1300:Circle' -At '4000,5100'`,
with `-SkipIntro`, speed 1 and its own capture/PCM paths.

An owner-started interactive game reused the shared Release log while the final
transition replay was still running. It was left untouched. The authoritative
trace is the replay's independently captured
`build\quality\fidelity-final-release-transitions-console.log`, through its own
8,100-tick completion marker. The similarly named `-native.log` contains mixed
shared-log content and must not be used for that replay's metrics.
`-metrics.json`, `-astats.log`, the harness completion/captures and the dedicated
`.f32` recording corroborate the owned run.

**Remaining limits:** the float reverb network and sfizz sample interpolation
are not bit-exact console arithmetic. Reference listening/waveform parity,
every track/scene transition, dialogue/stream conversion, other EC-02 consumers
and 5.1 remain open. No custom comparison tool, second sequencer, particle fix
or broad content-authoring migration was added. Disc-derived assets, recordings
and images remain local and uncommitted.

## Demand-driven voice output (local, 2026-10-21)

The audio slice remains uncommitted. Concurrent encounter work advanced HEAD
to `43fb192633130720d0db7ec7cbf31ced45f810e9`; pushed quality run `39655769092`
passed for that base, not these dirty audio edits. Existing unrelated edits,
documentation cleanup and read-only references are preserved.

**Resolved long-stall mechanism:** a fixed PCM queue cannot cover arbitrary
gameplay-thread pauses. The existing sequencer still produces ordered control
ticks, but SDL now renders active sfizz voices on demand. A bounded SPSC queue
publishes complete ticks only; the callback cannot observe half-loaded note
updates. A tick retains its 200-frame interval at 48 kHz. When gameplay stalls,
samples and envelopes continue rather than falling into missing output.
Newly sequenced events can still be late; this is not an independent sequencer.

SFZ loading creates a separate program on the control thread. The renderer
takes ownership through a published command, returns retired programs through
consumed cells, and never frees/loads programs or performs file I/O/logging.
Atomic generation-tagged envelope feedback preserves pending-note allocation.
Reset invalidates old queued commands and clears/reprimes only the native
stream; movie sources remain independent. Closing detaches the callback before
capture/device teardown. The engine must outlive its attached open device.

`VS_AUDIO_CAPTURE` stages rendered PCM in a two-second bounded buffer and drains
to disk from the control thread; overflow explicitly stops capture, not output.
Captured duration now follows device demand, not logical ticks. It still
excludes movie audio, queue padding and physical-device gaps. Accelerated
desktop replay (`--speed > 1`) explicitly retains synchronous rendering and
the stale-PCM queue cap.

**Fresh checks:**

- Debug/Release x64 solution builds pass. `test-native.cmd Debug` and
  `test-native.cmd Release`: **337 cases / 829,988 assertions** each.
  `VS.Tests.exe "[audio],[clock]" --reporter compact`: **30 cases / 1,461
  assertions** each.
- SDL dummy-device postmix regression captures **one full second** while the
  control thread waits. Every 200-frame block after 100-ms warm-up has audible
  stereo energy. The push-only negative control is silent in the same window.
  This tests the actual final mix, not just renderer submission counts.
- Fifty concurrent prepared-program replacements leave another voice audible.
  Regressions cover partial-tick publication, command ordering, pending-note
  envelope visibility, ring wrap, reset with queued programs, live reset,
  device close/reopen and native/movie isolation.
- Two source/project generations preserve **76 owned files** and yield
  identical **775-file** snapshots. Four copy-fixup tests pass.
  Evidence: `build\quality\audio-demand-generator-stability.json` and
  `audio-demand-{copy,solution}-{1,2}.log`. The generator change updates its
  audio fixup description; no generated output was patched manually.
- Scoped `git diff --check` passes. Builds/tests use
  `build\quality\audio-demand-final-{Debug,Release}-{build,tests,focused}.log`.
- Final `quality-native.cmd --changed --tidy` passes: **313 existing findings,
  zero new**, with no baseline changes or new suppressions. Evidence:
  `build\quality\audio-demand-final-quality.log`.

**Production playback:**

1. Unskipped Debug prologue/manor: speed 1, 5,100 ticks, Start at 1000 and
   Circle at 1300, capture 4700. No shortages after renderer attachment,
   despite a **113.1298-ms** control-thread gap. Maximum callback **6.8578 ms**.
   MUSIC016/WAVE0069 plus SFX emit **4,079,600** finite, unclipped frames,
   peak **-3.626178 dBFS**. Only eight pre-attachment startup requests remain.
   Evidence: `build\quality\audio-demand-debug-{native.log,astats.log}` and
   adjacent `.f32`; `build\audio-demand-debug`.
2. Final Release transition replay, speed 1, 8,100 ticks, uses the exact
   Start/menu inputs below. MUSIC016/WAVE0069 -> MUSIC110/WAVE0091 ->
   MUSIC054/WAVE0011, plus two menu open/close cycles. **6,479,600** finite,
   unclipped frames; peak **-5.420009 dBFS**. Maximum callback **1.1126 ms**,
   maximum control gap **50.8112 ms**, zero shortages after renderer attachment.
   Seven initial pre-attachment requests are not counted as verified music
   dropouts. Evidence: `audio-demand-final-release-{native.log,astats.log}`,
   adjacent `.f32`, and `build\audio-demand-final-release`.
3. Release Sound submenu replay, 3,800 ticks, retains actual Stereo -> Mono ->
   Stereo actions from the first-sound checkpoint. Two mono confirmation/
   re-entry windows have exactly equal channels and nonzero energy; restored
   stereo difference energy is approximately **52.96 / 50.90**. Peak
   **0.420216**, no clipping/nonfinite samples, no post-attachment shortages.
   Evidence: `audio-demand-settings-release-{native.log,harness.log}`, `.f32`,
   `audio-demand-settings-contact.png` and `build\audio-demand-settings-release`.
4. `run_native.ps1 -NoCopy -Config Release -SkipIntro -Speed 8 -Frames 5100
   -Press '1300:Circle' -At '4700' -Cap build\audio-demand-speed8 -LogLevel warn
   -LogChannels 'audio,boot'` completes with the expected stale-output warnings,
   no errors or control-queue exhaustion. This is accelerated replay
   compatibility, not real-time listening acceptance.

```powershell
$env:VS_AUDIO_CAPTURE = Join-Path $PWD "build\quality\audio-demand-final-release.f32"
tools\test\run_native.ps1 -NoCopy -Config Release -SkipIntro -Frames 8100 `
  -Press '1300:Circle,3000:Start,4000:Start,7000:Triangle,7300:Cross,7600:Triangle,7900:Cross' `
  -At '4190,7100,7400,7700,8000' -Cap build\audio-demand-final-release `
  -LogLevel debug -LogChannels 'audio,boot,content'
Remove-Item Env:\VS_AUDIO_CAPTURE
```

Use new output paths when repeating. FFmpeg `astats` checks f32le/48-kHz/stereo
PCM; the synthetic SDL postmix regression provides the direct stall test.
No custom waveform-comparison tool was written. Disc-derived recordings and
images remain local and uncommitted.

**Remaining #7 limits:** reference listening, dry reverb, unsupported FM/noise,
dialogue streams, the previously documented rejected banks and 5.1. This fixes
the demonstrated output-starvation mechanism, not every fidelity requirement
or OS/device scheduling failure. Particle work remains deferred.

## Music restoration and scheduling (local, 2026-10-21)

Base remains `30f5369504d9438bc15b91f5a440383a4c6fba44`; audio and these
follow-ups are dirty/uncommitted. Existing unrelated edits and reference
checkouts are preserved. CI `39610716791` verifies the pushed base only.

**Owner evidence:** menu sounds and in-game voice/weapon clips were audible,
but music was absent and playback crackled. The attached particle reference
is deferred: fire should stay mostly in place and smoke rise upward. Both
owner images remain read-only; no particle code or custom waveform-comparison
tool was added during this audio work.

**Music root cause:** G00248/G00249/G00250 were separate allocations even
though the original addresses are fields at offsets `0x28/0x2C/0x30` of G00246
(`Field_ZoneContext`). Initialization enabled music on the canonical object,
but loading checked a different, zero-valued switch. The data ledger now
generates typed field views, not native byte-offset aliases. All seven
`[music-state]` checks failed before repair and pass after; normal zone load
and event opcode `0x92` now reach the populated music slot.

**Two scheduling defects, separately demonstrated:**

- Native output had no scheduling headroom despite frame-batched production.
  Open/reset now primes 2,400 frames (50 ms); an observing SDL callback counts
  shortages and requests rebuffering on the simulation thread. Reset affects
  only native audio, not separately queued movie data. No sfizz work or
  logging runs in the SDL callback.
- The host advances at `60000/1001` Hz, while VSync advanced counter 2 as if it
  were 60 Hz. That supplied about 47,952 frames/s to a 48,000-frame/s device.
  Counter advancement now shares the host's rational field-rate constants and
  carries integer fractional clocks, retaining the 240-Hz sound callback.
  Six callback-count checks failed before repair. The passing regression
  exercises 60,000 actual VSync fields (1,001 seconds), 240,240 callbacks,
  stop/resume and counter reinitialization.

**Fresh validation on the repaired clock:**

- Debug/Release x64 solution builds pass with the documented MSBuild command.
  `test-native.cmd Debug` and `test-native.cmd Release` each pass
  **331 cases / 828,750 assertions**. The final suite includes concurrent
  encounter changes; the preceding audio-only checkpoint passed
  330 cases / 828,736 assertions. These unrelated changes are preserved, not
  claimed as audio implementation. Each configuration's
  `VS.Tests.exe "[audio],[clock]" --reporter compact` passes
  **24 cases / 223 assertions**. Device tests cover exact buffer size,
  native/movie isolation, repeated clear, stale-output discard and reopen.
- `quality-native.cmd --changed --tidy` passes on the final shared worktree:
  **313 existing findings, zero new**. No quality baseline changes or new
  suppressions. Evidence: `build\quality\music-clock-final-quality.log`.
  Scoped `git diff --check` also passes.
- Two source/project generations preserve all **76 owned files** and produce
  identical **775-file** manifest/project snapshots. This snapshot scope is
  narrower than the first-sound checkpoint's 995-file snapshot below.
  Four `test_encounter_fixups.py` tests pass. Evidence:
  `build\quality\music-generator-stability.json`, `music-copy-{1,2}.log`.
- Two imports of wave-file IDs **0, 5, 11, 69, 91, 200** produce the same
  **612 files**. Evidence: `build\quality\music-asset-stability.json` and
  `music-required-bank-import-{1,2}.log`. The build/asset setup now includes
  all six banks; menu-only assets are insufficient for music.

**Replay evidence and limits:**

- The earlier, buffer-only Debug 5,100-tick New Game replay reduced shortages
  from 36 requests / 14,040 frames to 14 / 6,560, all remaining before gameplay
  (startup/movie loading). Its nine post-New-Game shortages disappeared.
  Logs/PCM: `build\quality\music-queue-{before,after}-native.log` and
  `music-queue-after.f32`. It produced 4,075,200 finite, unclipped stereo
  frames, peak about 0.754891. This earlier result did not prove long-run
  clock correctness; the later replay exposed the timing mismatch above.
- Final Release 8,100-tick replay uses real Start skips and controller menu
  entry/exit, not forced music events: MUSIC016/WAVE0069 ->
  MUSIC110/WAVE0091 -> MUSIC054/WAVE0011, with rendered music/SFX. It emits
  **6,481,600** finite, unclipped stereo frames, peak **-5.420009 dBFS**.
  Menu captures at 7100/7700 show open, 7400/8000 closed. Music voices continue
  through both cycles. This is audio transition evidence, not combat testing.
- The final replay still records startup shortages and **one 135.4006-ms
  submission stall at tick 7289**, with nine requests / 3,960 frames missing
  before rebuffering. The earlier drift-related shortage at tick 5707 is gone.
  Do not call playback dropout-free: simulation-thread mixing still depends on
  host responsiveness. Next observation is the call stack/timeline behind
  such long stalls, plus an owner listening check. Do not enlarge the buffer
  arbitrarily or move shared sequencer state onto the device thread.

Final replay (speed 1; prologue skips are intentional for transition testing):

```powershell
$env:VS_AUDIO_CAPTURE = Join-Path $PWD "build\quality\music-clock-fixed-release.f32"
tools\test\run_native.ps1 -NoCopy -Config Release -SkipIntro -Frames 8100 `
  -Press '1300:Circle,3000:Start,4000:Start,7000:Triangle,7300:Cross,7600:Triangle,7900:Cross' `
  -At '4190,7100,7400,7700,8000' -Cap build\music-clock-fixed-release `
  -LogLevel debug -LogChannels 'audio,boot,content'
Remove-Item Env:\VS_AUDIO_CAPTURE
```

Persistent evidence: `build\quality\music-clock-fixed-release-{native.log,astats.log}`,
the adjacent `.f32`, and `music-clock-menu-contact.png`. FFmpeg `astats`
analyzes f32le / 48,000 Hz / stereo submitted samples. That capture excludes
queue padding, physical output gaps and movie audio; it is not a loopback
recording. Build/test logs use `build\quality\music-clock-{Debug,Release}-*`.
Disc-derived captures/assets stay local, uncommitted and unattached to issues.

**Remaining #7/EC-02 limits:** reverb is dry; FM/noise voices are explicitly
reported/stopped. Reference listening/waveform fidelity, supported dialogue
streams, wider transitions and 5.1 remain incomplete. Mapped music banks
47/58/77/82/89 fail offset validation; 72/74/86/87 fail ADPCM predictor/shift
validation. These importer gaps are logged on #7, not claimed disc corruption
or bypassed with fabricated samples. WAVE0032 remains a separate unsupported
layout. #7 stays open; no commits or pushes were made for this audio slice.

<!-- Original handoff begins below. -->

# Handoff: prologue entry and shared rendering

## Current checkpoint

The ordered work tracker is [#4](https://github.com/JEschete/VS-NativePort/issues/4).
The maintained-production quality pass in [#5](https://github.com/JEschete/VS-NativePort/issues/5)
is verified locally. The Guildenstern textbox fix in
[#1](https://github.com/JEschete/VS-NativePort/issues/1) is now verified locally.
The menu-rendering/lifecycle checkpoints now cover those owner-reported text,
equipment/Status, Map/Data and Options gaps. The owner approves the menu-first
split: #39 retains actual combat/audio/cleared-puzzle consumer acceptance rather
than blocking the rendering/lifecycle gate on later ordered work. Final gate CI
and closure are recorded in #2. Issue #6's AI, initial-drop, encounter
progression and defeat-flow acceptance is now verified and closed; the next
ordered implementation is sound functionality #7. Menu shortcuts and setting
values are not combat verification.
Preserve the original dialogue pause and button advance; do not shorten the scene delay.

The later Quick Manual report is repaired and verified below (#66). Its scoped
publication/CI record belongs in that issue. The newly reproduced Score submenu
fast-fail remains #65; sound functionality #7 is the next ordered gameplay
implementation after #6's verified closure.

### Native audio first-sound slice (local, uncommitted)

Base revision: `30f5369504d9438bc15b91f5a440383a4c6fba44`, with the audio
implementation/documentation changes dirty. Pre-existing instruction edits and
reference-repository pointers are preserved. CI run `39610716791` passed for
that pushed base only; it does not verify the dirty audio implementation.

**Bounded result:** actual title-menu controller actions now reach the existing
AKAO sequencer, native WAV/SFZ instruments, sfizz and SDL float output. This is
not a second sequencer or an SPU-register synthesizer. sfizz 1.2.3 is a pinned
vcpkg overlay dependency. F03159, F03163-F03169 and F03178 now have owned C++
implementations, with behavior and EC-02 status tracked separately.

**Fresh validation:**

- Debug and Release x64 solution builds both pass with the documented MSBuild
  command. `test-native.cmd Debug` and `test-native.cmd Release` each pass
  **325 cases / 828,691 assertions**. Each configuration's
  `VS.Tests.exe "[audio]" --reporter compact` passes **17 cases / 141 assertions**.
  Coverage includes ADPCM predictor/loop/bounds vectors, integer/shape rejection,
  deterministic assets, measured 400/800-Hz playback, overlapping voices,
  release/reset/restart, gains and missing-instrument failure.
- `quality-native.cmd --changed --tidy` passes: **310 existing findings, zero
  new**; no baseline changes or new lint suppressions. Changed headers cause the
  full owned-translation-unit analysis. Scoped `git diff --check` passes;
  unrelated instruction-file whitespace was not modified.
- Two copy/solution generations preserve all **76 owned source/header files**;
  their **995-file** source/project snapshots are identical on the second run.
  `python -m unittest discover -s tools\port -p test_encounter_fixups.py` passes
  all four tests, including both native SFX wiring and the existing saved-layer
  null guard. Evidence: `build\quality\audio-generator-stability.json`.
- `build\Debug\VS.Extract.exe audio Native\data\disc Native\data 0 5 200`
  produces **289 files** identically on two runs. Invalid IDs `-1`, `200x`,
  missing file `9999`, and unsupported WAVE0032 all fail explicitly.

**Real output, not just note logs:** both speed-1, 3,800-tick Debug and Release
replays produce **3,035,200 stereo frames** at 48 kHz. The native mix is finite,
unclipped (peak about **0.420216**), zero before the first sound and zero after
the last effect. The actual Sound submenu changes Stereo -> Mono -> Stereo:
the two mono-confirm/re-entry windows have **exactly identical channels**;
stereo windows have nonzero channel-difference energy (about 51-53).
Captures show the selected labels, and no audio errors/250-ms queue resets
occurred. The opening movie source opened at 37,800 Hz stereo and Start skipped
it; the native capture does not include that source or prove its output fidelity.

Replay, with `$config` set to `Debug` or `Release` and distinct output paths:

```powershell
$env:VS_AUDIO_CAPTURE = Join-Path $PWD "build\quality\audio-sound-settings-$config.f32"
tools\test\run_native.ps1 -NoCopy -Config $config -SkipIntro -Frames 3800 `
  -At '1510,1750,2250,2550,3050,3350,3600' `
  -Press '1200:Down,1300:Down,1400:Down,1600:Circle,1800:Down,2000:Circle,2400:Circle,2600:Up,2800:Circle,3200:Circle,3400:Cross' `
  -Cap "build\audio-sound-settings-$config"
Remove-Item Env:\VS_AUDIO_CAPTURE
```

The verified Debug capture directory is `build\audio-sound-settings-debug-final`;
Release is `build\audio-sound-settings-release`. Logs, PCM and quantitative
results are `build\quality\audio-sound-settings-{debug,release}-{native.log,metrics.json}`
and the adjacent `.f32` files. A local, disc-derived navigation-sound listening
clip is `build\quality\audio-menu-preview.wav`; do not commit or attach it.
Final builds/tests and header-gate reports use the `build\quality\audio-final-*`
prefix. Use the waiting harness above for automated evidence: a bare GUI exe
invocation can return before its new log is ready. Two Down presses select
Vibration in this state, not Sound; the earlier `audio-settings-release`
capture is a vibration-settings run and is not mono verification.

**Remaining #7/M5 limits:** no owner listening/reference-waveform acceptance
yet. Reverb remains dry; noise/FM voices are logged and stopped. Menu select
itself requests FM, so it is not fidelity-complete. WAVE0032's different layout,
broader banks, music/dialogue/streams, full scene transitions and bank replacement,
worst-case synchronous SFZ-load latency, and 5.1 are not verified/implemented by
this slice. #7 and EC-02 remain open. Next: reference-check these menu sounds
and extend the same native path to one title/music track, rather than resuming
standalone backend-build experiments.

### Owner playtest and shared-runtime repairs (2026-10-21, stopping checkpoint)

Owner feedback on the `df748add` build confirms ground-enemy pathing/rotation
are solved and the prologue is fully playable apart from the listed gaps.
This is fresh owner evidence, not a new automated all-content/reference pass.
Stutter #70 is closed. Sound outside the opening FMV remains #7.

Two further fixes are implemented locally; publication/CI evidence belongs on
their issues rather than being inferred from the older `df748add` CI:

- **#75 Hellhound breath:** G00576 (`0x800F5798`) is the request field at original
  offset `0xD0` inside G00575 (`0x800F56C8`), not independent storage. The data
  ledger now makes it a canonical native field view. Four binding assertions
  failed before the fix; all seven pass afterward. The child-track completion
  guard also uses `childCount`, not original byte offset `0x14`.
  Real speed-1 eight-Hellhound runs
  `1792577241138340-0` (Debug) / `1792577275397935-0` (Release) pass 1600 ticks,
  matching every snapshot and all 6586 RNG records. Breath 147 damages limbs
  0/1/3/4 at tick 534 (24/28/20/27 HP), returns control at 664, and attacks again
  at 1572. No HP/progression edits or replacement effect were used.
- **#79 post-defeat arena:** the old Game Over event VM resumed in the next
  healthy encounter. Before repair, menu 63/game-over false changed to menu
  7/game-over true, then field rendering was disabled despite HP 250.
  Gameplay event initialization now resets playback/start/finished state,
  native thread cursors and wait timers. The initializer regression failed
  with phase 4 before repair and now passes. Actual Minotaur defeat at tick 658,
  F6/F7/F10 and same-room restart pass in Debug `1792578734686541-0` /
  Release `1792578765217479-0`; captures show a real arena, not Continue/Title
  on black. Same-room followed by a different zone (9) also passes in
  `1792580129571764-0` / `1792580170367914-0`.

Read-only frame diagnostics now include game-over state, active menu, field
rendering enable and frame-buffer index. The state was measured before choosing
the reset fix; no fake story transition or forced health restoration was added.
Evidence and exact validation commands are in [COMBAT_LAB.md](COMBAT_LAB.md).
Stopping validation passes both full builds and suites: **306 cases / 828512
assertions per configuration**. Focused changed-code tidy reports zero findings
and zero new findings. Native views check 621 fields / zero mismatches; event
dispatch checks 179 / zero errors; audit/fixup regressions pass 13 and 3 tests.
Two copier/scaffolder passes preserve 70 owned paths and 775 generated/project
paths. Exact pushed-revision CI and closure evidence will be recorded on #75/#79;
the source-wide shared-header gate is not inferred from the focused local gate.
The owner authorized committing/pushing this bounded checkpoint before stopping.

Remaining owner findings are tracked, not claimed fixed:

| Finding | Tracker / next observation |
| --- | --- |
| Birds spawn/path incorrectly; missing window god rays; generally scattered particles | #76; identify aligned original/native effect-space cases. |
| D'tok fire breath does nothing | #75 follow-up; prologue/Lab and missing VFX/damage distinction still unspecified. |
| Bat flight terrain | #77 is closed after commit `155a7438`, four 5000-frame Debug/Release Combat Lab replays, 310-case suites, zero-new-findings quality, and green exact-revision CI `39599618892`. |
| Eight Minotaur Zombies crash | #78; independently reproduced even in a fresh process after the reset repair. Frame cursor is already corrupt on entry to room drawing at tick 209. |
| Suspected multi-part abilities | #80; Hellhound's four-limb case now works, other exact action/target cases remain unspecified. |
| Selected body-part glow | #81; reported, not independently repaired. |
| Equip paper-doll weapon missing on first entry | #14; reported first-entry/re-equip distinction preserved. |
| Intro-controlled bosses unavailable | #16; intentional source-profile gate remains a roster limitation; do not fabricate hostile metadata or disable validation. |

### Bat flight terrain bounds (#77, 2026-10-21)

The Bat crash came from `Battle_Ai_LowestReachableGroundHeight` passing the
packed tile word (including its `0x40` search/status byte) as the X coordinate
to `Field_GroundHeightAtTile`; the failing word `0x400D0008` therefore became
X=`1074593800`. The owned implementation now derives X and Z separately from
the typed tile coordinates and direction delta. The two flight helpers reached
on this path are also implemented from their assembly: the segment query samples
16 signed ground heights, and the room-ceiling query returns the configured
value or zero when the room navigation context is absent.

`Field_TryGroundHeightAtTile` reports unavailable terrain/bounds without
changing the output value; the existing required-height query retains its
explicit diagnostic and abort guard. Regression coverage checks a valid
negative-height neighbor, a room-edge direction that must not sample outside
the grid, 16-point segment sampling, the configured/missing ceiling, and
valid/edge/missing-height-map results.

Production Combat Lab replays at normal speed use the original single-Bat and
eight-Bat scenarios with no player input (`--scripted`). Debug runs
`1792584773462606-0` / `1792584773457949-0` and Release runs
`1792584856381806-0` / `1792584856370509-0` each complete all 5000 frames.
The Bat actors select Ashley, their positions change during the encounter, and
Ashley reaches real HP 0/Game Over; no terrain-height abort is logged. These
four scenarios provide bounded Bat evidence, not full flight-roster coverage.
Both full test suites pass **310 cases / 828541 assertions**; changed-code tidy
reports **0 new findings**. Two copier and two scaffold passes are stable, and
the owned flight source survives regeneration. Commit `155a7438` passed exact-
revision Source Quality CI `39599618892`, and #77 is closed. #6 remains open
for its wider acceptance.

### #6 AI, initial drop and defeat-flow acceptance (2026-10-21)

F02771 (`MENU/MENU7.PRG/260.c:3504`) linked the negative-fade tile by
truncating its host pointer into the renderer's 24-bit link field. The native
renderer uses `LegacyGpu_Handle` IDs for that field. The new regression failed
before the fix (`LegacyGpu_Pointer` returned null for raw link `0x9f13a4`,
instead of resolving the fade packet); the copier fix now uses the native
handle and the test passes.

Current Debug and Release Combat Lab runs `1792588127503092-0` and
`1792588245223129-0` use the original Zone 9 Zombie definition and 32 ordinary
Circle inputs at 40-tick intervals. The Zombie moves toward Ashley, targets
actor 0 and executes attacks that lower Ashley's HP from 250 to 230. Its HP
falls from 121 to 49 by tick 1100 and 0 by 1150; actor 2 is removed by 1350,
when its real source drop (item 323, count 2) is queued. Ashley survives. The
New Game reward gate remains false and inventory unchanged, so this verifies
the initial drop queue, not inventory delivery or full drop-table coverage.
Exact args, manifests and traces are in the Combat Lab run folders.

Fresh full New Game prologue controller replays use 839 scheduled controls and
only Start at tick 1000 to skip the intro. Debug and Release each complete
48000 ticks / 22 captures with exit 0; logs load MAP004 then MAP005 (Debug
20425/23120; Release 22561/25240), and the final captures remain in the live
battle. Neither route edits HP, position, RNG, events or progression.

Fresh Debug/Release defeat-flow replays each complete 26000 ticks / eight
captures with exit 0. The real Game Over menu appears after natural combat
damage; Down at 21000 and Start at 21100 select Title, with the native title
transition at 21177. Exact inputs, manifests, logs and captures for both replay
groups are under `build/quality/issue6-final-{victory,gameover}-<config>/`.

Both configurations pass **311 cases / 828561 assertions**. Changed-code tidy
reports **0 new findings** (6 baseline), and a second copier pass is stable.
The F02771 fix in `ae2e30e1` passed exact-revision Source Quality CI
`39603967134`; documentation revision `b7f97ce0` also passed CI
`39604755566`. #6 is closed with its explicit acceptance verified. Complete
item/equipment drop-table coverage remains at priorities 09 and 10; broader
roster coverage remains separately tracked.

For #78, temporary stage tracing in run `1792579333054718-0` showed a valid
cursor at tick 207, then `0x00000557FECC05FB` before any room-render stage at
209. The write producer is not established. Next useful observation is a native
write watchpoint on `Render_frameScratch.primitiveCursor` during that pre-draw
action/camera update; do not assume packet capacity or solve it by reducing
enemy count. Temporary probes were removed from both generator and output.
The owner was unavailable for the D'tok/multi-part clarification; do not invent
expected damage or modify a scripted sequence on that basis.

### Combat Lab profiles, awareness and particles (2026-10-21)

The combined worktree based on `fd291cfe` now has selectable Prologue,
Post-Prologue and representative Post-Minotaur Ashley profiles. Normal startup
is unchanged. Presets/logs persist the profile; legacy presets default to
Prologue. The advanced profile learns/maps real table entries, not every action.
Weapon/resource/status restrictions still apply, including drawing the weapon
before Break Arts. The stage-3 gate enables Map/Score, not a new Setup root menu.

Actual speed-1 inputs in Debug/Release execute Solid Shock (97), Heavy Shot
chaining (22), the equipped art (188), and Cure Root (224). The item capture
is during the original particle animation, not after it ends. Natural awareness
markers now render through native links and a native frame matrix. Testing
also exposed/repaired full-pool emission UB (#73) and a one-element renderer
table overwriting the effect package (#74). Live pause, single-step, F10 return,
restart and switching back to a genuinely limited Prologue menu pass.

Full Debug/Release suites: 302 cases / 828486 assertions each. Focused action/
rendering regressions: 15 / 1006. Two generator runs preserve 70 owned paths and
775 generated/project paths; native views check 621 fields with zero mismatches,
event dispatch checks 179 with zero errors. Exact commands, source/dirty-state
limits, run IDs, captures, timing and generation hashes are in
[COMBAT_LAB.md](COMBAT_LAB.md), not inferred from successful exits.
Publication/CI evidence belongs on #69, #71, #72, #73 and #74.
The full changed-source/shared-header tidy gate passes with zero new findings
(316 existing); no baseline was changed.

The owner confirms stutter is solved and live combat feels natural; #70 is now
closed. Console/file batching retains full Trace/RNG files and improves
the matched normal-speed run, but Debug still exceeds the 16.667 ms simulation
budget on 41/600 ticks. Final four-wolf Release simulation is 1.910 ms mean /
4.863 ms maximum; maximum presentation interval is 26.526 ms. Redraw-throttling
experiments worsened pacing and were removed. These measurements are diagnostic
history, not an outstanding stutter blocker.

The owner's new Hellhound fire-breath crash is reproduced as #75 at `df748add`,
Release speed 1, zone 11/room 0, Prologue Ashley (8,12), eight definition-3
Hellhounds at (5..12,13), seed 1. With no injected gameplay input, action 147
loads effect 94 and faults reading `0x2` at tick 482 in shared
`Effects_TickChildEffectTrack` (F01563, Timeline.c:168). Local owner/repro runs:
`1792572817191067-0` / `1792573135209075-0`. The failing expression reads
`Effects_package->request->count`; its producer was unknown at this initial
triage. The later canonical-request repair is documented above. The scattered-particle report
is tracked separately as #76 pending a measured spatial/reference comparison.
Specials/magic share action records, target/result processing and the effect VM/
timeline, with actor-specific assets and effect-specific plugins. Do not invent
an independent Hellhound-only fire-breath implementation to bypass the fault.

The pending ground-navigation/motion/cleanup work and older actual defeat,
queued-drop, Game Over and ordinary prologue victories are documented separately
in that checkpoint. They do not establish complete roster behavior, delivered
reward coverage, every ability, C++ promotion or compatibility retirement.
#6, #16 and #42 retain those wider acceptance limits; the gap report has 50
C-backed entries remaining. Do not reorder #4 based on easy closures.

### Standalone Combat Lab checkpoint (2026-10-20)

Owner-requested tooling for #16 now launches independently as `VS.CombatLab.exe`;
no entry was added to the normal game menu. [COMBAT_LAB.md](COMBAT_LAB.md) records
the exact local configurations, inputs, evidence paths and limits. Source was
`69a88618` plus lab edits and pre-existing dirty AI work; none of those results
should be relabeled as clean-revision verification.

Real two-enemy attacks/retaliation matched across all 1500 Debug/Release frame
snapshots. Real item inputs consumed one Cure Root and restored HP after enemy
damage. Live pause/single-step/stop/restart restored initial actor HP/MP.
Full suites pass 271 cases / 827297 assertions in each configuration; scoped
tidy has zero new findings and repeated generation preserves owned files/output.
The lab also exposed and repaired omitted player-settings initialization and
the unencoded G00917 item-shortcut label (F02553), including its reset initializer.

The host is an initial same-zone, source-profile-backed slice. Full roster,
loadout editing, spell/ability coverage, collision-aware placement, defeat/rewards,
sound and reference parity remain open. #16, M8 and the ordered gameplay
priorities are not closed by this checkpoint. Publication/CI evidence belongs
in the issue separately from these dirty-worktree tests.

The later lab-only publication snapshot excludes the dirty AI changes. It builds
in Debug/Release and passes 239 cases / 827068 assertions per configuration.
Fresh combat, item consumption/healing and live lifecycle runs succeed independently;
both log files persist real results. Startup parse errors are now logged too.
See the isolated-publication section of COMBAT_LAB.md for exact runs and differences
from the earlier combined worktree. #16 remains open for full coverage.

### Combat Lab font readability (#67, 2026-10-20)

The owner screenshot exposed unreadable solid menu glyphs. Canvas treated every
nonzero 4-bit index as the same bright foreground color, so black outlines and
gray antialiasing filled in the letters. The fix reads the checked compact-font
CLUT at (864,223) from SYSTEM.DAT's first interface block, uploaded to (832,0)
by BATTLE/5BF94.c:3484. The image0 font remains on page (768,0); paletteOffset
is a different palette. Transparency is tested after lookup, including opaque
black 0x8000, and each RGB555 shade is tinted without flattening it.

Four asset-free Canvas regressions cover shading, tint, zero/nonzero-index
transparency, clipping and malformed image/palette offsets (66 assertions).
The solution generator compiles the actual Canvas source into VS.Tests; no
game-library dependency on the lab executable was added. Two generation runs
preserved project hashes. Scoped tidy reports zero new findings, no baseline edits.

Verification is based on `30cf4dde` plus this fix. Full Debug/Release combined
worktree suites pass 275 cases / 827368 assertions, including unrelated dirty AI
work. The isolated publication worktree excludes that AI work; full Debug/Release
suites pass 243 cases / 827134 assertions each and its editor capture matches the
combined-worktree Debug/Release output byte-for-byte (SHA-256
`30ED2C791662CF763ABF54768F4BAF0D69A43CC24088CE5220AED8D24D70B7E2`).
The isolated CI-equivalent quality gate also passes with zero new findings.

Local evidence: `build/quality/issue67/`, including `editor-before.bmp`,
`editor-after.bmp`, `editor-debug-after.bmp`, `editor-publish.bmp`, the corresponding
Windows OCR text, build/test logs/XML and quality reports. Capture command:
`VS.CombatLab.exe --frames 2 --scripted --capture <path>`; the isolated run also
passes the original extracted data root via `--data`. Before-capture OCR returned
no text; the corrected capture recognizes the title, actions, warnings and controls.
This is editor-readability evidence, not gameplay completion. Assets/captures
remain local and are not committed. Pushed revision and CI/closure evidence belong
on #67; #16 remains open for the wider combat audit.

### Combat Lab unsupported placement (#68, 2026-10-21)

The owner's four-Silver-Wolf setup allocated four actors, not two. In zone 9,
room 0, the line at (1,12), (3,12), (5,12), (7,12) puts the outer actors on an
invalid floor profile and a blocked tile respectively; later support queries
reach -3000. Original local run: `1792560287160365-0`, first ready tick 1928.
MPD tile-cell section is index 2, not index 3. Do not infer invalidity from a
negative Y alone: valid raised terrain is explicitly covered by the tests.

The lab now classifies complete static tiles with the existing collision decoder,
using isolated map bytes and normalizing the disc profile count as the loader
does. It marks supported tiles green and rejects unsafe starts with actor/tile
reasons, without relocating actors or changing production collision. It is a
conservative preflight, not footprint/ceiling/dynamic-support/path verification.

Use F10 to return to the builder, including while paused. A supported four-wolf
layout is Ashley (4,9), enemy 4, quantity 4, offset (-2,+3), spacing 1; clear
existing groups first. Rebuild and restart after ending the active debugger
session; the initial workspace build could not update that running process.

Isolated `0a074f32` plus this scoped fix passes Debug/Release full builds and
248 cases / 827175 assertions each, excluding unrelated AI work. Scoped tidy
has no new findings and no baseline edits. Both configurations reject the unsafe
setup before spawning and retain four supported enemies through 599 ready frames.
Release run `1792561359130756-0` verifies F6 at ready tick 57, F10 return at 68,
editor restart at 221 and another F10 return at 368. Debug/Release previews match.
Exact commands, local presets,
replay evidence and limits are in COMBAT_LAB.md and `build/quality/issue68/`.
Do not relabel the isolated wolf placement runs as verified AI or combat.
Publication and CI evidence belong on #68; #16 and ordered gameplay work stay open.

### Guildenstern window dialogue (2026-10-19)

Verification used `35618ef2` plus the feedback-link fix and renderer regression.
The original line is **"...Where are you, Sydney?"**, not "Where are you hiding Sydney?".

- A hardware write watch identified F02456, `Effects_ScreenDistortion_DrawFeedbackTiles`
  (`SCREFF2.PRG func_800F986C`, `src/GIM/SCREFF2.PRG/0.c`), writing native address bits
  into ordering entry 143. The invalid link terminated rendering before the textbox chain.
  Both the feedback and framebuffer-restore links now use `LegacyGpu_Handle`, through
  `copy_fixups.py`. The original effect, event delay and controller wait are unchanged.
- The regression in the existing GIM renderer tests checks both framebuffer buffers,
  feedback depths 1, 2 and 128, both links, packet-buffer guards and complete traversal
  through a lower dialogue packet. `[render][text][feedback]` passes all six combinations;
  all 19 `[render]` cases pass (200 assertions) in Debug and Release. The complete Debug
  suite passes 96 cases / 30713 assertions.
- Clean Debug New Game replay: `VS.Desktop.exe --frames 10000 --speed 16 --press
  1000:Start,1300:Circle,3800:Circle,3980:Circle,4160:Circle,4340:Circle,4520:Circle,
  4700:Circle,4880:Circle,5060:Circle,5240:Circle,6400:Circle --capture
  build/quality/issue1-window-final --at 6000,6300,6450,7000,8500,9900 --scripted
  --log-level debug --log-channels boot,input,content,render`. Join the wrapped
  `--press` list without whitespace. The line is visibly present at 6000 and 6300;
  Circle at 6400 advances into ZONE002 at 6450 and MAP002 at 6486. Later ZONE003/MAP003
  loads at 8757/8793. Exit 0. No event flags or timing were bypassed.
- Logs: `build/Debug/issue1-window-final.log` and `-stderr.log`; rendered captures:
  `build/quality/issue1-window-final/`. Earlier before/fixed captures and the write-watch
  stack remain under `build/quality/issue1-window-*` and
  `build/Debug/issue1-dialog-write-watch-stderr.log`. Temporary probes are removed.
- Release repeats the same input schedule with `--frames 7000 --capture
  build/quality/issue1-window-release --at 6000,6300,6990`; the line is visually verified
  at 6300 and the same Circle-driven ZONE002/MAP002 transition occurs at 6450/6486.
  Exit 0; logs are `build/Debug/issue1-window-release.log` and `-stderr.log`.
- A second `copy_to_native.py` run preserves manifest SHA-256
  `D52C8D3AE62F712540F2499CAB05D74EE96E8558CDB7790B3AF2D8041111E5E3`.
- `quality-native.cmd --changed --tidy`: zero new findings; no baseline refresh.
  Existing uninitialized actor-dialog `texture` warning is separate and not claimed fixed.
  This verifies the window dialogue, not full scene-rendering parity or the prologue.
- Latest pushed Source Quality run `39239821866` is successful for `35618ef2`.
  The fixing commit and closure evidence are recorded in #1. No push was performed for
  the window-dialogue fix.

### Menu copy helpers and quick test route (2026-10-19)

Issue #19 supports the active menu-rendering issue #2. Verification used `5694a3d3`
plus the owned halfword-copy implementations and memory regressions.

- F04188/F04189 are implemented in `Core/Memory/WordCopy.c`, not generated output.
  Original `573B8.c:242-269` copies leading halfwords, then loads four halfwords before
  storing each eight-byte block. The native code preserves that forward overlap order,
  halfword alignment and full native pointer bytes. Nonpositive sizes are no-ops;
  incomplete halfwords/blocks are ignored rather than entering invalid assembly loops.
  The header declarations now have C linkage for native C++ consumers.
- Before repair, the new direct copy tests failed 25 assertions in three cases, leaving
  destinations unchanged: `build/Debug/issue19-before-test.log`. After repair,
  `[core][memory]` passes six cases / 141 assertions in Debug and Release. The complete
  Debug suite passes 100 cases / 30805 assertions. Changed-source quality with tidy
  reports zero new findings; no baseline refresh.
- Use real Start presses to skip the opening and the A. J. Durai quote for menu testing:
  `--press 1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle` loads MAP009 at
  4058 and MAINMENU.PRG at 7005. Start at 1500 is too early in this replay. Repeated
  Circle inputs can open/close the battle sphere instead of testing the main menu.
  Keep the complete prologue for combat troubleshooting and verification.
- Menu/preview replay in both configurations: `--frames 10000 --speed 16 --press
  1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle,7600:Circle,8200:Down,8500:Circle
  --at 7500,7900,8400,9000,9900 --scripted`. Debug captures/logs use
  `issue19-firstroom-preview`; Release uses `issue19-firstroom-preview-release`
  (Release captures at 7500,8400,9900). Captures are under `build/quality/`; logs are
  under `build/Debug/`, including `-stderr.log`. Both exit 0. The item preview renders
  Ashley without the observed companion-slot corruption; the trace remains at slot 1.
- Main menu text/graphics still render as black bars and are NOT claimed fixed.
  Issue #2 remains active. The copy-helper fixing commit and CI/closure evidence are
  recorded in #19; the main-menu renderer is the next controlling path to inspect.

### Native menu rendering and workspace (2026-10-19)

Verification used pushed `e4442056` plus the pending row/workspace changes. #2 is
still active; its native workspace defect is #27 and the Status crash is #28.

- F01348 used PS1 scratch offset `+0x008`, which is the scene ordering pointer in the
  native frame rather than the overlay pointer. It now accesses the native fields and
  uses a packet handle. F02548's scrolling gradient also now uses a native handle.
  Root menu labels/gradients, setup labels and return to gameplay are visually verified.
- #27: F01367 allocates `40 * sizeof(Menu_Framework_MenuRow) + 0x124`, places string
  scratch after the forty rows, then row-type scratch 96 bytes later. F01349 clears the
  same native-sized workspace. Original PS1 `0xB24/+0xA00/+0xA60` sizes overlapped the
  native rows after pointer growth. Fixes are in the copier, not direct generated edits.
- Existing GIM render tests check normal and scrolling link traversal, separate scene
  and overlay tables, packet bounds, and real menu initialization of the final native
  row and all scratch bytes with two guards. `[render][menu]`: two cases / three checks.
  Release `[render]`: 21 cases / 203 assertions. Full Debug: 102 cases / 30808 assertions.
  Changed-source quality with tidy: zero new findings; no baseline refresh.
- Debug menu/item/setup/return inputs: `1000:Start,1300:Circle,3000:Start,4000:Start,
  7000:Triangle,7600:Circle,8200:Down,8500:Circle,9200:Cross,9600:Cross,10000:Triangle`.
  Both configurations complete 11500 ticks at speed 16, with stable preview companion
  slot 1. Debug captures: `build/quality/issue2-items-row-after`; Release captures:
  `build/quality/issue27-release-menu-return`; corresponding logs are in `build/Debug/`.
  Root-only restored capture: `build/quality/issue2-mainmenu-row-after/frame_8100.png`.
- Second copier run is stable: manifest SHA-256
  `8887CF7992242337205C67EDDA97A0F69906119AE435A740E3906E19A6D39E4F`.
- Remaining: item overview/category row/detail text is still absent despite visible
  category icons and Ashley preview. Status entry (`7500:Down,7800:Circle` after the
  same menu-open inputs) crashes in nested `Render_FormatEncodedText` at tick 7833;
  logged before repair as #28, evidence `build/Debug/issue2-status-release-stderr.log`.
  Passing unit tests and root-menu screenshots do not establish completion of #2.
  Fixing commit/CI and closure evidence for the narrower workspace repair are in #27.

### Status crash and embedded menu strings (2026-10-19)

#27 is closed in pushed `599d3037`, green Source Quality run `39270071612`.
#28 verification used that revision plus the following pending changes; #2 remains active.

- The copier treated `.vsString` source includes as one-word zero initializers and did
  not register them with the module loader. It now uses the original source include
  path and splat asset extents, allocates rounded `u_short` capacity, and loads the exact
  source byte count (including valid odd-byte tables without reading the next asset).
  Unresolved assets fail generation. Twenty-four embedded menu text arrays are bound;
  startup now loads 358 globals from 61 modules, 19 pointer tables, zero failures.
- Status source table: MENU4.PRG `0x4C48`, 3520 bytes / 1760 words. Inventory menu text:
  MENU3.PRG `0x6BB8`, 944 bytes / 472 words. The real Status format is valid nested text;
  its previous zero template led the formatter outside the table at tick 7833.
- F02682 also enabled Status drawing before its actor-switch producer loaded preview
  slot 1. Drawing is now enabled after that producer runs. F02662/F02663/F02677/F02678
  now retain native packet handles for indicators, leader lines, fades and backgrounds.
  No downstream null guard or scene bypass substitutes for those corrections.
- Existing content tests check the actual loaded Status/inventory tables and odd-byte
  destination padding. A guarded Status packet regression traverses the entire ordering
  chain under both background orders. Clean Debug suite: 103 cases / 30813 assertions.
  Release `[content][module],[render]`: 27 cases / 257 assertions. Changed-source quality
  with tidy: zero new findings, no baseline refresh. All temporary probes are removed.
- Clean Debug/Release replay: `--frames 10500 --speed 16 --press
  1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle,7500:Down,7800:Circle,
  8400:Down,8700:Right,9000:Left,9500:Cross,9800:Cross --at 8200,8800,9300,10300
  --scripted`. Both exit 0. Evidence labels `issue28-status-final-debug` and
  `issue28-status-final-release` under `build/quality/` and `build/Debug/` (including
  `-stderr.log`). Prior failing stacks: `issue2-status-release` and `issue28-status-bound`.
- Copier second generation is stable: SHA-256
  `58C0CAE0B7E0B320CD7CBABFE325AF479AF37886A86B0951DB96076C0F28487A`.
- Status now visibly draws Ashley, gauges and limb lines and returns to the root menu;
  some labels/numeric text and inventory category/detail text are still missing. #2 is
  not complete. Do not treat the #28 crash repair as complete menu or combat parity.
  Fixing commit and pushed CI/closure evidence for #28 are recorded on that issue.

### Assembly-only gap census (2026-10-20)

The requested full inventory is complete; conversion has resumed using the
located C counterparts, checked against the original assembly. See
[ASSEMBLY_GAPS.md](ASSEMBLY_GAPS.md) and its complete CSV links.
All 4,207 catalog rows / 3,762 port targets are reconciled against actual native
bodies, aliases and template implementations, plus four read-only references.
The initial census was 185 stubs and one partial native entry: 145 had C counterparts,
31 have assembly but no located matching C, and 10 PLG074 `INCLUDE_ASM` entries
have no locally extracted listing. C counterparts are not assumed correct or
ABI-compatible; most PC references still use guest pointers/offsets.

The tagged `7E05C.s` has 21 entries: six implemented, 14 open with C references,
and one assembly-only gap, F01833 `Field::IsPointOutsideSolidVolumes`.
Candidate copy/append/sort/readiness C already exists in read-only rood-boi;
check it against assembly before modernizing instead of treating it as absent.
Preserve the existing `// decomp: ... (src/...:line)` comment above each future
implementation. The auditor itself changes no Native or resource code. Re-run
`python tools\port\audit_assembly_gaps.py --publish` after an authorized reference
update; the report records revisions/fingerprints for assumption reconciliation.

### C-backed AI planning primitives (#6/#42, 2026-10-20)

Owned C++ [planning primitives](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp)
replace eight stubs: F01684-F01688 and F01794-F01796. This covers complete and
overlapping candidate copies, bounded append/exclusion bookkeeping, signed
descending candidate ordering with the original midpoint partition's tie order,
clamped simulated stat deltas, selective planning-stat snapshots, action resource
usability, target eligibility and the condition flags consumed by targeting.
No guest pointer accessors or original AI-state byte offsets were imported.
Reference symbols/assembly paths are above each implementation; donor attribution
and the pinned CC0 reference are in [THIRD_PARTY.md](../THIRD_PARTY.md).

Assembly/C comparison also exposed a coupled existing F01825 error:
`SetActorVerticalMovementSteps` selected the minimum instead of the maximum.
An independent regression failed for both argument orders before the correction;
signed comparisons and pre-truncation byte packing are covered. The older
layout fixture's matching minimum assumption was corrected too.

The first batch reduced the inventory to **178 remaining entries**: 177 stubs and one
partial; **137 have C counterparts**, 31 have only located assembly, and ten
still lack a locally extracted listing. #42 retains the original 186-entry
checklist; do not remove completed IDs from that historical baseline.

The actual full prologue reaches MAP003 and the 22,000-tick limit in both builds
with all requested captures and no logged errors. This is integration/regression
evidence, not an enemy-action, damage, defeat or drop completion claim. Remaining
scoring, cooldown/execution, randomized selection and movement stubs still block
#6's encounter acceptance criteria; neither #6 nor #42 is closed by this batch.

Debug and Release full suites each pass **159 cases / 364,083 assertions**.
The audit's 12 tests, 620 native-view fields and 179 event-dispatch signatures
pass; copier output and owned implementations are idempotent. New C++/header/test
quality checks report zero findings without changing the quality baseline.

### Continued C-backed fills and native weapon reach (#6/#42, 2026-10-20)

Nineteen further entries are implemented, bringing the original C-backed
checklist to **27 of 145**; **118 C-backed gaps remain**. The current complete
inventory has **159 gaps** (158 stubs and one partial): 118 C-backed, 31
assembly-only, and ten missing local listings.

- [Decision primitives](../Native/Game/src/Battle/Ai/DecisionPrimitives.cpp):
  F01674-F01677, F01683, F01690-F01693 and F01770. These cover execution and
  cooldown gates, signed-halfword condition scoring, action/target classes,
  fallback caching, weight-capped randomized promotion with exact draw ordering,
  and the actor random accumulator.
- [Terrain height](../Native/Game/src/Field/TerrainHeight.cpp):
  F01759-F01764. All 65,536 terrain-cell encodings are tested through tile,
  packed-tile and world-coordinate consumers, including retained collision flags,
  signed 15-bit heights and final-grid-cell bounds.
- [Navigation primitives](../Native/Game/src/Battle/Ai/NavigationPrimitives.cpp):
  F01775/F01789. Neighbor-step checks use signed direction deltas and reject
  closed grid boundaries; reservation preserves all other navigation bits.
- [Actor commands](../Native/Game/src/Battle/Ai/ActorCommands.cpp): F01720.
  All 18 intent cases now produce real native actor-command packets, including
  attack targeting, turning, gait choice, jump/drop selection, position staging,
  reservation and one-per-tile collision slides. Typed packet and movement
  helpers replace the monolithic assembly dispatch.

The native command/scoring paths deliberately do not recreate unused GTE
scratch-register clobbers or the proximity routine's guest pointer-array
pseudo-node. Meaningful arithmetic, packet fields, ordering and RNG consumption
remain covered; resources stay read-only.

Live read-only PDB observations also exposed an existing coupled range bug:
T00529 `ActionRangeView` was missing from native AI-view rebasing. Its stats
pointer was at offset 96, while canonical `ActorAiState.stats` is at 104.
An independent offset regression failed before the fix; the actual
`CalculateActionRangeSquared` consumer now returns the expected 25,600 in its
fixture, and live enemy candidates changed from zero reach to **65,536**.
The registered mapping repair increases checked native-view fields to **621**.

Debug and Release full suites each pass **187 cases / 760,898 assertions**.
The native view/dispatch contracts and 12 audit tests pass, with no quality
baseline changes. Ordinary controller input (after cancelling a stale attack
menu) now provides a measured in-range replay: Ashley moves from `(1216,0,576)`
to `(1272,0,1273)`, within squared distance 9,077 of actor 3. Both builds complete
all 24,000 ticks and ten read-only observations/captures with no logged errors.
Enemy action selection remains unproven: the engagement-ready/try-engage stubs
are the next live blockers, alongside the remaining pursuit/navigation work.
Do not close #6 or treat these checkpoints as completion of #42.

### Active engagement, enemy damage and real prologue victories (#6/#42)

Seven more C-backed entries are implemented in
[Engagement.cpp](../Native/Game/src/Battle/Ai/Engagement.cpp):
F01628/F01629 and F01631-F01635. The native room membership mask/count and
engaged-list links now drive actual join, 32-frame readiness, leave/rejoin
cooldowns and awareness markers. Existing buildup and actor-command consumers
are exercised by [EngagementTests.cpp](../Native/Tests/src/EngagementTests.cpp).
The original return value of `TryEngageActor` is restored as an integer result;
existing callers may still discard it.

The C-backed total is now **34 of 145**, with **111** remaining. The complete
inventory has **152** open entries. Both complete builds pass **196 cases /
760,994 assertions**; native-view/dispatch contracts, audit tests, targeted tidy,
quality baselines and copier idempotence pass.

Ordinary-input replays now demonstrate actual enemy command opcode 24 and
player damage, not merely populated plans. The Debug close-range observation
records Ashley HP **250 -> 240 -> 217 -> 0**, with the native defeat flag
`0x20`; Release also executes real enemy attacks and damage. This does not by
itself assert the complete subsequent game-over UI.

A separate normal-input Debug fight defeats both original enemies: actor 3 is
HP 0/defeat flag `0x20` at tick 19,000, actor 2 at 19,900, while Ashley survives.
The real prologue then advances to **MAP004 at 20,441** and **MAP005 at 23,120**.
No HP, position, RNG or progression memory was patched. All 24,000 ticks and ten
requested read-only observations/captures complete with exit 0 and no errors.
An initial real item-drop consumer and autonomous pursuit/movement are still
unverified; #6 remains open. The observed actor cleanup stub is the next
continuation, not an excuse to stop at another function-count checkpoint.

### Native actor teardown and navigation-override ownership

Five more entries are implemented in
[ActorCleanup.cpp](../Native/Game/src/Battle/Ai/ActorCleanup.cpp):
F01790, F01804, F01822/F01823 and F01842. Cleanup now releases reservation and
override storage, detaches linked/special actors and encounter membership,
clears marker/list references, releases actor AI storage and handles shared
orbiting-boss ownership. Ashley's original persistent AI lifetime is retained.
Room bounds and gate-mask operations retain the verified original byte semantics.

The prerequisite navigation-override pointer at original `0x18C` was also
missing from native rebasing. A new independent view-width regression failed
**4 == 8** before repair. All AI views now reserve that full native pointer;
perception's former integer presence test is a pointer, and later view fields
remain aligned with canonical storage. Real high-end heap allocation/free tests
use addresses above 4 GiB and verify complete heap recovery and repeat cleanup.
Shared encounter-mask access is extracted rather than duplicated.

The C-backed total is **39 of 145**, with **106** remaining; **147** total gaps
remain. Full Debug/Release suites each pass **203 cases / 761,040 assertions**,
with 621 native-view fields, 179 dispatch contracts, 12 audit tests, idempotence
and zero new quality findings. Both ordinary-input victory replays still
complete all 24,000 ticks/captures without errors and preserve the real
MAP003 -> MAP004 -> MAP005 transition. Initial drop evidence and autonomous
pursuit remain outstanding; work continues instead of declaring the tracker done.

### Canonical native math reuse and packed tile distance

Eight further C-backed entries are complete. F01771 now has a typed owned C++
[packed-tile distance](../Native/Engine/src/Core/Math/TileDistance.cpp)
implementation. F03397/F03398/F03407/F03410/F03411/F03412/F03413 are deliberately
redirected by the copy generator to the existing canonical native GTE/math API,
not duplicated as another PS1 implementation. The missing wide-to-short vector
normalizer is added to that shared API; short-to-short normalization reuses it.
Original symbols/assembly paths remain above the generated aliases.

The audit verifies both the exact emitted alias and a real implemented native
target body before counting a redirect. Wrong/commented aliases, declarations
without definitions and redirected stubs remain gaps. Its expanded 13-test suite
checks these failure cases.

The original C-backed set is now **47 of 145**, with **98** remaining
(**139** total gaps). Both complete suites pass **211 cases / 826,703 assertions**.
Tests cover exact alias function addresses/native pointer return types,
translation retention, distinct mixed XYZ/YXZ orders, Q12 cross products and
square roots, normalization output shapes/pads/in-place use, and all 65,536
unsigned packed tile-coordinate combinations. Copy idempotence and all native
contracts pass without changing quality baselines. The ordinary-input Debug
victory/progression replay also completes cleanly.

### D'Tok crash investigation and native animation outputs (#63/#64)

The owner's null-model perception failure is reproduced on untouched
`d357de4f`: actor/model slots contain only 0 and 2, but AI slot 3 still contains
the earlier guard. At tick 35,909, perception reads address `0x8`. Actor AI
teardown was still a stub in that revision. Existing fix `6eae802d` releases
that state; current replays have no orphan AI at the same boss transition.
The owner's exact revision is unknown; do not infer it from the report.

A separate current failure was confirmed after tick 36,500 on `c98afc09`.
A private Windows debugger, attached only after capture 33,900, recorded
`0xC0000409`, fast-fail code 2 (stack-cookie check). Local PDB public symbols
and disassembly locate `__report_gsfailure+0xA`, called from
`Battle_PlaySpecialActionAnimation+0x41E`. This was not a hang or the historical
perception AV. The old October 19 hang log is unrelated.

`FindActorAnimationHeader` writes a native `u_short*`, but special-action,
Break Art and spell consumers supplied four-byte integer locals through
incompatible function-pointer casts. The generator now emits real pointer
locals and direct declared calls. Enemy pose tests the complete pointer rather
than its low word; death and locomotion callers also use the declared contract.
The current upstream reference agrees with the special-action pointer type;
all reference trees remain read-only.

The independent `[animation-headers]` real-consumer test first exited
`0xC0000409`. After repair, all 16 absent/present, stage and consumer
combinations pass. Animation locking isolates lookup, fallback-facing/pitch
and optional follow-up behavior from pose decoding. Debug and Release each
pass **212 cases / 826,719 assertions**. Generator idempotence, 621 native
view fields, 179 dispatch contracts and changed-source quality pass without
baseline changes.

Both configurations then complete the full-prologue ordinary-input route
through **48,000 ticks / 22 captures**, exit 0, no logged errors and no orphan
AI. Captures show the live boss encounter beyond the old failures. The
retained route does not defeat this boss: Ashley remains out of attack range.
This verifies these crashes, not autonomous pursuit, boss victory, drops,
complete defeat flow, audio or the remaining #6 acceptance criteria.
Evidence stays local under `build/quality/issue63/` and
`build/quality/issue6/dtok-fixed-{debug,release}-encounter/`; no gameplay
memory writes or progression bypasses were used. The gap count remains
**47/145** C-backed complete, **98** remaining.

### Quick Manual frame pointers and deferred text lifetime (#66, 2026-10-20)

Verification uses `26d74433` plus the scoped Manual generator/test changes.
The original topic page crashed at tick 9011 in
`Menu_Manual_DrawControlsBackground`: manual producers treated the native
frame as two four-byte scratch words. Generator fixes for F04163, F02997,
F03000-F03005 and F03007 use the typed native frame cursor/ordering fields.
The independent panel regression first faulted with a packet buffer above
4 GiB, then passed exact byte/count, bounded link traversal and guard checks.

The no-crash replay exposed a separate blank-text defect in F02995:
`CopyPageText` passed a stack array to the deferred textbox consumer. Its
generator fix retains the 1024-byte page buffer for that consumer's lifetime;
the shared non-owning textbox API is unchanged. A regression failed after
reusing the returned stack before repair; full ten-line and partial final-page
encoded output now pass. Original `494.c:325-397,531-575` supplies the paging,
return, line-offset and control-code semantics; references remain read-only.

- Full Debug and Release suites each pass **214 cases / 826,757 assertions**.
  Focused `[menu][manual]` coverage is two cases / 38 assertions.
  `check_native_views.py`: 621 fields, zero mismatches;
  `check_event_dispatch.py`: 179 signatures, zero errors.
- `quality-native.cmd --changed --tidy`: zero new findings, no baseline refresh.
  The second copier run preserves manifest SHA-256
  `21B918C14B721015F11C76E92BAB3B732CE9D2387E94F24613942CAE1A6911C3`.
- Both configurations complete 18,000 ticks, 16 requested captures, exit 0 and
  no logged errors. The menu-only replay uses speed 16, scripted input and:
  `1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle,7600:Down,7700:Down,
  7800:Down,7900:Down,8000:Down,8100:Down,8300:Circle,9000:Circle,10100:Down,
  10198:Square,10200:Square+Down,10400:Circle,10800:Cross,11200:Cross,12000:Triangle,
  12700:Circle,13300:Circle,13800:Start,13900:Up,14400:Start,14900:Circle,
  15300:Down,15600:Circle,16200:Down,16600:Circle,17000:Cross,17400:Cross`.
  Join the wrapped press list without whitespace. Capture ticks are
  `8750,9500,10300,10700,11100,11500,12500,13000,13700,14200,14700,16000,
  16400,16900,17300,17900`.
- Visually checked text/illustrations on page 1 and page 2, remembered re-entry,
  the final Basic Controls page 22/22, Start returning to page 1, a second
  Game Modes topic, topic/root back and the final field return. Local captures
  and logs: `build/quality/issue66-final-{debug,release}/`; full test/build logs
  and `issue66-quality-accepted.log` are alongside them.
- Portable DuckStation attempts in `issue66-reference` and
  `issue66-reference-confirm` did not reach the matching Manual scene. They
  are not visual parity evidence. This verifies native content/navigation
  against the original data and source semantics, not pixel-perfect reference
  rendering or all fourteen topics.

An earlier re-entry schedule mistakenly navigated the remembered root selection
to SCORE. Selecting its first TITLE submenu produced `0xC0000409` after tick
13700; the actual route, capture and exit are recorded in #65. It is not a
Manual regression or an assumed unimplemented cause. Neither #65 nor the
remaining combat/audio/save acceptance is closed by this repair.

### Native pursuit modes and tracked target producers (#6/#42, 2026-10-20)

After the #66 publication/CI gate, ordered #6 resumes from `23fdeab9` plus
the owned [movement behavior implementation](../Native/Game/src/Battle/Ai/MovementBehavior.cpp).
Five C-backed entries are implemented: F01661/F01662 and
F01779/F01780/F01786. This brings the original C-backed checklist to
**52 of 145**, with **93** remaining; the complete audit has **134** open
entries (133 stubs and one partial), including 31 assembly-only and ten
missing local listings.

- All seven movement modes preserve the original return values, mode-specific
  resets, one-byte/two-byte-stride counters and no-op re-entry behavior.
  Patrol selects enabled waypoints by Manhattan distance and retains the first
  tie. The four-row bound comes from actor initialization's two-bit row selector.
  Cooldown refresh retains unrelated navigation bits and the signed enable flag.
- Native aliases expose the original mode, patrol and navigation fields without
  changing ownership or recreating scratch memory. The navigation word is named
  only after widening its adjacent override pointer. The view verifier records
  original locations for these new aliases; independent native pointer/patrol
  offset checks and all existing view contracts pass.
- Real model fields supply current/planned target tiles and moving-platform
  waypoint gates. Assembly confirms that the planned X coordinate is the signed
  low halfword of `supportOffset`, not its whole integer. The implementation
  preserves that width, current/planned tile padding, support-branch retained
  bytes, target-cache zero distinction and the original intermediate OR-combined
  target word. No donor guest-pointer helpers or address offsets were imported.
- Focused `[ai][movement-behavior],[ai][pursuit-targets]`: **17 cases /
  256 assertions**. Both full suites pass **231 cases / 827,013 assertions**.
  `check_native_views.py`: 621 fields / zero mismatches;
  `check_event_dispatch.py`: 179 signatures / zero errors. The 13 gap-audit
  tests and 24 quality positive controls pass; use `.venv-3\Scripts\python.exe`
  for the latter's installed tools. Do not run positive controls concurrently
  with quality analysis: both generate the same compiler database.
- `quality-native.cmd --changed --tidy`: zero new findings, no baseline refresh.
  Second-generation manifest SHA-256 is
  `CFF00291602B60A29016539B31A6808F6A63E45F8B9401D5C1C4194D11794AA7`.
  Owned files survive regeneration; projects come from `gen_solution.py`.
- Both full-prologue controller replays finish 48,000 ticks, all 22 captures,
  exit 0 and no logged errors. Debug enters MAP004/MAP005 at 20,441/23,120;
  Release at 20,761/23,440. The previous Release route was 20,763/23,440;
  configuration-dependent encounter timing is not newly claimed deterministic.
  Both final captures retain the live boss encounter, not a return to title.
  No HP, actor position, RNG or progression memory was patched.
- Exact 839-input schedules, capture selections and arguments are retained in
  `build/quality/issue6-pursuit-{debug,release}-encounter/arguments.json`,
  with captures and `vs-native.log` alongside. Corresponding full build/test
  logs and `issue6-pursuit-final-quality.log` are in `build/quality/`.

The live continuation is now **F01799 `DispatchAiLocomotionMode`**, reached at
tick 19,481 in Debug and 19,845 in Release. Its ground destinations
F01784/F01785 and directional resolver F01752 remain unimplemented; follow the
original six-entry table, route-gate continuation, recovery and actor-command
consumers rather than declaring pursuit complete. Autonomous movement, an
initial real item drop, complete player-defeat flow and boss victory remain
unverified. #6 and #42 stay open; these are verified producer contracts and
integration regressions, not completed encounter acceptance.

### Menu-only Options acceptance gate (#2/#39, 2026-10-20)

Following the explicit owner-approved menu-first split, all nine Options now
have verified menu selection/return coverage. #39's five remaining real
consumer criteria stay open; this section does not mark them implemented.

- Beyond the completed Simple Map, Information, Cursor Memory and Vibration
  cases, Debug and Release each complete 36,000 ticks / 41 observed stages for
  Ability Timing, Weapon Status, Armor Status and Sound. Each exercises both
  applied values, choice re-entry, a cancelled pending value, submenu/root back,
  real field return and subsequent Triangle-driven re-entry.
- Actual packed bits 5/6/7 change 1 -> 0 -> 1 and remain unchanged on cancel.
  Sound changes Stereo 0 -> Mono 1 -> Stereo 0, preserving the value on cancel.
  The other packed flags, Cursor Memory byte and menu-enable word are checked
  against each case's baseline. No unrelated prefix byte changes occur.
- Puzzle Mode is legitimately available in this fresh scene: the disabled
  flag is zero without any override. Both configurations complete 15,900 ticks
  / 11 stages. Setting bit 3 changes 0 -> 1 -> 0; the original puzzleMode state
  synchronizes to its inverse, stays unchanged on cancellation and survives
  field return/re-entry. The original message explicitly says this mode only
  affects cleared levels; no cleared-level puzzle result is claimed here.
- A persistent UI regression presents both legitimate availability states in
  an isolated fixture. Only Puzzle Mode's row is disabled when required, all
  other rows remain enabled, and the temporary Cursor Memory override restores
  the caller's value. It does not force a progression flag in a live game.
- Actual screenshots retain all nine labels, the selection arrow, encoded
  instant contextual text and the expected applied/highlight distinction.
  Field-return observations require menu state 63 and no bound rows, not merely
  a clean process exit or moving logical ticks.
- Related `[menu]` tests pass 16 cases / 214 assertions in both configurations.
  Complete suites pass 145 cases / 360,614 assertions each after the new gate
  fixture. Focused quality with tidy has zero new findings, no baseline changes.
  Arguments, settings/selection observations and captures are retained under
  `build/quality/issue39/menu-matrix-debug`, `menu-matrix-release`,
  `puzzle-menu-debug`, and `puzzle-menu-release`; no assets are uploaded.
- This closes the reported menu rendering/lifecycle slice after its fixing
  revisions and final regression CI succeed. Actual timing indicators,
  weapon/armor combat feedback, stereo/mono output parity and cleared-puzzle
  effects remain #39 work in their ordered stages. Save persistence remains
  #18, item effects/crash #8/#32, and actual equipment operations #14.
  Resume #6 with the full prologue and real battle/defeat routes, not menu skips.

### Working vibration output and safe stopping (#39, 2026-10-20)

The owned libpad bridge no longer discards `PadSetAct`'s mutable table. The
original `_padResetDefaults` registers it once; `vs_main_setVibrateParams`
subsequently writes its bytes. The host reads those live bytes each input tick,
mapping the binary small motor to SDL high frequency and the 8-bit large motor
to low frequency. Only the game's canonical actuator alignment is advertised.

- Vibration Off, scripted input, a missing/disconnected pad and pad reset mute
  output. This also suppresses stale registered bytes while the original
  pattern updater is disabled. Device removal and shutdown explicitly stop
  active motors. SDL requests last 250 ms and refresh every 100 ms while active;
  redundant logical-tick commands are avoided. Unsupported/failed SDK requests
  produce warnings, not success-shaped fallbacks. Shutdown uses SDL's C logger
  for a failed stop so cleanup cannot throw through the formatting logger.
- The T00581 copier fix gives each native slot two complete six-word envelopes,
  rather than `channels[10]` aliasing the next slot or overrunning the second.
  Original MIPS at 80043450/8004346C confirms the overlapping PS1 tail and
  0x38 stride; this is independent native runtime storage, not an on-disc
  layout change. The ledgers' original layout-conflict notes remain valid.
- Three real SDL virtual-controller cases failed before wiring output. A
  separate producer storage guard failed 10 != 12 before the copier fix.
  Original pattern parsing, both slots mixing/clamping, full-range intensities,
  live-table changes, Off/mute/reset, disconnect, destruction and a sustained
  refresh interval now pass. `[input]`: 11 cases / 116 assertions in both builds.
  The existing copied-C `/J` already preserves unsigned pattern bytes; the
  initially suspected signed-byte defect was disproven and not patched.
- Debug and Release each complete 20,500 scripted menu ticks with 72
  captures/read-only observations. The real On-confirm preset produces motor
  commands (28 positive samples in Debug, 25 in Release), followed by On/Off,
  cancellation, root return, field recovery and re-entry. Scripted runs
  intentionally do not activate physical devices.
- With explicit owner approval, hidden `[.hardware-rumble]` selects exactly one
  device matching `VS_NATIVE_RUMBLE_DEVICE`. A 320 ms, 25%-command large-motor
  check goes through the same owned input/registered-table bridge and explicit
  stop. Both attempts pass 27 SDK assertions; the first was missed, and only
  the ready-coordinated repeat has owner confirmation: felt it and stopped
  cleanly. The selected device is DualSense Edge Wireless Controller.
  This is not a claim that the owner felt the scripted game-menu preview.
- Real-device tests are never part of normal input/full-suite execution.
  Run the hidden tag only after consent, with the exact device name in the
  environment, and remove that variable afterward. Logs and game-menu/preset
  evidence remain under `build/quality/issue39/`; no assets are uploaded.
- A second copy preserves manifest SHA-256
  `CADEA2A54F52923034FB2DA5B308AC36198021B690404DD13EFEB546B797CAD5`.
  Complete Debug and Release suites each pass 144 cases / 360,588 assertions;
  the explicit physical opt-in is absent. Native view contracts check 620
  runtime fields with zero mismatches.
  The fixing commit, final checks and exact pushed CI are recorded in #39.
  This completes neither combat/audio/progression consumers nor all Options.

### Menu-first Options and Cursor Memory (#39, 2026-10-20)

The owner explicitly approves finishing menu/device checks now and verifying
combat/audio/progression consumers in their ordered stages. Keep #39 open for
those real effects; assess #2's rendering/lifecycle acceptance separately.
The decision is recorded on #4 and #39, not a silent work-order change.

- Cursor Memory requires no speculative production patch: the actual shared
  root/row consumers already implement the original semantics.
  `CursorMemoryTests.cpp` now covers root reset/remember with all entries and
  the real 0x3F8 enabled mask, repeat visits, shared row/scroll restoration,
  close/re-entry and a remembered position invalidated by fewer rows.
- Both Debug and Release complete 24,000 scripted ticks with 17 retained
  screenshots/read-only PDB observations each. On -> Off -> On -> cancelled
  Off -> Off applies 1/0/1/1/0, respectively, without changing progression.
  Field returns have menu state 63 and unbound rows; subsequent Triangle
  presses really re-open the root menu.
- Off resets the root to the first enabled entry (internal ID 3, Items),
  and Options to row 0. On remembers Options (root ID 7) and Cursor Memory
  (Options row 4). Cancel preserves the applied value but remembers the
  highlighted choice: Off is still highlighted on the next choice entry.
  With memory Off the choice instead starts at row 0, On, even while Off is
  the applied value. Do not confuse the applied marker with the cursor.
- Initial observer attempts were corrected for Windows' not-yet-populated
  loader module list, visible versus internal root IDs, and that choice
  highlight rule. These are retained diagnostic failures, not gameplay
  regressions or passing runs. No gameplay patch was made to fit an oracle.
- Focused Cursor Memory coverage passes three cases / 52 assertions; related
  `[menu]` coverage passes 15 cases / 188 assertions in both configurations.
  Changed-source quality with tidy has zero new findings; existing legacy
  advisories are not baselined away.
- Local arguments, completed logs, screenshots and observed settings/consumer
  states are under `build/quality/issue39/cursor-debug` and `cursor-release`.
  This proves session behavior, not save persistence.
- The earlier device finding was that owned `Legacy/Input/LibPad.cpp::PadSetAct`
  discarded the registered mutable actuator table, and `InputSystem` had no SDL
  rumble output. The original registers once and then updates those bytes.
  The implementation/verification above supersedes that recorded gap, not the
  deferred combat/audio/progression acceptance.

### Late-attract reproduction blocker (#26, 2026-10-20)

The original cyclic chains and malformed packet remain recorded evidence, but
the corrupt producer is not established. #26 remains open, not claimed fixed.

- Current `70cd21e0` Debug and Release each complete 36,000 attract ticks, with
  only `1000:Start`, all ten requested captures and no packet errors.
- Reversing only the two F02456 feedback-link fixes makes the focused regression
  fail, but still does not reproduce the long-attract failure. Normal sources,
  manifest and default binaries were restored before that experimental replay.
- Unchanged source archives of `2d64d275` and tracing checkpoint `35618ef2` each
  build privately and complete the same 36,000-tick interval with all seven
  requested captures. Existing installed dependencies and unchanged manifests
  were used; no main-tree source or reference changes were needed.
- The first historical run was manually closed by the owner at tick 14,464.
  Its zero exit is an interrupted attempt, not a passing replay. The subsequent
  complete runs require captures and the explicit tick-limit stop.
- Evidence is under `build/quality/issue26/`: `current-attract-debug`,
  `current-attract-release`, `without-feedback-link-fix`, `historical-2d64d275`,
  and `historical-35618ef2`. Original failure logs remain untouched.
- These results establish neither a tracing artifact nor F02456 as #26's cause.
  Next useful observation is the first invalid link/write in a repeatable
  failing binary/layout, not another guard that skips a bad command.

### Viewport boundaries and New Game mansion (#41, 2026-10-20)

The owner identifies the reported scene as the mansion immediately after New
Game and explicitly confirms that the artifact no longer occurs in the latest
build. This is separate from the Bardorba bedroom texture report.

- Current `70cd21e0` Debug and Release each complete 36,000 ticks of the existing
  Off/Frame/Simple selection, exit, field recovery and re-entry route.
  Eleven actual client/full-window pairs per configuration cover physical
  client sizes 320x240, 640x480, 853x637 and 960x720.
- The exact mansion route uses only `1000:Start,1300:Circle`, without either
  prologue skip. MAP001 and ENKEI.GIM load at 1398/1462. Both configurations
  complete 3,600 ticks and four more client/full-window resize captures.
  Native mansion frames at 1808, 2304, 2816 and 3328 are byte-identical between
  Debug and Release.
- Portable DuckStation's delayed 65-second capture confirms the same mansion
  view and caption. The initial reference attempt ended during loading/black
  transition frames and is excluded from scene-parity evidence.
- Native, 2x and 3x client images match a captured presented frame exactly.
  Fractional-scale differences are confined to source-coordinate integer
  sampling ties at the centre row/column and equal the adjacent nearest texel;
  no unexplained pixels or nonblack pixels outside the destination remain.
  The analysis retains these differences rather than hiding them in an error
  tolerance. Intentional minimap Frame borders and black letterboxing remain.
- Every matched presented image reconstructs exactly from a raw 320x224
  rectangle at page X=0 or 320, Y=0, with eight black output rows above/below.
  These are horizontal framebuffers, as initialized by `Render/Display.c`;
  assuming vertical buffers was an initial diagnostic-oracle error.
- Historical `2d64d275` mansion captures are retained for comparison, but the
  old owner artifact was not independently recovered. Do not claim that the
  known #34 top-left fix is proven to be this report's producer merely because
  it is included in the verified current revision.
- `ViewportBoundsTests.cpp` guards the exact output dimensions and all pixels
  with poisoned surrounding page storage, both framebuffer origins, drawing
  offsets/clipping and mask on/off. The focused case passes 307,220 assertions;
  related `[render][gpu]` coverage passes six cases / 314,723 assertions in
  both configurations. Existing raster/atlas regressions remain unchanged.
- Local arguments, raw pages, source frames, client/full-window PNGs and pixel
  results are under `build/quality/issue41/`. No disc-derived evidence is
  uploaded. The verification/regression commit and its CI are recorded in #41.

### Bardorba bedroom window atlas seams (#34)

The owner identified the affected view as Bardorba lying in bed speaking to
Rosencrantz, **not** the earlier Guildenstern window. Reproduce with
`1000:Start,1300:Circle,3000:Start`, then let the quote finish without a second
Start. MAP008 contains the quote; the actual bedroom is **ZONE018 / MAP062**
(loads at 4561/5075), followed by `ENKEI2.GIM` at 5130. The first bedside
caption/window is stable at 5500.

The source image was not corrupt. All three uploaded texture sheets
(255/255/135 rows) and the entire 256-color palette match the original asset
byte-for-byte. Matching-PDB reads confirm the actual layer uses 8bpp, page 26,
a 5x16 grid, offset (20,-20), scale 2688 and draw routine 0. Targeted temporary
packet/pixel diagnostics also ruled out texture-window state and subsequent
room-geometry overwrites; all instrumentation was removed before final tests.

The first invalid producer is the owned `PageSpace::Triangle` coverage bias.
For screen coordinates with Y increasing downward, its purported top-left
rule instead included bottom/right edges and excluded top/left. The scaled
64x15 GIM quads from F01393 (`5BF94.c:4056`) therefore sampled lower/right UV
endpoints from unrelated neighboring atlas tiles. This produced the brick-like
stripes across the distant skyline. Reversing the two edge comparisons restores
the original half-open polygon coverage; no GIM/MPD/disc data, palette, loader,
event delay or input wait was changed.

Three new synthetic 8bpp atlas/coverage tests first fail **1,076 of 7,412
assertions**, then pass all of them after the fix. They cover both windings,
inclusive draw-area clipping, poisoned atlas borders, horizontally/vertically
adjacent tiles in reverse draw order, and shared translucent diagonals shaded
exactly once. Full Debug and Release suites pass **131 cases / 53,206
assertions** each. Changed-source tidy quality has zero new findings, with no
baseline refresh; generated test-project registration is stable on regeneration.

Both builds complete **24,000 ticks / 21 captures** with Circle every 600 ticks
from 6000 through 18600. The entire bedroom conversation ends normally, then
the original script loads `JYO.GIM` at 18622, ZONE033/MAP172 at 19686/19692 and
ZONE010/MAP211 at 22403/22411. The subsequent arrival dialogue remains waiting
for input; this is **not** a full interactive prologue/combat verification or
the later MAP009 first room, which remains separate under #6.

The portable original comparison was driven from observed title/opening/quote
states, avoiding unreliable wall-clock-only scene assumptions. It confirms the
continuous skyline in the exact bedside shot and the subsequent conversation
camera. The aligned window crop's mean absolute RGB difference drops from
**6.20 to 2.27**. Release and **normal-speed Release** reproduce the corrected
Debug window pixels exactly. The normal-speed route completes **7,200 ticks /
four captures**. A separate Release **15,400-tick / four-capture** Map, Data,
Options and field-return smoke replay passes. No accepted runtime logs an
error or crash.

Exact argv, before/fixed/original captures and diagnostics stay local under
`build\quality\issue34`, notably `bedroom-before-debug`, `bedroom-final-debug`,
`bedroom-final-release`, `bedroom-normal-release`, `menu-regression-release`
and `reference-live`. The fixing revision and successful pushed CI are recorded
on #34 before closure. #41's separately reported client-border artifacts and
the remaining #2/#39 acceptance are not claimed resolved by this repair.

### Data entry and safe no-card lifecycle (#38)

The exact first-room Data entry at pushed `9af8a50e` crashed at tick **7713**,
before any card or file confirmation. F02777, `Menu_Files_DrawPlayTime`
(`MENU7.PRG/260.c:4157`), overwrote digits through a static pointer to a
read-only string literal. The copier now emits an owned mutable array before
local-static hoisting. The generated Menu7 reset therefore restores the whole
12-byte buffer, not a pointer to read-only storage. The original clock, timing
and 100-hour overflow display are unchanged.

`DataMenuTests.cpp` adds **80 assertions / two cases**: emitted clock glyphs
at 0/12/99/100 hours, reset/reentry, unchanged global play time and packet
guards; five empty summaries on each card port remain unavailable, Circle
never starts a load while paging through them, and Cross returns cancellation.
The input fixture binds an empty SFX bank, as required by the existing feedback
API, and restores its pointers and all borrowed public UI/input state. No
card-readiness result or successful save/load is fabricated.

Both Debug and Release complete **27,800-tick / 18-capture** recorded routes:
Data entry, Load, both real no-card errors, dismissal, slot/Data/root-menu back,
three entries, Triangle return, disabled-Save rejection and resumed field
movement. Read-only matching-PDB observations at ticks 19500/22800/27400
verify normal control mode, unpaused interaction, menu 63, action 0, no menu
rows and restored soft reset. At all six observations the card UI allocation
is back in the real heap freelist and free space is unchanged at **16,267,104
bytes**, with no accumulating allocation across the repeated exits. The
existing resource-release path retires the card UI resources; legacy card
event calls remain unsupported, not a new native callback/persistence backend.
A separate **normal-speed Release** route completes **11,200 ticks / five
captures**, including the changing clock, both back steps and field movement.
No runtime run logs an error or crash.

Portable original-game `reference-data` captures establish the same disabled
Save/Load root and clock placement (132 seconds), slot 1 and slot 2 no-card
messages (146/167), dismissal and complete cancellation back to the field.
Only the portable emulator was used, with both cards `None`. Native output
matches these semantics after accounting for its eight-pixel presentation
margin. Empty-slot coverage is a synthetic summary/UI regression, **not**
working native card enumeration or actual file I/O.

Full Debug and Release suites pass **128 cases / 45,794 assertions** each.
Changed-source quality with tidy has zero new findings, without a baseline
refresh; copier, manifest and project output are stable on a second generation.
Exact argv, pre-fix stack, replay logs, captures and cleanup observations stay
local under `build\quality\issue38` (`data-entry-before-debug`,
`data-lifecycle-*`, `data-observed-*`, `normal-data-release`, `reference-data`).
The fixing revision and successful pushed CI are recorded on #38 before
closure. Actual persistence, import/format decisions and save/load behavior
remain scoped to #18; no owner save files were used or overwritten.

### Full Map entry and lifecycle (#37, 2026-10-19)

The first-room Map crash was independently reproduced at tick 7747 in
`Menu_Map_DrawControlBackdrop`, not inferred from a minimap warning. F02703
(`MENU5.PRG/4D8.c:1965`) read an eight-byte ordering pointer through original
scratch word 1, now the middle of the native primitive pointer. It now uses
the native frame ordering field. Related Map consumers are corrected through
the copier:

- F02699 (`4D8.c:1788`) no longer truncates framebuffer-strip cursors or reads
  the ordering table through two 32-bit scratch words. Both native framebuffer
  strip chains remain intact.
- F04150/F04151 use `Render_frameScratch.camera.angles.vy` for initial/reset
  heading instead of obsolete scratch word 22.
- F04153 (`4D8.c:1112`) uses a local 256-entry XY/depth/fog workspace for the
  format's byte-indexed vertices instead of the 96-entry shared scratch tail.
  The already-bounded scene decoder remains the input-validation boundary;
  no original room/visited flags or room lists are fabricated.

Three focused tests cover control backdrops at edge/minimal dimensions in
both framebuffers, both native darkening strip/page chains, and a validated
256-vertex room referencing the last vertex. Exact projected coordinates,
complete ordering traversal, packet guards and unchanged scratch beyond the
native frame are checked. Full Debug and Release suites pass **126 cases /
45,714 assertions** each; changed-source quality with tidy has **zero new
findings**, with no baseline refresh.

The identical formerly crashing Map entry now renders the original Wine
Cellar / "Entrance to Darkness" room. Both configurations complete recorded
**22,300-tick** routes with all **17 captures**: pan, L2 rotation, R2 zoom,
Start reset, previous/next visited room, the real visited-area menu and cancel,
UI hide/show, Cross/back and Triangle/direct return, three entries and resumed
field movement. No run reports a render error or crash. A separate
**normal-speed Release** route completes **11,200 ticks / five captures**,
including entry, back, field return and movement.

Portable original-game `reference-map` captures at 132/139 seconds establish
initial/reset room geometry, heading, compass, labels and controls; 143/146
cover real area selection/cancel, and 164 repeats entry after returning to the
field. Native output matches those menu/room semantics, accounting for its
eight-pixel presentation margin. This is not full-renderer or full-playthrough
parity.

Local evidence and exact argv are under `build\quality\issue37`:
`map-entry-after-debug`, `map-lifecycle-debug`, `map-lifecycle-release`,
`normal-map-release` and `reference-map`; the pre-fix stack remains under
`build\quality\issue40\map-entry-before`. The fixing revision and successful
pushed CI are recorded on #37 before closure. Data-menu persistence support
and the other remaining #2/#39 acceptance gates stay separate.

### Equipment details, Status Square and correct comparison values (#31/#33/#35/#36, 2026-10-19)

The blank weapon chooser, item-detail pages and Status Square subview share
F02555 `_renderStatPage` (`MENU/MAINMENU.PRG/C48.c:673`). Its stat-bar link at
ordering entry 15 stored truncated pointer bits, terminating traversal before
lower titles, rows, values and information. DP/PP and the preview survived
because they occupied higher entries. The copier now uses a native link handle.
The same function's local `pragma vsstring` header also remained literal ASCII:
its 29-byte loop and color operands at 1/11/24 require the original encoded
CLASS/AFFINITY/TYPE bytes. Those controls are now encoded, not drawn as glyphs.

The newly visible comparison panel exposed the independent #35 data defect.
Original `D_80102488[4]` and the adjacent eight-byte `D_80102490` region were two
views of delta/index and total halfwords. Native globals are separate, but
F02561/F02562 still accessed `comparisonStatDeltas[4..6]` outside its four
elements, corrupting comparison accessory state. Public-API fixtures reproduced
the exact wrong values: Status 200/200/200 instead of 100/101/100, and Fandango
210/201/196 instead of 110/102/96. G00864 now contains four signed halfwords
in the same eight bytes, and every total write/subtraction/comparison read
explicitly uses that buffer. No actor formula, original equipment definition,
game-state bypass or hardcoded subtraction changed.

Evidence:

- Isolated link/header regressions fail before the fix on all three pages and
  pass afterward, checking complete lower-UI traversal, bounded packet storage
  and 19 visible header glyphs rather than printed controls. Comparison
  regressions fail with the exact original report before the typed-buffer fix;
  both Status and equipment-preview modes now pass with positive, zero and
  negative actor modifiers and accessory contributions applied exactly once.
- Full Debug and Release suites pass **123 cases / 45,677 assertions** each.
  Changed-source quality with tidy reports **zero new findings**, with no
  baseline refresh.
- Both configurations complete three recorded input routes: **22,000 ticks**
  for all equipment stat pages, an actually empty Shield slot, cancellation
  and repeated entry (**14 captures**); **18,000 ticks** for repeated Status
  Square/Affinity/Type/navigation/back/re-entry (**13 captures**); and **21,200
  ticks** with Information Off in the deep weapon and Status views (**13
  captures**). All 40 requested captures per configuration exist, every run
  reaches its tick limit, and no log reports a crash, cyclic chain or render
  error.
- Native now matches the fresh original's Status ORG 100/100/100 and EQP
  100/101/100, and Fandango ORG 10/1/-4 with EQP 110/102/96. Original and native
  show the real Fandango/component rows, matching Class values, encoded page
  headers and contextual information. Empty Shield shows NONE / "Nothing
  equipped.", not invented inventory. With Information Off, the body/header
  disappears but equipment labels, stat values and Status limb rows remain.
- Portable DuckStation reference `reference-chooser-status` captures original
  Equip->Weapon at 138/141 seconds, normal Status at 164/166, and Square at
  171/173. These are real menu inputs in the same fresh-room/default-loadout
  state. Native actor animation/presentation differences are not represented
  as full-scene parity.

Exact argv, before/after logs and local captures remain under
`build\quality\issue33` and `build\quality\issue36`. The final route folders are
`equipment-cycle-final-<config>`, `status-cycle-final-<config>` and
`information-off-deep-final-<config>`. The fixing revision and successful
pushed CI are recorded on #31/#33/#35/#36 before closure. The remaining #39
Options effects, actual equip/unequip implementation #14 and the Map/Data
crashes are not completed by these view/comparison fixes.

### Contextual information and first-visible-frame text (#30/#31, 2026-10-19, earlier checkpoint)

The shared F02524 `vs_mainmenu_setInformationMessage` producer
(`MENU/MAINMENU.PRG/58EC.c:475`) still contained literal ASCII `|!0|` from a
`pragma vsstring` initializer. The original encoder proves this prefix is
**F8 00**: instantaneous text, not printable glyphs. The helper copies 96
payload bytes starting at byte two. Thus the unencoded leading pair survived:
repeatedly rebound Equip/Setup messages spent every frame's initial character
budget on that pair instead of their body, while one-shot empty-category
messages revealed progressively with a stray glyph.

The copier now emits the exact encoded prefix, zero-glyph payload storage,
both original E7 guards and the C terminator. No global text speed, dialogue
wait, help string, progression gate or Information option semantics changed.

Verification:

- A first-visible-frame regression failed before the repair and passed after.
  Isolated `InformationTests.cpp` now covers the exact prefix/payload/tail,
  complete first-frame output despite repeated rebinding, immediate replacement
  with empty text, ordinary dialogue still revealing one glyph, and Information
  Off hiding normal help while preserving required cautions. Existing legacy
  renderer tests were left unchanged rather than reformatting/baselining them.
- Full Debug and Release suites pass **119 cases / 45,629 assertions** each.
  Changed-source quality with tidy reports **zero new findings**, without
  refreshing any baseline.
- Both configurations complete **26,500-tick** controller replays toggling
  Information On->Off->On through Options, returning to Items/Equip/Setup and
  the equipment-category overview, cancelling and re-entering, then returning to the
  field. All **18 requested captures per configuration** exist. The actual
  information panel/body disappears when Off and the original contextual
  messages return when On; this verifies a consumer effect, not merely a bit.
- Debug's category replay and Release's dense-sampled repeat exercise weapons,
  blades, grips, empty Shields, back/forward selection, Equip/Setup, chooser
  details, cancellation and re-entry. Release's first complete new Shields
  sample is tick **10316** (after the category transition); its body remains
  byte-identical in all subsequent samples through 10600. The first rendered
  message is complete, not a gradually revealed string.
- Portable DuckStation comparison `reference-help-b` uses real title/prologue
  skips and menu/category inputs. Captures at 128/133/138 seconds cover original
  Equip/Setup; 158/160 cover empty Shields. Native messages match "Choose items
  to equip.", the original assembly/disassembly/repair description, and "You
  have no items in this category." without a leading glyph. An earlier
  wall-clock reference attempt missed its intended boot state and is retained
  as an unsuccessful comparison, not evidence.

Exact argv/logs and local captures are under `build\quality\issue30`:
`help-after-debug`, `categories-after-debug`, `categories-final-release`,
`information-toggle-debug`, `information-toggle-release`, and
`reference-help-b`. A deeper 14,800-tick Debug item-detail/actions replay and
13,900-tick Release Equip->Weapon replay confirm that action cautions and
cancellation work, but the weapon-detail/chooser view still loses its labels
and information together (#33). Do not represent those blank views as help
completion. #30 can close on its verified control/timing repair; #31's root
Equip/Setup messages are restored, but its deeper-context acceptance remains
linked to #33 until those views render. Fixing revision and successful pushed
CI are recorded on the issues. #39 still has other Options effects outstanding;
this does not complete equipment-changing semantics, the Status Square
subview or overall #2.

### Working minimap scenes (#40, 2026-10-19)

This completes the missing minimap functionality after the shared exit-hang fix
`ef559a2e`. The resolver and renderer are owned implementations, traced to
`BATTLE.PRG/2EA3C.s:2085-2489`, not edits to generated output or reference code.

- Scene room records remain **12-byte file records**. The resolver validates the
  entire section before replacing payload lengths with record-relative offsets;
  the copier applies the existing `VS_ONDISC` pattern to every native consumer.
  MPD and ARM callers supply their actual section/file extents. Native pointers
  are never written into packed scene bytes.
- The renderer projects visited rooms, draws the current room's fills and dim
  edges, other visited-room connections, and the original gated/blinking door
  markers. It validates vertex/primitive indices and reserves bounded packet
  capacity before writing. Ordering links use native handles. Projection-center
  accessors now work, including original high-halfword semantics; the field
  caller restores its rotation, translation and projection center afterward.
- Synthetic tests cover truncated/malformed scenes without mutation, packed
  strides/offsets, projected coordinates, colors, command order, both frame
  buffers, unvisited rooms, entry blinking, locked-door gates, capacity guards
  and projection restoration/wrapping. Disc-configured tests validate **all 31
  ARM scenes and all 367 MPD minimap sections**; `SCEN031.ARM` intentionally has
  no name tail. All MPD minimaps draw within packet guards using test-owned
  copies. Full Debug and Release suites pass **115 cases / 45,580 assertions**
  each.
- Fresh Debug and Release replays each complete **54,000 ticks**, all **36
  captures**, the full nine-transition Frame/Simple/Off matrix, Cross and
  Triangle cancellation, movement and menu re-entry. Read-only PDB-based samples
  confirm every applied setting and actual resumed field state: mode 1,
  unpaused, current menu 63, selection status 0 and released rows. A fresh
  **speed-1 Release** Frame->Off->Simple replay completes **18,000 ticks**, all
  nine captures and the same applied-setting/resume checks.
- Original-game comparison uses only the portable DuckStation copy and the
  real two-Start menu-test route. Local `reference-minimap-a` captures at 95 and
  124 seconds establish Frame's border and Simple's retained room geometry
  without that border, after real option confirmation and Cross/back exit.
  Native/reference projected room outline, fill and marker placement agree
  after accounting for the native capture's eight-pixel presentation margin.
  This is minimap verification, not whole-scene rendering or combat parity.
- Evidence, exact replay argv and state observations remain local under
  `build\quality\issue40\minimap-final-debug`, `minimap-final-release`,
  `minimap-final-normal-release`, `reference-minimap-a` and the minimap test logs.
  No captures, disc data or memory dumps are committed/uploaded.
- Native-view checks cover **620 fields with zero mismatches**; all **179 event
  handler signatures/dispatch checks** pass. All **24 quality positive
  controls** pass; changed-source quality with tidy reports **zero new
  findings**, with no baseline refresh. Copier regeneration is stable, manifest
  SHA-256 `3C1BAEC71675262FADF0532D30519FBC6076E0D179E295B9FB8C21B593EBC0A5`.

The fixing revision and successful pushed CI are recorded on #40 before closure.
#39 still owns the remaining eight Options' real effects; #2 remains the menu
acceptance gate. The separate full Map issue #37 still reproduces a crash in
`Menu_Map_DrawControlBackdrop` at tick 7747; fixing packed scene records alone
does not complete that screen.

### Repeated Options exit freeze (#39/#40, 2026-10-19, earlier checkpoint)

Verification used `2cdc8100` plus the scoped F01342 copier change. No reference,
disc data, progression flags, quality baseline or hardware-shaped renderer was
changed.

The first Frame/Simple/Off visit could return normally, but a second Options
visit froze field control even with Frame unchanged. Read-only samples through
the matching local PDB showed `Field_controlMode=5`, interaction paused,
`currentMenu=31` (closing), `menuStatus=1` and allocated menu rows while logical
ticks continued. Thus a field screenshot or clean bounded process exit was not
proof of recovery.

The original MAINMENU `C48.c:_quitDelay` completes only on its fifth call. Native
execution retained its counter across visits: the copier generated the intended
MAINMENU reset, but no runtime caller invoked it. F01342
`vs_battle_loadMenuPrg` (`5BF94.c:2576`, native
`Menu_Framework_LoadMenuOverlay`) now calls the existing
`Boot_ResetMainMenuStatics` only after a MAINMENU load completes. Already-loaded
polls do not reset the delay. This restores the original reload boundary rather
than forcing Map Off, skipping cleanup or weakening the fifth-call condition.

Evidence:

- The public-API regression "Menu reloads restore the original five-tick closing
  delay" fails before the fix and passes afterward. It exercises three completed
  load/close cycles, including already-loaded polls, and requires completion on
  exactly the fifth closing call.
- Debug and Release each pass all **106 cases / 30,816 assertions**. The focused
  `[render],[content][module]` selection passes **30 cases / 260 assertions** in
  both configurations.
- Both configurations complete **54,000-tick**, speed-16 input replays covering
  Frame->Frame, Frame->Simple, Simple->Simple, Simple->Off, Off->Off, Off->Simple,
  Simple->Frame, Frame->Off and Off->Frame, then Cross/back cancellation and
  Triangle/direct cancellation of an unconfirmed change. All **36 captures per
  configuration** are present. Direct state reads confirm applied values 3/1/0,
  unmodified settings on cancellation, root/selector re-entry and actual field
  recovery (`mode=1`, not paused, `currentMenu=63`, `menuStatus=0`, no retained
  menu rows). Final movement and another menu open/close remain responsive.
- A separate **normal-speed Release** Frame->Off->Simple replay completes
  **18,000 ticks**, all nine requested captures, applied-mode/state checks,
  movement and menu re-entry. The owner-confirmed manual close of the earlier
  normal-speed attempt remains an incomplete attempt, not a reproduced crash.
- Local exact argv, observations and logs are under
  `build\quality\issue40\matrix-fixed-quiet-debug`,
  `build\quality\issue40\matrix-fixed-release` and
  `build\quality\issue40\normal-off-simple-fixed-release`. An earlier Debug
  trace-heavy matrix timed out at tick 37173 after 240 seconds; the completed
  reduced-log Debug repeat took 352.9 seconds within its 480-second bound.
- Changed-source quality with `--tidy` reports **0 new findings**, with no baseline
  refresh. Second copier generation is stable: manifest SHA-256
  `A5B925DAF78C5BE11D48602FD1E152670F4B02BEBC9C2E06A06AEF2C7C33AC10`.

This earlier checkpoint repaired only the shared repeated-exit hang. Its
then-unported minimap resolver/drawer/projection accessors are completed in the
newer checkpoint above. The other eight Options' real effects remain #39 work,
and #2 still blocks #6. Do not infer complete Options or menu parity from this
earlier exit-only evidence.

### Restored Submenu Text And Names (2026-10-19)

Verification used pushed `f991796c` plus the final shared panel/name fixes. #28 is
closed after successful pushed Source Quality run `39275882740`.

- The remaining missing submenu labels were a shared F02536 prompt-background link
  at ordering entry 15, not a hidden-row flag or stale frame buffer. Runtime traversal
  reached entries 15 and above but skipped 0-14 and executed zero variable-width glyphs.
  Native handles in F02536 and adjacent action-cost F02535 restore all low entries and
  72 glyph packets in the same Status frame. Normal root, inventory and Status labels,
  counts, information panels and preview now visibly render.
- #29: G00620's default names retained unprocessed `#pragma vsstring` ASCII/markup.
  Original read-only `encode_raw` confirms the encoded actor name, spacing opcode and
  terminator, and encoded weapon name. Scoped copier literals preserve those exact
  semantics; the before-fix name regression fails and passes after repair. Status now
  visibly displays "Ashley Riot" rather than accented glyphs/zeros.
- Public-API panel regressions traverse the complete lower chain and guard the packet
  storage. Default-name regression checks original bytes. Full Debug: 105 cases /
  30815 assertions; Release `[render],[content][module]`: 29 cases / 259 assertions.
  Clean Debug menu selection: five cases / seven assertions. Final quality with tidy:
  zero new findings, no baseline refresh. Temporary row/GPU/companion probes removed.
- Clean Release Status entry/navigation/exit: same input schedule as the #28 section;
  10500 ticks, exit 0. Evidence `build/quality/issue2-final-status-release` (8800 shows
  readable names/limb status/stats). Inventory/preview/return uses
  `1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle,7600:Circle,8200:Right,
  8500:Circle,9200:Cross,9600:Cross,10000:Triangle`, 11500 ticks, exit 0. Evidence
  `build/quality/issue2-final-items-release` (9000 inventory, 11000 gameplay).
  Debug equivalents and regression logs use `issue2-inventory-prompt-final`,
  `issue2-status-prompt-after`, `issue29-status-names-after`, `issue29-names-before`.
  Logs are under `build/Debug/`; captures stay local.
- #2 covers the reported missing menu text/black bars and preview corruption. Item use,
  spells, chains, Break Arts, drops and equipment operations remain ordered #8-#15;
  readable menus do not establish those gameplay features. #6 needs the full prologue,
  not the efficient menu-only skip. Fixing commit and pushed CI/closure evidence for
  #2/#29 are recorded on those issues.
- #26 remains open: a clean Debug 35500-tick attract attempt hit the 120-second runner
  limit without target captures. Last content log was 14464, not proof of a tick stall.
  No long-attract success or original-34609 corruption repair is claimed. Next useful
  observation is intermediate capture/tick progress at 15000/20000/25000.

### Quality verification (2026-10-19)

Verification used `work` at `3c0a1311` plus the pending retained-code cleanup. No baseline
refresh was used, and no commits were pushed during this work.

- Full `tools/lint/quality.py --tidy`: **zero actionable native-production findings** and
  zero new findings. Remaining counts are 332 test-harness, 154 tooling and 13 legacy
  SDK-name/signature/null-spelling diagnostics. Test/tooling follow-up is #25; compatibility
  retirement remains #18. Exact fixture literals and replacement-only style are reported
  separately, not described as fixed code. See [QUALITY.md](QUALITY.md) for the explicit policy.
- Complete Debug and Release solution builds pass. Each full `VS.Tests.exe --reporter compact`
  run passes **95 test cases / 30707 assertions**. All **23 quality-tool controls** pass.
- `check_native_views.py`, `layout_check.py` and `check_event_dispatch.py` pass. Running
  `copy_to_native.py` again leaves the copier manifest SHA-256 unchanged.
- Release replay: `--frames 14000 --speed 16 --press 1000:Start --capture
  build/quality/final-release-replay --at 1500,6000,13900 --scripted --log-level info` completes
  and produces all captures. A corresponding Debug frame at tick 13900 was visually inspected:
  actor geometry and dialogue are visible. This is regression evidence, not complete gameplay parity.
- Evidence is under `build/Debug/issue5-final-*` and `build/quality/`. Release logs also live
  under `build/Debug/` because the bounded runner uses that common log directory.

### Verified defects and remaining limits

- #20: SEQ decoder int-alias/alignment defect, fixed in `7da0da5b`; halfword-aligned vectors,
  packed signed deltas and rotation/cursor rules are tested.
- #21: unrepresentable disc-read byte counts, fixed in `c8516e00`; rejection preserves request state.
- #22: signed squared-length overflow in normalization, reproduced before fixing in `5ab86b63`;
  full-range positive/negative vectors, zero input, aliasing and padding are tested.
- #23: manifest sector-range overflow, fixed in `4dfd783b`; synthetic boundary entries avoid
  large-file allocations and share the same arithmetic as real lookup/read paths.
- #24: GPU upload byte-count overflow and partial resize ownership, fixed in `3c0a1311`;
  CPU boundary tests and real presentation pass. Allocation failure handling was reviewed,
  not fault-injected.
- #3: pushed Source Quality failure at `54290b05` is fixed locally by `c41dd5b2`; it remains
  recorded historically. Fresh inspection confirms successful run `39239821866` for
  pushed `35618ef2`. No commits were pushed during the window-dialogue fix. Commit
  permission does not imply push permission.
- #19: the two owned halfword-copy helpers are implemented and verified as recorded
  above. This resolves a dependency of #2, not the remaining missing menu graphics.
- Enemy combat completion, sound, item/spell use, chains, Break Arts, drops, equipment and
  final arena coverage remain open in #6-#16. Follow #4's order rather than treating this
  quality checkpoint as gameplay completion.

## Previous prologue checkpoint (superseded)

Prologue combat entry and remaining intro regressions are the active priority. The older
tick-5751 stopping point below is historical, not the current runtime baseline.

- Debug and Release desktop/test targets build. Both full suites pass **66 tests / 30505 assertions**.
- Required module initialization reports **334 data entries, 19 pointer tables, 2991 symbols,
  50 modules, zero failures**. Copies and relocations validate source extents, destination capacities,
  and reviewed native element strides; unresolved required data prevents startup.
- Native views: **590 fields, zero mismatches**. Layout check: **514 types, zero pointer-free
  mismatches, 78 pointer-bearing differences, zero parse errors**. Event dispatch: **179 signatures,
  zero errors**, including both 256-entry tables. A second copier run leaves all generated content
  hashes unchanged.
- Shared room/sky and normal actor geometry now render. Owned scalar/matrix math, native model
  workspace, bone transforms/projection, encoded text, and native ordering links have focused tests.
  Packet decoding rejects truncated commands and cyclic lists instead of reading beyond packets.
- Facial ETM loading/release now uses `Actors_sharedSequenceBankTexturePages[slot - 1]`. Original
  `D_800F244F[slot]` and `D_800F2450[slot - 1]` name the same byte; independent native arrays broke
  the loader-to-animation hand-off. Public slots are checked before the one-based conversion.
  Tests cover 4-bit/8-bit frame rectangles, uploaded texels, a nonzero blink frame, frame-zero
  restoration on bank switching/release, and page retirement. Both hand-off and bank-switch tests
  failed before their respective corrections.
- Shared-bank selection now reads named actor fields, not PS1 offsets. The three row bytes read
  by `func_8009CE9C` at actor+0x183E are represented by a union: the third byte retains its original
  overlap with `momentumDistance` at +0x1840. The actor extent and subsequent field offsets are
  unchanged; regression coverage switches to both alternate banks and checks the overlap.
- The second-demo sword-draw stall was an audio-volume wait. Counter 2's callback was discarded by
  the removed `OpenEvent` implementation. It now runs at 240 Hz from logical frames; fades advance
  every fourth callback. The newly reached mixer path also handles an absent saved music layer.
- Room AI allocation and its native-sized flag/height tables are initialized before enemy metadata.
  The two-layer collision sampler now decodes real 4-by-4 patterns and surface flags. The later
  `ActorStatusConditionScore` fault came from the stubbed condition snapshot: the owned producer now
  fills actor IDs, HP, MP, statuses, and equipment DP before scoring. A poisoned-table fixture checks it.
- The first basic attack reproduced a null-menu access in `Battle_CommitSelectedAction` at tick 12621.
  G01904 (`Battle_actionDefinitions`) was excluded from startup loading by incorrect pointer metadata
  and the anonymous hit-record spelling. Its reviewed 52-byte records now load from SLUS offset
  `0x3C1DC`; basic attack 1 has category 14 and body-part targeting, not zero-filled environment targeting.
  Native layout assertions and the real-disc module test check its decoded fields and targeting policy.
- G00229 (`Effects_emptyPackage`, the existing label for the embedded default package) had only its
  four-byte header allocated. The parser walked into unrelated native memory; a relink exposed a
  watchdog stall at tick 1372. It now owns all `0x5200` bytes from BATTLE offset `0x83CBC`: nine tagged
  blocks and the zero terminator at `+0x51FC`. The module regression binds the real package and checks
  texture, animation, timeline, curve, particle, program, parameter, transform, and sound-table data.
- GIM rendering and event views now share the two-byte flag header and native pointer stride.
  `ENKEI.GIM` fills the manor splash rather than displaying a small image over the room. Captures
  at ticks 1600/1800 in `build/new-game-actors` match the supplied full-illustration composition.
- On-disc room-effect command headers remain four bytes, fixing shifted motion/rotation operands.
  Gimmick views expose their native model pointer and use the canonical room scale. Room triangle/quad
  subdivision, adaptive special-model quads, GPU mask packets, and room-light color sampling now have
  focused tests. Actor drawing preserves the update-owned flag and applies signed tint adjustments.
- Debug and Release New Game replays reach **19000 ticks** with dialogue, basic-attack, and directional
  button inputs without the reproduced crash. The final Debug log is `build/Debug/prologue-debug-replay.log`;
  captures and the preserved user playthrough log are in `build/new-game-actors/prologue-input`.
  Release also completes **43000 ticks** across both demos and returns to title. Its event trace is
  `build/Debug/window-event-replay.log`, with captures in `build/facial-textures/window-pause`.
  These accelerated runs are not proof of complete combat or normal-speed timing parity.
- The null-actor movement dump in `Pasted text.md` matched a detached room-spawn alias. The shared
  caller now reads `Field_zoneContext.mpdEnemyCount`; both current binaries contain that correction.

## Reference update review

- Reviewed `resources/rood-reverse` at `2a589e01`, including the latest pull after `c37e3eb2`.
  That batch adds shared trail/ring implementations, effect relinking, ending refactors, and image tools.
  Its battle-path changes rename screen dimensions without changing behavior.
- Actor dispatch already has the native label `Actors_ExecuteQueuedActorCommands` (F00161), and the
  pinned snapshot already includes C implementations of it and shared trail routines. Do not duplicate
  these implementations or replace established native types with another snapshot's layouts.
- `RunActorAiUpdateStages`, `SetActorVerticalMovementSteps`, and `SetDirectMovement` remain assembly-backed
  in the updated upstream on the inspected paths. The local `rood-reverse-noprompt` snapshot also retains
  assembly for those entry points. No reference snapshot was changed or bulk-imported during this review.

## Remaining limits

- The user confirmed that lighting fixes resolved the doubled faces and restored title cards resolved
  the apparent early staging/repeated entrance. Door motion and the full wyvern sequence remain unverified.
  The companion rendering pass is enabled. Material overrides, full lighting/shadows, and broad
  camera/pose parity remain incomplete. The model workspace is sequential and not reentrant.
- The reported extended Guildenstern window pause was not reproduced as a hard stall in the accelerated
  demo trace; the window scene advances and the demos finish. Normal-speed duration and the recording's
  exact interval remain unverified. Do not shorten original script delays or claim this pause fixed.
- `func_800B3A68` does not read its third argument in the original assembly either. The unused native
  `nearOffset` parameter alone is not evidence of a missing clipping operation. Texture scrolling's
  incoming-register comparison also exists in the original assembly; its intended semantics remain
  unresolved and were not guessed during the facial fix.
- The title/battle boundary uses C `setjmp`/`longjmp` after existing cleanup. It must not bypass live
  C++ destructors. Earlier runs observed title re-entry, but this is not complete lifecycle ownership.
- The two human encounters and D'tok are interactive prologue gameplay, not skippable demo segments.
  Validation must drive actual controller actions and check combat/scene state. No enemy defeat,
  HP edit, event-flag bypass, or combat completion was used or claimed in these replays. Higher-level
  navigation, planning, and combat routines still include stubs. The live handoff reaches the unported
  `RunActorAiUpdateStages`, `SetDirectMovement`, and `DrawSphereVolumeWireframe` paths; completing these
  and validating actual target selection/action execution is still required for a playable encounter.
- No full playthrough, collision/combat completion, save support, audio/timing parity, or final
  asset-format conversion is claimed. VS Workshop informs asset layouts but does not execute ETM
  facial overlays or establish runtime visual parity.

## Diagnostics

- `--capture <dir> --at <ticks> --capture-pages` also writes a 1024-by-512 page-space image and raw
  little-endian 16-bit words for each presented capture. `tools/test/decode_pages.ps1` decodes a selected
  4-bit, 8-bit, or direct-color region with its runtime CLUT. Coordinates X/Y are page-space words/rows;
  Width/Height are decoded pixels.
- Optional `texture` and `event-step` debug channels report facial copy selections and event commands.
  Frame presentation may happen on alternating ticks; inspect adjacent render ticks when correlating logs.
- Portable DuckStation captures were collected under `build/ds-render-parity-20261018`; the installed
  emulator and BIOS were not modified. These do not constitute a complete matched-sequence comparison.

## Historical checkpoint

The notes below describe the earlier native-layout/dispatch follow-up based on `218a9b5f`.
Read with `.github/copilot-instructions.md` (rules, machine paths),
[BUILDING.md](BUILDING.md), [PORTING_WATCHLIST.md](PORTING_WATCHLIST.md) §2/§5/§9 and
[NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md) §6.8.1.

## Rules (owner)

- Never edit generated files. Fixes go into `tools\port\copy_to_native.py` or `tools\port\copy_fixups.py`; hand
  ports go through `python tools\port\own.py <id> --to Native\<Project>\src\<Area>\<File>.c`.
- Never commit a BIOS. x64 layouts: **option B** (per-site fixes, 64-bit pointer members kept).
- Actor/sequence/room work stays on the shared path (plan §6.8.1): Content `ActorAssets.c` (SHP/SEQ/WEP loader),
  `Actors\ActorLoading.c`/`ActorState` (actor handler), `Actors\Animation.c` (sequence handler). Every fix is a
  copy-tool rule on those shared functions, so it applies to every actor.

## Done

- 1b title slice: logos, intro movie, title, menu, load screen (CAUTION label fixed), controller input.
- Tools and `C:\Tools` in the solution (Tools folder); DuckStation/BIOS paths logged; test scripts in `tools\test\`.
- Attract path (commit `d9664700`): on-disc offsets via `VS_ONDISC`, `SIG_RETYPE`, `TYPE_FIXES`, actor record and
  allocation fixes, module pointer tables resolve, debug aids (`Core_Tracef`, `Platform::LogCallStack`,
  `Platform_WatchWrites`, null-call stack reporting), `--speed`.
- After it: `Render::QueueSprite`, `QueueSpriteWithTexturePage`, `QueueDefaultSprite`, `LoadFrameViewMatrix`
  hand-ported from inline assembly (owned, `Native\Engine\src\Render\Sprites.c`). This fixed the
  `Battle_DrawBattleHud` crash (a stub returned null).
- Toggleable logging: `--log-level trace|debug|info|warn|error|off`, `--log-channels ai,actors,...`, and logical
  tick stamps. C diagnostics use `Core_Debugf`; existing `Core_Tracef` is trace-level. See BUILDING §6.
- `Core_Memory_ClearAlignedWords` (F01758 / BATTLE `func_800E4288`, `7B5DC.s:344-367`) is now an owned port in
  `Engine\src\Core\Memory\WordCopy.c`, with assembly provenance and bounds/full-size tests. Unlike SLUS's
  `ZeroAlignedWords`, it does not truncate the size to 16 bits. Nonpositive sizes are a documented native no-op.
- Shared actor/AI views: `tools\port\native_views.py` exposes native pointer slots hidden in PS1 padding,
  including the canonical AI state's position/tile sources and decision callback. Corrected the linked-actor
  byte's MSVC bitfield storage, model-pointer view, and two misleadingly named view families. File layouts are
  unchanged. `check_native_views.py` checks 532 selected non-padding field offsets: **0 mismatches**.
- AI creation (F01818): allocates/clears the full `Battle_Ai_ActorAiState` (1264 native bytes, not the partial
  creation view), uses `RoomAiState.specialActors`, and calls the real tagged-bone signature with `SVECTOR`.
  G00579 has **16 slots** in both definition and header: the next BSS symbol is `tileFlagMap` at 0x800F58B8,
  0x40 bytes after the table at 0x800F5878 (`7E05C.s:541-575`). `0x10` is a linked-actor sentinel, not a slot.
  The copier now applies reviewed fixups to globals synthesized from extern declarations too.
- Effect timelines (F01557/F01589): allocate native-sized tracks and index `timeline->tracks[i]`, instead of
  assuming 16-byte PS1 records. Native stride is 24 bytes; the old sound-track crash at tick 5483 is cleared.
- Event dispatch: all 179 distinct normal/skip handlers (182 catalog rows), tables and interpreter result use `intptr_t`. This includes
  saved-address returns and control-code-only handlers; no incompatible function-pointer casts were added.
  The skip table allocation/copy uses its complete native size (256 pointers), not 0x400 bytes. Actual branches
  at tick 5485 now retain their full native addresses. Debug channels `effects` and `event` log these boundaries.
- Layout/dispatch baseline: Debug and Release each passed **19 tests / 30141 assertions**. Tests cover guarded full AI
  storage and cross-view pointers, two-track timeline indexing, normal/skip branch dispatch, saved returns,
  conditional/backward jumps and thread starts. Release build log: `build\ai-release-build.log`.
- `check_event_dispatch.py` verifies the complete normal/skip handler set and compatible argument/return types
  against the extracted BATTLE.PRG and generated skip initializer: 179 distinct signatures, zero errors.
- Final 1800-tick scripted title/menu run with Down at 1200 and Circle at 1300 exits 0 and produced captures at
  1500/1700 (`build\ai-final-title`); log: `build\ai-final-title.log` (git-ignored).
- CLI smoke checks reject invalid/missing logging values with exit 1; `off` leaves an empty log and still
  captures tick 30; `trace`/`frame` emits begin/end records through tick 30 without info-channel leakage.
- GIM tiles (F01393 / BATTLE `func_800CBBCC`, `5BF94.c:4056`): copy-tool fixes replace the computed PS1 scratch
  address with two native 256-entry coordinate arrays (byte dimensions plus the final boundary), reject grids
  exceeding the 250-entry tile payload, and use `LegacyGpu_Handle` for scaled-quad ordering links. Fixed-point
  coordinate formulas and the shared tile/fade API are unchanged. No generated source was patched by hand.
- GIM follow-up: Debug and Release builds and **25 tests / 30178 assertions** pass. Six new tests exercise
  coordinate rounding, clipping, dimension/tile-table bounds, shared-scratch isolation, packet-buffer guards,
  native links, framebuffer fades and wipe/crossfade fast paths. Regeneration is stable; native-view, layout and
  event-dispatch checks pass. Release build log: `build\gim-release-build.log`.
- Fresh 1800-tick title/menu regression exits 0 with captures at 1500/1700 in
  `build\gim-title-20261018-011630`; log: `build\gim-title-20261018-011630.log`.

## Historical stopping point (superseded)

Run: `tools\test\run_native.ps1 -NoCopy -SkipIntro -Speed 8 -Frames 8000 -At '1500,5400,5500,7900' -LogLevel debug -LogChannels 'ai,actors,effects,event,boot,input,content,render'`
(the demo starts about tick 5300).
Latest log: `build\gim-attract-20261018-011630.log`; captures at 1500/5400/5500 in
`build\gim-attract-20261018-011630`. The former tick-5487 GIM fault is cleared, with no GIM grid-rejection
diagnostics in this run. The tick-7900 capture is still not reached.
Ashley initialization returns at tick 5363; scripted actors 2/3/4 report matching native/view IDs at
5371/5381/5391. They now take the intended no-AI path according to their actor flags. Loads ZONE020.ZND,
MAP213.MPD, HEADQUAT.GIM and E229.P; playback now advances to **tick 5751**:

```text
access violation reading 0xFFFFFFFFFFFFFFFF
#0 Actors_FindActorAnimationHeader Game\src\Actors\Animation.c:1247
#1 Actors_PlayActorAnimation       Game\src\Actors\Animation.c:759
#2 Event_ScriptPlayActorAnimation  Game\src\Event\ScriptActors.c:116
#3 Event_ExecuteScriptThread       Game\src\Event\ScriptVm.c:964
```

**Next investigation:** the shared actor/SEQ animation-header lookup. The failing read is
`VS_ONDISC(p, animationMap, u_char *)[idx]`; trace the selected sequence pointer, on-disc offset and index before
choosing a fix. This later animation failure was not repaired as part of the GIM work.

The runner correctly rejects this run with exit `-1073741819` (0xC0000005); the requested tick-7900 capture is
not reached. Wrappers that catch this expected exception may exit 0: that is **not** gameplay success.
The earlier actor-ID, effect-stride, event-return and GIM scratch failures are cleared; **the attract run still fails**.

Validation limits: the 532-field checker audits selected offsets, not all widths/strides or every runtime view.
The model-family slot list covers only the reviewed early model/sequence region, not the entire ActorRecord.
`layout_check.py` reports 519 checked types, zero pointer-free mismatches, 80 expected pointer-bearing layout
differences and zero parse errors. No full AI simulation, visual-fidelity comparison or real-time A/V validation
is claimed. AI room/relations/locomotion prerequisites remain stubs; downstream planning workspace offsets
remain unreviewed. The initialization log's height 65397 is the creation view's unsigned representation of -139,
not proof of correct collision behavior. No further assembly functions were ported in this layout slice.

## Stubs reached at the historical checkpoint

From the log (`[warn] [tick=N] unported:`). Assembly lives in `resources\rood-reverseWin\src\...` (`.s` files) or the
`INCLUDE_ASM` splat output under `resources\rood-reverseWin\build\splat-artifacts\`.

| Stub | Catalog | Assembly | Size |
| --- | --- | --- | --- |
| `Core::ElapsedFrameCounterTicks` | SLUS | root counter (hardware): modernize to a frame timer; returns 0 now = always within AI budget | tiny |
| `Core::Math::MultiplyMatrixIntoRight` | F03415 | `SLUS_010.40\32154.s` | small (GTE) |
| `Field::SampleCollisionHeightCode` | F00772 | `BATTLE\BATTLE.PRG\2EA3C.s` | small |
| `Render::SetRoomPolygonNearClip` | F00753 | `2EA3C.s` | small |
| `Render::BuildRoomGeometryViewMatrix` | F00759 | `2EA3C.s` | medium |
| `Render::DrawSkyDome` | F00757 | `2EA3C.s` | medium |
| `Battle::Ai::InitializeRoomAiMap` | F01838 | `BATTLE\BATTLE.PRG\7E05C.s` | medium |
| `Battle::Ai::InitializeActorAiExtensionHook` | F01835 | `7E05C.s` | small |
| `Battle::Ai::UpdateActorNavigationOverrides` | — | `Battle\Ai\Navigation.c:856` stub | ? |
| `Actors::DrawActorModel` | F01023 | `func_800B28A8.s` (INCLUDE_ASM) | ~1150 lines |

Room/actor geometry was not implemented at that checkpoint. The current shared normal-material path and its
remaining limitations are described above. Raw offsets still in code reached later are listed in
PORTING_WATCHLIST §2 (Option B).

## Reference

- DuckStation reference of the attract scene: `tools\test\duckstation_capture.ps1` (demo at about 100 s, VKP
  Headquarters / Valnain); earlier captures in `build\ds_attract`.
- Regression before each commit:
  - Debug and Release builds:
    `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' VagrantStory.Native.sln /m /nologo /v:minimal /p:Configuration=<Debug|Release> /p:Platform=x64`
  - Tests: `test-native.cmd Debug`
  - Title, menu and load screen: `tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1600 -At 1590 -Press '1200:Down,1300:Circle'`
- After new files: `python tools\scaffold\gen_solution.py`. After copy-tool changes: `python tools\port\copy_to_native.py --report`.
- Layout gates: `python tools\port\check_native_views.py`, `python tools\port\layout_check.py`, and
  `python tools\port\check_event_dispatch.py` (requires the extracted US BATTLE.PRG).
  Visual Studio currently does not discover this Catch2 suite; run `build\Debug\VS.Tests.exe` and
  `build\Release\VS.Tests.exe` (or `test-native.cmd <configuration>`).

  ### AI reaction-delay reset (F01791, #6; 2026-10-21)

  `Battle_Ai_ResetActorAiReactionDelay` is now an owned native implementation, traced
  to `func_800E4C64` in `BATTLE.PRG/7BDD4.s:571`. It ignores Ashley (actor 0) and
  missing AI states, and resets each present non-player state's minimum action delay
  to the source value of 30 frames. Out-of-range actor IDs are logged and rejected.
  The regression covers the player slot, all non-player slots, and a missing state.

  Validation on the current worktree: `test-native.cmd Debug` and
  `test-native.cmd Release` each pass 307 test cases / 828528 assertions.
  `quality-native.cmd --changed --tidy` exits successfully with 0 new findings
  (304 total baseline findings; format, cpplint, Ruff and clone checks each report
  zero findings). A repeat `python tools\scaffold\gen_solution.py` leaves generated
  project files byte-identical. `git diff --check` passes on the implementation's
  scoped files. The worktree also contains unrelated edits to
  `.github/copilot-instructions.md` and the `resources` repository pointers; those
  were preserved. #6 remains open: this slice does not establish flight, defeat,
  reward, or the broader gameplay acceptance criteria.
