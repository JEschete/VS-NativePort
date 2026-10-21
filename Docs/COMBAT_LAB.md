# Standalone Combat Lab

`VS.CombatLab.exe` is a live native-window encounter tool. It links the production
Engine, Content and Game libraries; it does not launch `VS.Desktop`, enter the
title/prologue, or add an option to the game menu. Removing the host project and
`Native/Tools/CombatLab` does not remove gameplay implementations. The optional
production encounter hooks are inactive in normal sessions.

This is an initial usable slice of [#16](https://github.com/JEschete/VS-NativePort/issues/16),
not completion of that issue, M8, or the ordered gameplay work in #4.

## Launch

Build using [BUILDING.md](BUILDING.md), with extracted `Native/data` available.
Run `build\Debug\VS.CombatLab.exe` (or the Release equivalent). The default opens
the encounter editor; a physical gamepad or keyboard remains active.

The menu uses the extracted SYSTEM.DAT font. Select zone, room, enemy, quantity,
Ashley tile/elevation, group offsets, spacing and RNG seed. Add multiple groups
for mixed enemy definitions; clear groups to return to the current selection.
The grid shows starting tiles over a conservative static-floor preview: green
tiles have complete floor support; dark tiles are blocked/partial, pits, missing
support or unavailable data. Invalid starts identify the actor, tile and reason.

The compact font uses the SYSTEM.DAT interface palette, including its black
outline and gray shades. The unreadable solid-glyph rendering in the original
lab checkpoint is tracked separately in [#67](https://github.com/JEschete/VS-NativePort/issues/67).
Rebuild and restart the lab to pick up that correction; no data re-extraction is needed.

- Arrows / gamepad D-pad: select rows and adjust values.
- Enter / Circle: confirm. Select **Start Encounter** to enter gameplay.
- Keyboard gameplay bindings are unchanged: arrows move; C=Circle, X=Cross,
  Z=Square, S=Triangle, Q/E=L1/R1, 1/3=L2/R2, Enter=Start, Backspace=Select.
- F6 pauses simulation; F7 advances one tick; F10 requests a production-frame
  stop and returns to the editor. Select Start again for a fresh encounter.
- Closing the window or reaching `--frames` interrupts the session; this is not
  victory or gameplay-completion evidence.

For four Silver Wolves in zone 9, room 0, use Ashley X=4/Z=9, enemy definition 4,
quantity 4, X offset -2, Z offset +3, spacing 1, elevation 0. This places wolves
at (2,12), (3,12), (4,12), (5,12). Use **Clear Groups** first if a group or preset
is already loaded: the current quantity/offsets do not replace explicit groups.
The reported spacing-2 layout at X=1,3,5,7 allocated all four actors, but X=1
has no valid floor and X=7 is blocked. See [#68](https://github.com/JEschete/VS-NativePort/issues/68).
F10 returns to the builder even while paused. Rebuild/restart to pick up the
floor-preview fix; a running debugger session does not automatically receive it.

**Save Preset** writes `build\<Config>\combat-lab\last-scenario.json`.
Load it with `--scenario <path>`; add `--start` to enter immediately. Presets record
zone/room, seed, player tile/elevation/direction and each enemy definition and
position. Enemy IDs are local to the selected zone.

### Ashley testing profiles

The **ASHLEY** row selects `prologue`, `post-prologue` or `post-minotaur`.
These are isolated testing configurations, not imported saves or claims of
completed story events. The default remains the original New Game equipment,
inventory and action set. Normal game startup is unchanged.

- **Prologue:** unchanged New Game gates.
- **Post-prologue:** the original stage-3 Map/Score menu gates.
- **Post-Minotaur:** those menu gates plus three mapped chain abilities, three
  defenses, one learned spell from each real spell-class table, and the first
  Break Art for the equipped weapon family. This is the owner-selected
  representative set, not an all-actions or all-ranks unlock.

Profiles retain the real weapon, resource, status and target restrictions.
In particular, draw the weapon before selecting a Break Art; its menu entry
is unavailable with the weapon sheathed. These profiles do not implement a
loadout editor or complete the wider spell/ability/equipment trackers.
Enabling a menu gate does not repair that submenu: the known Score crash
remains tracked by #65.

Presets and encounter logs include `ashley_profile`. Existing version-1 presets
without that key still select `prologue`; unknown profile names are rejected.
F10 followed by Start initializes a fresh player. Changing the ASHLEY row back
to Prologue also removes the advanced profile's grants on the next start.

Other options: `--data <directory>`, `--frames <ticks>`, `--speed <1..64>`,
`--capture <bmp-path>`, `--catalog-only`, `--scripted` (ignore gameplay devices),
and `--press <tick:Button[,tick:Button...]>`. A scripted press lasts two ticks;
`+` combines buttons. Host function keys remain available during scripted runs.

## Production contracts and current limits

- Rooms and enemy stats come from the extracted ZND/MPD definitions. Each accepted
  enemy also needs an independent hostile MPD placement profile in that zone.
  Its AI, hostility, drops and variant fields are preserved. Unavailable entries
  carry a reason; enumerating them is not a claim they work.
- Primary actor slots 2–9 and auxiliary-body reservations follow production rules.
  Validation rejects unsupported encodings, unavailable assets, overlaps and
	exhausted slots. Static-floor preflight uses the production collision decoder
  on a private copy of room data, rejecting blocked/partial tiles, pits and any
  invalid floor sample. Partial tiles and dynamic platform/gimmick support are
  conservatively excluded. This does **not** establish ceiling clearance,
  suitable elevations, large-enemy footprints, paths or safe movement in every
  room. Positions/counts are never silently adjusted to pass validation.
- Source-backed enemies are currently **same-zone only**. Script-dependent actors,
  cross-zone model loading and full roster verification remain open.
- Ashley retains the real new-game equipment and inventory; available actions
  depend on the explicit testing profile above. There is no full loadout editor,
  all-items grant, spell/rank unlock editor or
  ability coverage matrix yet. Items/spells/abilities that are available use the
  original menus and gameplay path; unavailable actions are not fabricated.
- Story and door events are explicitly isolated. Advanced profiles set the
  documented startup menu stage only; no room event is completed to obtain it.
  This mode does not verify prologue combat or story encounters.
- Audio/effect/AI stubs still exist. Defeat/rewards, chains, Break Arts, all items,
  spells and ranks, equipment combinations, and original-game parity are unverified.
  An idle enemy, clean exit or passing unit suite is not acceptance of those features.

## Extensive diagnostics

Every process creates `build\<Config>\combat-lab\runs\<timestamp>-<suffix>\`:

- `session.log`: engine output at Trace level, all channels.
- `events.jsonl`: ordered engine logs, raw RNG draws/seeds, production action
  results, host controls, and per-tick input/actor/action/inventory snapshots.
- `catalog.json`: zone-local room/enemy availability and spawn provenance.
- `manifest.json`: source revision, dirty worktree, data path and host options
  (interactive runs); `encounter-N.json`: each submitted encounter definition.

File logging is automatic, including option-parsing and content-startup errors;
no console redirection or debugger is required. The startup log records the run
directory. The console shows warnings/errors; the files retain Trace-level
engine messages and RNG records. Engine file writes are batched and flushed at
frame/control boundaries, with immediate error flushing. The host restores the
previous logging policies on teardown; other hosts retain their prior defaults.

Structured writes are flushed and failures terminate the run rather than silently
losing trace data. A frame snapshot includes HP/MP, risk, statuses, actor commands,
positions, recovery, weapon state, action/target state and item counts. It also
records spawn IDs, defeat state, awareness markers, AI candidates/destinations,
and pending miscellaneous loot by source actor. Pending loot is not proof of
inventory delivery; `rewards_available` records the original progression gate.
`performance.summary` contains bounded phase and presentation-interval aggregates,
including counts above the 16.667 ms tick and 33.333 ms frame budgets. Logs are
local evidence, not a complete formula/AI decision explanation or a replay restore
format. Content hashes, a trace browser and persistent coverage reporting remain
future work. Full tracing can create large files; no rotation is implemented.

Crash stacks go to the session logs. The existing bounded-run watchdog writes
`build\<Config>\vs-native-hang.log`. Keep captures, extracted catalogs and game data
out of commits and issue attachments.

## Combined combat/action checkpoint (2026-10-21)

Source: `fd291cfe29b45656e00ff1e198f53e7730b59a19` plus pending native
AI/navigation, motion, Lab profiles/diagnostics and effect fixes. These are
dirty-worktree results. Fixing revisions and successful pushed CI are recorded
on #69, #71, #72, #73 and #74 separately. #6, #16 and #42 remain broader work.
All captures, presets and runtime logs below remain local.

Both Debug and Release full builds and suites pass **302 cases / 828486
assertions**. Logs: `build/quality/issues69-74-{final-<Config>-build,full-<Config>-tests}.log`.
The focused profile, awareness, particle, emission-capacity and renderer-storage
selection passes 15 cases / 1006 assertions. Two copier/scaffolder passes preserve
all 70 owned paths and 775 generated/project paths; the manifest SHA-256 is
`b5674a43cfef9ce954ca16a7c65de6ba3e7e943b536bbe5e4ea0d8a0af0aec32`.
Native views: 621 fields / zero mismatches; event dispatch: 179 / zero errors.
Assembly-gap audit and encounter-fixup regressions pass 13 and 3 tests.
The final `quality-native.cmd --changed --tidy` gate passes with **zero new
findings**, retaining 316 existing findings and no baseline edits. Full output:
`build/quality/issues69-74-full-quality.log`.

### Real action and visual evidence

Every replay here uses `--start --scripted --speed 1`, ordinary controller
presses and real data. HP, RNG and encounter completion are not forced.
The advanced scenario is zone 9, room 0, seed 1, Ashley (4,9), Silver Wolf
definition 4 at (4,10), with `ashley_profile: post-minotaur`.

| Action | Presses | Frames | Debug / Release run IDs |
| --- | --- | --- | --- |
| Solid Shock | `140:Triangle,220:Circle,300:Circle,380:Circle,460:Circle,540:Circle` | 1500 | `1792567997566581-0` / `1792568676890936-0` |
| Equipped Break Art | `140:Circle,330:Triangle,410:Down,490:Circle,570:Circle,650:Circle,730:Circle` | 2000 | `1792568357154255-0` / `1792568709218612-0` |
| Heavy Shot chain | `140:Circle,180:Circle,220:Circle,272:Circle` | 1000 | `1792568624284307-0` / `1792568749832573-0` |
| Cure Root | `760:L2,761:L2,762:L2+Up,763:L2+Up,764:L2,900:Circle,1050:Circle` | 1340 | `1792568398043471-0` / `1792568768749218-0` |

Each Debug/Release pair matches every frame snapshot and RNG record: spell
1500/2275, art 2000/5657, chain 1000/1336, item 1340/3423, respectively.
The final four-wolf pair also matches all 600 snapshots and 1773 RNG records.

Solid Shock action 97 costs 25 MP, deals 92 HP at tick 724 and returns control.
The equipped art 188 costs 25 HP, deals 63 at tick 916 and returns control before
later enemy attacks. A basic attack deals 45 at tick 270; the real timed Circle
press chains into Heavy Shot action 22, dealing 31 at tick 358. This verifies
representative actions, not every granted defense/spell/art or reference parity.

The item scenario uses the default prologue profile and adjacent Mandel
definition 0 at (4,10). Real damage occurs first; Cure Root count goes 10 to 9,
and action 224 produces a 49-HP recovery result at tick 1318, capped from 208
to 250 HP. Captures `build/quality/issue72-item-particles-{debug,release}.png`
at tick 1340 visibly contain the original effect-program particles. The native
renderer advances actual texture programs/curves and emits bounded native
ordering packets; it does not substitute a fabricated flash.

Testing exposed two additional production defects: a full particle pool let
F01515 dereference an uninitialized first node (#73), and the incomplete
G00574 renderer-table declaration allocated one callback instead of eight (#74).
The generator now handles zero/full-capacity emission explicitly and preserves
all eight native callbacks without overwriting the adjacent effect package.
Regressions cover empty/full/clamped bursts, unchanged RNG on skipped emission,
all eight slots and every original plugin-index slice.

The awareness repair uses native packet handles and the native frame matrix
rather than the original scratch offset. All six marker-frame variants have
high-address traversal/projection tests. Natural four-wolf markers were captured
at ticks 144 and 164 in Release runs `1792568808686727-0` and
`1792568798455539-0`; Debug evidence includes `issue71-awareness-speed1.png`.

Live F6/F7/F10, advanced restart and a third start after cycling back to Prologue
pass in Debug `1792571153025350-0` and Release `1792571186843537-0`.
Both wait past tick 700 before pausing: natural marker 4 and actual Ashley
damage precede teardown, and the restarted encounter has cleared markers.
The final menu captures `issue69-<Config>-profile-reset.png` show the original
limited menu again, without advanced actions or Score. Both runs exit zero with
`trace_failed: false`. Exact control ticks: `issue69-<Config>-lifecycle.json`.

### Normal-speed performance limits (#70)

The owner subsequently verified that stutter is solved and live combat feels
natural; **#70 is closed**. The measurements and rejected approaches below are
retained as diagnostic history, not an outstanding stutter blocker.

Full tracing remains enabled. In the earlier matched four-wolf 600-tick A/B,
console filtering/frame-boundary flushing reduced simulation's worst sample
from 31.131 to 21.710 ms; all 600 snapshots and RNG records matched. Startup
plus replay wall time changed from 18.227 to 12.378 seconds. This is not a
claim that all combat stutter is solved.

Final four-wolf measurements:

| Configuration / run | Simulation mean / maximum | Maximum present interval | Intervals >33.333 ms |
| --- | --- | --- | --- |
| Debug `1792568914935697-0` | 7.315 / 22.435 ms | 26.057 ms | 0 |
| Release `1792569400494834-0` | 1.910 / 4.863 ms | 26.526 ms | 0 |

Debug still has 41/600 simulation samples over 16.667 ms. Release has zero.
No severe stall was reproduced in these final bounded runs; this does not
establish an all-encounter performance guarantee.

An attempted redraw gate, with both coarse and precise Windows sleeps, reduced
presentation work but worsened maximum intervals to 39.640/37.196 ms, with
10/7 intervals over 33.333 ms. Those changes were **rejected**; original
presentation scheduling remains. The experiments preserved all 600 actor/input
snapshots and 1773 RNG records but not capture bytes, further limiting the
experiment's validity. Runs: `1792568974185681-0`, `1792569084718367-0`.
These failed approaches remain recorded to avoid repeating them. They were not
shipped and are not needed for the owner's verified natural live combat.

### Shared special attacks and current effect defects

Enemy specials and magic use a shared, data-driven pipeline, not one complete
spell implementation per enemy:

1. Enemy stats provide action slots; shared
   [AI action selection](../Native/Game/src/Battle/Ai/ActionSelection.c) evaluates
   their IDs, ranges and availability.
2. The 256-entry [action table](../Native/Game/include/Battle/Action.h) supplies
   type/category, costs, ranges/area shape, recovery, hit/stat-calculator IDs and
   the visual `effectId`.
3. The shared [action flow](../Native/Game/src/Battle/ActionFlow.c) coordinates
   targets/results, the actor's animation/assets and its effect request.
4. [Effect resource loading](../Native/Game/src/Effects/EffectResources.c) binds
   the effect package/textures and optional renderer callbacks. The shared
   script VM and [timeline](../Native/Game/src/Effects/Timeline.c) drive particles,
   child effects, sound, colors, actor signals and impact timing.
5. Generic emitters and [particle rendering](../Native/Game/src/Effects/ParticleRender.cpp)
   coexist with effect-specific native plugins/templates. Specialized visual
   geometry is usually selected by effect, not hardcoded to an enemy name.

Correct skeletal/action animations do not establish correct particle source/
target transforms, velocity/force scaling, attachments or child-effect state.
Those stages require independent behavioral/reference verification.

The owner's Hellhound report is reproduced as **#75** on `df748add`, Release,
speed 1, without gameplay inputs or HP/progression edits. Zone 11, room 0,
seed 1, Prologue Ashley (8,12), eight Hellhounds (definition 3) at (5..12,13).
Action 147 selects effect 94 (`E094`). At tick 482, the shared
`Effects_TickChildEffectTrack` (F01563, Timeline.c:168) faults reading address
`0x2` while evaluating `Effects_package->request->count`. At initial triage its
producer was unknown; the later shared binding repair is documented below.
Owner evidence: `build/Release/combat-lab/runs/1792572817191067-0`;
reproduction: `1792573135209075-0`, with stderr in
`build/quality/hellhound-owner-repro-stderr.log`.

The separately reported particles flying everywhere are **#76**. They need an
aligned source/reference spatial comparison; visible original-program particles
are not proof that their placement/direction matches the game. No cause or fix
is claimed yet. Keep captures and extracted data local.

### Later playtest repairs and remaining limits (2026-10-21)

The owner confirms ground-enemy movement/rotation are solved and the prologue
is fully playable apart from the reported visual/audio/combat gaps. This does
not establish every enemy, ability, or reference-parity case.

**Hellhound #75:** the original request-pointer symbol at `0x800F5798` aliases
the field at `0xD0` in the primary effect package at `0x800F56C8`. Independent
native storage left the package request null. The ledger now emits a native
field view, preserving initialization, reads/writes and native module bindings.
The final-child gate also reads the native `childCount` field instead of original
offset `0x14`. The binding regression fails four checks before repair and passes
all seven afterward; a separate child-track test checks outstanding children.

The original eight-Hellhound preset from
`build/Release/combat-lab/runs/1792572817191067-0/encounter-1.json` is replayed
with `--start --scripted --frames 1600 --speed 1`. Debug
`1792577241138340-0` and Release `1792577275397935-0` match all 1600 snapshots
and 6586 RNG records. Real breath damage occurs on four Ashley limbs at tick
534 (24/28/20/27 HP), control returns at 664, and breath executes again at 1572.
This does not verify every particle's spatial fidelity or the unspecified
D'tok report.

**Post-defeat restart #79:** the actual one-Minotaur scenario at zone 12/room 0,
Ashley (9,10), definition 1 at (9,11), produces unedited player defeat at tick
658. Before repair, run `1792578016560816-0` starts the next healthy player,
then changes `game_over` to true/menu 7 and disables field rendering because
the old event thread resumes. Gameplay event initialization now makes playback
inactive and resets start/finished flags, thread cursors and wait timers.
It does not suppress real defeat or fabricate progression.

After repair, actual death, pause/single-step/F10 and same-room restart pass:
Debug `1792578734686541-0`, Release `1792578765217479-0`. Both keep fresh HP
250, menu 63, `game_over: false` and field rendering throughout their ready
observation window. A subsequent controller-selected change to zone 9 also
passes: `1792580129571764-0` / `1792580170367914-0`.
Local evidence: `build/quality/issue79-<Config>-post-defeat.png`,
`issue79-<Config>-different-room.png` and lifecycle JSON. The session-workspace
`verify_lab_lifecycle.ps1` uses `-WaitForDefeat`, the persisted scenario above,
and optionally `-DifferentZone 9`; no HP or memory writes are used.

Read-only snapshots now persist `game_over`, `menu_state`,
`field_rendering_enabled` and `frame_buffer`, so healthy player initialization
is distinguishable from a stale menu/event taking over afterward.

**Bat flight #77:** the failing lowest-reachable-ground query used the packed
tile word as X, so `0x400D0008` reached the terrain guard as X=`1074593800`.
The owned fix uses distinct tile X/Z and direction-delta values. The assembly-
backed 16-sample segment-height and nullable room-ceiling helpers are now native.
Focused tests cover valid and edge flight tiles, missing height maps, segment
sampling and an absent room context; the existing strict terrain accessor still
logs and aborts rather than returning a fabricated height.

The original one-Bat and eight-Bat scenarios were replayed through production
Combat Lab at speed 1 with no player input. Debug runs
`1792584773462606-0` / `1792584773457949-0` and Release runs
`1792584856381806-0` / `1792584856370509-0` each complete 5000 frames. Bats
select Ashley and move; actual damage reaches Ashley HP 0 and Game Over, and
the original terrain abort is absent. This resolves the reproduced Bat path
locally, pending exact-revision CI and issue update; it does not establish all
flight-enemy coverage.

Other remaining defects: #76 covers the birds/window rays/particle placement
reports; #78 confirms a separate fresh eight-Minotaur render-cursor corruption
at tick 209; #80 tracks the broader multi-part hypothesis and #81 the missing
selected-part glow. Sound remains #7 and first-entry Equip weapon preview
remains #14. Bosses lacking an independent hostile spawn profile remain
unsupported under #16; validation is not bypassed. D'tok and the additional
multi-part cases lack exact reproduction details, so no damage/targeting
assumption is made.

The #78 temporary stage trace (`1792579333054718-0`) locates the corrupted
primitive cursor before room drawing, not within minimap emission. Its write
producer remains unknown; temporary probes were removed. A data-write
watchpoint is the next concrete observation, not a guessed buffer enlargement
or actor-count reduction.

Stopping verification for these shared-runtime changes:

- Full Debug/Release solution builds pass; each suite passes **306 cases /
  828512 assertions**. Exact logs:
  `build/quality/playtest-stopping-<Config>-{build,tests}.log`.
- `quality-native.cmd --files` for the changed owned producers, trace/tests and
  copier fixups, with `--tidy`, reports **zero findings / zero new findings**:
  `playtest-stopping-focused-quality.log`. The pushed source-wide/header gate
  must still be confirmed for the exact fixing revision before issue closure.
- Native view fields: 621 / zero mismatches; event dispatch: 179 / zero errors.
  Assembly-audit and encounter-fixup regressions pass 13 and 3 tests.
- Two copier/scaffolder runs preserve 70 owned paths and 775 generated/project
  paths. Manifest SHA-256:
  `66d89e88c9cf0ff16d90d68f6aaac5db4e94ddccf60ec2583bc6a866dcb6c5f1`.
  Details: `playtest-stopping-generation-stability.json` and generation logs.

These runs use the `df748add` plus scoped dirty worktree recorded in their
manifests. Publication and exact CI evidence belong on #75/#79. Unrelated
instruction/reference-tree changes are preserved. No full-particle-parity,
flight-roster, high-count Minotaur, audio or equipment-preview completion is
claimed.

### Pending AI/navigation evidence

The same worktree implements native ground queries, bounded occupancy/detours,
movement behaviors, cleanup and five actor motion-request producers. Earlier
live Zombie runs `1792564049493217-0` (Debug) and `1792564092031473-0` (Release)
demonstrated real defeat/removal and two queued Cure Roots at tick 1350; all
5000 snapshots and 4484 RNG records matched. Inventory was deliberately not
credited through the disabled prologue reward gate.
Those runs and the ordinary 24000-tick guard victories predate the newest
particle fixes; they are not fresh all-gameplay verification of this checkpoint.
The gap report now retains 50 C-backed, 31 assembly-only and 10 unlocated gaps.
Ownership, behavior verification, C++ promotion and compatibility retirement
remain separate; no wider issue is closed just from those counts.

## Verified local checkpoint (2026-10-20)

Source: `69a886187649ef663b3c05d3e3b2d11bd2a0426e` plus the lab changes and
pre-existing dirty AI work. These are **dirty-worktree results**, not proof of a
later commit or all-content completion. Logs/captures are under
`build/quality/combat-lab/`; run directories below are executable-relative.

- Release workspace build and documented Debug x64 MSBuild succeed. Full suites
  in each configuration: **271 cases, 827297 assertions** (`tests-*-final.log/xml`).
  Test Explorer discovered no tests; direct `VS.Tests.exe` console/JUnit reporters
  were used. Existing copied-C warnings are not claimed fixed.
- `python tools\port\test_encounter_fixups.py`: three passing regressions,
  including actual generated item-label and reload initializer bytes.
- Scoped `quality-native.cmd --files ... --tidy`: zero new findings, no baseline
  changes. Full selection/output: `quality-final.log` and `quality-final.json`.
- Two consecutive copier/scaffolder runs preserve manifest SHA-256
  `2C0012B56FF44B421B8995FBDEB386F7E019E3565F781243A2BAB722EF31CB00`,
  all 66 registered owned paths and generated projects. See
  `generation-stability.json` and `generation-stability-*.log`.
- Two-enemy encounter: zone 9, room 0; Ashley tile (4,9), enemy definition 0 at
  (4,10) and (4,11), elevation/direction 0, seed 1. Run with `--start --frames 1500
  --speed 4 --scripted --press 140:Circle,220:Circle,280:Circle,800:Circle,880:Circle`.
  Player hit for 16, enemies retaliated, and Ashley ended at 89 HP. All 1500 frame
  snapshots matched between Debug run `1792557472547405-0` and Release run
  `1792557504705649-0`. No HP or event flags were edited.
- One-enemy item replay: same setup without the second enemy, `--frames 2000
  --speed 4 --scripted --press
  1000:L2,1001:L2,1002:L2+Up,1003:L2+Up,1004:L2,1250:Circle,1400:Circle`.
  Release run `1792557402453352-0`: enemy attacks reduced Ashley to 209 HP;
  Cure Root count fell 10 to 9; action 224 applied 50 recovery at tick 1668,
  restoring HP to 250 before further enemy attacks. `item-healing.*` captures
  this evidence. This verifies one item, not every item/effect.
- Live Release run `1792557652836988-0`: actual F6/F7/F10 after readiness paused
  at tick 126, stepped to 127, stopped at 138 and restarted at 307. Ready at 354:
  Ashley 250 HP/50 MP, enemy 146 HP/13 MP. Earlier attempts sent keys before
  readiness and are not counted. Logs: `restart-ready.*`.

The lab exposed two entry/menu defects tracked on #16: omitted player-settings
initialization caused an item-menu hang, and raw ASCII G00917 (`MISC`) caused
unbounded glyph packets to overwrite room geometry. Shared defaults and the
F02553 generator fix repair those causes, including overlay-reset bytes. No
collision guard or fabricated item result was used. Temporary watchpoints are removed.

## Isolated publication verification (2026-10-20)

The lab-only staged changes were built in the detached validation worktree
`build/quality/combat-lab/publish-check` on `69a88618`, excluding all unrelated
AI edits. This is the publication acceptance evidence; the larger test counts
and different battle outcomes above belong only to the combined dirty worktree.
No full-game coverage or reference parity is implied.

- Debug and Release x64 solution builds succeeded using the documented MSBuild
  command with `/p:VcpkgManifestInstall=false` to reuse installed dependencies.
  Full suites: **239 cases, 827068 assertions** in each configuration.
  Logs/XML: `build/quality/combat-lab/publish-{build,tests}-{debug,release}.*`.
- Full CI-equivalent `quality-native.cmd`: zero new findings, no baseline changes
  (`publish-quality.log`). Three generator regressions pass and now run in CI.
  Staged hashes for the four regenerated files match the staged copy manifest.
  Project files were generated without the unrelated AI additions. Full copier
  regeneration in the detached environment was not used for publication because
  local source-index/binary metadata differed; it produced unrelated module-table
  changes. The reviewed four-file output from the earlier successful copier runs
  was retained, not hand-edited.
- The same two-enemy input schedule above ran 1500 ticks in Release run
  `1792558354743674-0`: player damage and enemy retaliation were written to both
  `session.log` (221590 bytes) and `events.jsonl` (1836248 bytes). Every structured
  combat-result message was found in the text log; orderly trace finalization
  reported no write failure.
- The same item schedule ran 2000 ticks in Release run `1792558415561275-0`:
  enemy attacks reduced HP to 162; item 323 count decreased 10 to 9; action 224
  applied 48 recovery at tick 1668, raising HP to 210 before another enemy hit.
  Outcome differences from the combined worktree are not normalized away.
- Release run `1792558470287103-0`: actual F6/F7/F10 paused at 125, stepped to
  126, returned to the editor at 138 and restarted at 305 with initial HP/MP.
  These run directories are under the validation worktree's
  `build/Release/combat-lab/runs`; console evidence is `publish-{combat,item,lifecycle}.*`.
- `VS.CombatLab.exe --speed invalid` now writes the fatal parse error to both
  files before displaying the error dialog. Verified normal-worktree Release run
  `1792558050667135-0`; focused file-log/encounter tests pass 105 assertions in
  12 cases. Files are readable while the sink is still open, proving flush behavior.

Publication revision and pushed CI results are recorded on #16. That issue stays
open for the remaining roster, loadout, spell/ability and gameplay audit work.

## Static-floor placement verification (#68, 2026-10-21)

Source: `0a074f32` plus the scoped floor fix in
`build/quality/issue68/publish-check`, excluding unrelated dirty AI changes.
The source MPD loader replaces the on-disk profile count with width*height;
catalog parsing does the same only on its private copy before reusing the
checked production sampler. Normal gameplay retains its existing sampler API.

- Isolated Debug/Release x64 full solution builds pass. Full suites pass
  **248 cases / 827175 assertions** each, including five new floor/validation
  cases. Tests cover truncated data, invalid profile references, blocked/partial
  tiles, pits, invalid edge samples, valid raised floors, input preservation and
  actor-specific errors. Commands: documented MSBuild with
  `/p:VcpkgManifestInstall=false`, then `VS.Tests.exe` with console/JUnit reporters.
  Test Explorer did not discover these tests; the Catch2 executable was used.
- Scoped `quality-native.cmd --files` with CollisionSampling.c,
  EncounterCatalog.cpp, Editor.cpp and EncounterTests.cpp plus `--tidy` reports
  zero new findings. No quality baselines or generated files were changed.
- Exact local inputs and outputs are under `build/quality/issue68/`:
  `unsafe-wolves.json`, `four-wolves.json`, `verify-ui.ps1`, `ui-results.json`,
  build/test logs, quality reports and captures. Replay arguments:
  `--data <Native/data> --scenario <preset> --start --frames 650 --speed 4
  --scripted --capture <bmp>`. The unsafe preset returns 2 before spawning;
  its errors identify enemy 1 at (1,12) as missing floor and enemy 4 at (7,12)
  as blocked. The safe layout starts all four enemy slots at Y=0.
- Final Debug/Release replays each retain four supported enemies through all
  599 ready frames. Safe runs: Release `1792561333604857-0`, Debug
  `1792561348725539-0`; unsafe Debug run `1792561341006280-0` has no ready frame.
  The preview captures are identical (SHA-256
  `ebedb02e15c5c07db484d292ef2481c42c40eb153e623e3a909e1dd93e8dad72`);
  `preview-check.json` verifies green floor versus dark unsupported tiles.
- Actual native-window F6/F10 and editor keys in Release run
  `1792561359130756-0` pause after readiness at tick 57, return at 68, restart
  at 221 and return again at 368. Both encounters begin with all four enemies
  at Y=0. The first automation attempt lacked Windows scan codes and paused
  before readiness; it is not counted as lifecycle acceptance. The corrected
  local script includes scan codes and waits for a ready room.

These are static placement checks, not proof of Silver Wolf AI/combat completion
or full room/roster coverage. Keep logs/captures local. Pushed revision and CI
evidence belong on #68; the broader #16 remains open.
