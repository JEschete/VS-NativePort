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
The grid shows starting tiles, not a collision or height-map preview.

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

**Save Preset** writes `build\<Config>\combat-lab\last-scenario.json`.
Load it with `--scenario <path>`; add `--start` to enter immediately. Presets record
zone/room, seed, player tile/elevation/direction and each enemy definition and
position. Enemy IDs are local to the selected zone.

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
  exhausted slots. It does **not** yet establish walkability, suitable elevations,
  large-enemy footprints or safe placement in every room.
- Source-backed enemies are currently **same-zone only**. Script-dependent actors,
  cross-zone model loading and full roster verification remain open.
- Ashley starts with the real new-game equipment, inventory and available actions.
  There is no full loadout editor, all-items grant, spell/rank unlock editor or
  ability coverage matrix yet. Items/spells/abilities that are available use the
  original menus and gameplay path; unavailable actions are not fabricated.
- Story and door events are explicitly isolated. No progression flags are marked
  complete. This mode does not verify prologue combat or story encounters.
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
directory. Both engine messages and structured events are flushed as written.

Structured writes are flushed and failures terminate the run rather than silently
losing trace data. A frame snapshot includes HP/MP, risk, statuses, actor commands,
positions, recovery, weapon state, action/target state and item counts. Logs are
local evidence, not a complete formula/AI decision explanation or a replay restore
format. Content hashes, a trace browser and persistent coverage reporting remain
future work. Full tracing can create large files; no rotation is implemented.

Crash stacks go to the session logs. The existing bounded-run watchdog writes
`build\<Config>\vs-native-hang.log`. Keep captures, extracted catalogs and game data
out of commits and issue attachments.

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
