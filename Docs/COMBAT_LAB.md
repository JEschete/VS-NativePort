# Standalone Combat Lab

`VS.CombatLab.exe` is a live, native-window encounter tool. It links the production Engine, Content and Game libraries,
so encounters run through the production gameplay code. It does not launch `VS.Desktop`, enter the title or prologue,
or add an option to the game menu. The optional production encounter hooks it uses are inactive in normal game
sessions. Removing the host project (`Native/Tools/CombatLab`) does not remove any gameplay implementation.

The Lab is part of [#16](https://github.com/JEschete/VS-NativePort/issues/16). It is not completion of that issue, M8
or the ordered gameplay work in [#4](https://github.com/JEschete/VS-NativePort/issues/4).

## History

The per-issue checkpoint, repair and acceptance records that used to be in this file are preserved in the last full
version: https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/COMBAT_LAB.md .
Ledger notes that cite a "COMBAT_LAB.md ... checkpoint at <commit>" refer to that version.

## Launch

Build the solution and extract the game data to `Native\data` as described in [BUILDING.md](BUILDING.md). Then run
`build\Debug\VS.CombatLab.exe` or `build\Release\VS.CombatLab.exe`. With no options the Lab opens the encounter
editor; the keyboard and a connected gamepad both work. The Lab searches upward from the executable folder for
`Native\data`; pass `--data <dir>` if it cannot find it.

```powershell
build\Debug\VS.CombatLab.exe
build\Debug\VS.CombatLab.exe --scenario tools\test\scenarios\silver_wolves.json --start
build\Release\VS.CombatLab.exe --scenario my-preset.json --start --frames 3600 --speed 8 --scripted
```

Exit codes: `0` normal; `1` invalid options, missing data or another fatal error (runs without `--scripted`,
`--sequence`, `--catalog-only` or `--door-transitions` also show an error box); `2` a rejected `--start` encounter,
a sequence script or door case that did not pass, or a cancelled sequence.

### Saves

The Lab keeps native saves in `combat-lab-saves` inside SDL's per-user folder (on Windows,
`%APPDATA%\VS-NativePort\VagrantStory\`). This is separate from the Desktop game's `saves` folder.
Pass `--save-dir <dir>` to use another folder, for example an isolated root for automated runs or a deliberate
cross-host test. Do not point automated tests at the owner's save files.

Save, Load and Continue show three original-style file cards. Up/Down scrolls slots 00-99, Circle opens the
confirmation and Cross goes back; the vertical Yes/No defaults to No. Normal Save still needs an enabled original save
point. Native saves are not PS1 memory-card images; import and migration are tracked in
[#18](https://github.com/JEschete/VS-NativePort/issues/18).

## Command-line options

Ticks are logical game ticks (1/60 s). Unknown options and invalid values are errors.

| Option | Description |
|---|---|
| `--data <dir>` | Extracted data root. Default: the first `Native\data` found above the executable folder. |
| `--scenario <preset.json>` | Load an encounter preset into the editor. |
| `--start` | Start the loaded encounter at once instead of waiting in the editor. |
| `--frames <ticks>` | Stop after this many ticks. In door mode this is the per-case budget (default 3600). |
| `--speed <1..64>` | Simulation ticks per real tick. Default 1. Use 1 for listening and timing checks. |
| `--press <tick:Button[+Button][,...]>` | Scripted presses, each held for two ticks. `+` combines buttons. Repeatable. |
| `--scripted` | Ignore keyboard and gamepad game input so nobody can disturb the run. Host F-keys still work. |
| `--capture <file.bmp>` | Write the last presented frame to a BMP file on exit. |
| `--save-dir <dir>` | Use this folder for native saves instead of `combat-lab-saves`. |
| `--sequence <manifest.json>` | Run several presets in one window. See [Script sequences](#script-sequences). |
| `--catalog-only` | Load data, write `catalog.json` to the run folder and exit without opening a window. |
| `--scene-catalog` | With `--catalog-only`, also write `scene-inventory.json`. |
| `--door-transitions` | Run directed-door tests. With `--catalog-only`, only write the door inventory and plan. |
| `--door-case <id>` | Door mode: run one case, for example `z009-r005-d001`. |
| `--door-from <id>` | Door mode: start at this case and continue in inventory order. |
| `--door-limit <n>` | Door mode: run at most `n` cases. |
| `--door-resume <sequence-results.json>` | Door mode: keep valid passes from an earlier run's results. |
| `--verification-log` | Use the lighter verification log profile. See [Logging](#logging). |
| `--player-observations` | Keep full per-tick observation frames for the external test player. Normally passed by its runner. |
| `--fixed-frame-ticks <1..262>` | Advance the frame-counter clock by a fixed step per read, so repeated runs match. |
| `--trace-effect-paths <first:last>` | Log effect and particle paths for an inclusive tick range. Needs `--speed 1`. |

Button names for `--press` are `Up Down Left Right Cross Circle Square Triangle L1 R1 L2 R2 L3 R3 Start Select`.
`--fixed-frame-ticks` is explained in [MODERNIZATION.md](MODERNIZATION.md). The external test player is described in
[Native/Player/README.md](../Native/Player/README.md).

- `--sequence` cannot be combined with `--scenario`, `--start`, `--frames`, `--press` or `--catalog-only`.
- `--door-case`, `--door-from`, `--door-limit` and `--door-resume` need `--door-transitions`. Door mode cannot be
  combined with `--scenario`, `--sequence`, `--start` or `--press`, or with both `--door-case` and `--door-from`.
  Door mode always ignores keyboard and gamepad game input.
- `--scene-catalog` needs `--catalog-only`. `--trace-effect-paths` needs `--speed 1` and the default full logging.

## Encounter editor

The editor uses the extracted SYSTEM.DAT font. Up/Down selects a row, Left/Right changes it, and Enter or Circle
activates it (on value rows it also steps the value). Main rows:

| Row | Use |
|---|---|
| ZONE, ROOM | Zone (default 9) and room within it. |
| ENEMY | Enemy definition. IDs are local to the zone. |
| QUANTITY | 0 (SOLO) to 8 enemies in the current group. |
| ASHLEY TILE X, ASHLEY TILE Z, ELEVATION | Ashley's start tile and elevation. |
| GROUP OFFSET X, GROUP OFFSET Z, LINE SPACING | The first enemy starts at Ashley's tile plus the offset; the rest follow along X, LINE SPACING tiles apart. |
| RANDOM SEED | RNG seed for the encounter. |
| ASHLEY | Testing profile. See [Ashley testing profiles](#ashley-testing-profiles). |
| ASHLEY SETUP | Opens the stat and weapon overrides. |
| ADD GROUP, CLEAR GROUPS | Add the current group to mix definitions, or clear explicit groups. |
| START ENCOUNTER | Validate the setup and enter gameplay. |
| SAVE PRESET | Write `build\<Config>\combat-lab\last-scenario.json`. |

- Set QUANTITY to 0 and clear groups to enter with Ashley alone. Loaded presets become explicit groups; use CLEAR
  GROUPS first, because quantity and offsets do not replace them.
- The grid preview shows start tiles over a conservative static-floor check. Green tiles have full floor support;
  dark tiles are blocked, partial, pits or missing data. A rejected start names the actor, tile and reason.
- Production-room and external-event presets lock the group rows; the labels read ORIGINAL ROOM or SCRIPT-OWNED.
  Facing direction, room-event policy, external events and progression fields have no editor row; set them in JSON.

### Ashley setup

Rows: MAX / STARTING HP, MAX / STARTING MP, BASE STRENGTH, BASE INTELLIGENCE, BASE AGILITY, WEAPON,
RESET ALL TO DEFAULT and BACK TO SETUP.

- Each stat starts at DEFAULT (the profile's value). Overrides are 1..999. Left from DEFAULT gives 999 and Right
  gives 1; then Left/Right step by 1, or by 10 holding L1 (Q) and by 100 holding R1 (E).
- Enter or Circle on a stat or WEAPON restores DEFAULT. RESET ALL TO DEFAULT clears every override. Cross (X key) or
  BACK TO SETUP returns and keeps the edits.
- Overrides apply once, after normal Ashley initialization. HP and MP start full; attributes set current and base
  values. Equipment, inventory and learned spells are unchanged. For permanent-stat item tests, leave the stat below
  999 so a real increase can be measured.
- WEAPON cycles `default`, `dagger`, `sword`, `greatsword`, `axe-mace`, `greataxe`, `staff`, `heavy-mace`,
  `polearm`, `crossbow` and `bare-hands` through original equipment records and the production equip path. Two-handed
  choices unequip (but keep) the shield; `bare-hands` only unequips the weapon.

### Keys

Gameplay keyboard bindings: arrows move, X = Cross, C = Circle, Z = Square, S = Triangle, Q/E = L1/R1,
1/3 = L2/R2, Enter = Start, Backspace = Select. Host keys:

| Key | Action |
|---|---|
| F6 / F7 | Pause or resume the simulation / advance one tick while paused. |
| F10 | Stop the encounter and return to the editor (works while paused). During a sequence it cancels the sequence. |
| F11 | Show or hide item-test help. Help pauses the game and restores the earlier pause state. Close it before F6/F7. |
| F1 / F2 / F3 | Ashley HP -50 (minimum 1) / MP -25 (minimum 0) / Risk +25 (maximum 100). |
| F4 / F5 | Choose / apply a condition (Poison, Paralysis, Numbness, Curse, Silence, Strength Down, Intelligence Down, Agility Down, Equipment Down) with the real status policy and timers. |
| F8 | Restore MP to Ashley's current maximum. |

F1-F5 and F8 set up item tests. They work only for a living Ashley in idle field control, not while loading, in
menus or resolving actions, and are rejected in door and scene fixtures. Every request is logged as
`control.adjust` with before/after state or a rejection. Item use itself still goes through the normal menus.
F10 and then START ENCOUNTER gives a fresh player and world.

Example, four Silver Wolves: zone 9, room 0, Ashley X=4 / Z=9, elevation 0, enemy definition 4, quantity 4, group
offset X -2 / Z +3, spacing 1. This places the wolves at (2,12), (3,12), (4,12) and (5,12). The same setup (with the
post-prologue profile) is in [silver_wolves.json](../tools/test/scenarios/silver_wolves.json).

## Presets

A preset is a version-1 JSON file. SAVE PRESET writes one, and each started encounter is also saved as
`encounter-N.json` in the run folder. Load a preset with `--scenario`; add `--start` to enter at once.
Unknown values are rejected. Omitted optional fields keep their defaults, so older presets still load.

```json
{
  "version": 1, "zone": 9, "room": 0, "seed": 1,
  "ashley_profile": "post-prologue", "ashley_weapon": "default", "ashley_stats": {"max_hp": 999, "max_mp": 999},
  "player": {"x": 4, "z": 9, "elevation": 0, "direction": 0},
  "enemies": [{"definition": 4, "position": {"x": 2, "z": 12, "elevation": 0, "direction": 0}}]
}
```

| Field | Values and meaning |
|---|---|
| `version` | Required. Must be 1. |
| `zone`, `room` | Required. Zone and room index. |
| `seed` | Required. RNG seed, 0..4294967295. |
| `player` | Required. `{x, z, elevation, direction}` tile position. `elevation` (0..3) and `direction` (0..1) default to 0. |
| `enemies` | Required array (may be empty), at most 8 entries of `{definition, position}`. Definitions are zone-local. |
| `ashley_profile` | Testing profile, default `prologue`. |
| `ashley_weapon` | Weapon choice, default `default` (the profile's equipment). |
| `ashley_stats` | Overrides `max_hp`, `max_mp`, `strength`, `intelligence`, `agility`, each 1..999. |
| `room_events` | `isolated` (default) or `production`. `production` keeps the original room actors and entry events and needs an empty `enemies`. |
| `external_event` | Original external EVT number, 0..9999. Needs isolated room events and no enemies. |
| `room_local_event` | Room-local event selector, 0..15. Needs production room events and no `external_event`. |
| `story_progress` | 0..255, written once to the campaign progress counter (script flag 0x82) before the first room loads. |
| `clear_count` | 1..99, the New Game+ clear counter. |
| `crossed_puzzles` | Nonempty list of distinct puzzle time-trial unlock flags: 221-228, 231-235, 237-239 or 242. |
| `inherited_music` | 1..199. Loads that music's wave bank first, for zones whose own music ID is 0. |
| `attract_demo` | Title-timeout attract demo 3 (zone 20, room 0) or 4 (zone 2, room 1). See the notes below. |

Notes:

- Validation checks tiles, floor support, overlaps and actor slots. It never moves actors or changes counts to pass.
  `position_units` may appear in saved presets; it is ignored when loading.
- `story_progress`, `clear_count`, `crossed_puzzles` and `inherited_music` are declared fixture state, not earned
  campaign progress. Some original enemy placements and checks read `story_progress`; play does not change it later.
- `inherited_music` stands in for a wave bank that an earlier zone or event would have left loaded. It is rejected
  if the music has no mapped bank, if the zone loads its own music, or with `attract_demo`.
- `attract_demo` needs Ashley at tile 4,2, elevation 2, direction 0, production room events, the `prologue` profile,
  the default weapon and no other overrides, and a sequence script with a `terminal: attract_to_title` scene and no
  presses.
- Samples: [silver_wolves.json](../tools/test/scenarios/silver_wolves.json) (isolated combat) and
  [first_lever_room.json](../tools/test/scenarios/first_lever_room.json) (production room, crossbow).

### Ashley testing profiles

Profiles are isolated test setups, not imported saves or completed story events. Normal game startup is unchanged.

| Profile | Adds |
|---|---|
| `prologue` | Nothing. New Game equipment, inventory, actions and menu gates. |
| `post-prologue` | The original stage-3 Map/Score menu gates. |
| `post-minotaur` | Post-prologue gates plus a representative set: three mapped chain abilities, three defenses, one spell per spell class and the first Break Art for the equipped weapon family. |
| `all-items-a`, `all-items-b` | Post-prologue gates plus 10 of each named non-equipment item, split over the two profiles (A: consumables and the first grimoires; B: the other grimoires and keys/sigils). |
| `all-spells` | Post-prologue gates plus every spell and rank in the battle-magic classes. |
| `all-abilities` | Post-prologue gates plus all chain abilities and defenses. The first three of each are mapped; change them in the Battle Abilities menus. |
| `all-break-arts` | Post-prologue gates plus all Break Arts. Only the equipped weapon family's arts are usable. |

- Profiles keep the real weapon, resource, status and target rules. Draw the weapon before choosing a Break Art.
- Grants are fixed; there is no loadout or spell-rank editor. Keys and sigils are stocked for inspection and still
  need their matching lock records and story events.
- Profiles do not set story progress; for example, offensive chain-ability learning still needs story flag
  0x82 >= 10. Switching back to `prologue` removes the grants on the next start.

### Production rooms, external events and room-local events

- `"room_events": "production"` keeps the original room actors and runs the original entry events. The editor shows
  ORIGINAL ROOM - PRODUCTION ENTRY EVENTS.
- `"external_event": N` plays original external EVT N through the production direct-event and cutscene APIs once the
  room is ready. The editor shows EXTERNAL EVENT N - COMPONENT PLAYBACK; the event owns its actors and input stays
  interactive. Only the initial room entry is isolated: chained scenes, room handoffs and returns use production
  rules. The EVT file is checked and read before startup; a missing or malformed file is an error.
- Some events need an earlier event to set up audio or actors. Pick a room, profile and event that match the original
  source; a file number alone does not provide those preconditions.
- `"room_local_event": N` starts that room's local event once at the first free-control frame, without evaluating
  its trigger conditions. These modes do not set story flags, simulate boss defeats or count as story progression.

## Script sequences

`--sequence <manifest.json>` runs several presets in one window. The window, renderer, audio device and catalog stay
alive; each script still starts a fresh production encounter and tears it down at the end of its budget. Nothing
carries over between scripts.

```json
{
  "version": 1,
  "scripts": [
    {"name": "first encounter", "scenario": "last-scenario.json", "frames": 900, "press": "200:Triangle,300:Cross"},
    {"name": "repeat with a fresh world", "scenario": "last-scenario.json", "frames": 900}
  ]
}
```

```powershell
build\Debug\VS.CombatLab.exe --sequence build\Debug\combat-lab\sequence.json --speed 8 --scripted
```

- Top-level fields are `version` (1) and a nonempty `scripts` array. Script fields are `scenario` (required, relative
  to the manifest), `frames` (required, positive), `name` (default: the preset file name), `press` and `scene`.
  Other fields are errors. Press ticks restart for each script, include room loading and must be below `frames`.
- Malformed entries, missing presets, invalid preset fields and unknown buttons stop the run before the first script.
  A plain script that does not complete (rejected setup, engine error or early end) stops the sequence; scene and
  door cases continue after a failure.
- `--data`, `--speed`, `--scripted`, F6 and F7 still work. F10 or closing the window cancels the remaining scripts
  and exits with code 2. The run folder gets `sequence-results.json`; each `script-0001`, `script-0002`, ... folder
  has its own `events.jsonl`, `encounter.json`, `manifest.json` and `result.json`.
- A completed script means initialization, the frame budget and teardown finished without engine errors. It is not
  evidence of victory or correct gameplay. Plain scripts report `playback_acceptance: not_asserted`.

Sample: [first_lever_handoff.json](../tools/test/scenarios/first_lever_handoff.json), checked with
`.\.venv-3\Scripts\python.exe tools\test\verify_first_lever.py <run-dir> --revision <full-revision>`.

### Scene assertions

Add a `scene` object to a script to check that original events actually play and hand control back:

```json
{
  "name": "source-backed-80-82-81", "scenario": "component-80.json", "frames": 24000,
  "scene": {
    "events": [80, 82, 81], "event_source": 0, "return_zone": 12, "return_room": 0, "confirm_dialogue": true,
    "preconditions": "Original 80 at 12/0 (7,2); component only, not an earned Minotaur victory."
  }
}
```

| Field | Meaning |
|---|---|
| `events` | Required, nonempty, in order. Each must start executing (not just load). |
| `event_source` | 0 disc EVT (default), 1 preloaded buffer, 2 local room event. |
| `return_zone`, `return_room` | Required unless `terminal` is set. The room that must regain control afterwards. |
| `terminal` | `ending_to_title`, `attract_to_title` or `game_over_to_title`, for scenes that end at the title instead. |
| `event_zone`, `event_room` | Optional pair. Only count events while in this room. |
| `preconditions` | Required, nonempty text describing the source or save state the fixture relies on. |
| `confirm_dialogue` | Default false. When true, presses Circle at visible dialogue waits (never Start). |

- The preset must match the scene: an `external_event` equal to the first event, or production room events. A
  `room_local_event` needs `event_source` 2. Scenes cannot use Start presses.
- `game_over_to_title` must start with event 0 from source 0. `attract_to_title` cannot confirm dialogue.
- A scene passes when its events run in order and the return room is ready with ordinary control for 120 stable
  ticks (or the declared terminal is reached naturally). Status is `passed`, `blocked` (budget ran out first) or
  `failed`. Defeat, skips and engine errors never pass. Results keep the observed IDs, matched count, control
  interval, inputs and last state. A scene assertion does not start an ending or set game state, and a pass is
  component playback, not story progression.

## Directed-door tests

```powershell
build\Debug\VS.CombatLab.exe --door-transitions --catalog-only
build\Debug\VS.CombatLab.exe --door-transitions --speed 64
build\Debug\VS.CombatLab.exe --door-transitions --door-case z009-r005-d001 --speed 64
build\Debug\VS.CombatLab.exe --door-transitions --door-from z009-r017-d003 --door-limit 20 --frames 3600 --speed 64
build\Debug\VS.CombatLab.exe --door-transitions --door-resume $prev --door-case z048-r006-d000 --speed 8 --scripted
```

- Door IDs are `z<zone>-r<room>-d<door>`. `door-inventory.json` always lists every door; `door-plan.json` lists the
  pending cases, retained passes and invalidated passes.
- Each case starts with the post-prologue profile on a non-door tile of the source room, with original actors and
  events. The Lab does not write story flags, inject actors or force events.
- A pass needs loading, the exact destination, an idle original entry event, restored control and 120 uninterrupted
  observation ticks. Running out of `--frames` is not a pass. This covers the loader, entry events and restored
  control; it does not prove physical approach, locks, switches or campaign reachability.
- `--door-resume` (here `$prev` is an earlier run's `sequence-results.json`) reuses earlier clean passes, reported
  separately as `retained_passed`, and runs only pending work. A changed door contract invalidates its old pass. A
  different data root or unreadable results file is an error. Omit it for a deliberate full regression.
- Unit tests: `VS.Tests.exe "[doors]"`; `"[disc-door-inventory]"` needs local data.

## Static scene inventory

```powershell
build\Debug\VS.CombatLab.exe --data Native\data --catalog-only --scene-catalog
```

This writes `scene-inventory.json` and a manifest without opening a window or running scripts. It lists every original
MPD room script, external EVT, SEQ/ESQ animation bank and the movie, with script graphs and room handoffs. Malformed
data is recorded and fails the command. Every playback status is `unverified`: indexing a scene does not prove it
plays. Unit tests: `"[scenes][catalog]"`, and `"[disc-scene-inventory]"` with local data.

## Logging

Every run, including option and startup errors, writes to a new folder next to the executable:
`build\<Config>\combat-lab\runs\<timestamp>-<n>\`. The newest folder is the current run, and `session.log` records
its path. No console redirection or debugger is needed.

| File | Contents |
|---|---|
| `session.log` | Readable engine messages. Trace level, all channels (Debug with `--verification-log`). |
| `events.jsonl` | Ordered structured records: engine messages, RNG draws, action results, host controls and per-tick input, actor, action and inventory snapshots. |
| `manifest.json` | Source revision, worktree status, data path, run mode and options. |
| `catalog.json` | Room and enemy availability per zone, with reasons and spawn provenance. |
| `encounter-N.json` | Each submitted encounter setup. |
| `sequence-results.json`, `script-NNNN\` | Sequence and door runs. |
| `door-inventory.json`, `door-plan.json`, `scene-inventory.json` | Door and scene catalog runs. |

- The console shows warnings and errors only; the files keep everything. Each record has `sequence`, `tick`,
  `elapsed_us` and `category`; script records add `script` and `script_tick`.
- Frame snapshots include HP/MP, Risk, statuses, commands, positions, weapon state, targets, item counts, AI
  candidates and pending loot. Pending loot is not proof of delivery.
- `event.command`, `event.result` and `state.flag_write` records show executed script commands with all operand bytes,
  their results (joined by command ID) and flag writes with old and new values.
- `performance.summary` at exit has phase and frame-interval timings. A write failure stops the run rather than
  losing data silently. Crash stacks go to the session logs; the run watchdog writes `vs-native-hang.log` beside the
  executable.

### Verification log and log volume

Full tracing is the default. It can make large files, and there is no rotation. `--verification-log` keeps every
frame, command, result and flag write, content reads, lifecycle records and all warnings and errors. It drops
per-draw Trace messages and lower-level engine messages outside the `content` and `boot` channels. In sequences
without `--player-observations`, heavy records go only to each script's `events.jsonl`. Use full tracing for
effect-path work.

For bulk checks, use one sequence instead of one process per case:

```powershell
build\Release\VS.CombatLab.exe --sequence <sequence.json> --speed 64 --scripted --verification-log
```

- Group cases that share a prerequisite, and record a failed prerequisite once.
- Remove or archive run folders you no longer need. Keep logs, captures and game data out of commits and issues.
- Older raw logs were moved to the cold archive `%LOCALAPPDATA%\VS-NativePort\evidence-archive\20261028`, where
  `index.jsonl` maps each original path to its archived copy.

### Detailed logging in the Desktop game

The Desktop game uses the same trace writer and frame format. `kEnableDetailedGameLogging` near the top of
[Native/Desktop/src/main.cpp](../Native/Desktop/src/main.cpp) turns it on. It is `true` in Debug builds and `false`
in Release builds, where normal logging is also off. Change it and rebuild to switch.

- Each launch writes `build\<Config>\game-logs\<timestamp>-<n>\` with `manifest.json`, `session.log` and
  `events.jsonl`. Frames also include `scene` state: event and thread cursors, dialogue waits, camera, effect
  progress and model poses.
- `vs-native.log` beside the executable mirrors the current session and is replaced at each launch; run folders are
  kept. `--verification-log` and `--player-observations` also work here, but only with detailed logging.
- `--log-level` and `--log-channels` filter Desktop engine messages. A diagnostic filter must include
  `event-command`, `event-result` and `state-flag`. The Lab does not accept these options.

### Effect paths

```powershell
build\Debug\VS.CombatLab.exe --speed 1 --trace-effect-paths 1:1200
```

The `effect-path` channel gets `frame` markers for every selected tick, `particle` records (pool slot, effect ID,
age, positions, matrices, projection and emitted command addresses) and `primitive` records (final vertices, texture,
color and blend state). Join particle `command_addresses` to primitive `command_address` within the same tick. Slots
and addresses are reused, so they are not lifetime IDs. A recorded path does not prove the effect is placed
correctly; watch the whole effect at normal speed. The Desktop game accepts the same option.

### Reviewing manual item tests

Manual play uses the same automatic logs. Open the run folder and check `manifest.json` and `encounter-N.json` for
the revision, profile, overrides and targets.

- For each use, line up the inventory decrement, chosen action, `combat.result` messages, nearby actor snapshots,
  learned-action changes and the return to field control.
- Compare the frames right around the result: passive MP recovery and Risk decay can look like item effects. Check
  permanent items against base values or maximums. Keep consumption, learning and target effects separate. In a live
  file, read only complete JSON lines and note the last tick you checked.
- Ranked grimoires need four uses of the same book in one encounter; use F8 between ranks. The training dummy
  suppresses magic effects, so it is not a valid target for offensive spells. Area spells take three confirmations:
  height, area position, then the target list.
- Pick enemies by class: Mandel (zone 9, definition 0) is Undead; Sackheim (zone 9, definition 6) is Human with MP.
  Drains reject Undead; Exorcism needs Undead.

## Startup feedback and timing

The Lab shows a readable loading screen before it reads the encounter catalog, opens audio and controllers, and
records the run configuration. All of this stays on the main thread. `--scripted` runs do not open gamepads.
`startup.stage`, `startup.first_present` and `startup.interactive_present` records carry `elapsed_us` timestamps.
They separate the first loading frame from the first editor or encounter frame and show slow device, catalog or
renderer startup. A failed step still logs its error and stops startup.

## Production contracts and current limits

- Rooms and enemy stats come from the extracted ZND and MPD data. Each enemy needs a hostile MPD placement in its
  zone; AI, hostility, drops and variants are kept. Unavailable entries carry a reason in `catalog.json`. Being listed
  is not a claim that an enemy works.
- Enemies are same-zone only. Cross-zone model loading and script-dependent actors are not supported. Guildenstern's
  orbiting form is unavailable because its arena state is not ported.
- Boss placements that the original keeps after defeat are accepted. Only the Lab's copy of the placement clears that
  flag; the original data and normal sessions are unchanged.
- Actor slots follow production rules. The floor check is conservative and does not prove ceiling clearance, large
  footprints, paths or safe movement. In isolated mode, story and door events are off; profiles set only the startup
  menu stage.
- Apart from declared preset fields and the logged F1-F5/F8 adjustments, the Lab does not grant flags, edit HP, inject
  actors, force events or bypass save eligibility.
- External-event, room-local and scene playback is component playback, not story progression or earned campaign
  evidence.
- Accelerated runs (`--speed` above 1) establish semantic behavior only. Timing, listening and fidelity checks need
  `--speed 1`; faster runs drop stale audio.
- A completed script, clean exit, idle enemy or passing unit suite is not acceptance of a gameplay feature. Feature
  acceptance is tracked in the issues.
