# Handoff: prologue entry and shared rendering

## Crash and transition checkpoint (2026-10-24)

### Latest source and next observation

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
- Current autonomous crash work is local/unpublished on `508b4bd7` plus the
  preserved dirty worktree. #150's original Bat/gimmick endpoint fix and shared
  Desktop logging remain local; #151 now inventories all **1,001 directed door
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

## Native audio functional acceptance (2026-10-21)

This revision implements the bounded functional scope of
[#7](https://github.com/JEschete/VS-NativePort/issues/7). The existing AKAO
sequencer drives sfizz/SFZ/WAV and SDL without a competing sequencer or synth.
All 83 nonempty banks and 113 effect streams import; noise, preceding-voice FM,
ten reverb presets, Stereo/Mono and ending XA playback are connected.
All five in-game menu wrappers dispatch their original effects. Native-sized
music state and reserved stream voice lifecycle replace incomplete bridges.

Debug and Release full suites each pass **379 cases / 991,535 assertions**.
Release `[audio],[clock]` passes **72 cases / 163,008 assertions**. Full
changed-header quality reports **313 existing findings, zero new** without
baseline/suppression changes. Source/project and import repeatability checks
are recorded in [ASSET_PIPELINE.md](ASSET_PIPELINE.md).

Actual controller replays cover simultaneous music/SFX, scene/menu transitions,
all five menu sound actions, Stereo/Mono, skip/re-entry and Solid Shock. Isolated
Combat Lab run `1792631613651010-0` binds E044 at tick 515, starts its pair at
607, stops at 691 and reuses the voices at 1143. The installed ending FLAC
passes production SDL postmix at start/30-second seek and natural EOF/drain.
The final mix restores the original signed-PCM saturation boundary without
changing source stream gains; this is not normalization or a claim of no
intentional overload distortion.

Local evidence: `build\quality\audio-output-*`, `audio-ending-*`, and paired
`audio-listen-*.wav` menu/opening auditions. The reference windows are matched
by scene/action state, not sample-synchronized; recordings are AAC-derived.
No subjective parity approval is claimed. Disc-derived captures/data remain
local. Full credits navigation/listening, every stream/track in context,
bit-exact DSP and 5.1 are outside this bounded acceptance.

Publication, exact fixing revision, CI and closure evidence belong to #7.
EC-02 retirement remains separate. After #7, the next ordered priority in
[#4](https://github.com/JEschete/VS-NativePort/issues/4) is item use #8.
Unrelated local documentation/reference cleanup is not part of this audio
publication. The sections below retain earlier checkpoint evidence, not
current implementation or priority claims.

## Historical checkpoint

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
