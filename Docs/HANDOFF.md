# Current Handoff

Updated 2026-11-17. The latest integrated #276 group is status components,
F02647-F02654, including production module entries, description formatting,
selection predicates and empty-accessory refusal (#418).
Its shared Debug/Release gates passed;
the publication and exact-revision CI checkpoint is recorded on [#276](https://github.com/JEschete/VS-NativePort/issues/276).
The original 900-function cohort has **839 resolved (827 promoted,
12 retired) and 61 remaining**; see
[REMAINING_FUNCTIONS.csv](ledgers/REMAINING_FUNCTIONS.csv) and the
[promotion plan](ledgers/PROMOTION_PLAN.csv).

## Current status and next steps

- Remaining by plan row: 30: 1 (F00830, blocked by #296), 39: 0, 47: 0,
  48: 20, 50: 30, 52: 10. F02647-F02654 now meet the no-bridge gate;
  no WIP function is counted as a completed promotion.
- Process: one coordinator and at most two isolated workers. Each worker uses
  `D:\Dev\NativePort\wt\batch-<name>-<base8>` with a read-only `Native\data`
  junction to the root data and delivers one local commit (typed cores, Live
  entries, focused tests, per-function differentials with mutants). The
  coordinator owns registration (`promote.py`), copier output, caller switches,
  golden digests for Live tests whose generated bodies are retired, ledgers,
  the group gate, the commit and push, and exact-revision CI.
- Status-components evidence (`VR-MENU-STATUS-COMPONENTS`): eight original
  spans pass 8,192 vectors, zero mismatches; 65 instruction mutants and eight
  compiled native variants are detected. Debug and Release each pass 8,993
  cases / 12,956,714 assertions, 26 focused cases / 307 assertions and both
  canonical replays. `group-20261117-005949` passes in 454 seconds;
  second generation changes zero Native files (`127b8e79be106b34`),
  copier tests pass and tidy reports four existing findings, zero new.
  Final ledger sync/check passes. Evidence:
  `build\scratch\status-components\integration.json`.
  All eight actual module entries, real formatting/conversion, complete
  96-byte gem copy, stat callbacks and current-page selection are covered.
  Empty accessory refusal preserves buffer/context/rows (#418); occupied
  accessories use the actual conversion and inventory row formatter.
  A completed allocated MENU4 request preserves borrowed text and rows.
  These functions introduce no new reset or allocation owner.
  Original external display/stat/row boundaries are doubled; no interactive
  gameplay, empty-accessory reachability or EC retirement is claimed.
  Publication and exact-revision CI remain required for #418 closure.
- Production reload dispatch (#423): a completed, pool-owned MENU2 request
  passed through `LoadLiveMenuOverlay` without calling its reset hook.
  `iterate-20261116-235431` fails two of six assertions: learned action 24
  and the shared row borrow survive. Direct reset-hook tests had not exercised
  this producer. The magic integration adds loaded-program dispatch, including MAINMENU shortcut
  redirection, plus MENU0 school/teleport cold and ordinary resets. Its real
  module/renderer and production completion controls pass 285 assertions in
  12 cases (`iterate-20261117-001709`) and the full group below. Exact-revision
  CI now passes; #415/#417/#423 are closed with production-boundary evidence.
- Magic-terminal evidence (`VR-MENU-MAGIC-TERMINAL`): all five original
  instruction spans pass 1,024 vectors each, zero mismatches; 153 original
  instruction mutants and five compiled native variants are detected.
  Debug and Release each pass 8,967 cases / 12,956,407 assertions,
  97 focused cases / 1,189 assertions and both canonical replays.
  `group-20261117-002709` passes in 526 seconds; second generation changes
  zero Native files (`028818a45cf53945`), copier tests pass and tidy reports
  19 existing findings with zero new findings.
  Evidence: `build\scratch\magic-terminal\integration.json`.
  The five original module entries and framework pointer-byte dispatch use
  native Live adapters. Bounded populated rows address #373; complete
  normalized original rank chains and actual Live glyph/arrow linkage cover
  #413. MENU0 completion and actual cold-session invalidation reset all four
  school owners plus teleport (#414), preserving external progression and
  cursor state. The old rank-data representations G00983/G00994 and legacy
  header are retired, separately from function promotion.
  #423 controls cover pending/completed/already-loaded MENU2, independent
  MENU3/MENUB owners, loot workspace preservation and redirected MAINMENU
  program identity. Requests are allocated from the real queue; controlled
  completion is not an interactive disc-read replay. Original external
  endpoints remain doubled; no gameplay completion or EC retirement claim.
  Published in `52630eacd0e5bd027ec3ec68649e397b20066857`.
  Exact-revision [Source Quality 44026652046](https://github.com/JEschete/VS-NativePort/actions/runs/44026652046)
  passes on the unchanged-revision retry. Its initial header-positive-control
  timeout is #424: profiling measured 51.675 seconds in repeated full-repository
  clone scans. Bounded fixture discovery and isolated reports preserve real
  tools, header findings and the unchanged production scope; both controls
  now pass in 1.262 seconds and detect the runtime scope-alias mutant.
  The scoped fix `6b44ee153961832683cac9d96fe8267f0dce2ae0` passes
  [Source Quality 44028951991](https://github.com/JEschete/VS-NativePort/actions/runs/44028951991).
  #373/#413/#415/#417/#423/#424 are closed. #414 remains open for an explicit
  blink-reset observation; no private-blink assertion is claimed.
- Ability-terminal evidence (`VR-MENU-ABILITY-TERMINAL`): six originals and
  the supplemental actual-Live assignment entry each passed 1,024 vectors,
  zero mismatches; 184 distinct primary instruction mutants, 60 repeated
  through the Live comparison and all seven compiled native variants were
  detected. Debug and Release each passed 8,926 cases / 12,955,612
  assertions, 70 focused cases / 3,217 assertions and both canonical replays.
  Second generation changed zero Native files (`28dd19a2ad012f41`);
  copier tests and changed-file tidy passed with five existing findings
  and zero new findings. Group duration: 448 seconds.
  Evidence: `build\promotion-check\group-20261116-232059\summary.json`
  and `build\scratch\abilities-terminal\integration.json`.
  The six module entries and the framework's pointer-byte dispatch now
  use the native Live adapters. Direct MENU2 reset-hook tests failed in both learned
  lists before the new hook (`iterate-20261116-231418`) and pass
  afterward. Learned arrays, cursor/list phases and animation state reset
  according to the original image, separately from mappings/progression
  and row ownership. The full Boot hook clears the shared row borrow,
  not its heap allocation; terminal-only and actual cold-session resets
  preserve the external owner. Real screen/category construction and
  reentry pass. The retired generated row-info macro's original packed
  values are retained in both dependent Live adapters.
  Metadata listing also passes with Capstone and pefile explicitly blocked.
  Original external services are doubled; actual production module, heap,
  framework and lifecycle tests independently check integration.
  No interactive presentation, gameplay completion or EC retirement claim.
  Implementation [`a64b1bff9`](https://github.com/JEschete/VS-NativePort/commit/a64b1bff9ad0355d08e7aaf9b05b3425740402ed)
  required a documentation-only follow-up
  [`a3fd7632b`](https://github.com/JEschete/VS-NativePort/commit/a3fd7632b67991637942652817325dfbb192dda6)
  for the synchronized modernization inventory; complete-checkpoint
  [Source Quality 44018201391](https://github.com/JEschete/VS-NativePort/actions/runs/44018201391)
  passed. The parent CI failure was precisely `tracker-stale`, not a native
  implementation failure. Ordinary production reload dispatch is separately
  tracked by #423 above.
- Loot-terminal evidence (`VR-MENU-LOOT-TERMINAL`): five originals each
  passed 1,024 vectors, zero mismatches; all 56 original instruction mutants
  and five compiled native variants were detected. Fresh verification of all
  eight inventory contexts adds 8,192 vectors, zero mismatches and 92
  detected original mutants; the unchanged inventory core retains its prior
  native-variant evidence. Debug and Release each passed 8,889 cases /
  12,955,080 assertions, 79 focused cases / 688,511 assertions and both
  canonical replays. Second generation changed zero Native files
  (`e828be46e17aa505`); copier tests and changed-file tidy passed with six
  existing findings and zero new findings.
  Evidence: `build\promotion-check\group-20261116-224554\summary.json`
  and `build\scratch\loot-terminal\integration.json`.
  #412's original five-unit preview manufactured 6,320 units when Ashley
  had room for 20. The native correction caps each stack by remaining loot;
  typed boundary tests and actual Live preview/confirm/cancel tests verify
  conservation and complete inventory/176-row rollback. That defective
  input domain is excluded explicitly from original parity.
  #417's direct MENU3/MENUB navigation reset-hook tests failed before the
  correction (`iterate-20261116-212739`) and pass afterward. MAINMENU
  shortcut, MENU3 countdown and MENUB inventory/loot reset hooks restore
  their own image-backed controllers. Immutable MENUB text and the owned workspace
  survive ordinary reload; actual cold session invalidation resets paging,
  clears borrowed views/substitutions and releases storage before heap
  replacement. Real teardown and subsequent reentry pass.
  The existing paging fixture now binds both scene and overlay ordering
  tables; the focused run has no unexpected renderer diagnostics.
  Original callee endpoints are doubled; native pointer layout, expanded
  capacity and lifetime are checked separately. No interactive presentation,
  gameplay completion or EC retirement claim. Published in
  [`3599873728`](https://github.com/JEschete/VS-NativePort/commit/3599873728c03510dc884ff74ddd5f8932f5c076);
  exact-revision [Source Quality 44014438394](https://github.com/JEschete/VS-NativePort/actions/runs/44014438394)
  passed. #412 is closed; #417's later production-dispatch acceptance is
  completed by the magic checkpoint above;
  [acceptance checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6938117160).
- Ability-foundation evidence (`VR-MENU-ABILITY-FOUNDATION`): six originals
  and the supplemental actual-Live row entry independently passed 1,024
  vectors each, zero mismatches; 154 distinct original instruction mutants
  (the row proof repeats 32) and seven compiled native variants were detected.
  Debug and Release each passed 8,859 cases / 12,954,776 assertions,
  41 focused cases / 6,201 assertions and both canonical replays.
  Generation stability, copier tests and changed-file tidy passed with zero
  new findings. Evidence: `build\promotion-check\group-20261116-205439\summary.json`
  and `build\scratch\abilities-foundation\integration.json`.
  The first group failed one new Live tidy literal; proper-size array extents
  fixed it, followed by fresh Live proof, focused tests and the full group.
  Seven stale adaptations were pruned for #378. F02619's module/caller ABI
  uses the pointer-array adapter, with bounded action-name staging and a full
  16-byte unsigned Risk header. Actual Live tests cover module entries,
  row allocation/copying and two failing-before/passing-after MENU2 reset
  regressions (#415). The original image physically includes all foundation
  state through `0x8010511F`: uninitialized C declarations are not evidence
  of persistent BSS. Existing row owners retain cleanup responsibility.
  Original external rendering/allocation/dispatch/BCD endpoints are doubled;
  the supplemental Live proof doubles framework/heap/logging, with actual
  integration tested separately. No interactive presentation or EC retirement
  claim. Published in
  [`221343948`](https://github.com/JEschete/VS-NativePort/commit/221343948342a84512b1e318838a51ffcc11a61c);
  exact-revision [Source Quality 44001392919](https://github.com/JEschete/VS-NativePort/actions/runs/44001392919)
  passed. #378 is closed; #415's later production-dispatch acceptance is
  completed by the magic checkpoint above;
  [tracker checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6936114927).
- Inventory-terminal evidence (`VR-MENU-INVENTORY-TERMINAL`): eight original
  contexts independently passed 1,024 vectors in current root, zero mismatches;
  all 92 original instruction mutants and eight compiled native variants were
  detected. Debug and Release each passed 8,826 cases / 12,952,091 assertions,
  33 focused cases / 657 assertions and both canonical replays.
  Generation stability, copier tests and changed-file tidy passed with zero
  new findings. Evidence: `build\promotion-check\group-20261116-201034\summary.json`
  and `build\scratch\inventory-terminal\integration.json`.
  Live tests exercise all eight module entries, real row creation, separate
  MENU3/MENUB text and Manage/restriction behavior, and original MENUB text
  loading after cold reset. Loot callers now use
  MENUB navigation and native-owned MENUB text (#405); row 47 is complete,
  98/98 promoted. Original callees are endpoint doubles; renderer packets and
  unused workspace bytes are excluded. No interactive/visual gameplay or EC
  retirement is claimed.
  Published in [`1275c6a72b`](https://github.com/JEschete/VS-NativePort/commit/1275c6a72bba310a954bfddf8bbd4201eca1d784).
  Its exact-revision [Source Quality 43995551631](https://github.com/JEschete/VS-NativePort/actions/runs/43995551631)
  exposed #416: metadata-only differential listing eagerly imported optional
  Capstone, absent on CI. The correction defers that import until proof
  disassembly and makes the listing regression block Capstone explicitly;
  original checks are unchanged. The correction
  [`0c79a09a4`](https://github.com/JEschete/VS-NativePort/commit/0c79a09a4f5a399ba55db18b4296de1f12cf493b)
  passed exact-revision [Source Quality 43996700466](https://github.com/JEschete/VS-NativePort/actions/runs/43996700466);
  #405 and #416 are closed; [tracker checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6935470143).
  Subsequent physical-image evidence invalidated the inventory packet's
  persistent-BSS assumption and its corresponding reload test. The loot
  group above supplies fresh #417 evidence and supersedes that assertion,
  including the corrected F02592 promotion note and verification references.
  The original inventory gate slice retains its historical bridge counts;
  historical test totals are not the fresh reload verification.
- Emitter/ring evidence (`VR-EFFECTS-EMITTER-RINGS`): all eight original
  spans independently passed 2,048 vectors in current root, zero mismatches;
  79 original instruction mutants and eight compiled native variants were
  detected. The seven original ring comparisons observed 148,176 complete
  submitted quads. Independent module identities and all ten corresponding
  EFFPURGE entries have checked create/tick/expiry/refusal/owner-cleanup tests.
  Debug and Release each passed 8,805 cases / 12,951,975 assertions,
  60 focused cases / 1,306 assertions and both canonical replays.
  `build\promotion-check\group-20261116-190031\summary.json` retains its
  initial copier-test failure: old assertions referred to retired fixups,
  and ring-case metadata read disc images during listing. After those
  Python-only fixes, all 226 encounter-fixup and 69 differential-tool tests
  passed with zero new quality findings; unchanged native builds/replays
  were not duplicated. Generation stability, changed-file tidy and final
  ledger checks passed. Recovery and proof paths:
  `build\scratch\emitter-rings\integration.json` and
  `D:\Dev\NativePort\wt\batch-emitter-rings-69396c5e\build\scratch\emitter-rings\verification.json`.
  Original allocation, texture, bone and recursive-child boundaries remain
  explicit; no interactive gameplay or EC retirement is claimed.
  Row 39 is 80/80 promoted. Shared GTE saturation defect #411 and broader
  effect-safety investigation #271 remain open.
  Published in [`5502ab8c6f`](https://github.com/JEschete/VS-NativePort/commit/5502ab8c6f3e77da4d26b38e6b42302393ff519e);
  exact-revision [Source Quality 43988564883](https://github.com/JEschete/VS-NativePort/actions/runs/43988564883)
  passed; [tracker checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6934137024).
- Text-frame evidence (`VR-MENU-TEXT-FRAME-PROMOTION`): F01413-F01416, including
  completing the previously owned F01416 WIP. Each original passed 1,024
  vectors with zero mismatches; all ten original instruction mutants and
  four native broken variants were detected. Drawing comparisons include
  150,267 complete packet words, cursors and chain heads; the reflection
  compares all 32 result bits. Resident drawing callees run without stubs.
  Debug and Release each passed 8,768 cases / 12,951,006 assertions,
  20 focused cases / 39,736 assertions and both canonical replays.
  Generation stability, copier tests and changed-file tidy passed with zero
  new findings. Evidence: `build\promotion-check\group-20261116-181001\summary.json`
  and `D:\Dev\NativePort\wt\batch-text-frame-b4b11beb0\build\scratch\text-frame\verification.json`.
  Live tests cover packet capacity and retained native VM frame leases.
  Arbitrary prior unwritten packet contents and PS1 temporary point storage
  are outside the differential. Compatibility style lookup remains; no
  EC retirement is claimed.
  Published in [`b88a6b2cf3`](https://github.com/JEschete/VS-NativePort/commit/b88a6b2cf312e2cc7bbcd27d58a9af3f6c983ec2);
  exact-revision [Source Quality 43978677859](https://github.com/JEschete/VS-NativePort/actions/runs/43978677859)
  passed; [tracker checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6932733860).
- A39-05 was published in [`3401e5e8b`](https://github.com/JEschete/VS-NativePort/commit/3401e5e8bc6b24f5696c961ad137c13c58bd5da6);
  exact-revision [Source Quality 43974821676](https://github.com/JEschete/VS-NativePort/actions/runs/43974821676)
  passed; [tracker checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6932216999).
  Evidence (`VR-EFFECTS-ANALYZE-GRID`): F01893, F01926, F01928 and
  F01996. Each independently loaded original passed 1,024 vectors, zero
  mismatches and 48 detected original mutants; all four native variants were
  detected. Debug and Release each passed 8,762 regression cases /
  12,944,536 assertions, 57 focused cases / 1,280 assertions and matching
  canonical replays. The focused suite exercises the actual EFFPURGE entries
  0, 16, 18 and 91, including lifecycle and outer-owner cleanup. Copier
  stability, copier tests and changed-file tidy passed. Local evidence:
  `build\promotion-check\group-20261116-174153\summary.json`; per-ID proof:
  `D:\Dev\NativePort\wt\batch-analyze-grid-69396c5e\build\scratch\analyze-grid\verification.json`.
  Original allocation is stubbed; reserved bytes and unwritten packet fields
  are not compared. No interactive Analyze acceptance or EC-04 retirement.
- Clean-checkout generation blocker [#407](https://github.com/JEschete/VS-NativePort/issues/407)
  is fixed in [`b4b11beb0`](https://github.com/JEschete/VS-NativePort/commit/b4b11beb058400b4f31a3c52d1ba145637854b29);
  exact-revision [Source Quality 43964697564](https://github.com/JEschete/VS-NativePort/actions/runs/43964697564)
  passed. Plugin twins use canonical extracted data and required symbol maps
  fail explicitly when absent. Fresh worktree-local CRLF indexes reproduce
  committed Native output; a second generation is stable. This does not
  resolve the separate #391 fresh symbol-ledger churn.
- B47-04 evidence (`VR-MENU-EQUIPMENT-MENUS`): F02630-F02635. Each differential
  passed 1,024 vectors with zero mismatches; all 92 original mutants and six
  native broken variants were detected. Debug and Release group checks each
  passed 8,705 regression cases / 12,943,256 assertions, matching canonical
  replays and 69 focused cases / 392 assertions. Full Debug and Release test
  suites passed sequentially with the same case/assertion totals. The group
  gate passed after ledger synchronization; the integrated worker worktree was
  removed after push and exact-revision CI.
- The earlier B50-05 game-clear initialization stopped without an implementation commit,
  tests, or differentials. Its clean worker worktree remains at
  `D:\Dev\NativePort\wt\batch-game-clear-init-c8f3e2b4`; the preserved source
  snapshot is outside the repository at
  `C:\Users\judee\.copilot\session-state\8cb283b1-2d32-4344-87de-bccc86d8748a\files\b50-05-backup\`.
  [#410](https://github.com/JEschete/VS-NativePort/issues/410) records two
  unconfirmed lifecycle hypotheses; reproduce them separately before
  classifying or fixing either one. The replacement six-function game-clear
  packet is delivered in `6b9c330d36c12741b3f0284dd3a86d6478674110`:
  36 cases / 13,783 assertions, 6,144 original vectors, 80 instruction
  mutants and six compiled native variants. #421 covers its verified
  file-backed MENUF reset omissions, not either #410 hypothesis.
- Keep the promotion plan order and dependencies aligned with issue #4.
  F00830 remains blocked by #296. #373 is resolved; status integration
  and its remaining issue acceptance continue below. #384 waits on #377.
  The copier gates #381 and #383 are closed.
- No interactive combat verification, gameplay completion or compatibility
  retirement is claimed.

Next steps:

1. Confirm status-components publication CI, then integrate the remaining
   delivered status groups in dependency order. Magic publication/CI and
   its eligible issue closures are complete; the remaining #414 observation
   is explicit above. Status-components packet
   `7bd797a55e77bcd7c84c75768f7e24313f7b6f0e` is integrated.
   Status-limbs
   `f682bd35b5814988f36f5c37fecb4a4bb767f1fd` and status-transitions
   `fea43799d892e4fee1ead0a50a8232846583d63b` are delivered; no packet
   alone is a root promotion. Status-terminal packet
   `981252c35ea77ad877221db67ee7bf5c9c9f6654` is delivered: 31 cases /
   186 assertions, 6,144 original vectors, 68 instruction mutants and six
   native variants. Gazette-time packet
   `a841342b54a8362a3e2ad38d34ae154058e78d14` is delivered: 23 cases /
   165 assertions per configuration, 4,096 original vectors, 64 instruction
   mutants and four native variants. Time-attack packet
   `26c6c2b52d5969fdffa4e28e6192a057eba80bb7` is delivered: 53 cases /
   1,011 assertions, 7,168 vectors, 93 mutants and seven native variants.
   Gazette-statistics packet `0343f605887247accb573494b14edbe2df518219`
   is delivered: 30 cases / 749 assertions in each configuration,
   5,120 vectors, 122 mutants and five native variants. Active workers are
   Gazette-terminal in `D:\Dev\NativePort\wt\batch-gazette-terminal-0343f605`
   and map-terminal in `D:\Dev\NativePort\wt\batch-map-terminal-52630eac`.
   #418, #419, #420, #421 and #422 retain their bounded acceptance and
   root integration/CI gates.
   The coordinator's text-update worktree is
   `D:\Dev\NativePort\wt\batch-text-update-b88a6b2c`; its frame-update core
   has 1,024 original vectors, 42 detected
   instruction mutants and a detected compiled native variant. Its shared
   word-codec extraction also preserves all eight earlier text-box cases.
   The actual frame-update Live adapter strictly compiles and its core/Live
   suite passes 11 cases / 106 assertions, including the original scene
   ordering slot, three-buffer rotation and invalid-binding refusals.
   The final Live comparison fix was rebuilt and retested; scoped tidy has
   five existing findings and zero new findings. F01392 is locally registered
   for its compilation database; do not import those old-base shared registry
   edits wholesale. Notification cores have been strictly rebuilt after the
   final test refactor: 10 cases / 104 assertions, zero new quality findings.
   The notification Live adapter and actual Live tests strictly compile;
   core/Live suites pass 19 cases / 164 assertions. Final Live lint,
   original/native proofs and cold cleanup producer wiring remain pending.
   The partial packet is not promoted.
   Continue the dependency-ordered plan.
   The emitter investigation logged the shared ApplyRotMatrix saturation
   defect in #411; the shared helper is unchanged, and no shared-helper
   repair or gameplay completion is claimed. F00830 remains blocked on the
   actual climbing-hit contract in #296, not a guessed blend constant.
2. Keep #410 open until the loader and reward caller paths are reproduced; do
   not present B50-05 as a completed promotion.
3. For each group, follow [WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md),
   then commit, push, and verify exact-revision CI before closing its tracker
   checkpoint.

### Previous: battle-sphere lifecycle/geometry (2026-11-15)

[`4dc191b04`](https://github.com/JEschete/VS-NativePort/commit/4dc191b04c586ad212d8c982ca8bb094779d99e2)
promoted F01279-F01286, F01297, F01303-F01305 and F01312 with the copier layout
reconciliation they needed (#391, #392); evidence is in
`VR-BATTLE-SPHERE-LIFECYCLE-GEOMETRY`. #371 was fixed in `b20053f3a`: `Core_Gte`
RTPS uses the GTE's unsigned Newton-Raphson perspective divide and derives IR0
and SX2/SY2 from the wide sums (`gte_projection` differential: 8,192 vectors,
0 mismatches). `RotTransPers4` still runs four RTPS steps rather than one RTPT.

### Previous published batch (Group O)

[`08b0f209`](https://github.com/JEschete/VS-NativePort/commit/08b0f20960cb6a8e5d62c8924c170b6e6d7fa4db)
promoted F00056, F00057, F01417, F01418, F01447, F01448, F01451 and F01452 to
typed bone/weapon-trail operations; its evidence and exact-revision CI are on
the [#276 checkpoint](https://github.com/JEschete/VS-NativePort/issues/276).
## Earlier status (2026-11-13)

Rows 28 (battle AI) and 29 (damage/status) of the C++ promotion work (#276) were source-complete and published; row 30 (battle-actions) was active. The earlier perception promotion is the
F01638-F01640/F01643 perception group in `33ad4f3e`
([CI 43073456695](https://github.com/JEschete/VS-NativePort/actions/runs/43073456695)). Each published row-28
commit and its exact-revision CI run are recorded on [#276](https://github.com/JEschete/VS-NativePort/issues/276)
and in the ledgers. The per-function publish list, Latest Work and Verified sections that used to be here are in the
[archived handoff](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF.md).
The step-by-step procedure is [WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md).
See [Coordinated continuation](#coordinated-continuation) and
[Current continuation and blocker](#current-continuation-and-blocker) for that earlier boundary.

## Coordinated continuation

The owner resumed the remaining-function work on 2026-11-13 with a coordinator
and two isolated implementation workers; [#276 checkpoint](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6846379221).
The verified base is `dcf481125fb575cc9a02f30c43d93038c2f65097`,
with successful exact-revision
[CI 43374243364](https://github.com/JEschete/VS-NativePort/actions/runs/43374243364).
At that verified base the cohort had 196 resolved (187 promoted, nine retired),
704 remaining. Current progress is in [REMAINING_FUNCTIONS.csv](ledgers/REMAINING_FUNCTIONS.csv)
and the CI-backed issue checkpoints.

- Initial Worker A cluster: F00196/F00197/F00198/F00224 reward-dialog continuations in
  `D:\Dev\NativePort\wt\coordinated-a-battle-events`.
- Initial Worker B cluster: F02566/F02567/F02569/F02571/F02573/F02574 inventory-detail helpers in
  `D:\Dev\NativePort\wt\coordinated-b-inventory`. Release integration only after
  checking actual native callee/shared-state dependencies against maintained order.
- The coordinator owns main-tree registration/generation, canonical ledgers/CSV,
  shared integration gates, scoped commits/pushes and exact-revision CI.
  Worker preparation is not a completed promotion. Sources stay frozen during
  evidence runs; linked reference/data/dependency inputs are read-only.
- Existing broad dirty `a47-menu-inventory-container` and `a52-menu-map-manual-text`
  worktrees are preserved untouched and are unverified prior art, not fresh bases.
  F00830/#296 remains separately blocked; row 30 is not complete.

Routine batches use the canonical ledgers, machine-readable evidence and concise
CI-backed issue checkpoints, not a handoff diary.

## Reward-flow checkpoint (2026-11-13)

The eight-function reward-flow batch was completed on 2026-11-13. This section
records its evidence; current cohort progress is maintained in the CSV.

- F04123-F04126 and F00220-F00223 are eight typed reward-flow promotions.
  The original 900-row cohort is **196 resolved (187 promoted, nine retired),
  704 remaining**. Row 32 has eight promoted and 85 remaining.
- Evidence: `build\scratch\reward-flow\verification.json`, verified on
  `2cdb3e64e4a5603b8fbd4433df62f800e0f10fcf` plus the scoped changes.
  Publication commit and exact-revision CI are recorded on
  [#276](https://github.com/JEschete/VS-NativePort/issues/276) and in that evidence.
- Per-function proof: 285 original instructions, 8,192 vectors, zero mismatches,
  89 detected original mutants, eight detected native variants, 29 own Catch2
  cases/277 assertions and 33 Live controls. The aggregate native-variant hash
  guard was rerun successfully on an unchanged tree, not waived.
- Final group `build\promotion-check\group-20261113-003641\summary.json`:
  376 seconds; Debug/Release each pass 6,380 cases/12,926,759 assertions.
  Both canonical replays, copier stability/controls and zero-new quality pass.
  Ordered camera/shadow controls pass 39 cases/175 assertions in each configuration.
- Canonical ledgers and the CSV are synchronized and checked. No reward
  implementation WIP remains; nine other generated reward functions and both
  reward globals are retained. The earlier WIP snapshot is historical.

The coordinated continuation above records the earlier reward package.
F00830 remains separately blocked under #296; row 30 is not complete.
No gameplay or compatibility-retirement completion is claimed.

## Current continuation and blocker

Routine batch status now lives in [REMAINING_FUNCTIONS.csv](ledgers/REMAINING_FUNCTIONS.csv),
the canonical ledgers, machine-readable evidence and [#276](https://github.com/JEschete/VS-NativePort/issues/276).
The older continuation sections below are historical checkpoints; use the current
CSV, ledgers and issue #276 for next-work status.

The six animation policies are published in `10f09362`, with successful exact-revision
[CI 43354155209](https://github.com/JEschete/VS-NativePort/actions/runs/43354155209).
The final defined reactions F00819/F00832/F00891 are published in `013924cd`,
with successful exact-revision
[CI 43360661373](https://github.com/JEschete/VS-NativePort/actions/runs/43360661373).
Evidence: `build\scratch\animation-reactions\verification.json`.
Current cohort counts are maintained in the CSV, not duplicated into each handoff.

Row 30 is **not complete**: F00830 remains unresolved under [#296](https://github.com/JEschete/VS-NativePort/issues/296).
Its source-climbing blend inherits VM address/linked-list state; zero is not equivalent to all shadow/blend effects.
The original function body and DATA remain unchanged. Do not invent a blend constant, pointer/register API,
or a refusal presented as a completed gameplay fix. The next observation is an evidenced native behavior decision
or aligned reference climbing-hit evidence; the target-route visited-count evidence alone does not resolve it.
Continue independently verifiable maintained-code work without marking this
function or row 30 complete.

## Active continuation (2026-11-11)

The owner requested completion of the original 900-function cohort and progress
tracking in `REMAINING_FUNCTIONS.csv`. Work resumed from clean `0e8989ad`;
exact-revision Source Quality `43105524312` passed. The earlier checkpoint below
is retained as historical evidence.

### Latest row-30 group

F00100/F00101/F00108/F00109/F00110/F00191 are locally resolved on `475ed67d`:
four typed `ActionPresentation` promotions and two separately retired forwarding
aliases. The completed evidence was recovered before publication; source hashes
still match the final verified group.

- Evidence: `build\scratch\action-presentation\evidence.json`.
- Active functions: 837 original instructions, 12,288 defined vectors, zero
  mismatches; 64 original mutants and four native broken variants detected.
  The two aliases have separate VOID caller/reference controls across 89 images.
- Group `build\promotion-check\group-20261112-110611`, 369 seconds:
  Debug/Release each pass 6,205 cases/12,924,644 assertions; coupled selection
  530 cases/35,599 assertions. Both canonical replays, copier stability and
  controls pass; tidy five existing findings, zero new.
- #356 production regressions verify correct compacted target kinds, current-hit
  default limbs, bystander exclusion and the 16-target effect boundary. The
  seventeenth unique target is diagnosed before append/dispatch, never truncated;
  255 duplicate source hits remain supported. Separate broken variants detect
  the corrected cases. The signed model nibble is projected as its original
  unsigned four-bit value, with its own red/green control.
- Original parity excludes the corrected indexing/overflow cases and unspecified
  padding. The overwritten first bone lookup is omitted only after auditing the
  read-only query contract. No fabricated effects or capacity expansion.
- Publication and exact-revision CI are pending. No interactive particle/combat,
  row-30 completion or EC/M8 retirement claim; #355 and #296 remain open.

### Previous resource/injury/dispatch group

F00500-F00507 are locally resolved on `f2d4c84d`: five C++ promotions and
three separate reserved-selector retirements. Resource changes reuse
`DamageRecoveryEffects`; injury/progression policies use `ActionEffectDispatch`.

- Evidence: `build\scratch\resource-dispatch\evidence.json`.
- Five active functions: 509 original instructions, 11,264 vectors, zero
  mismatches; 82 original mutants and five native broken variants detected.
  The three empty entries are not numeric-return or C++ promotion claims.
- Selectors 67-69 have no uses in the 512 original action hits. Across 89
  configured executable images, their addresses appear only in the original
  mediator slots, with no direct jumps/calls. Definition writers and current
  consumers were audited. Their module entries use the existing VOID5
  diagnostic refusal; each has a relocation control.
- Group `build\promotion-check\group-20261112-094200`, 446 seconds:
  Debug/Release each pass 6,191 cases/12,924,552 assertions; coupled selection
  342 cases/90,638 assertions. Sixteen new focused cases include the three
  retirement controls. Both canonical replays, copier/project stability and
  controls pass; tidy six existing findings, zero new.
- Real stat/status primitives and module dispatch are exercised with explicitly
  controlled callbacks where recorded. Prior preview, damage and status
  comparisons pass. Used-input guards and the low-byte script-result contract
  remain explicit.
- `promote.py ledgers` now accepts an audited `retirements` map separately from
  promotions, with identity/idempotence and double-disposition controls. The
  original 900-row cohort is retained; no baseline was weakened.
- Published in `475ed67d`, green on
  [CI 43242544386](https://github.com/JEschete/VS-NativePort/actions/runs/43242544386).
  No whole selected-stat,
  interactive combat, row-30 completion or EC/M8 retirement claim.

### Previous damage/recovery group

F00484/F00485/F00494-F00499 are locally verified on `242d0a0f`.
The two triggered recoveries reuse `TriggeredRecoveryQueue`; the six remaining
policies use `DamageRecoveryEffects` with checked VOID5 bindings. All 47 old
code references are removed; the emptied generated `ActionEffects.cpp` is gone.

- Evidence: `build\scratch\damage-recovery\evidence.json`.
- Eight new IDs: 294 original instructions, 10,240 vectors, zero mismatches;
  79 original mutants and eight native broken variants detected.
- Group `build\promotion-check\group-20261112-085536`, 476 seconds:
  Debug/Release each pass 6,175 cases/12,923,631 assertions; coupled selection
  227 cases/137,042 assertions; 13 new focused cases. Both canonical replays,
  copier stability and controls pass; tidy six existing findings, zero new.
- Original stat dispatch and limb damage/recovery/equipment DP/PP helpers
  execute with a controlled SHORT stat callback. Production tests exercise
  actual live helpers and original-module VOID5 relocation. Separate guard
  callbacks verify cached owners, fresh post-limb statuses and DP-before-PP
  observations. Previous affected comparisons and guard controls pass.
- Missing used inputs are diagnosed; inactive inputs remain lazy. Source and
  target ownership, signed MP caps, pending halfword wrap and mode-bit
  preservation remain verified. Empty-weapon and fresh-preview contracts are
  unchanged; no quality baseline changed.
- Published in `f2d4c84d`, green on
  [CI 43234065347](https://github.com/JEschete/VS-NativePort/actions/runs/43234065347).
  This is source promotion,
  not whole selected-stat equivalence, interactive combat, row-30 completion
  or EC/M8 retirement.

### Previous preview/resolution group

F00508/F00509/F00513/F00514/F00515/F00516 are locally verified on `64a95a3d`,
using typed `ActionPreview` policies and checked live bindings. Generated
and owned callers now use the native reset, outcome, resolution and preview APIs.

- Evidence: `build\scratch\action-preview\evidence.json`.
- `action_preview`: 861 original instructions, 12,288 defined vectors,
  zero mismatches; 56 original mutants and six native broken variants detected.
- Group `build\promotion-check\group-20261112-081034`, 533 seconds:
  Debug/Release each pass 6,162 cases/12,916,857 assertions; coupled selection
  120 cases/154,524 assertions; own tests 26/531. Both canonical replays,
  second-generation stability and 213 copier controls pass; tidy 49 existing
  findings, zero new. A test-only statement-count finding was repaired before
  repeating the full group; no assertions or baselines were weakened.
- Both production preview entrypoints execute original actions 23/25 through
  real resolution and chain evasion. All 12 action/builder/prior-storage
  combinations produce identical complete fresh results. Source limb 0 and
  focus 0 follow the existing fresh-selection contract, not an actor's default
  target limb or a reaction's copied hit. Checked limb guards are unchanged.
- Native 255-target reset/copy, clear-before-read aliases, callback-current
  data with cached owners, cancellation identity, redirects and lazy unused
  inputs pass controls. Original trap definitions 6-21 also run populated
  world-source resolution previews.
- Original unspecified preview fields are excluded from numeric equivalence
  and verified separately as the #353 fix. Original geometry comparisons
  cover nonnegative low32 squared lengths; negative words retain the existing
  native scalar clamp. Used missing redirect/world-physical context is diagnosed.
- Published in `242d0a0f`, green on
  [CI 43227261952](https://github.com/JEschete/VS-NativePort/actions/runs/43227261952).
  [#353 is closed](https://github.com/JEschete/VS-NativePort/issues/353#issuecomment-6825098332).
  Effect/injury/volume services
  and module tables remain migration debt; no interactive gameplay, row-30
  completion or EC/M8 retirement claim.

### Previous action-cost group

F00510/F00511/F00512/F00548 are locally verified on `e55a82ea`, using typed
`ActionCosts` policies and checked live bindings. All 37 old symbol references
are removed, including generated menus and owned ability/diagnostic callers.

- Evidence: `build\scratch\status-queue\cost4-evidence.json`.
- `action_costs`: 16,384 defined vectors over 261 original instructions,
  zero mismatches; 36 original mutants and four native broken variants detected.
  Cost-preview and restriction comparisons execute the real cost callee.
- Group `build\promotion-check\group-20261112-070000`, 439 seconds:
  Debug/Release each pass 6,136 cases/12,916,326 assertions; coupled selection
  223 cases/25,220 assertions; own tests 28/593. Both canonical replays,
  second-generation stability and copier controls pass; tidy six existing
  findings, zero new.
- All 81 checked-live diagnostic probes pass. Tests exercise actual empty
  equip/unequip and shipped natural/armed enemy producers. All 256 ZNDs'
  455 character records use cost pairs 0/0 or 2/1-3; 216 are no-resource
  records, including 65 positive-HP records offering base attack.
- For #352, empty/natural weapon type 0/cost 0 explicitly means no payment;
  unsupported combinations are diagnosed. This is a native contract, not
  numeric equivalence to the original uninitialized register. Defined resource
  widths, HP reserve, risk clamp, chain arithmetic, additive packed carries,
  lazy inputs and callback-visible reads remain verified.
- Published in `64a95a3d`, green on
  [CI 43216475577](https://github.com/JEschete/VS-NativePort/actions/runs/43216475577).
  No interactive gameplay,
  attack-eligibility, row-30 completion or EC/M8 retirement claim.

### Previous triggered-recovery group

F00476-F00483 are locally verified on `0c3c8f6f`, using a shared
`TriggeredRecoveryQueue` and checked VOID5 bindings. Selected type/affinity
gates remain separate from input action data; calculator results update
post-callback pending HP/flags before the real recovery helper. All 32 old
symbol references are gone.

- Original span: 0x80084390-0x800847F0, 280 instructions. Evidence:
  `build\scratch\triggered-recovery\ledgers-final.json`.
- `triggered_recovery`: 16,384 vectors, zero mismatches, 98 mutants detected.
  Eight native inverted-gate variants detect every own-ID vector, with
  unrelated IDs unchanged; a shared stale-read variant is also detected.
- Group `build\promotion-check\group-20261112-060558`, 421 seconds:
  Debug/Release each pass 6,108 cases/12,915,733 assertions; coupled selection
  196 cases/109,974 assertions; own tests 24/52,524. Both replays, copier
  stability and controls pass; tidy zero new findings.
- Ninety-eight diagnostic probes, 32 real-helper successes and 24 controlled
  recovery-entry observations pass. Actual table/ABI controls and prior basic/
  random differential and guard suites pass again.
- Original and native stat dispatch/recovery algorithms execute, with a
  controlled selected stat callback. Published in `7a3360d4`, green on
  [CI 43208041790](https://github.com/JEschete/VS-NativePort/actions/runs/43208041790).
  No row30, gameplay or EC/M8 completion claim.

Random8 effects are published in `0c3c8f6f`, green on
[CI 43202720511](https://github.com/JEschete/VS-NativePort/actions/runs/43202720511),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6821655175).

Basic7 effects are published in `ca4ff6a3`, green on
[CI 43199227740](https://github.com/JEschete/VS-NativePort/actions/runs/43199227740),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6821132916).

Row 29 is published in `7b797783`, green on
[CI 43193683736](https://github.com/JEschete/VS-NativePort/actions/runs/43193683736):
all 88 IDs reconciled, 85 promoted and three retired. [#351 is closed](https://github.com/JEschete/VS-NativePort/issues/351#issuecomment-6820264328).
Its supported healing contract and undefined-input refusals remain verified;
no original numeric return behavior was invented for retired sentinels.

C6 damage policies are published in `85fb812e`, green on
[CI 43188974850](https://github.com/JEschete/VS-NativePort/actions/runs/43188974850),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6819687952).

B8 incoming-stat factors are published in `d29b8d86`, green on
[CI 43183509421](https://github.com/JEschete/VS-NativePort/actions/runs/43183509421),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6818895374).

A8 stat factors are published in `93b3ac07`, green on
[CI 43179698344](https://github.com/JEschete/VS-NativePort/actions/runs/43179698344),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6818328299).

The hit/stat dispatch group is published in `d592578e`, green on
[CI 43175458376](https://github.com/JEschete/VS-NativePort/actions/runs/43175458376).
[#350 is closed](https://github.com/JEschete/VS-NativePort/issues/350#issuecomment-6817641744).
Those three sentinel retirements remain distinct from C++ promotion and
numeric behavior verification.

The six hit modifiers are published in `42acb462`, green on
[CI 43170979793](https://github.com/JEschete/VS-NativePort/actions/runs/43170979793),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6817073762).

The actor-vitals group is published in `d1dd80f4`, green on
[CI 43166632406](https://github.com/JEschete/VS-NativePort/actions/runs/43166632406),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6816362626).

The lifecycle group is published in `c888aab4`, green on
[CI 43160888226](https://github.com/JEschete/VS-NativePort/actions/runs/43160888226),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6815535713).

The seven supporting leaves are published in `8e21f5b7`, green on
[CI 43156876978](https://github.com/JEschete/VS-NativePort/actions/runs/43156876978),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6814998528).

The defense/cure group is published in `fcc50d5d`, green on
[CI 43152730395](https://github.com/JEschete/VS-NativePort/actions/runs/43152730395),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6814680374).

The preceding ailment/attack group is published in `fff60643`, green on
[CI 43149478721](https://github.com/JEschete/VS-NativePort/actions/runs/43149478721),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6813988874).

The preceding F00443-F00450 group and #349 are published in `e8565059`;
[CI 43146186212](https://github.com/JEschete/VS-NativePort/actions/runs/43146186212)
passed. [#349 is closed](https://github.com/JEschete/VS-NativePort/issues/349#issuecomment-6813404271)
with compiler-negative-control and actual typed-dispatch evidence. Its
16-function differential is archived at
`build\differential\status_queue-F00443-F00450.log`. Incidental MIPS returns
remain driver-only projections, not production callback APIs.

### First row-29 group (published)

F00405/F00406/F00437-F00442 are locally verified on `b6c5c42a` in the
`StatusQueue` core/live module. Typed rules, target status/immunity/exemption
and queued additions/removals replace the global reads; six status handlers
share the rule kernel while preserving their own rule and feedback choices.
The eight old symbols have zero references (62 before). Production encounter
and generated action-resolution consumers use the checked live API.

- Original spans: 0x80081020-0x80081130 (68 instructions, SHA-256
  `64025fc62cd0f26206a1c079b5b0c41878fb9aab80a366befe9215f89c23c9c6`)
  and 0x80083998-0x80083B78 (120 instructions, SHA-256
  `107dfe5fc9b8f9c2f2dae0acd89f076721670639ec0685cedce907b363ae23b9`).
- `status_queue`: 8,192 vectors, zero mismatches; 46/46 mutants detected.
  Original primitives execute through the handlers without status-logic
  stubs. Eight native broken variants detect 823/152/154/155/157/159/149/166
  mismatches of 2,048 vectors respectively.
- Final group `build\promotion-check\group-20261111-212737`, 384 seconds:
  Debug/Release each pass 5,786 cases/12,652,032 assertions, with 23 focused
  cases/239 assertions. Both canonical replays match in both configurations;
  copier stability/controls pass; tidy reports five existing findings, zero
  new. The first group failed only the case's import sorting; it was fixed,
  the differential rerun, and the complete group gate repeated.
- Canonical ledgers and progress refresh pass. Local evidence:
  `build\differential\status_queue.log`, eight `status_queue-F*-variant.log`
  reports and `build\scratch\status-queue\ledger.json`.
- Limits: distinct valid synthetic input storage in the differential; missing
  rules and invalid live pointers/indices refuse diagnostically. The live
  32-rule and 42-actor tables remain compatibility dependencies. No complete
  interactive status coverage or EC-04/M8 retirement claim. Published in
  `1c420d3f`; exact-revision
  [CI 43142573923](https://github.com/JEschete/VS-NativePort/actions/runs/43142573923)
  passed.
- Subsequent caller tracing confirmed [#349](https://github.com/JEschete/VS-NativePort/issues/349):
  the real effect-mediator table takes `void` callbacks, but the six promoted
  Live handlers retained misleading original `int` declarations. A native
  compiler assignment probe rejects the types; no gameplay crash is claimed.
  Commit `e8565059` repaired those six bindings and its eight new handlers,
  with compile-time and actual typed-dispatch tests; #349 is now closed.
  Green builds/replays alone did not prove this contract. The two status
  primitives retain their consumed `int` masks.

### Completed battle-AI groups

The candidate-movement batch is published in `fc0e2525`; exact-revision Source
Quality [43136464204](https://github.com/JEschete/VS-NativePort/actions/runs/43136464204)
passed. Publication and detailed evidence are recorded on
[#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6812029760).

The first batch is F01649-F01654 in `CandidateMovement.hpp/.cpp` and
`CandidateMovementLive.hpp/.cpp`: candidate movement, combat pursuit,
planned-target approach, close follow, enemy pursuit and guard movement.
The six old symbols had 24 code references; all are removed. Generated callers
and all six original module entries use the checked live bindings.

**Progress: 153 of the original 900 resolved (145 promoted, 8 retired); 747
remain** (746 generated plus F01416 owned WIP). Modernization is 2,989/3,741;
C++ promotion is 2,990 plus
one WIP. These remain separate measures.

- Original range: 0x800D9E18-0x800DAA6C (exclusive), 789 instructions,
  SHA-256 `9f02fb28f761f8b846c643243bf60e6bc5cc335caec80929103dbb087cee375d`.
- `differential.py candidate_movement`: 6,144 vectors (1,024/function),
  zero mismatches; 73/73 mutants detected on 1,152 vectors each. External
  callees are queued stubs, including mutation effects; original scalar math
  instructions are followed. Evidence: `build\differential\candidate_movement.log`.
- Native broken variants detect 2/47/27/111/78/40 mismatches respectively
  for F01649-F01654 over 1,152 vectors each. Reports:
  `build\differential\candidate_movement-F01649-variant.log` through
  `candidate_movement-F01654-variant.log`.
- Initial focused expectations incorrectly assumed exact integer square roots.
  Running the original instructions confirmed 10000 -> 99, 40000 -> 199 and
  90000 -> 298; the tests now use those values and the actual range boundaries.
  The differential also caught stale candidate reads across callbacks; the
  core preserves the entry-time target and range while rereading the fields
  the original reloads. Dedicated tests cover both callback cases.
- The first group check passed both builds, full suites and replays but failed
  tidy on optional access and signed-byte diagnostics. These were corrected
  without changing baselines; signed linked IDs intentionally preserve the
  original `lb` semantics rather than converting negative IDs to unsigned.
- Final group gate: `promotion_check.py group --focused "[candidate-movement]"`,
  `build\promotion-check\group-20261111-203535`, 371 seconds. Debug and Release
  each pass 32 focused cases/174 assertions and 5,753 full-suite cases/
  12,651,820 assertions. Both `silver_wolves` (2,400 frames) and
  `first_lever_room` (900 frames) match in both configurations. Copier
  stability and controls pass; tidy reports four existing findings, zero new.
- Canonical gates, modernization and hazards sync/check pass. The progress CSV
  retains 900 rows with six `done`/`promoted` rows. All 18 promotion-tool
  controls pass, including original-cohort retention, unchanged snapshot,
  retirement versus WIP, byte-stable rerun and failure-before-write.

The live boundary retains the actor-record, AI/frame and room tables. Missing
required records, negative linked IDs and invalid rosters are refused with
diagnostics. Empty/unused candidate paths remain lazy. Neither these tests nor
the canonical replays establish every pursuit/guard gameplay path, complete
battle-AI/M8 behavior, or EC-04 retirement.

The final row-28 batch, F01656-F01659, is now locally verified on `fc0e2525`.
`ActionExecution` and its checked live binding implement candidate execution,
planned actions and frame-budget dispatch; escort movement shares the follow
prefix in `CandidateMovement`. Generated `Behavior.cpp/.h` are retired and
their four old symbols have zero consumers (17 before).

- Original range: 0x800DAAD8-0x800DB0BC (exclusive), 377 instructions,
  SHA-256 `1559513320faef91480dbd464ee03abfb539cad367eca3e435866f70ea2408d3`.
- `differential.py action_execution`: 4,096 vectors, zero mismatches;
  47/47 mutants detected. Four native broken variants detect 81/212/84/82
  mismatches over 1,024 vectors each. The shared `candidate_movement`
  differential was rerun: 6,144 vectors, zero mismatches, 73/73 mutants.
- Group `build\promotion-check\group-20261111-205725`, 385 seconds:
  Debug/Release each pass 5,773 cases/12,651,925 assertions; combined focused
  tests pass 52 cases/279 assertions. Both canonical replays match in both
  configurations; copier stability and 272 tool controls pass; tidy reports
  five existing findings, zero new.
- Row-28 audit: all 89 original cohort IDs are promoted; no generated
  function remains in the slice. Plan row 28 is marked done. This is source
  promotion, not M8 or EC-04 completion. External differential callees remain
  queued stubs, and #337 remains suspected with diagnostic refusals preserved.
- Local evidence: `build\differential\action_execution.log`, the four
  `action_execution-F01656-variant.log` through `F01659` reports, and
  `build\scratch\pursuit\action_execution_ledgers.json`. Published in
  `b6c5c42a`; exact-revision
  [CI 43139398025](https://github.com/JEschete/VS-NativePort/actions/runs/43139398025)
  passed, recorded on
  [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6812436244).

Continue ordered row 30 (`battle-actions`, 36 of 91 still remaining;
50 promoted and five retired).
The #354 classifier repair and cost group F00510/F00511/F00512/F00548
(#352) and six-function preview group (#353) are published and green.
Damage/recovery effects F00484/F00485/F00494-F00499 are published and green.
Resource/injury/dispatch F00500-F00507 is published and green in `475ed67d`.
Publish the verified presentation group with #356, then continue phase helpers
F00001/F00089/F00096/F00097/F00098/F00114/F00121/F00122. Their 152 original
instructions and alias audit are in `action_phase_helpers.txt` and
`action_phase_configured_module_references.log` under `build\scratch\status-queue`.
Their original source/instruction analysis is in `damage_recovery_effects.txt`
and `resource_effect_dispatch.txt` under `build\scratch\status-queue`.
Subsequent phase and outcome helpers also have complete instruction analysis.
Do not silently jump ahead of this ordered package.

[#351](https://github.com/JEschete/VS-NativePort/issues/351) records F00421's
latent healing-affinity defect: drawn encoded-affinity 1 reads an unwritten
stack halfword. The shared original-MIPS probe returns 100 versus 200 when
only that synthetic residue changes; both original healing definitions use
encoded affinity 6 and avoid the path. The D5 supported contract and explicit
refusals are published and CI-verified; #351 is closed.
No zero intermediate or gameplay-failure claim is invented. Evidence:
`build\scratch\status-queue\healing-affinity-probe.py`
and its log.
Finish the remaining damage/hit functions afterward. Keep every original
cohort row in the progress CSV, including
resolved ones; the `snapshot` column stays `44fa8847`. F01416 remains the one
owned WIP and must not disappear from the remaining count.

## Prior checkpoint (before this continuation)

- Branch `imp/01-ma04-gates`. Last code commit: `33ad4f3e` (perception group). It is followed by documentation commits
  only: `44fa8847` (handoff), `8db4f46e` (the remaining-functions snapshot) and the Docs cleanup described below.
- **900 functions remain** (after the perception group in `33ad4f3e`).
  Modernization: 2,844 done of 3,744. C++ promotion: 2,845 promoted plus one
  frame-orchestrator WIP. These are separate measures. Row 28 has 10
  generated functions left. [REMAINING_FUNCTIONS.csv](ledgers/REMAINING_FUNCTIONS.csv)
  lists all 900, with purpose, location, plan row and status, as a snapshot at
  `44fa8847`. `promote.py ledgers` now refreshes its progress columns after
  the canonical ledger checks pass. It retains all 900 original rows,
  including resolved entries, and keeps their original snapshot/provenance.
- Original 3,479-function cohort at `ffd1a8e1`: 2,505 promoted, 16 retired,
  957 generated, one WIP; 2,521 resolved. This continuation resolved 72 more
  than `6c8ac0b9`.
- Plan row 27, field gimmicks/doors, is done for C++ ownership: its original
  51 IDs audit to 46 promoted and five retired. Row 28, battle AI, is active.

### Docs cleanup (2026-11-11)

On owner request, obsolete and duplicate documents were removed from `Docs`. Their last versions remain at
`https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/<file>`.

| Removed | Where it went |
| --- | --- |
| `HANDOFF_HISTORY.md` (847 KB) | Archive link only; it held old checkpoints and evidence paths |
| `CodeQC Reports\Code_Quality_Report_20261022.md` | Its findings map to existing issues; the one untracked finding, QC-18, is #348 |
| `CUTSCENE_PLAYTEST_REMAINING.md` | The 41 unchecked scenes are tracked in #346 |
| `DATA_MIGRATION_PLAN.md` | Summarized on #47, its home issue |
| `ASSEMBLY_GAPS.md`, `ASSEMBLY_GAPS.csv`, `ASSEMBLY_GAP_COVERAGE.csv` | `audit_assembly_gaps.py` writes to `build\quality\assembly-audit`; `--publish` is opt-in |
| `RUNTIME_TRACING.md` | Deferred work tracked in #347; `TRACE_QUEUE.csv` stays |
| `EMULATION_COMPATIBILITY_AUDIT.md` | Merged unchanged into the plan as [§15](NATIVE_SOURCE_PORT_PLAN.md#15-emulation-compatibility-register) |
| `ledgers\SOURCE_FILE_LEDGER.md` | Never updated after generation; `build_ledgers.py` no longer writes it |
| `ledgers\NATIVE_SYMBOL_REVIEW.csv` | Drifted and not in CI (#273); the inventory tool stays |

`SDK_API_LEDGER.md` is kept: it is maintained by hand as shims retire. `COMBAT_LAB.md` is now a usage guide, and
`NAMING_CONVENTIONS.md` §11.3 keeps only the rules from the old dated logs. The owner's `Decomp Process.md` notes and
the Reddit draft were local, untracked files: the notes moved to `C:\Tools\Decomp Process.md` and the draft was deleted.

## Historical execution checkpoint (2026-10-23)

Row 28 was published and green. The
row-29 checkpoint has all 88 IDs resolved (85 promoted, three retired),
published and green in `7b797783`. #349, #350 and #351 are closed.
Row 30's first 23 effect producers are published and green through `7a3360d4`.
The four cost functions are published and green in `64a95a3d`; #352 is closed.
The six preview/resolution functions are published and green in `242d0a0f`;
#353 is closed. The next eight damage/recovery effects are published and green
in `f2d4c84d`. Resource/injury/dispatch is published and green in `475ed67d`;
the next presentation group owns #356.

[#354](https://github.com/JEschete/VS-NativePort/issues/354) records the
forwarding classifier treating an unbraced `if (...) Call();` as a call to
`if`. Current native policies use ordinary braces and pass unchanged gates;
the classifier correction is locally verified with four formerly failing
control-flow cases, keyword-prefixed forwarding controls, all 98 gate tests
and scoped Ruff. Fresh before/after live gate reports are identical with
zero violations. Published in `e55a82ea`, green on
[CI 43209366101](https://github.com/JEschete/VS-NativePort/actions/runs/43209366101);
[#354 is closed](https://github.com/JEschete/VS-NativePort/issues/354#issuecomment-6822541802).

[#352](https://github.com/JEschete/VS-NativePort/issues/352) records a later
F00510 cost-check defect: BaseAttack with a weapon cost type outside 1-4
uses an uninitialized resource value. A shared-MIPS probe changes the
requirements byte by changing only incoming a3, which is not a formal
argument. Repro: `build\scratch\status-queue\action-cost-input-probe.py`.
The original/native unequip paths and shipped natural-weapon data produce
type zero/cost zero. The verified cost group explicitly treats that pair as
no resource payment without rejecting ordinary empty loadouts; other
unsupported pairs refuse diagnostically. See the latest group above and
[#352 evidence](https://github.com/JEschete/VS-NativePort/issues/352#issuecomment-6823239942).
The fixing revision `64a95a3d` and its exact-revision CI are verified;
[#352 is closed](https://github.com/JEschete/VS-NativePort/issues/352#issuecomment-6823558442).

[#353](https://github.com/JEschete/VS-NativePort/issues/353) records partial
preview-request initialization in F00515/F00516. The original builders pass
stack-dependent source-limb/focus fields to the resolver; original self-route
actions 23/25 use chain evasion, which reads that limb. Do not weaken the
checked limb guard. Repro: `build\scratch\status-queue\preview-initialization-probe.py`.
The six-function preview group now verifies complete fresh initialization
and actual self-route calls without weakening the limb guard; see the latest
group above and [local acceptance evidence](https://github.com/JEschete/VS-NativePort/issues/353#issuecomment-6824772846).
The fixing revision `242d0a0f` and exact-revision CI are verified;
[#353 is closed](https://github.com/JEschete/VS-NativePort/issues/353#issuecomment-6825098332).
Resume the prepared effect groups within row 30.

[#355](https://github.com/JEschete/VS-NativePort/issues/355) records F00193's
empty-sequence defect: zero `chainStep` skips both queued-action assignments,
then the recovery-mode test dereferences an uninitialized pointer. The original
138-instruction probe changes only incoming s3 and produces modes 1 versus 0;
the nonempty control produces 1 in both cases. This corroborates existing
compiler hazard `d963ed2dcf8d`, not an observed gameplay failure.
Evidence: `build\scratch\status-queue\empty_recovery_probe.py/.log` and
`action_execution_tail.txt`. Resolve the lifetime/empty-phase contract in its
owning action-execution group, without inventing a pointer or reward mode.
This does not alter the current effect-group order.

[#356](https://github.com/JEschete/VS-NativePort/issues/356) records four
F00109 request-indexing defects retained by current native presentation:
compaction changes a later default-limb lookup, a world point can suppress an
actor target, targeted actors can be listed as bystanders, and a seventeenth
effect target overwrites the adjacent bystander list. The effect request still
holds 16 targets, unlike the native action request's 255 hits; apply bounds to
the compacted output, not an arbitrary limit on input hits. The shared
original-instruction probe captures each at effect dispatch using action 60,
with controlled position/animation/sound/bone services. Evidence:
`build\scratch\status-queue\presentation_index_probe.py/.log`.
This is not an interactive visual result or a reopening of #76's unrelated
particle-motion acceptance. Fix in the owning presentation group after the
current effect groups; keep corrected output separate from original parity.

The remaining ten battle-animation functions have complete source and
930-instruction analysis in `build\scratch\status-queue\action_animations.txt`.
[#296 remains open](https://github.com/JEschete/VS-NativePort/issues/296#issuecomment-6827636218):
the original target-impact caller supplies its visited-hit count as the
climbing blend argument, but the source-impact caller still inherits incoming
s4. `climbing_blend_probe.py/.log` verifies both call chains. Source-route
provenance and actual climbing/reference behavior still need evidence; no
blend constant or completed climbing fix is claimed.

The supporting group covers F00339/F00340/F00342/F00343/F00347/F00348; all
353 original instructions are read in `build\scratch\status-queue\hit_modifiers.txt`.
[#350](https://github.com/JEschete/VS-NativePort/issues/350) tracks the separate
F00341/F00407/F00436 empty-callback return/ABI defects. The source/definition
audit and typed retirement guards are published and CI-verified in `d592578e`;
the issue is closed. No original numeric return behavior is claimed.
The later F00344/F00345/F00346 attribute modifiers have their 433 original
instructions read in `build\scratch\status-queue\hit_attribute_modifiers.txt`;
reuse `Core::Math` wrapping arithmetic and `R3000Quotient`, including its
defined zero-divisor behavior. Keep F00412/F00414/F00426 and F00418/F00428
aliases together when grouping the subsequent stat calculators.
The perception checkpoint below is historical.

Stopped on owner request (2026-11-11) after the perception group, published
in `33ad4f3e` on base `3aa5b806` and green on the exact revision
([CI 43073456695](https://github.com/JEschete/VS-NativePort/actions/runs/43073456695);
[#276 comment](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6803558605)):

- F01638 `IsTargetOccludedAtBodyHeight`, F01639
  `IsTargetOccludedAtPointHeight`, F01640 `UpdateTargetPerception` and F01643
  `UpdatePerceivingActor` are typed in the pure `ActorPerception` module over
  `TargetPerceptionWorld` and `ActorPerceptionWorld`. `ActorPerceptionLive`
  binds them at the module addresses 0x800D87E8 and 0x800D8EF4, and
  `FrameUpdateLive` calls `UpdateLiveActorPerception`. The generated
  `Perception.cpp` keeps only three data definitions (`D_800F16F4`,
  `D_800F58D4`, `D_800F58E0`). The four old symbols had 18 code references
  and have none.
- Differential `perception`: one 852-instruction range (0x800D87E8-0x800D9534,
  SHA-256 98f4c68b...6013), 4,096 vectors, 0 mismatches and 100/100 mutants.
  The interpreter gained the GTE `GPF` instruction for the facing and distance
  arithmetic. The terrain segment test and the pass's 14 callees are stubbed
  with queued answers.
- One native broken variant per function is detected (of 1,024 vectors): F01638
  body extent plus one (172), F01639 default point height 65 (46), F01640
  marking without activity (174), F01643 detour step wrapping to 0 (60) and the
  HP rule boundary (29).
- Limits: the differential aliases the actor-record table to the battle-actor
  models, so the live binding's resolution of targets and occluders through
  `Battle_actors[id]->model` (kept for auxiliary AI targets without an
  actor-record entry) is not compared. The live bindings refuse a missing
  model, pose, link, HP or listed actor where the original read through them.
  No complete EC-04 or battle-AI/M8 retirement claim.
- Group check on base `3aa5b806` (`promotion_check.py group --focused
  "[actor-perception]"`, `build\promotion-check\group-20261111-124014`, 998 s):
  focused 54 cases and 327 assertions in Debug and Release; the full suite
  5,721 cases and 12,651,646 assertions in both configurations with 0
  failures; `silver_wolves` (2,400 frames) and `first_lever_room` (900 frames)
  match their baselines in both configurations; copier tests,
  `test_encounter_fixups.py` (211) and `test_differential` (30) pass;
  changed-file tidy 177 existing findings and 0 new. The 177 are in unchanged
  translation units (`SequenceTests.cpp`, `LayoutAsserts.cpp` and others) that
  include the changed `Distance.hpp`.

Next groups (row 28 has 10 generated functions left, all in the generated
`Behavior.cpp`, `BATTLE/BATTLE.PRG/71618.c`):

1. F01649-F01654 (`func_800D9E18`-`func_800DA738`, 71618.c:511-957): the
   pursuit, flee and follow movement choices for an action candidate.
2. F01656-F01659 (`func_800DAAD8`-`func_800DAED0`, 71618.c:958 onward): the
   retreat hostility adjustment and the per-frame action and movement decision
   within the frame budget.

Row 28 then needs its completion audit (its original IDs promoted or retired)
before row 29 starts.

No inline assembly remains in `Native`. The differential runs the original
BATTLE.PRG instructions in the repository's test-only R3000 interpreter,
[differential.py](../tools/port/differential.py), purely for behavior
comparison; it is not shipped code.

Disk usage: the working tree was about 34 GB and is about 21 GB after the
owner-approved replay cleanup on 2026-11-10.

| Path | Size | Notes |
| --- | ---: | --- |
| `build\Debug\combat-lab` | 0.47 GB | 31 kept runs (was 7.65 GB) |
| `build\Release\combat-lab` | 0.61 GB | 34 kept runs (was 6.92 GB) |
| `tools\Base` + `tools\PSX` | 9.1 GB | owner tool installs |
| `.vs` | 3.2 GB | |
| `vcpkg_installed` | 1.3 GB | |

The Combat Lab growth came from `regression.py` leaving every scripted
replay's run directory behind
([#339](https://github.com/JEschete/VS-NativePort/issues/339)). The cleanup
removed only the 1,038 uncited `regression.py` replays (frames 900 or 2,400,
speed 1, no scripts, sequence or devices) and freed 14.49 GB. It kept every
interactive, sequenced, device and cited run (those cited at the time in
`COMBAT_LAB.md` or the now-archived `HANDOFF_HISTORY.md`). Since `71973e51` (#339, closed)
`regression.py check` removes its replay run directories. Removing anything
else needs owner approval. **Never delete Ghidra, `tools\Base` or `tools\PSX`** (owner
directive, 2026-11-10).

## Promotion Throughput

The owner ruled 33-63 minutes per promoted function unacceptable (2026-11-10,
[#340](https://github.com/JEschete/VS-NativePort/issues/340)). The time went
mostly to serial per-function rounds: two full builds, both full suites, four
replays, a local CI replay and a CI wait for every function, plus a scratch
differential rewritten each time. The rules, also in
`.github\copilot-instructions.md`:

1. **Group 4-8 related functions per commit** (for example F01814-F01817).
   Each function keeps its own tests, differential and detected mutants; the
   builds, replays, ledgers, commit and CI are shared.
2. **One differential tool.**
   `python tools\port\differential.py <case> [--vectors N] [--no-build]`
   loads the original module from `Native\data\disc`, runs the case's vectors
   through the interpreter and the compiled native driver, then checks that
   each mutant (patched original words) changes a result. A case without
   mutants fails. A case's optional `MUTANT_VECTORS = N` reruns each mutant on
   only the first N vectors, which keeps large cases fast. Reports go to `build\differential\<case>.log`.
   `--replace SOURCE OLD NEW` builds a native broken variant from a patched
   copy of one source (the tree is untouched) and expects mismatches; use it
   for each function's broken-variant check instead of editing and reverting
   the source. The F01813
   case, [actor_settings.py](../tools/port/differentials/actor_settings.py),
   reproduces the recorded evidence exactly: 265 instructions, 8,192 vectors,
   0 mismatches, every mutant detected, about 33 s including the driver
   build.
3. **Scripted bookkeeping.** `python tools\port\promote.py`:
   - `show <id>`: catalog facts and the address range;
   - `scaffold <id> --case <name> --source <cpp> [--test <cpp> --tag <tag>]`:
     case, driver and Catch2 skeletons, never overwriting;
   - `register <id> --typed <name> --source <cpp> [--live <adapter>] --notes
     <text>`: `NATIVE_HELPER_HEADERS`, `OWNED_CALL_BINDINGS`,
     `MODULE_BINDING_ENTRIES`, the FIXES removal, `owned.csv` and the
     `gen_solution.py` C-header list;
   - `prune-adaptations [--dry-run]`: drops `cpp_adaptations.json` hunks the
     copier output no longer matches and re-anchors the rest;
   - `ledgers <spec.json> --summary <summary.json>`: catalog, promotion,
     verify, gate-slice, modernization and plan rows, then the gates,
     modernization and hazards sync and check.

   Each command is idempotent; a second run reports nothing to change.
4. **No duplicate local checks.**
   `python tools\test\promotion_check.py iterate --focused "<tags>"` builds
   only the Debug tests. `promotion_check.py group --focused "<tags>"` runs
   once per group:
   1. copier stability;
   2. the Debug build;
   3. `regression.py check` Debug, overlapped with the Release build;
   4. `regression.py check` Release;
   5. the focused tests in both configurations;
   6. the copier tests and changed-file tidy.

   Logs and `summary.json` go to `build\promotion-check\<mode>-<time>\`. The
   GitHub CI workflow is not replayed locally.
5. **Overlap the waiting.** Prepare the next function's analysis, case and
   tests while builds and CI run.

Result: from the tooling commit `48ab62e3` (2026-11-10 21:44) to `3aa5b806`
(2026-11-11 10:42), 11 commits promoted 54 functions in about 13 hours, about
14 minutes per function against the earlier 33-63, and every function still
has its own MIPS differential, mutants and broken variant. With the perception
group (`33ad4f3e`, 12:59) the total is 58 functions in 12 commits in about
15 hours, including writing the procedures and splitting oversized tests.
A group check takes 15-18 minutes, about 11-12 of them in changed-file tidy,
so lint first with `quality.py --files` (seconds) to avoid a second run for a
style finding. The checklist, including the test-size and ledger re-apply
rules learned on the perception group, is
[WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md).

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

Earlier handoffs, including `Archived Handoff: 2026-11-09 / f6ba998e`, are in the
[archived HANDOFF_HISTORY.md](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF_HISTORY.md).
Their current/next/pending statements are historical, not instructions or fresh verification.

## Next Work

1. Recheck `git status`, HEAD, and the exact-revision CI before resuming.
   Preserve intervening owner edits; archived results do not verify a new tree.
2. Triggered recovery, cost contracts, previews and #354 are published and green;
   #352, #353 and #354 are closed.
   Keep #337 open until its in-game reachability is established. Do not close
   #276: 747 remain.
3. Publish presentation with #356, then continue phase helpers, following
   [PROMOTION_PLAN.csv](ledgers/PROMOTION_PLAN.csv).
   F01630/F01636/F01638-F01644/F01660/F01678-F01680/F01705-F01710/F01716/
   F01717/F01719/F01721-F01751/F01793/F01810-F01818/F01846-F01849 are verified
   dependencies, not proof of remaining AI behavior or gameplay.
4. Read original source and actual assembly, then trace callers, random-state
   use, candidate-list mutation, failure and reset paths before implementing.
   Reuse the owned candidate, evaluation, stat-copy and lifecycle policies.
5. Test the deciding core first, then native/caller/module bindings. Make
   generated changes through the copier or owned registry, never by patching
   generated output. Remove only adaptations made obsolete by the owned body
   (`promote.py prune-adaptations`).
6. Follow [WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md) and the
   [Promotion Throughput](#promotion-throughput) workflow: focused
   `promotion_check.py iterate` while editing, one `promotion_check.py group`
   per 4-8 function group. Keep behavior verification, C++ promotion,
   modernization, and compatibility retirement distinct. Log a newly confirmed
   defect before fixing it.
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
& .\.venv-3\Scripts\python.exe tools\port\promote.py show F01818
& .\.venv-3\Scripts\python.exe tools\port\differential.py actor_settings
& .\.venv-3\Scripts\python.exe tools\test\promotion_check.py iterate --focused "[actor-settings]"
& .\.venv-3\Scripts\python.exe tools\test\promotion_check.py group --focused "[actor-settings]"
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
  match delivered scope. Do not declare the remaining 900 functions complete.
- Never delete Ghidra, `tools\Base` or `tools\PSX` to save space, even where
  they mirror `C:\Tools`. Delete other local data only with owner approval.
- Keep disc assets, BIOS, captures, dumps, logs, and local scratch out of commits.
  Do not force-push, rewrite history, or add attribution trailers/headers.
- Avoid full gameplay JSON, huge terminal histories, and unbounded parsers.
  Pinned clang-format is required. Header-aware tidy can fan out widely;
  use source-only focused checks between milestones, then the full required gate.
- The editor-memory detour was explicitly stopped by the owner. Workspace
  Python-analysis limits remain; Java/Gradle were not disabled. The temporary
  accessibility override was removed. Do not resume that detour as C++ work.
- The [archived HANDOFF_HISTORY.md](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF_HISTORY.md) contains old failed attempts and
  evidence paths. Consult it only for a specific question; do not reload it wholesale.
