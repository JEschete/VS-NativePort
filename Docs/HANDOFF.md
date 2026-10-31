# Current handoff

Updated 2026-10-30. This is a short entry point, not another work tracker.
Detailed checkpoint evidence, commands and replay paths are preserved in
[HANDOFF_HISTORY.md](HANDOFF_HISTORY.md). Historical results are not fresh
verification of the current worktree.

## Work order and current state

### Latest source and next observation

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
