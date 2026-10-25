# Current handoff

Updated 2026-10-24. This is a short entry point, not another work tracker.
Detailed checkpoint evidence, commands and replay paths are preserved in
[HANDOFF_HISTORY.md](HANDOFF_HISTORY.md). Historical results are not fresh
verification of the current worktree.

## Work order and current state

### Latest source and next observation

- #179 new interrupted-scene reentry defect is confirmed and logged before
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
  Fresh repeated same-window chains plus ordinary-mode controls are running;
  #179/#174 remain open pending live gates and exact fixing-revision CI.
  The controller now holds Circle until production native-buttons acknowledge
  it, then releases;40ms pulses were often lost between slow Debug host polls.
  Inputs remain original dialogue actions, never Start or state mutation.
- #149/#174 source-aligned component playback is now opt-in via an isolated,
  empty-enemy preset's `external_event`. Initial entry is isolated; after the
  direct event starts, original actor/room-event handoffs resume. It does not
  assign story flags or simulate a boss defeat. Release1792893708848660-0
  plays80 ->82 ->81 with original SEP26/WAVE99 setup and real dialogue Circle
  input, no Start/forced skip. Parent2 removal produces72 deferred prop7/bone2
  draws at8573..8715, not the reported0xB0 access. Original return reaches
  zone12/room0, phase0/control1 at14468 and stays settled through24000, with
  zero engine errors. Speed8 audio scheduling warnings are not fidelity proof.
  Six component/compatibility tests bring full serial Debug/Release suites to
  722 cases/3,164,568 assertions each; fast quality remains195 existing/0 new.
  Changed production-source tidy is zero. Evidence is based on d9f4a876 plus
  this scoped component work; unrelated ignore/solution edits are preserved.
  Local `build/quality/family174-through81-Release.*` and `scene149-*` retain
  commands/results. Single-window reset/reentry and Debug full-chain acceptance
  are still in progress; #174 remains open until those and exact-revision CI.
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
- #175/#177 reference audit limits: box attacks are conditional, not a direct
  beeline. Original73460.s requires a nearby hostile actor at tile squared
  distance below3, attacker box range and allowed action-delay state before
  candidates compete; a controlled live rare-box selection is pending.
  Owner's roulette loads MENUF at11609. Native reel initialization, three
  weighted16-entry tables, acceleration/deceleration and stop selection match
  original MENUF source; BIOS Rand15 golden controls and owner raw RNG advance.
  /J keeps the selected byte unsigned. No simple RNG defect is established,
  and no guessed uniform reroll was introduced. Exact stop/update cadence and
  award application still need aligned live observation. Reference
  `rood-boi` movement file is unchanged from the pinned counterpart; the
  `rood-reverseWin` snapshot is not an independent Git root, so a git log there
  must not be mistaken for upstream changes.
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
  event82's attachment handoff and is **not** full-scene acceptance. #174 stays
  open for a source-aligned external82 replay/owner confirmation. No HP,
  progression flags, fake event completion or math null fallback was used.
  Original scene82's three text entries are control-only strings, despite
  executing dialogue commands; their zero-glyph flashes are repaired by #176.
  Evidence: `build/quality/owner-reports-20261024-193314/`,
  `family174-*` and the retained unique owner run.
  Publishedb8e2e782908526abe79e38fd66ab816dec01475f has successful exact
  Source Quality40146187813; #174 remains open for its playback gate.
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
