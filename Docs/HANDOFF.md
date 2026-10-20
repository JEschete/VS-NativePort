# Handoff: prologue entry and shared rendering

## Current checkpoint

The ordered work tracker is [#4](https://github.com/JEschete/VS-NativePort/issues/4).
The maintained-production quality pass in [#5](https://github.com/JEschete/VS-NativePort/issues/5)
is verified locally. The Guildenstern textbox fix in
[#1](https://github.com/JEschete/VS-NativePort/issues/1) is now verified locally.
Menu issue #2 remains open: later owner comparisons identify additional text,
equipment/Status, Map/Data and Options gaps. The checkpoints below verify their
specific fixes, not complete menu parity. #6 remains blocked on #2 acceptance
and successful CI for its fixing revisions.
Preserve the original dialogue pause and button advance; do not shorten the scene delay.

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
