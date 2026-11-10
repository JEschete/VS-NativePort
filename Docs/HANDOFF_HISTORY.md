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

## Continued non-MA acceptance (2026-10-27)

The owner explicitly resumed #151/#167/#18/#148/#149 after the bounded resume
feature. The following is local dirty-base804d1899 evidence, not publication
or issue completion. Unrelated deletion of Docs/ENEMY_DATA_SCHEME.md remains
untouched. Full solution regeneration also reflects that deletion in the
solution and refreshes existing tool inventory entries; those incidental
project changes are separate from the scoped gameplay/tool implementation.

### Semantic names and the owned dispatch audit

#148 advances two source-backed batches:

- F03449-F03460:12 vibration functions,31 identifier pairs,208 occurrences.
  Original source/assembly and existing numerical/device tests establish
  port/motor indices, pattern ownership, envelopes and Q8 intensity meanings.
- The existing182-ID normal/skip opcode contract supplies instruction and
  thread_index parameter meanings (573B8.c:67-142).180 ledger identities
  cover177 unique native names and353 parameter bindings (347 unique pairs)
  across16 C/header files. Original literals,
  numeric/operator/type tokens and unrelated generated files are unchanged.
  Registered C++ implementations, existing meaningful names and unknown
  locals remain untouched; no behavior/C++/retirement status is promoted.

Both generator batches are stable on repeat generation. All121 registered
owned implementation/header paths remain byte-identical to their saved
pre-batch hashes. Current lexical inventory reports24,835 candidate
occurrences/2,158 file-identifier pairs/371 files; this is unresolved candidate
work, not a semantic certification or a mass-rename target.

The actual `tools\port\check_event_dispatch.py` initially fails on unchanged
owned F01130: Event_ScriptSetAshleyBattleMode is missing from its declaration
translation unit. #218 records this before repair. The audit previously
included only copier-manifest headers, which exclude owned headers. It now
includes registered owned opcode headers while retaining exact canonical ABI,
all256 normal/skip slots and target-set checks. Missing owned headers fail
explicitly. Two new selection tests fail before the helper exists and pass
after repair; the actual audit changes from1 error to0 across179 unique
canonical target names (182 registered IDs include aliases).

Local naming/audit evidence:
`build\quality\issue148-vibration-verification.json`,
`issue148-event-token-verification.json`, `issue148-event-generation*.log`,
`issue218-red.log`, `issue218-green.log`.
Commands include two `tools\port\copy_to_native.py` runs, the actual
`tools\port\check_event_dispatch.py`,123 encounter-fixup tests and32 naming
tests. Neither #148 nor #218 is closed by this local checkpoint.

### Static original scene inventory

`--catalog-only --scene-catalog` writes the complete static scene inventory
and revision/worktree/data-root manifest without opening a game window.
The production opcode-size and dispatch tables drive bounded graph discovery.
MPD entry/local scripts retain original offsets and all ZND zone/room aliases;
variable-length trigger tables preserve masks, selectors and unevaluated
conditions/negation. Both conditional edges, signed forward/backward targets,
loops and child/parent threads are retained. Dynamic returns and special
operation prerequisites remain explicitly unresolved. No story flags, HP,
RNG, script completion or source assets are changed.

Original assets are not conflated:512 MPDs,512 external EVTs,797 SEQ/ESQ
actor-animation banks and one movie give1,822 unique asset records. There
are807 indexed and504 empty script definitions,576 triggers,5,444 branch
edges and30 loop edges. Every playback/precondition status remains unverified:
static discovery does not demonstrate legitimate campaign setup, audiovisual
fidelity, every dynamic path or return to player control. Missing/invalid
sections, instructions or targets are reported in the inventory and cause
command failure rather than omitted records.

The initial parser incorrectly required a full32-byte trigger-offset table;
the original evaluator stops at its firstFFFF sentinel. Real short tables
exposed391 parse errors. Source-backed variable-length reading plus short/
sentinel/condition regressions now indexes all assets with0 errors.
This was a new parser assumption, not391 original-content defects.

Actual catalog runs Debug1793100216430604-0, Release1793100223189335-0 and
repeated Debug1793100227491059-0 produce identical inventory SHA256
FD2B479A0D2D28B14F0C966DE13FF7C2B1F17B93A5C5E436B18390C26105A494.
Exact command: `VS.CombatLab.exe --data <absolute Native\data>
--catalog-only --scene-catalog`. Local verification:
`build\quality\issue149-scene-inventory-verified.json`.

Parser tests8/39; optional full-disc scene/door tests2/46,935 assertions in
both configurations, with repeat output equality. Full Debug/Release each
1032/9,578,509 pass after the naming changes. Focused `quality-native.cmd
--files` covers SceneCatalog.cpp, main.cpp, SceneCatalogTests.cpp,
VibrationTests.cpp, copy_fixups.py, test_encounter_fixups.py,
check_event_dispatch.py and gen_solution.py with `--tidy`:0 findings/0 new,
unchanged baselines. This is focused compilation-based quality, not a new
whole-tree static-analysis certification.

### Continuing door frontier and remaining limits

The frozen Debug52B374E0174BE753382ECB8D9AE4EAC17BB53E7CE68CD9B975DD0CDE2A2CBFD9
image retains the earlier96 passes, then executes only new frontier records
with --door-resume, --frames36000, --speed64 and private saves:

-1793097750475420-0:12 selected,10 passed/2 fixture-defeat blocks,0 errors.
-1793097911107060-0:100 selected,74 passed/26 blocked,0 errors.25 blocks
  are real fixture defeats; z048-r022-d003 has unavailable destination0/255.
-1793099116963801-0:100 selected,74 passed/26 fixture-defeat blocks,0 errors.

All212 records reach explicit outcomes/teardown, not fresh passes for the
retained history. The next forward-tail batch carries254 passes and selects
the remaining new tail. It subsequently completes197 records:144 passed/
53 blocked/0 errors in1793100327166692-0. Exit2 for
genuine blocks is not a crash, nor a passed physical/story gate. Evidence is
under `build\quality\issue151-frontier-20261027` and the frozen runner's
`combat-lab\runs` directory. Do not rerun these passed prefixes after repairs.

A fresh hash scan of31 local frozen executables finds no original #167 ACFD
image. Existing exact-state clean Debug/Release history does not identify a
causal malformed-packet producer; #167 remains open. #18's source audit now
accurately distinguishes working AV1/FLAC decoding and initial schema3 native
persistence from still-open import/migration/clear-save/campaign/compatibility
retirement gates. No local component or inventory result closes those gates.

### Completed all-record reconciliation and actual prerequisites

The final earlier-prefix run1793102211628943-0 preserves403 actual older
Debug proofs, validated by the native planner, and executes only458
never-passed earlier IDs. It completes361 passed/95 blocked/2 failed,
with complete teardown and no unrun rows. Reconciled exact IDs cover all
1,001 originals (1,003 observations include two deliberate repeat observations):
latest761 passed/238 blocked/2 failed. There are764 distinct prior-or-latest
positive proofs. Earlier passes for z040-r014-d003/z040-r008-d001/
z040-r009-d005 also have later blocked rows; neither fact is concealed or
relabeled fresh. Full local report:
`build\quality\issue151-frontier-20261027\all-door-observations.json`.

Both failed rows (z009-r017-d003/z012-r000-d000) stop on closed#158's explicit
E222/model53 missing-binding diagnostic at12/0/event0/source1/PC143; only
Ashley is present. Original MAP026 placement requires flag130=8,
scene flag1=1 and flag187<=1. The fixed post-prologue fixture establishes3,
not8. There is no evidence of a new spawning defect, and no fabricated actor,
changed flag, effect skip or weakened acceptance is used to suppress it.

Existing owner native saves provide actual source-aligned states without
editing their contents: slot01 backup generation1 has8/1/1 for130/1/187;
primary generation2 has10/1/2. Copies are private; original hashes are
755961a8bd2c193c5fae5b01d6a35ef654636d702eb901ace5deeea5448417ef
and dda977497fd0bdf9e7cf3bc52cc85509546a3f4e622e7d6f53588f3ee07e3946.
These establish the documented component preconditions, not a newly verified
full-story campaign.

Production Continue uses real1000:Start,1300:Circle,1450:Circle,1510:Up,
1570:Circle inputs. Preboss movement is Up2100..2114,Right2140..2206,
Cross2240,Right2300..2329; scripts hold each entry3 ticks. Cross opens the
door, then movement actually traverses it. Debug1793120300950490-0 and
Release1793121146016243-0 both restore1610, enter12/0 at2388 with model53,
play the original E222 entry, restore control3768, and hold the unchanged
quiet predicate4050..4267 with0 errors/forced skips. Release stops4300/
HP149. Debug continues6000 and later dies idle; no combat victory is claimed.

Postboss movement is Up+Right2100..2116,Down+Right2140..2210,Cross2250,
Down+Right2320..2349,Up+Left2850..2873,Cross2920,Up+Left2980..3019.
Debug1793120889832308-0 and Release1793121158151381-0 both restore1615,
enter12/0 at2402, physically return9/17 at3066 and retain normal control
3104..3900 with0 errors/forced skips and HP250. Initial camera yaw512 is
why these real diagonal inputs differ from the preboss yaw1024 route.
No live position/HP/story/event writes occur. Local strict report:
`build\quality\issue151-minotaur-legit\physical-door-source-preconditions-verified.json`.

### Additional semantic contracts and independent packet experiment

F01099 originally named ScriptPreloadRoom actually calls8007BBB8, then
80069DEC (main cached-actor-state clear). It is now
Event::ScriptClearRoomActorCache, with the original symbol/address retained.
F00231's parameters are mapId/zoneId. Exact expected-output comparison
restricts generation to one symbol rename and two wrapper parameters across
six generated files; both generations are stable and121 owned paths stay
unchanged. Native before/after1 case/22 assertions verify matching and
nonmatching rooms/zones, duplicate cache entries, unchanged current slot,
idempotence and absent-cache no-op.

F03463-F03467 separately rename10 allocator bindings for insertion predecessor,
following free block, selected allocation, remainder, native allocation units
and arena byte extent. No arithmetic, width, link/reset order or behavior
changes. The earlier purpose text incorrectly promised16-byte alignment;
native blocks use sizeof(Core_HeapBlockHeader), which is24 bytes on this host.
Before/after Debug/Release heap2/22 controls splitting/coalescing from both
ends and verifies payloads are not implicitly zeroed. Exact two-file output
comparison and stable second generation preserve all121 owned paths.
Evidence: `issue148-room-name-verification.json`, `issue148-heap-verification.json`
and corresponding generation/test logs under ignored build\quality.

A distinct #167 experiment avoids guessing from the unavailable old image.
The installed LLDB driver lacks python311.dll and then liblldb.dll; a private
signed Python runtime resolves the first dependency, not the absent backend.
No installed tool or global environment is modified. An ignored native Win32
debugger is built instead, resolves the matching frozen PDB's real heap-init
RVA3BF800, and fills only the complete free arena before its initializer runs.
It does not edit an active actor, HP, flags, event state or completion.
Debug41300/session1793119680872832-0 fills16MiB twice withCD and still
completes the full current scene chain: control13320/pass gate13440/
teardown13452,280 confirmations/0 errors. This is allocator-stress evidence,
not normal gameplay certification or a causal historical #167 fix.
Private harness/logs are under `build\quality\issue167-debugger` and
`issue167-allocator-Debug`; no dumps or source assets are attached/committed.

Final static inventory also annotates102 chained-event and443 room-handoff
commands, preserving signed event IDs, current-zone0 versus explicit zones,
all entry coordinates/flags and transition effect/speed. Parser12/50 and
optional scene/door/cache3/46,957 pass; all1,822 assets parse with0 errors.
Debug inventory1793121459722536-0 SHA256 is
C3CB9272E0F86E9A23605D83C4D7D31E3BD36D68C4177A031D14CE8450C1B045.
It supersedes the earlier static output hash only for the added semantic links.
Full Debug/Release before the two added heap cases each1037/9,578,542 pass;
125 copier controls and default/focused quality gates have0 new findings
(the selected EventScriptTestCases file retains6 existing findings).

## Passed-stage checkpoint resume (2026-10-26)

Owner observed that retrying an entire132-case prefix after each fix wastes
time. New `--door-resume <sequence-results.json>` reads actual prior door
results and their neighboring manifest. The existing stable inventory,
single-window runner, selectors and per-case acceptance are reused.
No prior passes are rerun in repair workflow unless full/history replay is
deliberately requested by omitting the checkpoint.

The planner retains only actual passed door rows with initialized=true,
engine_errors=0, teardown_complete=true and the existing120-tick observation
interval. Current encoded door identities/conditions/destination/entry data
must match. Changed/removed contracts are reported as invalidated, not reused.
Schema, duplicate identities, incomplete passes, unknown selectors and unreadable
checkpoints fail explicitly. Current empty inventory cannot become a successful
zero-work fallback. Data-root identity is checked before reuse.

Prior evidence stays in `retained_passed` with original checkpoint, revision,
dirty state/source status and data-root provenance. Fresh scripts/counts include
only cases actually executed. Prior passes outside a selected retry range are
carried forward for the next checkpoint; they are not called fresh verification
of a new revision. Failed/blocked/cancelled/running/not-run rows are never promoted.
The next checkpoint combines retained passes and fresh passed rows without
losing their distinct provenance. Already-passed-only selections produce a
zero-case completed checkpoint, no gameplay frames, and no fabricated fresh pass.
`--door-limit` counts executable pending cases after filtering, not skipped rows.

Fresh dirty-base5db1173d verification:

- `test-native.cmd Debug "[resume]"`:8 cases/39 assertions cover132 synthetic
  stages,95 retained/37 pending, broken-stage selection, post-filter limits,
  zero-work reporting, chained provenance, changed contracts, incomplete/
  duplicate passes, malformed schema/selectors and all non-pass states.
- Full Debug and Release each1,020 cases/9,578,414 assertions pass.
- Focused `quality-native.cmd --files Native\Tools\CombatLab\src\DoorTests.cpp
  Native\Tools\CombatLab\src\main.cpp Native\Tests\src\DoorTransitionTests.cpp
  --tidy` and default gate both0 findings/0 new; baselines unchanged.
  These are the affected planner/runtime/test translation units; no full-tree
  static-analysis result is claimed by this focused command.
- Real checkpoint:
  `build\quality\issue160-native\history167-Debug\combat-lab\runs\
  1793068535457849-0\sequence-results.json`.
  Already-passed z040-r008-d000 resumes in Debug1793071650310342-0:
  selected0,retained95,empty fresh counts/scripts,exit0 and no gameplay.
- Actual Debug1793071739553589-0/Release1793071739641813-0 use that checkpoint
  with `--door-from z048-r006-d000 --door-limit1 --frames36000 --speed8
  --scripted`, private saves and frozen matching executable/PDB/DLLs.
  Both retain95, execute exactly one `sequence.script.begin` for
  z048-r006-d000, restore control13320 and pass/teardown13452 with0 errors.
  No passed stage is replayed; this reduces132 executed selections to1.
- Resuming the resulting Release checkpoint again for z048-r006-d000 retains
  96 passes and executes0,exit0. This verifies cumulative reuse after a fix.
- A missing checkpoint in actual `--catalog-only` mode returns1 and a
  diagnostic, not an unfiltered fallback. The full inventory remains saved
  even for filtered/zero-case runs.

Logs/results are local under ignored `build\quality\door-resume151`.
The original132-stage evidence is read-only and its blocked rows stay blocked.
This feature satisfies bounded resume workflow, not all of #151's exhaustive
door/physical-story acceptance or #167's unresolved historical producer.
Unrelated owner deletion of Docs/ENEMY_DATA_SCHEME.md remains untouched.
The resume milestone is published in
`c9f6b43a491581e55709ce1fcacb2a6db4f557de`, exact successful Source Quality
40445126847. #151 retains its broader open acceptance; only this retry
workflow slice is complete.

## Gravity material and canonical target origin (2026-10-26)

Owner clarified that #160/#167 may be modernized for correct native behavior,
not byte-exact legacy reproduction. Ghidra12.1.4 and its installed MCP
extension were launched with JDK25 into ignored `build\ghidra\VSNativePort`;
the owner opened/analyzed BATTLE.PRG. MCP confirms little-endian MIPS,
577,828 bytes mapped80068800..800F5923, source SHA256
ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12.
The installed PS1 loader is not active in12.1.4; this raw-overlay static
analysis is checked against existing original instructions, not certified
GTE emulation or dynamic original-state evidence.

Ghidra800D7A14/800D6CF0 confirms the source binder and unchanged selector.
Among278 nonempty original effect block1 payloads,276 supply one table,
one supplies zero and E210 supplies three. All table/descriptor extents
validate. E084 authors channel1 for its grid/rings but supplies only table0.
The native binding intentionally maps logical channel1 to that package's
own authored table0 only for primary E084 with one source table. It retains
authored frame numbers, descriptor/palette/UVs and immutable original bytes;
there is no white texture, cross-package borrowed table or skipped draw.
The new owned TextureBindings API receives the actual block payload extent
from the copier, checks table/header/count/alignment/frame descriptor spans
before publication, and clears channels absent on reentry. E210's distinct
tables and secondary resource contexts are unchanged.

Material red2 cases/8 assertions has3 failures (missing E084 channel and a
stale table on reentry). New controls cover actual authored holds/offsets,
ended/delayed frame retention, multi-table separation, secondary isolation,
all290 extracted P files (including placeholder/empty files), and three
isolated malformed descriptor/offset/frame controls, each diagnosed/exit3.
The actual ClosedRing entry emits32 textured packets with correct CLUT/page/
UVs and expires/releases storage on three production-list cycles.

The material-only route no longer AVs, but owner observed Gravity bottom-left
offscreen. This was not accepted as visual completion. A separate exact
source defect explains it: G00532/D_800F5310 equals G00529/D_800F5230+0xE0,
the request's position. Ghidra800CF0E8 copies232 bytes including that field;
Battle_StartActionPresentation copies action.focusPosition into it.
Native code had separate SVECTOR[109] storage inferred from an oversized
876-byte source-symbol extent, so origin-attached transforms used zero.
G00532 is now an8-byte field view of Effects_actionRequest.position through
the existing ledger/generator; detached storage/module registration is gone.
Red2/11 placement checks all fail; green2/12 verifies the canonical request
address, absence of the stale module symbol, and actual initialized/updated
world/view matrices at target[1200,-384,900], camera offset[-100,200,600].
The PLG074 fixture formerly assigned conflicting origin/request values.
It now assigns one canonical request per case, preserving every original
anchor expected value and assertion; no runtime expectation was weakened.

Owner fresh live observation after the repair: Gravity was on Ashley.
Normal-speed frozen Debug/Release original z023-r004-d002 replays use
private saves, original actors/events and no stat/HP/position/flag edits.
Material-only Debug1793066888797409-0/Release1793067082710968-0 and
target-corrected runs under `target-visual-Debug`/`target-visual-Release`
survive1508, retain original action137 damage45/45/46/46 at1504, clear
effect state1692 and restore ordinary control1726. A later real action84
defeats the unchanged fixture; the runner exits2/blocked with complete
teardown, not a passed door. Live-window captures1512/1540/1580/1620
and bounded primitive traces remain local. The initial `--capture` image
was taken after teardown and showed the editor; it is explicitly not active
effect evidence. Owned-window PrintWindow captures and owner confirmation
establish selected target placement, not all-spell artistic/reference parity.

Fresh dirty-base4e89e9ae validation:

- `test-native.cmd Debug "[gravity-placement]"`:2/12;
  direct `[plg074],[gravity-placement]`:15/3,654.
- Full Debug and Release each1,012 cases/9,578,375 assertions.
- Port133/naming32; native-view621/0 mismatches; two copier generations
  repeat1,031 outputs and preserve121 owned paths and unchanged baseline.
- Focused `quality-native.cmd --files ... --tidy` and default gate0 new.
  Required full `quality-native.cmd --tidy` is still running and is not
  pre-claimed successful.
- The new parser fixup initially duplicated an existing F01626 registry key;
  the failed compile exposed that it was shadowed. It was merged into the
  existing list, preserving auxiliary-audio extent adaptation, and a registry
  regression now checks both. C-header interop is registered in the existing
  scaffold predicate. No generated native/project output is hand-patched.

Separate #167 observation uses the preserved pre-fix22fbc737 Release image,
not this E084 repair. Exact persistent prefix `--door-from z040-r008-d000
--door-limit132 --frames3600 --speed64 --scripted` completes all132 cases:
96 passed/36 blocked,0 engine errors, no AV/malformed packet. Final
z048-r006-d000 remains honestly blocked by authored event budget, as its
long standalone chain previously passed; no threshold is weakened. Local
session1793066888873758-0 retains per-case results. This does not identify
the historical ACFD56-word packet producer or justify a speculative #167
code change.

Evidence and retained scripts are under ignored `build\quality\issue160-native`.
No original assets, screenshots, dumps or Ghidra databases are committed.

Final publication:142491cc003c35ba040204d542338262c2eee988 contains the
material/ledger/test repair;706c41592654f5dbee0fd8f9856feea151236b9b
includes the generated Runtime.c/h canonical view that was initially
omitted from staging. The output guard2e40a3212228cf4063a5ee416c2abc21e3fd8079
now checks both the ledger and generated macro/no-independent-storage body,
so CI cannot validate just the catalog while missing the generated fix.
All three have exact green Source Quality40438987477/40439217035/
40439326171. Required full tidy completes successfully:294 existing/
0 new with unchanged baseline. The attempted Pylance syntax checks were
unavailable after MCP catalog changes; BOM-aware Python AST validation,
Ruff and all133 port controls pass instead, with no editor-environment claim.

Target-corrected normal-speed Debug1793067819656796-0 and
Release1793067908613441-0 both clear effect1692, restore control1726 and
retain original four-limb damage at1504. All active captures are local
under target-visual-Debug/Release; owner accepted the actual placement.
#160 is closed with source/CI/component/runtime and owner evidence; #149/
#151 were reconciled without rewriting the later fixture-defeat result.
Current exact132-case Debug history for #167 subsequently completes under
`history167-Debug`, session1793068535457849-0:95 passed/37 blocked,0 engine
errors, complete host trace and teardown. At final case132/script1050
(aggregate168592) it matches the historical fault state exactly:
zone32/room17/scene30/control3,event0/source1/phase4,thread0 PC delta233/
wait70,models0/170/169/10. No malformed packet/AV occurs. The original
3600-tick final-case budget still blocks the long authored chain; no gate
is relaxed. Blocked reasons are14 fixture defeats,7 source-event budgets
and16 destination-event budgets. Current Debug exe SHA256
210b16e2ea56d9051b4688b295e7dd43c2e75e6a5440c80bbe7d0b54d1e5ae3e;
the preserved pre-fix Release prefix separately reports96/36 with0 errors.
All132 cases are accounted for in each run, not all passed.

The source/code fix and owner acceptance close #160. #167's historical
producer remains unproven despite exact current state/history alignment;
no code was invented simply to close it. Its next discriminating input is
a current failing packet with producer/call stack or the historical ACFD
matching image. Ghidra MCP is live and is no longer a startup blocker.
Per-issue/#4 comments and local
`scene167-persistent-debug-verified.json` preserve these limits. Unrelated
owner deletion of Docs/ENEMY_DATA_SCHEME.md appeared during validation and
was left untouched and unstaged.

## Current separate scene observations and blockers (2026-10-26)

Both observations below use frozen22fbc737 executable/PDB/DLL sets with
private save directories, after exact green Source Quality40431257034.
Original assets/health/story state are not modified; Lab uses authored
room events and original dialogue-confirmation inputs.

- #167/#149/#151: `VS.CombatLab.exe --data <Native\data> --save-dir <private>
  --door-transitions --door-case z048-r006-d000 --frames 36000 --speed 8
  --scripted`. Debug1793063834761061-0 and Release1793063834657676-0 both
  align zone32/room17/scene30 at ticks979..6216, restore40/21 ordinary
  control13320, meet the unchanged120-tick gate13440 and teardown13452.
  Each records280 dialogue confirmations, a passed loader_entry_event
  result,0 engine errors, host exit0 and complete trace. Debug exe hash
  736a233584bccd934de5c3b09ad823e5e95e98f98875571939b0b7b112bc119d;
  Release435c8fbebd733195a86a56fa6a8833c1c8575f7169c562a12ba9d34c5d6260c8.
  These reproduce current standalone success, not the historical ACFD
  persistent-image malformed56-word packet. The named historical frozen
  artifacts are absent from current build\quality; a matching-image stack/
  packet producer or a current persistent-history reproduction is still
  needed. #167 stays open; no inferred #184/#185 shared cause or skip fix.
- #160: the same hosts with `--door-case z023-r004-d002 --frames 4800
  --speed 16` both AV at1508. Debug1793064516424253-0 and
  Release1793064516431171-0 retain primary request84, definition16,
  parameter101E, table1/program1/count1, cursor4/word0002001D, null
  previous texture and the0x76 read at ClosedRing.inl:230. This is fresh
  evidence that #42/#197 did not repair the authored-selector/binding
  problem. The original-runtime table1 binding/history during action137
  remains the discriminating observation; static/native data does not
  justify subtracting a selector, aliasing a table or supplying a texture.
  Portable DuckStation savestate/memcard directories currently have no
  files; its installed copy was not run/reconfigured. Ghidra MCP's backend
  remains unavailable at localhost8080. No native effect code was changed.

Logs/manifests/results remain under
`build\quality\issue42-final\nonma167-Debug` and `nonma167-Release`; later
E084 sessions coexist with the successful scene-chain sessions. Per-issue
comments retain the actual outcomes and limits. #18 still includes
campaign/NG+ and compatibility-retirement acceptance; excluded MA work is
not silently completed. #148's full semantic sweep and #149/#151's
exhaustive scene/door coverage also remain open, not inferred from these
selected runs.

## Separate whole-tree header/save quality repair (2026-10-26)

#42 closed after final source98d38089 and evidence011eecc3 received exact
green CI40428688667/40429529689. #197 was opened before repairing the
newly inventoried whole-tree findings; it is separate non-MA work.

The eight original affected source files are repaired without baseline
changes. Content's sampled-data member uses the native spelling
`sample_source` with both loader/test consumers updated; table layout and
positional generated bindings are unchanged. LFO invalid selectors retain
their explicit error/null result in both C17 and C++23. Save size-limit
arithmetic widens before multiplication. Room-light and snapshot dimensions
receive semantic constants with identical values/types/extents; snapshot
wire schemas remain unchanged. Save-menu parameter names and asset phases
follow current naming, with explicit resource-count/page constants.

The container-save state machine keeps its original0..3/7..16 values,
frame boundaries, inventory-copy order, rollback, fallthrough and cleanup.
Named phases and bounded initialization/staging/restoration/completion
helpers replace the oversized function. Invalid states now log explicitly
without changing the state or pretending completion; a new observer test
verifies that contract. During extraction a missing source-address operator
was caught by the compiler and corrected before acceptance.

Five existing save tests exceeded expanded Catch statement limits when
their source was analyzed explicitly. Existing input/consent/scroll/bank
checks were moved into bounded helpers, retaining all original assertions
and sequences rather than suppressing checks. The save selection remains
19 cases/1,426 assertions including the new three-assertion diagnostic test.
Full Debug/Release each pass1,002 cases/9,572,050 assertions.

Fresh commands/evidence under `build\quality\issue42-final`:

- `quality-native.cmd --files Native\Game\src\Save\MenuAssets.c
  Native\Game\src\Save\NativeContainer.c Native\Content\src\ModuleLoader.cpp
  Native\Tests\src\NativeMenuTests.cpp Native\Tests\src\ContentTests.cpp
  --tidy`:3 existing/0 new.
- `test-native.cmd Debug "[save-menu]"`, direct full Debug executable,
  and `test-native.cmd Release`: all pass with the counts above.
- Port unittest discovery131 passes. Two copier generations repeat1,028
  native outputs, preserve119 owned paths and leave the baseline unchanged.
- The required repeat `quality-native.cmd --tidy` is running and will retain
  both its text output and JSON report as `quality197-full-tidy.txt` and
  `quality197-full-report.json`. This checkpoint does not pre-claim a clean
  full-tree result or issue completion.

Production repair22fbc737 has exact green40431257034. The repeat full tidy
subsequently completed with304 actionable/10 new, all ten in four unchanged
test files: ActorAttachmentTests, JumpEntryTests, EncounterTests and
TextTransitionTests. Whole-object memcmp checks become explicit byte-array
snapshots (retaining every byte), an existing snapshot helper is reused,
fixture extents/indexing widen before multiplication, and controller buttons
retain their original unsigned type. An oversized event test moves the same
handoff assertions into a bounded helper. No expected values/assertions are
removed; full Debug/Release remain1,002/9,572,050 each.

The smallest covering recheck is four-file tidy:0 findings/0 new. The retained
full-tree JSON plus the fresh four-file JSON are verified using the existing
quality Finding/new_findings API and the unchanged baseline. `git diff
22fbc737 --name-only -- Native` must equal those exact four test paths, with
no untracked native sources, before retaining any full-tree diagnostics.
The resulting `quality197-incremental-report.json` records294 existing/0 new
and baseline SHA256
a205dd02dd039ded00ff4a0d99160cc9c1966b9609c263add4ce892b97faad24.
This is explicit source-checked incremental verification, not a claim that
the earlier full command exited0 or a baseline refresh. Final fast gate is
0/0. Final test repair `eb0e49a287b2327ae8bd7728c67851ae4f5094ae` has exact
green Source Quality40433467658 and #197 is closed. The issue-close CLI
completed, then its Python output display hit a local encoding error;
subsequent readback independently confirmed CLOSED and the exact green
revision. #4/#149/#151/#160/#167 were reconciled with actual outcomes.
The next unresolved crash investigation is blocked on original E084 table1
binding/history; no unsafe placeholder patch is left in the worktree.

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
disc-derived data or captures are committed. Final implementation is
`c61d4eab47b35a79ce5bfdc920a4dadd4ac3dab1`. Its exact CI40428271455 failed
on the three owned-AI timing-header include positions. The scoped repair
`98d38089bf1b9b38ec5dab7668a5da389fb47a69` passes exact Source Quality
40428688667, the default gate0 findings/0 new and focused AI tidy0/0.
No baseline changed.

The full `quality-native.cmd --tidy` completed with389 actionable/95 absent
from its historical baseline. Three are the now-fixed include positions.
The remaining emitted locations are eight unchanged Content/audio/render/
save headers and save implementations; their Git blobs exactly match
owner checkpoint506f750b. Header diagnostics repeat across translation
units, so95 is not a distinct-defect count. This newly inventoried scope is
tracked in #197 and is not hidden by a baseline refresh, misrepresented as
an all-tree pass, or silently folded into #42. The full command's UTF-16
output remains `final-full-quality.txt`; changed-code analysis and both
full runtime suites pass independently. #25's earlier fast-quality scope
remains distinct. Continue #197 as separate non-MA quality work, then the
remaining crash/coverage/name investigations without weakening their gates.

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


## Archived Handoff: 2026-11-09 / f6ba998e

Historical snapshot only; its current/next/pending statements are not live status.
Source revision: f6ba998e902c41aa4de4ddbc86a141cbae3f0d37
Prior archive bytes: 273703
Prior archive SHA256: a85ce204ac0a4615975d279c27653531ec80e7c753fbeb6d05f7438c86afac27
Source bytes: 593590
Source SHA256: c9cd784fd36c241446cfc13d207f34b2d6eeaf6721c5a50b93061b7f5d2d481a

# Current handoff

Updated 2026-11-09. This is a short entry point, not another work tracker.
Detailed checkpoint evidence, commands and replay paths are preserved in
[HANDOFF_HISTORY.md](HANDOFF_HISTORY.md). Historical results are not fresh
verification of the current worktree.

## Work order and current state

### Current AI Simulated Results

- Ordered row28 remains active. F01697 ApplySimulatedActionResults is promoted
  in existing PlanningPrimitives core/native code, reusing ActorCondition and
  the owned signed stat-delta helper. Producer checks in ConditionSnapshotBinding
  verify HP, MP, weapon DP and shield DP rather than guessing decomp fields.
  Cost types1/3/6 apply before performer effects; ordered targets follow,
  including repeated actor slots. Only operation2 restores; status clear wins
  after status set. Original C/module800DC888 and generated caller remain.
- Original134MIPS800DC888-CA9C match actualBATTLE bytes, SHA256
  `342ddca4ea84cdc5aa5bae9d02ee308546f3188b4234f01b3522177a1cd6e467`.
  Native workspace decoding checks the complete flexible target tail against
  known allocation bounds and native255target capacity, never sizeof(header)
  alone. Byte copies avoid additional alias reads; output updates only current
  resources/statuses and preserves maxima, row IDs and reserved metadata.
  Short tails, overlapping buffers and malformed active IDs refuse before
  output stores; borrowed outside-arena buffers remain caller-owned contracts.
- Focused11cases95assertions pass; with readiness native18cases126assertions.
  Both freshly rebuilt full configurations pass5167cases/12,593,504assertions
  with zero failures/skips/regressions and unchanged2400/900canonical replay
  frames. All202copiercontrols pass actual0; source quality0/0 and full
  header-aware quality5existing/zero new actual0 pass without changed baselines.
  Both ledgers pass zero violations:2768repositorypromoted/oneframewip;
  modernization2767done/977todo. Original3479cohort2486promoted/16retired/
  976generated/oneframewip gives2502resolved/977remaining, fifty-three additional
  since6c8ac0b9. This is not full battle-ai, M8 combat or EC/global retirement.
- Two pre-status generations preserve1761reviewedpaths/rawhashes; manifest
  `bd04d1ec83116ba434b186ddcdd9eec383405da7ba57ed4dd2f6b59b2bbaf8d1`.
  Final post-status generations also preserve all1761paths/rawhashes and the
  same manifest. This scoped milestone's publication/CI are next. Prior
  readiness376c79691850067face6bbd9508fc3858d8bbf89 has successful
  exactCI42705380792;276comment6752381288 records it. Earlier pending-publication
  wording below is historical and superseded by this checkpoint.

### Current AI Candidate Readiness

- Ordered row28 is active. F01637 RefreshActionCandidateReadiness is promoted
  in the existing PlanningPrimitives core/native owner. It reuses packed
  ActionCandidate records and existing targeting/usability/movement services;
  both generated production callers and original C/module800D85D8 are adopted.
  Its obsolete two-line cast adaptation alone is retired through the copier.
- Original132MIPS800D85D8-87E4 match actualBATTLE bytes, SHA256
  `82be842cc0d5706927697f48963818b74a64d63ab3bed4460634a04b7d462db3`.
  Weighted/threshold gates precede targeting; row/column and loop counts are
  read fresh after callbacks; clearing preserves every other candidate bit.
  First surviving rank replaces only slot0 without compaction. Movement
  selection precedes fresh count/status; no candidates yield hostile2/absent3,
  otherwise unsigned readiness+30 is compared against the first threshold.
- Core/native/module14cases131141assertions pass, including all65536thresholds,
  callback field/count changes, exact packed-state preservation, real native
  usability, promotion/movement and linked-actor status. Native canonical/view
  offsets are asserted; count/rank/known-heap/room/table-identity and cyclic
  roster checks refuse malformed data without invented status. Borrowed
  outside-arena inputs and global/AI lifecycle retirement remain separate.
- Both freshly rebuilt full configurations pass5156cases/12,593,409assertions
  with zero failures/skips/regressions and unchanged2400/900canonical replays.
  All202copiercontrols pass actual0; source-scoped quality0/0 and full
  header-aware quality5existing/zero new actual0 pass. Both ledgers pass zero
  violations:2767repositorypromoted/oneframewip; modernization2766done/978todo.
  Original3479cohort2485promoted/16retired/977generated/oneframewip gives
  2501resolved/978remaining, fifty-two additional since6c8ac0b9.
- Two pre-status generations preserve1761reviewedpaths/rawhashes; manifest
  `98d997a5306c4c44a401a54f9e322062ae2c1da88b3e0f4be9c58e4e53f20b07`.
  Final post-status generations also preserve all1761paths/rawhashes and the
  same manifest. This scoped milestone's publication/CI are next. Row28 remains
  active; this is not full battle-ai or M8 combat closure.
- Prior row27 is published36ae14438b9fddab6951cbdd7a8cdd3c2c4cd84c with
  exactCI42698413726 successful; all325-332issues are reconciled closed with
  fixing revisions and evidence.276comment6751475112 records row27 completion.
  Earlier pending-publication wording below is historical and superseded here.

### Current Row27 Completion

- F00547 CurrentSavePointId and F00828 RefreshAshleyInteractionTile are
  promoted in the existing ChestInteractions core/native binding. Save-point
  decisions preserve placement/flag/collision ordering, the physical platform
  probe versus actual trigger tile, held-mask504000 and kind16 activation.
  The original native high-byte discriminator already passed; no signedness
  bug is claimed. Maintained save-menu callers now consume the typed query.
- Interaction-tile decisions retain slot0, all nine accepted byte animations,
  one/two-tile reach, every signed-heading quadrant and fullword coordinates
  after unsigned tile reads. The native adapter preserves actor bytes and
  every map byte except the selected reservation bit; known-short/misaligned
  records and out-of-grid nonempty maps refuse before aborting services.
  The preview caller is typed; original C/modules8008A4FC/8009E634 remain.
- Original53MIPS8008A4FC-A5CC SHA256
  `45d5a93144c5eca577a48842ebad29b83f37f3c5f38c33054a5bd9fe4560e22e`
  and51MIPS8009E634-E6FC SHA256
  `2e35605af026c61f48719a2835ddc2306c234a88b2e87dbf5ed4adbb6a730614`
  match actualBATTLE bytes. Coupled18cases526548assertions pass. Both freshly
  rebuilt full configurations pass5142cases/12,462,268assertions with zero
  failures/skips/regressions and unchanged2400/900canonical replay frames.
  All202copiercontrols pass; focused14file quality0/0 and full header-aware
  quality5existing/zero new actualexit0 pass without changed baselines.
- Both independent ledgers pass zero violations:2766repositorypromoted plus
  oneframewip; modernization2765done/979todo. Original3479cohort is2484promoted,
  16retired,978generated and oneframewip:2500resolved/979remaining, fifty-one
  additional since6c8ac0b9. Structured audit follows every one of the original
  51primary IDs despite relocation:46promoted/five retired/zero pending.
  Plan row27 is done for C++ ownership; generated Gimmicks/Doors/Interactions
  retain required data but no pending function bodies. Global/controller/
  reference/allscene and EC/data/type lifetime retirement remain separate.
- Two pre-status generations preserve1760reviewedpaths/rawhashes; manifest
  `cad2fad115dbb42653a4240bfc9efe7682e318a2e626b970ded5a6aea1cd68ed`.
  Final post-status generations also preserve all1760paths/rawhashes and the
  same manifest. Final policy commit/push/CI are next.
  Row28 battle-ai stays the next ordered slice after this publication gate.
- Published627f829924c3820568ac2793c7eac1078f0119dd has successful exact
  SourceQuality42689907989. Issues325-331 are closed with fixing revisions
  and their specific regression evidence;276comment6750380662 reconciles it.
  Final body retirement exposed missing direct actor/SHP includes: issue332
  was logged before repair and its eight include-only changes are separately
  published62214ed43c2faa22cd856075b2a3f60b1e45c81c. ExactCI42697089210 is
  successful;332 stays open until the dependent final body-retirement revision
  also passes exactCI. The final policy work is still uncommitted.
  Earlier row27 subsections below retain historical evidence; their older
  pending-publication wording is superseded by this checkpoint.

### Current Platform Policies

- F00066 FreezeRoomGimmicksTemporarily and F00619 SetMovingPlatformVisibility
  are promoted in the existing PlatformPlacement core/native adapter.
  The former reuses the owned freeze service with exactly180ticks; the latter
  stores the wrapped one-bit hidden state before geometry lookup and rereads
  it afterward. Original C/module entries8006CE50/8008E224 and generated
  callers are retained. Native bounds cover whole aligned records, including
  the fifth record, and captured room/geometry identity checks stop stale writes.
- Original8MIPS8006CE50-CE6C SHA256
  `82b15b6ac1f14a5dc1dcb5fbb5c76ca576532d7d9d53606031caf34d63d70de5`
  and44MIPS8008E224-E2D0 SHA256
  `6f91c0d7be766da9b1072c9b76a5c455c764c3252c167ec1b6e67d3a21e12951`
  match actualBATTLE bytes. Exhaustive lowword inputs, signedword extremes,
  appearance/store/lookup ordering, callback rereads, exact packed-byte and
  geometry-bit preservation, malformed tables and original modules are checked.
- Full rebuilt Debug/Release each pass5128cases/11,935,773assertions with
  zero failures/skips/regressions and unchanged2400/900canonical replay frames.
  Combined platform policy/control20cases786594assertions and all202copier
  controls pass. Source-scoped quality is0/0; the full header-aware gate passes
  109existing/zero new with actualexit0. Both independent ledgers pass zero
  violations: repository2764promoted/oneframewip; modernization2763done/981todo.
  Original3479cohort2482promoted/16retired/980generated/oneframewip is
  2498resolved/981remaining, forty-nine additional since6c8ac0b9.
- Two pre-status generations preserved1760reviewedpaths/rawhashes; manifest
  `cdea13c840c0f6457482ebb194b0f1b3da353a89d7dba67bad7ddc016ec4aed8`.
  Final post-status generations also preserve all1760paths/rawhashes and the
  same manifest. This C++ milestone's publication/CI remain pending gates.
  Row27 now has44promoted/five retired/two generated
  primary IDs; remaining F00547/F00828 stay ordered. Controller/reference/
  allscene visual and borrowed global/lifetime retirement are not claimed.
- Workflow checkpoint: the isolated #325 ledger-integrity fix is published
  as b801734c5842a12713cf591a44585c3f8680dcf2; exact SourceQuality42687546097
  succeeded. Supported marking preserves842unique type IDs and the complete
  T00542 notes/retired status; #325 is closed with that fixing revision and
  acceptance evidence. C++ work remains uncommitted; unrelated edits
  are preserved. No issue closure is implied by these local promotion counts.

### Current Room Trigger Operations

- F00065 DisableTrapAtPosition, F00602 RoomTriggerAtTile and F00616
  SetRoomTriggerEnabled are promoted in InteractionTarget core/native
  binding. The preexisting header-only target owner now has a strict core
  implementation; C wrappers retain original pointer and null-all shapes.
  AI RoomState and ChestInteractionsLive adopt typed generation targets;
  generated effect/save-point callers bind the retained entries.
- Original30words8006CDD8-CE4C SHA256
  `2b95144922fc623f2491e177f5883a2d7e7a13dadd3d5f3871429fc96038bae2`,
  52words8008D438-D504 SHA256
  `e711275dda7b2921be52e7ebae2838cc6eb01f7e2b15e1d42d59a3104ab4dc9f`,
  26words8008DEAC-DF10 SHA256
  `802aba4d53f944ddb8e887b004715b6b989d8d2fb0d23ec968d7a4ccc5dcc7a1`
  all match actualBATTLE bytes. Lookup keeps signedhalfwords/fullquerywords,
  first matching record and immediate suppression for signedkind<16 when
  either unsigned opaque20/40 byte is nonzero. Activation keeps only low8
  input bits for one target or all records in order. Disable keeps signed
  world/128 truncation/layer0/activation1 before rereading tile for obstacle.
- Focused new/existing query/activation/disable/view/module16cases196732
  assertions pass, including all65536signedposition values, callback tile
  mutation, exact byte-only native stores, firstduplicate suppression,
  stale/partial/foreign targets and actual AI flag0x80->0x480. Existing whole
  twelve-byte/alignment domain guards stay intact; captured generation/base/
  extent prevent later writes on replacement. Original malformed tails were
  not verified accepted inputs; domain safety remains stricter there.
- Initial FullDebugRelease each5115cases/11,542,461assertions pass with zero failures,
  skips or regressions; canonical2400/900frames stay unchanged. Scopedtenfile
  quality0findings/0new after nine brace-only repairs with all assertions kept
  and no baseline/suppression edits. Generation first added only the new
  C header's project/filter metadata; unchanged fail-closed check then passes
  both runs with1,760reviewedpaths/rawhashes unchanged. ManifestSHA256
  `0d05af2441451496192a5b497fedadd70258752da9450f927daf0cc440c16ff7`.
  Owner-authorized recovery subsequently restored232missing maintained cases
  without dropping current edits; delayed-door fixtures bind their actual
  room records and packed fall coordinates retain31/30. The two new bounded
  scene-reader cases for #331 remain intact. Fresh rebuilt Debug/Release each
  pass5117cases/11,739,089assertions with unchanged canonical replays and zero
  failures/skips/regressions. Focused recovered60cases461203assertions and
  minimap30cases343714assertions pass; focused quality0/0 and wider quality
  109existing/zero new with actualexit0. All202copiercontrols pass actualexit0.
  VR-ROOM-TRIGGER-OPERATIONS/CPP/MOD are recorded; both ledgers pass zero
  violations. Repository2762promoted/oneframewip; modernization2761done/983todo.
  Original3479cohort2480promoted/16retired/982generated/oneframewip gives
  2496resolved/983remaining; forty-seven additional since6c8ac0b9. All current
  work remains uncommitted; publication/exact-revision CI and issue closures
  remain separate. Final post-status generations preserve all1760reviewed
  paths/rawhashes and the same manifest. Recovery-only scratch files are
  removed; the bounded generation/quality verifier is reused for the next
  row27 batch.
- Published-baseline primary-file ID audit found51currentmatches:39promoted,
  five retired and seven remaining copied bodies (not moved completions).
  This slice addresses three; remaining F00066/F00547/F00619/F00828 stay in
  orderedrow27. The current primary cohort is42promoted/five retired/four
  generated. Do not mark the row done from final-path searches alone.
  Borrowed pointer/controller/reference/allscene/lifetime/publication limits
  remain separate.

### Current Gimmick Dynamics

- F01013 UpdateGimmickMotion is promoted in GimmickDynamics core/native
  adapter, using the owned reaction API. ActorMovementLive keeps its Synced
  scope around the typed call; ModelRenderingLive adopts the same record API.
  Originalunused ticks stayunused and C/module800B0908 is retained. The old
  generated body and its six scratch/child/reaction adaptations are removed;
  no scratchpad or variadic calls remain in maintained dynamics.
- All453MIPS800B0908-1018 match actualBATTLE bytes, SHA256
  `10cd7e3fa21a67223e92e714bf1440b95e56fc5914413ad219077eb3db4480e3`.
  Typed rules preserve delay-first byte countdown, signed rotation division/
  wrapped remainders and angle masks, mode255 damped slide versus254 next-point
  wrapped probe, reaction-specific dispatch, generic signed motion countdown,
  fall/gravity/landing/stack reaction and durability, and sixteen carrier
  records. Final durability break skips child propagation as in the source.
  State publication precedes service calls and fresh values are read afterward.
- Core/native/module19cases262236assertions pass; with reactions35cases262355.
  Every nonzero signed-halfword slide, parent-held countdown, support/occupancy
  callback mutations and stackkind dispatch/refusal are checked. Actual room,
  separate flag/height maps, owned reaction, carrier gravity, durability and
  next-point probes pass natively. Canonical/physics-view offsets, generation/
  captured pointer prefixes and original physical occupancy offset preflight
  ground the adapter. Outside-arena borrowed records/allscene lifetime remain
  separate; native SFX dispatch doesnotclaim audible output with fixture banks.
- FullDebugRelease each5102cases/11,345,744assertions pass with zero failures,
  skips or regressions; canonical2400/900frames stay unchanged. Scoped eleven
  file dynamics/caller/tooling quality is0findings/0new after ten local guard/
  fixture-access repairs without removed assertions/suppressions/baseline
  changes. Copier controls follow the old four reaction calls into their
  final retirement by the owned dynamics host. Two supportedgenerations
  preserve1,758reviewedpaths/rawhashes; manifestSHA256
  `c9d4da0f196602ca8cbda3510132678fa8c1f317c720b37e67791e64788787ba`.
  Wider quality passes109existing/zero new with actualexit0. Independent
  VR-GIMMICK-DYNAMICS/CPP/MOD status are recorded; both ledgers pass zero
  violations. Repository2,759promoted/oneframewip; modernization2,758done/
  986todo. Final two generations preserve all1,758paths/rawhashes; ignored
  verifier removed and disk absence checked separately. Original3,479cohort
  2,477promoted/16retired/985generated/oneframewip is2,493resolved/986remaining,
  forty-four additional since6c8ac0b9.
  Continue orderedrow27 after this gate; controller/reference/allscene and
  publication/lifetime acceptance remain separate.

### Current Gimmick Reactions

- F01014 UpdateGimmickReactionAnimation is promoted in the new
  GimmickReactions core and native adapter. This is F01013 motion's existing
  reaction dependency, not a reordered gameplay priority. The core stays
  legacy-free; its adapter uses the established native build profile.
  Original C/module800B101C is retained. Four generated motion calls are
  typed through registered adaptations; the old variadic casts/body are gone.
- Source236MIPS800B101C-13C8 match actualBATTLE bytes, SHA256
  `d34a69093f27f62d9542bd418290d2b48d05f5651b82b7a974dd6ea93a912d1c`.
  Fourteen logical tables800E91AC-9277 total204bytes, SHA256
  `c5e6885f77e4b5661f5a0eed36273bac6ce554da307ee7a71023aa89c95adc15`.
  Typed rows retain signed spin/tilt/bounce/squash/displacement samples but
  unsigned roll angles. Frames normalize0->1, advance nonterminal cursors,
  and retain terminal cursor while completing. Palette restore/motion12 at
  break9, release at40, orientation callback atroll14 and callback rereads
  preserve original order. Refusal is not C completion.
- Core/native/module16cases119assertions pass for width/bounds/frame/order,
  exact32-byte palette copy and neighboring flags, reused orientation reset,
  real heap-model release and null/short record refusal. Canonical/view
  layout assertions ground the old pointer boundary. Native callbacks retain
  ignored original return statuses while checking record identity/readability;
  released model/slot and surviving record are distinguished. Outside-arena
  borrowed records/global scene lifetime remain separate limits.
- FullDebugRelease each5083cases/11,083,508assertions pass with zero failures,
  skips or regressions; canonical2400/900frames stay unchanged. Scoped
  reaction/native/copier/scaffolder quality is0findings/0new after header-name,
  EOF, guards and explicit signed-extension repairs with all assertions kept.
  All202copier controls pass actualexit0, including four typed generated call
  shapes. Two supportedgenerations preserve1,753reviewedpaths/rawhashes;
  manifestSHA256
  `ded8a3fdedf1c486ff993fc2f125639ef29e60b4c8256e4503c4caf47e7783cc`.
  Wider quality passes109existing/zero new with actualexit0. Independent
  VR-GIMMICK-REACTIONS/CPP/MOD status are recorded; both ledgers pass zero
  violations. Repository2,758promoted/oneframewip; modernization2,757done/
  987todo. Final two generations preserve all1,753paths/rawhashes; ignored
  verifier removed and disk absence checked separately. Original3,479cohort
  2,476promoted/16retired/986generated/oneframewip is2,492resolved/987remaining,
  forty-three additional since6c8ac0b9.
  Continue F01013 motion after this dependency; controller/reference/allscene
  and publication acceptance remain separate.

### Current Gimmick Interaction Commands

- F00914 ApplyGimmickInteractionCommand is promoted in GimmickPlacement.
  The production AnimationPoseLive callback adopts the typed API; original
  void C/module800A41D0 is retained. All177MIPS words800A41D0-4490 match the
  actualBATTLE image, SHA256
  `be298b822504dca2ab5d54a8e849a2af4a5065ff5753a9ab3aee20f351d4fb16`.
- Typed dispatch preserves mode2 kind-specific motion/support reset before
  occupancy removal, mode1 full-width adjacent tile/world/yaw stores before
  F0/F1 parent-bone queries then signed truncating average+60/drop state,
  and other modes' missing/motion-nibble idle fallback or unsigned tile
  reservation release/removal (twice interaction2), wrapped slide/cardinal
  yaw/attachment/carry updates. Direction, object and actor values are
  reread across callbacks; source low-byte selected-slot attachment stays
  distinct from the captured object pointer. No stale whole-record stores.
- Core/native/module14cases131171assertions pass, including all65536yaw
  values, callback mutation/refusal, real separate occupancy+flag maps,
  registered two-tag parent bone data (-1/0 yields60), byte/halfword stores,
  actor alignment and module binding. Captured record/room identities,
  gimmick prefix/known heap actor extents, grid and missing-bone checks add
  refusal safety. Outside-arena borrowed actor storage remains trusted debt;
  missing tagged output now refuses rather than reading uninitialized data.
- FullDebugRelease each5067cases/11,083,389assertions pass with zero failures,
  skips or regressions; canonical2400/900frames stay unchanged. Scoped
  nine-file command/production-callback quality is0findings/0new after seven
  local guard/Catch-size repairs with all assertions retained and no
  baseline/suppression edits. Two supportedgenerations preserve1,748paths
  and rawhashes on both runs; manifestSHA256
  `b28596aac5e5b61590f210255c571bb163d60e51635b3d4cd62d2ab59879a9aa`.
  Wider quality passes109existing/zero new with actualexit0; current command
  behavior was rechecked after reported intervening edits. Independent
  VR-GIMMICK-INTERACTION-COMMAND/CPP/MOD status are recorded; both ledgers pass
  zero violations. Repository2,757promoted/oneframewip; modernization
  2,756done/988todo. Final two generations preserve all1,748paths/rawhashes;
  ignored verifier removed and disk absence checked separately. Original
  3,479cohort2,475promoted/16retired/987generated/oneframewip is
  2,491resolved/988remaining, forty-two additional since6c8ac0b9.
  Continue row27 with F01013 motion (F00915 obstruction is already owned);
  controller/reference/allscene/lifetime/publication remain separate.

### Current Gimmick Interaction Initiation

- F00913 BeginGimmickInteraction is promoted in GimmickPlacement.
  GameplayLoopLive adopts its typed result and ignores it as before; retained
  C/module800A3E6C returns0 after successful dispatch and -1 on refusal. The
  original accepted-path integer result was unspecified and unused, not a
  source-verified zero. Reference remains read-only; generated output changes
  through the copier. Manual edits apply to maintained code and registered
  copier controls.
- Typed stages preserve weapon-pose gate, selected-object orientation and
  publication, signed difference/64 truncation then+1, initial slide clear,
  kinds0-1 material roll, kinds0-2 full-tile slide/kind3 quarter-tile slide,
  sound-before-animation pushes versus animation-before-sound carry/place,
  busy flag and reservation, then turn/recenter. Actors, objects and direction
  are reread after callbacks rather than overwritten from stale snapshots.
  Packed tile byte/spare preservation differs from full-width reservation
  arithmetic; world centers and signed height use original halfword stores.
- Original217instructions800A3E6C-41CC and seven-way table80069460 match
  actualBATTLE bytes; codeSHA256
  `9f4784b3d8c8342b9e846e680f586312c8db1f546f0a1c1c0ce079b71722c876`,
  dispatchSHA256
  `c8f3c45d783e1833f05a2ff5e1befdbfdd2b6edb5665b8b33089a1074f42b234`.
  Original96-byte roll table800E8FD0 has domain0-23 and SHA256
  `76ac98a3d4fa01c9b91ba940043a95b2332a46fd9a3e8ddf6dc6d735067a222d`.
  Oversized3424-byte backing does not expand the24x4logical table.
- Core/native/module12cases391assertions pass, including actual reserved
  flag preservation, nontrivial four-step turn, native roll/refusal and
  callback-mutated actor/object/direction inputs. Captured room/actor/gimmick
  identities/prefix bounds and grid preflight prevent stale writes/fatal
  absent-grid calls. Native audio banks are unbound in these fixtures: sound
  dispatch/order/refusal is checked, not audible output. Original ignored
  animation/sound status remains ignored; no allscene/controller claim.
- FullDebugRelease each5053cases/10,952,218assertions pass with zero failures,
  skips or regressions; canonical2400/900frames remain unchanged. Scoped
  nine-file quality is0findings/0new after twelve local style/test-size
  repairs with every assertion retained and no baseline/suppression changes.
  Two supportedgenerations preserve1,748paths/rawhashes on both runs;
  manifestSHA256
  `f82f3d313b5391810df9482d010e6a2e775e20c8c2d2d491202c3c93594cdbce`.
  Wider quality passes109existing/zero new with actualexit0. Independent
  VR-GIMMICK-INTERACTION-START/CPP/MOD status are recorded; both ledgers pass
  zero violations. Repository2,756promoted/oneframewip; modernization
  2,755done/989todo. Final two generations preserve all1,748paths/rawhashes;
  ignored verifier removed and disk absence checked separately. Original
  3,479cohort2,474promoted/16retired/988generated/oneframewip is
  2,490resolved/989remaining, forty-one additional since6c8ac0b9.
  Continue row27 interaction commands;
  controller/reference/allscene/lifetime and publication remain separate.

### Current Gimmick Interaction Selection

- F00908 FindInteractableGimmickAhead is promoted in GimmickPlacement.
  Its typed host reuses ActorMovement, TopAt and real placement, ceiling and
  actor-occupancy APIs. GameplayLoopLive adopts the typed optional result;
  the generated interaction starter uses the retained C entry. Original
  module address800A38E0 is verified. All185MIPS words800A38E0-3BC0 match
  actual BATTLE bytes, SHA256
  `f8de2877f57e73f362f7dd40ad33f4ec78266df428d21b2d816c11dbbece5446`.
- Original idle gates precede the rounded direction store, which precedes
  adjacent negative-grid refusal. Held objects use attachBone, adjacent
  placement at actorY-128 and ordered actor/adjacent ceilings at actorY-384,
  without the ground branch's above-object occupancy check. Ground objects
  retain top-slot-zero refusal, interaction/mode gates, inclusive signed
  [actorY-128, actorY+64], farther placement for kinds0-3 or two ceilings for
  kinds4-7, then actual scalar occupancy mode1. Full-width adjacent/far tile
  coordinates are not byte-wrapped. Captured room/actor/gimmick identities
  and prefix bounds refuse stale/missing native records.
- Core/native/module11cases131152assertions pass, including every signed
  halfword heading, boundary/order/refusal and real top/floor/occupancy cases.
  All201copier controls pass with actualexit0; existing scalar retirement
  controls remain and inspect both owned hosts. Scoped9-file quality is
  zero findings/zero new after braces/constants and assertion-preserving
  checked-state/coordinate helper repairs; no suppressions or baseline edits.
- FullDebugRelease each5041cases/10,951,827assertions pass with zero failures,
  skips or regressions; canonical2400/900frames remain unchanged. Supported
  two-run generation preserves1,748reviewedpaths/rawhashes, zero changes on
  both runs; current manifest SHA256
  `6a0fb65d66526c907791448d890a6861473ab8a4f1bcacc67f9e3f6550dac29d`.
  Wider quality passes109existing/zero new with actualexit0. Independent
  VR-GIMMICK-INTERACTION-QUERY/CPP/MOD status are recorded; both ledgers pass
  zero violations. Repository2,755promoted/oneframewip; modernization
  2,754done/990todo. Final two generations preserve all1,748paths/rawhashes;
  ignored verifier removed and disk absence checked separately. Original
  3,479cohort2,473promoted/16retired/989generated/oneframewip is
  2,489resolved/990remaining, forty additional since6c8ac0b9.
  Continue row27 with F00913 interaction initiation; actual
  controller/reference/allscene/global-lifetime acceptance remains separate.

### Current Native Unsupported Stack Drop

- F00836 DropUnsupportedGimmickStack is owned/wired in GimmickPlacement
  with a captured native room/gimmick/actor host and original C/module8009F530.
  ActorRosterLive and unchanged existing sequence stack scenarios adopt it.
  Original153MIPS8009F530-F790 match module bytes, SHA256
  `0720959db08fac83ba196924fc8fec0ebdfb9b2ee051b1e8dbb89a6436ddb2e0`.
- Original mark-falling precedes top/below queries; base velocity0/mode252 is
  stored before occupancy removal, twice only for interaction2. Survivors
  clear carrier before conditional low24tile/height/parent/idle/nonfalling
  attachment; pointer aliases of base are skipped. Typed MovementActor is
  reused for actor falls: full root-motion reset/newfall countdown4 and NPC
  destination, preserved existing mode3 motion/countdown, common locomotion/
  movement-pending/support reset and next-slot skip1. Native identity changes
  and cleanup refusal stop writes; actor scan is bounded and progress-checked.
- Core order/carrier/actor/refusal and actual native base/fall/alias/missingtop/
  module checks pass. Existing sequence empty/nonempty scenarios retain every
  assertion; captured host reuses actual owned occupancy and actor movement
  load/store. Combined stack/core/native/sequence/occupancy11cases72assertions
  pass. Existing actor-above retirement controls now inspect the actual owned
  host; original source guard/scalar controls are preserved. All201controls
  pass nativeexit0; no generated output/reference was hand-patched.
- FullDebugRelease each5030cases/10,820,675assertions pass, zero failures/skips/
  regressions and unchanged2400/900-frame canonical fingerprints. Scoped
  core/native/roster/sequence/tests quality passes90existing/zero new after
  guard/include/test-size repairs without removed expectations/suppressions/
  baseline updates. Supported2run generation preserves1,724paths after only
  Gimmicks.cpp/.h/ModulePointers.cpp/manifest first changes, then byte-stable.
  Wider quality passes109existing/zero new with actualexit0; independent
  promotion and VR-GIMMICK-STACK-DROP are recorded. Both ledger checks pass
  zero violations; repository2,754promoted/oneframewip, modernization
  2,753done/991todo. Final two generations preserve1,724paths/raw hashes;
  manifest SHA256`50f57568c243ec0d890dc7f5d85b02f5596934b7df13e401cf9825dd5631fcd4`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,472promoted/16retired/990generated/oneframewip:
  2,488resolved/991remaining, thirty-nine additional since6c8ac0b9. Continue row27
  afterward; actual controller/reference/all-scene/global lifecycle acceptance
  and unrequested publication/closure remain separate.

### Current Actor-Above Alias Retirement

- F00910 FindActorAboveGimmick is retired in the catalog after its two generated
  callers adopt the already-owned LiveFindActorOccupyingTile scalar API, without
  adding a replacement bridge. Original13MIPS800A3C00-3C30 are only LBU X/Z,
  LH support height and unchanged occupancy-mode forwarding; actual byte SHA256
  `d461aa79ce0baa4a8a4584849006854bb9384a823affb3f4b5fd40a6b742dc9b`.
  No stored BATTLE pointer word targets this address; generated function/header
  and original module binding are removed. Existing stack-update/null-top guard
  order and owned mode0/1 occupancy behavior stay intact.
- Existing copier guard fixture now uses the actual source call expression,
  retaining all ordering assertions; new exact scalar-rewrite/emitted-source
  control passes. The retired symbol's original name remains before fixups,
  so the local source-name stage was corrected after fail-closed generation.
  No generated output was hand-patched. All201copier controls pass nativeexit0.
  Native module-absence and unsigned-tile/signed-height/mode0-1-2 checks plus
  placeability10cases185assertions pass; focused copier/test quality is0/0.
- FullDebugRelease each5021cases/10,820,616assertions pass, zero failures/skips/
  regressions and unchanged2400/900-frame canonical fingerprints. Supported
  two-run generation preserves1,724paths after only Gimmicks.cpp/.h/
  ModulePointers.cpp/manifest first-run changes and zero second-run changes.
  Wider quality passes109existing/zero new with actualexit0; retirement and
  VR-GIMMICK-OCCUPANCY-SCALAR are recorded. Both ledger checks pass zero
  violations; repository2,753promoted/oneframewip, modernization2,752done/992todo
  across3,744functions after the retired alias leaves its scope. Final two
  generations preserve1,724paths/raw hashes; manifest SHA256
  `991817bbcd050c6a3157104ce4268d990c220da0287aeb31141c342f3095473d`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,471promoted/16retired/991generated/oneframewip:
  2,487resolved/992remaining, thirty-eight additional since6c8ac0b9. Continue row27
  afterward. Raw records in remaining generated consumers/global/controller/
  reference/all-scene lifecycle remain separate; no unrequested publication/
  closure claim.

### Current Native Gimmick Placeability

- F00912 CanPlaceGimmickAtPoint is owned/wired in GimmickPlacement.cpp/.hpp
  and its captured-generation native host, original C/module entry800A3DB4
  with generated query/motion callers adopted. Original46MIPS800A3DB4-3E68
  match module bytes; SHA256
  `817169198e1ad6d6ddda587b55b553a7568f81c0c2d5042b8b06b7535888752d`.
  Support uses zero origin and full-word X/Z offsets in original mode1, then
  signed minimum-height refusal, only probe bit20 refusal, then occupancy mode1
  at X/Z divided128 with truncation toward zero and sampled height forwarded.
- Core order/bit/fullword signed-coordinate/height boundary tests and actual
  native floor/probe/actor/absent-room/module checks pass. Fixture first lacked
  collision layer descriptors and4x4sample payload, then inherited same-tile
  support gimmicks; both were isolated/completed without changing positive
  placement or refusal expectations. Native fixture restores room, actor
  table, probe/samples and all original gimmick records. Placeability/query
  16cases66,018assertions pass; placeability-only selection8cases180assertions.
- Full Debug/Release each5019cases/10,820,613assertions pass, zero failures/skips/
  regressions and unchanged2400/900-frame canonical fingerprints. Scoped
  placeability/query/core/native/tests quality is0findings/0new. All200copier
  controls pass nativeexit0; supported2run generation preserves1,724paths after
  only Gimmicks.cpp/.h/ModulePointers.cpp/manifest first changes, then byte-stable.
  Wider quality passes109existing/zero new with actualexit0; independent
  promotion and VR-GIMMICK-PLACEABILITY are recorded. Both ledger checks pass
  zero violations; repository2,753promoted/oneframewip, modernization
  2,752done/993todo. Final two generations preserve1,724paths/raw hashes;
  manifest SHA256`13b24b5ca07316b3231fe9e837b68190c3001d4fc764e24ab00fea0b964bb7d9`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,471promoted/15retired/992generated/oneframewip:
  2,486resolved/993remaining, thirty-seven additional since6c8ac0b9. Continue ordered
  row27 afterward. Controller/reference/all-scene/global lifecycle stays
  separate; no unrequested commit/push/closure claim.

### Current Native Gimmick Orientation

- F00854 SetGimmickOrientation is owned/wired in GimmickPlacement.cpp/.hpp
  and the generation-checked native host; original C/module entry800A0104 and
  creation/generated reaction callers are adopted. Original49MIPS800A0104-01C4
  match module bytes, SHA256
  `9476368483ccc2379dc8fc69802de7ade3dd9ef0da5e5d8d98971ba1ec54870e`.
  Logical table is24four-byte rows at800E9030-908F before next symbol800E9090,
  not compatibility3328byte capacity. All96actual bytes match SHA256
  `8463ddc7b38b4bb62b8d8a94ccc2c25a770d408aaff4634495cc194770e0487c`.
- Original kind mask permits0/1; low-byte material stores before full-index
  bounded lookup. Unsigned quarter-turn bytes multiply1024 and wrap to signed
  halfwords before the existing typed Euler kernel; native matrix4 rotation
  changes without translation/padding writes. All original rotation reset
  halfwords are cleared. Known invalid tables/indices refuse with documented
  material-store ordering; borrowed record/global lifecycle remains separate.
- Confirmed defect#330 was logged before fix: original CPU800A01B0 SW clears
  rotationZ+reserved2A, but generated halfword-only store left adjacent19.
  Native failing-before case1/6 onlyreserved2Afail is now all6pass, without
  changing expected values. Core kind/order/unsigned-byte/bounds/store-refusal
  and native matrix/translation/actual unnamed padding/table/module tests pass;
  coupled orientation/creation/status29cases140,961assertions pass. Creation
  retains its original ignored orientation return for non-crate kinds.
- Full Debug/Release each5011cases/10,820,433assertions pass, zero failures/
  skips/regressions and unchanged2400/900-frame canonical fingerprints. Scoped
  orientation/query/creation/tests quality is0findings/0new after local alias/
  brace repairs; no suppression or baseline refresh. All200copier controls
  pass nativeexit0. Supported2run generation preserves1,724paths after only
  Gimmicks.cpp/.h/ModulePointers.cpp/manifest first changes, then byte-stable.
  Wider quality passes109existing/zero new with actualexit0; independent
  promotion and VR-GIMMICK-ORIENTATION are recorded. Both ledger checks pass
  zero violations; repository2,752promoted/oneframewip and modernization
  2,751done/994todo. Final two generations preserve1,724paths/raw hashes;
  manifest SHA256`0cbf546c0a9dae0b70bf99f1e0bd2cbd31e43dad03434d705aa92f95d6487a0b`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,470promoted/15retired/993generated/oneframewip:
  2,485resolved/994remaining, thirty-six additional since6c8ac0b9. #330 remains
  open pending publication/exactCI. Continue ordered row27 afterward; no
  controller/reference/all-scene/global lifetime or unrequested commit/push/
  closure claim.

### Current Native Menu Gimmick Queries

- F02468 MeasureTileStack/F02469 IsGimmickWithinHeightBand are owned/wired
  in GimmickPlacement.cpp/.hpp and its checked native snapshot adapter, with
  original MAINMENU.PRG C/module entries800FA188/800FA238 and generated sphere
  caller adoption. Height-band retains its historically unused status; actual
  37instruction-word body is present in the module, not merely a declaration.
- Missing generated ASM files were resolved by direct read-only module audit:
  original config gives base800F9800. Existing pinned Windows Capstone decodes
  all44/37words from offsets988/A38 to A38/ACC. Both use LBU X/Z and skip/motion
  filters; measurement alone gates signed mode zero and decrements128 unless
  interaction bits equal2, while band uses inclusive signed Y and wrapping
  ADDIU upper bound without a mode/falling/attachment gate. REA Rizin was
  unavailable on Windows; no retry/bootstrap/reference mutation was used.
- Core all65,536height values/256interaction bytes, active gates/span limits and
  native unsigned/mode/band/byte-preservation/unaligned-output/null/short-prefix/
  module cases pass. Menu/query18cases196,979assertions pass. One oversized
  native test was factored into a local helper without removing assertions.
  Native output memcpy preserves safe unaligned/null policy separately from
  original valid-pointer behavior; other borrowed prefixes remain tracked debt.
- Full Debug/Release each5003cases/10,819,367assertions pass, zero failures/skips/
  regressions and unchanged2400/900-frame canonical fingerprints. Scoped quality
  is0findings/0new; all200copier controls pass nativeexit0. Supported two-run
  generation preserves1,724paths after only Sphere.cpp/ModulePointers.cpp/
  Gimmicks.cpp/.h/manifest first-run changes, then zero second-run changes.
  Wider quality passes109existing/zero new with actualexit0; independent
  promotions and VR-GIMMICK-MENU-QUERIES are recorded. Both ledger checks pass
  zero violations; repository2,751promoted/oneframewip and modernization
  2,750done/995todo. Final two generations preserve1,724paths/raw hashes;
  manifest SHA256`1f68f0b0f2a3622d5cb03f2fe12cba9f338138e2c837137358235e1ca3c762da`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,469promoted/15retired/994generated/oneframewip:
  2,484resolved/995remaining, thirty-five additional since6c8ac0b9.
  Continue ordered row27 afterward. No controller/reference/all-scene/global
  lifetime or unrequested publication/closure claim.

### Current Native Gimmick Status

- F00838 GimmickSubtype and F00917 AreGimmicksMoving are owned/wired in
  GimmickPlacement.cpp/.hpp and the checked query snapshot adapter, with
  original C/module entries8009F858/800A47C4. GameplayLoopLive uses the typed
  status API; generated Feedback uses the retained subtype entry. Subtype is
  unsigned LBU after three-bit kind==1; status scans first16present records
  with skipped clear/four-bit motion zero/nonzero signed mode. Falling and
  attachment are not extra status filters. Original16/25MIPS words match bytes.
- All256kind/subtype bytes, all65,536signed mode values and256motion bytes are
  covered; native subtype/skip/nibble/ignored-fields/prefix-refusal/module cases
  pass. Signed one-bit skipped fixture uses representable-1 for the original
  true bit. Status/query18cases270,942assertions pass. No existing assertion or
  baseline was weakened. Native source extents remain the shared checked prefix.
- FullDebugRelease each4995cases/10,753,529assertions pass with zero failures/
  skips/regressions and unchanged2400/900-frame canonical fingerprints. Scoped
  status/query/native/production-loop/tests quality is0findings/0new. All200
  copier controls pass nativeexit0. Supported two-run generation preserves
  1,724paths; first changes only Feedback.cpp/ModulePointers.cpp/Gimmicks.cpp/.h
  and manifest, second byte-stable. Wider quality passes109existing/zero new
  with actualexit0; VR-GIMMICK-STATUS and promotions are recorded. Both ledger
  checks pass zero violations; repository2,749promoted/oneframewip and
  modernization2,748done/997todo. Two final post-status generations preserve
  all1,724paths/raw hashes; manifest SHA256:
  `1ecca73a81030181112450c28825959c4b7ac7c0777cfe13a39c7825abc5ec1a`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,467promoted/15retired/996generated/oneframewip:
  2,482resolved/997remaining, thirty-three additional since6c8ac0b9.
  Continue ordered row27; this verified milestone is not task completion.
  Actual controller/reference/all-scene/global lifetime acceptance stays
  separate. The two menu stack-check ASM filenames were not found by bounded
  ignored-file rg lookup; defer their port until actual instruction evidence
  is recovered. No unrequested commit/push/closure.

### Current Native Gimmick Stack Queries

- F00837 FindGimmickBelow and F00942 FindTopGimmickAtTile are owned/wired
  in GimmickPlacement.cpp/.hpp with checked native query snapshots and original
  C/module entries8009F794/800A91DC. ActorSoundLive consumes detached values
  while preserving its original zero-slot fallback. Generated gimmick/Sphere
  callers adopt the retained C shapes; no generated output was hand-patched.
- Original49/55MIPS words match module bytes. Below keeps the first largest
  eligible Y above-30000 and <=referenceY, including full four-byte tile
  equality; top keeps the last smallest Y <=4096, LBU tile X/Z, optional idle,
  parent255/carrier equality and original skipped filter. Falling is a below
  filter only, not a new top-query filter. Native known short/misaligned prefixes
  refuse; other borrowed prefixes remain tracked compatibility debt.
- Core all65,536halfword heights and boundary/eligibility/tie/slot tests pass;
  native unsigned coordinates, unchanged complete byte snapshots, detached
  values, null/short/alignment/refusal/module tests pass. Query/audio82cases/
  131,817assertions and coupled query/creation/resources/release/occupancy/audio
  116cases132,240assertions pass. The two adapters share the native prefix
  readability helper; clone detection stays intact without suppression.
- Full Debug/Release each4987cases/10,613,728assertions pass with zero
  failures/skips/regressions and unchanged2400/900-frame canonical fingerprints.
  Scoped quality across core/native/shared-guard/audio/tests is0findings/0new;
  wider changed+tidy `--jobs 1` passes109existing/zero new with actualexit0.
  All200copier controls pass nativeexit0; two supported generations preserve
  1,724reviewed paths after only7expected ownership/project/caller changes,
  with zero second-run changes. VR-GIMMICK-STACK-QUERIES records both queries
  and refreshes shared-guard creation evidence; both ledger checks pass zero
  violations. Repository2,747promoted/oneframewip, modernization2,746done/999todo.
  Two final post-status generations preserve1,724paths/raw hashes; manifest
  SHA256`23e0db2c719fb9da8be12bbb7da77ae529d31b53f1f9f180f85534bc2788c755`.
  Owned ignored verifier removed; disk absence checked separately. Exact
  original3,479cohort2,465promoted/15retired/998generated/oneframewip:
  2,480resolved/999remaining, thirty-one additional resolved since6c8ac0b9.
  Continue ordered row27; reaching fewer than1,000remaining is not completion.
  No controller/all-scene/sound/reference/global
  lifetime acceptance or unrequested commit/push/closure is claimed.

### Current Native Gimmick Creation

- F00782 is owned/wired in GimmickResources.cpp/.hpp and the native resource
  adapter, with original C/module entry8009998C and production ActorAssetsLive
  queue adoption. The original248MIPS8009998C-9D68 match actual module bytes;
  SHA256`26d5e62874803ea36582012510a1a0c42c47469fef4ac06ca1a4451f1382679c`.
- Typed creation preserves busy refusal before publication, published null or
  model-failure behavior, clear/shared copy of the native prefix only, first
  shared-ID match, palette/default setup, unsigned tile/variant bytes, wrapped
  height, orientation/occupancy, and the first earlier supporting-stack match.
  The native adapter bounds8logical template indices/3pointers and template
  bytes in a heap allocation or registered module blob; validates model/UV
  spans, uploads CLUT13, and retains the caller-owned instance tail. Known
  short/misaligned destinations refuse before publication; other borrowed
  native prefix pointers remain compatibility debt, not lifetime retirement.
- Real heap model/UV, shared-prefix/tail, palette row edges, template/destination
  bounds, unaligned C request preservation, unsigned placement/variant and
  original module cases pass. Empty-room support is explicitly checked at
  original sentinel-3000 before the variant subtraction/halfword wrap. The
  coupled creation/resources/release/production occupancy selection passes
  34cases423assertions. All existing production occupancy assertions remain.
- Complete Debug/Release each4977cases/10,482,587assertions pass with zero
  failures/skips/regressions and unchanged silver_wolves2400-frame and
  first_lever_room900-frame canonical fingerprints. Scoped quality is0/0;
  wider changed+tidy `--jobs 1` passes109existing/zero new with actualexit0.
  No baseline refresh/suppression. All200copier controls pass nativeexit0;
  their model registration/forget-before-free checks follow the owned adapter.
- Two180s bounded supported generations preserve1,723reviewed paths; only
  Gimmicks.cpp/.h, ModulePointers.cpp and the manifest changed on the first
  ownership generation; the second is byte-stable. F00782 promotion and
  VR-GIMMICK-CREATE are recorded; both ledger checks pass zero violations.
  Repository2,745promoted/oneframewip and modernization2,744done/1,001todo.
  Two final post-status generations preserve all1,723paths and raw hashes;
  manifest SHA256:
  `0a04a65cea55770a9b438502df7fcf1eb2d2285d97154f6381e7e0ae8dce51d9`.
  Owned ignored verifier removed; its disk absence is checked separately.
  Original3,479cohort now2,463promoted/15retired/1,000generated/oneframewip:
  2,478resolved/1,001remaining, twenty-nine additional resolved since6c8ac0b9.
  Continue ordered row27. No unrequested commit/push/closure;
  actual controller/reference/all-scene/import/global lifecycle stay separate.

### Current Native Gimmick Release

- F00783 is owned/wired in GimmickResources.cpp/.hpp and the checked Live
  bridge, with original C/module entry80099D6C and maintained ActorRosterLive
  removal plus generated callers adopted. It is promoted with independent
  native C++/behavior evidence. Original68MIPS80099D6C-9E78 match module bytes;
  SHA256`be96c32225ad821ce6adcea197f3d9969d753ac24cbc5b3a6b16044fd13c8cda`.
- Typed release validates slot/unsigned zero-bone state, searches other live
  same-kind records by original three-bit ID, frees/forgets unshared geometry
  before occupancy removal and final slot clear. Shared geometry remains until
  its final matching owner releases it. Native host checks captured pointer
  and room generation before/after cleanup, retaining caller-owned instance
  storage. Empty/negative/out-of-range/bone-zero inputs refuse unchanged.
  Existing model/heap/AI occupancy services are reused; no new asset format.
- Core cleanup-order/refusal and actual native heap/extent/shared-retention/
  invalid-slot/module cases pass. The existing two-cycle production occupancy
  test was adopted without changing its assertions. Combined release/resource/
  occupancy21cases329assertions pass. Failed fixture patch placement was
  repaired only for the new nested additions; the original fixture destructor
  and all existing state restoration/expectations remain intact.
- Final full Debug/Release each4964cases/10,482,493assertions pass, zero
  failures/skips/regressions and unchanged silver_wolves2400-frame /
  first_lever_room900-frame canonical fingerprints. All200copier controls and
  strict generation pass. Scoped resource/core/actor-removal/tests/occupancy
  quality passes zero findings/zero new after four local alias/brace repairs;
  no suppression or baseline refresh. All1,125original instructions across
  ten routines and original frozen palette halfwords are audited.
- F00783 is promoted with VR-GIMMICK-RELEASE. Wider changed+tidy `--jobs 1`
  passes109existing/zero new with actualexit0; no baseline refresh/suppression.
  Both ledger checks pass zero violations; repository2,744promoted/oneframewip,
  modernization2,743done/1,002todo. Two180s bounded supported copier/scaffold
  generations preserve all1,723reviewed code/Live/test/project/ledger/tool
  paths and raw hashes. Manifest SHA256:
  `4c6d034cb5e00e3fe26952b310afe9a22f3a3bb300cf938aef39b0306cc8e001`.
  Owned ignored verifier removed; disk absence is checked separately.
  Continue F00782 creation in resource owner. Exact original3,479cohort is
  2,462promoted/15retired/1,001generated/oneframewip:2,477resolved/1,002remaining
  (twenty-eight additional since6c8ac0b9). #325-329 publication/exactCI stay
  open. Actual controller/reference/all-scene/model import and whole global
  lifetime retirement remain separate; no unrequested commit/push/closure.

### Current Gimmick Resource Selection And Palettes

- F00780/F00781 are owned/wired in new GimmickResources.cpp/.hpp and its
  checked native bridge, distinct from placement/platform state. Original
  C/module entries80099900/80099960 and the maintained room-palette producer
  adopt it through supported generation. Both are promoted with independent
  VR-GIMMICK-SHARED-MODEL and VR-GIMMICK-PALETTE-TABLE records.
  Existing ActorAssets handles remain the
  asset-loader ownership boundary; no new asset authoring format is introduced.
- Original24MIPS80099900-995C preserve signed LB request-slot exclusion,
  unsigned unmasked request kind, three-bit loaded object identity, original
  sixteen-slot scan/first match and dataAddress-only success mutation. Native
  callbacks snapshot model values and memcpy requests for unaligned inputs;
  absent/no-match requests are unchanged. Original11MIPS80099960-9988 copy
  exactly0xE0palette bytes; core validates/stages a complete source, including
  self/partial overlap, and the native240byte compatibility table retains its
  trailing16bytes. Oversized table/model pointer ownership remains migration
  debt; no model sharing lifecycle/global/palette retirement claim is made.
- All original words match BATTLE image bytes; F00780 SHA256:
  `4b9eda0cb0bc8d7b4d5b6312f6808db8971560445845f1761aa82d4ed34be446`;
  F00781 SHA256:
  `5eae8c68006ca852dccc68cb1fe424d487f869fb7e1376c4088c4d44ae22ec44`.
  Audited total1,057words across nine routines plus the original frozen palette.
  Core/native/room-rendering27cases374assertions pass, covering signed slots,
  masked IDs, scan limits, exact/short/alias palette bytes, request preservation,
  unaligned request, compatibility tail and both original module addresses.
- Final full Debug/Release each4956cases/10,482,460assertions pass with zero
  failures/skips/regressions and unchanged2400/900-frame canonical fingerprints.
  All200copier controls and strict generation pass. Supported project generation
  registers the new files and only its Live bridge as legacy-binding capable;
  no project XML is hand-edited. Focused core/Live/producer/tests quality passes
  zero findings/zero new after two local brace/include fixes. No suppression,
  baseline refresh, controller/reference/all-scene/model lifecycle claim.
- Wider changed+tidy `--jobs 1` passes109existing/zero new (legacy5/tests104)
  with actualexit0; no baseline refresh/suppression. Named kGimmickPaletteBytes
  describes the original size without changing the array/type/data extent.
  Header and focused four-unit quality also pass zero findings/zero new.
  Both ledger checks pass zero violations; repository2,743promoted/oneframewip,
  modernization2,742done/1,003todo. Two180s bounded supported copier/scaffold
  generations preserve all1,723reviewed code/Live/test/project/ledger/tool
  paths and raw hashes. Manifest SHA256:
  `38f0c598e38f06124a8b0353c2766756ebeca1b2532cf787fc43dfaa1adc3ec1`.
  Owned ignored verifier removed; disk absence is checked separately.
  Continue F00782/F00783 original creation/release lifecycle in the resource
  owner. Exact original3,479cohort is2,461promoted/15retired/1,002generated/
  oneframewip:2,476resolved/1,003left (twenty-seven additional since6c8ac0b9).
  No unrequested commit/push/closure or whole-cohort finish.

### Current Platform Target Waypoint

- F00620 is owned/wired in PlatformPlacement.cpp/.hpp with original C/module
  entry8008E2D4 and promoted with VR-PLATFORM-TARGET-WAYPOINT.
  The maintained MovementBehaviorLive AI route-gate consumer now
  uses a detached value rather than a borrowed VECTOR pointer. Original19MIPS
 8008E2D4-E31C match module bytes; SHA256:
  `2dc102677b05115641b2f67ed030839cac3c70bfc6857e1f4b9521bf5f644013`.
  The core masks the selected waypoint to two bits and returns checked index
  and exact value; the C boundary still returns the original record address
  after validating complete aligned platform data. Hidden authored waypoints
  remain accessible; absent/partial/invalid indices refuse. Borrowed C/module
  compatibility and room lifetime retirement remain separate.
- Core2cases770assertions and native pointer/value/invalid/module cases pass;
  initial coupled navigation50cases795,772assertions passed. Full Debug then
  aborted in PursuitTargetsTests.cpp140 because its existing PursuitFixture
  bound four platform records without binding/restoring their byte extent.
  The incomplete report at `build/quality/native-tests-debug.xml` is failed
  diagnostic evidence, not fresh passing verification. The gap was logged on
  #276 before repair; all original pursuit expectations are retained. Added
  sizeof(platforms) and saved/restored the previous extent in that fixture;
  production bounds checks were not weakened. The confirmed owned diagnostic
  VS.Tests process40236 was stopped after it retained the executable lock;
  unrelated processes were left alone. Corrected pursuit/waypoint14cases /
  854assertions pass, including current/planned and group0/group3 gates.
- Final complete Debug/Release each4944cases/10,482,174assertions pass with
  zero failures/skips/regressions and unchanged2400/900-frame canonical replay
  fingerprints. All200copier controls and strict generation pass. Scoped
  core/Live/AI/tests/pursuit quality0findings/0new passes. All1,022original
  instruction words across seven platform functions and frozen16palette
  halfwords are audited; current new generation stability is not yet claimed.
- Wider changed+tidy `--jobs 1` passes109existing/zero new with actualexit0;
  no baseline refresh or suppression. Both ledgers pass zero violations;
  repository2,741promoted/oneframewip, modernization2,740done/1,005todo.
  Two180s bounded supported copier/scaffold generations preserve all1,718
  reviewed code/Live/test/project/ledger/tool paths and raw hashes. Manifest:
  `b9ea460df95ab5290b51ab425a94d234e01c9412e5d4b11c02207b8bd029f66c`.
  Owned ignored verifier removed; disk absence is checked separately.
  Exact original3,479cohort is2,459promoted/15retired/1,004generated/oneframewip:
  2,474resolved/1,005left (twenty-five additional resolved since6c8ac0b9).
  Continue row27 gimmick resources/lifecycle. No unrequested commit/push/closure
  or whole-cohort completion; wider original acceptance remains unchanged.

### Current Platform Freeze Appearance

- F00629 is owned/wired in PlatformPlacement.cpp/.hpp with checked C/module
  entry8008E4DC and native freeze/motion hosts; original generated body removed
  through the supported copier. It is promoted with VR-PLATFORM-APPEARANCE.
  Original69MIPS8008E4DC-E5EC match every module word;
  SHA256`7ec25ae3ed7ab496dc81cc15098c49607ae2ae0e985b64b17ce88b061ea139b5`.
  Original16halfword palette at80068BFC matches native values;
  SHA256`99cbd5852e331084d118db806b27bd81fb94728f615eabeac38905c8e344bd86`.
  Across six platform routines all1,003instruction words are audited.
- Typed appearance state preserves full-request equality against the unsigned
  tint byte, nonzero palette choice, ordered flagged-group red/green swaps,
  optional nonnegative effect toggle and the final byte reread/zero-test flip.
  Equality returns before all room/palette/environment access. Native records
  are complete/aligned; environment field extent and generation/pointer/length
  are checked. Existing typed LoadLiveClutRow handles uploads and refusals;
  source palette ownership/extent remains a borrowed legacy boundary, not a
  newly retired native resource lifetime. Missing restoration palette refuses.
- Four core cases541assertions and five actual native palette/group/refusal
  cases22assertions pass, plus original module resolution. Native fixtures
  preserve packed records and restore complete palette animator/room state;
  exact palette words and real geometry red/green/blue bytes load/swap/restore.
  Coupled appearance/freeze/motion/shadow44cases721,674assertions pass. The
  optional effect call/order has core-host evidence; no actual controller/
  all-scene/reference/palette screenshot or global lifetime retirement claim.
- Final complete Debug/Release each4938cases/10,481,391assertions pass with
  zero fail/skip/regressions and unchanged silver_wolves2400-frame and
  first_lever_room900-frame canonical fingerprints. All200copier controls and
  strict generation pass. Scoped core/Live/tests quality0findings/0new passes.
  The fixed palette is an immutable constexpr factory with local kPalette;
  that resolves cpplint's namespace initializer parsing and tidy's data-literal
  rules without suppressions or baseline refresh. A failed automatic patch
  relocation inserted include/data into PlatformRecords; only those owned
  strays were repaired and all compilation/behavior checks rerun successfully.
- Wider changed+tidy `--jobs 1` passes109existing/zero new (legacy5/tests104)
  with actual child exit0; no suppression or baseline refresh. Both ledger
  checks pass zero violations; repository2,740promoted/oneframewip and
  modernization2,739done/1,006todo. Two180s bounded supported copier/scaffold
  generations preserve all1,718reviewed code/Live/test/project/ledger/tool
  paths and raw hashes. Manifest SHA256:
  `33486156190cc3793908c276dc62a32a1e91ebc2e1e135f8601de7e5056d439f`.
  Owned ignored verifier removed; disk absence is checked separately.
  Continue remaining ordered row27 gimmicks. Exact original3,479cohort is
  2,458promoted/15retired/1,005generated/oneframewip:2,473resolved/1,006left
  (twenty-four additional resolved since6c8ac0b9). #325-329 remain
  open for publication/exactCI. No unrequested commit/push/closure.

### Current Native Platform Shadows

- F00596 is owned and wired in PlatformPlacement.cpp/.hpp with the maintained
  RoomRenderingLive scene caller and original C/module entry8008CEB0. It is
  promoted with VR-PLATFORM-SHADOW and VR-SHADOW-PACKET-CAPACITY.
  Original260MIPS8008CEB0-D2BC were read directly; every word matches the
  original BATTLE module. Span SHA256:
  `e02ae8d73c3dcb7ae0208a875448c35e1f9bd1bc06e631e84dff0912e844fe68`.
- Typed shadow geometry preserves visible/group/pit-surface filtering,
  signed15bit floor decoding for all65,536halfwords, truncation toward zero,
  altitude-based fade clamp/radius/shade, wrapped corner halfwords and exclusive
  depth4/2048 gates. The Live adapter uses the existing room camera/GTE and
  bounded LivePacketQueue plus subtractive texture-page service. A tilted
  native frame fixture verifies nonzero projected area, exact gray quad command,
  draw-mode chain, hidden suppression, room/record preservation and full-buffer
  refusal. It restores native frame and room state. This is not a screenshot,
  actual controller/reference/visual/all-scene or global lifetime claim.
- #329 was logged before fixing a confirmed dirty-worktree partial-emission
  defect: with only six words left the quad emitted before its two-word draw
  mode was dropped. The failure-before cursor assertion is retained. Added
  reusable non-mutating LivePacketQueue::CanEmit and preflight both packets;
  now one refusal is counted and cursor, all primitive bytes and all ordering
  tags remain unchanged. Its dedicated case1/4assertions passes; coupled
  shadows/shared texture-packets36cases/131,290assertions passed before the
  final two preservation assertions. Existing full/hidden/sufficient-capacity
  and nondegenerate/shade regressions remain. Issue329 stays open for scoped
  publication/exactCI; no retail defect or observed blending error is claimed.
- Final full Debug and Release each pass4,928cases/10,480,827assertions,
  zero failures/skips/regressions and unchanged canonical2400/900-frame
  fingerprints. All200copier controls and strict generation pass. Only the
  obsolete F00596 polygon-pointer cast adaptation was removed; remaining
  gimmick adaptations were preserved. Focused shadow/core/Live/scene/tests/
  shared packet units pass zero findings/zero new after eight local style
  repairs and formatting, without suppression or baseline refresh.
- Full changed+tidy `--jobs 1` passes109existing/zero new (legacy5/tests104),
  actual child exit0. The public four-corner extent is named with unchanged
  type/layout; header and focused five-unit checks also pass0findings/0new.
  No baseline refresh or suppression. Both ledger checks pass zero violations;
  repository2,739promoted/oneframewip, modernization2,738done/1,007todo. All934
  original words across five platform routines match module bytes. Two180s
  bounded supported copier/scaffold generations preserve all1,718reviewed
  code/Live/test/project/ledger/tool paths and raw hashes. Manifest SHA256:
  `1448e37148b9f37d8dcce24318a0455670358a8376f893f04a613ea1d1e22feb`.
  Owned ignored verifier removed; disk absence is checked separately. Continue
  F00629 freeze appearance and remaining ordered row27 gimmicks.
- Exact original3,479cohort is2,457promoted/15retired/1,006generated/oneframewip:
  2,472resolved/1,007remaining (twenty-three additional resolved since6c8ac0b9).
  Work remains uncommitted/unpushed;
  #325-329 publication/exactCI gates stay open. The complete-cohort objective
  remains unfinished and wider owner requirements are unchanged.

### Current Native Platform Motion

- F00595 is owned and wired in PlatformPlacement.cpp/.hpp with the production
  GameplayLoopLive caller and original C/module entry8008C8A8. It is promoted
  with VR-PLATFORM-MOTION; independent C++/behavior and lifetime gates remain.
  Original386MIPS8008C8A8-CEAC were read directly and every instruction word
  matches the actual BATTLE module. Span SHA256:
  `f90d3485bd0346b1611d1d6fb6d06cf0a859b36ec54ea9045ee43d50263def33`.
- Typed frame/state hosts preserve initial room/platform pause gates; signed
  freeze halfword subtraction and flash/clear order; each substep's full
  half-speed bounce increment and byte clamp/wrap; candidate position publish
  before actor obstruction; callback-selected segment re-read for rollback;
  unsigned motion halfword wrap, signed duration comparisons, exact arrival,
  docking and endpoint/segment wraps. The loop re-reads tick speed after its
  callback. Render_frameBuf is an integer: the flash condition is its value
  being nonzero, not framebuffer-pointer existence. Hidden/detached-loop
  geometry behavior and original signed bounce-height updates are retained.
- Live records are complete/aligned/bounded and checked against captured room
  generation plus pointer/extent after callbacks. Waypoint projection/storage
  is shared with the already-promoted reset adapter. Native fixtures restore
  room, actor/model arrays, tick speed, pause/freeze/tint/frame-buffer and
  bounce state; packed record and unrelated geometry bytes remain unchanged.
  Fourteen motion cases cover native updates/freeze/table/module and core
  callback/arithmetic/order boundaries. Coupled platform selection38cases /
  263,042assertions passes. No actual controller/visual/all-scene/reference or
  global lifetime retirement claim is made by these minimal actor-free cases.
- Final Debug and Release complete builds/regression gates each pass4,917cases
  /10,349,683assertions, zero failures/skips/regressions and unchanged canonical
  silver_wolves2400-frame/first_lever_room900-frame fingerprints. All200copier
  controls and strict generation pass. Only two obsolete F00595 return-edit
  hunks were removed from cpp_adaptations.json; remaining shadow/gimmick cast
  records stay intact. Focused core/Live/gameplay caller/tests quality passes
  zero findings/zero new after nine local style repairs, without suppression
  or baseline refresh.
- Full changed+tidy `--jobs 1` passed with actual child exit0 and95existing/
  zero new (legacy5/tests90). Captured output is at
  `build/quality/platform-quality.stdout.log`; no baseline refresh. Both ledger
  checks pass zero violations; repository2,738promoted/oneframewip and
  modernization2,737done/1,008todo. Two180-second-bounded supported generations
  preserve all1,718reviewed code/Live/test/project/ledger/tool paths and hashes.
  Manifest SHA256:
  `36d823836c2430ae56aefd387a5d0ee6d80d91fc2e4f35f8bc98851ca6e0e632`.
  The owned ignored verifier is removed; disk absence is checked separately.
- Exact original3,479cohort is2,456promoted/15retired/1,007generated/oneframewip:
  2,471resolved/1,008remaining (twenty-two new resolved since6c8ac0b9).
  Continue ordered #276 row27 with shadows/gimmicks. Work is uncommitted/
  unpushed; #325-328 scoped publication/exactCI gates remain open. F00629,
  not F00626, is the remaining legacy freeze-appearance service; F00626 is
  already-promoted audio ambience. Preserve wider owner requirements.

### Current Platform Initialization, Waypoints And Contacts

- F00593/F00594/F00597 are promoted in PlatformPlacement.cpp/.hpp,
  with checked native adapters and original C/module entries at8008C538,
  8008C6B4 and8008D2C0. Original95/125/68MIPS
  listings were reviewed directly. Initialization resets pause/freeze/tint and
  the original four bounce slots before geometry placement and terminal
  entrance reset. Waypoints preserve masked traversal and exact positions.
  Collection preserves visible file order, source indices, bounce and wrapped
  halfword coordinates; maintained targeting/collision/navigation-mask callers
  now consume typed values. The one Movement.cpp cast-adaptation record was
  renamed with its existing explicit cast intact; strict copier generation
  succeeds and all200copier controls pass.
- A coupled collision regression exposed that collection must not inherit the
  four-slot control limit: the existing five-platform native collision case is
  preserved. The typed collector is bounded by complete aligned records and
  existing native bounce storage; the legacy C output entry refuses more than
  four contacts before writing. A separate fixture verifies that refusal and
  unchanged output. Oversized compatibility storage is not promoted as a new
  logical control capacity or a retired global lifetime boundary. Initialization
  positions all bounded room records, not just four; the fifth-platform and
  highest encoded entrance-layer/sixth-platform fixtures pass. Only the four
  original bounce slots reset, and entrance waypoint reset runs last.
- Fresh complete Debug and Release builds and regression gates each pass
  4,903cases /10,349,627assertions, zero failures/skips/regressions and unchanged
  silver_wolves2400-frame /first_lever_room900-frame canonical fingerprints.
  Final platform/control selection passes33cases /852,896assertions. The native
  initialization fixture now proves the original four bounce bytes reset from
  non7 values while the fifth compatibility byte remains19. Existing coupled
  collision/targeting/navigation selection passed122cases /1,060,210assertions;
  no unmatched filter is counted. Earlier waypoint-only results are historical.
- The earlier combined quality run finished with six new findings. All three
  missing vector includes and bool-to-char/unsigned-byte-to-int conversions
  are repaired explicitly. Captured combined changed+tidy `--jobs 1` reports
  95existing/zero new (legacy5/tests90), with empty stderr and normal final
  report. Its return path is zero when no new findings exist. The final
  initialization boundary correction then changed only core/Live/tests units;
  those exact three units pass zero findings/zero new with clang-tidy. No
  suppression or baseline refresh was used. Both ledger checks pass zero
  violations; repository2,737promoted/oneframewip and modernization2,736done/
  1,009todo. Behavior records are VR-PLATFORM-INITIALIZATION/WAYPOINT/COLLECTION.
- The bounded preservation helper successfully checks all288original
  instruction words against the actual
  BATTLE.PRG bytes at image base80068800. F00593/F00594/F00597 span SHA256s are
  `123d3159e521788c9fd9d18c41fc0d25d58797806c771adc47450c58f5129c7d`,
  `bab98b522cc25debab3ece33d0449dd4f388a953b661ab66969a34a5dadf2d5d` and
  `ee53ee290e4b445b332d5b28818252e1ce89e6ee111605c81fc7e4be4fc7bddb`.
  Two180-second-bounded supported generations preserve all1,718reviewed
  code/Live/test/project/ledger/tool paths and raw hashes. Manifest SHA256:
  `75980e393ba4e59e7ed4a8deff24c9bbe019ea89517846f65ee69db68ae232a3`.
  The owned ignored scratch verifier is removed and Test-Path returnsFalse.
- The exact original3,479cohort is2,455promoted/15retired/1,008generated/
  oneframewip:2,470resolved/1,009remaining, twenty-one additional resolved
  entries over published6c8ac0b9. Work
  remains uncommitted/unpushed; #325-328 publication/exactCI gates remain open.
  Continue ordered #276 row27 with platform motion/shadows and remaining
  gimmicks. The
  owner's complete-cohort objective is unfinished; wider requirements remain.

### Current Platform Control Milestone

- F00591/F00604/F00605/F00621 are promoted in PlatformPlacement.cpp/.hpp with
  checked native C/module entries at8008C49C/8008D594/8008D5A0/8008E320.
  F00592's unused clear-association forwarding alias is retired after no
  module/caller binding remains. Maintained actor, battle, camera, mode,
  cutscene and VM callers and remaining generated calls adopt the new entries.
- Original31/3/23/20MIPS verify control7bits then bounce reset only for full
  nonnegative input while freeze0; pause SB lowbyte, not normalized boolean;
  full-input appearance request before SHduration (65536enables/stores0);
  waiting-bit LHUdwell and maxword sentinel. Native views check record alignment,
  whole184byte extent and supported four-platform capacity. Original packed
  fields outside control bits and other bounce bytes remain unchanged.
- Exhaustive control/pause/dwell halfword inputs,32bit freeze edges, actual
  packed-record preservation, invalid index/span and module cases pass. Coupled
  platform/actor/cutscene/VM/state selection258cases /728,468assertions passes.
  Complete Debug/Release builds and regression gates each pass4,879cases /
  10,086,641assertions, zero failures/skips/regressions and unchanged canonical
  silver_wolves2,400-frame /first_lever_room900-frame fingerprints. All200copier
  controls pass. Final expanded changed+tidy `--jobs 1` scope reports95existing /
  zero new (legacy5/tests90), with no baseline refresh; its long-run terminal
  completion is now verified, not pending or inferred from prior console history.
- Both ledgers have zero violations; repository2,734promoted /oneframewip,
  modernization2,733done /1,012todo. Exact original cohort3,479 is2,452promoted /
  15retired /1,011generated /oneframewip:2,467resolved /1,012remaining, eighteen
  additional resolved entries since published6c8ac0b9. No complete1,000 claim.
- Two supported generations preserve all1,719reviewed code/Live/test/project/
  ledger/tool paths and hashes. Manifest SHA256:
  `f028361ebd2ec1c526b3fd73b36ed2a851bfc3995d1e824111682f9a80445f5f`.
  Temporary verifier removed and absence checked. The legacy freeze-appearance
  service and original data/global lifetime remain tracked migration boundaries;
  no palette/geometry/controller/all-scene parity claim is made by these tests.
  Continue ordered #276 row27 with platform waypoint/reset/movement and remaining
  gimmick functions. Work remains uncommitted/unpushed over6c8ac0b9, with #325-328
  scoped publication/exactCI gates still open; wider requirements are unchanged.

### Current Active-Trigger Retirement

- F00624's borrowed active-trigger table getter is retired after its sole
  maintained TargetingLive consumer adopts generation-tagged RoomTriggerInfo
  values from InteractionTargetLive. Original28MIPS8008E400-E46C verify the
  unsigned flags20/40 suppression and file-order table behavior. No module/
  caller binding remains; retirement does not count as another C++ promotion.
- The shared trap table resolver now refuses partial/misaligned/absent records,
  so both snapshots and returned identities are bounded by complete native
  table extents. Original signed kind/X/Z values are retained; stale generation
  identities refuse. Active/target/trap/facing selection20cases115assertions passes.
- Full final Debug/Release builds and supported regression checks each pass
  4,870cases /9,496,733assertions, zero failures/skips/regressions, unchanged
  canonical silver_wolves2,400-frame /first_lever_room900-frame fingerprints.
  All199copier controls and final changed+tidy `--jobs 1` quality five existing /
  zero new pass; both ledgers have zero violations. Repository2,730promoted /
  oneframewip; modernization2,729done /1,017todo.
- Exact original cohort3,479 has2,448promoted /14retired /1,016generated /
  oneframewip:2,462resolved /1,017remaining, thirteen additional resolved entries
  since published6c8ac0b9. No complete1,000-function claim is made.
- Two supported generations preserve all1,718reviewed paths/hashes. Manifest:
  `762ec5cc9ff197f9b40faf8053b5c0875508b17c368299b7e404270ec5d55b18`.
  Temporary verifier removed and absence checked. Continue ordered #276 row27
  with platform/gimmick control, retaining all wider original flag/global/data/
  controller/reference and lifetime acceptance requirements. Current work stays
  uncommitted/unpushed over6c8ac0b9 and #325-328 publication/exactCI gates remain.

### Current Unlock-Effect Milestone

- F00067 is promoted in ChestInteractions as a typed door/chest unlock consumer;
  its checked native input retains module8006CE70 and generated effect-timeline
  callers adopt it. The obsolete F00623 borrowed room-door getter is retired
  only after this consumer moved to RoomObjectTarget/ChestTarget resolvers and
  no module/caller binding remains. Doors.cpp and Interactions.cpp retain their
  required global data, not unreviewed function bodies.
- Original110MIPS8006CE70-D024 verify target tags10/20/100 and low4index,
  unsigned door lock/signed chest lock eligibility2..3, chest opened-flag signed
  halfword wrap before risk, clear before sounds3F/40 and failure-label position.
  The typed core covers all65,536flag values. Native fixtures restore complete
  room/character/labels/flags/random state and exercise both chest tags, unknown/
  null no-ops, invalid indices without record/risk access, opened-gate ordering,
  unaligned/input preservation and original module resolution. Label coordinates
  are copied synchronously by the existing label routine; no stack pointer is
  retained by the new adapter. Actual sound/controller/all-scene parity is separate.
- Latest full Debug/Release builds and supported regression gates each pass
  4,867cases /9,496,720assertions, zero failures/skips/regressions and unchanged
  canonical silver_wolves2,400-frame /first_lever_room900-frame fingerprints.
  All198copier controls and final changed+tidy `--jobs 1` quality five existing /
  zero new pass. Both ledgers have zero violations and derive2,730repository
  promotions /oneframewip; modernization2,729done /1,018todo.
- Exact original cohort3,479 is now2,448promoted /13retired /1,017generated /
  oneframewip:2,461resolved /1,018remaining, twelve additional resolved entries
  since published6c8ac0b9. No complete1,000-function claim is made.
- Two supported generations preserve all1,718reviewed Native/generated/Live/
  test/project/ledger/tool paths and hashes. Latest manifest SHA256:
  `161dda0281e0a60574bf85e6a9fa25e59820422b90c6c68d889d1558dd0d3427`.
  Temporary verifier removed and absence checked. Work remains uncommitted/
  unpushed over6c8ac0b9; #325-328 stay open pending scoped publication/exact CI.
  Continue ordered #276 row27 with remaining gimmick/platform functions.
  All original global/data/queue lifetime, compatibility retirement and broader
  controller/reference gameplay requirements remain unchanged.

### Current Door Lifecycle Milestone

- F00581/F00582/F00583/F00586 are promoted in the existing typed RoomSwitches
  owner: collision XOR5, busy/sigil/eight-tick begin, sound/effect/closing-collision/
  linked start and effect-wait/primary-linked open-clear completion. Original
 82/35/74/62MIPS ranges at8008BD74-BEB8,8008BEBC-BF44,8008BF48-C06C and
 8008C1C8-C2BC were read in full; all original C/module addresses are retained
  through checked native entries. Maintained room-object and queue consumers
  and all generated callers are adopted through the supported copier.
- #328's actual full-queue test initially reported success and latched busy1/
  delay8 despite refusal. The deciding native core now refuses before stores/
  effects and remains retryable. All65,536key halfwords are covered; late
  admission failure restores original busy/delay. Existing valid-data ordering
  and saturated next-tick /unsaturated same-tick continuation assertions remain.
- The queue now has RoomObjectTarget payloads alongside existing typed chest
  payloads, with room generations and job serials retained across capture,
  full-queue handoff and reset. The shared room-object resolver now rejects
  partial/misaligned tables before returning a record. The post-admission
  extent40to39 discriminator failed before and passes after this owning fix;
  stale or invalidated jobs retire without changing door countdowns.
- Final complete Debug/Release builds and supported regression gates each pass
  4,857cases /9,365,598assertions, zero failures/skips/regressions and unchanged
  canonical silver_wolves2,400-frame /first_lever_room900-frame fingerprints.
  All197copier controls and final changed+tidy `--jobs 1` quality five existing /
  zero new pass. Both ledgers have zero violations and derive2,729repository
  promotions /oneframewip; modernization2,728done /1,020todo.
- Exact original cohort3,479 now has2,447promoted /12retired /1,019generated /
  oneframewip:2,459resolved /1,020remaining, ten additional resolved entries
  since published6c8ac0b9. This is measured source progress, not a complete1,000
  claim or gameplay completion. Actual sound/effect/controller/reference parity
  is not claimed from minimal fixtures that isolate state with unbound effect255.
- Two supported generations preserve all1,718reviewed Native/generated/Live/
  test/project/ledger/tool paths and hashes. Manifest SHA256:
  `2cf9ea289e90bac66116e719516229447d5eb1b1a172a59ad972f5578e19dd66`.
  Temporary verifier removed and absence checked. Current work remains dirty
  over6c8ac0b9, uncommitted/unpushed; #325/#326/#327/#328 stay open pending scoped
  publication and exact-revision CI. Continue ordered #276 row27 with the
  remaining room-door getter/data consumers and gimmick/platform routines.
  All wider global/data/queue lifetime and compatibility retirement requirements
  remain unchanged; earlier sections below preserve prior local milestones.

### Current Door and Sigil Tables

- F00580/F00633/F00634 now have typed RoomSwitches cores over bounded placements,
  queries, endpoint pairs and byte flags. Live adapters expose room-object
  identities and preserve the original C/module contracts at8008BC04,
  8008E6DC and8008E7BC; maintained facing and generated MenuMap/door callers
  are adopted. F00632's numeric coordinate accessor is retired after both
  consumers moved to typed pairs and no registered module/caller binding remains.
- Original92/56/52MIPS words verify the first-record activation guard, signed
  coordinates/kinds/layers/ranges/scene halves, four companion tiles, first-pair
  selection, unsigned endpoint/unlock bytes and exact marker-byte1 store.
  #327's native extent discriminator failed before and passes after limiting
  Live range views to the eight original boundary words800E849C-800E84AB.
  G00036's54short/108byte generated compatibility storage contains adjacent pair
  bytes starting800E84AC; its declaration/provenance remains unchanged and is
  separately recorded migration debt, not a54-group domain.
- Coupled door/sigil/facing selection17cases /1,148assertions passes. Complete
  Debug/Release builds and supported regression checks each pass4,836cases /
  9,037,712assertions, zero failures/skips/regressions and unchanged canonical
  silver_wolves2,400-frame /first_lever_room900-frame fingerprints. All197copier
  controls and final changed+tidy `--jobs 1` quality five existing /zero new pass.
- Both ledgers have zero violations:2,725repository-promoted /oneframewip.
  Exact original cohort3,479 now has2,443promoted /12retired /1,023generated /
  oneframewip:2,455resolved and1,024remaining. Six additional cohort entries are
  resolved since the published6c8ac0b9 checkpoint; no complete1,000 claim.
- Two supported generations preserve all1,718reviewed Native/generated/Live/
  test/project/ledger/tool paths and hashes. Manifest SHA256:
  `5330d4a4c287bf26936302a290f9fa3c7b2a3199ff1510f23bd3040ba5851c21`.
  Temporary verifier removed and absence confirmed. Current work is uncommitted
  over6c8ac0b9; #325/#326/#327 remain open pending scoped publication and exact CI.
- Continue ordered #276 row27 with door collision and begin/start/finish animation
  logic, then the other remaining gimmick routines. Existing controller/all-scene/
  reference, persistent/global/data lifetime and compatibility retirement gates
  remain unchanged. Earlier sections below preserve their original local milestones.

### Verified Entrance and Fall Continuation

- The owner authorized the pending repairs on2026-11-08. Current source remains
  `6c8ac0b92cdfafb19c5f9bf0be2095ac2ab58e26` plus uncommitted changes.
  Nonfatal CheckAshleyFall reloads use the original room route; scripted
  destinations retain room/zone selection. Both scripted entry coordinates
  again retain their original five-bit widths, backed by actual ANDI instructions
  at80074FEC/80075020 and the room-transition JAL at80075148.
- The formerly failing route discriminator passes. Fall/tile selection:
  19 cases /2,143 assertions. Entrance/scene selection:15 cases /132,280
  assertions, including complete record/clear/order/refusal checks and a valid
  scene with index128 mapping to unsigned254. All256coordinate-byte inputs and
  all65,536entered-door halfwords are covered without weakening expectations.
- Complete Debug and Release solution builds and native suites pass:
  each4,823cases /9,036,584assertions, zero failures/skips. Supported
  `tools/test/regression.py check --config <config>` in both configurations
  confirms unchanged silver_wolves2,400-frame and first_lever_room900-frame
  fingerprints and zero regressions. Current changed+tidy `--jobs 1` reports
  five existing findings and zero new; all196copier controls pass.
- All four removed verification records are renewed with fresh limited evidence,
  including the unchanged324endpoint initialization policy. F00610/F00611 now
  pass promotion/modernization review with `VR-SCENE-DOOR-TABLES`; the separate
  gates derive2,722promoted /oneframewip and zero violations. The modernization
  tracker has2,721done /1,028todo. No complete1,000-function claim is made.
- Supported copier/solution generation passes twice with all1,718reviewed
  generated/maintained Native/Live/test/project/ledger/tool paths and hashes
  unchanged. Manifest SHA256:
  `82a17b02baf080e7c1a28744f276abed482b3ac12b56334e4a912105186a8b3e`.
  The temporary bounded verification script was removed and absence checked.
- #325/#326 have complete local acceptance evidence recorded, but remain open
  pending scoped fixing publication and exact-revision CI. No commit/push or
  issue closure is claimed. Continue ordered #276 row27 with the remaining
  door-table/sigil/animation logic; controller/all-scene/reference and global
  lifetime retirement remain separate, unchanged requirements.

### Earlier Blocked Investigation (#326; Historical)

- Current source is `6c8ac0b92cdfafb19c5f9bf0be2095ac2ab58e26` plus
  uncommitted changes. Its published CI42462163420 is successful, but does
  not verify this dirty worktree. Existing local edits removed the published
  entrance core and tests, removed verification records, and changed the
  Ashley nonfatal reload route before this continuation. Those edits were
  not reverted.
- The missing entrance API is now owned by Transitions.cpp/.hpp instead of
  ChestInteractions. F00552/F00577/F00578 ownership and caller contracts
  follow that move. The original local chest-core deletions remain intact.
- F00610/F00611 now have bounded typed scene-table implementations in
  MinimapScene, checked MinimapSceneLive C entries, retained original module
  addresses and adopted maintained/generated callers. They remain `wip`,
  not completed promotions. Debug scene-door/minimap selection passes
  18 cases / 147,029 assertions. Coupled field/minimap/room selection passed
  429 cases / 416,771 assertions before the new failing discriminator below.
- Original scene-door listings cover 50 MIPS words at8008D880-8008D944 and
  55 at8008D948-8008DA20: identifier LBU, entered-door SH and marker-byte
  LBU/OR0x10/SB. Ghidra's configured backend was unavailable; no Ghidra
  analysis or original-scene controller/reference parity is claimed.
- #325 records the generator marking defect. T00542's unchanged notes are
  now properly CSV-quoted. Marking preflights all three tables before writes
  and preserves modernized owned copied/compiles flags. Two focused marking
  controls pass; supported generation/marking completes with all842type rows
  present. Full control rerun, hash-stable second generation and fixing-revision
  CI remain required before closure.
- #326 is a confirmed source-contract counterexample, not a controller-observed
  gameplay failure. Original BATTLE.PRG byte offset0xC948 / CPU80075148 contains
  `38-F1-01-0C`, JAL8007C4E0 (room transition). The existing local CheckAshleyFall
  instead selects zone route when entrance zone10 differs from host zone9.
  New existing-fixture `[fall-reload-route]` compiles and runs: one case,
  four assertions, three pass and route1!=0 fails. The deciding production
  branch was not reverted; explicit owner direction is needed for that change.
- Reproduction from the repository root:
  `MSBuild VagrantStory.Native.sln /m:1 /nologo /v:quiet /p:Configuration=Debug /p:Platform=x64 /t:VS_Tests`,
  then `build/Debug/VS.Tests.exe "[fall-reload-route]" --reporter compact`.
  Next: resolve #326 in the deciding core, retain the discriminator, renew
  missing coverage with fresh evidence, run Debug/Release and required quality,
  generation and replay gates, then continue ordered #276 row27.
- Current synchronized gates derive2716promoted/sevenwip and four invalid
  behavior-evidence claims (F00158/F00552/F00577/F00578). No promotion count
  increase, complete1,000-function claim, Release/full-suite/quality/replay
  success, issue closure, commit or push is claimed for this continuation.
  Older checkpoint notes below are historical, not current verification.

### Scripted Fall Door (#323)

- Registered copier initialization defines every field of F00158's local
  transition door before its existing assigned-field stores. The native consumer
  copies unlock, id and reserved fields that were previously indeterminate.
  This is a defined native zero-initialization policy, not inferred retail data.
- The actual generated producer passes same-zone and cross-zone cases: 14
  assertions verify pit/destination decoding, return0, transition1/2 and the
  complete pending-door value. The fixture uses a bounded 256 KiB scoped heap,
  sizes saved actor tables from their actual declarations (42 battle entries,
  17 actor records), and releases only its own pending read/allocation.
- Current complete Debug/Release each pass 4,789 tests / 8,902,162 assertions,
  zero failures/skips/regressions and unchanged canonical 2,400/900-frame
  fingerprints. Debug coupled40cases/367assertions, copier192/extent9 and
  quality five existing / zero new pass. Both generations preserve all1,735
  paths/hashes; manifest
  `58DED5105801030C0B1A1E44AA5D2EDA43195F3224071B84AB539CEB546DF727`.
- `VR-SCRIPTED-FALL-DOOR-INITIALIZATION` records only this safety contract.
  The minimal fixture's unsupported room3 produces an expected loader refusal;
  successful destination loading and controller fatal-fall/reference parity
  are not claimed. F00158 remains generated and unpromoted. #323 remains open
  pending scoped publication and exact fixing-revision CI; #276/#321 and
  remaining Ashley/global compatibility/lifetime requirements remain open.

### Non-Player Tile Triggers (#276 Row 27)

- F00159 uses the existing strict typed interaction core with value placement/collision,
  typed trap identity and a checked actor/room/event Live host. ActorCommandsLive
  now passes typed placement directly; the native trap begin factory introduces
  no trap-to-lock record alias. Original C/module800751B8 remains a separate boundary.
  Core/Live ownership is in ChestInteractions, preserving the completed trap
  slice's files and avoiding overlapping promotion-plan ownership.
- Original144MIPS words `800751B8-800753F4` verify collision-before-transition
  refusal, low24tile/cache-mode checks, forced-layer lookup, every trap-suppression
  bit, event-owned trap hold, trap-before-pit order, held-location/mode stores,
  signed17pit threshold, HP-zero before defeat and signed-halfword event-result==1
  producing original distinct results0/1/2/3. Native guards refuse absent actors,
  stats, targets and invalid input instead of dereferencing them.
- Eleven focused cases / 54 assertions include actual original packed cache-byte
  writes, caller-input preservation, repeat-entry/missing-actor and original module
  resolution with restored live actor/record/room fixtures. Release coupled50cases /
  416assertions pass. Full Debug/Release4,788tests / 8,902,148assertions each, zero
  failures/skips/regressions; both canonical2400/900-frame fingerprints unchanged.
  Fixtures are not controller combat, an original-scene event/fatal-fall replay or
  all-scene reference parity; those broader requirements are not narrowed.
- Copier191 / extent9/modernization44/editor pass; quality five existing / zero new.
  After relocation, both rebuilt configurations and the recorded coupled selection
  pass the totals above. Two supported generations preserve all1,735paths/hashes
  including owned source/fixtures, generated projects/manifest/ownership/adaptations,
  root solution and build properties; manifest
  `6563B67DCC66521C9426A98D6E7DB4C3D3BB2182E7F19A62FF63C057538CD397`.
  `VR-ACTOR-TILE-TRIGGERS` and four-to-zero old code-consumer slice track only this
  source promotion overd25d8d69. Unchanged gates derive2,716promoted/oneframewip,
  zero violations; row27 has55generated functions. Published NPC revision
  `4d918907` has exact successful CI `42445155609`, recorded on276.
  Continue ordered row27 work.
- The preceding three-query milestone is publishedd25d8d69 with exact successful
  CI42440898460. #276/#321 and required C/module/current-room/actor/event/trap globals,
  raw asynchronous door jobs and broader lifetime/retirement gates remain open.

### Facing Queries (#276 Row 27)

- F00153/F00154/F00155 use the published typed adjacent tile, strict typed
  event/resource hosts and generation-checked room-object/chest targets.
  Gameplay caches those identities instead of raw lock/chest record pointers;
  compatibility C entries resolve records only at their original boundary.
- Original22/38/52 MIPS words at `80074860-800748B4`, `800748B8-8007494C`
  and `80074950-80074A1C` confirm mode4/source0/context-kind1 event arguments,
  lookup-before-floor order including missing locks, low15 floor sign extension,
  unsigned height+63<127 lock window, chest-facing/flag/floor order, signed-halfword
  collection-flag argument wrap and exact floor+96 chest height.
- Twelve focused cases / 66 assertions cover those boundaries and three original
  module entries, stale/out-of-range targets and actual native event12 dispatch,
  direct-event busy refusal and mismatch. Release coupled selection passes
  65 cases / 262,722 assertions. Full Debug/Release each pass 4,777 tests /
  8,902,094 assertions, zero failures/skips/regressions; canonical2400/900-frame
  fingerprints remain unchanged. Native event fixture is not a sourced scene replay.
- Original MAP124 matching chest runs: Debug `1794105683130125-0`, Release
  `1794106004055339-0`:486mode9frames, lock0/key465count10, cancel-returnfield1phase0.
  Original MAP247 lock: Debug `1794105771270134-0`, Release `1794106026205314-0`:
  138mode11frames/key458count10/field1phase0. Opposite-facing MAP124: Debug
  `1794105801206528-0`, Release `1794106052208753-0`:facing2,9,0, no chest/lock
  interaction, original lock1, field1. All exact traces are intact with zero engine
  errors. Commands reuse original private fixtures and production controller input;
  no HP/event/record edits or complete loot acceptance (#321).
- Copier191 / extent9 and editor checks pass; quality five existing / zero new.
  Two supported generations preserve all1,733path hashes, including owned source,
  projects, manifest/ownership/adaptations; manifest
  `52EEE34CF9157A47BC8364DABFDD4D83FD653EEAC569FA163ECB50DBBAD4FC8A`.
  Removed query bodies exposed an incidental generated absent-cell include;
  concrete compile errors were logged on276 before registered helper/control repair.
- `VR-FACING-QUERIES` and four-to-zero old code-consumer slice record this bounded
  promotion over770c8fa2. Unchanged gates derive2,715promoted/oneframewip with zero
  violations; row27 still has56generated functions. Exact publication/CI evidence
  is recorded on276 before issue completion. Required C/module/current-room/global
  and raw asynchronous door-job retirement remain open, as do #276/#321 and broader
  scene/reference parity. Continue row27; this checkpoint is not a stopping point.

### Adjacent Facing Tile (#276 Row 27)

- F00152 uses existing typed actor placement with `FacingTile`/`CardinalFacing`:
  bias0200, mask0FFF, shift10, then one byte-wrapping tile step. Original50MIPS
  words `80074798-8007485C` verify those operations and writes only0/1/2.
  Native C output retains X/layer/Z and untouched fourth byte; null refuses.
  Module `80074798` remains a required native binding.
- Four focused cases / 262,154 assertions exhaust all65,536heading patterns,
  byte-boundary wraps, output layout/null refusal and actual module resolution.
  Coupled21cases / 262,252assertions; complete Debug/Release4,765 / 8,902,028
  each, zero failures/skips/regressions and unchanged canonical fingerprints.
- Current original MAP124 chest Debug `1794103443974395-0`, Release
  `1794103493591254-0`: facing2,7,0, lock0/key465count10/cancel-returnfield1.
  MAP247 lock Debug `1794103512962024-0`, Release `1794103543599636-0`:
  facing3,15,0,138waitframes,key458count10/field1. No complete loot acceptance.
- Copier191 / extent9 controls and editor/quality five existing / zero new
  pass. Two measured generations preserve1,733path hashes; manifest
  `86E7636D1480C35C50822CE287C10FE55B9BCE4AE78E9C7B696B8F451753611D`.
  Three larger generated query callers adopt via copier; maintained
  PlayerObservation's old call was found by compile and migrated explicitly.
- `VR-FACING-TILE-CORE` and four-to-zero old code-consumer slice record the
  bounded leaf promotion. Current gates2,712promoted/oneframewip, zero
  violations; per-function tracker is reconciled. Published at770c8fa2 with exact
  successful CI42438146863; larger-query continuation is recorded above.
  Larger queries/remaining row27/#276/#321 and required C/module/output/global
  compatibility remain open; source promotion is not all-scene reference parity.

### Interaction Camera Waits (#276 Row 27)

- F00139/F00140/F00142/F00146 are strict typed focus/readiness/return-camera
  cores. Live uses checked current target identity and typed actor placement;
  the old short-array actor-query alias is removed. Original C/module entries
  `80074120`, `8007418C`, `8007424C`, `80074374` remain required bindings.
- Original instruction ranges confirm unsigned halfword copies, negative90
  adjustment followed by halfword store, camera-before-actor wait gate and
  camera-before-transition-pose order. Six focused / 28 assertions and coupled
  28 / 183 pass in Debug/Release; final full 4,761 / 8,639,874 each, zero
  failures/skips/regressions; both canonical fingerprints unchanged.
- Current original MAP124 chest unlock/cancel/return runs Debug
  `1794101435674508-0`, Release `1794101499012482-0`: 486 mode9 frames,
  lock0/key465count10, finalfield1phase0. This is not complete loot claiming.
  MAP015 trap runs Debug `1794101217528954-0`, Release `1794101518481790-0`
  activate the trigger, spend132frames in mode8 and return tofield1phase0.
- Trap damage is not run-stable under the fixture's setup seed: identical
  current fixed-counter Debug diagnostics ended HP195 and190, both with the
  same mode transitions. Encounter setup seeds shuffle/interface while combat
  calls its real RNG. These observations do not prove a camera regression or
  deterministic damage parity; original math/order tests and unchanged
  canonical fingerprints remain the regression gates. No baseline was changed.
- Copier191 / extent9 controls, editor diagnostics and quality five existing /
  zero new pass. Two measured generations preserve1,733path hashes; manifest
  `42B717F74E2AA51C48FBE5FD4EBD17B185F7AE25D1595772B957F77B028F3349`.
  `VR-INTERACTION-CAMERA-WAITS` and four-to-zero old code-consumer slice record
  this bounded source promotion. Current gates2,711promoted/oneframewip,
  zero violations; modernized function tracker is reconciled, not stale.
- #322 is closed at525d182e with exact green42433562058. This four-row camera
  slice is published at3d995b77 with exact green42436301250. Remaining row27/#276/#321, required C/module/
  camera globals and broader compatibility retirement stay open.

### Trap Action Initialization (#322, #276 Row 27)

- Latest published audit `6e17239902b0767893aefa6bc231a94f7c308c5d` has exact
  successful Source Quality `42385331538`; its ordinary-lock implementation
  base `16730c33` has exact successful CI `42380807184`. #287 is closed with its own fixing
  revision/evidence; the wait and ordinary-lock milestones below are verified.
  #321, remaining row27/#276 and global compatibility acceptance stay open.
- Original F00135 `Field_TriggerActorTrap` declared an uninitialized local
  `Effects_ActionEffectRequest`. Original 64 MIPS words
  `80073D30-80073E2C` write the request tag5 at `sp+0x14`, target tag4 at
  `sp+0x20`, actor at `sp+0x24`, partFD at `sp+0x26`, but no source position
  payload at `sp+0x18/+0x1A/+0x1C/+0x1E`. Resolver `80085B10` runs before
  secondary-effect entry `800CF3F8`.
- This was logged before fixing as #322: native secondary spawning copies the
  complete request; the transform adapter extracts endpoint positions; non4
  endpoints resolve as positions. Type5 must not be called an actor. The native
  correction represents an absent source explicitly and value-initializes the
  C-shaped request; zero compatibility storage is a defined correction to
  formerly undefined fields, not an inference of original source coordinates.
- PartFD is resolved separately: actual original 39 words `800A1720-800A17B8`
  and the skeleton consumer select bone0 for ground-anchor253, then replace
  matrix translationY with the actor's placed Y. It is not an all-limbs label.
- The typed producer validates actor/action state and real floor target, stages
  original target/limb/range/chain/queue fields, resolves the action, reobserves
  kind after that callback, then submits the initialized effect. The Live
  dispatch no longer manufactures a lock record for a floor trap. Required
  native C/module `80073D30` and generated effect runtime remain migration debt.
- The native `_pFiles` table uses `P(006)` / `P(007)` / `P(008)` macros that
  resolve the initial package filenames through their disc layout extents;
  ID-to-filename equality is verified there, not inferred from ID labels.
  The bounded `[trap-source-audit]` content check reads those actual files via
  `DataStore`, uses the maintained package/transform parsers, and compares all
  decoded transform states under two different source points. All three initial
  packages ignore that point during transform initialization. This is not an
  audit of every trap kind, selected script path or source-impact callback.
  Their compiled LBA mappings and sector-rounded read extents also match the
  extracted file metadata; logical file lengths are not the rounded extents.
  `[trap-source-audit]` passes one case / 36 assertions in Debug and Release;
  complete suites pass 4,746 / 8,636,067 with zero failures/skips/regressions
  and unchanged canonical fingerprints. Audit quality is eight existing /
  zero new; no baseline or gate was relaxed. Package parsing keeps reads under
  65,536 bytes and checks that every whole definition fits the 64-slot audit.
- The extended bounded inventory uses the maintained room decoders and actual
  declared resource spans. Short all-zero empty resources are counted explicitly
  instead of reading the following section as room lengths. All original trap
  kinds0-15 are present and all mapped transform definitions ignore source
  point during initialization. This is not exhaustive source-impact/script or
  original undefined-coordinate parity; the native policy above stays explicit.
- Original MAP015 kind3 at tile15,9 is reached by actual Right input from14,9.
  Debug `1794098965381322-0`, Release `1794099097920197-0`: mode8 starts128,
  lasts132frames, HP250 becomes196, triggeractivation1, final mode1/phase0;
  zero engine-error/trace-failure records. Private fixture/captures remain ignored.
- Current complete Debug/Release builds and full checks pass 4,755 tests /
  8,639,846 assertions each with zero failures/skips/regressions and unchanged
  2,400/900-frame canonical fingerprints. Trap cases eight / 59; combined
  trap/package selection ten / 3,815. Copier191 / extent9 controls and 44
  modernization controls pass. Two measured generations preserve1,733 Native/
  manifest/ownership/adaptation hashes; manifest
  `B31446B01752F6B0A04938F5C143169F6947BCA389526469C6376A22E74C9B36`.
  Quality eight existing / zero new; no baseline or gate was relaxed.
- Stale summary, row27 publication notes and per-function modernization records
  are reconciled. Plan gate lists now require every referenced record, including
  rejection of missing/empty members; F01416 is tracked in its existing row52.
  `VR-TRAP-ACTION-INITIALIZATION` and its four-to-zero code-consumer slice support
  source promotion. Current gates derive2,707 promoted / one framewip with zero
  violations. Published fixing slice is `525d182e`, exact successful CI
  `42433562058`; #322 is closed. Remaining row27/#276/#321 and global retirement stay open.

### Ordinary Room-Object Locks (#276 Row 27)

- F00134 now uses the existing strict interaction core with a typed snapshot/
  result/host. Live accepts only a present room lock-record address and rechecks
  room generation/index before access. No chest or floor-record alias is read.
- Held keys unlock without consumption; held sigils set flag value two before
  consumption; prior nonzero flags open silently. Busy records return without
  item reads; rejection retains notification, sounds `0x65/0x66`, actor0 idle.
  The existing raw asynchronous door-toggle producer is unchanged migration
  debt, not a new native ownership destination or a general pinning guarantee.
- Original 141 MIPS words `80073AFC-80073D2C` confirm open/triggered/item/lock
  accesses `+0xE/+0x12/+0xC/+8`, threshold `0x1D2`, setter value two and call
  order. Required native C/module `80073AFC` remains; supported copier adoption
  retires the old body/prototype. Its nine obsolete function-local `template`
  keyword adaptations were removed after strict matching failed; other
  adaptation records and matching requirements were preserved.
- Complete Debug/Release 4,745 / 8,636,031, zero failures/skips/regressions and
  canonical fingerprints unchanged. Eight focused cases / 33 assertions;
  coupled 25 / 136. Quality five existing / zero new; 191 copier / nine extent
  controls, Pylance syntax and editor diagnostics pass. Two measured generations
  preserve 1,733 Native/manifest/ownership/adaptation hashes. Manifest:
  `1D1F9D30FFF8465E1B35C93D8D476CD4C0882291C65333CF5C12587B02A02D56`.
- Current original MAP247 key458 runs: Debug `1794059543651132-0`, Release
  `1794059562842773-0`; each has 138 wait-mode11 frames then field1/phase0 and
  reusable count10. Original MAP013 Chamomile Sigil466 runs: Debug
  `1794059161959422-0`, Release `1794059468038790-0`; count10 becomes9, the
  visible unlock notification is confirmed once at600, then field1/phase0.
  Private captures/fixtures remain in existing ignored diagnostic directories.
- `VR-ROOM-OBJECT-INTERACTION` records the bounded source promotion. This is
  representative controller evidence, not a fresh all31 original-lock matrix.
  Required C/module/raw jobs, other row27 functions, #321 and global retirement
  stay open. Published implementation is `16730c33`, exact successful Source
  Quality `42380807184`; the test evidence above is that bounded checkpoint.

### Object Interaction Wait (#276 Row 27)

- #287 is closed at `47efa7c71d0f886ade12ac9e426bb2f9172e923d`, exact green
  Source Quality `42374393222`. #321 remains open independently.
- F00132/F00133 now use the existing strict interaction core and narrow Live
  adapter: set typed mode11 before pausing; release notification, resume field
  presentation, then enter exploration. Existing field services are reused.
  Original 22 MIPS words `80073AA4-80073AF8` confirm the mode store in the
  pause-call delay slot and the three finish JALs in that order.
- Actual native C bindings/module addresses `80073AA4` / `80073ACC` remain.
  Supported registration/copier adoption removes original bodies/prototypes
  and updates gameplay/module callers, not manual generated source edits.
- Three focused cases / seven assertions and coupled 37 / 210 pass in both
  configurations. Complete Debug/Release 4,737 / 8,635,998, zero failures/skips/
  regressions; both canonical replay fingerprints remain unchanged. Quality
  five existing / zero new; 191 copier / nine extent controls, Pylance syntax
  and editor diagnostics pass. Both measured generations preserve 1,732 path
  hashes; manifest `2C81051A67022099A2E0207FE5E552509D109834672A44AC07736242341B5E46`.
- Current original MAP247 controller runs, `120:Up,180:Cross`, enter mode11 at
  tick180 for 138 frames each, then field1/phase0 with key458 at ten. Evidence:
  Debug `1794057094760040-0`, Release `1794057118286130-0` under their existing
  `combat-lab/runs` directories. Private fixtures/captures are not committed.
- `VR-OBJECT-INTERACTION-WAIT` records this bounded behavior. Old code-symbol
  consumers reduce four to zero; required C/module/global services, remaining
  row27 functions and #321 are not retired or completed by this slice.
  Published pair is `a0f53f60`, exact successful Source Quality `42376304098`.

### Typed Interaction Targets (#287, #276 Row 27)

- Published target slice is `47efa7c7`, exact successful Source Quality
  `42374393222`; #287 is closed with that revision and the evidence below.
  #320 is closed at `fff7ebcabfd956fe59be474374f5914bf8b08aa6` with exact green
  Source Quality `42363616728`; older pending-publication notes below describe
  their earlier checkpoints, not current issue status.
- Eight functions F00136/F00137/F00138/F00141/F00144/F00145/F00147/F00148
  now have strict typed chest/trap selection, dispatch, finish and contents
  cores. `InteractionTarget` distinguishes floor traps from actual treasure
  chests by index and room generation. Live adapters read each record's own
  fields; gameplay chest casts and the shared lock-view focus pointer are gone.
- Chest animation payloads carry `ChestTarget`, leaving the legacy queue's
  trigger pointer null. Room release/unload and successful section publication
  invalidate target identity and clear actor observation. Stale jobs retire
  without reading the replacement chest. This is not general room pinning.
- Original BATTLE.PRG MIPS halfword accesses and JAL consumers were read at
  `80073E30`, `80073F7C`, `80074050`, `800741D4`, `800742A4`, `80074314`,
  `800743E0`, `800744B8`: chest tile X/Z, lock/item/flag and inline loot map
  to `+0/+2`, `+0xC/+0xE/+0x10`, `+0x14`; trap kind remains `+6`.
  No exhaustive original-executable or malformed-field parity is claimed.
- Complete Debug/Release solutions build. Both final regression checks pass
  4,734 tests / 8,635,991 assertions, zero failures/skips/regressions; both
  2,400-frame `silver_wolves` and 900-frame `first_lever_room` fingerprints
  remain unchanged. Focused `[interaction-target],[chest-interaction-core]`
  passes 14 cases / 96 assertions in each configuration.
- `quality-native.cmd --changed --tidy`: six existing / zero new; format,
  cpplint, Ruff and clones zero. Copier 191 / extent nine controls pass. Two
  measured supported copier/project generations preserve all 1,732 Native,
  manifest and ownership path hashes. Manifest SHA256:
  `4365720AD549354629E76E8D295D7FADEE79AA1BBCEEA04FDB5793F5982E2913`.
- Real controller controls use original MAP124 chest and MAP247 lock fixtures,
  seed one, `all-items-b`, unchanged isolated room-event policy and solo Ashley.
  MAP124 `120:Down,180:Cross,600:Cross` unlocks, preserves Chest Key465 at ten
  and cancels the ready loot menu back to field mode one / phase zero.
  Opposite-facing and `all-items-a` missing-key controls refuse unlocking.
  MAP247 `120:Up,180:Cross` faces `[3,15,0]`, preserves key458 at ten and
  visibly opens the doorway; opposite facing is a separate control.
- Local evidence: Debug chest cancellation `1794053656316376-0`, chest
  opposite-facing `1794053076238422-0`, lock `1794053965353507-0` under
  `build/Debug/combat-lab/runs`; Release chest cancellation
  `1794054453730507-0`, lock `1794054473047855-0`, missing key
  `1794054521272535-0` under `build/Release/combat-lab/runs`.
  Private fixtures/captures live under `build/owner-checkpoint/interaction-*`;
  none are committed or uploaded.
- #321 separately records an active but invisible chest loot panel in Debug
  run `1794053759826123-0`; Square transfers no loot there. Capacity and cause
  remain unresolved. Cancellation is not successful loot acceptance, and this
  source-promotion milestone does not close #321 or complete all chest gameplay.
- Unchanged gates derive 2,703 promoted / one separate frame `wip`, 244 behavior
  records and zero violations. `VR-INTERACTION-TARGETS` and its four-to-zero
  historical code-consumer slice retain required native C/module entries,
  actor-query observation and raw lock/door queue jobs as compatibility debt.
  The fixing revision and exact successful CI are recorded on #287/#276.
  Continue remaining row 27 before later owning rows and #217.

### Switch-Start Admission Transaction (#320, #276 Row 27)

- #320 logged before fixing: source `be6f0560` plus only the reproduction calls
  original F00606 against a full queue. Count eight is preserved but marker
  becomes one and activation seven becomes twenty without an admitted job;
  two of three assertions fail and the marker prevents retries. Original 23
  MIPS words `8008D5FC-8008D654` confirm stores before queue admission.
- Actual strict typed `BeginSwitchToggle` now preserves/reverts prior marker/
  countdown on refused delayed admission, while accepted marker1/delay20/kind2,
  busy suppression and immediate nonzero switch toggling remain original.
  Native Live Boolean admission is explicitly C++-linked when its C header is
  included under `extern C`; required C/module `8008D5FC` ABI remains.
- Original count/marker/countdown assertions pass unchanged. Six focused cases
  / 39 assertions cover retry after capacity is freed, admitted job/busy state,
  signed immediate values, invalid/store refusal and actual module resolution.
  Live immediate tests isolate refused effect/cell/audio inputs, not physical
  switch/chest gameplay. Raw target/room lifetime and #287 remain open.
- Complete Debug/Release 4,720 / 8,635,895, zero failures/skips; both canonical
  replays unchanged. 191 copier / nine extent controls, editor diagnostics and
  quality five existing / zero new pass; no baseline/gate was weakened.
  Logs: `build/owner-checkpoint/switch-start-admission-*-regression.log`.
- Source `be6f0560dc306b4ccee2396346fb649685249fbe` has exact green reset CI
  `42361923743`. Current #320 fixing source/closure are pending publication/
  new exact CI; #318/#319 remain closed with their own verified gates. Row 27,
  #287 and global compatibility/physical acceptance remain independently open.
- Final repeat copier/project generation preserves tracked/new Native/project/
  manifest bytes. Unchanged gates derive 2,695 promoted / one separate frame
  `wip`, 243 behavior records and zero violations; old switch-start consumers
  are zero while required C/module/raw target lifetime stays tracked.

### Native Queue Reset (#276 Row 27)

- F00570 now targets strict native reset core: pending count, marker ticks,
  then all eight slots clear in the original order. Live increments private job
  serials for each clear, so supported reset/reuse invalidates captured jobs.
  This guards queued work, not raw pointee/room lifetime; #287 remains open.
- Supported ownership removed original reset implementation; native C/header/
  original module `8008B6B4`, loading/state callers and tests adopt the native
  reset entry. Original 14 MIPS words `8008B6B4-8008B6E8` confirm both counter
  clears before the 64-byte original queue zeroing. Native no longer assumes
  original pointer stride/layout for slot reset.
- Source `e3514aa0` plus current slice: complete Debug/Release 4,714 / 8,635,856,
  zero failures/skips and both canonical replays unchanged. Sixteen focused
  cases / 1,658 assertions include #318/#319; reset-only three / 26 cover typed
  order/repeat, actual C/module/filled queue/count/ticks and callback-reset
  host model. Model callback evidence is not an instrumented physical callback
  or Combat Lab chest/lock parity claim.
- 191 copier / nine extent controls and editor diagnostics pass; scoped quality
  five existing / zero new. Required native C/module/raw target/update ABI and
  overall row 27 / #287 remain; no baseline/gate or physical acceptance relaxed.
  Logs: `build/owner-checkpoint/gimmick-queue-reset-*-regression.log`.
- #319 closed with `e3514aa03a181e3ff09e94f531cea7e4c4514aef` and exact green
  CI `42360550780`; #318 remains closed at its own fixing revision/CI. Current
  reset source is pending publication/exact CI; continue typed targets and
  owning interaction/door/gimmick paths rather than treating this as row done.
- Final repeat copier/project generation preserves tracked/new Native/project/
  manifest bytes. Unchanged gates derive 2,694 promoted / one separate frame
  `wip`, 242 behavior records and zero violations; old reset consumers are zero.
  Required native C/module/raw target/room lifetime obligations stay tracked.

### Saturated Door Continuation (#319, #276 Row 27)

- #319 logged before fixing: source `ba227890` plus only the new regression
  fills eight delayed door jobs, starts callbacks before old slots clear, and
  loses the first finish continuation. Pending seven versus eight and missing
  first target are two failures in ten assertions. Original 66 MIPS words
  `8008C0C0-8008C1C4` plus callback tail `8008C040-8008C06C` confirm that order.
- F00585 now targets strict native `UpdateGimmickAnimationQueue`. It preserves
  original signed-switch/unsigned-door byte countdown behavior and regular
  unsaturated callback order. Only a full completed delayed-door slot retires
  before its callback; all eight finish jobs survive and drain next tick.
- Private Live job serials and captured target checks prevent post-callback
  cleanup from clearing an admitted replacement job. They guard queue reuse,
  not pointee/room lifetime: raw target/update ABI and #287 remain tracked.
  Required native C/header/original `8008C0C0` and actual room-rendering caller
  are adopted explicitly; no generated output was manually patched.
- Original pending/target assertions pass unchanged. Thirteen focused cases /
  1,632 assertions include #318; seven dispatcher/continuation cases / 1,467
  cover all 512 byte/kind combinations, callback replacement, saturated and
  unsaturated timing, module/refusal and next-tick draining. Native Live tests
  isolate refused cell/effect/audio inputs, not physical or Combat Lab parity.
- Complete final Debug/Release 4,711 / 8,635,830, zero failures/skips and both
  canonical replays unchanged. 191 copier / nine extent controls, editor
  diagnostics and scoped quality five existing / zero new pass. Per-byte
  scenario helper extraction retained all samples/assertions; no baseline or
  gate was weakened. Logs: `build/owner-checkpoint/gimmick-door-continuation-*-regression.log`.
- #318 is closed with `ba2278900eef22249aac683fc51d7aaa6fa6fd6a` and exact
  successful CI `42358122654`; #319 fixing source/closure are pending new
  publication/exact CI. Row 27 / #287 and global ABI/physical acceptance remain
  open; this does not relabel the original saturated bug as exact parity.
- Final repeat copier/project generation preserves tracked/new Native/project/
  manifest bytes. Unchanged gates derive 2,693 promoted / one separate frame
  `wip`, 241 behavior records and zero violations; old dispatcher consumers are
  zero. Raw target/room lifetime and native C/module/update obligations remain.

### Gimmick Queue Admission Accounting (#318, #276 Row 27)

- #318 was logged before fixing: original/current F00584 increments pending at
  `8008C08C` before searching the eight slots. Original 20 MIPS words
  `8008C070-8008C0BC` confirm the count-before-refusal order. On source
  `5fb63815` plus only the new regression, a ninth request leaves eight slots
  intact but pending nine: one case / 18 assertions, 17 pass and `9 == 8` fails.
- Actual strict `VS::Field::QueueGimmickAnimation` validates typed kind/target,
  selects the first free bounded slot and publishes occupancy count only after
  the store commits. Full/refused/invalid requests cannot write slots/count.
  Required native C/header/original `8008C070` and existing raw queue/update
  target ABI remain in the Live host; alignment is not lifetime validation.
  #287 and complete typed queue/interaction ownership remain open.
- Same original capacity/slot assertions now pass unchanged. Six focused cases
  / 165 assertions also cover invalid kinds/null/misalignment, repeated full
  refusals, store refusal, module resolution, admitted eight switch jobs
  draining to zero and supported-kind resets. Completion isolates refused
  effect/cell/audio paths, not physical chest/lock Combat Lab acceptance.
- Complete final Debug/Release 4,704 / 8,634,363, zero failures/skips; both
  canonical replays unchanged. 191 copier / nine extent controls and editor
  diagnostics pass; quality five existing / zero new. One oversized scenario
  test was split into local helpers without assertion/baseline weakening.
- Source `5fb63815` has exact green CI `42355536122`. Current fixing source and
  #318 closure are pending publication/exact CI; row 27 / #287 remain open.
  Logs: `build/owner-checkpoint/gimmick-queue-accounting-*-regression.log`.
- Supported ownership was recorded before output filtering interrupted the
  producer; supported copy generation was completed instead of repeating
  ownership. No reference/generated manual patch, asset upload or baseline
  refresh was used. Original rejected-request behavior is deliberately fixed,
  not relabeled as exact original parity or general gameplay completion.
- Final repeat copier/project generation preserves tracked and new Native/
  project/manifest bytes. Unchanged gates derive 2,692 promoted / one separate
  frame `wip`, 240 behavior records and zero violations; old admission symbol
  consumers are zero, while existing raw queue/target/update ABI stays tracked.

### Native Room Switch Core (#276 Row 27)

- F00607 now targets strict `VS::Field::ToggleRoomSwitch` over a typed host/state
  interface. Sound selection, effect dispatch, post-callback state reread,
  marker/switch normalization and collision passability order are native core
  rules; Live alone touches original records, audio/effect/collision bindings.
- Supported `own.py F00607 --to Native\Game\src\Field\RoomSwitchesLive.cpp`
  first removed the generated implementation. Ownership now points at actual
  core/public header; exact native C/header/module mappings preserve original
  `8008D658`. Only this Live adapter gets the existing legacy-include policy;
  the core stays strict and legacy-free, with no warning/gate relaxation.
- Source `67740e21` plus current slice: complete Debug/Release 4,698 / 8,634,198,
  zero failures/skips, both canonical replays unchanged. Focused switch/gimmick
  12 / 759 (switch-only four / 36); original 46 MIPS words `8008D658-8008D70C`
  confirm signed loads, callback order and masked passability stores. Actual
  native entry/module/signed state tests isolate refusal paths for effect/cell/
  audio inputs; they are not physical or Combat Lab chest/lock acceptance.
- 191 copier / nine extent controls and editor diagnostics pass; changed
  quality five existing / zero new. Original signed short/char conversions are
  explicit only at the Live boundary. Required `Field_ToggleRoomSwitchEntry`
  ABI/old-record interface remains; the old symbol's three Native definition/
  header/module consumer files retire, not all Gimmicks.h or global EC state.
- Previous snapshot source `67740e21` has exact green CI `42353262369`.
  Current F00607 source is pending publication/exact CI; row 27 / #287 remain
  open. Their queue/typed chest/trap target, producers/readers and reset paths
  still require completion. Logs: `build/owner-checkpoint/room-switches-core-*-regression.log`.
- Recompilation exposes existing generated Gimmicks return mismatches,
  ActionAnimation/Ai Movement shifts, ActionAnimation/Magic uninitialized locals,
  Sphere pointer-width casts, Map VLA and SequenceTests bit-field warnings.
  No suppression, unrelated fix or baseline refresh was used.
- Final repeat copier/project generation preserves tracked and new Native/
  project/manifest bytes; unchanged gates derive 2,691 promoted / one separate
  frame `wip`, 239 behavior records and zero violations. F00607's old symbol has
  zero current consumers; required native C/record/module ABI remains tracked.

### Native Gimmick Placement Core (#276 Row 27)

- F00853 now targets strict `VS::Field::GimmickPlacement` over typed host/state/
  snapshot values. Legacy record reads and terrain binding stay in the Live
  adapter; invalid full-width slots refuse before reads, stationary refusals
  before support sampling, and output is published only after a valid snapshot.
- Required `Field_GimmickPlacementEntry` C output/header and original module
  `800A0024` remain explicit. The old symbol's five Native definition/header/
  trigger/module/test consumer files retire through the unchanged gate; this is
  not required ABI/global EC retirement or completion of row 27 / #287.
- Source `5cf7c848` plus current owned slice: full Debug/Release 4,694 tests /
  8,634,162 assertions, zero failures/skips, both canonical replays unchanged.
  Focused gimmick 8 / 723; independent checked-MIPS height/packing model covers
  77 signed/wrapping samples. Exact original 56 words `800A0024-800A0100` read
  from verified overlay SHA1 `D53AACCC3B3A2FC057D05E0DCEA92F7182BC72A9`;
  inherited uninitialized check is prior #316 evidence, not safely executed as
  an original whole-function oracle. No reference files or assets changed.
- 191 copier / nine extent controls pass, editor diagnostics clean; quality
  five existing / zero new. Explicit optional refusal and final newlines fixed
  the eight new local findings without baseline/expectation weakening. Repeat
  copier/project generation preserves all tracked and new Native/project/
  manifest bytes. Logs: `build/owner-checkpoint/gimmick-placement-core-*-regression.log`.
- Row-33 metadata `5cf7c848` also has exact green CI `42350966202`. This partial
  row-27 source milestone is pending publication/exact CI; next work remains
  the owning gimmick/door/interaction paths, including #287's typed target.
- Final Live host preserves original eligibility read order before placement
  fields are copied; focused/full Debug/Release and scoped quality were rerun
  after that review repair with unchanged results. Reviewed modernization plus
  unchanged gate derivation now yields 2,690 promoted / one separate frame `wip`,
  238 behavior records and zero violations; no gate or baseline was relaxed.

### Event VM Source Promotion Complete (#276 Row 33)

- Final source `7172dfae50be38b95b0f1e4826d9cf1e2e8e2af2` is pushed and exact
  Source Quality CI `42350522025` completed successfully. Row 33 is now done for
  all 57 source identities; #276 remains open for its remaining rows.
- `Event-native-vm-triggers-slice` records the remaining 53 identities and 50
  historical C symbols outside the four separately recorded cutscene identities.
  Historical `2ed487e1` yields 31 Native code consumer files using unchanged
  `gates.strip_source`/`IDENTIFIER`; current gates require zero. Comments/strings
  are excluded in both measurements; no source or retirement gate was relaxed.
- Required native C entries, opcode/module tables, global bindings and all wider
  EC retirement/behavior gates remain separate. This is source promotion, not a
  retail-wide gameplay or complete compatibility retirement claim.
- Next ordered remaining #276 row is 27, field gimmicks/doors/interactions,
  including #287's typed chest interaction target. Metadata reconciliation does
  not change the freshly verified source or require a replay baseline refresh.

### Native Trigger/Interpolation Cores (#276 Row 33)

- F01261/F01276 ownership now targets actual strict typed bounded trigger and
  interpolation cores, not their pure C forwarding adapters. Required C caller/
  header/module mappings remain explicit and intact. This is native core promotion,
  not global ABI/EC/table retirement. Provider selection now gives retained C
  symbols their C binding headers and qualified native symbols their core headers;
  a targeted copier regression prevents C++ headers leaking into C-linkage callers.
- Fresh unchanged-original trigger harness: 100,000 comparisons, seed `7A1663`,
  zero mismatches/all thirteen callback categories; all six planted deviations
  detected in bounded diagnostic copies. Callback stand-ins/valid bounded records
  only, not unsafe original extent/indices or visual/audio/retail comparison.
  Native entry/trigger/retention/gimmick gate: 60 / 884, including module resolution, typed status/
  direct-request state and interpolation built-in/custom/invalid/zero-duration
  entry equality. Entry equality is not a self-referential original oracle.
- On `096b58f3` plus this slice, complete Debug/Release builds and regressions
  pass 4,690 cases / 8,633,604 assertions, zero failures/skips and both canonical
  fingerprints unchanged. Two new cases add 192; no baseline refresh. Changed
  quality: five existing / zero new; 191 copier/nine extent/Pylance syntax gates
  pass. Comparison sample/explicit narrowing helpers preserve all assertions.
  Evidence: `VR-EVENT-FINAL-CORE-PROMOTION`, existing original trigger harness logs
  and `build\owner-checkpoint\native-trigger-easing-cores-*-regression.log`.
  Required C bindings/global retirement remain open and existing generated
  StatusEffects/ScriptCamera warnings remain visible; no suppression or baseline
  change. Queue/init predecessor `096b58f3` is green on exact CI `42346621904`.
- Unchanged consumer gate required old entry symbols to retire. Adopted native
  binding names `Event_EvaluateRoomEventTriggersEntry` and
  `Event_EvaluateInterpolationCurveEntry` across definitions/headers/generated/
  maintained/test callers and original module addresses. Signatures and required
  ABI remain; no gate relaxed and no global EC retirement claim. Source promotion
  now derives for all 57 row identities, pending this exact pushed revision's CI.
- Repeat generation preserves all tracked Native/project/manifest bytes,
  including native entry symbols and generated/maintained caller adoption.
  Unmodified gates derive 2,689 promoted / two separate compatibility `wip`
  (frame/gimmick entries); row 33 is 57/57 source-promoted with exact new CI
  pending. Required native C/module bindings and all EC retirement gates remain
  tracked separately; this is not whole-game or global compatibility retirement.

### Native Queue/Initialization Helpers (#276 Row 33)

- F01277/F01850 register existing strict typed owned queue/init cores. No C
  callers remained; raw destination queue C/module and heap-allocation initializer
  bodies/prototypes retire through the copier. Only the obsolete initializer
  pointer-cast adaptation was removed. Native startup already uses this initializer.
- Queue ASM verifies `0x1800` bytes and event id * three sectors plus EVT LBA.
  Reference build has no INITBTL matching ASM; exact 27 words were instead read
  from the existing overlay, SHA-1 `D7EA16EF957179CAD6E3B02727BB714713CFCC32`
  matching config, offset `0x12AC`, CPU `800FAAAC..800FAB14`: two clears,
  three allocation calls for `0x1800/0x80/0x400`, and `0x400` copy from `800FAF7C`.
  No read-only reference output was generated or edited.
- Actual live tests cover script/chained owned destinations, busy refusal without
  replacing staging, completed publication/adoption, initializer cancellation and
  state/return/all-thread/wait reset. Existing failure controls remain. Focused
  gate: 3 / 69. Full-queue control retains 15 routes: 14 generated plus explicitly
  validated native allocation guard/refused fallback, not a lowered threshold.
- On `c9bd38a5` plus this slice, full Debug/Release builds and regressions pass
  4,688 cases / 8,633,412 assertions, zero failures/skips and both canonical
  fingerprints unchanged. New cases54/null1 minus2per-symbol gives net53; no
  baseline refresh. Changed quality: five existing / zero new; 190 copier/nine
  extent/Pylance syntax controls pass. Assertion-only complexity factoring keeps
  every check. Evidence: `VR-EVENT-QUEUE-INIT-PROMOTION` and
  `build\owner-checkpoint\native-queue-init-*-regression.log`.
  Native owner/cancel/failure safety is not unchecked-original parity; required
  table/global retirement and trigger/easing C entries remain separate. Actor
  predecessor `c9bd38a5` is green on exact CI `42344203422`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,687 promoted / four compatibility `wip`; row 33 is
  now 55/57. All selected generated VM helper bodies are retired; trigger/easing
  C-entry and table/global EC gates remain separate and open.

### Native Actor-Reference Helper (#276 Row 33)

- F01275 registers the strict typed actor-reference core. Maintained actor/audio/
  dialog/effect hosts and generated camera calls migrate through a typed native
  live adapter; original C body/prototype retire via the copier. Module address
  `800BFE50` resolves to that native adapter. Core, not the forwarding adapter,
  is the promotion destination; required caller/module ABI remains explicit.
- ASM verifies unsigned halfword input, nibble-0 model lookup, nibble-1 low-five
  direct slot, and unchanged special references for nibbles 2-15. Exhaustive core
  test checks all 65,536 values and model-host calls; live slot/special test and
  original five real lookup expectations remain. Focused gate: 3 / 192,518.
- On `5bd88fa0` plus this slice, full Debug/Release builds and regressions pass
  4,686 cases / 8,633,359 assertions with zero failures/skips and both unchanged
  canonical fingerprints. Two new cases add 192,513; existing case expectations
  unchanged. No baseline refresh. Broad quality: 32 existing / zero new; 190
  copier/nine extent/Pylance syntax controls pass. Generator helper header path
  corrected to workspace spelling without changing semantics. Evidence:
  `VR-EVENT-ACTOR-REFERENCE-PROMOTION` and
  `build\owner-checkpoint\native-actor-reference-*-regression.log`.
  Model-host corpus is not all-actor/visual parity; special references are not
  guessed as pointers. Remaining queue/init/trigger/easing/table/global debt stays
  separate; no whole-row or global ABI retirement claim. Opcode-size predecessor
  `5bd88fa0` is green on exact CI `42342552787`.
- Repeat generation preserves all tracked Native/project/manifest bytes,
  including maintained/generated caller and native module adapter mappings.
  Unmodified gates derive 2,685 promoted / four compatibility `wip`; row 33 is
  now 53/57. Queue/init and trigger/easing/table/global gates remain open.

### Native Opcode-Size Helper (#276 Row 33)

- F01274 registers the existing strict native byte-indexed size lookup through
  a bounded span. No C callers remained; unused C body/prototype/module binding
  `800BFE34` retires through the copier. ASM `800BFE38/800BFE44` uses unsigned
  byte opcode/size. Native parser and skip-scan zero-size refusal remains intact.
- Exhaustive tests cover all 256 byte indices against spans of 0/1/17/255/256
  entries, including zero encoded size and absent entry refusal. The existing
  recording fixture keeps its full-table default. Retired address resolves null.
  Focused gate: two cases / 1,294 assertions.
- On `76f7a6bf` plus this slice, full Debug/Release builds and regressions pass
  4,684 cases / 8,440,846 assertions with zero failures/skips and both unchanged
  canonical fingerprints. New exhaustive checks1,280/null1 minus two removed
  per-symbol assertions gives net1,279. No baseline refresh. Changed quality:
  five existing / zero new; 190 copier/nine extent controls pass. Evidence:
  `VR-EVENT-OPCODE-SIZE-PROMOTION` and
  `build\owner-checkpoint\native-opcode-size-*-regression.log`.
  Native short/absent span safety is not unsafe original bounds parity. Remaining
  actor-reference/queue/init/trigger/easing C-entry/table/global EC helpers stay
  separate; no whole-row/retail claim. Cleanup predecessor `76f7a6bf` is green
  on exact CI `42341345494`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,684 promoted / four compatibility `wip`; row 33 is
  now 52/57. Remaining actor-reference/queue/init/trigger/easing/table/global
  retirement requirements remain open.

### Native Texture/Actor Cleanup Helpers (#276 Row 33)

- F00972/F01278 register existing strict native fixed texture restore and
  seventeen-slot loaded-actor idle cleanup. No C callers remained; unused
  scratch/C bodies/prototypes/module bindings retire through the copier. This
  removes the dead decompiler artifact by ownership, not a naming-only cleanup.
  Actual texture image identity remains unresolved; catalog confidence stays low.
- ASM verifies source `(470,384,18,36)` to `(16,400)` and one `0x18` move packet,
  plus loaded-actor checks/idle callbacks over all slots 0-16. Direct tests cover
  exact rectangle, none/all/every individual actor and callback order. Existing
  live end-skip cleanup packet/cursor checks remain. Retired addresses resolve
  null. Focused gate: four cases / 702 assertions.
- On `e1b37ce9` plus this slice, full Debug/Release builds and regressions pass
  4,683 cases / 8,439,567 assertions with zero failures/skips and both unchanged
  canonical fingerprints. New direct checks39/null2 minus four removed per-symbol
  assertions gives net37; no baseline refresh. Changed quality: five existing /
  zero new findings; 190 copier/nine extent controls pass. Evidence:
  `VR-EVENT-CLEANUP-HELPERS` and
  `build\owner-checkpoint\native-cleanup-helpers-*-regression.log`.
  Remaining actor-reference/opcode-size/queue/init/trigger/easing C-entry/table/
  global EC obligations stay open; no whole-row or actual-image visual parity claim.
  Playback predecessor `e1b37ce9` is green on exact CI `42340089266`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,683 promoted / four compatibility `wip`; row 33 is
  now 51/57. Remaining actor-reference/opcode-size/queue/init/trigger/easing and
  table/global retirement obligations remain explicit.

### Native Playback Query Helpers (#276 Row 33)

- F01253/F01254/F01260 register the existing strict typed playback-phase query,
  packed pending selector and requested-phase start latch. Maintained actor pose/
  model/animation, cutscene and room callers directly use the native live host
  API; no new compatibility wrapper. Unused C bodies/prototypes/module bindings
  retire via the copier and addresses resolve null.
- ASM `800BEB04` reads unsigned phase; `800BEB18/800BEB1C` read halfwords before
  selector packing/truncation; `800BEC3C/800BEC48` latch only requested phase.
  Tests cover all 256 phases/two confirmation states, 16 source nibbles/seven
  boundary event halfwords and retained confirmation. Query/null gate: 3 / 1,659.
  Actual actor/cutscene/model tags pass 118 / 4,764; unmatched initial guessed
  tags are not claimed as coverage. Fixture now restores source and event id.
- On `7685f016` plus this slice, full Debug/Release builds and regressions pass
  4,681 cases / 8,439,530 assertions, zero failures/skips and both unchanged
  canonical fingerprints. New query checks add 1,648/null checks three; removal
  of three unused symbols removes six per-symbol assertions, net 1,645. No
  baseline refresh. Changed quality: five existing / zero new; 190 copier/nine
  extent controls pass. Evidence: `VR-EVENT-PLAYBACK-QUERY-PROMOTION` and
  `build\owner-checkpoint\native-playback-query-*-regression.log`.
  Remaining VM queue/init/actor/texture/trigger/easing C-entry/table/global EC
  helpers still require their own gates; no whole-row or visual parity claim.
  Special predecessor `7685f016` is green on exact CI `42338479772`.
- Repeat generation preserves all tracked Native/project/manifest bytes including
  owned actor/cutscene/room caller migrations. Unmodified gates derive 2,681
  promoted / four compatibility `wip`; row 33 is now 49/57. Remaining helper/
  C-entry/table/global retirement obligations stay open.

### Native Special Control Slot (#276 Row 33)

- F01187 registers the final strict typed special-control core and guarded native
  module/normal/skip identity; C body/prototype retire via the copier and Combat
  Lab uses the native special identity. ASM `800BB040` bounds selectors 0-17,
  jump table `8006967C` identifies every branch, and `800BB240` saves six-byte
  subroutine continuation. Existing reward/unlock/count/camera/subroutine/typed
  forwarding checks remain rather than guessing selector meanings.
- Live direct/guarded normal/skip tests observe force and undefined continuation,
  real timer pause/resume, pending chained request cancellation/free and refused
  adoption with current source retained. Selector 6 frees storage without clearing
  the original chained flag; the new fixture expectation was corrected against
  `800BB168` without removing ownership/refusal checks. Two new cases add 128;
  two module/detached checks add two. Focused control/Combat Lab: 34 / 94,943.
- On `1da322cc` plus this slice, full Debug/Release builds and regressions pass
  4,679 cases / 8,437,885 assertions, zero failures/skips and both unchanged
  canonical fingerprints. No baseline refresh. Broad quality: 32 existing / zero
  new findings; 190 copier/nine extent controls pass. Evidence:
  `VR-EVENT-CONTROL-SPECIAL` and
  `build\owner-checkpoint\native-control-special-*-regression.log`.
  Required table ABI and remaining VM/trigger/C-entry/global EC helper debt stay
  separate. Live callbacks are not full reward/ending/render/UI or whole-row parity.
  Lifecycle predecessor `1da322cc` is green on exact CI `42337079495`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,678 promoted / four compatibility `wip`; row 33 is
  now 46/57. All selected control identities are native; remaining VM/trigger/
  easing C-entry/table/global retirement helpers still need their own gates.

### Native File/Lifecycle Control Slots (#276 Row 33)

- F01027/F01030/F01205/F01206 register strict native diagnostic/wait/chained-load/
  finish cores and guarded native module/normal/skip identities. Original C bodies/
  prototypes retire via the copier; Combat Lab diagnostic/finish/load comparisons
  follow native identities. ASM confirms continuation, complete-state wait release,
  allocate/queue before chain flag, and eight-dialog dismissal before completion.
- Live guard/table tests observe pending/ready/failed wait cursor/tick/source state,
  old generation retained until completed chained adoption, actual completion with
  field-tick restore and retained finish cursor, and diagnostic continuation to a
  real subsequent action. Existing rollback/busy/queue/cancellation/reused-slot/
  pending-dialog tests remain. Fixture now restores mutated event id. Four new
  cases add 244 assertions; eight module/detached checks add eight. Focused
  lifecycle/lifetime/Combat Lab gate: 40 / 94,878.
- On `3996780c` plus this slice, full Debug/Release builds and regressions pass
  4,677 cases / 8,437,755 assertions, zero failures/skips and both canonical
  fingerprints unchanged. No baseline refresh. Broad quality: 32 existing / zero
  new findings; 190 copier/nine extent/Pylance syntax controls pass. Header dedup
  fixture moves to unpromoted F01031 and separately proves promoted F01027 uses
  its registered header; no audit requirement weakened. Evidence:
  `VR-EVENT-CONTROL-FILE-LIFECYCLE` and
  `build\owner-checkpoint\native-control-file-lifecycle-*-regression.log`.
  Native failure/rollback/refusal safety is outside unchecked original behavior;
  required table ABI, special control/C-entry/global EC debt remain separate.
  No whole-row/retail/physical/visual parity claim.
  Configuration predecessor `3996780c` is green on exact CI `42335100514`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,677 promoted / four compatibility `wip`; row 33 is
  now 45/57. Diagnostic/file wait/load/finish identity and state checks are intact.

### Native Timer/Easing Configuration Slots (#276 Row 33)

- F01106/F01111 register strict typed timer/easing cores and guarded native
  module/normal/skip identities. Original C bodies/prototypes retire via the
  copier. Easing original `9DB56A489D7AD325` was captured/published in `c2a90621`
  before retirement, exact green CI `42333102131`; current independent model/
  native all-slot corpus and slot/family/sign mutants keep that unchanged oracle.
- Live direct/guarded normal/skip timer selectors and packed times observe actual
  timer record/HUD/A0-A3 publication. Invalid selectors preserve state before
  callbacks; reset ignores unused selectors and preserves paused mode/scope.
  All 256 encoded easing operands and untouched slots preserve family/signed
  power. Fixtures restore timer/HUD/flags. Three new cases add 8,378 assertions;
  four module/detached checks add four. Combined gate: 25 / 94,753.
- On `c2a90621` plus this slice, full Debug/Release builds and regressions pass
  4,673 cases / 8,437,503 assertions with zero failures/skips and both unchanged
  canonical fingerprints. No baseline refresh. Broad quality has 32 existing /
  zero new findings; expected-state/flag/matrix helper factoring preserves every
  sample/assertion. All 190 copier/nine extent controls pass. Evidence:
  `VR-EVENT-CONTROL-TIMER-EASING` and
  `build\owner-checkpoint\native-control-timer-easing-*-regression.log`.
  Timer packing is ASM-backed and tested through its existing native callback;
  unsafe original table selectors were never executed. Required table ABI and
  remaining special/I/O/C-entry/global EC debt remain separate; not full curve
  interpolation/controller/visual or whole-row parity.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,673 promoted / four compatibility `wip`; row 33 is
  now 41/57. Original curve digest and actual timer callback state remain intact.

### Original Easing Configuration Frozen (#276 Row 33)

- Before retiring F01111, captured the actual retained original C setter's
  return and all four two-byte curve records for every encoded operand, with
  sentinel family 3 / power -4 in untouched slots. Snapshot original records
  before native execution. Original/native digest: `9DB56A489D7AD325`.
- ASM `800B9730` selects slot by bits 6-7, `800B973C/800B9740` selects family
  bits 4-5, and `800B9750-800B9760` sign-extends the low nibble. All 256 operands /
  1,024 slot comparisons match; three planted slot/family/sign deviations are
  detected. Focused gate: two cases / 261 assertions.
- On `d7836622` plus these tests, full Debug/Release builds and regressions pass
  4,670 cases / 8,429,121 assertions with zero failures/skips and both unchanged
  canonical fingerprints. Two new cases add 261; no baseline refresh. Changed
  quality reports five existing / zero new findings. Evidence:
  `VR-EVENT-EASING-ORIGINAL-FREEZE` and
  `build\owner-checkpoint\original-easing-freeze-*-regression.log`.
  Original C setter/module/table identities remain. Ownership stays 2,671
  promoted / four compatibility `wip`, row 33 39/57. This preserves an oracle,
  not interpolation/gameplay/visual parity or ownership/ABI/EC retirement.
  Wait predecessor `d7836622` is green on exact CI `42331913207`.

### Native Button-Wait Control Slots (#276 Row 33)

- F01042/F01110 now register strict native yield/timeout cores and guarded native
  module/normal/skip identities. Original C bodies/prototypes retire via the
  copier. The independent original result models must match unchanged digests
  `45DB1AECCD72B51C` / `3E94AE9F58CCF5A6` / `482D3AE966711625`, captured with
  actual originals before retirement in `88d88251`, exact green CI `42330375280`.
  All 197,376 per-case comparisons and four planted deviations remain; no new
  binding is used as its own original oracle.
- Live normal/skip/direct/guarded byte masks and timeout initialization/expiry/
  full-word input clearing preserve exact per-thread held/yielded/advanced cursors,
  including zero duration and negative/wrapped counters in all four valid threads.
  Two new cases add 4,060 assertions; four module/detached checks add four.
  Combined controls/original gate: 24 cases / 283,496 assertions.
- On `88d88251` plus this slice, complete Debug/Release builds and regressions
  pass 4,668 cases / 8,428,860 assertions, zero failures/skips and both unchanged
  canonical fingerprints. No baseline refresh. Broad quality reports 32 existing
  / zero new findings; explicit bounded test narrowing fixed without changing
  samples. All 190 copier/nine extent controls pass. Evidence:
  `VR-EVENT-CONTROL-BUTTON-WAITS` and
  `build\owner-checkpoint\native-control-button-waits-*-regression.log`.
  Required table ABI and remaining controls/C-entry/global EC debt stay explicit;
  this is not whole-row or physical-controller/full-game completion.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,671 promoted / four compatibility `wip`; row 33 is
  now 39/57. Frozen original constants and all live wait states remain intact.

### Original Button-Wait Results Frozen (#276 Row 33)

- Before retiring F01042/F01110, captured actual retained original C outcomes,
  halfword waits and entire pressed-input words against native typed cores. The
  original/native host shares globals, so each original output is snapshotted
  and identical inputs restored before native execution. No raw pointers hashed.
- All 65,536 mask/input-byte pairs, all 65,536 halfword counters with both input
  states, and all 256 initialization bytes with three input values match. Frozen
  digests: yield `45DB1AECCD72B51C`, timeout `3E94AE9F58CCF5A6`, initialization
  `482D3AE966711625`. Four planted native mask/counter/input-clear deviations
  are detected. Focused gate: four cases / 197,386 assertions.
- ASM confirms initialization before consumption, byte mask over word input,
  halfword decrement/wrap then signed comparison, and clearing the entire pressed
  word. Valid threads/bounded commands only; unsafe original indices are never
  executed. Original C bodies and module/table identities remain in this freeze.
  Ownership stays 2,669 promoted / four compatibility `wip`, row 33 37/57.
- On `dedb5300` plus these tests, complete Debug/Release builds and regressions
  pass 4,666 cases / 8,424,796 assertions and both unchanged canonical replays,
  zero failures/skips. Four new cases add 197,386; no baseline refresh. Quality
  reports five existing / zero new findings. Evidence:
  `VR-EVENT-BUTTON-WAIT-ORIGINAL-FREEZE` and
  `build\owner-checkpoint\original-button-wait-freeze-*-regression.log`.
  This checkpoint preserves an independent oracle; it does not promote code,
  retire ABI/EC debt or establish physical-controller/full-game parity.
  Cleanup predecessor `dedb5300` is green on exact CI `42329062089`.

### Native Skip Cleanup Helper (#276 Row 33)

- F01033 now registers the strict native generation-owned cleanup core; no C
  callers remained after the cutscene opcode retirement. Its unused raw-pointer
  C body/prototype/module binding retires through the copier. Only the obsolete
  palette-call cast adaptation inside that retired body was removed; the unrelated
  skip-table allocation adaptation remains intact.
- ASM `800B67E4` tests end opcode `0x0F` before `800B6800` compares the four-byte
  sound marker; `800B681C` reads unsigned stride, and both branches clear A8.
  Existing native palette/texture, end/invalid extent and stale-generation tests
  remain. Added tests prove a marker in operand bytes is ignored while a marker
  at an instruction boundary matches, and address `800B6778` now resolves null.
  Actual native end cleanup retains correct cursors and emits a guarded texture
  packet. Focused gate: six cases / 676 assertions.
- On `f6f0f9d1` plus this slice, full Debug/Release builds and regressions pass
  4,662 cases / 8,227,410 assertions with zero failures/skips and both canonical
  fingerprints unchanged. Seven marker/null assertions were added; two per-symbol
  assertions disappear with the unused binding, for net five. No baseline refresh.
  Changed quality/tidy reports five existing / zero new findings; 190 copier/nine
  extent controls pass. Evidence: `VR-EVENT-SKIP-CLEANUP-PROMOTION` and
  `build\owner-checkpoint\native-control-skip-cleanup-*-regression.log`.
  Source/range/zero-stride refusal is native safety behavior, not an unsafe-original
  comparison. Remaining controls/C-entry/table ABI/global EC debt stays separate;
  state/packet checks do not claim full visual/audio/retail parity.
  Predecessor `f6f0f9d1` has exact green CI `42327744063`.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,669 promoted / four compatibility `wip`; row 33 is
  now 37/57. The unused raw-pointer cleanup module boundary is retired, while
  remaining control-table and compatibility retirement gates stay open.

### Native Cutscene Control Slots (#276 Row 33)

- F01032/F01034/F01035 now register the strict typed begin/jump-end/plain-end
  cores with guarded native module/normal/skip identities. Original opcode C
  bodies/prototypes retire via the copier; Combat Lab branch discovery follows
  the new identity. The F01033 cleanup helper's original C body/binding remains
  separately unpromoted; production already uses bounded native cleanup/scanning.
- ASM `800B6760/800B6764` confirms operand bit 0 music policy; `800B6768`
  treats every nonzero force byte alike. Both end routines snapshot then clear
  skip state and compare the old byte to 2. Live direct/guarded normal/skip tests
  observe music policy, exact retained yield cursors, signed targets, skip/A8
  state and actual cleanup texture-packet emission with no drops/guard damage.
  The interpreter Boolean reports completion, not yield; fixture expectations
  were corrected against native control flow and a real bounded overlay queue.
- On `90aceeaf` plus this slice, full Debug/Release builds and regressions pass
  4,661 cases / 8,227,405 assertions with zero failures/skips and both canonical
  fingerprints unchanged. Two new cases add 21,130; six binding checks add six.
  Focused controls pass 18 / 82,046. Broad quality has 32 existing / zero new
  findings; 190 copier/nine extent controls pass. No baseline refresh.
  Evidence: `VR-EVENT-CONTROL-CUTSCENE` and
  `build\owner-checkpoint\native-control-cutscene-*-regression.log`.
  Packet presence/state tests do not establish visual/audio or physical-controller
  parity. Remaining controls/cleanup ownership/C-entry/table ABI/global EC debt
  stays explicit; this is not whole-row completion.
- Repeat generation preserves all tracked Native/project/manifest bytes.
  Unmodified gates derive 2,668 promoted / four compatibility `wip`; row 33 is
  now 36/57. Store predecessor `90aceeaf` is green on exact CI `42325918506`.

### Native Inventory/Input Store Slots (#276 Row 33)

- F01098/F01102 now register the strict typed inventory-count and button-state
  cores with guarded native module/normal/skip identities. Original C bodies and
  prototypes retire through the supported copier. Inventory stores a byte-narrowed
  count, not a Boolean; ASM `800B9194/800B91A0` sign-extends flag/item and
  `800B91AC` masks the count. Button ASM `800B9264/800B9268` selects low ten flag
  bits and high-bit current/previous source; both sources use unsigned byte loads.
- Live normal/skip/direct/guarded button selectors cover all 256 input bytes,
  boundary flag ids and ignored selector bits. Recording-host count tests cover
  signed item ids and byte widths; the existing Items fixture separately verifies
  actual inventory count propagation. Module addresses and detached guards pass.
  Focused controls: 16 cases / 60,910 assertions. Surrounding VM/inventory:
  264 / 270,573. Physical-controller parity is not claimed.
- On `24fcca63` plus this slice, complete Debug/Release builds and regressions pass
  4,659 cases / 8,206,269 assertions with zero failures/skips and both unchanged
  canonical fingerprints. Two new cases add 24,824; eight actual-inventory and
  four module/detached checks add twelve more. No baseline refresh. Broad quality
  reports 32 existing / zero new findings; 190 copier/nine extent controls pass.
  Evidence: `VR-EVENT-CONTROL-STORES` and
  `build\owner-checkpoint\native-control-stores-*-regression.log`.
  Required table ABI and remaining controls/C-entry/global EC debt stay open.
- Repeat copying preserves all tracked Native/project/manifest bytes; old store
  names have no Native references and editor diagnostics are clear. Unmodified
  gates derive 2,665 promoted / four compatibility `wip`; row 33 is now 33/57.
  Jump/return predecessor `24fcca63` has exact green CI `42323181412`.

### Native Jump/Return Control Slots (#276 Row 33)

- F01087/F01088/F01094/F01095 share the existing strict signed operand1 jump
  core/binding; all four original ASM bodies are identical in operation and each
  original address resolves to that shared native identity. F01091 now owns typed
  generation-aware saved return. Old C bodies/names/prototypes/parser/branch/live
  consumers retire; required table ABI remains explicitly guarded.
- Observed direct/guarded normal/skip jump targets cover signed bounds within source;
  saved return present/missing/stale-generation cases preserve advance/jump/refusal.
  Fifteen control cases pass 36,298 assertions; surrounding consumers pass 178 /
  245,121. No exit-code proxy is used for target correctness.
- On `e3d73dc9` plus this slice, complete Debug/Release builds and regressions pass
  4,657 cases / 8,181,433 assertions and both unchanged canonical replays. Two new
  cases/ten binding checks add 154; no baseline change. Broad quality/tidy reports
  zero new findings; 190 copier/nine extent controls and Python syntax pass.
  Repeat copying preserves tracked Native/project/manifest bytes; old jump/return
  names have no Native reference. Evidence: `VR-EVENT-CONTROL-JUMP-RETURN` and
  `build\owner-checkpoint\native-control-jump-return-*-regression.log`.
  Five catalog identities mean two logic implementations, not fabricated duplicate
  code. Remaining controls/C-entry/table ABI/global EC debt stays separate.
  Branch predecessor `e3d73dc9` is green on exact CI `42321098884`.
  Unmodified gates derive 2,663 promoted / four compatibility `wip`; row 33 is
  now 31/57. Shared alias identities preserve one real jump implementation.

### Native Flag-Branch Control Slots (#276 Row 33)

- F01085/F01086 now register strict native unsigned immediate/pair comparison and
  typed signed relative flow. Original C bodies/names/prototypes/parser consumers
  retire after actual-original digests `E1BCD4375C206433` / `C89571B1A7F86BCD`
  were captured in `7a49f59b`, green exact CI `42319048643`. Every safe per-case/
  comparison/alias/bit-test/unknown-operation/model/mutant check and both unchanged
  original constants remain; no new binding is used as the original oracle.
- Live normal/skip/direct/guarded all comparison nibbles and unsigned boundary
  values preserve targets/tick outcomes. Control/branch selection passes 19 cases /
  167,232 assertions; helper type-order correction preserves every expectation.
- On `7a49f59b` plus this slice, complete Debug/Release builds and regressions pass
  4,655 cases / 8,181,279 assertions and both unchanged canonical replays. One new
  case/four contract checks add 4,100; no baseline changes. Broad quality/tidy has
  zero new findings; 190 copier/nine extent controls and Python syntax pass.
  Repeat copying preserves tracked Native/project/manifest bytes; old branch names
  have no Native reference. Evidence: `VR-EVENT-CONTROL-FLAG-BRANCHES` and
  `build\owner-checkpoint\native-control-flag-branches-*-regression.log`.
  Only these cores are reviewed; required ABI and remaining jumps/controls/C-entry/
  global EC debt stay explicit, not whole-row completion.
  Unmodified gates derive 2,658 promoted / four compatibility `wip`; row 33 is
  now 26/57. Original branch digests and required table ABI remain explicit.

### Branch Original Oracle Preservation (#276)

- Existing immediate/pair flag corpora captured normalized FNV64 directly from
  still-present original C handlers: `E1BCD4375C206433` / `C89571B1A7F86BCD`.
  Digests retain taken/fall-through and signed relative offset, not ASLR pointers.
  Every 65,536 safe per-case comparison and existing mutant remains; original C
  functions still run. Both digest/native equality and frozen constants are checked.
- Debug/Release focused branch coverage passes six cases / 131,088 assertions;
  full suites on `005fd14f` plus this test-only slice pass 4,654 / 8,177,179 and
  both unchanged canonical replays. Four added assertions; no case/baseline/
  promotion/retirement changes. Scoped quality/tidy is clean; temporary prints
  removed. Evidence: `VR-EVENT-BRANCH-ORIGINAL-FREEZE` and
  `build\owner-checkpoint\branch-oracle-freeze-*-regression.log`.
  Arithmetic predecessor `005fd14f` is green on exact CI `42317614201`. Keep these
  oracles independent when original branch handlers retire.

### Native Arithmetic Control Slots (#276 Row 33)

- F01096/F01097 now register strict native immediate/pair arithmetic cores with
  guarded table identities. Original C bodies/names/prototypes retire only after
  independent actual-original digests `41C1AFD9493A9DAF` / `593182F1F2B16919` were
  captured in `16fd680c`. Every safe per-case/byte/alias/operation/model/mutant check
  remains and normalized native results must match those unchanged originals.
  Unsafe original zero-divisor execution stays excluded; native R3000 results are
  tested separately rather than invoking original traps.
- Live normal/skip/direct/guarded operation nibbles 0-15 cover aliases, byte results,
  copy/difference/swap, unknown no-op operations and zero-divisor native bytes.
  Control/arithmetic selection passes 15 cases / 97,588 assertions. Scenario loop
  extraction preserves every check below strict nesting limits.
- On `daa632dd` plus this slice, complete Debug/Release builds and regressions pass
  4,654 cases / 8,177,175 assertions and both unchanged canonical replays. Two new
  cases/four binding checks add 1,324; no baseline changes. Broad quality/tidy has
  zero new findings; 190 copier/nine extent controls and Python syntax pass.
  Repeat copying preserves tracked Native/project/manifest bytes; old arithmetic
  names have no Native reference. Evidence: `VR-EVENT-CONTROL-ARITHMETIC` and
  `build\owner-checkpoint\native-control-arithmetic-*-regression.log`.
  Only these cores are reviewed; required ABI and remaining control/C-entry/global
  EC debt remain. Loop predecessor `daa632dd` is green on exact CI `42315217179`.
  Unmodified gates derive 2,656 promoted / four compatibility `wip`; row 33 is
  now 24/57. Original digests and required table bindings remain explicit.

### Native Loop Control Slots (#276 Row 33)

- F01092/F01093 register strict native counter-store/backward-loop cores with
  guarded table identities. Old C bodies/names/prototypes/parser/raw-pointer test
  consumers retire. Independently captured original digests `79BC34873972BB25` /
  `DF9CCA1BC970DBA5` were published in `16fd680c`, green exact CI `42312665009`;
  every valid per-case/all-16-counter/model/flow/mutant check remains and normalized
  native results must match those unchanged original constants.
- Live normal/skip/direct/guarded indices 0/15 and initial counts 0/1/2/255 verify
  byte wrap, frame yield and finish. Bounded sentinel frames preserve the entire
  counter bank. Control/loop selection passes 13 cases / 39,435 assertions. Shared
  data-driven binding checks retain all 11 entries and checks below strict limits.
- On `16fd680c` plus this slice, complete Debug/Release builds and regressions pass
  4,652 cases / 8,175,851 assertions and both unchanged canonical replays. Two new
  cases/four binding checks add 16,818; no baseline changes. Broad quality/tidy has
  zero new findings; 190 copier/nine extent controls and Python syntax pass.
  Repeat copying preserves all tracked Native/project/manifest bytes; old loop
  names have no Native reference. Evidence: `VR-EVENT-CONTROL-LOOPS` and
  `build\owner-checkpoint\native-control-loops-*-regression.log`.
  Only these two cores are reviewed; table ABI and remaining controls/oracles stay
  explicit debt, not whole-row or global EC retirement.
  Unmodified gates derive 2,654 promoted / four compatibility `wip`; row 33 is
  now 22/57. Required table bindings and frozen original digests remain explicit.

### Loop/Arithmetic Original Oracle Preservation (#276)

- Before those C handlers retire, existing safe per-case corpora capture normalized
  FNV64 directly from actual originals: immediate arithmetic `41C1AFD9493A9DAF`,
  pair arithmetic `593182F1F2B16919`, loop store `79BC34873972BB25`, loop back
  `DF9CCA1BC970DBA5`. Original/native equality and frozen constants are checked.
  Digests contain modified result flags or all 16 counters/relative flow; raw
  addresses and unmodified immediate-operand state are excluded. Existing safe
  divisor/index limits and mutant coverage remain; original functions still run.
- Debug/Release focused coverage passes six cases / 74,259 assertions; full suites
  on `447f6140` plus this test-only slice pass 4,650 / 8,159,033 and both unchanged
  canonical replays. Eight extra digest assertions; no case/baseline/promotion/
  retirement change. Scoped quality/tidy is clean; temporary prints are removed.
  Evidence: `VR-EVENT-LOOP-ARITHMETIC-ORIGINAL-FREEZE` and
  `build\owner-checkpoint\loop-arithmetic-oracle-freeze-*-regression.log`.
  Cached predecessor `447f6140` is green on exact CI `42310971131`. Preserve these
  original constants independently when their handlers retire.

### Native Cached Control Slots (#276 Row 33)

- F01089/F01090 now register strict native cached flag/equality/jump cores with
  guarded module ABI identities. Original C bodies/names/prototypes and parser/
  raw-pointer test consumers retire. The independently captured actual-original
  digest `A21BEE19C943E325` was published first in `deea1c7f`, green exact CI
  `42308284876`. Current per-case model and normalized native digest must match
  that unchanged original constant, not a new binding substituted as the oracle.
- All 65,536 byte pairs remain checked; direct/guarded normal/skip cached values
  0/127/128/255 preserve branch target/tick results. Planted wrong cache byte,
  operand and target mutants are rejected. Control/branch selection passes
  14 cases / 144,986 assertions; helper factoring preserves all assertions.
- On `deea1c7f` plus this slice, complete Debug/Release builds and regressions pass
  4,650 cases / 8,159,025 assertions and both unchanged canonical replays. Two new
  cases/four binding checks add 168; no baseline changes. Broad quality/tidy has
  zero new findings; 190 copier/nine extent controls and Python syntax pass.
  Repeat generation preserves all tracked Native/project/manifest bytes; old
  cached names have no Native reference. Evidence: `VR-EVENT-CONTROL-CACHED` and
  `build\owner-checkpoint\native-control-cached-*-regression.log`.
  Only these cores are reviewed; table ABI and other original/control oracles stay
  independent tracked debt, not global EC retirement.
  Unmodified gates derive 2,652 promoted / four compatibility `wip`; row 33 is
  now 20/57. The original cached digest and required module-table ABI are retained.

### Cached Original Oracle Preservation (#276)

- Before cached-flag handler retirement, the existing exhaustive 65,536-byte-pair
  test captured a normalized FNV64 digest directly from the still-present original
  C cache/branch handlers: `A21BEE19C943E325`. Records retain cached byte, branch
  decision and signed relative offset, not ASLR-dependent pointers. Original outputs
  are captured before native mutation; all individual comparisons remain.
- The digest and native/original equality checks pass in Debug and Release with
  65,538 focused assertions. Full suites on `e07806d2` plus this test-only slice
  pass 4,648 cases / 8,158,857 assertions and both unchanged canonical replays.
  Scoped quality/tidy is clean; the temporary capture print is removed. No case,
  baseline, promotion or retirement change. Evidence:
  `VR-EVENT-CACHED-ORIGINAL-FREEZE` and
  `build\owner-checkpoint\cached-oracle-freeze-*-regression.log`.
  Context predecessor `e07806d2` is green on exact CI `42306516301`.
  Keep this oracle independent when the original C handlers retire.

### Native Context Control Slots (#276 Row 33)

- F01202/F01203/F01204 register strict native wait/start/stop context cores.
  Guarded module bindings preserve low-two-bit selection/release bit `0x80`,
  embedded context start at instruction+4, signed caller skip and stop status 3.
  Original C bodies/names/prototypes and consumers are gone. Combat Lab parser,
  weapon dispatch and the legacy branch test now use native identities; the branch
  test keeps relative positions with native generations rather than raw pointers.
- Normal/skip/direct/guarded child scheduling and all 256 release-selector bytes
  pass; exact caller/child cursor generations are checked. Seven control cases
  pass 13,738 assertions. Size helpers preserve all checks. Stop uses a direct
  typed constant value, not a factory forwarder counted as a native core.
- On `b33cc082` plus this slice, complete Debug/Release builds and regressions pass
  4,648 cases / 8,158,855 assertions and both unchanged canonical replays. Two new
  cases, six binding checks and three source-fixture assertions add 3,169; no
  baseline changes. Broad quality/tidy has zero new findings; 190 copier/nine
  extent controls and Python syntax pass. Repeat copying preserves all tracked
  Native/project/manifest bytes; old context names have no Native reference.
  Evidence: `VR-EVENT-CONTROL-CONTEXTS` and
  `build\owner-checkpoint\native-control-contexts-*-regression.log`.
  Only these three cores are reviewed; required table ABI and remaining controls
  remain debt. Wait/tick predecessor `b33cc082` is green on exact CI `42302771043`.
  Unmodified gates derive 2,650 promoted / four compatibility `wip`; row 33 is
  now 18/57. Required module-table ABI is still explicitly tracked debt.

### Native Wait/Tick Control Slots (#276 Row 33)

- F01199/F01200 now register strict native frame-wait/tick cores with guarded
  module ABI bindings. Old C bodies/names/prototypes are gone; original addresses
  remain mapped to native table adapters. `800BB474-800BB4C8` confirms the halfword
  countdown; `800BB4D4` loads the unsigned tick operand. Actual tick selection still
  accepts 2/4 and preserves current speed for other bytes, not a new opcode bound.
- Live normal/skip, contexts 0/3 and direct/guarded invocation cover durations
  0/1/2/255, exact resume and tick state. All 256 tick bytes are checked against
  the existing live policy. Wait/tick globals are snapshotted/restored in fixtures.
  Five control cases pass 10,572 assertions; scenario/transition helper extraction
  satisfies strict size limits without removing assertions.
- On `e7a2326d` plus this slice, complete Debug/Release builds and regressions pass
  4,646 cases / 8,155,686 assertions and both unchanged canonical replays. Two new
  cases and four extended contract checks add 10,548 assertions; no baseline change.
  Broad quality/tidy has zero new findings; 190 copier/nine extent controls and
  Python syntax pass. Repeat copying preserves tracked Native/project/manifest
  bytes; actual imports across 75 modules have zero failures.
  Evidence: `VR-EVENT-CONTROL-WAIT-TICK` and
  `build\owner-checkpoint\native-control-wait-tick-*-regression.log`.
  Only these two cores are reviewed; table ABI and remaining identities stay
  separate debt. Control predecessor `e7a2326d` is green on exact CI `42299194355`.
  Unmodified gates derive 2,647 promoted / four compatibility `wip`; row 33 is
  now 15/57. Retained module-table bindings are not global EC retirement.

### Native Control Slot Basics (#276 Row 33)

- F01028/F01029 now register the existing strict no-op/yield cores. Actual
  `800B66E4-800B66F0` returns 0/4; their typed results preserve advance/yield.
  The established actor-opcode pattern keeps module-table ABI at guarded native
  `Event_*Opcode` bindings, not old C bodies or pointer-valued outcomes. Generated
  normal/skip/module mappings use the new identities; no old name reference remains.
- Guarded adapters validate the active command, thread and mounted generation;
  typed flow stays in invocation state. Direct/native lookup and guarded-call tests
  cover normal/skip no-op, yield and stop; detached command borrows refuse. Three
  cases pass 24 assertions. The initial fixture observer overflow was fixed by a
  stateless production stop identity without changing expected flow.
- On `e8f773bb` plus this slice, complete Debug/Release builds and regressions pass
  4,644 cases / 8,145,138 assertions and both unchanged canonical replays. Actual
  module imports load 440 globals / 16 tables / 1,705 symbols / 75 modules with
  zero failures. Broad quality/tidy has zero new findings; 190 copier / nine extent
  controls and Python syntax checks pass. Repeated copier/project generation
  preserves tracked Native/manifest/solution and the new header bytes.
  Evidence: `VR-EVENT-CONTROL-OPCODE-BASICS` and
  `build\owner-checkpoint\native-control-opcode-basics-*-regression.log`.
  Only these two cores are reviewed; the table ABI and remaining control identities
  remain tracked debt, not global EC retirement. Cleanup predecessor `e8f773bb`
  is green on exact CI `42295990919`.
  Unmodified gates derive 2,645 promoted / four compatibility `wip`; row 33 is
  now 13/57. Retained control table bindings are explicitly not EC retirement.

### Native Cleanup Ownership (#276 Row 33)

- F01257/F01272 register the existing strict native zone hook and actor-control
  cleanup. The zone hook is an actual original no-op (`800BEBEC/800BEBF0` is
  `jr ra`/`nop`), not missing implementation. Its lone zone-release caller now
  uses the native core. Actor cleanup preserves the original 17-byte selection
  and resets selected movement before camera free (`800BFDBC-800BFDE4`).
  Supported copying removes both old C bodies/prototypes/module entries; no old
  cleanup-name reference remains in Native.
- None/all/every individual actor selection passes 57 ordering assertions, and
  all seven retired lifecycle/room/cleanup addresses resolve null. Complete
  Debug/Release builds and regressions at `061e2da8` plus this ownership slice
  pass 4,641 cases / 8,145,114 assertions and both unchanged canonical replays.
  The new case plus two address checks offset four per-symbol iterations; no
  baseline changes. Surrounding cleanup/room/VM coverage passes 282 / 209,630.
- Broad quality/tidy reports zero new findings; 190 copier and nine extent
  controls pass. All tracked Native/manifest hashes survive repeat generation.
  Evidence: `VR-EVENT-CLEANUP-PROMOTION` and
  `build\owner-checkpoint\native-event-cleanup-promotion-*-regression.log`.
  Only these two cores are reviewed; remaining VM/control/C-entry compatibility
  and global EC retirement stay separate. Room predecessor `061e2da8` is green
  on exact CI `42293389198`.
  Unmodified gates derive 2,643 promoted / four compatibility `wip`; row 33 is
  now 11/57. No behavior-verification or global EC marker was advanced.

### Native Room-Section Ownership (#276 Row 33)

- F01252/F01255 now register the existing strict native room load/free cores in
  `RoomTriggers.cpp`. Production already uses transactional bounded input, immutable
  section owners and retained execution leases; this slice removes only the unused
  C bodies/prototypes/module entries and the obsolete free-body cast adaptation.
  An old-name sweep finds no code consumer, only a historical room-input comment.
  The original empty zone hook and C trigger entry remain separate debt.
- On `be560ec15053845efbf6293cced497a832d1947f` plus this ownership slice, both
  complete builds and full Debug/Release regressions pass 4,640 cases / 8,145,059
  assertions and both unchanged canonical replays. Two new address checks offset
  four removed iterations from two per-symbol tests; no baseline refresh. All five
  retired lifecycle/room addresses resolve null. Actual imports load 440 globals,
  16 tables and 1,707 symbols across 75 modules with zero failures.
- Focused room/lifecycle/module coverage passes 189 cases / 1,546,564 assertions.
  Broad quality/tidy has zero new findings; 190 copier and nine module-extent
  controls pass. Repeat copying preserves every tracked Native/manifest hash.
  Native core semantics are unchanged; fresh original load/free comparisons and
  seven mutants remain in `VR-EVENT-FRESH-ORIGINAL-DIFFERENTIAL`.
  Evidence: `VR-EVENT-ROOM-SECTION-PROMOTION` and
  `build\owner-checkpoint\native-room-sections-promotion-*-regression.log`.
  Reviewed claims cover only this pair; row 33 and global EC retirement remain open.
  Unmodified gates derive 2,641 promoted / four compatibility `wip`, with row 33
  now 9/57. No behavior-verification or global EC marker was advanced.

### Native Lifecycle Ownership (#276 Row 33)

- F01262/F01263/F01271 now register the existing strict native initialization,
  playback and dispatcher implementations in `ScriptControl.cpp`. Supported copying
  removes their obsolete C bodies/prototypes and all three unused module entries.
  Three adaptation hunks belonging only to the removed playback body were deleted;
  unrelated palette and skip-table allocation adaptations stay. A complete Native
  reference sweep finds no old code consumers, only one historical trace comment.
- The dispatcher accepts `ScriptThreadId`; numeric Live bindings explicitly convert
  and preserve full-width refusal before any source access. Recording-host callers
  use named ids. Source/assembly provenance is retained in the implementations.
  Core-only typed host operations have no legacy/shim include or call; the remaining
  Live compatibility boundaries and other opcode families are not retired by this.
- On `d2ec7bcb14053f3b3c8a8c54dca30cbf47bc6c99` plus this ownership slice,
  complete Debug/Release builds and regressions pass 4,640 cases / 8,145,061
  assertions and both unchanged canonical replays. Two new cases add 19 assertions;
  removing three entries removes exactly six iterations in two per-symbol checks.
  No baseline changes. Direct address-resolution checks return null for
  `800BF5F0`, `800BF850`, `800BFBB8`; actual imports load 440 globals, 16 pointer
  tables and 1,709 symbols across 75 modules with zero failures.
- Broad changed-source/header quality and clang-tidy has zero new findings; existing
  generated warnings remain. All 190 copier and nine module-extent controls pass.
  The weapon-wait reset control follows both actual native reset and Live setter,
  preserving the named-state/no-old-alias checks. Repeated copying preserves all
  tracked Native/manifest hashes before final provenance/test additions.
  Evidence: `VR-EVENT-LIFECYCLE-PROMOTION` and
  `build\owner-checkpoint\native-vm-lifecycle-promotion-*-regression.log`.
  Reviewed promotion claims cover only these three functions; row 33 and global EC
  retirement remain incomplete. Prerequisite `d2ec7bcb` is green on `42287455543`.
  Unmodified ledger gates derive 2,639 promoted / four compatibility `wip`, with
  row 33 now 7/57. Modernization review markers apply only to the three owned cores;
  no global bridge marker or behavior-verification status was advanced.
  Published lifecycle milestone `be560ec1` is green on exact CI `42291264882`.

### Native Lifecycle Regression Migration (#276 Row 33)

- The existing Game Over initialization, weapon dispatch and branch/return/yield
  cases now exercise production native lifecycle APIs with owned source generations
  and typed cursors. Both normal/skip tables are tested and restored. A shared
  scoped runtime reset keeps ownership cleanup exception-safe. No case is removed
  or renamed; all four native contexts and existing animation/hold/stop expectations
  remain checked. The branch fixture explicitly sets/restores all four instruction
  lengths; its legacy version only specified the yield length and failed full-suite
  ordering after migration because bounded dispatch validates every opcode size.
- Focused Debug reset/weapon/script coverage passes 25 cases / 1,017 assertions.
  On `201e56754ec6734b9e991acd6216e7ccb58ad359` plus this test-only slice,
  Debug/Release test builds and full regressions pass 4,638 cases / 8,145,048
  assertions and both unchanged canonical replays. Scoped quality/tidy has zero
  new findings; existing test debt and the copied-C compiler's delayed-template
  parsing deprecation warning are not suppressed. No baseline changes or formal
  promotion credit. Evidence: `VR-EVENT-NATIVE-LIFECYCLE-REGRESSIONS` and
  `build\owner-checkpoint\native-vm-lifecycle-tests-*-regression.log`.
  Next: reference/data checks and registered ownership for obsolete initialization,
  playback and dispatcher bodies, not another runtime switch.

### Fresh Safety Acceptance Refresh (#313)

- On clean published `e8f45c4e8be8cae81e44e2f2e69ee577348c8c93`, the retained
  frozen-generated-original harnesses were rebuilt against current native cores
  with MSVC x64 `/std:c++latest /O2 /J`. Trigger evaluation passes 100,000
  comparisons (seed `0x7A1663`, all 13 callback categories exercised), room
  load/free passes 40,000 (`0x5EC710`), and cutscene lifecycle passes 40,000
  (`0xC075CE`). All six trigger, seven room and five cutscene mutants are detected.
  These are fresh results, not reuse of historical reports or calls through the
  new compatibility wrappers.
- Retained harness root is the user session-state
  `7b8f0cc4-51d4-4651-bd50-6037b3afeab1\files`. Existing
  `triggers_mutants.ps1 -BaselineOnly -Suite <triggers|room_sections|cutscene>`
  rebuilds/runs each baseline; `-Suite triggers` / `-Suite cutscene` runs their
  mutants, and `room_sections_mutants.ps1` runs the room mutants. Compiler and
  execution limits are 60/30 seconds with owned-child cleanup. Frozen originals
  and harness C++ stay unchanged; mutation copies are isolated under
  `build\owner-checkpoint` and their removal was verified on disk.
- Evidence: `VR-EVENT-FRESH-ORIGINAL-DIFFERENTIAL`,
  `build\owner-checkpoint\{triggers,room_sections,cutscene}-fresh-differential.log`
  and `{triggers,room-sections,cutscene}-mutant-results.txt`. Callback stand-ins
  compare bounded valid state, bytes and order, not actual actor/render/audio
  service output. Unsafe original indices, extents and dispatch loops are excluded;
  maintained native refusal/lifetime tests cover those cases separately.
- Complete Debug/Release builds and regressions at this source checkpoint pass
  4,638 cases / 8,145,023 assertions and both canonical replay fingerprints.
  Scoped quality/tidy and ledger gates pass without baseline refreshes. Exact
  Source Quality `42281621566` is green; #317 is closed on `7267c7ac` plus the
  independent-oracle follow-up `e8f45c4e`. #313 is closed with evidence publication
  `201e5675` and successful exact CI `42283641169`. Formal row-33 promotion remains #276;
  original unused bodies/module identities and global EC retirement are not done.

Frozen original SHA-256 values:

- `triggers_original.inc`:
  `819C2C43E7B8D1D3E489166A213CF6A170989CA5A0919055F97BBC6932E79233`
- `room_sections_original.inc`:
  `8DA47335EF7945FE6C88D006F335FA37FD8834C86F592CBCDC45DBC960EFDF8F`
- `cutscene_original.inc`:
  `234449B412323A4A5F17C9E3B2810B0F4A2CAE0235B993DEC54863F51F430935`

### Independent loop/cache differential (#313)

- Actual loop originals match native stores/decrements over all 16 valid indices,
  every byte value and the `0xFF` sentinel: 8,704 comparisons with signed targets
  kept inside 64-byte storage. Five planted counter/value/target/state mutations
  are rejected. Original ASM `800B8C54-800B8C70` and `800B8C94-800B8CD4` grounds
  the sentinel, byte wrap and signed subtraction. Invalid original indices and
  original dispatch loops are never executed; existing native refusal tests stay.
- Initial loop-oracle failures exposed shared storage, not a production defect:
  the live counter/cache accessors alias the retained C globals. Expected original
  counter results are now snapshotted before restoring inputs for native execution.
  The cached branch corpus likewise snapshots original cache/flow before native
  cache writes, independently checking the byte result. This explicitly supersedes
  the prior cached oracle's shared-state ordering without weakening any expectation.
- Three new cases add 8,711 assertions. Loop plus branch selection passes
  8 cases / 139,789 assertions. On `7267c7ac38ae035114601240b6460236a6c7c9ed`
  plus this test-only follow-up, Debug/Release test builds and full regressions pass
  4,638 cases / 8,145,023 assertions with no failures/skips and both canonical
  replay fingerprints unchanged. Scoped quality/tidy is clean; no baseline changes.
  Evidence: `VR-EVENT-LOOP-ORIGINAL-DIFFERENTIAL` and
  `build\owner-checkpoint\native-loop-differential-*-regression.log`.
  Broader #313 differential/source promotion and #276 gates remain open.

### Unsigned flag branch correction (#317 / #313)

- Fresh branch comparison found a native semantic defect: immediate/pair branches
  sign-extended byte values, and cached equality rejected values above 127.
  Original BATTLE `800B88D4`/`800B8A10` mask bytes and use `sltu`; cached branch
  `800B8BF8` uses `lbu` for both operands. Copied originals compile with `/J`.
  Two focused high-byte assertions failed before the correction; #317 was logged
  before implementation. Native comparisons now preserve every unsigned byte.
- Safe actual-original differential coverage adds 65,536 immediate/pair samples
  across all operation nibbles, full flag IDs and aliases, plus all 65,536 cached
  byte pairs. Signed relative targets stay inside a 64-byte backing array; original
  handlers are invoked once, not allowed to follow loops. Three planted comparison,
  target and operand mutations are detected. All flags/cache state are restored.
  The five-case selection passes 131,078 assertions; existing VM/refusal/routing
  coverage passes 149 cases / 199,839 assertions in Debug.
- The older branch helper and cached test encoded signed assumptions contradicted
  by actual ASM. Their expectations are explicitly corrected, with extra unequal
  cached-byte checks. The regression baseline changes only that cached test's name;
  no assertion floor, replay fingerprint, normalization or quality baseline changes.
- On `b8375c98490cd9feb91f948ad273a2da43856770` plus this correction, complete
  Debug/Release builds and suites pass 4,635 cases / 8,136,312 assertions with no
  failures/skips. Both canonical replay fingerprints match; scoped quality/tidy is
  clean. Evidence: `VR-EVENT-FLAG-BRANCH-ORIGINAL` and
  `build\owner-checkpoint\unsigned-flag-branch-*-regression.log`.
  Semantic fix `7267c7ac` has successful exact CI `42279833831`; independent-oracle
  follow-up `e8f45c4e` is green on `42281621566`, and #317 is closed. This is not
  retail-wide event verification or full row-33 promotion.

### Fresh original flag differential (#313)

- The live VM tests now compare immediate and flag-pair arithmetic directly with
  the retained actual C originals over a deterministic 65,536-case corpus. Every
  byte operation class, all ten-bit flag indices, alias/nonalias pairs, byte wraps
  and safe nonzero divisors are included. Original zero-divisor paths are never
  executed; separate typed tests retain those refusal/R3000-semantics controls.
  Three planted operator/target/operand mutations are detected by the oracle.
- Three new cases add 65,540 assertions. On `e731e2b5` plus this test-only slice,
  both complete builds and full suites pass 4,630 cases / 8,005,230 assertions with
  all baseline tests and both canonical replay fingerprints unchanged. Scoped
  quality/clang-tidy is clean. Evidence: `VR-EVENT-FLAG-ORIGINAL-DIFFERENTIAL` and
  `build\owner-checkpoint\native-flag-differential-*-regression.log`.
  This is fresh arithmetic evidence, not complete VM/trigger differential coverage
  or row-33 promotion; #313/#276 remain open for their remaining gates.
  Published checkpoint `b8375c98` has successful exact CI `42277412014`.
- Production switch `41df09f3` initially failed Source Quality `42274649322` on
  two long lines and formatting in the late room-local inspection condition.
  Layout-only correction `e731e2b5` has successful exact CI `42276114919`.
  #315 is now closed against that revision and its concrete frame/actual-playback
  evidence; #316 remains closed on `86f27ef7`, green `42251517985`. Naming-only
  prerequisite `cfdb4af8` is green on `42268046105`; #273 remains open for its
  complete review/CI decision. No baseline was refreshed or marker advanced.

### Production native VM switch (#313 / #315)

- Production Boot now initializes and updates the native VM; room import/free
  publishes exact native-owned sections. The remaining C trigger entry converts
  actor/tile contexts into the bounded native query, so timer, cutscene, facing-tile,
  encounter and gameplay callers use the same owner. Easing's owned C entry reads
  native custom curves. Canonical scene inspection resolves native generation-owned
  cursors rather than the obsolete raw cursor globals; the room-local fixture also
  reads its offsets from a bounded native door lease.
- Verification found and repaired two frame assumptions before acceptance: missing
  encoded parts use the original base-frame fallback (`800CD574 -> 800CD684`), and
  storage may have an unused slot (stock style 3 reserves 62, while base plus parts
  accounts for 61). Original `800CD5D8-800CD674` transforms every post-base slot;
  part faces may share those initialized extras. The prior intermediate per-part
  restriction test is explicitly corrected to initialized-storage bounds, with
  separate valid shared-slot and unwritten-base controls. Frame selection passes
  14 cases / 33,266 assertions; the frozen original packet oracle is unchanged.
- On `cfdb4af8` plus this functional switch, both complete builds and full suites
  pass 4,627 cases / 7,939,690 assertions with no failures/skips. All original
  baseline names/assertions and both canonical replays are preserved without a
  baseline refresh. Timer and actor-retention fixtures now publish the identical
  synthetic bytes through native ownership; the existing byte-return/no-cast
  copier control follows the owned host. All 190 copier controls and the broad
  changed-source quality/clang-tidy gate pass with zero new findings. Thirteen
  scoped source/manifest/project hashes survive repeat copying/generation before
  the final frame-source review; the frame changes are registered owned code.
- Actual native New Game dialogue captures at 4800/5400/6500 match the earlier
  generated-VM BMPs byte for byte in both configurations. Fresh final Debug/Release
  16,000-tick attract runs load real `EVENT/0020.EVT` at tick 12,581 and advance to
  MAP214. Captures at 8000/13000/15000 are byte-identical between configurations;
  the retained Debug log has no VM, trigger, frame, dialog or crash refusal. The
  failed earlier frame-format runs and bounded diagnostics remain separate local
  evidence; temporary header/part probes and rename scripts were removed.
  Evidence: `VR-EVENT-PRODUCTION-SWITCH`, `VR-FRAME-ORIGINAL-STORAGE`,
  `build\owner-checkpoint\native-vm-switch-*-regression.log`,
  `native-vm-switch-*` dialogue captures and `native-vm-final-evt-*` captures/logs.
- This is production integration, not complete row-33 C++ promotion or global EC
  retirement. Original unused C VM/control bodies and module identities remain;
  F01261/F01276 join F01416/F00853 as `cpp=wip`. Fresh safety-acceptance
  differentials are recorded above; formal row-33 promotion remains under #276.
  #315 subsequently closed after layout correction `e731e2b5` and successful exact
  CI `42276114919`. No full playthrough, physical-controller/audio parity, or
  new behavior markers are claimed. #316 is closed on `86f27ef7`, green exact
  Source Quality `42251517985`; routing `4e430c80` is green on `42246868261`.

### Shared-header naming prerequisite (#273 / #248)

- Production VM validation expanded clang-tidy reach into unchanged typed SEQ and
  audio-reset headers, exposing 30 native naming findings: 18 pose/shadow/enum
  identifiers and 12 reset/channel-kind identifiers. Identifier-only updates
  preserve layouts, enum numeric values, serialized keys and legacy record names;
  the bridge reads/writes retain their original SDK field spellings. No suppression
  or baseline refresh is used. Temporary guarded rename scripts were removed and
  their absence verified.
- Focused Debug sequence/pose/appearance controls pass 131 cases / 5,684 assertions;
  audio reset/control/voice/mixer controls pass 247 / 2,132. The unchanged broad
  `quality-native.cmd --changed --tidy` gate passes with zero new findings after
  these fixes. Validation on `86f27ef7` plus both the naming repairs and pending
  native VM switch passes complete Debug/Release builds, 4,626 cases / 7,939,685
  assertions each, and both original canonical replays without baseline changes.
  Naming cleanup is committed separately from the functional VM switch; these
  are combined-worktree local results, not exact-revision native test claims for
  the naming-only commit. #273's complete review and CI decision remain open.

### Gimmick snapshot and native trigger host (#316 / #313)

- Source review found F00853 `Field_GimmickPlacement` checking an uninitialized
  local before its slot load, with unchecked slot/output arguments. Original ASM
  `func_800A0024.s` branches on inherited `$s0` at `800A0034`, loads the chosen
  pointer at `800A004C` and reads it at `800A0054`; this is not a defined C++ local
  value or a reproduced retail crash. Logged as #316 before the fix; #299 covers
  the separate actor-placement path, not this field snapshot helper.
- F00853 is registered owned strict C++. It validates the slot/output and loaded
  record, preserves attached/falling/moving refusal, and writes a complete local
  snapshot only on success. Named fields and explicit signed packing preserve
  original stores at `800A00A0/00B0/00D0/00DC/00E8`; signed height division truncates
  as the original add-127/arithmetic-shift sequence did. Its C/module entry remains,
  so `cpp=wip` records ownership without promotion/retirement credit.
- `EvaluateLiveRoomEventTriggers` now binds the bounded trigger core to actual
  flag, actor, inventory, collision and repaired gimmick snapshot APIs. It uses no
  actor-query cast for gimmick placement and preserves signed-halfword references
  in context-only comparisons. Production trigger/playback callers are not switched
  yet; this host is the prerequisite for switching producer/evaluator/playback
  together under #313.
- On `4e430c80` plus the snapshot/host/caller/generator/tests, six new cases add
  170 assertions. Snapshot-only controls pass 3 cases / 159 assertions; snapshot
  plus room-trigger selection passes 52 / 655, including real native condition-11,
  context and status consumers. Full Debug/Release builds and suites pass 4,621
  cases / 7,939,613 assertions; every previous case and both canonical replays
  remain unchanged. Scoped quality/clang-tidy, 190 copier controls and ledger gates
  pass; repeat copy/project generation preserves nine scoped hashes. Evidence:
  `VR-GIMMICK-SNAPSHOT-SAFETY`, `VR-EVENT-NATIVE-TRIGGER-HOST`, and
  `build\owner-checkpoint\gimmick-snapshot-*-regression.log`. #316 awaits its fixing
  push and exact CI before closure. #313/#315/#276 remain open.

### Native control-table routing (#313)

- The native VM recognizes the selected normal/skip table's 31 original control
  handler identities and invokes their typed implementations directly. Branches
  and returns remain checked native locations, not integer-cast pointers. Other
  opcode families keep the existing guarded C-slot invocation contract.
- The two initial normal/skip reproductions failed two of six assertions before
  routing. Four new cases add 16 assertions, including the original loop handler's
  16-counter bound and return handling through the native saved location. The
  combined `[event-vm-live],[event-vm],[script-sources],[room-triggers],[session-reset]`
  selection passes 186 cases / 3,785 assertions in Debug. On `3ff2bf3c` plus the
  runtime/test changes, both full suites pass 4,615 cases / 7,939,443 assertions
  with every previous case and both canonical replays preserved. Complete builds
  and scoped quality/clang-tidy pass unchanged baselines. Evidence:
  `VR-EVENT-NATIVE-CONTROL-ROUTING` and
  `build\owner-checkpoint\script-control-routing-*-regression.log`.
- This still does not switch production gameplay from the generated VM. The room
  loader, trigger evaluator and playback caller must migrate together: the current
  legacy trigger evaluator cannot consume only the native room-section owner.
  Native trigger-host integration is the next dependency before that atomic
  switch. No original C control body is retired or promoted by pointer-identity
  routing; #313/#315/#276 remain open. Dialog milestone `3ff2bf3c` is pushed with
  successful exact Source Quality `42244488428`.

### Native dialog retention (#313)

- Native dialog display now resolves bounded halfword-offset tables and validates
  string indices, offsets, control parameters and a real terminator. The table
  convention was checked against the original consumer and an extracted event;
  tests use synthetic bytes only. Invalid native data does not replace existing
  UI text or fall back to unrelated legacy strings.
- A fixed retention set pins immutable source storage rather than box indices,
  so the actual selection routine can swap entire text-box records safely. Eight
  different box owners can be replaced without dropping live bytes; repeated
  generations of the same storage do not accumulate pins. Reinitialization,
  completed dismissal and actual session invalidation clear borrowed pointers
  before releasing their owners. Ordinary VM reset alone preserves open UI text.
- On `72866afd` plus the dialog source/Live/session/tests/header registrations,
  nine new cases add 323 assertions. Debug dialog/reset selection passes 28 cases
  / 589 assertions, including a real title-session transfer with no automatic
  owning object in its abandoned frame. Both complete builds and full suites pass
  4,611 cases / 7,939,427 assertions, with every previous case retained and both
  canonical replays unchanged. Scoped quality/clang-tidy is clean; repeated
  project generation preserves nine scoped source/header/project hashes. The
  shared source-mount test helper preserves the existing vibration assertions.
  Evidence: `VR-SCRIPT-DIALOG-OWNERS` and
  `build\owner-checkpoint\script-dialog-owners-*-regression.log`.
- Limits: production VM loading/control-opcode dispatch and exact native room/file
  producer adoption remain generated. Legacy-only dialog display is unchanged;
  native dialog retention is a verified consumer contract, not retail native-VM
  playback or complete text/render modernization. #313/#315/#276 remain open;
  no new promotion or compatibility-retirement credit is claimed. Native vibration
  milestone `72866afd` is pushed with successful exact Source Quality
  `42239255504`; its predecessor frame milestone `ac6f55db` is green on
  `42237200065`.

### Native vibration consumer (#313)

- The production script-effect host now prefers `LeaseLiveVibrationPattern` and
  passes its exact post-header span to slot-owned playback for both buffered and
  unbuffered script modes. Invalid native bindings and short headers cannot fall
  back to unrelated legacy bytes; malformed streams leave active slots unchanged.
  Playback copies outlive VM reset without retaining the original script owner.
- Three pre-integration controls failed 13 of 78 assertions. The final four new
  cases add 104 assertions, covering source selection, both modes, source release,
  every short-header length, a complete stream beyond the old 120-byte copy
  window, and malformed-stream refusal preserving existing playback.
  `[input-controller],[event-script-effects],[event-vm-live]` passes 78 cases /
  981 assertions in Debug. On `ac6f55db` plus these two source/test files, both
  complete builds and full suites pass 4,602 cases / 7,939,104 assertions, with
  every previous case retained and both canonical replays unchanged. Scoped
  quality/clang-tidy is clean. Evidence: `VR-SCRIPT-VIBRATION-NATIVE-SOURCE` and
  `build\owner-checkpoint\script-vibration-native-*-regression.log`.
- The legacy-only fallback is unchanged: buffered playback still assumes a
  readable 128-byte source record, and unbuffered playback still borrows its
  original pointer through the trusted importer. Production VM loading/dispatch
  is not switched yet, so those producer paths are not retired. No retail crash,
  physical motor, new promotion or compatibility-retirement claim is made.
  Dialog retention and exact native source-producer adoption remain under #313;
  #313/#315/#276 remain open. Frame milestone `ac6f55db` is pushed with successful
  exact Source Quality `42237200065`; its predecessor `81bd2cdc` is green on
  `42232929820`.

### Text-box frame safety (#315)

- F01416 is now an owned, strict-compiled production entry point. Its live
  binding prefers retained native VM leases and bounds compatibility styles to
  known module symbols or game-heap allocations. Invalid indices, unknown source
  storage, malformed geometry and exhausted primitive buffers refuse safely;
  allocation failure is caught at the C boundary. Every packet goes through the
  existing bounded queue; the scratch-point and direct-packet writes are gone.
- On `81bd2cdc` plus the live binding/caller/generator/test changes, the focused
  selection passes 11 cases / 33,210 assertions. The independent original-renderer
  fingerprint is `9c1cb765277a2800`, captured before replacing the generated body.
  Both full suites pass 4,598 cases / 7,939,000 assertions with all previous names
  retained, no failures/skips, and both canonical replays unchanged. Full builds,
  scoped quality/clang-tidy and all 190 encounter-copier controls pass. Repeat
  copying/generation preserves nine scoped source/manifest/project hashes.
- Actual New Game captures in Debug and Release show the opening speech frame.
  With existing local saves, use `1200:Up` before `1300:Circle`; confirming without
  Up enters Continue and is not dialogue evidence. The verified runner command is
  `tools\test\run_native.ps1 -NoCopy -SkipIntro -Config <Debug|Release> -Frames 6800
  -Press '1200:Up,1300:Circle,5000:Circle,5200:Circle' -At '4800,5400,6500' -Speed 8
  -Cap build\owner-checkpoint\text-box-frame-new-game-<debug|release>`.
  Both runs exit 0 and produce all captures; the retained Debug log contains no
  frame refusal or crash. Evidence: `VR-TEXT-BOX-FRAME-LIVE`,
  `build\owner-checkpoint\text-box-frame-live-*-regression.log`, and the two
  `text-box-frame-new-game-*` capture directories. No original-game visual-parity
  or physical-controller claim is added.
- Limits: the compatibility source fallback knows allocation/module capacity,
  not every imported section's logical extent. Production VM loading/dispatch is
  still generated, and the original C entry/module binding remains. Exact native
  producer adoption and remaining UI/dialog/vibration lifetimes stay under #313;
  #315 and #276 remain open. F01416 is `cpp=wip`, not promoted or retired. Baselines
  are unchanged. The component-only checkpoint below is historical.
- The bounded `Menu::DecodeTextBoxFrame` and `DrawTextBoxFrame` component now
  owns decoded geometry, validates all parts/faces before drawing, refuses
  backward outline resumes and uses `Render::PacketQueue` for every packet.
  Native point storage supports all 255 encoded vertices without scratch RAM.
- On `615c4b87` plus the frame implementation, tests and generated projects,
  `[text-box-frame]` adds 8 cases / 15,351 assertions. This includes
  1,200 deterministic comparisons against the current production renderer's
  valid-domain packet output, all four alignments, both optional parts,
  triangles/quads, brightness, signed dimensions and every small-buffer cutoff.
  Scoped quality/clang-tidy is clean; full Debug/Release builds pass. Both full
  suites pass 4,595 cases / 7,921,141 assertions with no failures or skips.
  `regression.py check --config Debug` and `--config Release` each preserve the
  previous tests and both canonical gameplay replays with zero regressions.
  Evidence: `VR-TEXT-BOX-FRAME-CORE` and
  `build\owner-checkpoint\text-box-frame-release-regression.log`.
- At published prerequisite `81bd2cdc`, this component was not a production caller.
  The generated frame renderer remained unchanged for direct comparison. Production
  adoption must supply exact owner-backed style bytes, including default module
  styles, preserve retained UI lifetimes, and migrate the caller through ownership
  registration or the copier. #315, #313 and #276 remain open; no promotion or
  compatibility-retirement credit is claimed. Quality baselines are unchanged.

### Resumed row-33 work

- The owner resumed implementation and authorized commits/pushes at verified
  milestones. #276 remains active. Current promotion: **2,635 / 3,749 done,
  1,114 remaining**; 41 / 54 plan rows done. Row 33 has 4 / 57 functions
  promoted and remains planned. The pause-era counts below are historical.
- Native dialog/frame/vibration data now has checked, owner-backed lease
  accessors. Rebinding or resetting the VM does not revoke a consumer's old
  bytes; absent slots and invalid indices/locations are refused. The native
  room importer prepares an entry-source token before changing flags or room
  state, then publishes without allocation. Continuing room transitions keep
  the old event/door sources; freeing the current room does not revoke them.
  Nested opcode dispatch also has a direct invocation-restoration control.
  Seven new cases / 103 assertions pass on `812be6c5` plus these changes.
  Debug/Release focused selection passes **227 / 3,708**; full suites pass
  **4,587 / 7,905,790**, retaining all 4,580 previous names and unchanged
  complete replay objects. Evidence: `VR-EVENT-DATA-OWNERS`,
  `build\owner-checkpoint\script-data-leases-*`. Source quality/clang-tidy and
  the final factored test scope are clean; no assertions were removed to meet
  test complexity limits.
  **The new importer and data accessors are not yet gameplay callers.**
  Existing production room loading still delegates to the generated VM.
  Dialog/frame payload validation, UI retention and vibration consumers remain
  unwired; no allocation/generation-exhaustion injection, retail native-VM
  playback, promotion or compatibility-retirement credit is claimed.
- The concrete `ScriptVmLive` adapter now compiles and has direct runtime
  controls: typed cursors/returns and opcode flow, persistent source/data
  owners, private main/chained disc destinations, publication only after
  completion, and cancellation before storage release. Disc allocation
  generations distinguish slot reuse even when cancellation clears the
  destination. New door events bind the current room; ongoing door execution
  keeps its previous source. The real Boot session boundary resets native
  state after invalidating UI borrows and abandons retained uses immediately
  before `longjmp`; a native-dispatch-to-title control verifies owner release.
- Verification on `f600c1a9` plus the runtime, queue, startup, session and
  scaffold changes: **14 new cases / 121 assertions**. Debug/Release selection
  `[event-vm-live],[event-vm],[script-sources],[room-triggers],[disc-queue],[session-reset]`
  passes **220 cases / 3,605 assertions**. Both full suites pass **4,580 cases /
  7,905,687 assertions**; all 4,566 previous names and complete replay objects
  are unchanged. Full builds, scoped quality/clang-tidy and gates pass; two
  generations preserve all **1,662** source/project hashes.
  Evidence: `VR-EVENT-NATIVE-RUNTIME`, `build\owner-checkpoint\script-vm-live-*`
  and `script-door-binding-*`.
  Published as `812be6c5`; exact Source Quality `42219789329` succeeded.
- This is **not the production VM switchover**. The generated VM and trigger
  evaluator still drive gameplay. The native opcode table/callers, room hosts,
  dialog/frame/vibration consumers and fixtures still need migration before
  row-33 promotion. Native file publication tests complete synthetic request
  destinations; they do not prove retail EVT playback. Allocation/generation
  exhaustion injection is not claimed. No new promotion or retirement credit.
- Source publication can now be prepared before the room host's nonallocating
  commit boundary. Move-only tokens preserve the current mount until
  publication, belong to their originating mount set, and can be consumed
  only once. Reset invalidates outstanding preparations without reusing
  their generations. Tokens must not outlive their originating mount set.
  Six new cases / 51 assertions pass; the combined
  `[script-sources],[event-vm],[room-triggers]` selection passes **145 cases /
  2,965 assertions** in Debug/Release. Both full suites pass **4,566 cases /
  7,905,566 assertions**, with all 4,560 previous names and complete replay
  objects unchanged. Full builds, scoped quality/clang-tidy and gates pass.
  Evidence: `VR-EVENT-SOURCE-PUBLICATION`,
  `build\owner-checkpoint\script-source-publication-*`. This prepares the
  integration contract; the production VM and room host are still unwired.
  Published as `f600c1a9`; exact Source Quality `42213631606` succeeded.
- `ScriptSourceMounts` now owns the three versioned source mounts and retains
  every exposed generation through explicit nested use boundaries. Repeated
  reads retain a generation only once; ordinary reset invalidates mounts but
  leaves active byte views alive. Abandonment is separate and only valid once
  users have unwound or immediately before a non-returning transfer. The
  serial survives both operations. The native adapter now brackets reads and
  the actual session boundary abandons uses; production VM/UI migration is
  still pending as described above.
- Mount verification on `55c7da02` plus the three ScriptSources source/test
  files: nine new cases / 185 assertions, including an actual `setjmp` /
  `longjmp` control with ownership in persistent heap state and no automatic
  owning objects in the abandoned frame. `[script-sources]` passes 17 cases /
  271 assertions; combined `[script-sources],[event-vm]` passes 96 / 2,482 in
  Debug and Release. Both full suites pass **4,560 cases / 7,905,515
  assertions**, retaining all 4,551 prior names and unchanged complete replay
  objects. Full solution builds, scoped quality/clang-tidy and static/ledger
  gates pass. Evidence: `VR-EVENT-SOURCE-MOUNTS` and
  `build\owner-checkpoint\script-mounts-*`. No production VM migration or
  promotion/retirement credit; allocation-failure injection is not claimed.
  Published as `40047346`; exact Source Quality `42209961570` succeeded.
- Buffered script vibration now validates its 120-byte stream window and
  stores a separate immutable copy in each native controller slot. New or
  malformed requests cannot overwrite a playing pattern through the former
  shared scratch buffer. Copies and pauses retain ownership; clear, reset
  and natural completion release it. Restarting a view of slot-owned bytes
  retains that owner and exact subview before replacement, including when
  moving playback to the other slot.
- Verification on `5e0003f9` plus the eight Input/Event source/test changes:
  three production-opcode reproductions failed eight of fourteen assertions
  before repair. The additional alias-lifetime control failed four of
  fourteen assertions before correction (#313 comment `6683012020`).
  `[input],[event-script-effects],[script-sources]` passes **84 cases /
  968 assertions** in Debug and Release. Both full suites pass **4,551 cases /
  7,905,330 assertions**; all 4,538 previous names remain and the complete
  Debug/Release replay objects are unchanged. Thirteen new cases add 213
  assertions, including frame-for-frame owned/borrowed motor equivalence.
  Scoped quality/clang-tidy and static/ledger gates pass. Two generations
  preserve 1,659 source/project hashes (including the solution). Evidence:
  `VR-SCRIPT-VIBRATION-OWNERSHIP`,
  `build\owner-checkpoint\script-vibration-ownership-*`, and
  `build\owner-checkpoint\script-vibration-alias-before*`.
- Vibration limits remain explicit: the generated producer still assumes a
  readable 128-byte buffered record; source-owner extent validation belongs
  to the pending VM integration. Unbuffered playback and the trusted raw
  4,096-byte compatibility importer remain borrowed. The bounded overload
  supports actual extents beyond that window. This fixes synthetic
  production-opcode reproductions, not a reported retail crash or physical
  controller test. No new promotion or compatibility-retirement credit;
  #313 remains open and production VM dispatch is still generated.
  Published as `55c7da02`; exact Source Quality `42206619195` succeeded.
- The new `ScriptSource` value retains a buffer or immutable room section
  behind an exact byte span and a fresh, nonwrapping generation. Copies pin
  the old source during replacement; moves revoke the moved-from view.
  Preparation errors are explicit and leave the generation unchanged.
  This is an ownership component, **not yet the production VM adapter**:
  no new promotion or compatibility-retirement credit is recorded.
  Eight cases / 86 assertions cover ownership, replacement, same-address
  remounts, invalid locations, empty sources, moves and generation exhaustion.
  Debug and Release full suites pass **4,538 cases / 7,905,117 assertions**;
  all 4,530 previous names and both replay objects are unchanged. Scoped
  quality/clang-tidy is clean; two generations preserve 1,658 source/project
  hashes. Evidence: `VR-EVENT-SOURCE-LEASES` and
  `build\owner-checkpoint\script-sources-*`. Published as `5e0003f9` with
  successful exact Source Quality `42200489658`.
- Remaining borrower obligations are concrete: text-box selection swaps
  complete records, so per-index string leases alone are insufficient.
  Controller vibration retains input spans, and its raw importer assumes a
  4,096-byte scan window. Production integration must use actual source
  bounds and retain or revoke these borrows before dropping their owners;
  #313 comment `6682106906` records the source evidence, not a retail crash.
- Room/encounter imports now carry `Event::RoomEventLoad` through Field rather
  than interpreting an error integer as a started event. Shared validation
  rejects oversize sections before the legacy boundary narrows lengths:
  trigger/door data above 65,535 bytes and entry data above 6,144 bytes.
  Continuations discard unused entry input before validation, as the core
  already does. The obsolete C encounter-load hook is replaced by a typed
  `VS::Game` entry point.
- Field refusal releases the room sections and read file without placing
  actors, starting the camera/play, or advancing new-game entry mode. A checked
  result now reaches the room, zone and outer gameplay loops: the existing
  title reset runs before any frame, and the loops stop if that host returns.
  The former void caller would otherwise run against the refused room.
- Verification on `55a43d06` plus the scoped Event/Field/Boot changes:
  the two initial failure-injection cases failed nine of thirteen assertions.
  Seven added cases contribute 47 assertions; one existing loop test now
  also checks its success result. The combined
  `[field-room-loading],[room-triggers],[event-vm],[session-reset],[boot-gameplay-loop]`
  selection passes **254 cases / 3,386 assertions** in Debug and Release.
  Both final full suites pass **4,530 cases / 7,905,031 assertions**; all 4,523
  prior names and both complete replay objects remain unchanged.
- Scoped quality/clang-tidy is clean. The first pushed room-load revision
  `f3ce984c` failed Source Quality `42194816527`: two encounter-copier tests
  still searched for the old `void FinishRoomLoad` signature. Their prior
  local run preceded the final bool-return change. The correction updates
  only those signature expectations, preserving both ordering checks. All
  190 encounter tests now pass against the final bool-return API, with clean
  scoped Python quality. Correction `141e87c3` has successful exact Source
  Quality `42195383745`.
  Two generations preserve 1,656 source/project hashes. Evidence:
  `VR-ROOM-EVENT-FAILURE-CHAIN`, `build\owner-checkpoint\room-event-failure-*`,
  and #313 comments `6681095636`, `6681134039`, `6681303616`.
  Live extent refusal and recording-host failure/reset paths are covered,
  not a retail malformed-room recovery. The legacy loader still owns the
  actual room/VM buffers; allocation failures there are not made transactional
  by this boundary. Native source ownership and production dispatch migration
  remain required before row-33 promotion.
- Published prerequisites: `cef090ce` dispatch guards have successful exact
  Source Quality `42189695444`; `55a43d06` disc lifetime changes have successful
  exact Source Quality `42190451967`. #314 is closed; #313 and #276 remain open.
- #314's shared-disc prerequisite now exposes terminal queue errors and
  distinct refused/failed/completed native reads. The integer blocking
  adapter also returns success only on completion (its two production
  callers discard the result); queue-exhaustion fallback cannot mark failed
  data Loaded. Deferred command failure is published to its own request
  before clients poll, then the drive is released for the next request.
- `CancelLiveDiscWrites` is a game-thread operation for writes starting
  exactly at the supplied span's address, not every subrange of an allocation.
  It stops the deferred write before changing matching pending records,
  clears borrowed destinations, repairs priorities/counts and preserves
  terminal handles for release. Unrelated destinations and XA playback are
  untouched. Completion requires the same read generation and actual
  delivery, not merely an idle drive. Legacy release is still not
  cancellation; #313 must call the new API before dropping a pending owner.
- Disc verification on `cef090ce` plus the seven Content source/test changes:
  four original reproduction cases failed nine of sixteen assertions; the
  Live blocking adapter additionally failed one of five before correction.
  **23 new cases / 150 assertions** pass in both configurations, including
  actual Live deferred commands, fallback records, repeat cancellation and
  recovery. Combined disc/event coverage is **196 cases / 5,438 assertions**.
  Full Debug/Release solution builds and suites pass **4,523 cases /
  7,904,983 assertions**, with both complete replay objects unchanged.
- Scoped quality/clang-tidy and static/ledger gates pass; two generation runs
  preserve 1,656 source/project hashes. Evidence: `VR-DISC-FAILURE-CANCELLATION`,
  `build\owner-checkpoint\disc-lifetime-*`, `disc-failure-before*`,
  `disc-blocking-before*`, and #314 comment `6680692023`. Full-suite validation
  corrected an invalid test assumption: an unsuccessful read may modify its
  destination, so the test checks a guard byte outside the requested extent
  rather than asserting failed data stayed unchanged.
- Limits: this does not migrate the production VM, make all callers respond
  to read failure, or establish retail failure recovery. The current native
  read completes synchronously inside deferred command service; a genuinely
  asynchronous backend would require persistent request identity, not this
  per-update host identity. No promotion or compatibility-retirement credit.
- The VM now refuses a thread after exactly 65,536 non-yielding dispatches
  per frame, including cross-region jumps. Hold/yield/stop/finish on the last
  allowed instruction remain valid; each thread/frame receives a fresh
  budget. Context starts, saved returns and cursor advances check offset
  overflow before publication. This is an intentional malformed-script
  refusal limit, not a measured maximum for every retail script.
- Dispatch-guard verification on `253edb44` plus the three VM changes:
  three pre-fix cases failed all ten assertions. Seven added cases contribute
  48 assertions; `[event-vm],[room-triggers],[cutscene-lifecycle]` passes
  **141 cases / 5,066 assertions** in Debug and Release. Full suites on the
  combined dirty tree (also containing the pending #314 disc changes) each
  pass **4,523 cases / 7,904,983 assertions**, with both replay objects
  unchanged. All 4,493 previous case names remain; the combined addition is
  30 cases / 198 assertions. No promotion markers changed.
- Evidence: `VR-EVENT-SCRIPT-DISPATCH-GUARDS`,
  `build\owner-checkpoint\vm-budget-*`, `disc-lifetime-*`, session
  `files\vm-budget-before-baseline.json`, and #313 comment `6680071363`.
  Scoped quality/clang-tidy is clean; two copier/scaffold passes preserve all
  1,656 inspected source/project hashes. Production VM integration and its
  source ownership remain unfinished.
- Ownership/generation checkpoint `13b0e482` is pushed; exact-revision
  Source Quality `42181532222` succeeded. Failure-contract checkpoint
  `253edb44` is also pushed with successful Source Quality `42183675692`. It checks
  buffer allocation, queueing, read status, chained adoption and complete
  header initialization. Failed startup reports its reason, clears borrowed
  views/cursors, releases actor/camera control and returns to idle instead of
  hanging in Loading or entering Running with refused data. Chained refusal
  preserves the previous selector, discards unqueued storage and permits retry;
  another outstanding request is not overwritten.
- Header validation now prepares all views before publication; one malformed
  table refuses the complete header rather than exposing a partially valid
  script. Header-relative arithmetic is widened before addition. Direct
  header/queue entry refuses an outstanding request without discarding it.
  Repeat initialization and interrupted startup require synchronous
  cancellation before releasing the destination. **Live must implement that
  contract:** freeing a busy disc queue slot alone does not stop pending writes.
- Verification on `13b0e482` plus the three scoped source/test changes:
  three pre-fix cases failed six of eight assertions. The final addition is
  19 cases / 161 assertions; `[event-vm],[room-triggers],[cutscene-lifecycle]`
  passes 134 cases / 5,018 assertions in Debug and Release. Full suites each
  pass **4,493 cases / 7,904,785 assertions and both unchanged replays**.
  Scoped quality/clang-tidy is clean. Evidence:
  `VR-EVENT-SCRIPT-FAILURE-CONTRACTS`, `build\owner-checkpoint\vm-failure-*`,
  session `files\vm-failure-before-baseline.json`, and #313 comments
  `6679731263`, `6679770807`, `6679949962`. This is failure-injection/core
  evidence, not a production cancellation or retail failure reproduction.
- Remaining before the Live switch: wire the bounded core and actual request
  cancellation, mount/pin sources through callbacks
  and transitions, propagate failures through Field/Boot, and migrate all
  callers. Promotion counts remain unchanged; #313 and #276 stay open.
- The published ownership checkpoint adds immutable, shared room-section owners
  and explicit load results. Preparation is transactional: allocation/extent
  failures publish nothing and do not clear flags or alter playback. Executing
  door leases survive room replacement/free; borrowed old input is copied
  before publication. Empty loads retain the previous main script, and a
  continuation does not inspect/copy the unused entry script.
- Legacy import limits are explicit: trigger/door sections at most 65,535
  bytes, entry script at most 6,144 bytes; excess is refused rather than
  narrowed or copied past the old buffer. This is not a new authoring format.
  Allocator-failure tests inject the host's preparation failure; they do not
  exhaust operating-system memory.
- Script locations now carry 64-bit mounted-source generations. The core
  refuses stale dispatch, headers, skip scans, saved returns and context
  starts before reading replacement bytes, even at an unchanged address.
  Derived views/jumps retain generations; explicit jumps to fresh sources and
  completion after source release remain valid. The pre-fix reproduction
  failed two of five assertions (`vm-lifetime-before.log`). Generation checks
  do not own memory: Live must pin mounted sources through callbacks and room
  continuation, and clear borrowed legacy views before releasing owners.
- Verification on `be48f8bf` plus the six scoped source/test changes:
  26 new lifetime/ownership cases add 152 assertions. The combined
  `[event-vm],[room-triggers],[cutscene-lifecycle]` selection passes 115 cases /
  4,857 assertions in Debug and Release. Full suites each pass **4,474 cases /
  7,904,624 assertions and both unchanged replays**. Loader/free recording-host
  differentials make 40,000 comparisons (seed `0x5EC710`), detecting seven
  mutants; the 100,000 trigger comparisons still pass. Corrected ownership and
  malformed/allocation cases are covered by explicit tests, not equated to
  the original's leaking/dangling allocations.
- Scoped clang-tidy/default quality is clean. Evidence:
  `VR-EVENT-SCRIPT-SOURCE-LIFETIME`, `VR-EVENT-ROOM-SECTION-CORE`,
  `build\owner-checkpoint\room-ownership-*`, `room-sections-*`,
  `vm-lifetime-*`, and session `files\room_sections_diff.cpp`,
  `room_sections_original.inc`, `room_sections_mutants.ps1`,
  `room-ownership-before-baseline.json`. Production integration is still
  pending: neither these owners nor source generations protect the generated
  runtime yet. Finish VM allocation/I/O error paths, mount/pin the native
  sources, propagate failed event loads through Field, and migrate all callers
  and borrowed fixtures before promotion.
- The bounded `Event/RoomTriggers.cpp` evaluator is compiled and tested
  alongside the generated production trigger path. Its actor-pair, tile and
  no-context variants replace pointer-encoded context in the core; offset
  tables, headers and all 16 conditions are bounded. Missing placements and
  malformed/unterminated records refuse explicitly. This is **core-only**:
  no additional promotion or compatibility-retirement credit is claimed.
- Trigger-core verification on `76da31a5` plus these scoped changes:
  `[room-triggers]` adds 27 cases / 316 assertions; combined with
  `[cutscene-lifecycle],[event-vm]`, 89 cases / 4,705 assertions pass in both
  configurations. The frozen-generated-original recording-host differential
  makes 100,000 comparisons (seed `0x7A1663`), with zero mismatches and six
  detected mutants. It covers valid bounded records, not malformed original
  accesses or actual actor/render/audio services.
- Debug and Release solution builds and full regression recordings each pass
  **4,448 cases / 7,904,472 assertions and both unchanged replays**. The baseline
  adds exactly the 27 new tests and 316 assertions; no old test or replay
  expectation changes. Evidence: `VR-EVENT-ROOM-TRIGGER-CORE`,
  `build\owner-checkpoint\triggers-*`, and session `files\triggers_diff.cpp`,
  `triggers_original.inc`, `triggers_mutants.ps1`, and
  `triggers-before-baseline.json`. Scoped clang-tidy/default quality is clean.
- Trigger integration still requires owned section lengths, repeated-load/free
  handling and explicit load-failure propagation through `Field/RoomLoading`.
  The original continuation opcode's bit 4 preserves VM cursors while room
  unloading frees door storage; executing door data must remain pinned across
  that transition. New source generations must reject stale cursors after
  replacement/reset. This source-backed lifetime construction is recorded on
  #313; no retail reproduction is claimed. Do not wire the evaluator through
  guessed spans or replace door ownership independently of VM lifetime.
- The complete four-function cutscene file is now the typed
  `Event/CutsceneLifecycle.cpp` core and `CutsceneLifecycleLive.cpp` binding.
  Generated and maintained callers use the native entries. The old C ABI,
  header and four unused original-address module entries are gone
  (9 source consumers to 0). Player preparation failure is explicit, actor
  state is re-read after callbacks, and music handles remain bounded and
  32-bit. Sound reset uses a value-initialized typed request.
- Cutscene verification on `2ed487e1` plus these scoped changes:
  `[cutscene-lifecycle]` passes 22 cases / 2,448 assertions; the combined
  cutscene/event-VM/actor-animation selection passes 113 cases / 4,703
  assertions in Debug and Release. A frozen-generated-original differential
  makes 40,000 comparisons (seed `0xC075CE`); five planted mutants are detected.
  Callback stand-ins compare state and ordering, not audiovisual output.
- Debug and Release x64 solution builds and
  `python tools\test\regression.py record --config <Debug|Release>` each pass
  **4,421 cases / 7,904,156 assertions and both unchanged replays**. All old
  test names remain. The exact assertion delta is +2,448 from new tests minus
  eight checks in two existing per-module-symbol tests for the four retired
  entries. No replay expectation changed. Logs: `build\owner-checkpoint\cutscene-*`;
  detailed commands/limits: `VR-EVENT-CUTSCENE-LIFECYCLE` and
  `VR-EVENT-CUTSCENE-REPLAY`. Scoped clang-tidy/default quality is clean;
  copier/scaffold second runs preserve all 1,691 native/project/manifest files.
  Generator controls pass (190 encounter checks with the repository's
  `.venv-3` Python, 9 module-extent checks, 94 ledger-gate checks).
- The recovered typed VM core is now `Event/ScriptControl.cpp`, with the
  `Event/ScriptVm.hpp` interface. It is compiled and tested alongside, not in
  place of, the generated production VM. Script locations are region/offset
  values and dispatch outcomes distinguish control codes from jumps.
  Rejected headers clear old dialog/style/vibration views.
- Core-only verification on `1bcfa1f0` plus this checkpoint's changes:
  `[event-vm]` passes 40 cases / 1,941 assertions. Debug and Release x64 builds
  and `python tools\test\regression.py record --config <Debug|Release>` each pass
  4,399 cases / 7,901,716 assertions and both replays. The recorded baseline
  change adds those 40 test names/assertions only: all earlier names are retained,
  and both configurations' replay expectations are identical to the pre-run
  baseline. Logs: `build\owner-checkpoint\resume-vm-*`.
- Scoped `quality-native.cmd --files <core/header/tests> --tidy --jobs 2`
  reports zero new/actionable findings; gate and modernization checks pass.
  No ownership or promotion credit is claimed for this core-only checkpoint.
- The core-only checkpoint was published as `2ed487e1`; its exact-revision
  Source Quality run `42166879061` succeeded.
- Next: finish the production VM Live adapter and bounded room triggers,
  retarget every remaining caller, run the original differential/mutation
  controls and all row acceptance gates, then publish and close only what passes.
  #313 and row 33 remain open/planned. The original draft backup is retained.

### Owner-pause publication checkpoint

- At this earlier checkpoint the owner paused implementation; work has since
  resumed as recorded above. #247 is closed; #276 remains the continuation tracker.
- Branch `imp/01-ma04-gates`: `c83d7772` publishes the #288 collision hazard scanner
  controls; `34cdf8b7` publishes the integrated native-system promotions, their
  generated callers, ownership, project integration, tests and ledgers. This
  accumulated checkpoint spans interdependent systems; it is not one small slice.
  [PROMOTION_PLAN.csv](ledgers/PROMOTION_PLAN.csv) records the publication commits.
- Promotion status: 2,631 of 3,749 functions done (70.2%), 1,118 remaining;
  41 of 54 plan rows done. Remaining rows: 27, 28, 29, 30, 32, 33, 35, 37, 39,
  47, 48, 50 and 52. The 763 tracked hazard sites are not all resolved.
  Promotion, behavior verification and compatibility retirement remain separate
  gates; these counts do not mean the game is fully verified or mod-ready.
- The unfinished row-33 event VM source and header are preserved outside the
  repository in the current session's `files\owner-pause-row33` directory. They
  were not integrated, built or published. The published source retains the
  pre-draft VM matching the copier manifest; #313 remains open.
- Fresh local validation of the source published as `34cdf8b7` (before commit,
  based on `77591989` with the integrated changes, excluding the row-33 draft):
  Debug and Release x64 solution builds pass; `python tools\test\regression.py
  check --config <Debug|Release>` passes 4,359 cases and 7,899,775 assertions in
  each configuration, with no failures or skips. `silver_wolves` (2,400 frames)
  and `first_lever_room` (900 frames) match the existing replay baseline in both.
  The replay expectations were not changed.
- `gates.py --check`, `modernization.py --check` and `hazards.py --check` pass.
  Python unittest discovery passes for `tools\port` (223), `tools\ledger` (199),
  `tools\lint` (30) and `tools\naming` (39). `quality-native.cmd --changed --jobs 2`
  reports zero findings/new findings, with one separately classified inapplicable
  C++ header advisory; this is not a fresh full clang-tidy run.
  A second copier run leaves all 512 generated outputs unchanged.
  `python tools\port\check_event_dispatch.py` checks 179 handlers and both tables
  with zero errors. Local logs are under `build\owner-checkpoint`.
- Exact fixing-revision CI:
  [Source Quality, 34cdf8b7](https://github.com/JEschete/VS-NativePort/actions/runs/42161727516).
  Issue closures must cite a successful exact-revision run and their specific
  acceptance evidence. CI does not run the game. The two replays do not replace
  menu, chest/lock, off-floor-room or interactive-combat verification.
- Remaining explicit limits include chest/lock interactions (#287), damage
  reaction blend-frame semantics (#296), the still-generated particle emitter
  (#271), and the event VM (#313). Broad quality and content-safety issues stay
  open where their complete acceptance evidence is still missing.

### Earlier published runtime evidence

- Published: 0.0.10 from `157f9303`. 0.0.11 follows it with the two changes below.
  0.0.11 adds `chdr.dll`; no real self-update to it has run yet.
- #254 first-time setup: `Asset conversion did not complete: MOV/TITLE.STR` meant
  the selected FFmpeg had no `libsvtav1`, usually Gyan's "essentials" build; two
  reporters confirmed that a different build fixed it. Setup now checks for an AV1
  encoder before extracting, falls back to `libaom-av1`, and the extractor stops if a
  conversion fails. `RunProcess` now appends: each setup step used to overwrite
  `setup.log`, which hid the reason. Testers' guidance is in
  [TESTER_RELEASE.md](TESTER_RELEASE.md). Limit: the fallback was tested with a
  stand-in that hides `libsvtav1` from FFmpeg 8.1.1, not with an essentials build.
- Disc images: the launcher and `VS.Extract` accept a single-track CHD through
  libchdr, verified by hashing the decoded sectors. The launcher also accepts a BIN
  without a CUE and a CUE whose BIN name is stale; the checksum alone decides. CHD and
  BIN extractions match file for file, apart from the movie's random container ID
  and the recorded source path.
- Modernization tracking on `imp/01-ma04-gates`: [MODERNIZATION.md](MODERNIZATION.md)
  tracks all 3,760 port targets, and [PLATFORM_HAZARDS.md](PLATFORM_HAZARDS.md) logs
  sites that may rely on what PS1 hardware tolerated, including the capacity limits
  that block inventory expansion. CI fails on a new hazard site until it is fixed or
  acknowledged. `tools\test\regression.py check` must pass locally before merging,
  because CI does not build. At that earlier checkpoint, tests or replays executed 1,600 functions;
  2,160 need characterizing first. Replays step the frame counter, and two runs match
  on every field, as do Debug and Release, once script addresses become offsets. Copied
  code now keeps null and overflow checks in Release. Limits: replays cover only two
  scenarios, carrying has none, and coverage shows a function ran, not that its result
  was checked.
- #252 carrying a box: finished steps, falls and reactions store `animationSet`
  0xFF to reinitialize the next clip, and the carry walk's morph events set
  `poseOverrideActive`. The bound lookup added for #226 compared that marker with
  the resolved set, so the unwind failed and `Actors_PlayActorAnimation` refused
  every later animation. Ashley stayed in the carry walk until he stopped
  responding (504 `cannot unwind` errors at the reported log). The marker now
  counts as still bound; a clip bound to another set is still rejected. The owner's
  live zone9/room11 Debug playtest on the fix: 2,034 carrying frames, 7 recoveries
  from the marker, including the carry-fall clip, 0 errors, still responsive.
  Limits: neither log shows Ashley below floor level, so the reported fall is
  closed on the owner's playtest, not on a measured depth.
- Launcher updates: the launcher checks GitHub's latest release on open and from
  **Check for updates**, then asks before installing. Installs need a numeric
  `release.json` version, GitHub's SHA-256 for the exact
  `VS-NativePort-<tag>-win-x64.zip` asset, a matching file manifest and a staged
  `--self-check`. Swaps are journalled in `update-backup` and roll back on failure
  or at the next launch. Copies older than the first updater release, including
  0.0.8, must update by hand once. Releases need numeric tags and unrenamed ZIPs.
  Published as 0.0.9 from `fae5e12c`. The live UI against the published release
  passes on disposable copies of the package marked 0.0.8: full update and restart
  (28 of 28 files match), a locked file rolling back, retry after that failure, and
  rollback of a simulated interrupted swap. A build without a numeric version is
  only offered the page link. Limits: tested on the development PC only, and no
  true release-to-release update has run yet; 0.0.9 to the next release is the
  first. A power loss between the two renames of a file the launcher needs to start
  cannot self-recover, so the tester guide says to re-extract the ZIP.
- #251 indestructible block crash: gimmick actors have no `Actors_actorRecords`
  entry, and F00818 reads through that NULL record on an impact4 hit. The owned
  `Battle_ApplyActionTargetEffects` now skips the defense pose when the record is
  NULL. The live preset is zone9/room11, production room events, Ashley1,2, with
  `--press 104:Circle,140:Circle,175:Circle,300:Circle,335:Circle,370:Circle`.
  Before the fix, Debug run 1793448318598283-0 faults at tick222. After it, Debug
  1793448357564932-0 and Release 1793448623687406-0 resolve both block strikes
  (222, 382) and return control. Crate control 1793448405042444-0 still destroys
  box6 and drops box7. Single-target spells and blocks in other rooms were not
  driven live.
- Publication checkpoint: first-use CUE/save/FFmpeg fields are now all blank;
  only user-selected paths in that user's local configuration are restored.
  Detection is explicit. Packager rejects personal configuration/logs and any
  payload containing the builder's profile path. No developer profile path was
  found in the runtime executables or bundled libraries.
  Final Debug/Release selections each pass56 cases/217,169 assertions.
  #245's production Unicode synth, streams, XA and movie controls pass without
  changed expectations; the game binary remains the same as the r2 packaged
  startup/exit control. Close #245 only with the fixing revision and green
  exact-revision CI; do not convert the separate r3 GPU-health limit into a pass.
  Owner authorized scoped commit/push and then creation/switch to
  `imp/01-ma04-gates`. IMP01/#46 implementation has NOT started; do not claim
  or edit its acceptance as part of this launcher checkpoint.
- Launcher r3 follow-up from base `e103a044`: release input is now
  fail-closed against the exact approved750,643,152-byte USA BIN SHA256
  `25221D7F8FDB2C4490AF12D9E82F0E58BDD6ACB43387F9082B3C5E79DFE090C0`.
  CUE order/layout/reference and complete BIN bytes are checked before cache,
  extraction or Play; image write/delete sharing is denied through setup.
  Cache provenance is now version2 with the verified hash; old caches rebuild
  once. A forged cache marker, wrong size and correctly sized wrong bytes have
  explicit negative controls. SHA256 primitives use known-vector/read-lock tests.
  The GUI is a DPI-aware dark/gold three-card setup flow with inline errors,
  verification progress, disabled Play until verified, keyboard focus, resize/
  scroll behavior and native pickers. 100/150-percent layouts and invalidation
  after changing the CUE were visually/control checked. No game assets are used
  in the launcher design.
  Debug and Release focused selections each pass55 cases/217,164 assertions;
  source quality is zero findings. Candidate:
  `build\releases\VS-NativePort-alpha-2026-10-30-r3-win-x64.zip`,
  SHA256 `94BDB315C95D4ECE6905185503B7DF3846E00347BAE5E5FC36F1F612512BA2B5`.
  Actual packaged verification and extraction produced11,405 assets with the
  approved hash in version2 provenance; bank import completed83 banks.
  Subsequent game-window smoke is NOT accepted: its initial input-idle check
  saw no visible game window, and the harness stopped only its owned processes.
  Windows recorded NVIDIA TDR resets at20:37:34..36 and Nexus.exe/nvml.dll crash
  at20:37:40. Cause is unproven; do not blame or change the game/driver from this.
  The game binary is identical to r2 (SHA256
  `59821119CF5DA6184767554D4BB772A5541273851F2D5358878EDD3574E77AC1`).
  Further game launches are paused rather than ignoring the health event.
  Owner was unavailable; finish the local ZIP with this limit explicit, then
  observe a stable driver session and a bounded visible-window startup before
  claiming fresh game-smoke acceptance. Do not retry launches in a loop.
  Evidence: `build\quality\launcher-r3-*.log`,
  `build\launcher-design-check\*.png`. Publication evidence is tracked on #245;
  no GitHub release was published.
- Local tester-launcher delivery from `e103a044` plus uncommitted changes:
  `VS.Launcher` uses native Windows CUE/save-folder/FFmpeg pickers and Play,
  remembers paths in per-user `launcher.json`, validates referenced BIN and
  USA/raw-track compatibility, and invokes the existing C++ extract/streams
  plus audio-bank stages. Unique staging, output verification and a versioned
  inventory prevent partial setup from launching the game; previous assets
  are preserved. No deferred IMP asset formats or player-bot work was added.
  Release Desktop explicitly disables normal/detailed gameplay logs; Debug
  logging remains available. Setup errors/output remain separate.
  Windows10 1903+/11 x64 is required: embedded UTF-8 process manifests repair
  sfizz's narrow-file dependency, alongside native/wide path handling for
  movie/XA/streams. #245 records the failing Unicode production-audio control
  before repair; it remains open pending authorized publication and CI.
  Debug and Release focused selections each pass50 cases/217,134 assertions, covering launcher,
  saves, logging, synth, XA, effect streams and movie playback, including
  ASCII/Unicode controls. Source quality is zero findings; tooling controls
  pass29 tests and repeated project generation is byte-stable across24 paths.
  Selection: `[launcher],[save-store],[log],[synth],[xa-playback],[stream-playback],[movie]`.
  Actual first setup prepared11,405 files; cached verification reused them
  without FFmpeg in0.106s. Fresh GUI Play also completed real extraction and
  automatic game startup while its window stayed responsive. Native CUE/folder
  dialogs open/cancel correctly; automated filename selection was not exposed
  by this machine's UI Automation provider, so acceptance fields were seeded
  through launcher arguments, not a bypass of Prepare or Play.
  An unpacked ZIP outside the repository, under Unicode/space-containing paths
  and a Windows-only PATH, boots and shuts down without gameplay log files or
  console output even with trace-level arguments. Missing BIN/FFmpeg/runtime
  files and damaged cached assets have explicit failing controls.
  Package entrypoint: `tools\release\package-release.ps1`; tester instructions:
  `TESTER_RELEASE.md`. Final local ZIP candidate is under `build\releases`
  (`alpha-2026-10-30-r2`); source remains uncommitted and no GitHub release was
  published. The ZIP includes only whitelisted Release binaries, app-local CRT/
  DLLs, scripts and notices, never game assets or BIOS. Actual winget installation
  and a clean Windows VM were not exercised; dependency discovery, download
  alternatives and missing-winget errors were checked without installing.
  Final ZIP SHA256:
  `6130E01C1EB547610E1417578AE8C8A71CC839DA7686DC892BEA170000A493BD`.
  Archive inspection confirms29 entries,28 matching payload hashes and no
  disc-derived data. Final packaged Play exits normally with code0.
  Evidence: `build\quality\launcher-*.log` and
  `build\launcher-acceptance\acceptance.json`; native test fixtures and captures
  were never distributed. Setup summaries/logs and the launcher screenshot are
  retained; temporary package copies and disc-derived test fixtures were removed
  after verification. The original disc and `Native\data` were not changed.
  This is launcher/setup acceptance, not campaign parity.
- #244 follow-up from base `d18ba477`: the owner's MAP038 clear
  screenshot lacked the food-chain heading and puzzle rank, not ordinary
  message-box text. F03675/F03677/F03678 were merged from per-overlay
  `renderTextures.h` copies into helpers bound to the shorter ENDING atlas.
  They are now owned in `Render/TextureRegionDraw.c` with an explicit atlas
  parameter; generator bindings preserve MENUF G01780 versus ENDING G00805.
  Initial Debug solution build (`/m:2`) and
  `build\Debug\VS.Tests.exe "[menu],[ending-art],[ending-results]"` pass
  (60 cases/53,870 assertions); copier tests pass171, repeat generation is
  stable and preserves all owned sources, changed-source quality has zero
  findings. The fixed Desktop executable was written at17:32:13, before
  the owner started game PID46180 at17:33:28. Final revalidation rebuilt
  Engine/Game/Tests and passed the same60 cases; Desktop relinking was
  blocked by that running executable. The owner's process was not stopped.
  Logs under `build\quality`: `issue244-build-debug.log` (final relink
  lock), `issue244-tests-debug.log`, `issue244-generator-tests.log`,
  `issue244-generation.log`, `issue244-quality.log`.
  No game or Combat Lab was launched for this fix. The owner subsequently
  confirmed "resolved": the 0:30.81 screenshot shows both "YOUR PLACE ON THE
  FOODCHAIN:" and "LITTLE GREEN MAN". Checklist item5 is now checked
  (5/46 verified,41 remaining); #244's visual acceptance is satisfied.
  See #244 for the fixing revision, successful exact-revision CI and closure
  evidence; local tests alone do not establish publication. This is not C++
  promotion or compatibility retirement.
- Previous follow-up base was `a05b0baf` with unrelated dirty Save/title,
  generator, project and documentation changes preserved. No new runtime
  repair or acceptance-gate change was made in this investigation.
  #233 clean normal1x Release run1793323847980193-0 executes MAP214's
  final commands3046..3054 at15852, then blocks through30000 with
  transition6; zero engine errors and teardown30012. The retained PC3044
  in the final snapshot is not proof of an ongoing music wait: the raw
  command/result stream shows the later terminal commands. The separate
  accelerated debugger diagnostic also observes musicId0, no active
  channels and freed slots. No music-wait exception is justified. Original
  source ends with fade-out and no terminal fade-in; establish its intended
  native return context before changing the transition or control gate.
- #232 original-parent control: MAP100 sets story130=79 and enters MAP101
  through its authored27/0,22/15/elevation1 transition, not the standalone
  MAP101 fixture. Normal1x Debug1793324229282562-0 and
  Release1793324229232254-0 cross the old fault with zero engine errors,
  then genuinely lose Ashley; teardown12295/12293. First exploration is
  about10461 and the hostile starts an action64 ticks later. Bounded actual
  direction, sheathing and running-jump probes did not establish120-tick
  living control; they remain failures, not completion certificates.
  Root bounds repair2cf601aa and its green CI remain published; do not
  weaken survival, edit HP or disable the enemy to close the issue.
- #223 isolated normal1x Release1793324372973483-0 reproduces the error
  at2409 and tears down2421. Native Debug error-site observation confirms
  MUSIC019/sequence20, requested sample bank69, both loaded music-bank IDs0,
  and empty instrument107 metadata. Bank69/instrument43's imported asset
  exists. Original Zone1 music16 imports bank69; Zones2,3,4,5,8 have music0.
  Next required observation is the actual inherited prologue bank context,
  followed by complete Debug/Release playback/reentry, not a replacement
  program. Session-local diagnostic captures are not teardown acceptance.
  The source-backed audio prerequisites are published as47524b00 with
  successful exact Source Quality run40988508440. No runtime change or
  issue closure accompanies that documentation checkpoint.
- #225 follow-up: published `943fd39e` has successful Source Quality
  run40983155864. The assisted MAP259 route reported in issue225 remains
  assisted evidence, separate from the original viewer acceptance.
  Original viewer506 still lacks flag910. A fresh normal1x Debug
  viewer504/reentry run1793323036963507-0 also fails E298/selector0x1002
  with actual teardown: its original setup writes911, not910.
  Release1793323245638082-0 reproduces the same first/reentry failures,
  each1114 ticks, one explicit error and complete teardown.
  An exploratory E298 exception was removed: although the package proves
  target independence, Debug1793322642238477-0 then stalls at E073 offset350
  waiting for actor0x1002 state0x80 through its8000-tick budget.
  All runtime changes from that experiment are reverted; retain the explicit
  missing-actor gate. Next required observation is E073 with original actor
  prerequisites, full source/control/teardown proof in Debug and Release.
  No new issue closure or runtime-definition promotion is justified.

- Exact runtime progress is now509/804, with99 verified in both configurations
  and295 still unverified (119 unrun/176 attempted-unverified).123 of the
  owner's418 outstanding definitions now have demonstrated runtime paths.
  #149 remains open; queues, blocked labels and cancelled cases are not passes.
  The original MAP506/ZONE250 NPC/type4 viewer producers and source-derived
  ordinary textbox selections establish documented debug playback contexts,
  not earned campaign progression. Six original viewer exits and19 additional
  gallery-produced identities are verified. Explicit Lab v5 reports now retain
  actual instance ownership, original operands/branches/cleanup/handoffs,
  pre-stop living control and native per-case footer/teardown;14 Lab controls,
  11 shared Desktop controls and six unchanged cleanup controls pass.
  A cancelled model run with earlier source activity is retained as a nonpass,
  not promoted. Old549-trace and subsequent new-only scans are not to be repeated.

- #225's E068 branch now has a genuine normal1x Release pass:
  model `e278ecabc4b44f719bafd33573444c8d`, native1793281890141797-0,
  frozen image196ff263c201f34289b700fa54c0e74b5ef745e4b0070eab3b6569b8c4acb52d
  from dirty `d856eeaf`. Original Golem actor2/model100 is genuinely defeated
  while Ashley remains250HP; original type8 produces E068, with3193 matched
  commands, living/rendered control11235..11355, teardown11367 and zero errors.
  The absent model101 is not substituted: a bounded original-package proof
  permits only target-independent mandatory execution and guarded optional
  target colour. Unknown/malformed/mandatory-target/multi-entry packages,
  wrong owners/tokens and no-package bypasses remain explicit failures.
  New requests and phase-zero cleanup clear deferred ownership. Actual E239
  and mandatory-target counterfactuals pass; Debug/Release native suites pass
  1169 cases/9,594,085 assertions each, focused27/4153, copier170,
  dispatch179 and exact naming1874/1874. Changed-TU quality/tidy is0 new;
  the unnecessary all-owned header fanout was stopped, not completed.
  Accelerated observed-edge replays genuinely lose and remain nonpasses.
  Fresh normal1x Debug model `8d806ac3e5d244068d8f1d8917469960`,
  native1793282324844776-0, also genuinely wins and verifies3193 E068
  commands, control11559..11679, teardown11691, zero errors and no skip.
  Its frozen image is7f640a3c89911cf0e45f8aca2a580dcd0262248f736fd8004479b8b3986e49d4.
  E073 remains unverified; #225 is not closed for its E068 milestone alone.

- E073's flag910=1 is an original loot-acquisition prerequisite, not a QA
  inventory or sigil-use shortcut. MAP259/49/12 original room loot0/misc2
  contains Aster Sigil472; `Menu_Loot_RecordClaimedKey` earns910=1 when it
  is actually claimed. MAP312 sigil use instead writes2. The real contiguous
  route is doors49/12/0,49/11/0,49/10/0 into MAP256's original boss intro.
  Both genuine MAP259 model attempts remain defeats: `68b19d0c` loses to
  original Meteor130 after one kill; `d4f8d76d` consumes no spell MP and
  loses without earning910. Do not retry these unchanged or claim their
  reasons mentioning Silence establish an actual cast. Cold-log pruning
  additionally archives58 files/545,423,678 bytes into7,779,306 verified
  compressed bytes before unlinking; active evidence remains untouched.

- Five more exact local definitions follow genuine prerequisites: original
  MAP175 local0 crosses its authored tile, writes origin286=1 and loads
  MAP177; normal movement then activates MAP177 local1 and its authored
  return. Release1793284268203836-0 proves22/28/20 commands across those
  three instances, control498..618 and teardown630, zero errors. Normal1x
  model fights genuinely clear Wyvern Knight `10e4bb66`, Wyvern Queen
  `9736e392` and Dragon Zombie `d91c3a09`, activating their original
  MAP366/MAP377/MAP392 local1 callbacks. Their control/teardown intervals
  are14375..14495/14507,11201..11321/11333 and7995..8115/8127;
  each has zero errors and118/117/118 matched local commands. Native
  reaction acceptance/effect counts and coverage gaps remain separate
  from source certificates; no perfect-chain or universal-defense claim.

- Original MAP164's missing portal context is now earned through its real
  story producer, not assigned: MAP164 loads MAP200/E040, which loads E047;
  original E047 writes stage130=41. Actual post-story room is40/21, not
  the initially hypothesized32/17. Original save-point trigger9,6/layer0,
  kind16/activation14 is reached by normal movement, then actual native
  Data/Save/affirmative file controls write private `slot00.vssave` at12993.
  Release1793286571826273-0 verifies3499/10/1351/3313 commands across four
  original instances, control13123..13243 and teardown13255, zero errors.
  Save SHA256 is16ff9f7cdddae9ebe765504a8f3dc3963b5aabbd82c40a9605c9a5eeffad6fea;
  no save-point gate, capture API or file contents were bypassed/edited.
  Current Desktop has been built and a byte-identical private Continue
  follows the ten original door legs back to the temple/portal. This is
  a source-backed QA checkpoint, not earned full-campaign progression;
  traversal/source verification remains in progress.

- #234's external choice-filter repair `296069ca`, CI40889102631 successful,
  is closed. A real follow-up actually casts Magic Ward76:21MP spent,
  native status262144 appears, original Tarnish57 consumes the ward.
  Later mana drain/Meteor still defeats Ashley; no Aster or E073 pass is
  claimed. Caster-first, Lich-first and existing curative-kit variants
  also fail and must not be repeated unchanged. All117 player regressions
  and full normal quality pass. Initial #225 CI40887801041's four shared-test
  formatting findings are repaired in `d2ce907b`, CI40888393696 successful,
  without changing a baseline.

- Recorded-input replay now uses actual engine-held runs, not the cached
  native pressed mask that duplicated edges and lost a64-tick movement.
  Release Golem/Queen and own-input Debug Golem replay through real combat
  and strict source gates; one-tick padding is explicitly measured and
  wall-clock timing is not certified. Cross-configuration Debug Golem
  and Wyvern Knight replays remain failed, not deterministic successes.
  Latest cold pruning additionally verifies/decompresses50 logs:
  1,294,807,385 raw bytes become10,088,533 archived bytes before unlinking.

- Owner-authorized scoped publication is complete through `d9b79fa0`:
  #229 `96a6515c`, #224 `c892e420`, #227 `7971c4d9`, #226 `1080de58`,
  ending #228/#230/#231 `ad8a3c8a`, bounds portion #232 `2cf601aa`, external
  source-testing support `58618938`, and camera portion #233 `d9b79fa0`.
  Each exact pushed revision has successful Source Quality evidence. Issues
  #224/#226/#227/#228/#229/#230/#231 are closed with their acceptance evidence.
  #223/#225/#232/#233 and parent #149 remain open for the distinct recorded
  prerequisites/runtime limits. No unrelated Save/title, solution, browsing
  project, architecture or customization edits were included.

- New original local proofs: Release1793278293922782-0 verifies MAP088 local0
  and MAP360 local0 through their entry-earned flag40 and real interactions;
  Release1793278925894121-0 verifies MAP199 local1 through its original
  ungated facing-tile trigger. The other native outcomes and eight measured
  survival replays remain nonpasses. A25-case longer original lock-producer
  batch1793279234029838-0 adds no verified identity; do not repeat it unchanged.
  Explicit standalone components cannot substitute: the native fixture
  intentionally requires isolated entry for direct requests, while the strict
  production-room source contract rejects isolated cases. That failed setup
  was not reclassified or used to weaken the804 matrix. Full current native
  suites pass1144 cases/9,589,968 assertions in each configuration.

- Two resumed source batches add16 exact identities through35 independently
  verified original instances. The new-only scans take5.38 and3.93 seconds;
  they do not replay the old corpus. Some later fixture fights end in defeat,
  while their earlier source-complete scenes and120 living/rendered control
  ticks remain separately proved. Failed/blocked whole-case outcomes are not
  victories. Reports are
  `build\quality\scene804\verified-resumed-source-instances-1793261480262058-0.json`
  and `verified-resumed-entry-instances-1793261820396309-0.json`.
  Interrupted MAP100/MAP101 effect-bone faults are #232; MAP214 camera
  constraints are #233. Neither suspected cause is patched or promoted.

- Ending blockers #228/#230/#231 are locally repaired: typed native OT
  consumers preserve original buckets, all12 full embedded TIMs own their
  palettes/pixels and writable72224-byte TIM8 workspace, and completion
  strings hold their original formatting plus NUL. Original unskipped
  Debug1793261918537648-0/Release1793261820312067-0 reach natural title handoff
  and teardown at42579 with zero errors; their old field-control expectations
  correctly fail. Final Debug1793263459268740-0/Release1793263459160911-0
  each bind8722 original commands across five instances to the actual boot
  title transition, unskipped completion flags, natural fiber return and
  footer/teardown; actual exit0.31 exact source/lifecycle controls pass per
  configuration. Persistent Release1793264413301690-0 then passes26 instances
  across original MAP134/135/136/137 producers in one window, with four genuine
  title handoffs/reset/reentries and zero errors. Six terminal-goal controls pass31
  assertions; combined render/scene/verification-log selections pass185 cases/
  1,812,710 assertions per configuration. Scoped style/tidy is clean. Ordinary
  field gates, cancelled/error/timeout outcomes and default gameplay are not
  relaxed. Published as `ad8a3c8a`, exact CI40876133291 passes and
  #228/#230/#231 are closed. Campaign/fidelity/promotion/retirement remain
  separate; subsequent audio integration also has fresh unskipped replays.

- #224's real hidden-platform placement crash is locally repaired and verified:
  isolated fresh1x original event31 replay formerly faults entering22/7 at7104.
  The native placement API now reads the existing authored transform without
  enabling hidden rendering/collision or changing HP/flags/RNG; the visible-only
  legacy lookup no longer returns stack storage. F00920/F00938 retain one owned
  definition, and F00901 integration is generator-owned. Debug1793249226277744-0
  and Release1793249507914535-0 both play3544 original event31 commands, return
  to22/6, hold living/rendered control7947..8067 and complete native teardown
  at8079 with zero errors. Exact1x images/PDBs are frozen under
  `build\quality\scene804\platform-normal-images`; final rebuilt images repeat
  the source/control path in1793250355766226-0/1793250478995821-0 (8x semantic
  controls). Both builds and related114-case/57,321-assertion selections pass;
  focused placement tests pass4 cases/66 assertions.158 copier tests,179
  actual event-dispatch targets and39 naming tests pass;1874/1874 exact naming
  positions are covered. Scoped quality and the changed C++ TU tidy have zero
  new findings. Only clean Game/Tests projects were generated; owner solution,
  browsing-project, Save and F03670 edits were preserved. The local checkpoint
  initially left #224 open; it is now published as `c892e420`,
  exact CI40875707111 passes and #224 is closed; no promotion/retirement or
  pixel/audio-fidelity claim is made.

- #229 is locally repaired and source-verified: native banked SFX selectors,
  sequencer `effectId`, mixer stop/fade/query paths and output arrays retain
  full pointer identity; missing banks/invalid indices log explicit failure,
  never select a built-in fallback. Debug1793268807181493-0 and
  Release1793268807093029-0 pass original E087/SFX67 -> MAP415/E089 -> authored
  27/1, six instances/8172 commands, living control12080..12200, teardown12212,
  exit0 and zero errors. Fresh zero-state MAP415 remains a failed prerequisite
  with one explicit error/real teardown, not a pass. Full suites pass1141 cases/
  9,589,946 assertions per configuration after subsequent owner-domain/trace
  integration;168 copier controls pass. Published as `96a6515c`, exact
  CI40875509110 passes and #229 is closed.

- #232's exact native AV is traced to the distinct slot domains: absent valid
  renderer16 fell through to out-of-bounds `Field_gimmickInstances[16]`, not a
  corrupted renderer registration. Bounded shared lookup preserves existing
  missing-owner query results; missing world matrices leave caller outputs
  untouched. Existing renderer16 models and valid gimmicks remain supported.
  Temporary argument logging is removed. Related62 cases/17,091 assertions
  pass per configuration. Original MAP101 now crosses3194 and completes3767
  entry commands with zero errors, but passive cases still fail: a real enemy
  starts at5304, only64 ticks after restored control, and later kills Ashley.
  Published bounds repair `2cf601aa` has exact green CI40876371035. This is
  not a120-tick live-control certificate or issue completion; #232 stays open.

- #233's nullable camera consumer is locally repaired: original MAP214 has
  collision width13/height25, but source-driven motion reaches tile4,31.
  The existing cell lookup returns null; camera occlusion now explicitly warns
  and uses its normal geometry-clearing path when no authored cell exists.
  No collision, actor position, source flag, geometry constraint record or
  valid view-trigger behavior changes. New tests cover missing cells, ordinary
  clearing and the valid authored occlusion branch. Original Release
  1793278757543012-0 crosses the old AV with zero errors and real teardown,
  but still fails the120-tick living-control goal; no runtime identity is added
  and #233 stays open. Published as `d9b79fa0`, exact CI40878539856 passes.
  This is native fault progress, not scene completion.

- External source-tile navigation is opt-in and has115 tests/clean scoped
  quality. It uses actual post-entry camera/geometry, native activation and
  the unchanged completion gate; only this objective can explore supported
  patch frontiers after nearest endpoints are exhausted. Cinematic control
  now rejects new/stale strategic preparations while preserving event-owned
  rewards and the unchanged transaction timeout. Spatial/model attempts remain
  nonpasses: MAP153 `dbd373c12dac4c658dd9d70940299031` never activates local0;
  MAP203 basic/all-spell runs genuinely lose, and the all-ability run hits the
  original512MiB budget after16091 ticks rather than proving victory. The
  measured duplicate238MiB root/per-script traces prompted explicit
  verification-only shared rich-frame references: every base actor/control/
  scene frame remains scoped; full schema2 data stays in the root. Actual600-
  tick control saves22.29% total bytes without sampling, deletion or a raised
  budget. The model's goal/reason is not proof of actual spell execution.

- Native source-goal completion is now optional in the external player:
  `--scene-expectation` embeds the existing original Lab scene/control contract
  and lets the native observer finish the case, rather than WM_CLOSE/cancel
  or a needless full-budget continuation. Defaults and physical-door behavior
  are unchanged.105 player tests and scoped quality pass. Original MAP121 run
  `491bdb7a823446da9499efd6dbe2bbd7` genuinely defeats its hostile, completes
  rewards/local2 and native source/control/teardown at5789 of12000:109 entry
  commands,119 local2 commands, living control5657..5777, zero native errors.
  Its one unconfirmed defense retains the overall player nonpass. The earlier
  `116544bfaeb342e5afcfafc8aa55f419` wins that fight and walks the actual door,
  but dies in the next room and cancels; it is not accepted Lab completion.
  Normal1x MAP316 run88c681b67b034854b42dab892204d008 genuinely wins and
  passes:104 entry/84 local0 commands, control5137..5257, teardown5269.
  Despite its spell-probe intent, the actual actions are Herakles and ordinary
  chains, not an offensive-spell victory or proof of spell adaptation.
  Four-times-speed MAP316/336 attempts genuinely lose and remain failures;
  MAP333 has no original type8 death producer and was not launched under an
  invented target. One normal1x MAP316 variant follows measured planning latency.
  #223 retains original inherited-audio failures. #225 remains unresolved:
  genuine retained Iron Golem defeat in2b0ad063c7bf4b138a0024b92c49bda3
  still leaves original effect239 selector101 unavailable. Independent
  source-disc inspection confirms the authored Golem model/effects ID100 and
  E068 operand101; a secondary-body explanation is rejected. No actor
  substitution, asset patch or error downgrade was made. E073 also needs its
  original Aster Sigil-used flag910 and retained Neesa/Tieger prerequisites;
  stage130 alone is not sufficient.

- #226/#227 now pass the complete original viewer504/E078/E026/E038 chain:
  Debug1793255056114631-0 and Release1793255464212292-0 validate all seven
  original instances, control12024..12144 and native teardown12156 with zero
  errors. Unwind/retirement uses the actual owned bound animation bank;
  new-clip selection still uses the current selector. The separate retention
  ABI fix calls the declared char-returning predicate directly, preventing
  stale upper-register bits from entering an absent actor's HP query.
  Debug/Release each pass123 related cases/57,352 assertions;160 copier
  tests,179 VM dispatch targets and1874/1874 naming checks pass. Scoped
  quality/changed-TU tidy has zero new findings; the all-owned tidy fanout was
  stopped and is not claimed complete. #226 is now published as `1080de58`,
  exact CI40875836489 passes; #227 is `7971c4d9`, CI40875783132 passes.
  Both are closed. These results remain distinct from promotion or
  compatibility-retirement acceptance.

- Earlier ordinary-input batches add16 paths after the completed Desktop instance
  recovery: five entries in1793244075047574-0, eight entries in
  1793245115600135-0, and external11/84 plus MAP265 local0 in
  1793245457958291-0. All retain unchanged v3 original-byte/branch/completion,
  living rendered120-tick pre-stop control and native teardown gates. Movement
  continues through actual enemy actions rather than ending during the first
  attack; original actors/stats/flags/RNG/collision remain unchanged. Other
  outcomes remain nonpasses. Speeds above1 are semantic investigation only.
  The exact prerequisite queue must follow the301-identity current matrix,
  not the earlier418 planning snapshot.

- Completed normal-speed Desktop evidence
  `bad76450a03b4ed3a0a6a9c8911a7789` independently verifies24 original script
  instances,23 previously uncovered identities. The v4 verifier binds actual
  script/buffer instances through original247/104/trigger handoffs rather than
  mutable pending event IDs, with explicit Desktop finalization/fiber lifetime
  proof and11 positive/counterfactual controls. All six unchanged v2/v3 cleanup
  controls still pass. This does not claim `game_finished`, complete campaign
  shutdown, fidelity or C++ promotion.549 eligible old Lab traces were scanned
  once in about109 seconds; they yielded no further new identities. Do not
  repeat that corpus scan or another full prologue. The separately tested Lab
  v5 candidates are not automatically accepted progress reports; only the
  explicit source/lifecycle-validated reports described above are accepted.

- Owner correction2026-10-28: the serial full-prologue workflow is retired.
  Prioritize source-ready persistent batches and shared prerequisite groups;
  use the model only for necessary earned combat/progression, not to inspect a
  scene already present in completed evidence. New `--verification-log` retains
  all source/control/error evidence while removing Trace chatter, structured
  engine echoes and non-player run/script frame duplication. The player requests
  this profile too. Full tracing remains available for targeted fidelity work.
  The measured same9-path control batch passes all unchanged source gates in
  9.39 seconds,57.5 paths/minute, with56.45% smaller logs. These are previously
  verified controls, not9 new paths. Remaining prerequisites are grouped in
  `build\quality\scene804\batched-verification-queue.json`; its categories are
  planning information, never passes. Final validation: Debug/Release each
  pass74 native cases/47,617 assertions; all102 player tests pass, scoped tidy/
  format/lint has0 new findings and naming1874/1874 is covered. The new MAP147
  producer attempt did not activate and remains unverified; five other
  first-try candidates lack a demonstrated field camera and were not guessed.
  Model-required Lab scenes now have explicit2x/4x investigation rates; normal
  1x strict-reaction behavior is unchanged. Rates above1 reject reaction-count
  certification, Desktop/save/New Game and physical-door objectives. The4x
  MAP042 control `0e1ae5fec0744566a2b8a95ba37c985a` passes its original scene
  source contract (85 commands/control4540..4660), while3 unconfirmed chains
  retain its overall player nonpass and timing eligibility is false.
  MAP106 `36eed3eb0e46448faba3de21fd0c4d44` and MAP382
  `6589b4f357984aa795472056d42b0990` end in genuine defeat. No new definition
  is promoted from these controls or failures.

- Owner-authorized heavy log pruning is complete:2300 generated logs,
  42.951GiB raw ->1.102GiB lossless cold storage outside the workspace at
  `%LOCALAPPDATA%\VS-NativePort\evidence-archive\20261028`. Every file was fully
  decompressed and SHA-256 checked before unlinking its original. The indexed
  original path/digest permits exact restoration; inventories, case reports,
  manifests, source/assets and saves were not removed. Small/recent logs remain
  live. `.vscode` excludes generated build output from watching/searching.
  Session `archive_session_logs.py --restore <original-absolute-path>` restores
  one indexed file without overwriting an existing file; `archived_scene_logs.py`
  streams verified cold logs for camera/source inspection. This explicit owner
  instruction supersedes the earlier preserve-all-raw/pinning policy. Do not
  regenerate gigabytes of logs or treat archived paths as absent evidence.

- The original normal-speed model playthrough is now complete:
  `bad76450a03b4ed3a0a6a9c8911a7789` passes its production New Game combat/control
  goal at59737, with3 observed hostile deaths, no death/skip/reaction failures,
  zero native errors and finalized host diagnostics. Its independently proven
  per-definition v4 instances are recorded above; Desktop lifecycle evidence
  remains explicitly distinct from Lab teardown and campaign completion.
  No further full-prologue replay is warranted without a specific regression.

- Current owner focus remains actual runtime verification of all804 definitions,
  using the existing external model for real combat prerequisites. #149 stays
  open. At this checkpoint the exact-identity matrix has454 verified paths,
  80 in both Debug/Release,350 outstanding (144 unrun/206 attempted-unverified).
  All referenced room-entry definitions have had native attempts; an attempt,
  completed host budget or unverified label is not a pass. No all-branch,
  campaign, pixel or listening certification is implied. Source remains dirty
  `a6cbaab48b6c98c7f176183e491f0ab116394dc3`; no new commit/push.

- The external limb/reward OODA gaps are repaired without ordinary gameplay
  changes. Ready multi-part targets offer explicit native actor/limb choices
  and cached forecasts, with no recalculation or RNG consumption. Actual
  event-owned loot/ability menus are visible under native control3; only their
  legitimate ready-menu keys are leased. Original MAP032 victory/rewards run
  `1e95c7846c02429fa1e72deb25bd3c36` verifies local2 with324 matching commands
  and living control2772..2892. MAP042
  `5f68618952cb4a75ba46288697d6c79f` defeats both original hostiles, completes
  rewards and verifies local1 with85 matching commands/control2962..3082.
  These use the existing declared post-minotaur representative action profile,
  not an earned campaign/save claim. Earlier stock-action failures are retained.
  The seven other cases in the eight-case batch remain nonpasses.
  Python100 tests and scoped quality pass; native Debug/Release selections each
  pass67 cases/47,570 assertions. Naming1874/1874 exact occurrences and39 naming
  regression tests pass. No baseline was refreshed.

- A MAP360 model HTTP400 is now causally reproduced and repaired, not retried
  blindly. The preserved run is `6e2645b36aef4371a2c83f9708207701`.
  The exact prompt required8218 tokens against8192. Lossless JSON compaction
  preserves all prompt facts and completes at6732 prompt tokens; local evidence
  is `build\quality\scene804\prompt-context-replay-01`. HTTP failures now persist
  bounded server details. Two focused regressions first failed, then passed.
  Extended MAP051 run `d46db4cdab2f467695e2ed25d638a665` genuinely defeats both
  original hostiles, completes real rewards and verifies local1 (116 matching
  commands/control10532..10652). Extended MAP358/MAP362 end in genuine defeat.
  Native Release1793234635260661-0 verifies9 additional original entries from
  supported alternative starts: MAP017/244/336/345/386/389/391/394/398.
  All39 source nonpasses and the native batch exit2 are retained; speed8 is
  semantic-only, not listening acceptance.

- Extending MAP015/010/005 to18000 ticks did not establish acceptance.
  MAP005 produces a native missing-program error at2409; isolated speed1
  reproduces voice5/instrument107. #223 tracks this confirmed symptom, not
  a guessed engine cause. Original external2 loads mapped WAVE0069, which
  later music17/18/19 share; jumping directly to later rooms omits predecessor
  context. Actual Desktop New Game `a3dff5450aaa46269a186d250848ed70` imports
  bank69 at1399 without an audio change, then reaches room3/0 at7490.
  Its external parser abort on nonhostile script actor6/model-1/action byte128
  is fixed: preserve the unsigned native byte and expose unclassified values
  without inventing physical/spell semantics. Both red regressions and all7490
  retained frames now pass. Explicit external `--new-game` uses ordinary title
  keys and private saves, not a Lab action grant or story skip; Continue and
  Lab sources stay mutually exclusive. The next actual replay
  `5cb6f06375d54c289224c4d9286fc0bc` reaches1/0 ->2/0 ->3/0 ->4/0 ->5/0 at
  1415/5158/7478/14167/16922, with zero native errors through18000 and finalized
  host diagnostics. It remains a failed external run because the new source
  mode incorrectly selected Lab finalization when no save was supplied.
  Completion now selects the actual host, preserving Desktop's separately
  labeled host-observation contract and all Lab teardown/error/status gates.
  Dedicated New Game/Continue/Lab controls pass.
  Release `317d926d4cab4a44b438c0e2d7795a65` reaches actual external4
  MUSIC019 load/play18902/18906 and a successful instrument107 note18911,
  plus180 later notes. All2049 recorded external4 commands before its chained
  request pair with native results and match full original opcodes/operands.
  Debug `b05be49c5df54258ad5381c0ca7b6f7e` repeats MUSIC019 at19204 and181
  subsequent107 notes, with zero native errors and finalized host diagnostics.
  Missing inherited context, not an unavailable program or a required silent
  substitute, explains the direct fixture symptom. No audio change was made.
  Both runs retain their blocked outcomes rather than gaining a prologue pass.
  Debug genuinely observes all3 hostiles at zero HP (last room6 death31272),
  then original7/0 ->8/0 ->18/0 story handoffs. Late dialogue is advancing:
  main-thread offsets760 ->978 ->1312 ->1574 ->1737 over40000..42000 and actual
  Circle32 at41971..41972 refute a controller softlock. Fresh normal-speed
  Release `aec3427247184c44a023bf2eb95b59a7` ends at54000 in original9/14,
  MAP023, with all3 hostile deaths, no native errors/skip/death, but no settled
  field-control goal. Actual main cursor309 ->663 ->863 ->1349 ->1489 ->1760
  proves continued progress; its original terminal room handoff is1953.
  Recognizing every dialogue box and acknowledged Circle input is already
  correct, so no speculative controller fix was made. The failed/blocked
  outcomes stay unchanged. Current bounded normal-speed Release replay
  `bad76450a03b4ed3a0a6a9c8911a7789` permits72000 ticks and stops early only
  after real goal/control acceptance. Active shell:
  `scene149-original-story-terminal-handoff`; local native session
  `1793239002398582-0`. No acceptance or #223 closure is claimed yet.
  Reviewed raw evidence15.584GiB/free C1120GiB supports its explicitly recorded
  2GiB/run,20GiB investigation cap. Defaults remain512MiB/10GiB; nothing pinned
  was deleted.

- Further source proof must follow native chained-event ownership, not mutable
  event IDs alone: original opcode247 changes `Event_eventId` and queues a new
  asset before the old script tail finishes. Release external4 requests5 at
  21628; old yield/finish bytes can consequently carry ID5 before the actual
  EVENT/0005 read21630 and replacement. Preserve exact PC/base/operands and
  source load/init boundaries. Do not count those old tail bytes as a new
  definition, invent a return, or weaken the current v2/v3 lifecycle gates to
  promote Desktop host observations.

- Fight-to-door behavior is demonstrated, not merely unit-tested:
  pinned81945558a2534341a97fb5ccec74b0fe defeats the3 original MAP018 hostiles
  at952/1492/1862, earns native local1/flags96 and48 at2192 and control2194..2314.
  Pinned c79537e8c52b4b589a846dc282b71ed1 additionally opens the actual door2642,
  arrives9/8 at2843, follows the original redirect9/13 at3700, closes native
  dialogue3994 and restores control4174 without skip/errors. Its MAP017 local0
  and MAP022 entry proofs are registered, including control5862..5982.
  Fixed authored9/8 door acceptance remains a nonpass after the original9/13
  redirect. Never relabel that as an authored-door pass or grant flag180.

- Re-entry diagnostic defect fixed narrowly in maintained Content code:
  `Content_ReadDiscBytes` logged a path only once per process, suppressing fresh
  source evidence on later actual reads. First-read Info is unchanged; later
  successful reads are Debug. No byte, padding, failure or gameplay changes.
  The new isolated regression first failed with1 record instead of2, then
  passed. Final Debug/Release selections each pass61 cases/1,400,309 assertions.
  Four normal-speed original MAP018 local2 reentries pass strict source/control
  verification: Debug1793225869739765-0 and Release1793225237897085-0; first
  actual MPD read is Info, second is Debug. Three unchanged formerly-unbound
  handoffs now pass in Release1793226101572717-0. Scoped fixture/content tidy
  and Python quality have0 new findings; naming1874/1874 occurrences covered.

- Original type2 producers require a changed trigger tile
  (`146C.c:5365`/`Field_CheckAshleyTileTriggers`), not spawning at a predicate.
  Native crossings use supported neighboring starts, actual field cameras and
  original point/rectangle predicates, including authored door tiles as targets.
  Batch1793223546713737-0 verifies8/79 paths; all71 nonpasses are retained.
  Directions-only opt-in player `--setup-press` establishes such a source
  precondition before model strategy; combat/jump/interaction/skip keys are
  rejected, pulses cannot overlap, and fixed-door/Desktop assertions cannot be
  combined with it.85 player tests pass. Live Release
  c2a75a48e2a74b428041789ddc395b81 executes MAP055 local0 at134 before first
  model strategy209, after the explicit140 handoff; actual destination control
  1627..1747 proves the source-defined handoff. Its overall defeat-all goal
  remains blocked, not an all-hostiles victory or physical-door pass.

- Native cleanup source binding is explicit, not an ignored mismatch:
  G00118 `Event_specialSubroutineScript` is original BATTLE offset0x8135C;
  original sizes0x81430 and imported dispatch0x81530 bind opcode0x48 to
  `Event_ScriptReturnToSavedAddress` (0x800B8C44). Do not use the C initializer
  placeholder as proof of the imported dispatch. Special16 enters original
  bytecode and returns to exact caller PC+6; caller FF and pre-stop living
  control must still occur. Six negative/boundary/baseline controls pass.
  Explicit v3 proof verifies MAP147's actual second visit; fresh Release
  1793229293186199-0 also verifies MAP148's real return, control4850..4970.
  The original inventory and208 static unresolved prerequisites are unchanged.

- All bounded native/model runs above are complete. Four additional original
  enemy-clear model cases finished without earning their defeat-all goals:
  MAP03260731e84cfa7408ea459e911cdb4a878 (0/1 deaths),
  MAP358c060c227774042679faafc978b8c80c2 (0/3),
  MAP36096089271d2164679864806bdac4d2a32 (0/3) and
  MAP362e9dd33966b014b7bb11fbb90399a40ed (1/3).
  Their enemy-clear selectors are NOT promoted. Actual model movement does
  activate MAP032 local1;353 full commands and living control1420..1540 pass
  separately. Raw native height/targeting/healing outcomes remain available;
  no unsupported causal combat fix or victory is claimed.
  Pinned raw evidence reached10.85GB against the default10GiB limit. It was
  reviewed without deletion (over1TiB free on C:); this investigation uses the
  existing API's bounded20GiB retention parameter for the current story replay,
  recorded in each run, while
  the default remains unchanged. Do not delete pinned raw proofs.

- Owner correction: #149 is **reopened**. The804 remaining indexed definitions
  need actual runtime verification; inventory/tool availability and explicit
  unverified labels were not sufficient acceptance. Do not close this issue
  again while that coverage remains outstanding. The earlier closure scope
  below is superseded, not a gameplay certification.
  Baseline remaining identities are389 room-entry/317 room-local/98 external.
  Current authoritative progress is
  `build\quality\scene804\runtime-status.json`, retaining all804 exact
  asset/source/selector/offset identities and Debug/Release evidence separately.
  Release pilot1793212709064667-0 verifies7 entries; normal-speed persistent
  batch1793212959727645-0 verifies26 more,33 unique total/771 outstanding.
  Thirteen entry bodies completed but enemy combat interrupted the required
  120 consecutive live/rendered control ticks; they remain unverified, not
  VM-hang or pass claims. MAP182's original600-tick budget did not pass:
  teardown-grace frames were excluded and its later1200-tick replay does pass.
  Verification requires actual native asset load, exact executed opcodes/full
  operands/taken branches, native main return, living Ashley, field rendering,
  and120 consecutive normal-control ticks before any stop request.
  Session helpers `scene804_entries.py`/`scene804_status.py` generate immutable
  batch plans and resumable reports; reports reject changed source operands
  and stop-boundary threshold false positives. The normal-speed59-case Debug
  and40-case medium-entry Release batches are completed historical evidence,
  reconciled in the current matrix above. General player changes remain
  frozen. No game/VM/Encounter implementation or reference file was changed.
  Current scoped Tool changes only expose supported non-door source positions
  from the existing native room catalog, reusing original door-fixture floor
  selection; they do not grant story flags, HP, items or defeated actors.

- #149 historical investigation checkpoint (superseded closure): semantic implementation
  `b3f68b46ca50a3aa3a04fa28ef1edd81f4cb0c64` has exact green Source Quality
  `40740895618`. Static inventory16e3d921 and diagnostic9db31b9c milestones
  also have exact green40733732970/40736486701. All1,311 definitions are
  reconciled in `build\quality\issue149-all-scene-status.json`: three bounded
  external components verified,804 indexed runtime-unverified,504 empty;
  208 special-operation prerequisites remain unresolved. Do not equate
  investigation/tooling acceptance with all-story/all-branch fidelity.
  Actual controller proof Release1793210619128669-0 uses no scripted or
  automatic dialogue input: visible wait549, F6 pause549, F7 step550,
  native Circle558, then F10 normal cancellation/teardown with0 errors.
  The local report is `build\quality\scene149-interactive.json`. Two earlier
  helper attempts were terminated: one sampled an incomplete live JSON line;
  another assumed global foreground could not change. Neither is a scene
  completion/crash proof. The final sampler uses complete lines and makes no
  global-focus assumption or activation call. Final closure documentation
  preserves all unverified cases and historical door nonpasses.

- #149 semantic playback gate, dirty `9db31b9c`: sequence entries can assert
  ordered event IDs from an explicit native source, execution phase4 and
  target-room phase0/control1/no-transition/no-dialogue for120 ticks. Native
  preparation phases and inherited IDs in preloaded room scripts cannot
  satisfy external-event expectations. Missing start/handoff/control yields
  blocked, not frame-budget success; skip/defeat/errors cannot pass. Ordinary
  visible-wait Circle assistance is opt-in; stat shortcuts are unavailable
  in proof cases. Generic scripts explicitly leave playback unasserted.
  Final Debug/Release selections each pass55 cases/6,025 assertions; scoped
  compilation quality is0 new and all affected projects match regeneration.
  Actual Desktop run `build\Release\game-logs\1793209418183309-0` exits0 at2200,
  retaining259/259 paired commands/results,121 flag writes, correct full
  operand shapes and a durable session end; target prologue is not skipped.
  Debug component run1793208919199067-0 completes first80/82/81 case and
  120-tick control at11030, with7,894 paired commands/results and410 flag
  writes. All7,879 external commands match original source offsets/opcodes/
  complete operands; all40 branches,2,232 camera and4,679 model/pose states
  are retained. The300s guard interrupted case2; it is not a full reentry pass.
  Source-aware normal-speed Release1793209401166242-0 passes both cases:
  each returns control10910, passes120 ticks11030 and cleanly tears down11042.
  Each has7,894 paired commands/results,410 flag writes,0 unmatched records,
  0 source/operand mismatches, all37 external branches source-valid,0 skips/
  errors/audio-queue drops. Debug1793209654937857-0 passes both cases with the
  same measurements; the strict four-case evidence checker passes and writes
  `build\quality\issue149-replay-evidence.json`. The final BMP captures the
  returned readable editor, not in-scene fidelity.
  Mixed blocked-scene/generic-reset/blocked-scene/generic-reset run
  Release1793210265851374-0 retains all four outcomes and clean teardowns;
  exit2 correctly preserves nonpass status. Earlier blocked cases no longer
  stop later successful generic resets and remaining scene cases.
  Historical frontier reconciliation recovered14/36 raw end states: all14
  have phase0/control1 after original story relocations, not VM hangs. Other
  22 raw traces are absent and remain unverified; old door statuses/counts
  are unchanged. See `build\quality\scene149-inherited-event-frontier.json`.
  Full static definition counts are706 indexed/93 empty room scripts and101
  indexed/411 empty external EVTs. There are208 explicitly unresolved special
  operation preconditions; the807 indexed definitions are not807 playback
  passes. The797 actor-animation assets and single movie stay distinct.

- #149 diagnostic milestone, dirty `16e3d921`: full pre-dispatch command
  snapshots and full-width result correlation now join ordered actual script
  flag writes and original Start/forced skip reasons in shared Desktop/Lab
  structured and readable logs. Observation is in Diagnostics; generated VM/
  flag hooks come only from copier fixups. Disabled observation does not read
  operands or change flags. Debug and Release each pass24 focused scene/trace
  cases with5,650 assertions; both hosts are relinked. Scoped compilation
  quality and default maintained-code quality have0 new findings. All742
  copier outputs match a second generation, including preserved owner work;
  Game project generation is stable. All1,873 lexical positions have exact
  reviews; only three unchanged bindings were individually renewed. No scene
  playback or fidelity claim is added by these diagnostic tests. Next establish
  actual scene/control outcomes and fresh source-aligned playback/reentry.

- #151 is closed completed with fixing/support revision
  `deeecf8ad7d159f991b52aeb48b4f731a72731b0` and exact green Source Quality
  `40731411773`; the issue's closure table explicitly preserves historical
  nonpasses and excludes all-physical/full-campaign claims. Owner priority is
  now #149, with general player work frozen.
  Fresh dirty-`deeecf8a` scene catalog checks pass14 cases/5,527 assertions
  per Debug/Release, including the actual original-disc inventory and repeated
  output comparison. Both fresh CLI outputs contain1822 distinct assets:
  512 MPDs/512 EVTs/797 SEQ-ESQ banks/one movie,807 indexed and504 empty
  definitions,0 errors. All playback/precondition labels remain unverified.
  Trigger-offset alias rejection is included; short sentinel tables remain
  valid. VS.Tests/VS.CombatLab project generation matches the working files
  and is identical on a second generation; scoped compilation quality is
  0 new findings without baseline changes.
  New inventory paths:
  `build\Debug\combat-lab\runs\1793207081262353-0\scene-inventory.json` and
  `build\Release\combat-lab\runs\1793207574668336-0\scene-inventory.json`.
  The catalog is only the first #149 gate. Next close the explicit runtime
  logging gaps (full executed operands and relevant flag transitions) and
  reconcile source-backed playback/control/reentry evidence, including the
  36 source/destination-event budget cases retained from #151.
- #151 closure audit (dirty `e493fa2c`): the original exhaustive machine report
  still reconciles exactly1001 unique directed IDs/1003 observations/0 missing
  or unrun. Its SHA-256 remains
  `1f0a586650b65f429e62d61511b01ba6f03014ca9f910b9c058652020be33487`.
  Latest761 passed/238 blocked/2 failed and764 prior-or-latest loader proofs
  are preserved, not converted into physical/story passes. Nonpasses remain
  166 fixture defeats,27 unavailable destination zones/rooms,36 source/
  destination event budgets and9 story gates, plus two correct historical
  required-actor refusals with separate legitimate-save proofs. Report rows
  retain actual reached phases and provenance. Owner log cleanup removed
  two original raw sequence checkpoints; their embedded reconciled result
  snapshots remain, and are not represented as fresh raw replays.
  Original discovered runtime faults #150/#152-#166 have separately verified
  source/generator fixes and dispositions; #167 remains owner-closed as
  unreproduced/not planned, not a claimed causal repair. Fresh original-disc
  and relevant lifecycle/inspection/effect controls pass62 cases/42,870
  assertions in each build. Default and focused compilation quality have0
  new findings without baseline changes. Retained actual Debug/Release
  controller proofs cover repaired room6 entry/quiet control and legitimate
  Minotaur preconditions; newer physical flag180 proofs stay supplemental.
  This delivery is all-record loader/entry-event coverage plus explicit
  physical evidence and limits, not all1001 records physically traversed,
  full-campaign completion or compatibility retirement. The36 event-budget
  cases belong to the subsequent all-sequence investigation #149.
  Player support is frozen to this test workflow. Schema2 diagnostic item/
  loot facts remain opt-in and contain no planning policy. Actual unchanged-
  save Debug5644b1feb0f34983ae92125fa1e00982 proves owned Root consumption
  7->6 with145->194HP and6->5 with142->192HP; its later native loot wait
  remains a blocked run, not a completed route. Source-backed ready-loot
  Square input is now wired with independent readiness/nonmutation controls.
  Focused original-room Debugf7ef3bb91c7041cabefbcd42792bcea5 defeats both
  actors and exits normally3611 with0 errors/forced skips/timing failures;
  that fixture did not produce loot, so it is not live loot-claim evidence.
  Larger Minotaur-Zombie item probes verified recovery but still lost and
  remain failed. No boss showcase, universal navigation or further general
  model development is part of the closure claim. Publication/exact-CI and
  issue disposition are recorded on #151 before moving to #149.
- External-player follow-up on dirty `315549db` adds measured-stall terrain
  pursuit without replacing successful ordinary combat, preserves native
  target/reward/menu/reactor priority, and lets the model explicitly choose
  an original onward door instead of requiring extermination of every room.
  Living foes remain living in all metrics; native door flags/locks/loading/
  entry events and the120-tick control gate are unchanged. Steep descents
  require the same normal sheathing as climbs; failed walking descents can
  propose a logged directed jump before rejecting that landing.
  Source-backed defeated-target continuation controls are red/green:
  no new offensive press at observed0HP, pending target-defeat cancellation
  is separate from timing success/failure, and actual native rejection is
  never hidden. The original806dcaf9 failure is retained, not relabeled.
  Live Debug `cd172b3c03a041f1b3031977dc4dfd84` completes six physical legs
  with clean host/trace exit20120, including the second flag180 gate
  7d000->13. First control gates are4722/6713/9345/13930/18250/20119,
  arrivals2106/5589/7649/13016/16296/18673. Native counters record twelve
  hostiles observed/eleven at0HP, three accepted/useful chains and seventeen
  accepted/useful defenses,0 timing failures/errors/forced skips. It uses
  the unchanged postboss source save and original controller traversal;
  leaving one enemy alive is not credited as defeating it. This is a
  bounded six-leg proof, not successful completion of the longer route.
  Earlier Debug `eaa79a6c6ab54ac6bb286187b2e0ca7d` also reaches the second
  gate but later dies in13; the failed run remains failed. Its native
  observations confirm no learned healing spell, despite50MP, and existing
  consumable stacks (including IDs323/324). The external tool currently
  supports spell healing but not using those items. Next wire ordinary
  inventory use and require both count decrement and actual HP recovery.
  Do not grant Heal, edit HP or silently pretend a usable spell exists.
  Independently verify the new gate in Release and target9d000 without
  gratuitously replaying already-proven physical branches. #151 remains
  open; original matrix, later187/190 requirements and unverified limits
  retain their previous status.
- Owner-approved player boundary: all player-only collectors/types now live in
  `Diagnostics/PlayerObservation.*`. Desktop and Combat Lab require explicit
  `--player-observations`; the managed external runner supplies it. Ordinary
  `Boot/Encounter.cpp` and `Battle/Encounter.hpp` inspection are restored
  exactly to pre-player `b1703e1d` content. Normal traces omit player schema,
  geometry, reaction/options and extra actor fields; invalid-player-geometry
  controls prove the default path does not invoke collection. All inference,
  navigation, retrieval, tactics and controller actions remain outside the
  game. Geometry collection includes native body clearance, current jump/
  climb state and stationary object support, without mutating contact state.
  The original `Field_GimmickSupportHeight` query is intentionally not called
  because it writes the supporting-object index.
  Fixed model route-cost comparisons and the corner-jitter detector: sideways
  motion no longer resets progress without a new best waypoint distance.
  Route endpoints, actual displacement and failed attempts feed later plans.
  SQLite now retrieves measured local navigation evidence by room/original
  door/exact geometry, retaining provenance across raw-log cleanup. This is
  bounded local RAG, not weight training or proof that a whole route works.
  The game still reloads the explicitly selected unchanged save per run.
  Fresh dirty-`d5a54172` Debug `0f1f822bf00b4ddaa0109ddac742058f` verifies
  17d002->12,12d000->11,11d001->10 and earned flag180 gate10d000->13.
  Arrivals2136/5713/7757/9866; first120-tick control gates
  4856/6839/9147/10812. Flag180 remains the untouched save's4.
  Normal sheathing, source-gated native ledge climbing and stacked support
  resolve the previous11d001 obstruction. Its eight-leg observation remains
  budget-blocked in13 with one live higher-terrain hostile; the recorded
  12/7 chain/defense timings and4/7 useful effects are not universal perfection.
  Independent Release `04bfcfbe0c884e478557ecb3a540a651` passes the same four
  physical legs at5778/7959/9117/12840 (arrivals2112/6903/8889/12216).
  Its native earned-ability menu is selected through observed enabled rows,
  normal Circle inputs11206/11299, and action24 becomes genuinely learned.
  Earlier Release `5c04d159bf5c43a58d8ef3c644981114` remains blocked at that
  unanswered earned prompt; it is not retrospectively passed. The tool does
  not grant the skill or edit reward state, and future runs must still obey
  their actual save's learned-action list.
  Release records8 accepted chains/14 defenses and3/14 useful effects,0 timing
  failures/errors/forced skips, clean host/trace exit12840. Nine hostiles
  were observed and six reached0HP: physical control acceptance is not a
  claim that all destination enemies died or campaign teardown ran.
  Source save SHA remains the postboss hash below. Evidence is local under
  the Player run IDs; successful Release proof is explicitly pinned.
  Next: the remaining7d000/9d000 flag180 gates require continuing through13.
  Current ordinary combat approach can still walk into geometry toward a
  higher enemy; expose the original door objective to strategic choices and
  use geometry-aware approach/avoidance rather than forcing every foe dead.
  Later187/190 requirements remain unearned. Historical1001-ID results stay
  unchanged; #151 is not closed by this checkpoint.
- #151 external-player follow-up adds continuous original-record routes,
  per-leg audit checkpoints, bounded native Square/axis recovery and empty
  learned-chain assignment through normal menus. #222 records the discovered
  same-position targeting loop: native no-target feedback was discarded while
  a horizontal rough reach check kept reopening targeting above an enemy.
  A red/green regression now preserves target/scene identity and the rejected
  player position; a fresh query requires16 world units of actual movement,
  including vertical movement, without guessing native hit eligibility.
  On dirty `0c425c67`, Debug `a99a5dcbf5da48f686a092c2c5a9de58` maps the
  save's learned22/28/23, moves off the elevated rejected position
  `[448,-640,1802]` through normal directional inputs, then confirms the
  native target and observes Heavy Shot13HP damage. Four original hostiles
  die across two rooms, eight chain/one defense timings are accepted, four
  chain/one defense effects are useful, including Gain Life7HP recovery.
  It accepts17d002->12 at4332 and12d000->11 at6007. The following11d001
  remains blocked at `[568,0,310]`: two directed jumps change height but not
  horizontal position, and two bounded axis corrections also fail. The
  owned window closes normally at6547 with0 engine errors/forced skips/
  timing failures; this is a blocked route, not a passed flag180 gate.
  Independent Release `2c73bc3b4ac74af6840759331b8f028a` passes the first
  two physical legs at3660/5957, clears four original hostiles, and records
  six/two accepted chain/defense timings and two/two useful effects, with0
  engine errors/forced skips/timing failures and clean host/trace completion.
  Both use the same untouched postboss source save hash documented below.
  Player regressions now pass49 tests. The historical matrix is untouched.
  Next discriminate the original physical interaction/lock in front of11d001;
  its transition-record center is not proof of the physical object's reach.
  Do not repeat the successful prefixes without a new discriminating change.
- #151 now has a reusable standalone local-model player in
  [Native/Player](../Native/Player/README.md), not scripted combat timelines.
  It reads additive, read-only production observations, makes constrained
  tactical decisions, uses normal menus/targeting and delivers exact-window
  reaction inputs to its own PID/window without focusing it or accepting the
  owner's gamepad. Source-verified semantics prevent Ward/Siphon Soul/magic
  reflection from being selected against ordinary damaging nonspell hits.
  Current live dirty-`b1703e1d` controls use the declared `all-abilities`
  production-room fixture, no enemy/stat overrides:
  Debug run `2f94547233c54c478867415939bb8a47` maps Reflect Damage43 and
  replaces ineffective MP-only Mind Assault24 with Temper28 through menus;
  actual reflections are4/1HP, three chain timings/two defense timings are
  accepted, one chain and both defenses have independently observed useful
  effects, both enemies die, quiet acceptance2496, normal teardown3612.
  Release `1096cfb57e984cc7bbcfabf3a1aac8fa` verifies the same mappings,
  reflections5/2HP, three/two accepted timings, one/two useful effects,
  acceptance2500 and teardown3612. Both have0 engine errors/forced skips/
  missed/rejected/unconfirmed timings; maximum measured observation age is
  4.104ms/2.295ms. Accepted chains against an already dying target retain
  `no_observed_effect`, not invented damage.
  Separate Debug `b45cabc1fb72486d929c51c4393af62d` verifies native Herakles/
  Prostasia status application; `376c652dce464dc697ad3f2e6e91b6fe` verifies
  Heal73->132 and47->106 against an original Minotaur Zombie, but remains
  budget-blocked rather than a victory. All-abilities/spells are explicit
  Lab preconditions, not earned campaign skills. Successful acceptance runs
  above and the physical proof below are explicitly pinned against cleanup.
  Evidence lives in `%LOCALAPPDATA%\VS-NativePort\Player\runs\<id>`;
  model/runtime, raw logs, frozen images and saves are not committed.
  Asset-independent player tests pass44 cases; native Debug observation/
  door tests pass40/490, Release plus `[ability-effects]` passes54/1353;
  Debug's separate `[ability-effects]` passes14/863.
  Pylance diagnostics timed out during final checking; actual Python tests
  and scoped Ruff checks pass instead, with no claim of fresh editor approval.
  Production Continue from the untouched postboss save SHA-256
  `dda977497fd0bdf9e7cf3bc52cc85509546a3f4e622e7d6f53588f3ee07e3946`
  independently traverses9/17d002->9/12 in Debug
  (`f0e813ba6b3c4b10bf87210e59e31a3c`): Cross1709, arrival1854,
  useful reflections4/4HP, both original hostiles defeated, quiet starts3510.
  The first120-tick threshold is3630; the old raw report's6000 recorded the
  last qualifying tick, not first acceptance, and is preserved unchanged.
  A regression now keeps first acceptance stable. Desktop tick-limit/trace
  completion is not campaign teardown. Next use this unchanged legitimate
  progression for12d000->11d001->10d000->13 and its flag180 gates.
  Straight-line navigation still lacks complete lock/terrain/platform logic;
  caster/status matchups and ranked/area spell UI remain uncertified.
  #151 stays open; the all1001-ID historical loader matrix is unchanged.
- #151 remains open at the owner's request: continue resolving physical/story
  gates, rather than close on inventory/tooling coverage. The original matrix
  is unchanged: all1001 unique IDs match its original inventory, latest
  761 passed/238 blocked/2 failed; all764 retained/prior-positive loader proofs
  still satisfy their recorded initialization/error/teardown/120-tick gates.
  New controller-only observations on dirty `a060e2d2` resolve two additional
  combat-blocked physical routes in both builds:9/5->9/6 reaches the unchanged
  quiet gate at2614 in Debug/Release, with Ashley225/218HP;9/4->9/3 reaches it
  at1270/1086, with203/246HP. Both use original room actors and real attacks,
  with0 engine errors/forced skips and complete teardown. The initial9/5
  attempt did not hold120 ticks and remains recorded; a targeted controller
  retry, not a threshold or HP change, settles the remaining enemy.
  Fresh normal-speed Debug9/7->9/6 also matches the retained older Release
  proof: arrival230, normal attacks reduce73->27->0 and42->0, actors leave
  the live snapshot by1548/1838, quiet control1840..2999, acceptance1960.
  No direct HP/story/actor/position writes or fixture enemy/stat overrides.
  These are declared post-prologue production-room fixtures, not a full-story
  claim, and do not rewrite the device-free runner's historical blocked rows.
  Inputs/results and frozen matching images are under
  `build\quality\issue151-physical-20261028`: Debug uses `room6-sequence.json`
  at speed1, `nearby-sequence.json` and `room5-retry-sequence.json` at speed8;
  Release uses `release-sequence.json` at speed8, all with `--sequence`,
  `--scripted`, explicit `--data` and private `--save-dir`.
  Strict reports: `room6-Debug-verified.json` and `nearby-physical-verified.json`.
  Focused `[doors],[disc-door-inventory]` tests pass24 cases/41,767 assertions
  per build, including the independent full-disc field comparison.
  Next: untouched pre/post-Minotaur saves already have flag180=3/4, satisfying
  the three flag180>=2 exits. Their187=1/2 and190=0 do not satisfy the later
  thresholds; no flags will be manufactured. Remaining combat, authored
  redirects and unavailable/conditional records retain their distinct limits.
- #167's owner disposition (2026-10-27): close as **unreproduced/not planned**,
  explicitly without claiming a fix. The historical ACFD8139/5B64FBA0
  scene30 malformed packet and C++ termination remain confirmed; the
  matching image/PDB and original failing logs are no longer at their recorded
  paths, and no causal producer repair or matching historical stack is proven.
  Existing Debug/Release standalone records `1793063834761061-0` and
  `1793063834657676-0` restore control at13320 and tear down at13452,
  with280 authored dialogue confirmations and0 engine errors each.
  Recorded132-case histories are95 passed/37 blocked in Debug and
  96 passed/36 blocked in Release, with0 engine errors and complete teardown;
  their final3600-tick case remains budget-blocked, not a passed door.
  Debug's recorded script1050 matches scene30/32/17/PC233/wait70 and
  models0/170/169/10 without the historical packet failure.
  These are existing recorded observations, not fresh replays of the current
  documentation revision. No historical rebuild, renderer workaround or new
  gameplay verification accompanies this disposition. The original causal-fix
  acceptance gates are not marked complete. Reopen with a current failing
  packet plus matching executable/PDB and exception/producer evidence, or
  recovered historical artifacts. Evidence remains under
  `build\quality\issue42-final\nonma167-{Debug,Release}` and
  `build\quality\issue160-native\scene167-persistent-debug-verified.json`.
  This owner disposition supersedes older keep-open statements below;
  no shared root with #26, #184 or #185 is established.
- #218's event-dispatch audit now includes registered owned opcode headers,
  fixing the missing F01130 declaration without broadening the C ABI or
  weakening table checks. On dirty base `f0326749`, the published audit
  reproduces exactly one missing-handler error; the repaired and exact-staged
  audits check all 256 normal/256 skip entries and 179 unique handler names
  with zero errors. Three asset-independent controls cover owned/generated
  selection, unrelated-header exclusion, shared-header deduplication and
  absent/empty/whitespace registry contracts. All 156 exact-staged copier
  tests pass (157 in the owner-dirty tree); focused tooling quality is
  zero findings/zero new. Commands are in [QUALITY.md](QUALITY.md#event-dispatch-contract-audit);
  local evidence is `build\quality\issue218-*`, with publication/exact-CI
  closure evidence on #218. This supersedes older unpublished #218 notes
  below; no gameplay, compatibility status or unrelated owner work changes.
- #148's exhaustive source-backed naming/retention review is complete for
  the sweep scope, including all 28 inline-template producers. The 570
  individual binding/snapshot records in
  [NATIVE_SYMBOL_REVIEW.csv](ledgers/NATIVE_SYMBOL_REVIEW.csv) cover every
  remaining lexical occurrence: 1,873 occurrences/362 file-name pairs in the
  dirty tree and 1,871/361 in committed sources, with zero uncovered or extra
  active positions. Unknown/mixed roles, unused formals and meaningful
  SDK/math names have specific justifications, not blanket whitelists.
  Published implementation checkpoints through lifecycle `b41e4343` have
  exact green CI (`40592629849` for that revision). Latest Debug/Release
  render tests each pass 150/1,804,142 and lifecycle tests 71/7,094;
  all 742 generated and 121 owned paths are checked across stable repeats.
  Final fresh reference indexing covers 138 translation units/100,172
  occurrences with zero unresolved/text mismatches and exact generation-map
  equality; 39 naming tests and focused inventory-tool quality pass.
  Coupled copier/masking defects #219/#220/#221 are fixed and closed.
  The maintained inventory command and per-cohort evidence are in
  [semantic sweep evidence](NAMING_CONVENTIONS.md#116-native-semantic-sweep-148-2026-10-27).
  Final publication/CI and issue-closure evidence are recorded on #148.
  Naming does not certify all gameplay, C++ promotion or compatibility
  retirement. The following resumed-work entry is historical #148 context:
  save, scene-catalog, owner guidance and the owner document deletion
  remain separate uncommitted work; do not stage them with #148.
- Owner resumed #151/#167/#18/#148/#149 as continuing work, not a request to
  stop after the checkpoint-resume feature. Local dirty-base804d1899 now has
  two verified #148 naming batches:12 vibration functions/31 identifier pairs
  and180 opcode-ledger rows/353 parameter bindings (177 unique native
  names/347 unique pairs). Event instruction/thread names
  follow the existing182-ID opcode contract; literal/type/operator tokens and
  all121 owned paths are preserved, with stable repeat generation. Remaining
  lexical candidates are24,835 occurrences/2,158 pairs/371 files, not a claim
  of full semantic understanding or architectural retirement.
  #149 adds `--catalog-only --scene-catalog`: all512 MPDs/512 EVTs/797
  SEQ/ESQ banks/one movie, room gates and bounded branch/loop/thread graphs.
  Debug/Release/repeated Debug produce identical1,822-asset inventories with
  zero parse errors; playback/preconditions stay explicitly unverified.
  Parser8/39, optional all-disc2/46,935, full Debug/Release1032/9,578,509
  each and focused compilation-based quality0 new pass.
  Validation also exposed #218: the dispatch audit omitted owned opcode
  headers. Its declaration selection is repaired with red/green controls;
  actual normal/skip audit179 unique targets now has0 errors.
  These milestones remain local/unpublished; no issue closure is implied.
  #148 additionally corrects F01099's misleading preload name to the actual
  actor-cache clear; before/after1/22 controls preserve both room/zone keys.
  F03463-F03467 now have10 source-backed allocator local/parameter names and
  accurate native-header-unit descriptions, not an incorrect fixed16-byte
  alignment promise; before/after Debug/Release heap2/22 and repeat generation
  pass. These are names/contracts, not allocator or architecture changes.
  #151 now accounts for every1,001 original ID: latest761 passed/238
  blocked/2 failed,0 unrun. There are764 prior-or-latest positive proofs,
  kept distinct from fresh execution; do not replay them routinely.
  The two failures are known158's correct missing-required-actor guard:
  profile3 cannot establish Minotaur stage8/scene1/flag187<=1.
  Actual unchanged preboss/postboss native saves now verify production
  Continue and physical9/17->12/0/return in both builds: real model53/E222,
  first-entry gate4050..4267; postboss return3066/control through3900;
  zero errors/forced skips, no HP/flag/actor edits. Later Debug idle defeat
  is explicitly not combat success. Original failed fixture rows are unchanged.
  #167's ACFD image is absent from31 local executables. A distinct private
  Win32 debugger now fills the16MiB free arena twice withCD before allocator
  initialization; current scene30 still passes13320/control and13452/teardown,
  280 confirmations/0 errors. No causal producer fix is established.
  #18 still requires its separate migration/import/clear/campaign gates;
  its stale decoder/save audit descriptions are corrected, not retired.
  A separate source-backed clear-save implementation is actively in progress;
  do not overwrite its owned Save files or shared copier changes.
  See [continued acceptance](HANDOFF_HISTORY.md#continued-non-ma-acceptance-2026-10-27).
- Owner-directed #151 retry optimization now adds `--door-resume
  <sequence-results.json>`. Subsequent fix runs must reuse a checkpoint and
  select the broken case/from point, not replay the132-stage passed prefix.
  Complete history is a deliberate exception for full regression or a
  demonstrated history-dependent reproduction.
  Passed rows are removed from execution, not deleted from evidence:
  initialization/zero-errors/teardown/120-tick gates and the current door
  contract are checked; original source provenance is retained separately.
  Blocked/failed/running/unrun cases remain pending. Chained retries retain
  earlier passes; already-passed selections execute zero cases.
  Focused8/39, full Debug/Release1020/9,578,414 each, focused/default
  quality0 new pass. Actual Debug/Release resumes retain95 prior passes,
  run only z048-r006-d000 and pass at13452; a chained retry retains96 and
  executes0. Missing checkpoint exits1, never restarts history.
  Publishedc9f6b43a has exact green Source Quality40445126847.
  This is resume/planning coverage, not all-door completion or a #167 fix.
  See [checkpoint resume](HANDOFF_HISTORY.md#passed-stage-checkpoint-resume-2026-10-26).
- #160 now has a locally verified native material and target-placement repair,
  following owner authorization for behavioral modernization and fresh live
  confirmation that Gravity is on Ashley. E084 explicitly maps its missing
  logical channel1 to its own sole authored source table0, with bounded
  frame/descriptor validation and stale-channel clearing. The offscreen
  placement was independently traced to G00532: original800F5310 is
  G00529.position at+0xE0, not independent SVECTOR[109] storage. The existing
  field-view generator now preserves that canonical8-byte alias.
  Material red2/8 has3 failures; placement red2/11 all fail before repair.
  Full Debug/Release1012/9,578,375 each,133 port/32 naming,621 fields/0
  mismatches and two stable generations1,031 outputs/121 owned paths pass.
  Focused/default quality0 new; required full tidy exits0 with294
  existing/0 new and unchanged baseline.
  Normal-speed original Gravity renders beyond1508, clears1692, restores
  control1726 and retains45/45/46/46 damage; later fixture defeat is a
  blocked door result, not a passed edge. Owner visual acceptance is recorded
  on #160, now closed. Source142491cc plus generated Runtime view706c4159
  and output guard2e40a321 have exact green40438987477/40439217035/
  40439326171. No artistic/all-spell or passed-door claim is implied.
  Ghidra12.1.4/MCP is now live on analyzed BATTLE.PRG in ignored build\ghidra.
  Separate #167's exact132-case persistent Release prefix completes with
  96 passed/36 blocked and0 engine errors on the preserved22fbc737 image;
  no malformed producer or causal fix is inferred.
  Matching current132-case Debug history now completes95 passed/37 blocked
  with0 engine errors and complete tracing. At case132/script1050 it
  matches the historical scene30/32/17/eventPC233/wait70/models0/170/169/10
  without malformed packets. #167 stays open: no causal producer repair
  is established. Next evidence is a current failing packet/producer stack
  or the historical ACFD matching image, not Ghidra startup or byte matching.
  See the [Gravity checkpoint](HANDOFF_HISTORY.md#gravity-material-and-canonical-target-origin-2026-10-26).
- #42 is closed after final source98d38089 and evidence011eecc3 both received
  exact green CI40428688667/40429529689. Separate non-MA #197 now repairs
  the newly inventoried unchanged-source header/save quality findings:
  named dimensions/states, C/C++ null spelling, size_t arithmetic and
  sampled-data member naming; container phases are split without changing
  valid state values or copy/cleanup order. Existing save assertions remain
  intact; one invalid-state diagnostic regression is added.
  Focused tidy3 existing/0 new, save19/1,426 and full Debug/Release
  1002/9,572,050 each pass;131 port controls and two stable generations
  preserve119 owned paths/1,028 outputs and the unchanged quality baseline.
  Repeat full tidy reached304 existing/10 new; the ten residuals were four
  pre-existing test files. Those are now repaired with original byte checks,
  fixture values and assertion counts preserved. Fresh four-file tidy0/0
  plus exact-source incremental verification leaves294 existing/0 new,
  baseline unchanged. Production22fbc737 has exact green40431257034;
  final test cleanupeb0e49a2 has exact green40433467658. #197 is closed.
  #160 still AVs at1508 in both current builds; original E084 table1
  binding/history is the next required observation. Portable reference
  cards/states are empty and Ghidra MCP is unavailable. #167's current
  standalone scene chain passes; the historical producer remains unresolved.
  No texture/default/packet-skip fallback is authorized.   The earlier E084 reference blocker is superseded by the verified owner-
  authorized native material correction and canonical-origin producer fix.
  #18/#148/#149/#151/#167 remain open with their distinct acceptance
  limits; neither selected replay nor naming/coverage inventory closes them.
- #42's last seven entries are resolved locally: F01805/F01806 native
  navigation (`15d290bc`, exact green40421506975); F00745/F00747 bounded room
  rendering (owner checkpoint `506f750b`, exact green40425198955); F01769
  native monotonic frame-work timing; and justified retirement of F03575's
  unused stack capture and F00766's unused executable-room dispatch.
  The original186 cohort is184 implementations/2 retirements, not186
  translated bodies. Fresh audit0 gaps/3139 implementations/161 aliases;
  SDK/platform coverage, MA retirement and full-game parity remain separate.
  Full Debug/Release1001/9,572,047 each, focused20/262,416,131 port/32 naming,
  621 native fields/0 mismatches, two stable generations/1,028 outputs/
  119 owned paths pass. Focused tidy0 new. Full tidy completed and exposed
  pre-existing header/save debt now tracked separately in #197; its eight
  source files are byte-identical to506f750b. Three introduced include-order
  findings are fixed in98d38089, exact green Source Quality40428688667.
  Normal-speed private-save Debug/Release New Game reaches MAP001/6000,
  with4,586 scripted-control frames and0 errors each. Real Lab AI targeting
  and movement runs1800 ticks each without errors; no damage or full-combat
  parity is claimed. Final implementation is c61d4eab plus98d38089;
  original-cohort closure does not certify the #197 whole-tree quality gate.
  Separate non-MA work proceeds after reconciling #42.
  See the [final-seven checkpoint](HANDOFF_HISTORY.md#final-seven-assembly-gap-resolution-2026-10-26).
- #42 F01805/F01806's existing scripted-navigation draft is now source-reviewed
  and locally verified against `7C7E0.s:216-421`. It preserves full native
  allocation, original repeat-request base/route aliasing, countdowns,
  signed arrival arithmetic, locomotion dispatch and route release.
  Focused19/197,756 and full Debug/Release993/9,506,238 each pass; three
  isolated invalid-identity/map/list controls diagnose and exit3.
  Port130/naming32,621 view fields and focused/default quality0 new pass.
  Two generations are stable across1,030 native source/header files.
  Isolated normal-speed New Game reaches MAP001 and tick6000 with4,586
  scripted-control frames,0 engine errors and complete tracing. This is
  component/production-loop evidence, not all-cutscene movement parity.
  The local audit has5 gaps; publication and exact CI remain separate gates.
  See the [scripted-navigation checkpoint](HANDOFF_HISTORY.md#scripted-navigation-draft-verification-2026-10-26).
- #42 F01694 now updates native route availability from visible moving
  platforms while retaining the original mutable shift operands. The source
  domain audit finds418 valid room auxiliary sections and no authored
  first-word second-only case requiring a source stack-register value; such
  unsupported input is diagnosed explicitly. Focused7/1,033, full
  Debug/Release981/9,309,515 each, three fatal controls,129 port/32 naming
  controls, and focused/default quality0 new pass. Two generations repeat
  across746 outputs and preserve117 owned paths;621 view fields match.
  A frozen normal-speed Debug replay reaches MAP001/1800 ticks with0 errors,
  leaving the owner's live Desktop and log untouched. Audit:179/186 complete,
  7 gaps (4 C-backed/3 assembly-only),3134 implementations/161 aliases.
  Revision/exact CI are on #42. Work continues through all non-MA issues;
  this is not a stopping or whole-game acceptance checkpoint. See the
  [route checkpoint](HANDOFF_HISTORY.md#moving-platform-route-availability-2026-10-26).
- #42 F01826/F01827/F01836 now own typed gimmick height-grid occupancy and
  actor contact release. Original placement layer bits, aligned/bounded
  indexing, halfword wrap and command/support/contact gates are preserved.
  Focused10/1,313,815 and full Debug/Release974/9,308,482 each pass.
  Three isolated invalid-placement/map controls emit diagnostics and exit3.
  Normal-speed Debug New Game reaches MAP001/008/009 and tick6000 with no
  errors; this is room-entry smoke, not full prologue/combat acceptance.
  Default/focused quality0 new,127 port/32 naming controls,621 native-view
  fields0 mismatches, and two stable generations across746 outputs preserving
  117 owned files pass. Audit:178/186 complete,8 gaps (5 C-backed/3
  assembly-only),3133 implementations/161 aliases. Publication/exact CI are
  on #42. See the [occupancy checkpoint](HANDOFF_HISTORY.md#gimmick-and-actor-contact-occupancy-2026-10-26).
- #42 F01681/F01682 now own Ashley-alliance entry/exit in `Relations.c`.
  The original `7399C.s:11-85` controls BIOS RNG, a byte reaction counter,
  native callback save/restore, markers, relationship rebuild and recovery.
  The resumed draft's count255 truncation error is corrected using the
  original full-register comparison. Focused10/230,618 and full
  Debug/Release964/7,994,667 each pass, including real action-reaction and
  saved-relation callers. Default/focused quality0 new;126 port/32 naming
  controls;621 native view fields0 mismatches. Two stable generations
  preserve117 owned files across746 outputs. Audit:175/186 cohort complete,
  11 gaps (8 C-backed/3 assembly-only),3130 implementations/161 aliases.
  Fixing revision and exact CI are recorded on #42. This is component and
  production-caller evidence, not a live-combat alliance sweep, C++ promotion
  or compatibility retirement. See the
  [alliance checkpoint](HANDOFF_HISTORY.md#ashley-alliance-entry-and-exit-2026-10-26).
- #42 F01986-F01995 are integrated and component-verified, with a build and
  cumulative effects/module regression gate between each entry. Original
  PLG073/074 images are byte-identical. Eight stateless entries reuse the
  established typed native templates; the origin and emitter have separate
  PLG074 bindings and an independent imported emission cursor. Original
  symbols, assembly provenance and all ten audit records remain intact.
  #196 also repairs G00659's missing 32-byte ring-arc color import; all eight
  modes are tested. Final focused selection: 13 cases/3,642 assertions.
  Full dirty-worktree Debug and Release: 957 cases/7,764,092 assertions each.
  Exact scoped source passes 124 port controls, 32 naming controls and default
  quality with 0 new findings; focused static analysis has 0 new findings.
  The published-scope audit has 13 gaps, not 11: the unfinished local
  F01681/F01682 edits remain excluded. The fixed cohort advances to 173/186.
  See the [PLG074 checkpoint](HANDOFF_HISTORY.md#plg074-template-integration-and-ring-arc-import-2026-10-26)
  for commands, staging isolation, source/data evidence and remaining limits.
  Fixing revision and exact-revision CI are recorded on #42/#196. This is not
  whole-spell controller/visual parity, C++ promotion or EC retirement.
- #42 F00752 now exposes a typed native packet-span/depth/bias link entry and
  reuses it for the existing room packet writer. Original
  `2EA3C.s:1036-1064` shifts projected depth by two, adds the signed high-halfword
  bias, and rejects depths outside `[8, 0x7FF)` before linking the packet into
  the ordering table. The native entry keeps the current quarter-depth input,
  uses checked native packet spans and ordering-table pointers, and centralizes
  the same clipping for base and subdivided polygons. Focused tests pass
  6 cases/3,456 assertions; full Debug and Release suites each pass
  941 cases/7,760,407 assertions. The 123 port controls and default quality
  check pass with 0 new findings. The regenerated audit now records 23 open
  gaps, 3,128 implementations and 153 aliases separately. This is locally
  verified and published in `7f523452`; exact Source Quality run
  `40377510616` passed. The local gap cohort is now 163/186, with 23 open.
- #42 F00748/F00749 now expose bounded typed triangle/quad subdivision
  entries sharing the existing recursion. Original2EA3C.s:612/752
  proves four recursive children plus three triangle edges, and four
  quad children plus four triangle edges. Native material state is
  explicit, levels are bounded0..3 by the source flags, and frame spans
  are checked against the active native arena. Input/material capture
  protects overlapping packet writes. The actual polygon queue uses
  these entries; recursive arithmetic/order remains unchanged.
  Red1/2 proves both missing ABIs. New9/2,349 covers every valid level:
  triangle1/7/31/127 and quad1/8/36/148 packets, exact lengths/CLUT/page,
  midpoint position/color/UVs, aliases, empty/short capacities, far/bias/
  off-screen rejection and both original bindings.
  Both hosts/full Debug/Release935/7,756,945 each,122 port/106 direct-CI
  pass. Focused quality has1 existing native finding/0 new; default0/0,
  baselines unchanged. Fixture screen-bound cleanup is224 (not240);
  restoring it fixes the intermediate legacy clipping regression.
  Two generations preserve311 files/repeat identically across748 outputs;
  selective projects preserve the owner solution. Local cohort162/186,
  24 gaps (10 C/C++ references,4 assembly-only,10 missing),3127 native
  implementations/153 aliases separately. No new visible feature,
  all-room/controller parity, C++ promotion or retirement claim.
  Private `roomsubdiv42-*`; publication/exact CI are tracked on#42.
- #42 F00746 now owns a bounded packed-coordinate decoder in RoomDraw.c,
  reused by the real polygon queue for both triangles and quads. Original
  2EA3C.s:506-544 loads three packed words, signed-byte deltas and a
  masked variable shift; the caller selects0/3 for unit/eight-unit
  coordinates. Native code captures all12 bytes before output writes,
  preserves signed16 positions and leaves vertex padding/materials alone.
  Red1/1 proves the missing typed ABI. New6/589,967 covers all256 byte
  values/all32 shifts on both delta corners/all axes, high shift bits,
  74 input/output overlaps, unaligned input, untouched fourth corner
  and original binding. Two extra production packet goldens verify
  actual unit/eight-unit projections; existing triangle/quad subdivision,
  materials, clipping and capacity checks remain green.
  Combined room selection12/590,222 and full Debug/Release926/7,754,596
  each pass; both hosts link. Port121/direct-CI105 pass. Focused quality
  has8 pre-existing legacy-test findings/0 new; default remains0/0,
  baselines unchanged. Named count/type alias and explicit byte snapshots
  repair initial new findings without weakening memory checks.
  Two generations preserve310 files/repeat identically across748 outputs.
  Only F00746 copied/final_location fields change; generated
  RoomGeometry.c/.h and ModulePointers.c change. Local cohort160/186,
  26 gaps (10 C/C++ references,6 assembly-only,10 missing),3125
  implementations/153 aliases separately. This consolidates already-
  working decoding, not a new gameplay or visual-parity fix, promotion
  or compatibility retirement. Private `roomdecode42-*`; scoped
  publication/exact CI are tracked on#42.
- #195/#42 fixes the existing map fog coefficient, not merely an export.
  Original SetFogNear32154.s:9 uses320; native used256. Red1/6 has4
  failures: at near768/projection320 the actual returned cue is1026
  instead of0, dimming RGB160/80/40 to119/59/29. Native projection and
  actual `Menu_Map_DrawRoom` triangles now retain their near-plane colors.
  A64-bit product also prevents signed overflow for large native near
  distances; existing disabled-fog, denominator-floor and saturation
  policies remain unchanged. F03404/F03408 now alias the existing typed
  native color/fog APIs; implementations stay3124, aliases151->153.
  New11/17,012 covers576 depth/projection tuples, all4097 valid cue
  factors, native policy/reentry/large distances, color aliases and
  original bindings, including the actual map packet producer. Combined
  fog/map14/17,049 and full Debug/Release920/7,164,627 each pass; both
  hosts link. Port120/direct-CI104 and focused/default0-new quality
  pass without baseline changes. Fixture ABI words/braces and C-header
  inclusion are repaired, not suppressed.
  Two generations preserve309 files/repeat identically across748 outputs.
  Fully aliased Lighting.c disappears; selective Engine projects remove
  that generated unit while Lighting.h and ModulePointers.c change.
  Owner solution/edits/references remain unchanged. Local cohort159/186,
  27 gaps (10 C/C++ references,7 assembly-only,10 missing).
  This is numeric and actual map-render component evidence, not a
  controller map-menu visual sweep, full GTE parity, promotion or
  retirement claim. Private `fog195-*`; fixing revision/exact CI and
  eligible closure are tracked on#195/#42.
- Read-only upstream check during this scope: local rood-boi remains
  e59320ca; remote default HEAD775d91cd adds3 commits (materials/relief/
  open-sky shadow, player-state APIs and clipped ClearImage). Local
  rood-reverse4edbdbf5 is63 commits ahead of remote default e0875ad8,
  not a stale checkout to overwrite. No fetch/checkout/reference edit
  occurs. Private `fog195-readonly-reference-updates.json` records
  exact heads/comparison/file lists; no upstream feature is assumed
  implemented in this project.
- #42 F03405/F03414 now expose separately typed source reciprocal-root
  parts and vector normalization in ScalarMath.c. Owner was unavailable
  for the contract question; the conservative choice preserves #22's
  accepted full-range native normalizer and all existing SDK-facing
  redirects, with no gameplay caller rerouted. Original32154.s:79/162
  supplies leading-sign counts, signed16 SQR/GPF components, imported
  root/prefix lookups and output ordering. All192 ordinary reciprocal
  entries match the integer formula in the read-only resident image.
  Zero/-1 parts preserve outputs; source-zero vectors need no table.
  Active lookups validate the existing resident import and bounds.
  Red2/2 proves both old missing ABIs; new13/660,061 covers all ordinary
  entries/scales, signed prefixes, every signed halfword/high bits,
  aliases/padding, immutable lookup data and both original bindings.
  Actual unit calls distinguish source4000,1000 from native3973,993
  and retain native full-range2364 components. Both hosts/full
  Debug/Release909/7,147,615 each,119 port/103 direct-CI and focused/
  default0-new quality pass with unchanged baselines. Oracle guards/
  fixture splitting repair initial findings without weakening outputs.
  Two generations preserve309 files/repeat identically; selective
  Engine/Game/Tests projects preserve the owner solution.
  Local cohort157/186,29 gaps (10 C/C++ references,9 assembly-only,
  10 missing); implementations3124/aliases151 separately.
  No gameplay rerouting, original CPU overflow traps/register state,
  live combat, C++ promotion or compatibility-retirement claim.
  Private `sourcenorm42-*`/`normalization-source-contract.json`;
  scoped publication/exact CI are tracked on#42.
- #42 F03399 is a proven alias of the existing typed rotation trig-pair
  kernel, not another native math implementation. All63 instructions/
  eight octant blocks in resident31724.s:147-211 match BATTLE2EA3C.s
  F00762 and48C7C.s F01017 after normalizing nop spelling; caller table
  setup is the same. The generator redirects its angle/pair API and
  resident80041104 binding to the canonical provider. Red1/1 proves the
  former missing ABI; new3/262,151 checks every signed halfword, wide
  angle metadata, pointer identity and typed original address.
  Full Debug/Release896/6,487,554 each,118 port/102 direct-CI controls
  and focused/default0-new quality pass; both hosts link. Two generations
  preserve309 files/repeat identically. Only F03399's copied catalog
  field changes; generated FixedPoint.c/.h and ModulePointers.c change.
  Local cohort155/186,31 gaps (10 C/C++ references,11 assembly-only,
  10 missing); implementations stay3122 and aliases advance150->151.
  Existing rotation gameplay stays unchanged; no controller parity,
  C++ promotion or compatibility-retirement claim. Private
  `residentpair42-*`; publication/exact CI are tracked on#42.
- #42 F01772 now has explicit native first-X/first-Z/packed-second inputs
  in the existing TileDistance provider. Original7BDD4.s:35 consumes t0/t1
  then shares the signed16 SQR entry. F01771 now delegates to this helper;
  the actual AI edge pursuit consumer passes its already-unpacked target
  coordinates, matching original70D4C.s:248. This consolidates an already
  correct packed-distance path, not a newly fixed gameplay behavior.
  Red1/1 proves the old zero-argument ABI; new4/522,652 cover all261,121
  signed byte-delta pairs through both entries,405 halfword-width tuples,
  metadata and the typed original binding. Combined ground queries
  15/522,750 and full Debug/Release893/6,225,403 each pass; both hosts
  link. Port117/direct-CI101 and shared Engine/Game C-header routing
  control pass. Missing Engine legacy includes are fixed in the existing
  shared generator/analyzer predicate, not ad-hoc build flags. Nested
  fixture assertions are extracted without reducing coverage. Focused/
  default quality is0 actionable/0 new with unchanged baselines.
  Two generations preserve308 files/repeat identically; selective
  Engine/Game/Tests outputs preserve the owner solution. Local cohort
  154/186,32 gaps (10 C/C++ references,12 assembly-only,10 missing),
  implementations3122/aliases150 separately. No controller combat,
  C++ promotion or compatibility-retirement claim. Private
  `tilecomponents42-*`; publication/exact CI are tracked on#42.
- #42 F01832 now invokes the existing special-terrain height builder during
  actual room initialization. Original7E05C.s:257 allocates32KiB only to
  switch the guest stack; the owned void adapter validates native maps and
  uses the existing16MiB native game fiber, without a fake heap allocation
  or host/guest SP manipulation. Red1/5 leaves all three connected cells
  unchanged (3 failures). New6/1,054 includes the full1,023-cell connected
  region on the game fiber, visited/high-bit preservation, unseeded regions,
  typed original binding and two actual room entries with cleanup/frame
  pointer checks. Combined special-terrain/room-context8/1,074 and full
  Debug/Release889/5,702,751 each pass; both hosts link. Port116/direct-CI100
  controls pass. Two generations preserve308 files and repeat identically;
  selective project outputs preserve the owner solution. Focused/default
  quality is0 actionable/0 new with unchanged baselines; publication
  evidence is recorded under `specialterrain42-*`.
  A fixture loop-index width finding is repaired rather than suppressed.
  Local cohort153/186,33 gaps (10 C/C++ references,13 assembly-only,
  10 missing listings); implementations3121/aliases150 remain separate.
  This restores room terrain preparation, not controller-tested enemy
  jumping, timing/navigation continuation, C++ promotion or compatibility
  retirement. Scoped publication/exact CI are tracked on#42.
- #42 F01833/F01834 now implement typed solid-volume and32-sample
  actor/terrain segment queries. Original7E05C.s:283/331-508 proves
  strict extents, actor/Gimmick priority/filters, signed-floor steps,
  ahead-of-start rejection, source15-bit terrain and id-plus-one pad.
  Actor Z/platform X unsigned-halfword quirks are preserved through
  typed members. Native contact storage replaces the original fixed
  stack; the result is an actual native SVECTOR pointer, not a guest
  address squeezed into int. F00514 consumes an explicit boolean and
  the typed action byte13/bit6 flag. Red1/7 has5 failures; new11/1,673
  verifies all256 kind bytes, reactions/locks, Gimmick mode-byte behavior,
  32-step rounding,343 volume-boundary tuples, five-platform collection,
  input/output aliases and both original bindings. Actual action resolution
  redirects a terrain-blocked positional projectile and preserves the
  source marker/animation path. Its fixture initially lacked the global
  action session; repository crash diagnostics identified the missing
  fixture context, and all temporary probes were removed.
  Both hosts/full Debug/Release883/5,701,697 each,115 port/99 exact-CI
  controls, shared interop and focused/default0-new quality pass with
  unchanged baselines. Two generations preserve307 files and repeat
  identically; four generated caller/header/symbol files change.
  Local cohort152/186,34 gaps (11 C/C++ references,13 assembly-only,
  10 missing listings); implementations3120/aliases150 stay separate.
  No controller-arrow/combat, all-room collision, promotion or retirement
  claim. Private `segment42-*`; publication/exact CI tracked on#42.
- #42 F02458 now executes the existing production screen-overlay radial
  warp callback. Original3D8.s:9 builds444 halfword scales, mutates eight
  polygon layouts around160/112 and walks scene OT2046 (not sky2047).
  Red1/7 has3 unchanged-coordinate failures. Owned native code preserves
  MFLO32 truncation, signed-halfword IR inputs, MIPS masked shifts and
  the original small-radius resident-prefix lookup, not ideal sqrt.
  Existing arctangent/root import owns the needed lookup bytes;192
  original root entries match the mathematical formula. Local arrays
  replace guest scratch, and native handles/Floyd validation replace
  guest pointer reconstruction without changing tags/root/frame state.
  New9/209,122 covers96 polygon/prefix/flag layouts,4096 signed coefficient
  tuples, all65,536 halfword coordinate samples plus the negative-sum
  corner, near-center/far clamps, identity, linked commands, immutable
  resident/frame/sky data and actual GIM800F9BD8 resolution. Combined
  state/warp15/209,205 and full Debug/Release872/5,700,024 each pass;
  both hosts,114 port/98 exact-CI controls and shared interop control
  pass. Focused/default quality is0 actionable/0 new with unchanged
  baselines; MIPS shift masking and fixture sharing fix initial findings.
  Two generations preserve304 files and repeat identically; only four
  generated caller/header/symbol files change. Local cohort150/186,
  36 gaps (12 C/C++ references,14 assembly-only,10 missing listings);
  implementations3118/aliases150 remain separate. No controller-triggered
  spell/cutscene, all-effect visual parity, promotion or retirement claim.
  Private `screenwarp42-*`; scoped publication/exact CI tracked on#42.
- #194 replaces G00512's original16-byte short-array backing with an
  owned, aligned native `Effects_ScreenEffectOverlay` object: native size24,
  request pointer offset16. Original5BF94.c:3481/3919 aliases the defaults;
  the old cast left the entire8-byte native pointer outside the object.
  Reset/acquisition and warp dispatch now use typed members/address.
  Red1/4 fails all storage/alignment/type checks without performing the
  invalid write. New6/83 assertions exercise actual enqueue, repeated
  acquire, release/reset/reentry, pointer canaries, parameter setters and
  original base bindings. A coupled red resolver check exposed omitted
  owned globals; G00512/G00330 now export opaque original bases with
  actual native sizes and no guessed interior aliases. Source16/4-byte
  extents remain separate. Both hosts/full Debug/Release863/5,490,902
  each,113 port controls, shared build/analyzer interop control and
  focused/default0-new quality pass. Two generations preserve301 files
  and repeat identically; only ScreenEffects.c/.h and ModulePointers.c
  change. No disc I/O completion, overlay execution, historical crash
  cause, promotion or retirement claim. Private `screenstate194-*`;
  initial4b0cf790 publication failed CI40322225799 because direct-script
  execution lacked the new cross-directory package import. The follow-up
  reuses the quality runner's file-path scaffold loader: exact CI entry97
  and full113 controls pass with all assertions retained and no native
  source/baseline changes. Final publication/exact CI tracked on#194.
  Cohort stays149/186 with37 gaps.
- F01769 timing remains gated under plan9.9/M2: deterministic simulation,
  original RNG and no wall-clock in game logic. Owner was unavailable;
  do not adopt the earlier live-monotonic suggestion. Next observation is
  original counter mode/baseline and per-consumer work/timing calibration
  before defining deterministic native costs and enabling navigation.
  F02458's independent warp milestone above preserves the observed
  small-radius lookup behavior, not ordinary SquareRootInteger; its
  source table bytes stay imported and out of commits.
- #193/#42 restores F01268 as an actual production actor-shadow pass,
  not an unused export. Original caller800B38FC..800B3924 proves
  companion0/flag8000 gating; the native draw path had omitted it and
  returned before it when body projection failed. Red1/14 has8 failures
  (zero instead of24 packets and no projected anchor). The owned typed
  actor/camera/root API now shares bounded native model packet storage,
  samples real terrain, preserves source-width scale/color/depth rules
  and imports18 vertices/24 faces through two runtime BATTLE bindings.
  No asset payload or reference code is committed. New11/5,492 covers
  production gates, absent body projection, actual two-axis/sloped and
  rejected-steep terrain, signed fades/full upper color word,360 wrapped
  scale tuples, three bands, winding/depth cutoffs, buffer exhaustion,
  typed resolver calls and imported strides. Both hosts/full suites pass
  Debug/Release857/5,490,819 each;112 port controls and focused/default
  quality0 actionable/0 new pass with unchanged baselines. Two generations
  preserve298 maintained/owner/reference/data files and repeat identically;
  selective Tests project generation preserves the owner solution.
  A normal-speed Release controller run1793013531362252-0 reaches
  MAP009 (read4057), shows Ashley's ground shadow at5900, imports389
  globals/2989 symbols with0 failures, and ends at6000 with0 engine
  errors. An earlier Continue-menu run is explicitly excluded.
  This is render/source-contract and one-view integration proof, not
  combat, full-cutscene or all-room visual parity. Local cohort149/186,
  37 gaps (12 C/C++ references,15 assembly-only,10 missing listings);
  implementations3117/aliases150 stay separate. Private `actorshadow42-*`
  and `actorshadow193-*`; scoped publication/exact CI are tracked on#193/#42.
- #189's classifier/table repair is now reflected in the generated report
  prose too: a parent repository HEAD is never a copied snapshot's
  reference revision. An isolated publication fixture was red on the
  contradictory footer; the correction preserves all38 gap records,
  coverage rows and reference source fingerprints. No native runtime,
  reference repository or quality baseline changes. Publication and
  exact CI are tracked on#189; last native gameplay/math proof remains
  the scoped3ee579a4 milestone below.
- #192/#42 restores F00761's staged YXZ room rotation, not the distinct
  F01018 ZXY builder or a sequential SDK matrix product. The actual
  identity-camera room-view regression at angles123/234/345 was red:
  m01=-1705 instead of the original-1704. Both normal and scale-compensated
  production views now share the typed source-specific kernel; destination
  translation/padding are untouched. F00762 returns explicit signed
  sine/cosine values from the validated canonical native trig functions;
  F01017's eight identical octant blocks redirect to the same pair.
  New5/276,023 assertions cover staged mixed-angle references, octants,
  all65,536 signed halfwords with poisoned upper words, source/output
  bytes, both real camera producers and all three original bindings.
  Combined room/math33/1,434,615 and full Debug/Release846/5,485,327
  each pass; both hosts and110 port controls pass. Focused/default
  quality is0 actionable/0 new; fixture findings were corrected
  without relaxing byte checks or baselines.
  Two generations preserve114 registered/owner/reference files,
  including both original assemblies, and repeat identically; only
  generated Matrix.c/.h and Trig.c/.h change. Local cohort148/186,
  38 gaps (13 C/C++ references,15 assembly-only,10 missing listings);
  implementations3116 and aliases150 stay separate. This proves a
  numeric production-path correction, not an observed visual complaint,
  all-room parity, promotion or retirement. Private `roomeuler42-*`;
  publication and final quality/exact CI gates are recorded on#192/#42.
- #42 F00751 now shares the production ordinary/subdivided room clipping
  predicate through explicit normalized projected bounds and an int
  visibility result, replacing original implicit t0-t3/AT operands.
  Original2EA3C.s:969/1101 proves inclusive left/top, exclusive right/
  bottom and signed16 setter immediates; the existing native state is
  reused without instruction patches or another viewport. Old int() ABI
  is compile-time red. New2/131,099 assertions cover65,536 deterministic
  full-width quad/configuration tuples against independent per-vertex
  rejection, wrapped setter words, every edge, spanning polygons,
  unchanged input and actual80098014 binding. Combined room15/786,740
  and full Debug/Release841/5,209,304 each, both hosts and109 port
  controls pass. The new fixture constant naming finding was corrected;
  focused/default quality is0 actionable/0 new, baselines unchanged.
  Two generations preserve297 maintained/owner paths and original
  assembly, repeating identically; only RoomGeometry.c/.h change.
  Local cohort145/186,41 gaps (13 C/C++ references,18 assembly-only,
  10 missing listings); implementations3114 and aliases149 stay separate.
  No all-room visual parity, promotion or retirement claim. Private
  `roomscreen42-*`; publication/exact CI are recorded on#42.
- #42 F00750 now exposes the existing native room-subdivision midpoint
  kernel through explicit workspace vertices, not saved registers or
  packed MPD offsets. Original2EA3C.s:921 establishes signed floor
  positions, unsigned RGB/UV averages and untouched remaining bytes.
  Existing triangle/quad production callers share the same kernel.
  Old int() ABI is compile-time red; new2/524,297 assertions cover every
  byte pair in every RGB/UV channel,65,536 signed-position samples,
  complete output/source/padding/canary bytes, all alias directions
  and actual80097F70 binding. Direct/room/tint13/655,641 and full
  Debug/Release839/5,078,205 each, both hosts and109 port controls pass.
  Focused/default quality is0 actionable/0 new with unchanged baselines.
  Two generations preserve297 maintained/owner paths and original
  assembly, repeating identically; only RoomGeometry.c/.h change.
  Local cohort144/186,42 gaps (13 C/C++ references,19 assembly-only,
  10 missing listings); implementations3113 and aliases149 remain
  separate. No packed-layout equivalence, all-room visual parity,
  renderer promotion or retirement claim. Private `midpoint42-*`;
  publication/exact CI are recorded on#42.
- #42 F03409 now implements native vector squaring from original
  32154.s:24. Input words narrow through the existing signed16 helper;
  full unshifted MAC products, not saturated IR results, are written.
  All operands are captured before writes, output pad is unchanged and
  the typed function returns its output. Old int() ABI is compile-time
  red; new2/327,688 assertions cover all65,536 halfwords on all axes,
  poisoned upper words, complete guarded output/source bytes, self
  aliases, a wide-word golden and actual80041980 binding. Combined
  core/trig13/371,852 and full Debug/Release837/4,553,908 each, both
  hosts and109 port controls pass. Focused/default quality has0
  actionable/0 new findings with unchanged baselines. Two generations
  preserve297 maintained/owner paths and original assembly, repeating
  identically; only FixedPoint.c/.h change. Local cohort143/186,43 gaps
  (13 C/C++ references,20 assembly-only,10 missing listings); native
  implementations3112 and aliases149 stay separate. No GTE-state,
  whole-math promotion or retirement claim. Private `vectorsquare42-*`;
  publication/exact CI are on#42.
- #42 F00763 now restores BATTLE's right-destination rotation product
  through the same native Q12 kernel as the resident matrix entries.
  Original2EA3C.s:1664 returns the right pointer and preserves every
  byte after its nine rotation halfwords. New typed ABI assertions reject
  the old int() stub;256 deterministic matrix pairs, both saturation
  signs, fractional floors, full-byte tails/other operands, self aliases
  and actual800989B8 binding pass. Combined core/trig11/44,164 and full
  Debug/Release835/4,226,220 each, both hosts and109 port controls pass.
  Focused/default quality is0 actionable/0 new, baselines unchanged.
  Two generations preserve297 maintained/owner paths and original
  assembly, repeating identically; only Matrix.c/.h change. Local
  cohort142/186,44 gaps (13 C/C++ references,21 assembly-only,10 missing
  listings); implementations3111 and aliases149 remain separate.
  No new arithmetic implementation, hardware state, all-matrix promotion
  or retirement claim. Private `battleproduct42-*`; exact publication/
  CI are recorded on#42.
- #42 F03406 now shares the native saturating Q12 rotation-product kernel
  with the existing right-destination entry; its typed API returns the
  left matrix and preserves all padding/translation bytes. Both operands
  are captured before writes, including self multiplication. F03568 is
  an alias of the existing empty Core::IgnoreCall: original main.c:11661
  is a static inline NOP, not a host sleep or a standalone fallthrough
  routine. The old int() matrix ABI fails the new compile-time contract.
  New4/1,812 controls cover noncommuting operands, both saturation signs,
  fractional floor,256 deterministic full-width matrix pairs, aliases,
  complete object bytes and the original80041C60 binding. Combined
  core/trig10/42,874 and full Debug/Release834/4,224,930 each pass;
  both hosts,109 port controls and0-new focused/default quality pass.
  Two generations preserve297 maintained/owner paths and three original
  reference files, repeating identically; only FixedPoint.c/.h and
  Runtime.c/.h change. Local cohort141/186,45 gaps (13 C/C++ references,
  22 assembly-only,10 missing listings); native implementations3110 and
  aliases149 remain separate. No native timer policy, whole-matrix C++
  promotion, audiovisual parity or compatibility retirement is claimed.
  Private `coreleaves42-*`; publication/exact CI are recorded on#42.
- #42 F01269 now shares the native ModelGeometry override kernel through
  typed weapon state, reaching139/186 original entries and47 gaps
  (13 C/C++ references,24 assembly-only,10 missing listings).
  Original49268.c:240 proves the weapon-base argument;3A1A0.h places
  its matrices at guest20, so original lbu64 reads matrix2 m02's low
  byte. The native adapter preserves that quirk via the member, never
  an actor cast or native byte64. Its logical rationale remains unknown.
  Red1/432 fails all; new2/435 and combined weapon/gimmick/actor6/1,048
  controls pass, including all states/group masks, a poisoned native-offset
  alias, source-layout assertions and original-address binding. Full
  Debug/Release830/4,223,118 each,108 port controls and both hosts pass.
  Focused/default quality has0 new findings with unchanged baselines;
  explicit bounded fixture assignments avoid implementation-defined
  narrowing. Two generations preserve297 maintained/owner paths and
  four original reference files, repeating identically; only Model.c/.h
  change. No invented mask meaning, all-weapon visual parity, renderer
  promotion or retirement claim. Private `weaponoverride42-*`;
  publication/exact CI are recorded on#42.
- #42 F01270 now adapts typed gimmick motion/highlight state to the existing
  native ModelGeometry packet kernel, reaching138/186 original entries
  and48 gaps (14 C/C++ references,24 assembly-only,10 missing listings).
  Original4B16C.s:2387 establishes all16 material states, group palette
  masks and uniform tint only for transparent overrides. Red1/432 fails
  every case; new2/435 and combined actor/palette4/613 controls pass,
  including all states, three masks, eight limb groups plus remainder
  and actual original-address resolution. Full Debug/Release828/
  4,222,683 each and107 port controls pass; both hosts link. Focused/
  default quality has0 new findings with unchanged baselines; exact-check
  fixture extraction retains all432 state/group checks. Two generations
  preserve297 maintained/owner paths and reference sources, repeating
  identically; only Model.c/.h lose the old stub/declaration. No new actor
  renderer, guessed weapon mask, all-model visual parity or retirement
  claim. F01269 remains separate: its original offset0x64 weapon read
  lies in matrix storage and must not become a native actor-state cast.
  Private `gimmickoverride42-*`; publication/exact CI are on#42.
- #42 F00769/F00770 now share an explicit native room-packet tint kernel
  in RoomDraw, reaching137/186 original entries and49 gaps (15 C/C++
  references,24 assembly-only,10 missing listings). Original2EA3C.s:
  1944/2018 establishes GT3/GT4 traversal, optional8-byte trailers,
  signed-byte*4 offsets and fixed-point5:9:2 luminance with saturation
  before offsets. Red2/5 fails all5; direct7/131,093 and real field/room
  integration13/131,346 pass, including every signed16 strength for two
  asymmetric colors, packet/code/UV/link canaries and both original
  function addresses. Full Debug/Release826/4,222,248 each,106 port
  controls, both hosts and0-new default/focused quality pass; focused
  LegacyGpu retains8 existing findings, not a changed baseline.
  Two generations preserve297 maintained/owner paths and reference
  sources, repeating identically with three expected generated outputs.
  #191 is the coupled public-header fix: transitive Field/Room.h now
  stays inside RoomDraw's C linkage block, removing demonstrated C++
  undefined symbols from the real field draw integration. No caller-only
  redeclarations or casts hide it. No all-room visual parity or
  compatibility-retirement claim. Private `roomtint42-*`; publication/
  exact CI and #191 closure are recorded on#42/#191.
- #190/#42 F00768 now exposes one typed room-light triangle kernel shared
  with the existing owned sampler, reaching135/186 original entries and51
  gaps (16 C/C++ references,25 assembly-only,10 missing listings).
  Original2EA3C.s:1857 uses signed32 NCLIP areas and unsigned32 weighted
  sums. The former widened64 kernel wrongly accepted a full signed16
  triangle whose original MAC0 area is -131071; red1/4 fails all4 checks.
  Final direct6/12,379 and combined existing-layer7/12,380 controls cover
  edges/corners/winding/degeneracy, both overflow widths,4096 signed16
  tuples, unchanged output/input canaries and original-address resolution.
  Full Debug/Release819/4,091,150 each,105 port controls, both hosts and
  0-new focused/default quality pass without baseline changes. Two native
  generations preserve296 maintained/owner paths and original reference
  files, repeating identically; only RoomGeometry.c/.h lose their old
  declaration/stub. Tests projects render selectively and repeat without
  rewriting the owner solution. No demonstrated owner-visible lighting
  cause, all-room visual parity, C++ promotion or retirement claim.
  Private `roomlight190-*`; publication/exact CI are on#190/#42.
- F01805/F01806 remain disabled as a coupled navigation package. The
  updater still requires unported F01769 frame-work timing, which feeds
  both AI budgets and RNG, plus a typed contract for cross-function
  L800DF8A4. Do not ship the request producer with a zero timer or a
  guest-register continuation fallback. The source/assembly trace is
  recorded in the session dependency tasks; no navigation code changed.
- #42 F01130 now implements the original Ashley weapon-mode opcode through
  typed native battle/animation/visibility APIs, reaching134/186 original
  entries and52 gaps (17 C/C++ references,25 assembly-only,10 missing
  listings). Original800B9DB4 and the read-only C counterpart establish
  gateAC, immediate/skip visibility and the shared pending animation byte.
  G00473 is now weaponTransitionPending, with its existing initialization
  reset and original address preserved. Red5/72 has51 failures; final
  7/471 drives real synthetic skeletal animation and both VM dispatch
  paths, including exact5/10-step queued-idle gates, non-restarting held
  idle, normal/skip completion, native table bounds and original-address
  resolution. Full Debug/Release813/4,078,771 each,104 port and32 naming
  controls pass; both hosts link and focused/default quality have0 new
  findings without baseline changes. Native generation repeats identically
  with five expected provider/global-binding outputs; selective Game/Tests
  rendering preserves the owner solution. Only resolved G00473 is removed
  from the trace queue; unrelated pre-existing regeneration drift is
  preserved and logged on#148. No all-scene gameplay, audiovisual parity,
  C++ promotion or compatibility-retirement claim. Private
  `weaponmode42-*`; publication/exact CI are recorded on#42/#148.
- #188 reconciles152 owned-function catalog rows against their actual
  registered definitions:146 obsolete provider paths and140 stale copied
  flags, with overlap. The regression checks all236 registered copy=yes
  function definitions. All4207 IDs/order and every other catalog field,
  including compiles/understood/modernized/cpp/verified, are preserved.
  F01758 was already implemented in WordCopy.c and is not a new gameplay
  fix or an unchecked original#42 entry. Native code, the manifest, owned
  registry, other ledgers and gap/coverage CSVs remain byte-identical;
  two generations change no native output. The53 real gaps and133/186
  cohort stay unchanged. Red has152 failures; all103 port/audit controls
  and0-new default quality now pass. An unrelated report-revision failure
  exposed the separately repaired provenance defect#189. Private
  `catalog188-*`; publication/exact CI and closure are on#188.
- #189 fixes reference revision attribution in the assembly audit:
  rood-reverseWin is an unversioned snapshot inside the native repository,
  not a checkout at the enclosing native HEAD. Only a matching reference
  repository root supplies a revision; copied snapshots are explicitly
  labeled unversioned and retain their source SHA/discovered-root metadata.
  Two isolated controls cover both branches without touching a repository.
  Native code, gap/coverage CSVs, source fingerprints and53 actual stubs
  remain unchanged; default quality has0 new findings. Private
  `provenance189-*`; publication/exact CI and closure are on#189.
- #42 F01845 now constructs the typed orbit motion state in the existing
  low arena, reaching133/186 original entries and53 gaps (18 C/C++
  references,25 assembly-only,10 missing listings). Original7E05C.s:
  1080-1095 ignores its settings argument, clears the complete32-byte
  record and sets cooldown900. Native value construction preserves that
  contract, with an explicit allocation-failure error and the existing
  AC/AD cleanup owning release. Red2/8 has4 failed assertions; poisoned
  reused storage, every byte, unused input, both cleanup categories and
  reconstruct/free cycles pass. Combined orbit/cleanup13/198 and full
  Debug/Release806/4,078,300 each pass; both hosts link,100 port/audit
  controls and0-new focused/default quality pass with unchanged baselines.
  Two generations preserve292 maintained native/owner paths and the
  original instruction/C reference files, repeating identically; only
  OrbitingBoss.c/.h and one existing caller include change. Exact boss
  identity, complete boss gameplay and compatibility retirement remain
  unverified. Private `orbit42-*`; publication/exact CI are on#42.
- #42 F01695 now performs the source-backed hostility transition through
  the existing owned reaction lifecycle, reaching132/186 original entries
  and54 gaps (19 C/C++ references,25 assembly-only,10 missing listings).
  Original73C84.s:246-284 sets classification3 and clears only bits8-14
  on every call. A newly hostile target additionally consumes one BIOS
  RNG draw, sets byte0x18, shows indicator6 for40 ticks and requests
  reaction2 through the existing actor-accumulator RNG/delay helper.
  Those two RNG streams must not be substituted for one another.
  Red2/68 has36 failed assertions; green hostility2/16,436 and combined
  lifecycle7/115,047 cover all128 counter values, all classifications,
  boundary/interior targets, other-bit/other-relation canaries, busy
  routes/delays and repeat calls without refreshing reactions or RNG.
  Full Debug/Release805/4,078,278 each and99 port/audit controls pass;
  both hosts link. Focused native/tidy and default quality have0 new
  findings with unchanged baselines. Exact-check helper extraction retains
  all16,436 hostility assertions. Two generations preserve111 registered-owned/owner/
  test paths and repeat identically, changing only Perception.c/.h.
  Existing transient AI views remain migration debt; no full relationship
  gameplay, whole-state C++ promotion or compatibility-retirement claim.
  Private `hostility42-*`; publication/exact CI are recorded on#42.
- #42 F01828 now performs the original guarded u16 tileFlagMap OR0x400
  through NavigationPrimitives, reaching131/186 original entries and55
  gaps (20 C/C++ references,25 assembly-only,10 missing listings).
  Existing trap/interaction callers use the typed native grid and existing
  32x32 bounds; missing-grid calls stay no-ops. Red3/1032 has1031 failures;
  final3/529,408 covers all65536 words at four corner cells, idempotence,
  all1024 coordinates, other-cell canaries, no-grid and rebind behavior.
  Full Debug/Release803/4,061,842 each,98 copier controls and0-new
  focused/default quality pass; both hosts link. Two generations preserve306
  paths and repeat identically, changing only Navigation.c/.h and one
  consumer include. Add/remove occupancy and workspace-builder stubs remain
  separate and are not enabled. Bit-category semantics, full trap/AI
  gameplay and compatibility retirement are not inferred. Private
  `tilemarker42-*`; publication/exact CI are on#42.
- #187/#42 F00756 now uses one signed16 native room clip rectangle in both
  ordinary/subdivided triangle/quad emitters, reaching130/186 original
  entries and56 gaps (21 C/C++ references,25 assembly-only,10 missing
  listings). Original2EA3C.s:1101 patches the same half-open predicate in
  func80098014. Red production fixture has2 failures/148 assertions:
  ordinary40/52-byte packets escape default y224..255 while subdivision
  rejects them. Original default320x224 is intentionally restored, correcting
  ordinary-only240; physical framebuffer, sky/camera offsets, near/far/LOD
  and memory bounds are unchanged.80 added packet checks cover all four
  edges/equality, tight/overlapping/hidden rectangles, signed halfword wrap,
  explicit240-bottom support and canaries; room6/248 passes both builds.
  Full Debug/Release800/3,532,434 each,97 copier controls, default0-new and
  focused8 existing/0-new quality pass. Two generations preserve306 paths
  and repeat identically, changing only RoomGeometry.c/.h. Private
  `clip187-*`; no owner-visible popping cause, every-scene parity, whole
  renderer promotion or compatibility-retirement claim. Publication and
  exact CI are on#187/#42.
- #42's F00754 far-depth limit is now a typed native RoomDraw setting,
  reaching129/186 original entries and57 gaps (22 C/C++ references,
  25 assembly-only,10 missing listings). Original2EA3C.s:1078 patches a
  signed16 comparison after material depth bias, not camera scale or LOD.
  Both ordinary/subdivided emitters apply the exclusive cutoff and retain
  the independent physical OT bound; default2047 is unchanged. Red ABI
  rejects the old int() stub.114 added packet-count checks cover normal/
  subdivided triangles/quads, below/equal/above thresholds, signed biases,
  halfword wrapping, buffer limits and canaries; room6/168 controls pass
  both builds. Full Debug/Release800/3,532,354 each,96 copier controls,
  default0-new and focused8 existing/0-new findings pass. Two generations
  preserve306 paths and repeat identically, changing only RoomGeometry.c/.h.
  Near/LOD/camera/screen policy and F00756 are unchanged. Private
  `farclip42-*`; no whole renderer promotion, visual parity or retirement
  claim. Publication/exact CI are recorded on#42.
- #42's F01829 pointer-key sort now has one Core-owned native kernel shared
  with the already-working typed AI ranking/promotion path, reaching128/186
  original entries and58 gaps (23 C/C++ references,25 assembly-only,
  10 missing listings). Original7E05C.s:114 and the C counterpart establish
  strict signed midpoint partitioning and tie identities; no std::sort or
  stable-sort behavior change is introduced. Typed AI pointers stage through
  a bounded16-entry void-pointer array without alias-punning or Game types
  in Engine. The generic kernel supports original inclusive byte ranges,
  full native strides and signed32 key reads without record mutation.
  Red ABI rejects the old int() stub. Core4/1195 and combined ranking
  21/4679 controls pass both builds; full Debug/Release800/3,532,240 each,
  95 port controls and0-new focused/default quality pass. Two generations
  preserve306 paths and repeat identically, removing only the obsolete
  generated Containers.c/.h and switching the original module symbol's
  provider. Selective Engine/Tests rendering preserves the owner solution.
  M2C output retains guest32 pointer/recursive-register artifacts and is
  evidence, not copied storage. Private `sort42-*`; no whole AI-state,
  planning gameplay or compatibility retirement claim.
- #151/#149 now have one complete authored scene30->26 chain observation,
  not just a short-budget block. Release1792989783417541-0 on runtime
  961cc1e8/docs af1435ce uses z048-r006-d000,36000 ticks/speed8 and280
  original waiting-dialogue confirmations. Scene30/32/17/PC233 is present
  at1050; authored scene26 is active at40/21 by9000. Field control13320
  persists through13440 (all121 frames ready40/21/control1/transition0/
  event phase0), then teardown13452,0 errors/forced skips. The unchanged
  120-tick gate passes; no story/HP/PC changes or Start skip occur.
  This does not rewrite the older3600/9000 rows or prove original #167
  matching-image history/producer, visual parity or all cutscenes.
  Other event-budget labels are not proof of frozen VMs: their sampled
  event phase is0 with changing control1/2, and one source entry already
  hands off to its destination. Private `doors151-event-budget-observations`
  and `doors151-scene30-long-verified.json` retain the exact states.
  Debug1792992005781732-0 on6ab2aef8 (exe557691c7/PDBdb76150f) independently
  matches control13320/quiet13440/teardown13452 and280 confirmations with
  0 errors/forced skips. `scene30-debug-long-verified.json` checks the same
  continuous121-frame interval; this completes the current standalone chain
  in both builds, not the old ACFD matching-image exception/producer gates.
- #151's final original410-record Release slice is now observed separately:
  frozen e9f0dbab exe a0294afe/PDB10b89faa,
  run1792988508566012-0, exact inventory indices591..1000 starting
  z048-r006-d000. It completes305 passed/105 blocked,0 engine errors,
  410 teardowns and no failed/unrun selection. Blocked reasons are93 fixture
  defeats,4 destination-event budgets,1 source-event budget,5 unavailable
  destination zones and2 unavailable destination rooms. Those7 unavailable
  records are not initialized; do not claim410 initialized. Full default
  trace/frames,3600/speed64/scripted,7200 outer; an initial unsupported
  --log-level option was rejected before gameplay, then removed for the run.
  This is not original #167 full-history reproduction, visual parity or a
  mixed-source all1001 aggregation. Event/combat/availability acceptance
  remains open. Strict private `doors151-remaining-release-verified.json`;
  no owned replay remains active.
- #42's F01820/F01831/F01835 AI lifecycle leaves are owned in ActorCleanup,
  reaching127/186 original entries and59 gaps (24 C/C++ references,
  25 assembly-only,10 missing listings). The existing RuntimeSnapshot
  persistence projection is now shared with room-cache writes rather than
  duplicated beside a stub. Missing state remains-1; native save schema and
  staging ownership are unchanged. Original7DE94.s:9-40 /7E05C.s:241-252,
  504-512 also verify the single hold-byte clear and truly empty startup hook.
  Red4/293 includes278 failures; final5/98,611 controls cover all8192 bit
  patterns, ignored bits, full delay byte, slot boundaries, source immutability,
  absent-state reset and repeated no-op calls with no false unported report.
  Full Debug/Release796/3,531,045 each,94 copier controls and0-new focused/
  default quality pass; both hosts link. Two generations preserve303 paths
  and repeat identically, changing only ActorState.c/.h and the three
  consumer include surfaces. No full room-cache gameplay, new save format,
  whole AI-state C++ promotion or compatibility retirement is claimed.
  Private `lifecycle42-*`; original186 order/prior124 checks retained;
  publication/exact CI are on#42.
- #42's resident SDK rcos/rsin F03400/F03401 now use the same canonical native
  scalar bodies, reaching124/186 original entries and62 gaps (27 C/C++
  references,25 assembly-only,10 missing listings). Their31724.s:9/12
  instruction bodies equal the BATTLE pair after label normalization; no
  new algorithm is introduced. Red SDK int() ABI checks become green6
  cases/41,062 assertions covering both exact original output fingerprints,
  all4096 cross-family results, extreme wrapping and all four original
  module-address resolution/invocation/interior-rejection paths. Full
  Debug/Release791/3,432,434 each,93 copier controls and0-new focused quality
  pass; both hosts link. Two generations preserve302 paths and repeat
  identically, changing only FixedPoint.c/.h; the #186 loader fix keeps the
  existing original-address rows unchanged. All11 configured native
  redirects retain bindings. Private `resident-trig42-*`; prior122 checks
  and original186 order are preserved. No whole GTE/matrix/ending promotion
  or compatibility retirement is claimed.
- #186 fixes the loader surface of native aliases: redirect macros were not
  counted as emitted functions, so the copier omitted their original-address
  module symbols. Actual production-resolver red controls return null for
  BATTLE80098AB4/80098AB8 despite working source aliases. The function-symbol
  set now includes configured redirect names, restoring all9 canonical
  aliases without fake bodies or changing the resolver's exact-function/
  interior-data rules. Native controls resolve/invoke both trig targets and
  reject+1 interior function addresses; a copier control checks every
  configured redirect. Full Debug/Release791/3,424,199 each,93 copier controls
  and0-new focused/default quality pass. Two generations preserve302
  protected paths and repeat identically, changing only ModulePointers.c.
  This completes the wiring needed before #42's two new entries advance;
  no particular gameplay symptom or compatibility retirement is inferred.
  Private `redirect186-*`; publication and exact CI are recorded on#186.
- #42's F00764/F00765 fixed cosine/sine now use the existing canonical native
  aliases, reaching122/186 original entries complete and64 remaining
  (27 C/C++ references,27 assembly-only,10 missing listings). Original
  2EA3C.s:1731/1735-1766 establishes wrapping, cosine phase and signed32
  negation. All4096 original signed sine/cosine pairs match the actual native
  LE32 output fingerprint8E620CA359EB4481; no table payload is committed.
  Red ABI checks reject both old int() stubs. Final5 cases/32,819 assertions
  cover alias identity, fingerprint, octants, every angle's range/phase/sign/
  period and extreme int wrapping. Full Debug/Release790/3,424,191 each,
  92 copier controls and0-new focused/default quality pass; both hosts link.
  Two generations preserve302 protected paths and repeat identically,
  changing only Trig.c/.h and ModulePointers.c. M2C private inputs normalize
  .nop spelling and preserve cosine fallthrough; its erroneous unsigned16
  negative-result inference is rejected against MIPS and the existing ending
  helper. No new trig algorithm, ending change, whole GTE C++ promotion,
  computed-register matrix certification or compatibility retirement is
  claimed. Private `trig42-*`; original186 IDs/order and prior120 checks are
  retained. Publication and exact CI are recorded on#42.
- #42's F01801/F01802/F01803 navigation leaves are now owned in the existing
  NavigationPrimitives API. Original7C7E0.s:122-193 and read-only rood-boi
  e59320ca actor_update.c:57-90 establish explicit actor IDs, missing2/
  reached0/moving1 and the FF00FF horizontal mask. The prior catalog0/1 note
  was inverted. The scripted-path consumer now forwards its actor ID through
  the copier instead of depending on a preserved PS1 a0 register. Typed full
  native state assignment preserves borrowed pointers and seven original
  resets without a fixed0x484-byte copy. Red ABI checks reject all three old
  int() exports. Final5 cases/183 assertions cover all16 native slots, masked
  tile axes, missing grids/states/routes, self-copy, canaries, repeated resets
  and the real reached-only collision-bypass consumer. Full Debug/Release785
  cases/3,391,372 assertions each and90 copier contracts pass; scoped tidy
  and default quality are0 new. Two generations preserve302 protected paths
  and repeat identically, changing only Navigation.c/.h and Movement.c.
  This reaches120/186 original entries,66 remaining (27 C/C++ references,
  29 assembly-only,10 missing listings). Override request/frame-update stubs
  remain; no complete scripted-path gameplay, whole-state promotion or
  compatibility retirement is claimed. Private `navigation42-*`.
- #151's combat-interrupted9/7->9/6 route now has separate actual Release
  controller acceptance on5671e3b5: normal production-room Lab1792978233864775-0
  opens the authored door with Cross, arrives9/6 at230, then ordinary weapon/
  sphere controls defeat original actors2/3 at1272/1652. Queued46/42 damage
  yields HP73->27->0, and54 yields42->0; no HP/story/actor edits or overrides.
  Exact unchanged production gate (ready9/6, control1, transition0, available
  scene/event phase0) holds1840..10000, crossing120 ticks at1960 with0 errors.
  Inactive thread PC337 remains populated; requiring null PCs was an overstrict
  private shutdown proxy, not the production predicate. The deterministic
  device-free door runner remains honestly combat-blocked; its row is not
  rewritten as passed. This is one Release interactive route, not all1001 doors
  or a new Debug/global coverage aggregation. `door151-room6-production-*`.
- #42's relocation constructors F04184/F04185/F04186 are now owned in the
  existing MotionRequests API, reaching117/186 original entries complete and
  69 remaining (28 C/C++ references,31 assembly-only,10 not located).
  Original4A0A8.c:490-549 verifies four/eight route bytes, narrowed facing,
  teleport idle admission/full reaction-byte reset, companion slot>=2 and
  travel pose1/4 preserving reaction/drawn bits. Existing consuming FSMs and
  command21/22/23 callers are retained; no raw record offsets are introduced.
  Red controls fail17/69 assertions against the stubs. Final controls add
  special/self/cyclic companion boundaries, busy-parent isolation and aliased
  inputs; full Debug/Release777 cases/3,390,932 assertions pass with88 port
  contracts and0 new focused compile findings. Two generations preserve300
  protected paths and repeat identically, changing only Movement.c/.h.
  This verifies request construction and native caller contracts, not full
  teleport/travel gameplay, whole motion-state promotion or retirement.
  Publication/CI remain recorded on#42; private `relocation42-*` evidence.
- #184's owning writer is proven in unchanged4f30cd49/PDB331f0f8a history:
  two replays repeat the original144918/144920 errors. Read-only coherent
  snapshots resolve both raw overlay heads to the vignette's final ring packet
  (E1000220/3AA0A0A0). F02461/F02462 write eight-byte pointers at OT+8, clobbering
  the adjacent four-byte entry with0x7FF6; the39287-handle namespace resolves
  that value to unrelated old packets, closing410-node cycles.
  Copier corrections now use a `u_long*` ordering entry, retain the encoded
  previous head directly and publish one `LegacyGpu_Handle(packet)` word.
  No cycle guard, buffer size, packet geometry, draw admission or F02460 ellipse
  arithmetic changes. Red controls fail two of three cases; fixed controls
  pass257 assertions, preserving all16 triangles/64 ring packets,752 words,
  canaries, prior tail and an82-node GPU walk. Full Debug/Release780 cases/
  3,391,189 assertions each,89 copier contracts, focused tidy and default
  quality pass with0 new findings. Two generations preserve301 protected
  paths and repeat identically; only ScreenDistortion.c/.h change.
  Fixed Debug1792982973682278-0 (exe3ca80430/PDB8ecd4651) completes the original
  114-case prefix with82 passed/32 budget-blocked,0 errors/all114 teardowns;
  the formerly failing case now renders through3612. Release1792982973680555-0
  (exea772e506/PDB2403d431) completes all132 with97 passed/35 blocked,0 errors.
  Both match scene30/event0/source1/phase4/PC233/models0/170/169/10 with field
  rendering enabled at original144918/144920, rather than merely exiting.
  Normal-speed Release1792983547181492-0 also renders600 ticks without errors.
  Root fix260f121b has exact green40261350623; #184's bounded producer/entry
  acceptance is complete. No full scene completion, pixel parity or causal
  #167 fix is claimed. Private `vignette184-*` and `packet184-*`.
- #185 is now a proven transition-call ABI defect, not a history requirement:
  frozen3ca80430/PDB8ecd4651 standalone1792985556391334-0 repeats the same
  fault as historical script115. External Windows debugger module/context/
  stack evidence resolves RVA214BA6 to TranslateActorToTileAtRate and its real
  caller to UpdateActorTeleportTransition. Original3A1A0.c:31 declares the
  actual void(int,TileTarget*,int) implementation as u_int(u_char,void*,int);
  three copied transition/cancel/travel casts therefore write only CL while
  the callee consumes a full int. Captured RCXFFFFFFFFFFBE1002 explains the
  exact bad actor-array address. Copier fixes now call the existing typed API
  directly with explicit packed-target views; no invalid-ID clamp/skip.
  Red source-contract control and actual unchanged-image AV become green91
  copier tests and actual fixed Debug1792986145027095-0 / Release
  1792986145026276-0: both arrive48/26 at233, restore control797, satisfy the
  unchanged120-tick gate917 and tear down929 with0 errors. Both retain real
  scene19/models0/89/89 through the original873 boundary and beyond.
  Full Debug/Release785/3,391,372 and0-new default quality pass; two generations
  preserve302 protected paths and repeat identically, changing only Movement.c.
  The old full Debug history remains82 passed/32 blocked/one interrupted/
  17 unrun; the repaired standalone route does not rewrite that cohort or
  close #167. No owned replay remains active. Private `actor185-*` and
  `packet185-*`; dump/observer remain private, not issue attachments.
- #167's original script132 truncation/termination is not reproduced by either
  current full132-case history, but Debug's separate #184 failure prevents a
  clean-cohort claim. Release1792970616137564-0 has97 passed/35 blocked,0
  errors and complete teardown. Both reach original scene30/VM PC233,
  wait70 at script_tick1050 with models0/170/169/10, and advance beyond it.
  Entry-event completion remains blocked at3600. A distinct normal-speed
  9000-tick single-entry observation advances scene30 toPC2865, returns to40/21
  and remains in an active event without errors; this is not proof of a hang,
  a successful door or a causal packet fix. Keep #167/#149/#151 open with those
  limits. The isolated original-game setup has no save/state near Gravity;
  owner unavailable, so #160 remains blocked on original table1 runtime content
  rather than a guessed texture/selector change.
- The preceding #42 sound milestone reached114/186 entries with72 remaining.
  F00995 and
  F01007 are two catalog occurrences of one function, func_800AEEC4.
  Its owned C++ actor-state leaf restores the original void(actor*) ABI and
  centralizes the already-working actor/companion inline interpolation.
  Original44F14.c:886 and MIPS800AEEC4-800AEF90 verify unsigned pan/volume
  bytes, signed truncation, min(tickSpeed/2,remaining), guards and timer
  consumption. Existing frozen-render conditions and producers are unchanged.
  The new implementation stays with Game-owned state in Actors/SoundOverrides;
  the Audio prefix preserves source traceability.
  Red ABI compilation exposes the old int() stub. Full-range, idle, odd/zero,
  clamped, completion/retarget/reset controls cover the typed leaf; no new
  audible gameplay fix, whole audio-state promotion or retirement is claimed.
  Two copier runs preserve300 protected files and repeat identically, changing
  only ActorSound.c/.h, ModelRendering.c and ModulePointers.c. Only the affected
  Game/Tests projects are rendered through the existing scaffold API, preserving
  the dirty owner solution. Build and analyzer share the C-interop predicate.
  Exact test/quality/publication evidence is recorded on#42; local
  `actor-sound-transition-*` reports retain the commands and limits.
  Full Debug/Release771 cases/3,390,847 assertions each and87 port/29 quality
  tool tests pass; focused implementation/test TU tidy and default quality are0/0.
- #80's general multi-body-part hypothesis now has a current player-side
  conformance check: Debug1792968266440918-0 and Release1792968266549016-0
  use normal controls at speed1 to cast ExplosionIV109 on all six original
  Minotaur Zombie limbs. Six results include four original blocks/misses and
  two damage values4/3; HP681->674 matches7 and MP999->939 is charged once.
  Both return field control2431 with0 errors. A compiled-PDB-guarded,
  read-only Debug request observation1792968898071584-0 proves intentional
  six-limb-to-one-actor visual deduplication; Release cancellation retains
  MP999 and applies no result. No target algorithm or runtime was changed.
  Added24 boundary/filter/wrapped-queue combinations pass; full Debug/Release
  suites are768 cases/3,390,555 assertions each, scoped tidy0 new.
  See [the exact fixture, commands and limits](COMBAT_LAB.md#multi-body-part-verification-80-2026-10-25).
  Publication/CI and disposition are recorded on#80. Local `multilimb80-*`.
- #160 remains reproducible on current b2a29e6b: single original23/4->24/15
  entry Debug1792966198610830-0 faults1508 during Gravity137 from model92.
  E084 definition16 requests table1/program1 with only table0 bound. Original
  disc EFFPURGE dispatch, timeline masks and PLG084 constructor all agree with
  native; changing the renderer or decrementing selectors is not justified.
  Next discriminating observation is original runtime ownership/history of
  textureInfoTable[1] at that cast. No default texture or skipped draw added.
- #141's quiet plain Skeleton has a source-backed native cause: missing
  F01792/func_800E4C8C, not a broken universal FOV predicate. Original
  7BDD4.s:585-614 and rood-boi e59320ca agree on first-ranked target, low24
  origin bits, full target-model tile and900-frame memory. The owned planning
  primitive now implements that contract using typed native aliases and the
  existing placement query, retaining no-candidate/nonzero-settings guards.
  Red controls fail9/15 assertions; fixed planning/layout25 cases/3502 assertions
  and full767 cases/3,390,315 assertions pass in each configuration. Two
  generations preserve306 owned/owner files and repeat identically; generated
  runtime changes are only ActionSelection.c/.h and AiState.h. All621 checked
  runtime-view fields match. Targeted implementation/test TU tidy is0/0; an
  overbroad header-triggered all-owned sweep was stopped, not claimed passed.
  Before: current Release1792961811405244-0 selects no attack in36000 ticks.
  After: same Release fixture1792964832686539-0 starts Normal1 at873, damages
  Ashley999->955 at935 and begins23 ordinary attacks, endingHP738. Debug
  1792965026988467-0 repeats fresh entries in one window: attacks873/4484,
  actual damage935/4540,0 errors and complete teardown. No runtime writes to
  HP, readiness, FOV, ranges, delays or actors; original stock data and
  perception predicates are unchanged. Existing memory expiration/forgetting
  and fresh-session reset remain active.
  Function/field notes and generated assembly inventories are reconciled:
  F01792's milestone reached112/186 entries; the sound leaf above advances
  current completion first to114 and now117/186 with69 remaining. No whole
  AI-state C++ promotion or compatibility retirement is claimed. Exact fixing
  publication/CI and closure are tracked in#141/#42. Evidence
  `ai-target-memory-*` in session files.
- #25's tracked maintained fast-quality debt is now fully repaired:0 actionable
  findings/0 new, all first-party Ruff checks pass, and the baseline is
  byte-identical. Imports, unused/ambiguous bindings and equivalent statements
  are cleaned up; all35 CSV readers now close their streams explicitly.
  Shared ledger loading removes duplication while preserving CSV order,
  quoted commas and the prior newline behavior, including closure on errors.
  The lone C17 `.string` member hint is retained as a `language_advisories`
  record, not hidden or "fixed" with a C++ header. Classification requires the
  exact cpplint rule/message and direct C member assignment; actual C++ missing
  headers and safety diagnostics still fail positive controls.
  Verification:29 quality/tool controls,32 naming contracts and85 port tests
  pass. Two real copier runs retain all751 runtime outputs and306 protected
  files; all14 CSV outputs from five real naming batches are unchanged and
  repeat-identical in isolated sinks. Fifteen project/solution renders match
  before/after and a second render without touching the dirty owner solution.
  Native binaries and the last764/3,390,300 Debug/Release test evidence are not
  rebuilt or relabeled for these output-identical tooling changes. Scoped
  historical test-tidy/retirement style categories are not claimed eliminated.
  Published79d054d0 has exact green40236141416 and#25 is closed; remaining non-MA runtime
  and assembly-gap work follows #4. Local evidence `quality25-completion-*`.
- Owner resumed review/repair of all open non-MA issues on2026-10-25. The
  initial inventory was13 issues, including umbrella#4; titles combining several MA
  numbers are excluded too. #76 is closed at the owner's explicit request:
  shared-origin repair e939db22 and its published evidence stand, and any new
  observed particle defect gets a separate concrete issue.
  #18/#42/#148 retain distinct retirement/import,69-native-gap and semantic-naming
  work. Current #80 conformance and #141 acquisition evidence are above.
  #149/#151 have delivered logging/playback/inventory tooling but are not
  all-scene/all-door certification. #160/#167 are not established as repaired
  by today's origin/audio/credit fixes.
  Fresh #156 persistent3-edge controls Debug1792959985239224-0 and
  Release1792960053430781-0 execute the repaired no-top path twice with0 engine
  errors and complete teardown; two neighboring edges pass in each window.
  #156 is closed as that verified crash repair and actual source re-entry.
  Target9/7->9/6 remains explicitly blocked under#151 at4800: its event is idle,
  but live enemy combat interrupts field control (longest ordinary interval110,
  below120). It is not another null-top crash and is not relabeled passed.
  The separate interactive Release acceptance above handles that combat
  normally; the original deterministic cohort result remains blocked rather
  than receiving a larger budget or weaker predicate.
  Maintained quality#25 stays first in the work order. A coherent test-only
  batch replaces40 ambiguous integer-width findings with exact-width types,
  retaining native callback ABI via the existing auto/0L pattern. Five touched
  locals also use the required naming style. Full Debug/Release764 cases/
  3,390,300 assertions each pass; scoped tidy228 existing/0 new. No assertion,
  fixture byte, generated runtime, vendor/reference file or quality baseline
  changed. Published c99a6f92 has exact green40231907155; default debt falls
  from182 to142. The next coherent #25 review makes23 one-to-one zip contracts
  explicit (24 guarded calls including one existing strict site), binds current
  declaration/module/native-type values in callbacks and names one unused
  original-field binding. Thirty naming and72 source-adaptation tests pass.
  Two real copier runs preserve all751 generated files byte-for-byte and305
  protected files. Audio/Engine_01/Game_Battle_02/Game_Menu_01 comparisons route
  all writes to isolated sinks and preserve all11 CSV outputs; live ledgers
  remain unchanged. Game_Battle_01 initially fails even BEFORE those changes:
  its adopted upstream padding incorrectly falls back to pinned declarations.
  The bounded #148 follow-up explicitly cites T00271.008 and
  T00273.004/.012 from unique live upstream declarations in6E644.c (377,392,400
  at this reference revision). It retains reserved names, types and low
  confidence; no guessed meaning or hardcoded generated line number.
  The real batch now writes all3 isolated CSVs, verifies those exact fields
  and citations, preserves live ledgers, and is identical on its second run.
  Thirty-one naming tests pass; the remaining repository-wide #148 sweep is
  not completed by this three-field provenance repair.
  Full fast now reports106 existing/0 new (105 Python plus the C17 textbox
  member `string` IWYU suggestion). No baseline update, C++ string header in C,
  or generic suppression was used. Current Skeleton follow-up
  Release1792961811405244-0 retains35960 ready frames/36000 ticks with no enemy
  offensive selection,0 errors and complete teardown; #141 is still a quiet
  fixture observation, not a proven original-game mismatch or a repaired bug.
  Evidence is `non-ma-audit-*` and `quality25-*` in session files.
- #183 owner-requested lethal chain credit is implemented through copier F00099.
  The copied original146C.c:3420 also coupled offensive credit to target
  survival; this is an explicit owner behavior change, not an invented x64
  diagnosis. Successful timing now earns the existing point even when the
  target died, while only a surviving target receives another queued attack
  or increases the executed-chain count. Both offensive and defensive credit
  now bank beyond a pending threshold, saturating the persistent ushort at65535
  instead of discarding further successes before the next death. Story
  flag0x82>=10, cumulative unlock thresholds and the22-unlock cap remain intact.
  Points are global persistent progression, not attached to a
  victim; the existing recoveryMode2 enemy-death path gates learning, so
  threshold credit can wait for a later different target's death.
  Focused regression first fails22 lethal threshold assertions; repaired
  Debug/Release ability/acquisition/Break Art selections each pass48 cases/
  21,892 assertions, including656 new lethal-credit/banking/non-kill checks.
  Source adaptation contracts72 pass; two generations change only Abilities.c
  plus manifest, preserve302 non-generated Native files and are byte-stable.
  Scoped tidy has1 existing/0 new. A single complete Debug controller acceptance
  uses an untouched copy of the owner's real post-Minotaur save, not fabricated
  Lab story flags: run1792958740259860-0 earns25 points on the surviving9/17
  dummy at5289 without learning, leaves through the original Cross-operated
  door, kills a different9/12 enemy at8360 and learns action24 at8836.
  Accepted lethal timing adds point46 at8390 while the victim is at0HP and the
  sequence stops in phase9. Successful defenses add points26,47 and48.
  All0..48 credit increments are retained; 120 normal-control frames follow,
  exit0 at9313, no error/crash, and owner save hashes remain unchanged.
  PDB observation only reads memory; all movement, attacks, timing, loot/menu
  acknowledgment and learning use production input. The first extended probe
  banked25 correctly but did not open the door; it is not kill/learning proof.
  No extra full campaign or duplicate Release replay is claimed. Published
  7b471a0e/green40227408982 is the initial lethal-credit milestone; the banked
  follow-up's fixing revision and exact CI are tracked in #183. Evidence
  `chain-credit-*` is local; no C++ promotion or bridge retirement.
- #182 bedroom Start-skip exception is reproduced and repaired through copier
  F03246/func_8001852C (original SLUS_010.40/25AC.c:3582). The camera/thread
  sequence was only the last logged work: the actual crash is
  Audio_Seq_TickPlayback at Sequencer.c:371. The original int-alias cancellation
  cleared only the low half of the widened pending-music pointer; assigning
  its full uintptr_t field restores the original cancellation semantics.
  Unfixed Release1792955307797622-0 enters zone18 at6157, receives actual Start
  at6289 and crashes at6294 reading0x7FF600000004. The first attempt did not
  reach the target skip and is excluded. The native regression first fails
  4/20 assertions, then passes all20 for queued/empty, saved/no-saved and
  repeated cancellation while preserving neighboring parameters/status flags.
  Fixed Debug1792955857390338-0 and Release1792956026885654-0 replay the same
  actual1x controls: zone9 at6689, event idle at6863, exit0 at10000. The
  documented alternate skip/menu route also passes in Release1792956283015448-0:
  zone9 at4058, 2768 idle field frames before Triangle, MAINMENU.PRG at7004.
  No HP/story/PC writes or camera fallback. All three hosts build in both
  configurations; full760 cases/3,389,644 assertions each pass. Copier contracts
  70 pass; both generation runs change only MixControl.c plus its manifest,
  preserve305 non-generated/owner files and produce identical second output.
  Published42cafc2aa27a30056f62156588dd7e092568119a has exact green Source
  Quality40226404485; #182 is closed. Evidence is `bedroom-skip-*` in session
  files. This is not C++ promotion or compatibility retirement.
- #76/#168 native child-emission origin fix is implemented through copier
  adaptation F01535, not a hand edit of generated output. The original PS1
  particle prefix and fixed-layout plugin-parent context overlap on 32-bit;
  native particle links/texture pointers widen and move its actual fields.
  Tick, lifetime-end and texture-end events now project position, size/mode
  inheritance into a typed parent value instead of reading pointer bytes.
  Valid synthetic regressions first failed coordinates/appearance, then pass
  all 33 assertions across the three lifecycle paths. Generated runtime changes
  are limited to Effects/Particles.c; protected owned/owner files were preserved
  and a second generation is byte-identical. This is not C++ promotion or
  compatibility retirement of the existing parent-context type.
  The owner's fresh run 1792944731133000-0 confirms grimoire 425 consumption and
  Heal/action 96 learning at 38186. The reported celebration is common secondary
  effect 20, not the preceding E043 item/spell effect. Actual all-items-a Lab
  controls consume 425 from 10 to 9 and learn 96 at 1392 in both configurations,
  without live HP/learned-bit edits or direct effect injection. Matched Release
  before/after runs retain all 4205 mastery-particle records and identical action
  timing. Parent-relative children move from erroneous world bounds
  x[-59,72],y[-60,24568],z[193,450] to x[200,452],y[-246,55],z[437,702]
  around Ashley at 320,0,576; active captures show the restored gold burst.
  Standard Debug/Release builds of all three hosts and full 759 cases/
  3,389,624 assertions each pass; affected maintained sources have 0 new tidy/
  style findings and full fast remains 182 existing/0 new, no baseline changes.
  Published fix is e939db22; exact Source Quality40217651083 is green;
  owner ignore, solution and updated reference-pointer edits are preserved.
  Isolated-output full-suite attempts failed only because their relative audio
  fixture paths were wrong; the passing full results use standard build paths.
  Missing ignored decls/hazards caches were restored using existing indexers;
  the existing MPD/SHP offset fixups were not weakened. Read-only reference
  review includes owner-updated rood-boi e59320ca and rood-reverse 4edbdbf5.
  The solo external-event 7 Lab component is invalid: camera code requires the
  Dtok actor context and fails at 165 before E211. It is not gameplay evidence.
  Full1x Debug1792952304769540-0 and Release1792949707855559-0 defeat Dtok at38142,
  retain all4501 ticks40500..45000 and return to120 consecutive normal-control
  frames, exit0 at68612. Every decoded video RGB frame matches its source and
  all4501 Debug/Release images are identical. Actual E211 phase4 is42132..43243;
  cleanup ends43248. DPI-correct original normal11 retains6509 full physical
  client frames,849x655,111.024s; max host capture gap49.7464ms. At aligned
  camera/hero position, max rotation delta7/4096 and translation delta0.
  Source/ghost placement agrees; no whole-body/pixel/every-guest-VSync claim.
  Portable settings restored exactly; owned emulators/recorders ended.
  Owner acceptance and closure of #168 are recorded2026-10-25. #76 is also
  closed at the owner's explicit request; newly observed particle problems
  get separate issues. #149 cutscene coverage remains separate.
  The following diagnostic milestones retain their original
  failed-attempt and observation limits, not current open/closed status.
- #168 historical bounded path instrumentation for both native hosts:
  `--trace-effect-paths first:last`, off by default, positive inclusive host
  ticks and normal speed 1 required. Generic particles log local/world/projection
  context and command links; translucent polygons/sprites/lines log actual
  vertices, material and viewport state, including paths outside that pool.
  Final `screen` coordinates exclude framebuffer draw offset; `raster`
  includes it so double-buffer flips cannot masquerade as motion. IDs are
  pool/slot/instance/age/context, not globally unique lifetime identities.
  Initial real speed 1 run Release1792935010474425-0 defeats Dtok at 38142 and
  traverses zone 7's window-opening/city-view sequence at 39011..40673 into
  zone 8. All 4,201 selected
  path-frame markers 37800..42000 exist exactly once; 43,428 particle and 57,893
  primitive records parse. All 3,501 consecutive captures 38500..42000 were
  recorded; a lossless 60fps clip retains all 3,501 frames / 58.350s, no frame
  dropping.
  This initial capture precedes the final screen/raster naming clarification;
  its raw records retain draw offsets for unambiguous normalization.
  The app exits 0 at 44557, but the helper correctly fails its stricter full
  prologue/120-normal-frame criterion: do not claim that criterion for this run.
  It records that entire zone 7 interval plus early zone 8; identifying the
  owner's actual optical subshot/producer and its complete lifetime remains
  separate. This is not a native flare-position fix.
  Local evidence is `flare168-normal-*` plus `effect-path-*` in session files.
  Final source builds all three hosts in Debug/Release; full 758 cases/
  3,389,591 assertions each pass. Affected-TU tidy 0 new and full fast 182
  existing/0 new, no baseline changes. Final actual host smoke runs retain
  Desktop1792936671129624-0 markers 1..2 and Lab1792936674177743-0 markers
  100..120 plus 602 final-schema primitive records; screen+draw-offset equals
  raster for every vertex. All 3,501 decoded clip RGB hashes/order match the
  source frames exactly (unused BGR0 padding is not image data).
  Redundant BMPs were removed only after that pixel/order verification; the
  complete lossless clip, hashes and raw trajectory logs remain. Logging is
  published b5e0ae5e, exact Source Quality 40200550100 green; at that milestone
  #168 remained open for the actual placement/producer diagnosis.
- #168 historical follow-up observation limits: the owner-supplied interior closed-window
  pose is not yet tied to a specific logged event or assigned a backend role.
  Restricting analysis to zone 7 was an assumption, not identification of the
  reported flare. That exact interval has 1,663 frame markers, 465 translucent
  primitive records and no generic-particle records. Later zone 8 loads
  E214.P; its decoded sprite is a bird, not a flare. Do not "correct" its
  unusual world coordinates or call reused context IDs package filenames.
  Original normal-speed replay `rays168-reference-normal9` does defeat Dtok
  through controls, enter zone 7 and reach zone 8. Its full-content PrintWindow
  video decodes to 1,353 frames / 23.083s, with a measured maximum capture gap
  of 33.853ms; timestamps preserve elapsed time, not accelerated playback.
  However, recording starts about 7.5s after zone 7 entry and stops at zone 8
  entry. It therefore does not verify every candidate optical interval.
  GDI capture probes showed a stale Qt game-list surface and are excluded;
  PrintWindow flags 3 was checked against actual game pixels. The wrapper
  exits 1 despite its actual capture/zone-8 success because a private status
  assignment incorrectly set a timeout failure during every live iteration.
  Original logs are unchanged; a focused AST red/green contract verifies the
  helper correction, not a retrospectively passing wrapper. The prior
  `normal8` attempt genuinely exhausted its 900s bound in Dtok's intro;
  neither attempt is a passed whole-prologue or placement comparison.
  An additional native speed-1 candidate observation, Release1792940899292622-0
  on fa94d19c plus owner ignore/solution edits, records all 2,801 ticks/
  captures 6400..9200: 16,136 primitive and 2,095 particle records, exit 0.
  The lossless early clip preserves every RGB frame/order; this is candidate
  localization evidence, not matched screenshot or flare parity. All reference
  processes/encoders ended and portable settings restore to 558A5842 exactly.
  Next observation must identify the actual reported subshot and source-backed
  producer, retaining its whole interval rather than stopping at an assumed
  room boundary or matching selected background poses.
- #168 methodology correction from the owner's2026-10-25 screenshots: the
  reported defect is the transient lens-flare source and colored ghost/ring
  placement, not merely the window/sky background. Earlier selected-pose and
  background comparisons below are NOT flare-trajectory/parity evidence and
  must not justify closing the issue. Speedup and pose-filtered sampling were
  insufficient. Use normal-speed continuous observation of the entire effect.
  Before path instrumentation, scene JSON recorded only effect flags/phases/
  track counts and particle output only node/quad totals. Asset-load filenames
  are logged separately. Identify the actual flare producer next using the
  new paths; do not assume it uses the generic particle pool. Producer-specific
  trace points still need source/resource identity,
  lifetime, attachment/local/world coordinates, camera/projection/viewport and
  emitted screen centers/quads. User image snapshots remain read-only/local;
  no new native placement fix or verified flare parity is claimed.
- #168 now has real original New Game/dragon/window evidence rather than attract
  mode. Title22C.c:3189 returns menu0 when neither card has a save; adding Down
  selected the wrong entry. Reference `rays168-ref-live6` uses real controller
  input, entry_mode0, guard damage70 ->20 ->0 /61 ->16 ->0, then Dtok/model58
  damage84 ->58 ->33 ->7 ->0. No guest memory writes, HP/flag/position edits or
  story skip. It captures zone7/event7/phase4 with Ashley[1140,9,2629], view
  translation[-1040,-140,-2321] and local image/matrix provenance.
  Fresh native Release1792929268622554-0 likewise plays the real prologue,
  defeats Dtok at38048, traverses7 ->8 ->18 ->33 ->10 ->9/14 ->9/0 and returns
  to120 consecutive ready/normal/idle-event frames before closing at68432.
  The earlier80000-frame probe1792928609925047-0 returned normally but its
  controller kept toggling the sphere, preventing its own120-frame criterion;
  input stops only after observed ready/control1/action0/event-phase0 now.
  At native40060, actor position matches the original exactly and measured
  view rotation differs by at most13/4096, translation by at most8 units.
  The early window beam visually agrees at that aligned state. This does NOT
  establish whole-window parity or resolve the owner's misplaced-ray report:
  subsequent native camera poses show a much larger window-region polygon.
  The next observation is aligned original imagery across that later camera
  motion, not a guessed beam offset or hiding geometry. Full native dense
  window evidence and original captures stay local in `rays168-*` session files.
  Portable reference processes/listeners end after each attempt and settings
  restore byte-identically to the newly restored copy's558A5842 baseline;
  installed emulator/BIOS/reference worktrees remain untouched. Replay source
  is81c81f4e with only owner ignore/solution edits; no native ray change yet.
- #25 paired-length checkpoint makes six audited B905 contracts explicit:
  four shared Gen member/spec pairs and both menu name/token pairs. Existing
  duplicate-length and exact member-order guards remain; layout and token
  producers already require full alignment, not intentional prefix truncation.
  Eight source-independent contract tests expose4 malformed-pair subtest
  failures before the guards;128 maintained tests pass afterward. The exact
  naming selection passes23 tests without analysis caches and is now included
  in the Windows Source Quality workflow. Four complete actual generators
  (Battle01/02, Menu01/02) retain all12 CSV byte hashes before/after/repeat:
  316 types,2,411 fields,38 globals. Canonical ledger hashes remain unchanged;
  output is private and no native/owned implementation was regenerated.
  Full fast188 ->182 existing/0 new (41 test-harness,141 tooling), no baseline
  or suppression updates. Remaining23 B905 sites are not blanket-certified;
  malformed internal fixtures do not establish a current campaign/cache defect.
  Evidence: private `quality25-pairs-*` reports, red/green/exact-CI selections
  and output probe. Source800a44ec plus the scoped helpers/tests/CI edits;
  publication/CI are recorded in #25. The broad issue remains open.
- #25 first-party unused-binding batch removes six retained F401/F841 findings
  across naming/port tools, plus the related generator import-order finding.
  `cite_declaration_only` still validates every field row identity before
  filtering; dropping its unused map must not silently accept malformed rows.
  No native/generated runtime, canonical ledger, reference, baseline or
  suppression changes. Full fast quality falls195 ->188 existing/0 new
  (41 test-harness,147 tooling); #25 remains open for that remaining review.
  Existing naming/port/audit/quality controls pass120 tests in one runner.
  Actual Game_Menu_02 output63 types/582 fields/38 globals is byte-identical
  before/after and on a second generation into private outputs. Naming checks,
  title reachability and title reference indexing all exit0 with identical
  output/semantics;5,483 occurrences have0 unresolved/mismatches. Valid rows
  stay unchanged and a missing field identity is still explicitly rejected.
  Baseline initially could not run because ignored `build/port` inputs were
  absent. Existing analysis-only generators restored headers/index/inventory
  and the full occurrence index:138 TUs,22 documented original compiler-leniency
  error TUs,100,172 occurrences,0 unresolved/mismatches. Context generation ran
  against a private ledger mirror, preserving IDs/human columns; all6 canonical
  CSV files stayed byte-identical. No analysis error is presented as native
  compilation or gameplay completion. Private `quality25-unused-*` reports and
  `quality25-ledger-preservation.json` retain the bounded evidence. Replay
  source9e44440f plus five scoped tooling edits; owner ignore/solution edits
  remain separate. Publication/CI progress is recorded in #25.
- #181 later save-point rejection is traced to source-consistent menu pause.
  Original146C.c:1893 / Field_PauseFieldForMenu freezes ordinary walking/turning
  without clearing turnFrames, horizontal root motion or momentum distance.
  Actual Left2250, Left+Triangle2253, Command > Data > Save/Yes3203 reproduces
  rejection in Debug1792921380918706-0: turnFrames11, rootMotion[-18,0,0],
  momentum18, with every jump/support/attachment/route/movement-mode field0.
  A settled approach saves, so waiting inside the paused menu is not a fix.
  Capture now accepts only that horizontal intent under control5/6 AND actual
  interaction pause, without mutating the live actor or saving transient intent.
  Normal room spawn resumes idle at the saved static position/facing. Active
  movement, vertical motion, jumps, attachment, platform/support, route,
  transition, placement and camera guards remain; no schema change or broader
  transient-state support is claimed. Red3/778 has2 failures; focused78/6,446
  and full752/3,389,521 each Debug/Release pass. All three hosts build both;
  affected-TU tidy/header quality0 new and full fast195 existing/0 new.
  Fresh focused reruns after log cleanup retain78/6,446 each.
  Actual identical MAP014 input writes generation2 at3203 in
  Debug1792921665506788-0 / Release1792921709645065-0, position[174,-384,1088].
  Separate restarts1792922035343375-0 /1792922047434690-0 preserve that position,
  HP/MP/maxima, primary stats, risk/status timers, weapon identities, inventory
  and learned actions for322 consecutive normal frames1679..2000 each.
  Zone13/0 moving-menu component writes generation1 at1102 in Lab
  Debug1792922006084449-0 / Release1792922095417863-0, position[1618,-128,1600].
  Serialized Desktop restarts1792922115045814-0 /1792922125884581-0 retain the
  same values for323 normal frames1678..2000 each. All eight runs have zero
  engine errors; a wrong-location restart is explicitly rejected by verification.
  These are original save-point component placements, not a new campaign replay
  or exact reproduction of every field from the older owner binary.
  Replay source is8d5c47aa plus the three boundary/header/test edits; unrelated
  owner ignore/solution edits remain separate. Small strict trace report and
  source are retained in session files as `save181-trace-verified.json` and
  `save181-trace-verify.py`; surviving unique per-run roots above are authoritative.
  Earlier `build/quality` save-file/backup copies and captures disappeared before
  requested log trimming; their bytes are not reverified by this trace report.
  Mistakenly concurrent GPU attempts were stopped and excluded. Publication/CI
  and closure evidence are recorded in #181; #18's broad remaining gates stay open.
- #180 post-Minotaur Heal save failure is fixed in8d5c47aa,
  exact green40181602003, and closed.
  Owner Debug1792918396645912-0 saves at23411, consumes original Grimoires425/432
  and learns Heal/action96 plus action54, then cannot save to either an existing
  or empty file: display reference11 points to the consumed slot.
  Original5BF94.c:4454 intentionally leaves display rows stale after consumption;
  2D10.c:1254 removes them on inventory reentry. Native capture wrongly required
  that lazy live cache to already satisfy canonical file validation.
  Capture now projects ordered occupied survivors on its own snapshot copy for
  all seven categories. Missing lazy entries stay missing; live order/inventory
  are not mutated. Out-of-range and duplicate references still fail, including
  duplicates of deleted slots; external file-content validation remains strict.
  Static/support/camera capture limits are unchanged, with all rejecting actor
  fields now included in the diagnostic rather than an undifferentiated message.
  Focused red3/106 has2 failures; green75/5,648 and full serial749/3,388,723 each
  Debug/Release pass. All three hosts build both; affected-TU tidy0 new and
  full fast195 existing/0 new, no baseline changes.
  Actual pre-fix Desktop1792920840234145-0 reproduces the exact failure at5103.
  Identical green Debug1792920927472590-0 / Release1792920905225737-0 learn
  Heal96 at2767 and action54 at3643 through actual controller actions, then
  Command > Data > Save/Yes writes generation4 at5103. The seed is an unchanged
  isolated copy of the owner's legitimate post-boss generation3; this is not a
  newly replayed boss victory. Both consumed items are absent, survivor order
  stays1..9, position[411,0,569]/9,17/scene1 is preserved, and the backup is
  byte-identical to generation3. Separate process restarts1792921065248414-0 /
  1792921077125704-0 restore HP133/MP45 at1645 and both learned actions, followed
  by318 consecutive normal frames1683..2000 with ordinary regeneration.
  Historical strict report was `build/quality/save180-verified.json`; that
  report/copy/capture directory no longer exists. Unique green Desktop run roots
  above and actual owner Debug1792918396645912-0 survive requested log trimming.
  Owner files were byte-identical throughout those isolated tests.
  Later owner failures at50948 (9/5) and75644 (13/0) hit a separate static-actor
  capture boundary, not this inventory error; #181's bounded source-backed
  reproduction/repair is described above. #18's full-story/transient/import/EC
  gates remain open.
- #169's actual first-lever handoff is now verified, not only direct MAP212
  loading. Original MAP020 event4 requires local flag48==1, tile3,13 and normal
  grounded actor state. The supported upper static floor6,12 resolves-512,
  within original marker2 interaction reach; lower5,11 is512 below it. Real
  Cross and the original Pull Lever selector establish the timed state.
  Earlier routes physically reached the tile only after event2 closed it;
  those clean exits/failures are not accepted as progression.
  Real crossbow action1 defeats original bat4 HP41->0, leaving wolf3 at76HP.
  Then normal sheathing, lever interaction/confirmation and movement reach
  embedded event4 before closure. Debug1792914639045805-0 /
  Release1792914784653033-0 load actual MAP212 at2190, enter19/0 at2191,
  naturally load9/12 at3900 and return normal control4666. No HP/flags/position
  edits, direct event request or forced skip; zero engine errors.
  Maintained `tools/test/scenarios/first_lever_handoff.json` repeats the whole
  controller path twice in one window. Debug1792915518582016-0 and
  Release1792915856293931-0 pass both real source/destination entries:
  event4 at1970/7982, MAP212 ready2191/8203, normal9/12 return4666/10678 and
  longest consecutive normal intervals126 each. Both cases complete normal
  teardown without errors. Second MAP212 entry correctly reuses cached data;
  physical-read absence is not missing logical room initialization.
  `tools/test/verify_first_lever.py` verifies actual attack, retained wolf,
  original trigger, catalog-backed MAP212 identity, ordering, unforced scenes
  and120 consecutive living/normal return frames. A clean-exit no-handoff
  control is explicitly rejected. It is registered in maintained Python scope.
  Core fix remains f1bba2b0/exact green40111624104; current native code is
  c470fd26 with only owner ignore/solution edits plus that test package.
  Evidence: `build/quality/lever169-sequence-*-verified.json`, adaptive/static
  runs and `lever169-*`. Acceptance publishedc8b2d13b6b726de02f593f35291ecea509e69b64,
  exact green40176464045; #169 is closed. This is not full campaign completion,
  every-door coverage or visual parity.
- #18 measured cold-preview/load repair retains validated native documents.
  Actual speed1 Desktop timing on the449,036-byte isolated owner-save copy
  identifies duplicate Decode plus two Encode/self-Decode comparison passes,
  not slow disk I/O: baseline first ReadFiles323.821ms and explicit fresh
  confirmation736.303ms; content checks1.4ms and PrepareResume1.6ms.
  Engine validators now support synchronous per-load captures. Game keeps
  only the successfully schema/integrity/content-validated primary or backup
  document, rather than decoding those same bytes again. Missing callback
  results are an explicit invariant error. Exact wire-field comparison reuses
  the codec's semantic encoder without generating/checking two new save files;
  it is not native-byte comparison or checksum-only consent. Fresh inspection,
  content checks, recovered-status matching and default-No remain unchanged.
  Same-input Debug first preview162.772ms (49.7% lower), confirmation203.217ms
  (72.4% lower), semantic comparison42.912ms instead of406.672ms. Release:
  preview8.627ms, confirmation12.058ms, cached100->1 window3us (Debug6us).
  Valid primary loads decode once; explicit malformed-primary recovery tries
  two candidates and retains the backup's own generation1 document. Both
  configurations prepare generation2 primary/generation1 backup at1602 and
  apply1640 with zero errors. Invalid primary text and backup SHA256 remain
  unchanged. This is not elimination of every cold/audio gap or a whole-frame
  timing guarantee; Debug still spends about200ms in explicit validation.
  Three new cases preserve old canonical-wire equality (including ignored
  transient/padding fields), invalid-model rejection and capture/recovery
  lifetime. Focused72/5,521 and full serial746/3,388,596 each Debug/Release pass;
  all three hosts build both. Focused six-TU tidy has zero new actionable
  findings after reusing existing generation-fixture setup;181 fixture literals
  remain separate. Header checks0; full fast195 existing/0 new, no baseline
  changes. A header-selected tidy invocation expanded to every native TU and
  was stopped; the actual six affected TUs were checked explicitly.
  Evidence: `build/quality/save18-retained-*`, `save18-cold-stages-Debug*`;
  strict `save18-retained-verified.json` checks measured reductions, decode
  counts, cached wrap and actual explicit recovery. Replays used995939df plus
  this scoped package and unrelated owner ignore/solution edits. Owner files
  were not the replay save root. Broader #18 import/migration/clear-save,
  transient/story and EC gates remain open.
  Published579a9aa243c98a2e385458d2e924abd963c70126; exact green40170087562.
  Fresh pushed primary timings and original reset/repeated Continue are also
  verified. Debug1792911133357885-0 / Release1792911271664146-0 restore exact
  owner-copy position[462,0,570], HP250/MP50 and9/17 twice: normal-control
  windows1678..1999 (322 frames) and3828..4299 (472). Actual six-button reset
  at2000 returns to title2010; second prepare3752/apply3790, no forced jumps.
  Fresh original MAP014 save-point component writes in both configurations
  create generation1/448,473 bytes and both save counts1. Separate Desktop
  restart1792911625826237-0 /1792911710233527-0 loads those new files, restores
  [192,-384,1088]/HP250/MP50/9/5 and sustains222 normal frames1679..1900.
  Strict reports: `save18-retained-pushed-verified.json`,
  `save18-retained-repeat-verified.json`, `save18-retained-write-restart-verified.json`.
  #18's initial native-only checklist item is verified, not its remaining
  compatibility/import/campaign gates; the parent remains open.
- #42/#178 reference occlusion subpackage: F01767/F01768 no longer return stub0.
  Owned `TerrainHeight` reuses the validated native32x32 decoder and implements
  original7B5DC.s:527/563 (C counterparts510/524): bit0x2000 ground0, signed
  15-bit height, unsigned16 X, endpoint extents, arithmetic delta>>4 and16
  advances-before-probe samples. It preserves endpoint quantization rather than
  replacing it with an intuitive exact-endpoint ray. Missing grids/bounds/null
  endpoints/source arithmetic overflow are diagnosed, not treated as clear.
  Red3 cases/12 assertions has7 failures; green occlusion7/196,651 and terrain
  selection21/660,233 each Debug/Release pass. Full serial743/3,388,537 each;
  all hosts build, copier68 controls, stable regeneration/owned preservation,
  selected tidy0 plus64 fixture literals and fast195/0 new. The old six
  terrain helpers' stale ledger locations/stub flags are reconciled separately
  from the two new implementations; no whole C++/EC promotion is claimed.
  Closed #171 behavior is preserved: Debug1792906593542606-0 and
  Release1792906702259646-0 still descend512+128 toground0 at223 and damage
  Ashley250->181. Existing128 climb controls1792906670059507-0 /
  1792906760909191-0 jump from-320 (arc-485) to-448 and damage250->231
  (regen232), with no runtime coordinates/flags/HP/ranges changed.
  Published e2c8b6ee66006b5271c9cd5f5e3f162a14b18bb8; exact Source
  Quality40166549728 passed. The always-clear stubs were a confirmed missing
  behavior, not the demonstrated cause of the owner's initial height refusal.
- #178 direct-height eligibility is now demonstrated with original occlusion,
  not inferred from a jump followed by same-floor attacks. Canonical Sackheim6
  uses original288-unit sword reach82944 and signed aim offsets-131/-139.
  Higher NPC at-448 versus Ashley-320: Debug1792905791514310-0 and
  Release1792906061641007-0 attack597/599 frames and naturally reduce HP250
  to159 while the128-unit difference remains throughout every grounded attack.
  Higher Ashley-448 versus NPC-320:1792905872776981-0 /
  1792906111806198-0 attack586/587 frames and reduce250->152, still128 apart.
  Distances30784/31265 are within82944. The source out-of-reach control
  1792905981808998-0 /1792906154560166-0 keeps Ashley-640/NPC0, squared
  distance448985, zero attack frames and HP250. All six runs have zero engine
  errors; strict `build/quality/height178-verified.json` checks actual source
  action1 damage, no airborne proxy, signed aim/range and original permissions.
  There is no blanket same-floor ban and no arbitrary reach/LOS weakening.
  This is not every enemy/action/terrain combination; #175 remains separate.
  Local `height178-*`/`occlusion42-*` retain commands, captures, PDB checks and
  records. Run evidence is7b8f82b7 plus this scoped occlusion package and unrelated
  owner ignore/solution edits. Published e2c8b6ee/exact green40166549728 satisfies
  the closure gate; #178 is closed with the bounded source/range evidence.
  #42's refreshed assembly reports now contain75 gaps (34 C-backed,31
  assembly-only,10 instruction listings not located). All13 audit controls pass
  and repeated publication is byte-stable. Exact comparison with the original
  186-entry tracker finds only five newly implemented entries: F01767/F01768
  and the three previously verified jump entries F01024/F04178/F04179
  (f368cbd5/exact green40148851247). All previous106 checkmarks remain valid:
  111 implemented/75 remain, C-backed111/145 and the other41 still open.
  Evidence: `build/quality/issue42-20261025-occlusion`, `occlusion42-audit-*`.
  These are implementation counts, not additional gameplay defects or whole
  C++/EC promotion. Broad #42 remains open.
- #175 now has actual supported-cover acceptance, not a speculative box fix.
  Source71618.c:1093 tries ordinary ranked actions before its cached fallback
  against registered special actors. In original production9/11/MAP020, real
  Right+Square at120 during normal control, then Right123..228 every3 ticks,
  climbs Ashley from[192,0,320] to[318,-384,320] on the original crate stack.
  Earlier Square80 was startup-action locked and did not jump; it is not a
  gameplay defect. No HP, coordinates, flags, ranges or readiness were written.
  Debug1792907616391601-0 / Release1792907657305767-0: wolf3's actual action1
  reduces original box6 HP1->0 at496, single-target phase7. Its cached fallback
  target is6; Ashley's squared signed-aim distance209979 exceeds50176 while
  box6 is19646 within reach/cap and box7 is67262 outside reach. Ashley and box
  share the required tile neighborhood. Generic AI navigation target0 is not
  the executed fallback target.
  Extended Debug1792907737516702-0 / Release1792907776647105-0 reach1200:
  destroyed6 leaves actor/special-target lists, box7 drops-256->-128, Ashley
  settles-384->-256 and wolf3 climbs onto the remaining stack (actual command10
  intermediate heights612..625, not a claimed jump arc). Normal control resumes.
  All six runs have zero engine errors and normal host shutdown with explicit
  interrupted encounters/trace_failed=false, not encounter-victory claims.
  Ground controls1792907849223984-0 /1792907882485500-0 keep Ashley reachable,
  naturally damage him, and leave both boxes at1HP through900. Ordinary actions
  correctly win rather than unconditional box aggression.
  Strict local proof `build/quality/box175-verified.json` covers all six modes;
  `box175-*` and unique runs retain actual inputs, captures and PDB snapshots.
  Source6db8d24d, owner-only ignore/solution dirty state, exact green40166918324.
  No runtime change or newly demonstrated common defect; original occasional
  fallback is verified. Acceptance995939df/exact green40167674588; #175 is
  closed. Broader enemy/terrain coverage remains separate.
- #172 wooden-box targeting repair is verified. Original F00979 accepted the
  PS1-sized T00469 palette view on a real native gimmick: source/output offsets
  300/332 instead of304/336 after its model pointer widens. Partial CPU-array
  alignment could look correct, but the upload started two color entries early
  and owning output colors14/15 were never updated. Nonzero preceding metadata
  also overwrote source tail colors in synthetic controls.
  F00979 is now registered in the existing owned appearance implementation and
  uses `Field_GimmickInstance` fields directly; all three generated actor/
  attached/gimmick callers use the typed API. Original blue pulse, transparency,
  alpha, saturation,2/4-speed boundary ordering and16/160-color actor paths
  remain intact. No texture, targeting or highlight is hidden/disabled.
  Red2 cases/52 assertions has40 failures in the gimmick record/actual VRAM
  CLUT checks; actor controls already pass. Green Debug/Release3 cases/
  100 assertions covers complete records, actual CLUT indices/neighbors,
  first/last slots and three select/cancel/reentry cycles. Appearance selection
  is7/7,978 each; full serial736/3,191,886 each passes. Copier67 controls,
  second-generation stability and owned-source preservation pass. Selected
  tidy0 plus83 fixture literals; fast195 existing/0 new.
  Original kind4 production crates in9/11/MAP020 were actually selected,
  cancelled and reselected, not patched through a debugger. All16 owning pulse
  colors now match the source formula and base palettes stay unchanged in all
  eight Debug/Release modes; pre-fix live output missed colors14/15. Corrected
  selected/reselected/cancelled images were reviewed locally. Debug runs:
  baseline1792903888282972-0, selected1792903722631867-0,
  cancelled1792903755402586-0, reselected1792903814974448-0; Release:
  1792903906707392-0/1792903924231782-0/1792903944196085-0/1792903965393681-0.
  Strict report `build/quality/box172-verified.json`, captures/logs `box172-*`.
  Evidence is2cc1db5c plus this scoped palette package and unrelated owner
  ignore/solution edits. This verifies the common renderer/root defect, not a
  fresh exact Fine Vintage Vault story replay or C++/whole-compatibility
  retirement. T00469's legacy declaration is retained for provenance; its live
  palette cast is retired. Published7b8f82b7/exact green40163900452; #172 is closed.
- #179 interrupted-scene reentry defect is repaired and closed after acceptance
  checkpoint2b2a175b/exact green40155513890. It was confirmed and logged before
  repair. Debug1792893835821337-0 stops the first chain while event81 box4/5
  dialogue is still active; normal teardown completes, but the fresh entry
  crashes at24126 in progressive text rendering after its new event buffer
  overwrites the previous borrowed string. This is distinct from #174's parent
  matrix fault. Original interface initialization clears only textbox0
  (matching800CAA9C); broad font-pointer suppression is not a lifetime fix.
  Owned native session entry, normal return and title/battle jumps now clear
  all eight textbox borrows, formatting pointers, selector/wait state and
  notification references before the next initialization replaces the heap.
  No backing storage is individually freed and ordinary in-scene closes are
  unchanged. Red3 cases/8 failed assertions becomes green15 focused cases/
  100 assertions in each config; full serial Debug/Release725 cases/
  3,164,578 assertions each pass. Selected quality is0 findings/0 new.
  Local evidence: `build/quality/scene179-*`, `family174-reentry-Debug.*`.
  Published0338c030 has exact green Source Quality40154361448. Deliberate box4
  interruption/reentry now passes Debug1792895435337715-0 and
  Release1792895991286486-0: withhold Circle from9035/9131, retain active81 box4
  through15011, then stop normally and start a fresh80/82/81 in the same window.
  The new chain reaches normal control at26972/27092 and stays there through
  39023, followed by ordinary isolated/production controls. All four scripts
  have complete teardown, zero engine errors and no forced skip/Start/adjustment.
  Both scene82 instances reproduce the exact parent/bone lifecycle and72
  deferred draws; reentry maps and actually uses cached bank99 before82.
  Debug is0338c030, Release15c70c78; only unrelated ignore/solution edits are
  dirty. Strict reports are `build/quality/scene174-verified-<run-id>.json`;
  manifests, acknowledged inputs and logs are in `family179-interrupted-reentry-*`
  and the unique runs. This establishes interrupted scene UI lifetime/reset,
  not legitimate boss defeat or broader scene/fidelity completion.
  The controller now holds Circle until production native-buttons acknowledge
  it, then releases;40ms pulses were often lost between slow Debug host polls.
  Inputs remain original dialogue actions, never Start or state mutation.
- #174 source-aligned playback gate now passes. Debug1792894544327667-0 and
  Release1792895226944470-0 each complete two fresh80 ->82 ->81 chains in one
  window, then ordinary isolated/production cases with normal teardown and
  zero engine errors. Typed snapshots confirm model171/slot7 attached to
  model7/slot2 bone2, followed by parent removal and exactly72 deferred draws
  per chain. Original SEP26/WAVE99 prelude is present; both entries map and
  actually use bank99 before82, including cached content on reentry.
  Debug normal control is11948..24011 and35964..48023; Release12212..24011
  and35988..48023. No Start, forced skip, runtime HP/flag edits or artificial
  completion. These are component fixtures, not boss-defeat verification,
  whole-scene pixel/audio fidelity or all-cutscene completion; #149 stays open.
  #174 is closed. Repairb8e2e782, componentddcee133 and session0338c030 have exact
  green runs40146187813,40153218937 and40154361448. Latest full suites are
  725/3,164,578 each config. Debug's manifest is ddcee133 plus the then-dirty
  Session/test/watchlist fix; Release is0338c030 with only unrelated owner
  ignore/solution edits. Both use the original80 component preset, seed1,
  post-minotaur profile, speed8 and enabled device input. Local assertion reports:
  `build/quality/scene174-verified-1792894544327667-0.json` and
  `scene174-verified-1792895226944470-0.json`; full logs and acknowledged inputs
  remain in `family179-fixed-reentry-*` and the unique run directories.
- #171 fresh height investigation: `ai.move` now logs queued from/to/rise,
  threshold and capability flags; `ai.edge` logs the actual branch result
  (bounds, tile flags/area, actor blocker, step distance/height, approach radius
  or destination distance) without changing permissions or return values.
  The old4,6 wolf fixture remains blocked: destination8,4, y-493..-320 and
  no ground frames. Actual spawned slot is2, not the prior run's3. Trace
  1792889290807340-0 shows eastward climbs exceeding the original130 threshold
  and north/south tile/area barriers; its128-unit detours do work.
  Do not make that fixture pass by weakening original traversal rules.
  Source-permitted controls, with only static preset starting positions:
  - Top5,6: Debug1792889378477502-0 and Release1792889627280654-0 each
    autonomously drop512 then128 units, reach ground0 at223, remain there2178
    frames, run attack command24 for1582 frames and reduce Ashley HP250 to181.
  - Uphill4,10 toward4,9: Debug1792889738902996-0 and Release1792889784999073-0
    climb128 units (y range-485..-320), then attack756 frames and reduce HP250
    to220. First attack195 and hit241 occur after reaching the target height;
    this is not cross-height attack-range acceptance.
  No runtime coordinates/HP/readiness/event/jump commands were forced.
  AI/entry/history selection145 cases/409,827 assertions passes each config;
  affected-source tidy is zero. Original actual MIPS confirms the unusual
  negative-delta arithmetic; it is not being intuitively corrected.
  Live unported hooks are not assumed causal: observed contact requests use
  base layer0, and special-terrain stack setup is distinct from the base height
  sampler. Matched owner-room controls now verify the original missing-entry
  failure: red1792855828990222-0 at4978 is NPCmodel147 taking command11 without
  an arc. Original zone9/room5 definitions6/7 (Sackheim/Goodwin), the same
  ledge approach, now climb0 to-128 (arc min-165), attack480/361 frames and
  reduce Ashley HP250 to129 in Debug1792890531220286-0 and
  Release1792890623179062-0. #171 is closed for verified jump execution and
  pursuit using cb4fe8b3/411d73a1, latest verifiedf368cbd5/exact green40148851247.
  Direct cross-height attack eligibility is separately #178, not claimed by
  attacks that occur only after reaching Ashley's height.
  Local profiles persist in the session `files/ledge171-wolf*.json`; traces
  and captures are under `build/quality/ledge171-*` and the listed run dirs.
- #175 fresh crate audit: original71618.c:1093 tries ordinary ranked actions
  first, then cached fallback against four special actors only after those fail.
  Original73460.s still requires nearby-hostile tile squared distance below3,
  range, readiness, cooldown and visibility/blocked permissions. Isolated initial
  room placement replacement removes original props, so empty isolated Hall is
  not a valid box control. Production Hall1792896917883918-0 registers boxes3/4;
  its behind-ledge control puts Ashley448 units lower and does not meet box reach.
  Production9/11 original wolf/stack1792897672113021-0 has fallback action1/
  weight128, room-delay0/cooldown0 and ready relations, but box6 distance79404
  exceeds range50176; box7 distance127020 exceeds the0x19000 cap. West-side
  1792897731306618-0 brings box6 to19974/50176 but ordinary Ashley attacks
  succeed first (target0, natural HP250->209), correctly suppressing fallback.
  No box attack/HP change or confirmed defect yet. Invalid tile2,1/height6 were
  rejected, not forced; supported height3 settles to ground, not atop the stack.
  Next observation: controller-established cover/high ground or a source single
  box where ordinary Ashley actions fail while the box remains within reach.
  No runtime stats/coordinates/flags/AI settings were written. Local read-only
  PDB-checked evidence: `build/quality/box175-*`; session `files/box175-*`
  retains fixtures/observer. Concurrent scouts stalled before GPU initialization
  and are excluded; serial probes worked, without a driver/root-cause claim.
  Retained reference audit: `rood-boi` movement is unchanged from the pinned
  counterpart; `rood-reverseWin` is not an independent Git root, so its apparent
  native-repository log must not be mistaken for upstream updates.
- #177 roulette audit is verified and closed after2cc1db5c/exact green40161617288,
  without a gameplay RNG change. The original
  chooses one of three weighted16-entry tables, seeds the selection byte once,
  then advances/decelerates it with actual stop input rather than uniform rerolls.
  Slot counts (STR/AGI/INT/HP/MP) are4/1/1/7/3,1/1/4/8/2,1/4/1/7/3;
  HP occupies7 or8 slots, not a claimed uniform per-boss outcome probability.
  Native C asset-free controls verify the actual update/award function:
  all256 starting bytes decelerate from48 through8, accumulate125 units before
  final alignment and stop after exactly40..55 calls on low nibble8, without
  a new BIOS roll. All48 table slots apply only their intended permanent stat,
  including high-byte slot indices and999 caps (96 normal/cap iterations).
  Early Circle skips presentation to timer273 without stopping/rerolling;
  full32-bit BIOS continuation survives seed restore.
  Debug/Release `[roulette] --reporter compact --rng-seed 177` each pass5 cases/
  27,185 assertions. The later diagnostics package brings full serial suites to
  733/3,191,786 each; selected tidy0 plus59 fixture literals, fast195/0 new.
  Project regeneration is stable on a second run and preserves the owner-edited
  solution. Equivalencee360fbea and diagnosticsddead640 are published with exact
  green40157906333/40159724018; no gameplay RNG/formula is changed.
  Shared Desktop/Lab frame diagnostics now have optional read-only `roulette`
  state for menu15/game-clear only: entry phase, screen state/timer, animation,
  reel, selection and full BIOS state. It is absent during unavailable inspection,
  other subtypes and after menu closure; it does not draw a random number.
  Three serialization/read-only/lifecycle cases and all host builds pass.
  Authentic live preconditions use the owner's unmodified
  generation2 save is zone9/room17, flag0x82=8. A byte-identical isolated copy
  is under `build/quality/roulette177-owner-save-copy`; the owner file was not
  written. Normal title Load/Yes, actual chest Square/take-all, directional
  movement and Cross/door interaction reach arena12/0 without flag/HP edits.
  Real-time controller fights then naturally defeat the277-HP Minotaur:
  - Debug1792900781496181-0: defeat6082; original10/21 initializes reel0 from
    raw18281 to byte104. Actual Circle stops timer351/byte52 at7632 through
    timer398/byte184 at7726:47 calls/94 host ticks. Slot11 gives HP+4 at7744,
    actually67->71 and max250->254; other stat pairs stay unchanged.
  - Release1792901413583068-0: defeat6218; raw6579 initializes byte184/reel0.
    Different requested stop400 yields timer401/byte176 at7868 through
    timer452/byte56 at7970:51 calls/102 ticks. Slot3 gives actual AGI100->101
    at7988. Square claims original boss loot; phase0/control1/menu63 returns
    at8212 and remains normal through26000 with no pending loot.
  Every intermediate deceleration byte/speed and all31,460/31,874 captured
  BIOS draws match the original arithmetic. Saved BIOS state is restored with
  no arena/reward reseed; owner save bytes remain unchanged. Both manifests
  are ddead640 plus only unrelated owner ignore/solution edits. No runtime HP,
  stats, coordinates, AI settings, progress flags or completion state were forced.
  Normal input really wins the fight and stops/confirms the original results.
  Debug's controller omitted Square and waits at loot12; exclude that from
  full return evidence. Release supplies the verified normal return. Neither
  run claims a later family-scene trigger without the required player action.
  A speed8 combat attempt died and is not acceptance; an empty-actor21 probe
  lacks its original camera actor and is likewise excluded, not null-patched.
  Strict local reports: `build/quality/roulette177-authentic-realtime-Debug-verified.json`
  and `roulette177-authentic-realtime-Release-verified-return.json`. Full traces,
  manifests and acknowledged inputs remain in the corresponding run directories
  and `roulette177-*`; session files retain controller, verifier and route.
  This verifies this audit, not all boss archetypes, scene fidelity or C++/EC
  retirement. Broader cutscene/persistence gates remain #149/#18.
- #176 zero-glyph transition repair: original matching `_renderTextImmediate`
  assembly at800C81DC/81E4/8274 uses encoded0xE5, space0x8F and subtracts0xE5.
  Copied F01337 instead used ASCII form-feed/space, so end0xE7 did not terminate
  and following zero padding rendered as digits. Event82's three string entries
  are control-only, not readable dialogue words; their word-based offsets are
  correct. The copy generator now uses the original encoded constants and
  unsigned byte loads, with no guessed string-stride or text replacement.
  Asset-free tests use the real framebuffer compositor, synthetic font pixels
  and finite strings: red4/4 cases fail with6 pixel assertions; green Debug/
  Release4 cases/10 assertions cover padding termination, visible letters,
  spacing, newlines and repeated control-only rendering. Heap, font, input and
  VRAM state are restored by the fixture. Full serial suites716/3,164,535 in
  each config pass; generator66 controls pass. Local evidence:
  `build/quality/text176-*`. This verifies the encoded transition defect, not
  the whole external event82 story sequence or the separate #174 end handoff.
  Published72139df384ad5ce26f024592ce24584df6d8a48c has successful exact
  Source Quality40147008544; #176 is closed for the exact verified renderer
  defect. Whole-scene confirmation remains in #174/#149.
- #174 family-cutscene attachment fault: owner run1792884331707681-0 reaches
  external event82 in zone19/room0/MAP212. Its original script attaches prop
  model171/slot7 to model7/slot2 bone2, then removes parent slot2 after the
  final observed wait. The unchecked renderer forms camera0xB0 from that null
  parent, matching the reported scalar-matrix AV. The typed parent-matrix
  resolver now defers only this dependent draw while the borrowed parent/model/
  bone is unavailable. It does not fabricate a world/identity camera, delete
  the independent prop, clear its attachment or prevent later parent reuse.
  Invalid slots/bone bounds are explicitly diagnosed. Original valid-parent
  rendering and all nonattached paths remain unchanged; generator F01008 owns
  the copied call-site correction. Scene model telemetry now includes attached,
  parent slot and bone to expose these dependencies in future owner traces.
  Actual linked Debug/Release attachment selection passes4 cases/18 assertions,
  including whole-child preservation and released-parent replacement.
  The6200-tick production-room Lab attempt1792886263923605-0 runs embedded
  room events (source1/id0) and returns to9/12; it does not reproduce external
  event82's attachment handoff and is **not** full-scene acceptance. The later
  source-aligned80/82/81 chains above now verify that gate. No HP,
  progression flags, fake event completion or math null fallback was used.
  Original scene82's three text entries are control-only strings, despite
  executing dialogue commands; their zero-glyph flashes are repaired by #176.
  Evidence: `build/quality/owner-reports-20261024-193314/`,
  `family174-*` and the retained unique owner run.
  Publishedb8e2e782908526abe79e38fd66ab816dec01475f has successful exact
  Source Quality40146187813; its source-aligned playback gate is now verified.
- #18 sound/cache follow-up: native file selection and affirmative confirmation
  now play their original selection sound; No/Cross/invalid/completion follow
  the corresponding original menu cues. Bounded per-picker previews avoid
  decoding/revalidating already inspected files and blank read frames on a
  cached wrap/revisit. Begin/reentry and notices invalidate previews; execution
  freshly inspects content/status and rejects a changed confirmed document.
  Successful saves update both the visible card and preview cache. Staged
  container ownership remains separate from picker cache/reset.
  Nine new controls cover cues, horizontal non-consent, cache/fresh reentry,
  changed/appeared/recovered files and staged-bank retry lifetime.
  UI/presentation Debug/Release each pass23 cases/1,621 assertions.
  Actual speed1 Desktop `save18-audio-cache-realtime` verifies navigation
  instrument19 and select/affirmative148 at the expected actions, zero logged
  errors and successful Continue. Cached file100-to1 interval reports maximum
  audio submit gap17.9636ms with zero starved requests/missing frames.
  First uncached page visit still reports676.6735ms, and explicit load
  revalidation752.1069ms; the cache fix is not claimed as complete cold-IO
  latency removal. Local evidence: `build/quality/save18-audio-*`.
- #173 actor grounded-history repair: original `40564.c:2548-2560` was copied
  through `Field_GimmickInstance*`, writing native offset6308 instead of actor
  `savedPlacement`6332. Asset-free red preserves the actor sentinel11223344
  instead of expected00060004 and corrupts neighboring state. Two exact
  regression cases reproduce31 failures/978 assertions before repair.
  Owned F00989 now accepts `Actors_ActorRecord*`, preserves all four tile bytes,
  packed X/Y, Z and facing, and updates the ground anchor only for support0.
  All16 support layers, high-bit tile bytes, signed position extrema and
  release/reentry whole-object controls pass Debug/Release; entry/history
  selection is12 cases/8,605 assertions in each. The five original
  callers are actors; the identical `44F14.c:1565-1571` inline block now reuses
  this API under its unchanged motion-state guard. No real gimmick caller was
  found in original or native references.
  Source-backed packing verification is separate from C++ promotion/EC removal.
  This does not establish autonomous enemy ascent/descent or cross-height
  attacks in #171. Local red/green evidence: `build/quality/grounded173-*`,
  `jump171-entry/consumer-investigation.json`; related full dirty-worktree suites
  (also containing #174/save-audio controls) pass712/3,164,525 in each config.
  Owner's new family-cutscene hard crash takes priority for playback; keep
  remaining direct-height-range gate #178 separate.
  Published411d73a1ae6962f2d38e1e17208a4d434051a095 has exact successful
  Source Quality40145646736; #173 is closed for the bounded placement defect.
- #18 save/load UI correction follows the owner's original Save/Load screenshots,
  not the rejected thin `SAVE NATIVE` rows. Data Save, Data Load and title Continue
  share three original-style cards with ARM room names, original location artwork,
  NO./MAP/SAVE/CLEAR, HP/MP values and bars, and HH:MM:SS. Native files use the
  truthful `LOCAL SAVES` breadcrumb instead of claiming a PS1 memory-card slot.
  Original atlas controller symbols replace spelled-out button names. Up/Down
  scrolls all100 files and selects vertical Yes/No; confirmations still default
  No, Cross cancels, and Left/Right does not navigate or grant consent.
  The renderer consumes typed snapshots, not synthesized card/save-image headers.
  Successful saves immediately refresh the committed file's card.
  - Original MAP014 save-point component profile, zone9/room5 at1,8: real
    Down/Circle/Up/Circle saves slot2 generation1 and displays its summary
    immediately (`save18-original-ui-write-new.png`). No live HP, position,
    inventory or progression edits were used to claim the controller operation.
  - Separate Debug Continue loads slot2 at1532/applies1571; Data Load then
    selects slot1 at2603/applies2642. Header/card/confirmation captures:
    `build/quality/save18-original-ui-data-load/`.
  - Recovery testing caught a WIP-only32-character prompt incorrectly constrained
    by an obsolete32-byte row element: red exit0xE06D7363 at1380. The footer now
    owns its full288-pixel/48-glyph line independently; all generic row elements
    remain inactive. Green `save18-original-ui-recovery-green` explicitly loads
    backup generation1 without changing invalid-primary SHA256. Separate real
    Data Save `save18-original-ui-recovery-overwrite` explicitly replaces it with
    generation2 and preserves backup1; its recovery badge clears after commit.
  - Release visually verifies vertical wrap to file100 and cancellation. A
    separate `save18-original-ui-release-continue` loads controller-created
    slot2 at1532/applies1571. The first combined cancel/reentry script returned
    to the original title animation before its next inputs; it is not claimed
    as a successful repeated Continue. Unit reentry/lifetime controls pass.
  Full linked Debug/Release each pass697 cases/3,163,196 assertions, serial;
  UI/presentation selection14/1,288; production frontend tidy zero findings.
  Copier63 controls pass and second source/project generations are stable.
  Validation uses cb4fe8b3 plus this UI worktree, with isolated test save roots;
  prior source slot1 SHA256 remains unchanged. Captures, logs and native saves
  stay local under `build/quality/save18-original-ui-*`, not in commits/issues.
  Backend schema3, overwrite/recovery protections and typed bank ownership are
  unchanged. #18 remains open for its separate import/migration/clear-save,
  transient-state, legitimate story campaign and EC-retirement gates.
- #171 bounded jump-entry checkpoint: F04178/F04179/F01024 are now owned in
  `Actors/MotionRequests.cpp`, following `4A0A8.c:340-410`. Command11/12,
  airborne14 and navigation18 retain original flag/branch/animation ordering,
  four packed target bytes, alias behavior and signed-low16 facing; all use
  the existing shared arc consumer. The `actors.jump` channel records requested
  targets and resulting phase/destination/velocity/countdown without changing
  route permissions. Entry/real-arc controls pass actual linked Debug/Release.
  This entry-only checkpoint was not initially sufficient: autonomous wolf
  red1792880185015402-0 stays high; patched1792880646195190-0 makes arcs but
  does not descend from that blocked4,6 start. The later allowed/blocked and
  matched owner-room controls above complete the jump scope without weakening
  original permissions. Evidence and exact layout/source
  comparisons: `build/quality/jump171-entry/consumer-investigation.json`.
  Published cb4fe8b322146d9f6188db8456a5c57464973cb7 has successful exact
  Source Quality40141256823; Debug/Release entry selection10/7,627 each.
  #171 was accidentally auto-closed by a negated closing phrase in cb4fe8b3;
  the GitHub timeline proves that commit caused the closure. It was reopened
  until the later concrete acceptance, then properly closed; direct
  cross-height attack eligibility remains #178.
  Separate F00989 corruption is tracked by #173 above, not proven causal to
  the full enemy behavior.
- #170 Gust reveal repair: owner trace identifies zone9/room6 tile15,9 kind3.
  Activation and fade already changed correctly; rendering used detached native
  globals as one PS1 record (`tail-0x58`) and copied a scratch-shaped matrix.
  Owned F00617/F00618 now import the complete original116-byte marker definition,
  construct a call-local typed group/quad and use the native view matrix,
  preserving material/texture animation/fade. No synthetic texture or reveal/
  story flag edits. Legacy G00039/40/41 remain module-binding debt, not render
  consumers; no C++ promotion or full render compatibility retirement claimed.
  Debug red1792877380886271-0 and Release red1792877773230749-0 reproduce actual
  walking activation with no marker; Debug green1792878534119518-0 and Release
  green1792878700310722-0 show the original revealed marker, including walking
  away in Release. Persistent fresh-room sequences1792878999681209-0 Debug and
  1792879031351581-0 Release each pass reveal/hidden-control/reveal-again with
  complete teardown and no errors; assertions verify fresh0/fade0, hidden0 and
  repeated1/fade128. Full suites each683 cases/3,154,382 assertions; focused
  marker8/203; copier62; fast quality195 existing/zero new.
  Evidence: `build/quality/trap170/`, `trap170-gust-*-green.png`,
  `trap170/reentry-evidence.json`. Concurrent owner commit18d715a8 captured and
  published the marker source changes alongside the owner's license work;
  its exact CI40135991604 passes. The subsequent owner revision2e3f1db4 also has
  green CI40136015635. Those owner commits are preserved without rewriting.
  Follow-up e9faac8d has green CI40136907763 and #170 is closed with measured
  evidence. #171 jump execution is now verified; #172 targeted-box texture
  remains open.
- #18 native runtime/menu checkpoint324438d948ef2b2fa4f0ca1f43bdd6c3b5536d98 is
  published; exact Source Quality40131160959 passes. Additional actual-controller
  acceptance on that clean revision, with isolated roots:
  - Changed-state Data Load1792874843480989-0 moves Z1106 to1124, reloads1106
    through the original C-only battle transition and has591 settled frames.
  - Repeated title Continue uses the original reset chord at2000, title jump2011,
    second prepare3752 and apply3791, without a process restart or error.
  - Explicit backup recovery loads generation1 and leaves the corrupt primary's
    SHA256 unchanged. A separate affirmative replacement commits generation2
    while preserving backup1. Windows deny-delete sharing produces a visible
    commit failure and unchanged primary; releasing the lock and retrying in
    the same session increments generation/game/cumulative saves exactly once.
  - Original MAP014 storage chest at1,15/layer0/facing2 is reached from1,14 by
    actual Up/Cross. Lab1792876112998915-0 stores item323 count10 through production
    menus, commits generation4 at2602 and removes it from the saved gadget bag.
    Desktop restart loads that bank, removes the same stack through production
    menus and commits generation5 at4253: bank empty, gadget bag count10.
    Discard run1792876444690926-0 restores the complete live item list
    (1700==5000) and preserves the primary's SHA256.
  Evidence: `build/quality/save18-same-process-load-evidence.json`,
  `save18-repeat-title-continue-desktop.log`, `save18-recovery-*`,
  `save18-write-retry-*`, `save18-container-store-evidence.json`,
  `save18-container-remove-restart-*`, `save18-container-cancel-final-*`.
  These remain original-content component profiles, not a legitimate full-story
  save campaign. Clear saves, import/migrations, unsupported transient/support
  states and final EC retirement remain open; #18 is not closed.
- #18 runtime/UI checkpoint now owns the three Data/Continue picker entries,
  the container transaction and original UI-asset loading/release. Native files
  use the existing readable game font,100 slots, explicit overwrite/recovery
  confirmation and persistent `save` logs; normal Save retains the original
  save-point check. Container data lives in typed session storage across UI
  resource release, not offsets into a save image. Clear-save conversion is
  explicitly unavailable rather than routed to card stubs. Builtin attract
  snapshots still use their separate original apply path.
  Pending-resume hooks preserve initial zone flags, finish after camera/display
  setup before event dispatch, and cancel before title/reset/session teardown.
  Desktop/Lab accept `--save-dir`; defaults are separate per-user `saves` and
  `combat-lab-saves`. UserDataDir now returns explicit failure and handles UTF8
  without a cwd fallback.
- Current acceptance is bounded: full production-linked Debug and Release
  each pass675 cases/3,154,179 assertions, run serially. The linked focused
  persistence selection passed46 cases/3,628 assertions before the final
  content-ownership cases. Full fast quality195
  existing/zero new; host/picker/file/resume source tidy zero findings.
  Second source render is stable751 files, plus Game/Tests project/filter
  stability without rewriting owner files. Actual Desktop controller probes
  verified Continue/paging/cancel, readable footer/error text, corrupt-file
  rejection with unchanged SHA256, directory failure, explicit process exits,
  Unicode root fidelity and a missing-directory-value exit1. Local evidence:
  `build/quality/save18-menu-*`, `save18-invalid-*`, `save18-directory-*`,
  `save18-unicode-root-*`, `save18-hosts-tidy.log`.
- The original zone9/room5 MAP014 save point is tile1,8/layer0, activation2.
  Lab run1792871002843960-0 used default post-prologue stats, no enemies or live
  HP/event-flag edits, then actual inputs
  `250:Triangle,350:Down,380:Down,410:Down,450:Circle,600:Circle,700:Circle,730:Left,760:Circle`.
  Its initial tick762 rejection exposed lazy/partial inventory display order,
  not dangling items; that source-backed case is now accepted without rebuilding
  live globals, retaining duplicate/dangling/identity checks. The next rejection
  exposed valid sector-rounded MPD requests: MAP014's65,904 actual bytes have a
  67,584-byte request. Exact/rounded requests are accepted while parsing stays
  bounded to actual bytes. Runtime fixtures now reset production momentum and
  use exclusive per-process/per-fixture directories. DataStore::ScopedOverride
  restores the exact prior instance, including borrowed references, rather than
  reopening a possibly deleted previous fixture or substituting an empty store.
  Mono output is synchronized on prepare/finish and restored on cancellation.
- Actual successful save: Lab1792874300176365-0, same original save-point
  controller replay, tick762 creates slot00 generation1,448,473 bytes,
  zone9/room5/scene1 at[192,-384,1088]. Process-restart Desktop Continue
  1792874360784383-0 prepares1532, applies1571 before event dispatch, then has992
  uninterrupted exploration frames. First-ready position, HP250/MP50/STR100 and
  yaw2048/pitch-384/distance1536 match the native document. Evidence:
  `build/quality/save18-first-roundtrip-evidence.json`, actual captures under
  `save18-desktop-first-continue`. This is a Lab-profile component save followed
  by production Desktop Continue, not legitimate story progression. Same-process
  changed-state load, live container transfers, overwrite/recovery controller
  acceptance and further repeated Continue are now covered above. Moving supports,
  active motion, first-person/non-orbit cameras and unfinished transitions are
  explicitly rejected rather than guessed. Import, older-schema migration,
  clear-save conversion and broader EC retirement remain open; do not close #18.
- #18 schema3 codec checkpoint adds explicit static collision-layer0/1,
  previous/saved placement and settled orbit-camera state. Actual pitch is
  separate from Ashley's historical cameraPitch/preferred-yaw field. V1/v2
  are explicitly rejected; no migration or invented camera/layer defaults is
  claimed. Linked Debug and Release `VS.Tests.exe '[native-snapshot]'`
  each pass12 cases/168 assertions; focused codec/test tidy reports zero new
  production findings. Evidence: `build/quality/save18-schema3-codec-tidy.log`.
  This checkpoint only publishes the DTO/codec extension, not the uncommitted
  runtime/menu integration. Its then-observed linked static-floor fixture and
  directory-ownership failures are repaired in the newer checkpoint above.
  Full gameplay/container/compatibility acceptance remains open.
  Verification was against17288c76 plus the dirty integration
  worktree; only the codec files/tests and this handoff belong to this milestone.
- Runtime-source audit identifies original G00254 as G00246+0x68: the low-ten-bit
  scene/area identity, not an independently written native value. Boot restores
  it through savedEntryDoor.requiredFlagId; Field_CurrentSceneId reads the room
  environment and drives actor-area/visited-area state. Native persistence must
  capture canonical Field_zoneContext.currentSceneId, not the detached alias.
  Schema2 adds explicit stable scene identity. Pre-integration schema1 is
  rejected by the standalone codec instead of guessing scene0; an application
  migration could derive it from validated room content, but none is claimed.
  These helper-only milestones have not created owner native save files.
- #18 storage checkpoint31b4247e has exact green Source Quality40113479363.
  The schema subpackage adds the Game-owned v1 JSON codec: named inventory/
  storage/settings/stats/limbs/arts/score/trial/room-cache fields, logical script
  flags, stable item/location/action IDs, source glyph-byte arrays, clocks and
  BIOS/interface/shuffle/movement random-state staging. It does not dump native objects or serialize
  pointers/padding/transient load markers. Integrity uses canonical-JSON FNV-1a64
  for accidental corruption, not authentication. Unknown keys, wrong types,
  extents, duplicate keys/IDs, excessive nesting and unsupported versions fail.
- Schema work exposed a coupled storage-policy bug: Save silently overwrote an
  unverified current file via its valid backup. The new future-format red case
  failed2/14 before repair. Default Save now refuses that replacement until
  explicit WriteConsent::kReplaceRecovered; both-invalid files remain protected.
  A missing primary can still be re-created without destroying existing bytes.
  Actual Windows failure/recovery controls and codec controls pass together:
  24 cases/400 assertions each configuration. Full Debug/Release648/
  3,150,686 pass; full quality195 existing/zero new; source tidy zero new.
  Game/Tests scoped project generation is stable. Logs: `save18-schema-*`,
  `save18-consent-*`, `save18-schema-generation-stability.log` in build/quality.
- Codec decoding stages data only. Live content-reference/cross-slot validation,
  safe gameplay capture/application, session storage ownership, actual Data/
  save-point/title Continue wiring and real process-restart round trips remain
  unfinished. Do not call this functional in-game save/load or close #18.
  Native UserDataDir currently falls back silently if SDL_GetPrefPath fails;
  that must become an explicit error before connecting production save roots.
- #169 map-boundary repair is published in f1bba2b0 with exact green
  Source Quality40111624104. The matching full-story event4 handoff remains
  open for verification; native save/load #18 is now the owner-prioritized work.
- Initial #18 milestone is native-only versioned JSON, with Game semantic
  validation and Engine recoverable storage; PS1 import is explicitly deferred.
  Engine SaveStore storage is being implemented/tested first, not advertised as
  working in-game saving yet. It validates every incoming/current/backup payload
  through a mandatory Game callback, returns explicit recovery provenance,
  preserves the prior valid backup, bounds file sizes and uses same-directory
  flushed temporary writes/commit plus OS-released per-slot write locks.
  Game snapshot schema, Data/save-point menu integration and repeated title
  Continue still require implementation and actual round-trip verification.
- The #18 storage subpackage now passes12 focused cases/249 assertions in
  Debug/Release, full636/3,150,535 each, full quality195 existing/zero new
  and source tidy zero/zero. Tests exercise actual OS-sharing commit failures,
  failed backup replacement, per-slot exclusion, valid-backup recovery,
  invalid/oversized/missing files, uncommitted temporaries, Unicode roots and
  exact embedded-byte preservation. No owner saves or memory-card files are
  touched. Scoped test-project generation is stable and leaves the solution/
  external tool projects untouched. This does not claim abrupt-power-loss
  simulation or working production save menus; the next subpackage is Game
  semantic snapshots and their validated application.
- Historical owner follow-up (the latest sections supersede this checkpoint):
  #169 is the
  first-lever-room crash; #170 Gust reveal, #171 ground-enemy ledge descent and
  #172 selected-box texture are separate reports. Owner requests native
  saving/loading under #18 next after the current crash repair checkpoint.
- Actual Desktop1792855828990222-0 is clean6de0a140: original9/11 event4
  issues68/zone19/room0, then MAP212 loads51834 and RoomState aborts51835.
  The sfizz destructor stack is secondary to the logged runtime abort.
  Original MAP212 has32x32 collision cells but308 direction bytes; MAP214 has
  13x25 and308. Original initialization reads only the fixed0x498-byte header.
  Native validation incorrectly required a full collision-grid direction tail.
- Local #169 repair retains fixed-header checks and bounds every actual mutable/
  read-only direction-byte access via a shared checked native cell. Missing
  bytes explicitly error/refuse rather than inventing directions. Strict copier
  fixes cover route/wander/temporary-filter consumers; owned corner/ground
  consumers share the same checks. The old short-tail test changes acceptance
  only because these original rooms demonstrate the valid format; independent
  missing-cell/negative/edge/null/canary and atomic-refusal controls remain.
- Red native shape controls fail2/2; actual pre-fix MAP212 run1792858131205962-0
  reads the asset40 and aborts41 (later scripts unrun). Fixed Debug
  1792858558711300-0/Release1792858778063531-0 each retain1170 ready frames
  for original19/0, repeat19/0 and21/0, zero errors and full teardown.
  Those cutscene fixtures remain control3, not completed story scenes. The
  Circle interaction control did not start event4; two Up exit attempts in
  Debug1792858944213004-0 stayed9/11. Do not call either a matching door pass.
  Next actual-story observation needs the legitimate owner progression/event4
  context; no flags/HP/event completion were forced.
- Current #169 source passes full Debug/Release624 cases/3,150,286 assertions,
  copier60, stable1,016 native files plus manifest, and source tidy zero/zero.
  Exact commands/hashes/results/limits: `build\quality\room169-actual-evidence.json`.
  Keep #169 open pending the actual event4 handoff even after publishing the
  verified map-boundary repair. Preserve owner VTune results and debug.log.
- Publication checkpoint: source fix `7b6fb81e` is published through
  non-rewriting integration `309f8c78`; exact Source Quality40104975781 passes.
  Debug/Release full suites622/3,150,240 each, copier59, full quality195
  existing/zero new. Closed with individual evidence: #150, #152-#155,
  #157-#159, #161-#166. #156 stays open because the observed control return
  at3572 leaves fewer than120 settled ticks before the3,600 budget; no complete
  door claim. #151, #160, #167 and broader #148/#149 remain open.
  All owned replay processes are stopped. Owner documentation reorganization,
  Skeleton tests, solution/tool projects and reference edits remain independent.
- Owner pause/commit checkpoint: all owned replay windows are stopped.
  Exact-history diagnostic rerun1792852943838401-0 ended by a graceful close,
  normal exit2:32 passed/7 blocked/one cancelled/501 unrun. It did not reach
  the #167 failing history, so it is not a crash fix or a reproduction failure.
  #167 remains unresolved; the earlier run's actual renderer failure and
  frozen ACFD8139/5B64FBA0 exe/PDB remain the next investigation source.
  Local dump capture is smoke-tested; the SDK reader can inspect memory and
  label raw stack symbol candidates, but its automatic stack unwind was
  incomplete on the smoke fixture and must not be called a verified call chain.
  Publication is now authorized; preserve the owner's independent documentation,
  Skeleton test and reference edits when committing this crash-work checkpoint.
- The following notes preserve pre-publication observations on `508b4bd7` plus
  the dirty worktree. #150's original Bat/gimmick endpoint fix and shared
  Desktop logging were verified locally; #151 inventories all **1,001 directed door
  records / 255 ZNDs / 421 room references** and runs production room/entry
  events through one persistent Combat Lab window. See [COMBAT_LAB](COMBAT_LAB.md) for CLI,
  strict pass definition, per-case checkpoints and explicit blocked/unrun
  outcomes. Do not interpret the full inventory as 1,001 executed scenes.
- The matrix exposed independently tracked #152 optional movement, #153
  auxiliary perception ownership, #154 mutable-kind AI retirement, #155
  post-arena-reset AI invalidation, #156 empty gimmick stack top, #157 SFX bank
  underflow, and #158 missing required scripted-effect target. Generator changes
  remain in the copier; owned cleanup/session implementations retain their
  separate lifetimes. No flags, actors, HP or event completion were forced.
  All issues stay open pending their publication/CI and acceptance gates.
- #158's actual 9/17 ->12/0 entry targets model53/E222 while only Ashley exists.
  Original Minotaur spawn requires flag1==1 and flag130==8; the fixed profile
  does not satisfy that. F01170 now errors/yields before changing effect
  playback rather than binding sentinel1 and crashing F00882. Four present
  model/direct/object controls and six refused-binding controls pass, including
  a different-model sentinel collision and unsupported reference tags.
  This is explicit failed-fixture handling, not verified Minotaur cutscene
  completion. Other nullable bone-matrix consumers still need their own audit.
- Exploratory run1792819424667543-0 continued beyond that refusal (33 passed,
  two failed/two blocked before real F10 cancellation), but predates tightened
  acceptance. Red lifecycle tests found eight failed assertions: pending-only
  self-loops and interrupted restored-control intervals could pass incorrectly.
  The runner now requires readiness revocation and 120 uninterrupted settled
  ticks. **Use only the new acceptance revision for final runtime counts.**
  Debug run1792819672204643-0 stopped at case136, after87 passed/46 blocked/
  two failed cases, at the independently reproduced #159 bank-lifetime crash.
  Original event28 releases bank3 before a later room-change idle restoration
  tries to undo its outstanding vertex edits through that detached bank.
- #159 now preflights borrowers, unwinds live vertex events, retires borrowed
  cache/event bindings and then releases Content storage without replacing the
  frozen skeletal pose. Native unwind/release return statuses are propagated
  to existing callers; unavailable restoration retains the original obligation.
  Real opcode36 X11->10, unrelated/two-owner/atomic-refusal controls pass.
  Fresh replay1792822646791332-0 reaches32/1 with zero engine errors/completed
  teardown, but uses event0 rather than the failing history's event28:
  **it does not verify the old branch**. Replay the full prefix/history without
  forced scene/story flags before claiming the crash acceptance. Its65-case
  tail stopped after33 passed/15 blocked, with16 still unrun, at a new
  ClosedRing renderer AV in z023-r004-d002. Do not call this full cutscene or
  final matrix acceptance.
- #159's full original prefix1792823796837457-0 is complete:87 passed/
  47 blocked/two known failed, normal exit2/all136 finished. Case136 actually
  observes event28/source1 in32/0, releases bank3 at142903 and crosses the old
  idle/vertex fault into32/1 still in event28, zero errors/complete teardown.
  The longer entry chain stays blocked at3,600 ticks, not passed cutscene/door
  acceptance. Command/frozen-image hash/scenes/release steps/result are in
  `build\quality\sequence159-aligned-evidence.json`. Release original-prefix
  replay1792829725153757-0 now independently matches those counts, event28 in
  both32/0 and32/1, and the bank3 release at142903, with zero target errors
  and complete teardown. Its exact image/command/scene proof is
  `build\quality\sequence159-aligned-Release-evidence.json`. Both longer entry
  chains remain blocked; publication/CI are still open.
- New #160 is a PLG053 closed-ring low-pointer read in E084 at24/15, native
  RVA36F1B1. The pointer is0x74 and dataIndex is read at+2 (fault0x76), not a
  large field offset on a null struct. Original MIPS binder/consumer both use
  table base0x98; no index-base change is justified. Read-only original E084
  has one texture table and15 nonempty sprite programs. A read-only debugger
  snapshot independently establishes definition16/lifetime30/age0, table1,
  program1 and info0x74. Do not infer this index from arbitrary stack words
  or invent a default texture.
- A separate confirmed #160 safety defect is repaired locally: F01590 clears
  only32 original bytes, leaving the native40-byte texture state's final
  pointer uninitialized. Poison/canary regression was7/8 then8/8. Its body now
  clears the typed native object, retaining the existing C entry signature
  as migration debt. Missing-table initialization diagnostics record context,
  request/instance effect and packed ring definition without changing playback.
- With the reset repair and initialization diagnostics now built, fresh
  matched E084 run1792825963767984-0 still faults at low pointer0x74 (+2 read),
  RVA36F48F. The reset defect alone is NOT the render-crash fix. The earlier
  absence of persisted initialization diagnostics was a buffering defect,
  not evidence that those diagnostics did not execute.
- #161 repairs that separate durability defect: Core and shared GameplayTrace
  flush Warn+ even while ordinary messages remain buffered; Error counters
  still count only Error+. Four live-file red assertions now pass, together
  with a separate Core-only/no-observer control (112 assertions/15 cases).
  Actual AV run1792827063353609-0 persists all four texture records in both
  complete JSONL streams and session text before RVA36F8F3/read0x76. At tick1508
  the table is already unavailable at birth: primary package84, definition16,
  packed0x101E, table1/program1, count1, frame word0x0002001D. The reported
  instance effect56 is not package ownership proof: primary instance creation
  does not initialize that secondary-only field. No context-switch fix is
  justified by that value.
- Read-only original bytes confirm PLG084 is identical to PLG053, including
  its CA/CB texture selectors. Original E084 definitions15-18 also contain1/1,
  its renderer keys select those definitions, and its texture binder only
  populates table0. Thus the native root is not yet resolved by a source-only
  index or renderer substitution. Next discriminating observation is the
  original running scene's table1 binding and texture-frame consumption.
  Keep #160 open; no missing-table/default-frame fallback has been introduced.
- The temporary texture probes no longer read primary instances' uninitialized
  secondary-only effect IDs; provenance selects the primary/secondary request
  matching the resource context. Red generator tests caught all three readers.
  The same bounded #148 batch names five verified parameter/local bindings in
  F01590-F01592 (`storage`, `animation`, `programIndex`, `command`) through the
  existing copier regex rules, including generated declarations. Copier51 and
  a second stable generation pass; all1,015 selected native source/project
  files and the manifest are unchanged on the second run. Full Debug605/
  1,813,310 and scoped Python quality0/0 pass after this diagnostic/name change.
- Current full suites after warning durability are **605 / 1,813,310** in both
  configurations. Actual particle-render/reset/lifetime selection is94/12;
  do not use `[particle]` alone for the older renderer tests (`[particle-render]`
  is their tag). Texture fixture/tooling quality0/0; copier49 passes. None of
  this establishes that the actual E084 crash is fixed. Read-only Workshop
  session52c3c765d52d4f7884055c0b701ac804 was closed with zero staged changes.
- Continuing after #160 exposed distinct #162: five records passed, then
  z024-r000-d000 crashed during its original24/0/E265 source entry, leaving94
  selected records unrun. F01592 read program+cursor at0xFEDF, reached through
  PLG265's particle-ribbon renderer. The source left the first line-only
  dispatcher's `VS_PARTICLE_UNTEXTURED` option active for its second, textured
  dispatcher. Original MIPS independently shows the first allocates0x10 while
  the second allocates0x30 and initializes the texture stream. The copier now
  applies F02351's instance correction, retaining the genuine line variant.
  Its native prefix request also uses `offsetof` rather than16: current heap
  rounding already covered the24-byte line prefix, so that control was not a
  demonstrated overflow. The textured red control had capacity24 <64.
- #162's exact six-case history now passes6/6 in both Debug1792830601020715-0
  and Release1792830661742875-0. Each records604 original E265 active-effect
  frames in24/0/event0, then24/3/control restored/159 destination ticks,
  zero target errors and complete teardown. Evidence:
  `build\quality\particle162-actual-evidence.json`. This verifies the bounded
  loader/entry/control case, not complete visual or all-cutscene parity.
  Final suites607/1,813,347 each, copier52, affected fixture/tooling tidy0/0;
  copier module23 existing/0 new. Second generation preserves1,015 selected
  native source/project files plus manifest. Both Desktop/Lab configs build.
  The100-case persistent Debug continuation1792830723227930-0 outcome follows;
  #160 itself remains unresolved and explicitly excluded by the resume start.
- That continuation completed26 passed/one blocked, then exposed #163 at
  z024-r008-d003 (24/8 ->39/0), leaving72 selected records unrun. The new
  C0000094/RVA1E5C0A is event-camera interpolation, not particle rendering.
  Single-edge1792830988376901-0 reproduces it; diagnostic1792831225654460-0
  captures a zero-duration look-at track at1078 from special operationB7/09.
  The original orbit-return commands are four bytes `D4 10 10 55` and three
  bytes `C2 01 55`, but G00130/G00131 were isolated scalar opcodes. Their
  missing payloads, not an authored zero duration, caused the divide.
- #163 restores complete command arrays, generated module-data/symbol
  capacities and proper array-decay callers through the copier/data ledger.
  Interpolation math and original85-frame timing are unchanged. Native tests
  first fail all three extent assertions, then pass35/2 using synthetic module
  images through actual registered loader entries plus85-step track completion.
  Fixed Debug1792831568527363-0 and Release1792831616664973-0 both execute the
  same39/0/event0/scene30 B7/09 branch at1078, start easing1/duration85, restore
  control and pass, zero errors/complete teardown/no zero-duration warnings.
  Exact proof is `build\quality\camera163-actual-evidence.json`.
- Latest full suites609/1,813,382 each, copier54, camera fixture/tooling
  quality/tidy0/0, stable second generation across1,015 selected native files
  plus manifest; both Desktop/Lab configurations build. Matching100-case Debug
  continuation1792831745328564-0 is active. Earlier stopped runs are not
  completed coverage; #160 remains excluded and unresolved. No new issue has
  been closed or source published.
- That100-case continuation finished62 passed/nine blocked, then crashed in
  active z028-r003-d000 (28/3 ->28/4), with28 records unrun. New #164 is
  F01384's null text-pointer write at native RVA238D12/address6. G00211 was
  already marked drop/replaced-by G00614, but the renderer still consumed it;
  G00614 was also const despite digit writes. The copier now supplies writable
  nine-byte storage and the renderer uses it directly. Native packet/format/
  glyph/ordering/terminator/guard/show-hide controls retain the original
  four-byte preceding ordering slot, independently confirmed in MIPS.
- Actual readable timer rendering exposed separate #165: the original packed
  timer word G01934 is the time-field subobject at G01933+4, but native storage
  was independent of `State_sceneTimer`. The HUD now consumes a typed const
  scene-clock pointer and its sole production caller supplies the authoritative
  clock, without copied state or a new packed-pointer bridge. The data ledger's
  original-subobject note is provenance, not a generated `view-of` directive;
  an explicit regression prevents recreating that bridge. Dormant G00211/
  G01934 compatibility declarations remain retirement debt.
- Final timer/binding/pause/reset selection370 assertions/five cases and full
  **614 cases / 1,813,753 assertions** pass in both configurations. Copier56
  and a second stable generation preserve1,015 selected native source/project
  files plus manifest. Scoped timer/camera fixture/tooling quality/tidy is
  eight existing/zero new, with no baseline change; both Desktop/Lab configs
  build. The red ownership control originally had five failures/40 assertions.
- Post-fix live Debug1792834573551820-0 and Release1792834651270755-0 each
  execute164 timer draws in28/4/event0/scene23, display00:30:00 through
  00:26:92 with every minute/second matching that record's source fields, then
  restore control/pass701 ticks/zero errors/complete teardown. Owned-window
  captures visibly show the readable00:29 countdown. They wait for a real
  `hud.timer` record and never focus another window or modify gameplay state.
  Per-run capture paths, exact commands/image hashes/results and validated
  source/text pairs are in `build\quality\timer165-actual-evidence.json`.
  Ordinary sequence-end `--capture` shows the editor after teardown and is
  NOT evidence of timer visibility. No puzzle-win or reference-pixel claim.
- Persistent Debug1792834906085352-0 completed67 passed/ten blocked, then
  crashed in active z028-r005-d002 (28/5 ->32/6), with122 of200 unrun.
  The original #164 history case passes695 ticks/zero errors/complete teardown.
  New #166 is audio auto-pan, not a timer regression: its resolved function
  is `Audio_Mixer_UpdateMusicChannelModulation`, old native RVA27C8D2, reading
  pointer012600DB00960060. Single-edge reproduction is deterministic.
- #166's frozen PDB/read-only snapshot proves primary music channel6, native
  width312, rate2/shape65/depth0/flags1044, with all16 nominal waveform pointers
  valid. Original MUSIC172 channel6 really contains `BC 02 41`; original
  dispatch and operand lengths prove that boundary. Original table[65] aliases
  adjacent pointer entries to80018BD8, whose source bytes form a bounded
  16-state LFO cycle. They are data here, not native executable pointers.
  The native16-entry inference was wrong. A source-role importer/256-selector
  repair is implemented locally: explicit sample owner, bounded program validation,
  full immutable resident payload as short data, unsigned enable operands,
  checked lookup/atomic refusal. The25 source paths leaving that image stay
  explicitly unavailable, not fabricated samples.
- #166 validation now includes1024 exact original/native samples for EACH
  of231 bounded source programs, source-data/host-code collision controls,
  unsigned byte/rate0/128/255, zero-depth phase, looping, disable/re-enable,
  atomic refusal and existing audio reset controls. Final full suites pass
  **622 cases / 3,150,240 assertions** each; focused audio/data1,507,745/85.
  Copier58 and stable second generation preserve1,016 selected native files/
  projects plus manifest. Main scoped quality26 existing/zero new; headers
  zero/zero. No baseline/suppression changes. Engine project generation added
  only the new native LFO header after checking existing owner edits.
- The original32/6 event0 -> event42 chain needs more than3,600 or7,200 ticks:
  those were honestly blocked, with script-PC progress measured before
  extending observation. Final Debug1792841911523801-0 and
  Release1792841730609451-0 both complete in7,832 ticks with restored control,
  zero errors and complete teardown. Each has1,248 actual note-on records,
  source65 binds mapped to the explicit immutable sample owner at original
  offset0x8BD8, and no forced skip. Exact commands, executable hashes, source/
  owner correlation and complete results are in
  `build\quality\music166-actual-evidence.json`. No HP/flags/timers were edited;
  only real waiting-dialogue Circle input was used. This is not complete
  reference PCM parity or support for undefined source-memory paths.
- Old #166 fault image hash26D9F3335F2144772F5B4ADF00414F0255794CD60A021529A835CF400BE44240
  and exact shape/table/program snapshot are in
  `build\quality\music166-state-Debug-replay.log`. The debugger's frozen-layout
  probes are now opt-in (`--frozen-music166` / `--frozen-texture160`); never
  enable them for a newly linked image merely because an RVA happens to match.
  Debug1792842259573600-0 selected200 cases from z028-r005-d002 with an
  explicit9,000-tick budget in one window. The outer two-hour debugger budget
  expired normally as a harness limit (exit124), without a native exception:
  161 passed/38 blocked/one interrupted z040-r008-d000 (inventory index460,
  zero-based). All199 completed rows have zero engine errors and complete
  teardown. Blocked:20 fixture defeats,8 destination events unsettled,
  9 source events unsettled,1 unavailable destination. This is not200 complete
  cases or full door acceptance. Exact evidence is
  `build\quality\door151-matrix166-partial-evidence.json`. PID47920 has exited.
  The separately named local `door151-long-debug.exe` extends only the outer
  diagnostic deadline to six hours, smoke-tested with a normal child exit;
  per-case acceptance is unchanged. Resume inclusively at the interrupted ID
  with the ordinary3,600-tick case budget now that the known7,832-tick music
  entry has completed in the preceding run. Do not aggregate revisions.
  Continuation Debug1792849529393328-0 selected the remaining541
  records inclusively from z040-r008-d000 (`--door-limit 600 --speed 64
  --frames 3600`), same frozen ACFD8139/5B64FBA0 exe/PDB, attached shell
  `door151-tail-debug`, owned game PID2908 (wrapper64100, both exited).
  The previously interrupted first record passes. New confirmed #167 stops
  this run:94 passed/37 blocked, active script132 z048-r006-d000,
  409 unrun, exitE06D7363. Its original events reach32/17/event0/phase4/
  scene30/control3; last complete tick167126/script_tick1050, thread0 PC+233.
  Tick167127 reports malformed0x34 packet56 words/offset53, first00221915,
  native game heap+45138, then unhandled C++ exception. Full per-script JSONL
  parses; both render errors persist. Related #26 packet corruption root is
  not established. No renderer skip/guard or product fix has been added.
  Matching-image standalone, last-two and last-eleven controls do not crash;
  standalone1792852476217916-0 remains honestly blocked through3,600 ticks,
  not a passing door. Persistent history must be retained until localized.
  The local wrapper now captures C++ first/second-chance stacks and writes an
  ignored unique full-memory dump only on unhandled exceptions, without
  frozen-offset probes. An owned throwing smoke fixture verified both chances,
  expected exit and a10MB dump. Dumps remain local; do not commit or upload.
  Current exact-history rerun Debug1792852943838401-0 uses the same unchanged
  Lab image/PDB and command; shell `packet167-history-debug`, gamePID73268,
  wrapper59308. Window responsiveness and actual trace progress verified.
  Capture stack/dump before any Debug Lab relink. The helper's6h deadline
  does not alter3,600-tick case acceptance.
  #160 remains an explicit excluded blocker. All new work remains local/
  unpublished; publication/CI/closure gates are open.
- While that image remains frozen, bounded #148 naming work replaces17 verified
  bindings in owned FlightTerrain.c and its public declarations: AI state,
  movement angle, neighboring tile/height, segment endpoints/steps and sample
  index. Meanings match original771A8.c:942 and7E05C.s:187, plus all movement
  callers. An identifier-aware comparison proves no changed member names,
  signature types, constants, operators or control flow. Debug and Release
  `[ground-query],[flight]` each pass103 assertions/13 cases; scoped format/lint/
  clones and source clang-tidy are zero/zero. Commands/results are in
  `build\quality\symbols148-flight-{proof,Release-build,Release-tests,tidy}.log`.
  Matching Debug build/test logs use the same prefix and Debug configuration.
  Debug Lab exe/PDB hashes remain ACFD8139/5B64FBA0; do not attribute these
  naming changes to the already-running matrix image. No promotion, retirement
  or new gameplay acceptance is claimed.
- A second bounded #148 batch names17 bindings in F00670-F00675 through copier
  regex rules: room geometry group indices/count/header array, complete flag
  swap and signed upper-halfword depth bias. Original2842C.c:550-606 plus event,
  gimmick and renderer callers verify the roles. The entire generated
  RoomEffects.c/header is token-identical outside those names; function symbols,
  types, stride, bounds, masks and arithmetic are unchanged. Six red generator
  controls are now green, with an unrelated-scope negative control; all59 pass.
  Both Debug/Release `[render][room],[ground-query],[flight]` selections pass155
  assertions/17 cases. Scoped copier/test quality is zero/zero; stable generation
  covers1,016 native source/template/project files plus the manifest and preserves
  owned edits. Evidence uses `build\quality\symbols148-room-*.log`. The frozen
  running Lab image predates both naming batches; no fresh gameplay claim is
  assigned to those edits.
- Fresh Debug and Release full suites each pass **599 cases / 1,813,261
  assertions** before the final exact-shape/binding expansion; current full
  totals after #159 are **602 / 1,813,292** each. The independent hidden original-disc door selection passes
  **41,459 assertions / one case** in each configuration. Copier tests47 pass.
  A second copy generation preserves all1,023 native tracked/untracked source/
  project files and the manifest. Scoped Engine/Tests/Lab project generation
  is stable twice, preserving owner-dirty root solution/tool projects. Both
  Desktop configurations build with detailed logging enabled. The broad audit
  caught 18 new quality findings in the added fixtures/preview code, repaired
  without baseline changes; final affected-file quality/tidy is131 existing/
  0 new. Scene diagnostics now record four actual VM threads and eight logical
  dialogue waits, not their overbroad legacy declaration extents.
  Logs are `build\quality\door151-current-*`; reference/disc material remains
  local and uncommitted. #149's all-cutscene investigation remains open.
- Owner stopped the per-process roster because repeated windows were disruptive.
  Do not restart it in that form. #143's native single-window script sequence
  is published/closed. The owner permanently closed #144 as NOT_PLANNED:
  do not revisit startup investigation.
  Four-script Debug
  run1792780995004927-0 preserves HWND66322990, fresh repeat resources/inventory/
  models and normal living-Lich teardown; actual Release F10 run1792781463758439-0
  preserves an unrun tail and returns2. Full worktree556/1804672 each, scoped
  quality0 new, Desktop build passes. New sequence CLI and per-script logging are
  documented in COMBAT_LAB; local roster driver now launches one sequence host.
  Sequence publication excludes #141's original-behavior question. The
  subsequently completed Score work is described below.
- #65 is verified/closed: scoped47fdbc17982e7878033fa40f4527954472ffa441,
  published ddf8171a5e39d609c7c8342d75240bc17261e94c, exact CI40023077559 green.
  All four actual Score submenus enter twice in one window/configuration.
  Real Zombie defeat unlocks the original model; Debug1792788396368047-0
  viewer4557/5091 and Release1792788491009214-0 viewer4564/5101 render and
  accept rotation/zoom controls with0 engine errors. No fake flags or platform
  fallbacks. Score2200/5; copier32; full worktree559/1804706 each, including
  separately uncommitted AI naming/perception tests; full quality498 existing/
  0 new. Commands/hashes/limits are in COMBAT_LAB. Final host close while
  viewing is intentionally interrupted, not claimed F10 teardown.
- #145 is verified/closed: scoped971c21dd, published4ceceda5, exact
  CI40016804261 green. Actual Worker -> Hall -> Worker -> Hall controller
  routes and original gimmicks pass Debug/Release; source relative offsets
  and owned-prefix bounds are retained. This is not a null-check workaround.
- #148's published batch a04a048d (merge8330fddc) renames five verified AI fields,
  with exact CI40023536123 green, shared
  by-offset source tables, eight canonical/fork ledger rows and five native
  offset assertions. Debug AI401006/124 and the current full suites pass.
  Refreshed index:138 TUs/100172 occurrences/0 unresolved/0 mismatches;
  22 existing error TUs remain explicit. No old five field names remain.
  This naming-only batch is separate from Score; keep #148 open for the broader
  25710-occurrence/2550 file-identifier-pair/393-file candidate inventory.
  No guessed names, behavior fix, C++ promotion or retirement claim.
- #146's proven operand-order repair is now scopedcb6e12a2, publishedaa09cef8,
  exact CI40030529880 green. Original MIPS reads register index BEFORE its
  halfword; native F01531 evaluated the assignment's RHS first, so E095's
  three stages all selected timeline0. F01531/F01532 now sequence their
  reads explicitly, preserving byte truncation/unsigned comparison and
  verified local names. Red46/248 ->green248/2. Copier33; stable second
  generation; full Debug/Release561/1804954 each; maintained498 existing/0 new.
  Do NOT change the strict F01540 remap bound: original800D4E28 confirms it.
- Fixed full-prologue runs reach48000/22 captures each, normal exits/0 errors:
  Debug655s has14 real breaths/70 limb results; Release122s has10/50.
  Native timing/Continue differs; counts are observations, not parity.
  Original839 inputs/only Start1000, no state writes. Lab1792791081079977-0
  proves timeline0/1/2 with1/9/10 tracks and actual HP997->957 at1190.
  Animation255 is VALID;91-frame bank/event dispatch and61+ changing pose/
  rendered-matrix hashes already worked. Source/assembly/commands/hashes and
  local evidence are in COMBAT_LAB and build\quality\dtok146-fixed-*.
  #146 is now CLOSED: subsequent1792793560646680-0 directly captures the active
  mouth flame jet in the native framebuffer. Acceptance7a33fe33/438c8edc,
  exact CI40035907684 green. No boss-victory claim.
- Owner requested autonomous completion after the zero-tick visual observer.
  Targeted sessions resumed, not the disruptive per-case roster. Captures now
  use DPI-aware PrintWindow on only the owned client and per-run/entry folders,
  not desktop coordinates or stale shared filenames.
- #147 is now CLOSED: scoped508b4bd79d58e37421fdd9c693c92937f2b5de0e,
  published421d444090aede716f557c2536b7e589f38ba3a9, exact CI40040775242 green.
  F00980 is registered owned C with typed actor/companion and separate gimmick
  callers. Actual fade bytes and the two selected160-color basePalette rows
  are used; currentPalette remains pulse-only. Canonical gimmick CLUT element
  width now agrees with existing ledgers without span/offset growth.
- Correcting state ownership alone left the image solid. Original assembly
  also requires appearance material selection, semi-transparency, subtractive/
  additive page modes and uniform Gouraud fade tint. Native ModelGeometry now
  preserves those rules, pulse/ordinary controls, adaptive splits and companion
  mode1. Red ownership2/2, blend80/128 and Gouraud20/128 controls become green;
  combined7878/4. This is not an arbitrary alpha fade or full EC retirement.
- Four genuine lethal Fireballs and normal F10 cleanup/re-entry pass in ONE
  window/config: Release1792797118662852-0 (hits1868/4167, ends2310/4609);
  Debug1792797363044415-0 (hits1866/4162, ends2306/4604). States1..5, actual
  fade0..64, old-view byte unchanged, floor visibly revealed before removal.
  Fresh999HP/999MP restored; original enemy HP86 is valid base85 +/-10 from
  146C.c:6128. Both0 engine/trace errors. Evidence dtok147-final-evidence.json,
  per-run captures and committed COMBAT_LAB; no live state writes. No prologue
  story-event victory or all-model acceptance claimed.
- Final Debug/Release full565/1812832 each; copier35; stable regeneration and
  ownership; scoped quality131 existing/0 new, owned implementation0 findings.
  Full original-reference index138/100172/0 unresolved/0 mismatches with22
  existing error TUs explicit. Only Game projects were regenerated via the
  existing scaffolder APIs; owner-dirty solution/tool projects were preserved.
- Broader #148/#16/#76/#80/#141 remain separately tracked. Naming batches and
  residual unknowns are documented; do not invent names, force readiness or
  infer C++ promotion/compatibility retirement from these fixes.
- Local HEAD deliberately remains508b4bd7. A concurrent owner Audit0933950e
  changed BUILDING plus a new report while BUILDING was already owner-dirty.
  Initial push was rejected; normal virtual merges most recently published
  01bf7884/a258dfdf/4ceceda5/ddf8171a/8330fddc/aa09cef8/438c8edc/421d4440 without
  stash/reset/rebase/force-push or touching any working file/local branch.
  Native/copier/scoped evidence trees match the scoped fixing commits;25 dirty/
  untracked paths were preserved before updating these owned handoff notes.
  Metadata: lab-sequence-{header-,}integration.json, gimmick145-integration.json
  and score65-integration/preservation.json. Thirty owner/independent entries
  were hash-checked unchanged for the Score publication before these owned
  handoff notes were updated.
  Initial CI40008528885 caught a missing direct <string> include; it was fixed,
  not baselined away, and exact follow-up40009015714 passed.
  Preserve this state; do not overwrite owner docs just to align branches.
- #15/#65/#78/#81/#83/#136/#137/#138/#139/#140/#142/#143/#145/#146/#147 are verified and closed. Current ordered
  parent is #16. Broad #76 original-frame/space/trail fidelity and #80's
  owner-reported unspecified multi-part case remain separate/open.
- #139's original key/sigil acceptance is complete locally on63beaf268:
  all32 named items in Debug/Release, wrong-facing controls in each positive,
  six missing controls and two actual repeat/F10/restart controls. All72
  sessions end normally with0 engine/trace errors. Reusable keys stay10;
  sigils10->9/flag2; Chest Key10/flag903=1; fresh entry resets10/locked/flag0.
  Full door inventory preservation passes. Debug/Release host hashes are
  840827e8ef5779bd68ab4c7c12452ce38a9dd939f93e6baa91662957b602159c /
  1598c04e8094e30e32fed4f2b9ba526ac37c3d95de2d628bd30130e36dfb0480.
  Native/generator sources are unchanged; temporary fixture diagnostics are
  removed. COMBAT_LAB and three function notes are published in9d43d9cb;
  exact CI39968889070 green and #139 closed. Evidence: build\quality\key139-final-audit.json;
  repeats1792764752246225-0/1792764775422319-0. No stock expansion, writes,
  synthetic locks or event-policy changes. The MAP160 height71 fixture
  rejection preserves the original63 guard and is not a native bug.
- No MCP disc changes, reference checkouts or installed tools were modified.
  The read-only disc session is closed. Preserve unrelated dirty files and
  leave this mixed owner handoff uncommitted.
- Further bounded #76 ribbon component evidence is published ina37ac5a0,
  exact CI39972483524 green: two PLG057 render cases cover96 configurations,
  matrix selection, exact geometry/UV/RGB/commands/OT, history wrap/collapse,
  strict depth256 near255/256 clipping and exact buffer fits. Final combined
  trail selection3036/4; Debug/Release full suites541/1802392 each; maintained
  quality0/0. Source9d43d9cb plus tests/docs/two function notes only; no runtime
  or generator change. Original texture timer8 was confirmed before correcting
  an initial fixture expectation7. Evidence: build\quality\trail76-render-*.
  Original-frame visual alignment and full source/target attachment callbacks
  still prevent blanket #76 completion; #80 still lacks the exact owner
  failing action/enemy (working147/source9 four-hit99HP evidence is on #80).
- Read-only branch-aware upstream check: local rood-reverse5163208b is ahead
  of origin/main e0875ad8 (a differing remote HEAD was not a newer revision).
  noprompt/rood-reverse decomp/verified-functions94a9e90d adds two commits to
  local39821e4f: the same19-function5163208b change and its merge, not an extra
  trail algorithm. noprompt/rood-boi81388794 adds five commits to localb2c3fede;
  wave2 has566 files (all pages inspected for metadata) and172 battle ports,
  not a new trail renderer. F01600 gravity/drag was identified as the relevant
  particle-adjacent entry; this was not a complete behavior audit of172 ports.
  New upstream room-entry null-LFO fix f2920efc affects15/0 and48/3. Native
  already initializes pending waveform counters to1, preserving slides until
  actual enable/sample; existing music/effect x vibrato/tremolo/pan regression
  passes6 assertions in each build. No PS1-address fallback was imported.
  Logs: build\quality\reference-update-{Debug,Release}-modulation.log;
  metadata: reference-boi-wave2-files.json. No reference fetch/checkout/edit.
- Current #16 corpus prepared368 supported source definitions /93 distinct
  names and87 explicit unavailable definitions; it stopped after34 clean
  Release initialization cases at confirmed Lich crash #140, logged before
  repair. Before Release1792768167460696-0/Debug1792768368479320-0 write0x10
  from F00103's unconditional optional-UI update. The scoped generator gate
  keeps all NPC geometry/results while updating UI only for isPlayerAction;
  other NPC collectors already follow this contract.
  Fix is now published/closed in13531f55 via3a9ed355, exact CI39980344884 green:
  final full543/1802406 each,
  focused14/2, copier26,751 generated/93 owned stable, maintained quality0/0.
  Fixed automatic Debug1792768984618836-0/Release1792768986232195-0 apply
  area130 five hits totaling398 and area126 five hits totaling392, with
  exact HP deltas and one38MP debit each; natural regen is recorded separately.
  Live Debug1792769467702248-0/Release1792769524387582-0 each perform3 exact
  seeded entries/3 F10 stops, including live area-effect cancellation and
  actual first/third-entry five-hit field returns. Four accepted traces are
  normal/0 errors; observer1792769290342565-0 is excluded for missing expected
  actor count, not a native failure. Audit build\quality\lich140-final-audit.json.
  Root sourcea37ac5a0 plus scoped changes; do not stage this mixed handoff.
  Roster resume now requires matching host/input hashes and frame/speed limits,
  so old initialization records cannot silently stand in for the fixed host.
  The earlier900-tick Wyvern quiet period was NOT a confirmed stall. Longer
  Debug1792770140866236-0/Release1792770170565795-0 reach field at902 and Bite3
  damage at3446, with normal3600-tick endings. Breath148 hit/visual fidelity
  remains unverified; do not invent a stall fix. The remaining93-name corpus
  is being refreshed on the fixed host, with exact runtime revision/dirty
  state now read from each native manifest rather than stale catalog metadata.
- Fixed-host Release distinct-name pass is complete:93/93 initialization
  records,89 with selected native actions/combat results, minimum845 ready
  frames, all normal/no errors and exact source-profile traces/input hashes.
  Audit build\quality\arena16-roster\Release-names-audit.{json,csv}.
  Additional Debug3600 controls prove Zombie/Ghoul Normal1 hits begin1711/1793;
  Dummy is intentionally inert. Plain Skeleton remains an explicit #141
  verification gap, not a guessed native bug: default3600, adjacent2400 and
  low-defense2400 controls select no action. Original ZUD has14816 battle
  animation bytes, not a missing on-disk sequence. Guarded actual-F6 read-only
  probe at1208: weight40, threshold6/readiness579, range36864/distance25188,
  delay0, relation400000E4 missing execution-ready bit31. Later1216 probe:
  viewCos3850, near/far589824, facing1018/offset0, pos208,0,784 vs player320,0,832.
  Initial88 probe still has uncomputed distance0/relationC0 and facing0.
  Next observation is the original/native perception/FOV/readiness producer,
  not forcing relation bits, range, timers or attacks.
- The all368-definition Release corpus ended. All368 supported definitions
  have initialization records:356 historical pre-#142 and12 on the fixed host.
  This is not fresh all368 verification or proof of all actions/defeats.
  Resume requires exact host/input/frame/speed matches and stop-on-error.
  No batch remains running. Work resumed after the owner pause with #141's
  original/native perception producer, without changing readiness conditions.
- Fixture wording correction: preset position `direction` is the production
  floor-layer selector0/1, not a cardinal facing override (ActorState.c
  PlaceActorInRoom). COMBAT_LAB's Lich fixture wording is corrected locally.
  No native field/API or actor facing was changed.
- All-definition pass stopped at #142 after356 pre-fix initialization passes:
  Gremlin97/2, MAP428/source row1, valid tile9,13 aborts before ready at46
  (Release1792773773981507-0/Debug1792774027535234-0). Cached frame heightMap
  was null during initial hover12; authoritative room map was already valid.
  Owned RoomState now publishes context before map callbacks/actor creation
  and clears borrowed context after actor teardown/before room free. No height
  math/bounds bypass, fake floor or in-command RNG refresh. Fix5a2d0866 is
  published in20ab4126, exact CI39993700195 green and #142 closed:
  focused20/2, full545/1802426 each, quality0/0.
  Debug7200 run1792775021061322-0/Release1792775009541767-0 initialize/move cleanly
  but have no offensive results; do not claim complete Gremlin AI. Actual
  normal-speed3-entry/F10 runs1792775386956705-0/1792775405766255-0 match seeded
  resources/models/positions and retire each world normally; four traces0 errors.
  Release observer1792775262731783-0 is excluded: clean18000-tick editor budget
  exit, not a product failure. Audit gremlin142-final-audit.json.
  The final11 definitions subsequently initialized cleanly on the fixed host.
  The356 historical passes are not fresh verification of this changed
  lifecycle; do not resume under a stale-host success assumption.

### Recent checkpoint notes

- #136/#137 target/planner repair is published in
  `073d58c5680772beb7910a225d5439bde8305135`; both issues are closed after
  exact Source Quality39940975312 passed. Native preview/result buffers retain255
  entries; whole-actor pages preserve40 candidates in20-entry visible slices.
  The coupled fixed7884-byte AI scratch overflow is replaced by typed call-local
  storage, preserving later instances' ZUD battle-animation data. Final Debug
  `1792751783466397-0` and Release `1792751783218080-0` exercise paging,
  cancellation/re-entry, absolute20->actor6 real hits, all eight defeats and
  normal loot/gameplay return. Two fresh page captures and two actual F10
  live-target stops also pass explicit trace/error audits. Full suites530 cases/
  1790939 assertions each;24 copier tests;751 outputs/93 owned files stable;
  quality/tidy0 new with one existing scaffold advisory. See the #136/#137
  checkpoint in [COMBAT_LAB.md](COMBAT_LAB.md). Current #15 work is the original
  city-to-workshop43 route plus live blade/shield transactions. Final Debug
  acquired two original loose bronze shields; Release acquired none. No
  combination acceptance is inferred from those targeting runs, and no
  same-preset editor restart is claimed. Keep this
  mixed owner handoff uncommitted.
- #15's final missing live blade/shield transactions now pass on073d58c5.
  Debug/Release blade runs1792752847953340-0/1792752847807210-0 use actual
  guaranteed Sabre/Rapier loot and original door43, two cancellation cycles and
  actual bronze Guisarme64 consumption with all3840 inventory bytes checked.
  Original seed47 shield drops are acquired through ExplosionIV plus Fireball;
  runs1792753985429352-0/1792753985229923-0 verify two bronze Buckler sources,
  cancel/re-entry, actual consumption and field/menu returns. No injected loot.
  Fresh assembly/naming, armor, gem, wear and all seven location gates also pass:
 26 accepted sessions on the current native revision have normal ends and0
  engine error/critical/trace-failure records. Current wear12592/12598 repairs
  to12600 with equipped refresh. Detailed IDs/commands are in COMBAT_LAB and
  `build\quality\workshop15-final-operation-audit.json`. Native binaries are
  unchanged. Acceptance is published ina1dff2281e929c7237d5fe4c416298250cfe0086;
  exact Source Quality39947119197 is green and #15 is closed. Current ordered
  work is #16 with its separate #78/#80/#81/#76/#83 limits.
- #78 root is now precisely aligned with #136, not just a guessed association:
  the current source9/Normal1 producer emits36 candidates; candidate31 is
  `{1531,-308,1367,0}`, exactly historical invalid pointer0x557FECC05FB.
  Temporary trace run1792756208327969-0 is retained locally and its generator
  hook removed. Final noninstrumented Debug1792756376145405-0 and
  Release1792756431965231-0 each run three exact eight-Minotaur entries, real
  96HP attacks, F6/F7 one-step during active phase7 and F10 stops/restarts.
  Seeded HP/MP/models/source indices match across entries; all nine actors
  remain, no errors or trace failures. Both full suites532/1790951; copier24;
  scoped test quality0/0. Explicit boundary/canary and36-target24+12-page tests
  are uncommitted with the COMBAT_LAB checkpoint, pending scoped publication/
  exact CI and #78 closure. Broader #80/#81/#76 remains unverified.
- #78 acceptance is now published in65ef4deef211a05ae162fcf54beada751d0b3244,
  exact CI39949550123 green, issue closed. Current #81 is implemented locally:
  correct mask/palette animation was already produced but all175 body faces
  used original CLUT. Typed actor polygon submission now applies original8
  per-limb face ranges and +0x480 only to selected groups; weapon entry points
  remain uniform and unrelated appearance stubs remain debt. Red32/50,
  green packet50 assertions. Final533/1791001 full suites each; quality0
  native/0 new with131 existing test findings; copier24 and751/93 stable.
  Actual final Debug soldier1792758009859446-0/Ghost1792758053402526-0 and
  Release soldier1792758084997631-0/Ghost1792758128007604-0 verify masks128/64/32,
  selected versus original submitted CLUT faces, page actor change, all-model
  cancellation clearing, re-entry and real lethal Fireball/normal post-defeat
  flow. All four final traces clean; original Ghost auxiliary slot6 retained.
  Pending scoped native/test/docs/ledger commit, push, exact CI and #81 closure.
- #81 is now published/closed in4bd0a3e7f8f3c9770d7588ef24f0e4c864366cd9,
  exact CI39953055128 green. Current #83 repair reuses Desktop's established
  `AudioDevice(speed==1)` policy: Lab formerly always chose realtime and current
  speed4 aborts at1132 (run1792758315183981-0). Accelerated native ticks now render
  synchronously; only already-rendered stale speaker PCM is explicitly discarded
  past250ms (bounded97600 bytes including one block). No voice command drop or
  larger queue. Eight final Debug/Release speed1/4/repeat4/64 runs finish1800
  ticks with clean traces; accelerated captures1436600 frames byte-identical
  across repeats/speeds per build, finite audible stereo. Both full suites
  534/1799247; focused mono/stereo4096-tick queue bounds plus existing speed1
  demand/stall9463 assertions/4 cases; quality0/0. Docs/COMBAT_LAB checkpoint
  contains exact8 run IDs, hashes/commands/limits. Native Lab/test/docs changes
  uncommitted, pending scoped publication/exactCI/#83 closure; then #80/#76
  broader multi-part/reference-particle gates under #16.
- #83 is published/closed ind00027688ff9a2f608c95a2c082627c7324a16ff,
  exact CI39954919516 green. Current #138 is a confirmed supporting #76 defect:
  compiled native particle152 bytes; stale whole-node view reads emitter flags112
  as strength and pointer bytes98/100/102 as target, rather than strength140/
  target124. Red15/109; generator4-field fix preserves original arithmetic and
  distributions. Debug/Release before-live E09489/90 false-force components at
  ticks459-467 becomes0/90 after fix. Fixed later flame-burst comparisons1272/
  1236 all pass. Final live fixed runs1792760103122030-0/1792760126849209-0 and
  later1792760262016302-0/1792760287240181-0 return normally; no errors/trace
  failures. Actual source-backed Hellhound147, no memory/HP/event edits. Full
  suites537/1799356 each; focused355/13; copier25 and751/93 stable;quality0/0.
  VS-Workshop read-only disc inspection kept0 changes and its session is closed;
  sprite-program preview is explicitly not full runtime trajectory reference.
  Native/generated/copier/tests/docs/ledgers are uncommitted, pending scoped
  publication/exactCI/#138 closure. #76 stays open for complete original-frame/
  attachment/trail acceptance; do not infer full fidelity from this one repair.
- #138 is now published/closed inf13f144d90eb6c54d9056eb94a3aa0b8b34fc19f,
  exact CI39957776285 green. Follow-up5/9 trail source-golden regression adds
  508 assertions/2 cases covering two12-step paths, exact original coefficients,
  independent histories, wrap, collapse, adjacent guard and repeat reset. Both
  full suites539/1799864 and scopedquality0/0 pass; no production trail change
  was justified. Unit/source evidence is not live original-frame fidelity.
  ParticleRenderTests.cpp, COMBAT_LAB and four function notes are uncommitted,
  pending scoped publication/CI while #76 remains open. No portable DuckStation
  save state/card was found for an aligned original combat reference.

- [#4](https://github.com/JEschete/VS-NativePort/issues/4) remains the ordered
  tracker. Quality prerequisites #3/#5, dialogue #1, menu rendering/lifecycle #2,
  and AI/initial drops/defeat flow #6 are closed. Sound #7 is now closed against
  its bounded functional criteria. Item use #8 is now closed; learned-spell
  use #9 is now closed against its bounded representative criteria. #10's
  all offensive/defensive chain ability acceptance is now closed. #11's full
  forty-art functional acceptance is also closed. #12's original item-drop
  selection/delivery acceptance is now closed. Equipment-drop acceptance #13
  and bounded staging repair #128 are closed. Equipment-menu acceptance #14
  is closed. Workshop-gated modification #15 is also verified and closed.
  Current ordered gameplay work is the comprehensive arena audit #16.
  The owner's read-only reference
  particle/path/trail refresh is complete; see the reference checkpoint below.
  GitHub already records #82 closed at2026-10-22T21:58:16Z. Its existing
  comments still describe bounded successful replays without an independently
  captured fault; do not mistake that separate closure for a new root-cause
  verification in this work.
- Current published revision: `63beaf26819b3f73a801baa0506cc70b051223f5`,
  pushed to `work`; exact Source Quality39959691765 is green. Native
  target/planner defects #136/#137 are closed. Native door-query storage #135 is verified and closed
  after exact Source Quality39921522813 passed. Placement-scope repair #134
  in `deb6c5b8` is also verified and closed with the combined live gate.
  Bounded workshop-operation verification in799d8854 is published
  after exact-revision Source Quality39914001594 passed; final #15 acceptance
  later closed after a1dff228 and exact Source Quality39947119197.
  Weapon naming-render repair #133 in6df50baa is verified and closed
  after exact-revision Source Quality39909886880 passed.
  Workshop selector-storage repair #132 incd2f911f is verified and
  closed after exact-revision Source Quality39907208600 passed.
  Equipment-menu acceptance #14 inb45dfd3c is verified and closed
  after exact-revision Source Quality39903038043 passed. Native model-storage
  repair #131 inadd47793 is verified and closed
  after exact-revision Source Quality39900130928 passed. Canonical
  preview-shield repair #130 is closed in1837e136 after
  Source Quality39893247389 passed. Earlier revision
  `e4d006694a188e996d66c390e2cb1ca23dd140a5` restored manual CI. Its Source Quality
  [manual run 39891724186](https://github.com/JEschete/VS-NativePort/actions/runs/39891724186)
  and [push run 39891724472](https://github.com/JEschete/VS-NativePort/actions/runs/39891724472)
  passed every step on that exact revision. #120's full Break Art menu repair
  and the shared Mesh10/tiled-ring import repairs #121/#122 are closed;
  original-equipment Lab selection is published and empty-slot materials #123
  is now verified and closed. Thunderwave's original zero-width remainder
  repair #124 and Iron Ripper's cone-emission repair #125 are also published,
  verified and closed. Heaven's Scorn's pending audio-modulation initialization
  repair #126 is also published, verified and closed. The forty-art damage/
  secondary-effect checkpoint and two maintained regressions are published
  in `f432f475`, followed by the60-encounter/400-gate acceptance matrix in
  `b59a6a7a`; both exact-revision CI runs passed and #11 is closed. No baseline or
  suppression was changed. #119 is now fixed in its own metadata commit:
  the orphan gitlink was removed while its ignored local reference checkout
  remains clean at `39821e4f`. CI checkout cleanup no longer emits the
  missing-URL/exit128 warning. Unrelated edits were not included.
- Six issue-scoped commits publish the verified item/Lab/rendering fixes:
  `af771c69` (#32), `d33b3f43` (#84), `fa794fab` (#115 and Lab fixtures),
  `65f3c8f1` (#85), `5aa6adab` (#86), and `9745ac35` (#116).
  Those six tickets are closed with individual fixing commits and replay
  evidence. Additional fixes `b6e0a84a` (#117, actual caster telemetry) and
  `ad6b2adb` (#118, Ghost secondary-body loading) are also pushed and closed
  after exact-revision CI and real-input acceptance.
  Unrelated documentation rewrites and local reference changes remain
  untouched/uncommitted. The orphan submodule removal was deliberately
  published separately for #119 in `a320c82e`.
  Disc-derived assets, captures and local diagnostics stay out of commits.

## Current workshop checkpoint

#15 remains open. All six original workshops and outside-room rejection pass
actual-controller gates in Debug/Release. The maintained gate regression covers
56 workshop/capacity/cursor combinations. #132 was logged before repairing six
unaligned PS1-sized temporary pointer tables through the generator. Its red
production regression corrupts the reserved heap-tail guard in all six
selectors; fixed full-capacity rows and inventory checks pass402 assertions.
No independent gameplay crash is claimed.

The #132 repair is published in cd2f911f and closed after exact-revision
Source Quality39907208600 passed. Final rebuilt full suites
pass503 cases/1,786,633 assertions each;20 copier tests pass; second generation
preserves751 outputs and87 owned files; maintained quality/tidy is0 findings/0
new. Repaired live Debug1792733764298314-0 and Release1792733836872489-0 repeat
blade/grip/armor chooser entry/cancel twice, release copied lists, preserve
inventory and return to field. All18 final gate/browse sessions pass the
explicit error/trace audit. See
[COMBAT_LAB.md](COMBAT_LAB.md#workshop-gates-and-native-selector-storage-15-132)
and local `build\quality\workshop132-*`.

The owner then reported the blank assembly naming editor, logged as #133.
Its generated F02781/F02782 raw pointer links are repaired in6df50baa. Red renderer
tests fail14/26 assertions; fixed Debug/Release pass30 assertions. Inspected
captures show the assembly/explicit naming editor and actual edited name/grid
selection in both builds. Two complete disassembly/assembly/rename/re-equip
cycles per build restore all3840 inventory bytes. Final suites pass505 cases/
1,786,663 assertions each;21 copier tests and751/87 generation audit pass;
quality/tidy0/0. Eight accepted sessions pass explicit trace/error audits.
See [COMBAT_LAB.md](COMBAT_LAB.md#weapon-naming-rendering-133) and local
`build\quality\naming133-*`. #133 is published and closed after exact-revision
Source Quality39909886880 passed.

#15's next checkpoint now verifies actual Grissom gem loot/removal/rollback/
reattachment and stock Bandage armor combination in both configurations.
Two repeated stock repair UI cycles pass but are explicitly no-op durability
coverage. Twelve new component cases cover meaningful DP/PP repair, equipped
runtime refresh, ownership, ordered synthetic/original recipes and material
filters. Focused14 cases/5,108 assertions and full517 cases/1,790,641 assertions
pass per configuration; quality/tidy0/0. Twelve actual-operation/preview
sessions have normal/error-free traces. See
[COMBAT_LAB.md](COMBAT_LAB.md#workshop-operations-checkpoint-15) and local
`build\quality\workshop15-*`. This checkpoint is published in799d8854 and
Source Quality39914001594 is green.

The lawful route reproduced #134: the active Lab reinserted source enemy18
after changing to workshop42, faulting in Field_SpawnRoomActors at tick2261
(red1792738861766246-0). The native hook is repaired locally to scope injection
to both source zone and room, preserving original destination placements.
Focused19 assertions and full519 cases/1,790,660 assertions per build pass;
quality/tidy0/0. Debug1792739660623754-0 now retains actual melee wear12598/12600,
repairs it to12600 and returns to field. Release can stop at the opened door
even with sustained input (1792739974174070-0); no Release transition claim.
See [COMBAT_LAB.md](COMBAT_LAB.md#cross-zone-placement-repair-checkpoint-134).

#135 is published in813acea0 with registered owned F00579/G00330/T00232 in
`Field/DoorMatches.cpp/.h`: a native vector backs the borrowed query view.
Red storage assertions fail2/2; fixed122 assertions cover order/40-record
platform lists/repeats/empty/RNG/malformed lengths. Full Debug/Release522
cases/1,790,782 assertions pass;22 copier tests pass;751 outputs stable and89
owned files preserved. Only the Game project/filter were regenerated via the
existing scaffold functions; owner's solution hash is unchanged.

The repaired final Debug1792741749151138-0 and Release1792741827685837-0 now both
make real melee wear12598/12600, cross the original doorway with the exact
inventory, cancel repair, confirm12600 and return to field. Both final traces
are normal/error-free and explicitly use original workshop placements. See
[COMBAT_LAB.md](COMBAT_LAB.md#native-door-query-storage-and-real-repair-acceptance-135-134)
and `build\quality\door135-live-audit.json`. Final maintained quality has1
existing scaffold tooling finding/0 new, native0; no baseline changed.
Exact813acea0 Source Quality39921522813 passed; #134/#135 are closed with the
fixing commits and actual evidence. #15 still needs live blade/shield
combination consumption. Do not close it from recipe tests alone.

Original MAP205's room-section door record leads
from workshop42 tile3,0 to zone13 room12 (MAP040); its top-level door section
contains event data and must not be parsed as transition records.
The active Lab placement hook currently reuses configured placements on every
room load; cross-zone consequences are not reproduced or established as a bug.
Stock Fandango starts at
full DP with a zero-slot grip, so no-op repair/empty gem browsing is not
acceptance. No inventory-capacity expansion or process-memory writes are
authorized as a substitute. Keep this mixed owner handoff out of scoped commits.

## Current enemy-drop checkpoint

#12 is now closed. Original MPD spawns supply two guaranteed item/count entries
and one item/rate entry; there is no universal enemy-item table. A confirmed
capacity defect was logged as #127 before repair: a rejected99+2 claim changed
99->100, then retry after freeing a slot produced102 instead of101.

The published F02511 generator fix preflights misc writes through the existing
query path. Original64 slots/100-per-stack and source ownership are unchanged.
The red test failed2 of7 assertions; repaired Debug/Release focused tests pass
3 cases/28,767 assertions and full suites pass474 cases/1,125,294 assertions.
Scoped quality: one existing finding, zero new, no baseline changes. Both
generations preserve86 owned files; the second preserves751 generated hashes;
all15 copier regressions pass. The function ledger records bounded misc-copy
coverage only, not equipment acceptance, C++ promotion or bridge retirement.

Live Debug `1792714592030967-0` and Release `1792714659101918-0` defeat the original
Zombie through a nine-window controller chain and take item323 x2 through the
normal loot screen,10->12. Each trace has one normal final record, no trace
failure/error-level engine entry, and a normally closed host. Full-capacity
reject/retry remains component evidence, not a live full-inventory claim.
See [COMBAT_LAB.md](COMBAT_LAB.md#loot-transfer-capacity-checkpoint-127) and
`build\quality\loot127-*`. #127 is closed after scoped publication in `a3a8cf1e`
and successful exact-revision Source Quality39877581259. The subsequent
checkpoint completes #12's original rates/RNG order and bounded encounter
acceptance.

The subsequent #12 acceptance checkpoint is published in `bc76bfc6`:
eight new drop-rule cases pass106,926 assertions; together with capacity checks,
11 cases/135,693 assertions pass in both configurations. Full suites pass
482 cases/1,232,220 assertions each; quality has one existing finding, zero new.
The original two guaranteed MPD entries and optional third ID/rate are checked,
including every roll0..254, exact RNG order, difficulty/count staging filters,
ordinary/retained eligibility, repeated defeat, prologue suppression0..2 and
heap/list cleanup. No further production change was needed after #127.

Read-only Workshop source inspection agrees with native catalog provenance.
Six new live sessions verify two defeats each for Minotaur, Dullahan and Bat in
Debug/Release, including real F10/re-entry, exact original drops, empty pending
lists on return and Minotaur key466's flag904 changing0->1 in each instance.
All end normally without trace failures or error-level engine records. See
[COMBAT_LAB.md](COMBAT_LAB.md#enemy-item-drop-acceptance-checkpoint-12) and
`build\quality\drop12-live-audit.json`. The Workshop session is closed with no
changes; no owned host remains. Original boss-story scripts/key-door traversal
and equipment #13 are not claimed. Exact-revision Source Quality39881313511
passed; #12 is closed and #4 is updated. #13 is now current. In F02511's shield
branch the local names `source` and
`target` are reversed, but the actual inventory copy direction is correct.
Do not log that naming alone as a defect. Keep unrelated dirty files out of
scoped commits.

### Equipment-drop verification and staging repair #128

The initial tests in `CombatTestCases.cpp` added eight equipment cases: all255
rolls at strict probability boundaries for four equipment kinds, independent
property checks, the two-armor-output limit with six limb draws,1080 composite
capacity combinations, shared armor/accessory capacity, and difficulty/row/
claimability staging. Debug/Release pass547,962 focused assertions and
490 cases/1,780,182 full-suite assertions **before** the subsequently added red
staging-boundary case. Quality/tidy at that green checkpoint reports one
existing finding and zero new. Those are pre-repair results.

Twenty real Fireball defeats across twelve native sessions exercise Mandel
weapons, Lizardman weapon/armor, Minotaur Zombie accessory, Dullahan equipment
rate0, and Zombie Knight probabilistic equipment. The Debug seed42 Knight
delivers shield132; Release seed42 delivers armor164/180 instead, so do not
claim identical outcomes across timed controller runs. PDB-checked read-only
observations compare initial actor component properties to pending loot and
then delivered inventory, including ownership indices. Evidence and limits
are in [COMBAT_LAB.md](COMBAT_LAB.md#equipment-drop-and-staging-acceptance-13-128),
`build\quality\equipment13-*`, `live-equipment13-*.json` and native run folders.
The observer is `build\quality\equipment13-observe.ps1`, loaded by the existing
live driver with `-ObserveEquipment`. All owned hosts and the read-only
Workshop session are closed.

**Original red reproducer:** `[staging-capacity]` failed14 of366
assertions. All seven `Menu_Loot_Stage*` helpers rejected the final valid slot
(8/16/16/8/16/48/64). #128 was opened before any production repair. Their caller
unconditionally appends the zero result, yielding an invalid item index;
composite staging also lacks complete component-space preflight. No actual
crash is claimed. Eight allowed Lab enemies can reach these boundaries.
Original available Lich/Lich Lord records in zones48/56 can additionally drop
two limb armors plus an accessory, so a full batch can exceed even a corrected
16-slot temporary armor array. A mere off-by-one edit or silently skipping
failed rows is not a complete repair.

The repair now uses bounded successive batches through copier fixups and a
small compatibility-private `Menu\Loot\Paging.h`. Complete component preflight
prevents partial weapon/shield staging; failed stages defer instead of
appending invalid indices. Source flags are consumed only when a batch is
claimed or positively discarded. Cancel and mandatory-key protection retain
their existing behavior. Cleanup resets source pointers before freeing them.
Only row metadata/backups grow160->176, not item arrays or player capacity.

Final Debug/Release focused selection is27 cases/688,244 assertions;
full suites are498 cases/1,784,771 assertions each. Production menu-state
component tests caught and fixed a cursor reset error on deferred pages, and
cover take-all, cancel, positive leave and keys. The24-armor multi-page test
is component evidence, not a real-controller overflow replay; bag-space
release between pages is a fixture precondition, not organization-menu proof.
Copier regressions pass18 cases; second generation preserves751 generated
outputs and87 maintained paths. Scoped quality/tidy has one existing finding
and zero new, without baseline changes. See the Combat Lab section for exact
commands, final host hashes and evidence paths.

Final-host ordinary-controller acceptance passes20 defeats across12 sessions,
including cancel/take-all, confirmed discard, key protection and F10/re-entry
on both configurations. Pending/delivered properties and ownership are checked
read-only. Final Knight outcomes differ between builds; shield132 remains
pre-repair evidence, not a newly repeated final-host pickup. All owned hosts
and the read-only Workshop session are closed. Details/run IDs are in the
Combat Lab table and `staging128-live-*` / `live-staging128-*.json`.
Published implementation:
`536d18114718edb768ae68ccb2b513933f719db2` on `work`. The scoped commit includes
production/copier/tests, `COMBAT_LAB.md` and function-ledger notes; this mixed
handoff and unrelated edits remain uncommitted.

**Publication gate resolved:** the first push returned `remote: fatal error in
commit_refs` / remote rejected, but both `git ls-remote --heads origin work`
and the GitHub ref API independently show536d1811. A non-forced retry succeeds
with `Everything up-to-date`. No Actions run or check suite exists for that
SHA; `gh run list --branch work --commit 536d18114718edb768ae68ccb2b513933f719db2`
returned `[]`. #129 added `workflow_dispatch` and exact-head recovery instructions
in e4d00669, without changing existing triggers/jobs/checks or rewriting history.
Both manual39891724186 and push39891724472 passed on that exact descendant.
`git diff --quiet 536d1811 HEAD -- Native tools` confirms native/tool source
is unchanged from the locally verified implementation. #129, #128 and #13 are
closed with fixing commits and evidence; #4 is updated.

#14 is now claimed in order. Its acceptance covers real-menu equip/unequip,
legal slots/handedness, derived values, instance preservation, model/texture
and preview lifecycle, restrictions/cancellation, empty slots and repeated
changes. First-entry Fandango was visible in run1792724245959463-0; the older
missing-weapon observation was not reproduced on that build. Combat Lab setup
overrides alone do not demonstrate game-menu acceptance.

#130 is published/closed in1837e136. #131 replaces a1512-byte, alignment1
model reserve with the actual1528-byte, alignment8 native weapon model.
Final Debug/Release suites pass500 cases/1,785,023 assertions each, focused
equipment6/300 and19 copier regressions pass. Repeated generation preserves
751 outputs/87 existing owned source/header paths. Broad quality's four new
findings were repaired; final targeted tidy/style have zero new findings.
Real one-handed and greatsword shield cycles, all stock slots and Ghost
secondary-body F10/re-entry pass on the final hosts. Evidence and hashes are
in [COMBAT_LAB.md](COMBAT_LAB.md#native-equipment-storage-repair-131-parent-14).
The seed8888 shield comparison failure was observer error: Fireball changes
affinity index2 from3 to4 before loot is copied. Exact replay verifies
defeat-time4 -> pending4 -> claimed4; no native loot fix is warranted.
The scoped repair is published inadd47793; exact-revision CI39900130928
passed and #131 is closed.
#14's remaining local acceptance now passes. The all-stock-loose category
matrix, one-/two-handed exact controller replays and prologue/Tarnish gates
pass in Debug and Release. All ten final traces close normally with no
engine errors or trace failures. Equipment contributions and packed range
are checked separately from actor totalStrength. The two gate harness
failures were observer assumptions (closed sentinel63; stale pre-F5 frames),
not native defects. The new maintained gate regression passes480 assertions;
each full suite passes501 cases/1,785,503 assertions. Scoped quality/tidy is
0 findings/0 new. See COMBAT_LAB.md's equipment-menu checkpoint and
build\quality\equipment14-final-audit.json. The scoped tests/docs/ledger notes
are published inb45dfd3c; exact-revision Source Quality39903038043 passed and
#14 is closed. #15 is claimed next, beginning with original room/workshop
state and material permissions before testing actual modification operations.

## Current Break Art checkpoint

#120 corrected G01005 through the F02610 copy fixup: four art IDs plus
View Arts, with the matching reload extent. Original layout provenance is
retained. That menu checkpoint passed **455 cases / 1,078,556 assertions**;
generation is stable, 86 owned paths are unchanged, and 11 copier tests pass.
Focused quality/tidy has one existing finding, zero new.

Debug `1792693091210881-0` and Release `1792693210368049-0` verify actual
five-row navigation through all ten categories, equipped-category restrictions,
four-row shortcuts, fourth Sword art191 selection, no-target rejection and
cancellation without spending resources, plus F10/re-entry/Prologue reset.
The new `all-break-arts` profile grants IDs184-223 without changing equipment,
inventory or progression counters. The forty original names, costs and
acquisition thresholds are listed in [COMBAT_LAB.md](COMBAT_LAB.md).

#11's enumeration criterion is checked. Wider actual art execution, damage,
secondary effects and target/range acceptance remain open. Initial source
tracing maps secondary mediators to Paralysis, Poison, Numbness, Curse,
equipment reduction, armor wear, MP damage and Risk reduction; do not infer
their behavior from names alone. No all-forty effect certification yet.

The current scoped #121 repair corrects fourteen Mesh10 ledger extents and
regenerates complete storage/imports for all seven shared instances. The new
regression failed fourteen assertions before the fix. All loaded bytes,
selectors, indices, UV offsets and repeat imports now pass. Final Debug and
Release suites pass **458 cases / 1,082,195 assertions**, including the
separate #11 kill-point qualification tests in `b18ec9e7` (2 cases / 1,860
assertions). Focused quality across both test files has four existing
findings, zero new; no baseline/suppression changes. Second generation is
stable and all eleven copier regressions pass.

Cherry Ronde190 completes three encounters per host with two F10/re-entry
cycles: Debug `1792696331113100-0` (four casts including one zero-damage
result/retry), Release `1792696162985589-0` (three casts). Every cast pays
HP55, applies the actual reported damage and recovers control. Release
Papillon Reel191 also passes in `1792696417809149-0` (HP75, damage1).
All three traces end normally without errors. #121 is published in `25831e66`
and closed after successful exact-revision CI. This does not close #82 or
establish visual parity.

#122 is now closed with the verified repair in `5272b755` for all thirteen missing
tiled-ring color imports. Each extent is32 bytes, following the original
eight-CVECTOR definition and0-7 selector. All thirteen destinations failed
the regression first. The tests now cover exact source bytes, bounds and
reloads, plus the byte-identical PLG085/139 shared binding and initialized
PLG044 control: fifteen renderer instances total.

Debug/Release builds and full suites pass **459 cases / 1,082,480 assertions**
each; focused module tests pass7 cases /2,116 assertions. Focused quality:
three existing findings, zero new; second generation stable, all eleven
copier regressions pass. Live Release `1792697927456019-0` and Debug
`1792698004510448-0` each cast Cherry Ronde in three encounters with two F10
re-entries. Read-only PDB-based sampling observes the actual tiled-ring age
advance1-39 and exact original colors throughout all entries. Every cast
pays55 HP and applies its actual reported damage, then recovers control.
Both traces end normally without errors. The first two new observer attempts
failed in PowerShell reporting, not in the native renderer.
Exact-revision Source Quality39842559519 passed; #122 is published and closed.
This still does not prove visual parity or explain the smoke paths; #76 and
broader #11 remain open.

The current all-art profile retains Fandango by design. Wider #11 acceptance
needs legitimate equipment setup for the remaining families, not category or
action-ID edits. The production path reads inventory blade/grip data in
`Items_LoadInventoryWeaponStats` and equips through `Items_EquipAshleyWeapon`;
Ashley spawning also derives the visible model from the equipped inventory.
The owner was unavailable for the selector-versus-profiles choice, so the
recommended Lab-only selector was implemented in the existing Ashley setup
submenu. It uses nine original ZND blade/grip records plus bare hands, preserves
stock capacity and campaign defaults, and applies equipment before profile
grants and normal spawning. JSON and frame logs distinguish requested choice
from actual category/parts/model. See COMBAT_LAB.md's weapon-family checkpoint.

Current scoped changes at base `5272b755` pass Debug/Release builds and 466 tests /
1,082,778 assertions each. All ten first-tier arts have positive controller
execution in both configurations, with original 25-HP cost and exact target HP
delta. Most use explicit starting AGI200; default-AGI bare hands missed twice
and then the replay lost a shortcut to enemy activity, so that run is excluded.
Greatsword/bare-hands live-enemy F10 re-entry and actual editor reset to Fandango
also pass. This is not completion of all forty arts or secondary effects.

#123 was logged before fixing F01856's uninitialized empty-slot material
arguments through the producer. Its regression failed before and twelve copier
cases pass after, with stable regeneration and owned-source preservation.
The checkpoint is published in `91f446c6`, exact-revision Source Quality
39851702872 passed every step, and #123 is closed. No baseline was changed.
Debug Dagger185/186/187 subsequently passed real controller execution in
`1792702041304012-0`, `1792702066934572-0`, `1792702088383033-0`, with costs40/55/75
and positive HP damage5/3/3. Release also passed in `1792702200811712-0`,
`1792702226323079-0`, `1792702252692436-0` (damage5/7/1). Debug Shadoweave185
did not apply Paralysis to the Zombie; positive secondary effects remain
pending. Do not close #11 on these partial results.

### Thunderwave #124 repair, published and closed

Thunderwave193 crashed with `0xC0000094` at tick851 in Debug
`1792702303877631-0`. Original PLG045 confirms that width reuse is intentional:
`DIV`/`MFHI` at `0x800FAA74`/`0x800FAA78` leaves the random dividend in HI for
a zero divisor. Do not split/re-clamp the width. F01867's maintained fix
preserves this remainder and one RNG call per sample. Shared template copying
now invokes the existing reviewed-fixup pass; the new producer regression
caught the missing hook. References were not modified.

Scoped changes on `91f446c6` pass Debug/Release builds, 15 effects cases /
13,381 assertions, full 467 cases /1,094,874 assertions, thirteen copier
regressions, stable second generation and owned-source preservation.
Final scoped tidy/format/lint/clones: zero new findings, 23 existing tooling
findings; no baseline changes. The native regression first failed with the
original divide-by-zero; it now checks all12 instances, sample-derived UVs,
RNG progression, packets/links and guards.

Real Thunderwave casts and live-enemy F10 re-entry pass twice per configuration:
Debug `1792703376728435-0` (damage2/4), Release `1792703466908989-0` (5/5),
each with cost40 and control recovery. Greatsword194/195 also pass:
Debug `1792703589953818-0` / `1792703614501626-0`, Release
`1792703739938947-0` / `1792703764325378-0`, costs55/75 and damage3 each.
All twelve new Dagger/Greatsword traces have final session.end, trace_failed:false
and no error-level records. Exact commands/evidence are in COMBAT_LAB.md.
The scoped fix is published in `8d66cf88`; exact-revision Source Quality
39856301304 passed every step and #124 is closed with the commit and evidence.
This mixed handoff and unrelated edits were kept out of that commit.
#11/#76/#82 remain open; continue the remaining arts and secondary effects.

### Iron Ripper #125, published and closed

Greataxe202 crashed on `8d66cf88` in run `1792704517072958-0`, tick907,
`Effects_EmitParticleEvent` case16 cone scaling by zero height. #125 was logged
before implementation. Original BATTLE.PRG DIV/MFLO at 0x800D34BC/0x800D34F8
returns -1 for both 0/0 quotients; zero height also skips the wide-range RNG
pair and gives sqrt(0). F01515's producer repair preserves (-1,0,-1), emission
and RNG, rather than clamping height or skipping particles.

The focused native regression failed before the fix and now covers all three
cone modes, heights0/32/-32, two seeds, final positions, allocation/list state
and RNG. Debug/Release builds, 16 effects cases/13,507 assertions, full468
cases/1,095,000 assertions, fourteen producer regressions, stable repeat
generation and owned-source preservation pass. Scoped quality: zero findings/
new findings; no baseline changes. Exact commands follow the #124 patterns;
evidence prefix is build/quality/iron-ripper-*.

Actual Iron Ripper twice plus live-enemy F10 re-entry passes in Debug
`1792705287321021-0` (damage5/3) and Release `1792705333411997-0` (1/3), cost55
and control recovery. The remaining higher Sword/Axe-Mace/Greataxe actions
also passed, bringing positive execution coverage to24/40 arts in both builds.
All16 additional run final records are audited clean. COMBAT_LAB.md contains
the complete table. The scoped fix is published in `54c62f9a`; exact-revision
Source Quality39860088352 passed every step and #125 is closed. This mixed
handoff/unrelated edits remain uncommitted.

### Further effects and Heaven's Scorn #126

Staff206/207 and Heavy Mace209-211 now pass both builds with audited final
traces. Separate all-break-arts Sackheim fixtures (zone9 room0 definition6,
HP150/MP15, Ashley4,9 and target4,11) demonstrate Killer Mantis198 HP49/MP13
damage in both builds. Riskbreak205 uses actual F3 to establish Risk25 and
reduces it to24 for damage3. Its source effect occurs four ticks after target
damage; the first harness check was premature, not a native defect.
Shadoweave185, Vile Scar189, Accursed Umbra201, Quickshock209 and Vertigo221
all newly apply their original status flags and timers on Sackheim in both
builds. COMBAT_LAB.md has every run ID and exact result.

Heaven's Scorn217 revealed #126 on54c62f9a: Debug1792707252049210-0 crashes
at tick928 in Audio_Mixer_UpdateChannelSlidesAndDelays. Read-only channel
observation in1792707507227388-0 confirms voice15's null auto-pan waveform
and counter0 before the original DF4801 depth slide (E164.P+0x173B), without
prior auto-pan enable. Original resident0x8001427C-0x80014318 dereferences
that pointer. The owned reset now initializes only unconfigured oscillators
to the original enable-command pending phase1; cached waveforms/phases stay
unchanged. No sound, depth slide or enabled modulation is skipped.

The six-mode regression fails before the fix and passes afterward, including
all44 channels, exact depth progression and enabled modulation, cached-state
preservation and repeated reset. Debug/Release builds, audio71 cases/162977
assertions and full469 cases/1095006 assertions pass. Scoped quality has0 new
findings,38 existing test findings; no baseline changes. No generator changes.
Actual art217 twice with live-enemy F10 re-entry passes in Debug
1792707904346821-0 (damage4/5) and Release1792708067805725-0 (8/5), with cost40,
control recovery and clean final traces. Published in07953575; exact-revision
Source Quality39865971299 passed every step and #126 is closed.

Current positive execution coverage:40/40 in BOTH builds; all accepted final
records audited clean. These latest runs are published in COMBAT_LAB.md:

| Art | Debug run / damage | Release run / damage |
|---|---|---|
|214|1792707195424468-0 /1|1792708623983606-0 /5|
|215|1792707221602121-0 /1|1792708649879492-0 /1|
|218|1792708513516595-0 /9|1792708680207524-0 /9|
|219|1792708540704239-0 /9|1792708707225182-0 /9|
|222|1792708569135588-0 /2|1792708735568615-0 /8|
|223|1792708596134006-0 /3|1792708762333533-0 /3|
|193 +Paralysis|1792708900706133-0 /54|1792708973838771-0 /55|
|197 +Numbness|1792708925480198-0 /47|1792708998430754-0 /48|
|213 +equipment-down|1792708950350244-0 /49|1792709023262919-0 /46|

The last three rows use the separate living fixture; original status19 is
mask0x80000 and timer slot2=15. The ignored RequireSecondary whitelist now
also covers193/197/213.

Armor-wear202/214 now passes in both builds, with all four final traces
audited clean. Iron Ripper: Debug1792709260707089-0 /
Release1792709350998616-0, damage54, limb3446->3230, shield5296->5188.
Giga Tempest: Debug1792709325935945-0 / Release1792709375332840-0,
damage48, limb3452->3260, shield5302->5206. The PDB-checked read-only observer
samples after ordinary equipment wear and before impact. Effect35's two
original application stages give 4*damage selected-limb / 2*damage shield
loss, saturating at zero; this is not a guessed 2*damage limb contract.

Two new maintained art-effect tests cover all nine two-hit arts through the
actual preview resolver (including canceled-target prerequisites), exact
per-hit arithmetic/aggregation and committed HP, plus both armor-wear stages
at saturation boundaries for all six limbs. Debug/Release full suites pass
471 cases /1,096,527 assertions; the two cases contribute1,521 assertions.
Scoped quality has0 new findings and1 existing test finding. No production
change or baseline weakening. Full details are in COMBAT_LAB.md.

The ignored driver now has ArtGate solo/range/cancel. Solo checks all four
arts' no-target confirmation, target cancellation and low-HP rejection
using actual F1 inputs. Natural field HP regeneration must be observed, not
assumed absent; the insufficient-HP replay is <=25, while exact cost
boundaries for all40 remain in maintained tests. Valid-target cancellation
allows normal enemy turns after leaving the menu. Initial timing/regen/
post-menu driver failures were harness assumptions, not native defects.
The all-family sweep is complete:60 encounters /400 gate selections, covering
all40 arts in BOTH configurations for empty-target confirmation/cancellation,
insufficient HP (all live checks at1), far-target rejection and valid-target
cancellation. Every final trace is clean and every result file contains the
required four action IDs per phase. Full matrix and host hashes are in
COMBAT_LAB.md; the ignored audited index is build/quality/art-gates-batch.json.
No background gate command or owned host remains. #11 closed against
b59a6a7a after exact-revision Source Quality39874136697 passed every step.
Preserve this mixed handoff and unrelated edits. #76 remains open; #82 is
already closed separately in GitHub, without new root-cause evidence here.

## Current ability checkpoint

#10 is closed after published verification and exact-revision CI. The Lab-only
`all-abilities` profile grants the original
14 offensive and 14 defensive abilities, maps the first three of each list,
and preserves inventory, equipment, resources, spells, Break Arts and unrelated
progression. The five code files and profile documentation are published in
`3cc999c3`; that initial step established enumeration/menu behavior only.
See [the bounded fixture evidence](COMBAT_LAB.md#ability-profile-acceptance-checkpoint-2026-10-22).

Debug `1792684051521630-0` and Release `1792684131131669-0` verify both 14-row
menus, scrolling, assign/move/unassign, F10/re-entry, and restoration of Prologue
grants/mappings/inventory. The earlier rejected shortcut was sent during room
transition 4; waiting for transition 0 fixes the diagnostic without a game-code
change. Both sessions finalize normally without error events. Both full suites
pass **420 cases / 1,060,980 assertions**; focused tests pass **45 / 1,861**.
Builds, focused quality and changed-header tidy pass without baseline changes.
Those fixture/menu checks alone do not establish effects, timing, costs,
acquisition restrictions or chain termination; later evidence follows.

The following published step adds cost/eligibility/timing regressions to
`Native\Tests\src\CombatTestCases.cpp`: all 28 base costs, chain-cost growth,
Risk cap 100, exact three-button timing boundaries, input priority and score
saturation. Final Debug/Release full suites pass **430 / 1,067,689**;
focused quality/tidy has **one existing finding, zero new**, no baseline
changes. Eight real-input runs now verify Heavy Shot's 70% damage formula,
one Risk per attempt, early/late rejection and cancellation in both builds.
Two legitimate charged misses precede Release's positive hit. See the
[timing/Risk evidence](COMBAT_LAB.md#ability-timing-and-risk-checkpoint).
These tests/docs are published in `2d231c81`.

Tests published in `e80a94a2` cover all 28 ability effect contracts, resource
capacity/drain clamps, the action-ID >=54 timing rule, all22 original ability
thresholds, all10 weapon/four-tier Break Art thresholds, priority and
non-mutating/capped acquisition gates. Both final builds/full suites pass
**448 cases / 1,074,484 assertions**; focused quality/tidy retains **one
existing finding, zero new**, without baseline changes.

Live Ward passes Debug `1792687747745849-0` and Release `1792687763174288-0`:
real enemy damage999->903, Paralysis cured before expiry, Risk+1 and original
defensive acquisition point+1. The lethal chain passes Debug
`1792688551760571-0` and Release `1792688605774947-0`: alternating Heavy Shot /
Gain Life kills the original121-HP Zombie with Heavy Shot, ordinary Square
take-all increases item323 from10 to12, and field control/recovery return
normally after actor removal. The earlier post-defeat timeout was an
unacknowledged normal loot menu, not a combat deadlock.
See [the effect and live-combat evidence](COMBAT_LAB.md#ability-effects-defense-and-defeat-checkpoint).

Acquisition now passes Debug `1792689722375744-0` and Release
`1792690148434786-0`. Twenty-one real Ward defenses demonstrate points0->20,
cap20 pending selection, unchanged learned actions and no premature unlock.
The profile's actual learned spell97 kills the ranged Minotaur Zombie with
1254 damage, spending25MP. Normal loot controls lead into the original reward
menu; cancelling/reselecting grants only Gain Magic25, reserves exactly one
unlock, preserves prior learned actions and unrelated counters, and returns
to normal combat. Failed diagnostic assumptions (melee proximity, spell99,
40-ms input shorter than a four-tick sampling interval) are retained separately.
No HP/progression/event flags are written by the driver. Exact story and save
acceptance remain separate under #17/#18. Publication and exact-revision
CI39823762991 are complete, and #10 is closed with this evidence.

## Current item-use and Combat Lab checkpoint

#8 is closed after scoped fixes, actual item-use acceptance and pushed CI.
Published [acceptance evidence](COMBAT_LAB.md#item-use-acceptance-checkpoint-2026-10-22)
separates those bounded criteria from #9, save/load and extra key/sigil fixtures.
The two all-items
Ashley profiles retain the 64-slot inventory, with 58 distinct stacks each.
Solo rooms and Lab-only resource/status controls are available. Startup
**ASHLEY STATS** overrides now support HP, MP, STR, INT and AGI, independently
defaultable and persisted in presets. HP/MP start full, overrides apply once
per encounter, and F8 refills current MP without altering its maximum.
See [COMBAT_LAB.md](COMBAT_LAB.md) for the controls and preset contract.

The item-use checkpoint's Debug and Release full suites each pass **416 cases / 1,060,702
assertions** after the spell-rendering and status-mechanics changes below.
Focused quality on all six affected C++ units passes with **zero findings**.
The complete changed-header gate passes with **328 existing findings, zero
new**. A final poison-test refinement additionally passes targeted quality;
it distinguishes current-HP damage from maximum-HP damage and separates the
last-HP boundary case. No baseline or suppression was changed.
Real scripted-editor replays verify all five overrides, depletion without
continuous reapplication, MP refill, F10/re-entry, and resetting to normal
250 HP / 50 MP / 100 base attributes. The new setup avoids trying to raise
maximum MP with consumables while the test enemy attacks.

With 999 starting HP/MP, Explosion's grimoire has now completed **four uses in
one encounter**, selecting actions 106-109, consuming 10 -> 6 copies,
retaining all four learned ranks and producing positive real enemy damage
at every rank. MP costs were 36/44/52/60, with F8 refills between uses.
Thunderburst also passed all four ranks in one encounter. The remaining five
ranked grimoires were tested by the owner. Broad automated item sweeps remain
paused at the owner's request; do not take over or restart an owner's Lab run.

The completed owner-driven Debug run `1792659407129820-0` has now been read
through its normal trace finalization: **462,588 consecutive events, two
encounters, 66 uses of 49 distinct items**, one copy consumed per use and
return to idle, no error-level events, `trace_failed=false`. All four Flame
Sphere, Gaea Strike, Avalanche, Radial Surge and Meteor ranks are accounted
for. Together with Explosion/Thunderburst, all **seven ranked books** have
four-use, same-instance evidence. Meteor IV consumed at 47631, damaged the
enemy 527 -> 518 at 48115, cost 62 MP and returned at 48447.
The combined consumption matrix now covers **84 of 84** named consumables/books.
The owner run lacked a chest, but corrected-facing Debug replay
`1792668279581682-0` used Clef (446) on the real MAP176 chest: lock2 -> 0,
two uses consumed10 -> 8, action92 learned, 3 MP per cast. Read-only process
observation verified the actual lock; no memory or progression flags were written.
Consumption and learning still do not establish every effect. Fireball,
Exorcism, Banish, Drain Heart, Drain Mind, Degenerate, Stun Cloud, Poison Mist
and Curse now have positive eligible-target evidence in both Debug and Release.
The drains transfer real resources; Degenerate/Curse change actual attributes;
the negative books apply their real conditions; Exorcism/Banish defeat eligible
enemies and return through the normal loot dialog. Original enemy class matters:
Mandel is Undead despite its human name; use Sackheim for positive drains.
Area books need height, horizontal-position and final-target confirmations.
The four previously unqualified cure books now
pass with real matching ailments in Debug and Release: paralysis, poison,
curse, and combined paralysis/poison/numbness are removed at their actual
effect frames. Blessing restores current STR/INT/AGI90 ->100; Clearance
restores walk/run4/9 ->9/18. Each consumes one copy, charges its original
MP cost and learns the spell.
Fixate now holds a genuinely moving MAP014 platform while its timer decreases
with ordinary room/platform updates unpaused, then lets it resume. Eureka reveals
the real MAP015 trap. Both pass in Debug/Release with original costs and learning.
Uncapped329/335 recovery matches exact result amounts and original95..105
variance;344 cures INT Down and separately AGI+Equipment Down while preserving
Poison. First/repeated Fireball retains only its own unlock, preserves intro and
Teleport gates and sets the real magic-used flag. Cancellation/zero-effect
consumption semantics are reconciled with the original commit boundary.
All eight final traces end normally without errors. Keys/sigils remain an
owner-requested follow-up, not evidence established by consuming the books.

All ten permanent-stat consumables previously passed actual base/max-stat
comparisons in Debug and Release, using normal starting stats below the cap.
Earlier recovery/Risk/status runs have now been reconciled against the exact
effect frames, separating item effects from passive changes. The detailed
checkpoint records the measured amounts, caps, condition removal and remaining
eligibility/uncapped-recovery gaps. The owner's item 336 uses additionally
verify full HP/MP restoration to the larger 999 setup maxima.
Published repairs address the native-width shortcut allocation (#32) and scoped
effect-plugin binding (#84). Both issues are closed after acceptance and
exact-revision CI. F10/re-entry #82 and accelerated-audio
queue exhaustion #83 remain open. Use speed 1.

Detailed commands and evidence are in the
[item checkpoint](HANDOFF_HISTORY.md#combat-lab-item-profiles-and-starting-stat-overrides-local-2026-10-22).
#8's bounded acceptance is complete. Native save persistence, comprehensive
learned-spell fidelity and compatibility retirement are not claimed. The optional
final combined .cmd test filter selected no tests, and a direct retry could not
obtain tool approval; neither is counted as a pass. The completed full suites
and real replays cover the unchanged native code. See the
[final item checkpoint](HANDOFF_HISTORY.md#environmental-recovery-and-progression-acceptance-2026-10-22)
for exact evidence, fixture corrections and remaining limits.
The owner explicitly resumed autonomous work. The focused selection now passes
in Debug and Release: 55 cases / 2,098 assertions each, using the native
executable with separate stdout/stderr redirection and checked process exit.
Expected warnings in negative tests must not be mistaken for failed assertions
by PowerShell's native stderr handling. #9 is now closed after18 accepted
Debug/Release real-input sessions and exact-revision CI. See the
[learned-spell checkpoint](COMBAT_LAB.md#learned-spell-acceptance-checkpoint-2026-10-22)
for exact costs, effect boundaries, all four Flame Sphere ranks, five negative
cases and the64 direct-entry IDs not individually exercised. Full #17
story-boundary verification and #76 particle fidelity remain open.

Next ordered #10 investigation identifies14 named offensive and14 defensive
abilities in the original action definitions. Untitled entries33,36,38,39 are
not additional offensive abilities. Both production unlock-list consumers
iterate14 entries; do not infer the count from their padded16-byte arrays.

### Ghost loading and current validation

#118 was a fresh-start Ghost crash, not the reported F10 fault. Original
`D_800F188C` aliases `_zoneContext+0xC`; F00176 now reads the live enemy table
for the secondary model, retaining the same ZUD index and model+1. Debug and
Release load models26/27, use Banish to change Ghost HP67 ->0, consume10 ->9,
charge25MP, learn action105 and return normally. Three live-Ghost F10 stops
and fresh entries also pass in each configuration. All four traces finalize
without errors. The independent alias declaration remains compatibility debt.

After this repair, both builds and full native suites pass416 cases /
1,060,702 assertions each; all23 copier tests pass. Two generations retain751
stable generated files and86 unchanged owned files. Focused generator quality
has zero findings; the pushed full gate has zero new findings. No baseline
changed. Existing generated actor-view pointer warnings remain.
Commands, run IDs, fixture corrections and remaining limits are in the
[positive-book/Ghost checkpoint](HANDOFF_HISTORY.md#positive-book-effects-caster-identity-and-ghost-loading-2026-10-22).

## Spell rendering and status follow-up

- **All-spells Ashley** grants 50 battle spells / 71 learned actions, including
  every rank, without changing normal inventory/equipment/resources or story
  gates. Use the existing startup overrides and F8 for MP.
- **#86:** the blank rank menu was independently reproduced. F02604's final
  packet link still truncated the pointer to 24 bits; the registered copier
  fix now uses `LegacyGpu_Handle`. Debug/Release real-input replays select
  Flame Sphere actions 114-117 and charge 36/44/52/60 MP. Cancellation,
  re-entry and returning to rank I are logged. Two actual book uses also
  produce a readable LV1/LV2 selector with locked `--` placeholders.
- **#85:** F01309 was an unported stub. Its owned C++ implementation follows
  original `5A624.s` terrain, wall/corner clipping and scanline rendering,
  using native interval storage and packet links. Before/after captures
  show the missing and restored green height plane; ordinary Up inputs
  change the selected elevation. Regression coverage includes guards,
  native links, clipping, stepped terrain and 32 camera yaw angles.
  GTE/packet compatibility remains; no frame-aligned reference-pixel parity
  or EC retirement is claimed.
- Ten production-mechanics tests verify real stat changes/restoration,
  Curse policy, Numbness/Quicken speeds and AI steps, silence/paralysis action
  restrictions, poison/regeneration cadence and HP changes, equipment
  reduction and immunity. These are mechanics-unit evidence, not a claim
  that every cure or offensive grimoire has passed live acceptance.
- **#115:** the former Lab SLOW entry was actually the injured-leg flag,
  immediately cleared for healthy legs. It was removed from F4; Numbness
  remains the timed half-speed ailment. Main-game injury rules are unchanged.
- **Keys/sigils/Clef:** correction to the earlier blocker: event isolation
  removes door/script events, not ordinary `lockTriggers` or `containers`.
  Clef now unlocks the real chest through production gameplay. Keys/sigils
  still need matching-lock replays; no broad event-policy change is justified.
- **Enemy Status weapon (#116):** the missing Minotaur-Zombie axe was
  reproduced. Original `D_800F4590` aliases weapon pointer slots2/3, but the
  generator had created independent one-element storage. F02465/F02637
  now address canonical weapon visibility/fade fields. Debug shows Ashley's
  blade; Release `1792668067114499-0` shows the enemy axe after switching,
  cancellation and re-entry, with no error events. Release
  `1792668480343311-0` also shows the Skeleton's weapon. Debug
  `1792669671662902-0` and Release `1792669671662303-0` show source-equipped
  Sackheim holding both sword and shield. Normal Fandango unequip/re-equip
  replays also verify model removal and restoration in both builds.
  Both full suites still pass
  416 cases / 1,060,702 assertions; 21 copier tests and generator quality pass.
- **F10/re-entry (#82):** additional Debug/Release five-encounter checks cover
  living-enemy action-phase7 interruption, pause/single-step, profile A -> B
  and fresh re-entry. All pass without reproducing the reported fault.
  This is not a root-cause fix: keep #82 open for a failing configuration,
  action phase and crash stack rather than weakening its acceptance criteria.

The bounded rendering/fixture defects #85/#86/#115/#116 are now closed after
acceptance, scoped publication and exact-revision CI. Commands, run IDs and limits are in the
[follow-up checkpoint](HANDOFF_HISTORY.md#spell-rendering-and-status-mechanics-local-2026-10-22).

Six-issue publication checks: both Debug/Release x64 MSBuild invocations pass;
`test-native.cmd Debug` and `test-native.cmd Release` each pass 416 cases /
1,060,702 assertions; the copier suite passes 21 tests. `quality-native.cmd`
reports 195 existing findings and zero new findings, with no baseline changes.
Logs: `build\quality\item-closeout-{debug,release}-{build,tests}.log` and
`build\quality\item-closeout-quality.log`. Committed native source, generators,
project files and affected ledgers exactly match the validated worktree.

The three requested reference trees were reviewed read-only for particles and
trails. Latest upstream PLG057 work is already represented by native templates,
including separate five-/nine-sample state. Rood-boi's newer whole-request
initialization fixes unused-byte determinism; it is not a demonstrated cure
for scattered particles. No blanket reference import or #76 closure is justified.
The post-#11 read-only refresh confirms ser-pounce/rood-reverse remains at
`5163208b`, while the configured JEschete fork remains at `e0875ad8`.
The verified-functions remote remains `94a9e90d`, ahead of local `39821e4f`
by the same PLG057 merge. No checkout was fetched, switched or altered.
F01930-F01948 already have native instances; G00645-G00647 retain distinct
particle/nine-/five-sample counters. There is no newer shared trajectory
implementation in these two upstreams.

Rood-boi's remote advanced three commits beyond the untouched local
`b2c3fede`, to `fdbb5e410351f91959a4a795275420e3016ac777`:
menu labels/menu-camera controls, room-entry audio/credits wipe, and a
cutscene replay/step/scrub/capture tool. The44-file comparison contains no
new particle/trail integration algorithm. The replay tool records controller,
camera and scanline-budget input alongside snapshots of guest RAM, scratch,
I/O, overlays, native statics, stack, SPU and VRAM. It is useful reference
workflow research, not a native drop-in subsystem or a tool run performed here.
Its catalogue explicitly limits story-flag/event/door/movie coverage.

The upstream room-entry sound repair `f2920efc` addresses the same
never-enabled-LFO depth-slide family as #126 (zones15/0 and48/3). It substitutes
an emulated RAM-base pointer for a null wave. Do not import that hardware
bridge into the native mixer: `07953575` establishes the pending phase at
initialization instead, with dedicated slide/enable tests. Those two room
entries have not been separately replayed here, and neither audio fix proves
particle placement parity.
Fresh findings are recorded in
[#76](https://github.com/JEschete/VS-NativePort/issues/76#issuecomment-6347104530).
Revisions, concrete differences and next observation are in the
[closeout/reference checkpoint](HANDOFF_HISTORY.md#item-follow-up-closeout-and-reference-review-local-2026-10-22).

## Latest audio checkpoint

The [menu/stream acceptance checkpoint](HANDOFF_HISTORY.md#native-audio-menu-streams-and-output-acceptance-local-2026-10-21)
restores all five in-game menu sound wrappers, imports all **113 effect streams**,
and connects their native voice lifecycle. Ending XA imports to a **443.52-second**
FLAC and plays through an independent SDL source. Native-sized music snapshots
and saved-voice conflict handling replace incomplete original-layout assumptions.
The producer/cleanup audit is in [ASSET_PIPELINE.md](ASSET_PIPELINE.md#audio-producer-and-cleanup-audit-7).
The prior 83-bank/noise/FM/reverb implementation remains in place.

Final Debug/Release builds and full tests each pass **379 cases /
991,535 assertions**. Release `[audio],[clock]` passes **72 cases / 163,008
assertions**. The complete changed-header quality gate passes: **313 existing
findings, zero new**, with no baseline/suppression changes. Before the final
mixer repair, two source/project generations retained 966 identical files and
two effect/XA imports retained 250 identical files; all 17 port-generator tests passed.

Final real-input Release replays exercise menu open/change/select/leave/invalid,
the opening scene without skipping the prologue, and Solid Shock's E044 stream.
The latter binds at tick 515, starts both voices at 607, stops at 691, then reuses
those voices for menu audio at 1143. Only one cast occurred; do not claim two.
Its earlier float capture reached **+6.774167 dBFS**. A failing coherent-voice
regression exposed the omitted final signed-PCM boundary. Dry+wet output now
saturates to [-1, 32767/32768] after Mono fold-down, preserving the original
stream pair gains and quiet additive mixes. The isolated final combat capture
stays within those bounds with no NaNs/Infs. Saturation is not proof of perceptual
parity or removal of intentional overload distortion.

Post-commit Debug/Release builds and full suites pass the same totals above.
Fresh Combat Lab run `1792633974201305-0` records the fixing revision and
unrelated dirty state, repeats the E044 lifecycle above, and produces bounded
PCM without NaNs/Infs or shortages after six pre-attachment requests.
Current evidence: `build\quality\audio-published-*`, `audio-output-*`; audition WAVs:
`build\quality\audio-listen-*`. Menu/opening native/reference excerpts use matched
scene states rather than equal wall-clock offsets. Reference recordings are AAC,
not lossless. No subjective owner-listening approval is claimed. Full credits
navigation/listening, every effect stream, all-content parity and 5.1 are not
verified. Primary PCM capture excludes independent movie/XA sources and physical
device gaps. Gameplay stalls can still delay new events.

The installed ending FLAC now also passes a production disc-queue/SDL-postmix
regression in both configurations: audible stereo at the start and after a
30-second sector seek, stop/re-entry, then natural EOF/drain from the last
50 sectors. This is real-asset audio-path coverage, not full credits navigation.
The test skips explicitly when the converted asset is absent. Latest test logs:
`build\quality\audio-ending-*`.

**#7 is closed; EC-02 remains open.** Publication used the original authorization
for this ongoing task; the earlier request for a second approval was unnecessary.
The issue records exact fixing-commit CI, real controller/postmix evidence and
all unsupported limits. Closing functional output does not establish subjective
parity or compatibility retirement. No custom comparison tool was added.

## Remaining limits

- Actual Options consumers remain under #39; menu visibility is not evidence
  of combat/audio/puzzle behavior.
- Open owner findings include Score submenu crashes (#65), particle placement
  (#76), eight-Minotaur-Zombie termination (#78), suspected multi-part targeting
  (#80), selected-body-part glow (#81), equipment behavior (#14) and broader
  Combat Lab coverage (#16). Their issues own acceptance and next observations.
- Hellhound breath #75, Bat terrain #77 and post-defeat restart #79 are closed;
  their historical open/local-only wording is superseded by issue evidence.
- Full-content/reference parity, native save/load and compatibility retirement
  are not established by the completed bounded gameplay fixes.

## Sources of truth and separate gates

| Question | Authoritative record |
| --- | --- |
| What is next? | #4 and its existing child issues |
| Architecture and owner decisions? | [Native source port plan](NATIVE_SOURCE_PORT_PLAN.md) |
| Who maintains a migrated implementation? | [Ownership registry](../tools/port/owned.csv); [copy manifest](../tools/port/copy_manifest.json) protects generated output |
| What do the ledgers currently record? | [Reconciled summary](ledgers/SUMMARY.md), with raw status limitations stated explicitly |
| Compilation and source quality? | Revision/configuration-specific build and quality results; [QUALITY.md](QUALITY.md) |
| Which behavior was exercised? | Issue acceptance evidence and [historical checkpoints](HANDOFF_HISTORY.md), including exact inputs and limits |
| Is a compatibility bridge retired? | [EC audit](EMULATION_COMPATIBILITY_AUDIT.md) and #18; not ownership, a filename change or a passing build |

## Local working state

Use the retained `.venv-3` environment; see [BUILDING.md](BUILDING.md#12-only-for-working-on-the-port-itself-not-needed-to-play).
`resources\rood-reverse-noprompt` is an ignored, local-only reference checkout,
not a parent-repository submodule. Its files, own Git history and edits are retained.

Existing build outputs, captures, logs, presets and extracted/user-owned data
remain in place. Follow the [retention rules](BUILDING.md#46-local-state-and-evidence-retention):
rebuildable objects are not the same as replay evidence or crash symbols.
