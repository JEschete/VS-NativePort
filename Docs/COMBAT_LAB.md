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
Set **QUANTITY: 0 (SOLO)** and clear any explicit groups to enter with Ashley
alone. An empty-enemy preset stays solo when loaded.
The grid shows starting tiles over a conservative static-floor preview: green
tiles have complete floor support; dark tiles are blocked/partial, pits, missing
support or unavailable data. Invalid starts identify the actor, tile and reason.

Select **ASHLEY SETUP** and press Enter/Circle to edit maximum/starting HP and
MP, base STR, INT and AGI. Each field starts at **DEFAULT**, preserving the
selected profile; overrides accept integers **1..999**. Left from DEFAULT
selects 999, right selects 1. Left/right then adjust by 1; hold L1/Q for steps
of 10 or R1/E for steps of 100. Enter/Circle on a stat restores DEFAULT;
**RESET ALL TO DEFAULT** clears every stat and weapon override. Cross/X or **BACK TO SETUP**
returns with the edits retained.

Overrides are applied once, after normal Ashley initialization at encounter
start. HP/MP overrides start full; attribute overrides update both current and
base values. They do not change equipment, inventory capacity, learned spells
or main-game startup. Damage and spell costs remain active, so use F8 to refill
MP between grimoire ranks. For permanent-stat item tests, leave the relevant
stat below 999 so a genuine increase can be measured.

Presets and encounter logs include an optional `ashley_stats` object, for
example `"ashley_stats": {"max_hp": 999, "max_mp": 999}`. Omitted fields preserve
defaults, and older presets without this object still work. Invalid types,
out-of-range values and unknown stat names are rejected explicitly.

The **WEAPON** row in the same submenu cycles through `default`, `dagger`,
`sword`, `greatsword`, `axe-mace`, `greataxe`, `staff`, `heavy-mace`, `polearm`,
`crossbow` and `bare-hands`. Enter/Circle restores `default`. The main setup
row order is unchanged. Presets optionally save `ashley_weapon`; omitted values
preserve the profile's equipment, and unknown choices are rejected.

Weapon overrides reuse the equipped New Game weapon's existing blade/grip
slots and indices, with original gem-free equipment records and the production
part/stat conversion and Ashley model-loading paths. They do not increase
inventory capacity or change consumables, spells or campaign startup. Bare
hands only unequips the weapon. The original equip rules unequip, but retain,
the shield for greatswords, greataxes, heavy maces, polearms and crossbows;
daggers, swords, axes/maces, staves and bare hands preserve it.

| Family | Original ZND / actor row (zero-based) | Blade / grip / WEP model IDs |
|---|---|---|
| Dagger | ZONE009 / 10 | 1 / 96 / 1 |
| Sword | ZONE001 / 0 | 15 / 96 / 15 |
| Greatsword | ZONE013 / 18 | 25 / 96 / 28 |
| Axe/Mace | ZONE009 / 1 | 35 / 105 / 35 |
| Greataxe | ZONE015 / 8 | 51 / 106 / 55 |
| Staff | ZONE024 / 2 | 60 / 111 / 60 |
| Heavy Mace | ZONE012 / 0 | 70 / 105 / 68 |
| Polearm | ZONE013 / 11 | 71 / 115 / 71 |
| Crossbow | ZONE009 / 9 | 83 / 121 / 83 |

The selector copies original names, material, DP/PP, damage type, range, stats
and affinities; it does not relabel Fandango or change a live actor's category.
Equipment is established before profile grants and normal Ashley spawning.
Post-Minotaur's representative art follows the selected family, including
Lotus Palm for bare hands. Each fresh entry restores New Game inventory before
applying its selection. Frame logs record actual `weapon_category`,
`weapon_blade`, `weapon_grip`, `weapon_model` and `shield_model` alongside the
requested choice in the encounter setup.

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
- F11 opens readable **item-test help**, pauses simulation and restores its
  previous pause state on closing. F1 reduces HP by 50 (minimum 1), F2 reduces
  MP by 25 (minimum 0), F3 increases Risk by 25 (maximum 100). F4 cycles the
  selected negative condition; F5 applies it using the real status policy and
  timers. F8 restores current MP to Ashley's current maximum without increasing
  maximum MP, learning spells or changing inventory. Help displays the selection
  and the last applied/rejected request.
  Controls also work while help is open; close help before F6/F7.
  These are Lab-only preconditions, available only for a living Ashley in idle
  field control, not while loading, in menus or resolving actions. Item use
  itself still goes through normal menus/targets/results. Every adjustment
  records before/after actor state or an explicit rejection in the run files.
  Frames identify the acting actor slot in `source_actor`, not the
  target-selection mode. Logs before the #117 repair mislabeled that mode
  as an actor; use their source revision when interpreting earlier evidence.
  Targeting frames also record the real area center and target-preview
  coordinates, allowing area-spell input and selected enemies to be checked.
  No main-game bindings are changed.
- F4 offers Poison, Paralysis, Numbness, Curse, Silence, Strength Down,
  Intelligence Down, Agility Down and Equipment Down. Use **Numbness** to test
  timed half-speed movement. The former **SLOW** entry was removed: its flag
  represents injured-leg weakness and clears when the legs are healthy, so
  applying the flag alone was not a valid lasting ailment fixture (#115).
  Production limb-injury behavior is unchanged.
- Ranked-grimoire acceptance requires four consecutive uses of the same book
  within one encounter, checking the selected ranks I-IV, learned actions,
  inventory counts and actual target results. Restore MP with F8 between uses
  and provide a valid enemy for offensive spells. Restart only between books,
  not between ranks; consumption or learning alone does not prove the effect.
  Use normal speed (`--speed 1`) while the accelerated-audio queue failure in
  [#83](https://github.com/JEschete/VS-NativePort/issues/83) remains unresolved.
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

The **ASHLEY** row selects `prologue`, `post-prologue`, `post-minotaur`,
`all-items-a`, `all-items-b`, `all-spells`, `all-abilities` or `all-break-arts`.
These are isolated testing configurations, not imported saves or claims of
completed story events. The default remains the original New Game equipment,
inventory and action set. Normal game startup is unchanged.

- **Prologue:** unchanged New Game gates.
- **Post-prologue:** the original stage-3 Map/Score menu gates.
- **Post-Minotaur:** those menu gates plus three mapped chain abilities, three
  defenses, one learned spell from each real spell-class table, and the first
  Break Art for the equipped weapon family. This is the owner-selected
  representative set, not an all-actions or all-ranks unlock.
- **All-items A / B:** post-prologue menu gates and 58 distinct item stacks
  each (10 per stack), keeping the normal 64-slot inventory and starting
  equipment. Together they contain all 116 named non-equipment items:
  34 ordinary consumables, 50 grimoires and 32 keys/sigils. A has IDs 323-356
  and the first 24 named grimoires through 430; B has the remaining 26
  grimoires (431-456) and keys/sigils (458-489). Unnamed holes are excluded.
  Neither profile pre-learns extra spells or grants equipment. Keys/sigils
  are stocked for inspection, not claimed to work without their story events.
- **All-spells:** post-prologue menu gates and all 50 spells in the four
  production battle-magic classes, including all four ranks of the seven
  ranked spells (71 learned actions). Starting inventory, equipment, resources,
  chain mappings and Break Arts are not expanded. Use **ASHLEY SETUP** for
  starting HP/MP and F8 for MP refill. This is a learned-casting fixture, not
  grimoire-learning evidence; it does not grant the story/save-point Teleport
  feature or bypass door/event requirements.
- **All-abilities:** post-prologue menu gates and all 14 offensive chain
  abilities plus all 14 defenses from the production lists. The usual first
  three of each list are mapped initially; use the normal Battle Abilities
  menus to change them. Inventory, equipment, resources, spells, Break Arts
  and story/progression counters are not expanded. This is a fixture for
  [#10](https://github.com/JEschete/VS-NativePort/issues/10), not evidence that
  all ability effects or timing windows have been verified.
- **All-Break-Arts:** post-prologue menu gates and the original 40 arts
  (IDs 184-223): four for each of nine weapon families and bare hands.
  Only the currently equipped category is usable; other learned categories
  remain viewable through **View Arts**. Equipment, inventory, HP/MP, chain
  mappings, spells and acquisition/story counters are unchanged. Use
  **ASHLEY SETUP** for starting HP; arts still require more HP than their
  nominal cost (25/40/55/75), leaving at least 1 HP after payment.
  This supports [#11](https://github.com/JEschete/VS-NativePort/issues/11),
  not a claim that every art or acquisition path is already verified.

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

## Ability-profile acceptance checkpoint (2026-10-22)

The `all-abilities` fixture supports ongoing
[#10](https://github.com/JEschete/VS-NativePort/issues/10); it does not close that
issue. The original action definitions and the two production ability lists
contain these 28 named abilities. Each list has 14 entries, not the 16 bytes
reserved for its native array. Offensive IDs 33, 36, 38 and 39 are unnamed holes.
Risk below is the definition's base cost, not a claim of verified live charging.

| Offensive ID / name | Risk | Defensive ID / name | Risk |
| --- | ---: | --- | ---: |
| 22 Heavy Shot | 1 | 40 Ward | 1 |
| 23 Gain Life | 2 | 41 Siphon Soul | 6 |
| 24 Mind Assault | 1 | 42 Reflect Magic | 2 |
| 25 Gain Magic | 2 | 43 Reflect Damage | 2 |
| 26 Raging Ache | 1 | 44 Absorb Magic | 4 |
| 27 Mind Ache | 1 | 45 Absorb Damage | 4 |
| 28 Temper | 2 | 46 Impact Guard | 4 |
| 29 Crimson Pain | 2 | 47 Windbreak | 4 |
| 30 Instill | 1 | 48 Fireproof | 4 |
| 31 Phantom Pain | 3 | 49 Terra Ward | 4 |
| 32 Paralysis Pulse | 3 | 50 Aqua Ward | 4 |
| 34 Numbing Claw | 3 | 51 Shadow Guard | 4 |
| 35 Dulling Impact | 3 | 52 Demonscale | 4 |
| 37 Snake Venom | 3 | 53 Phantom Shield | 6 |

Debug run `1792684051521630-0` and Release run `1792684131131669-0` use
zone 12 / room 0, Ashley at (9,10), Minotaur Zombie definition 1 at (9,11),
999 starting HP/MP, isolated events, speed 1 and ordinary keyboard input.
They verify all 14 rows in both production menus, scrolling, assigning the last
entry to Circle, moving it to Triangle (clearing Circle), and unassigning it.
Story/acquisition counters stay unchanged. F10 and a fresh encounter restore
the initial three mappings in each list; switching back to Prologue removes
the advanced grants/mappings and restores the original inventory and gates.
Both sessions finalize normally with no error events and `trace_failed=false`.

The local driver waits for `transition == 0`, not just field control mode 1.
An earlier diagnostic sent L2 during room-entry transition 4; this was an early
test input, not a demonstrated shortcut defect. The trace's `learned_actions`
also includes intrinsic/NPC/item actions: compare the ability family separately
and preserve that intrinsic set across profile changes.

Evidence uses `60c5bd3d` plus the five fixture code-file changes, with unrelated
dirty files preserved. Debug and Release builds pass; each focused
`[profile],[encounter],[editor]` run passes 45 cases / 1,861 assertions and each
full native suite passes 420 cases / 1,060,980 assertions. Focused quality and the
changed-header clang-tidy gate report zero final findings, with no baseline
changes. Local commands/evidence are `build\quality\live-restart-check.ps1
-Configuration Debug|Release -Abilities`, `build\quality\abilities-*`, and the
two configurations' `combat-lab\runs` directories. This does not verify every
ability's effect, timing, Risk, acquisition or combat-sequence termination.

### Ability timing and Risk checkpoint

The next bounded #10 checks use the published `3cc999c3` gameplay code, with
new maintained regressions in `Native\Tests\src\CombatTestCases.cpp`.
All 28 abilities now have original-cost tests at chain steps 0, 9, 10 and 31
and starting Risk 0, 97 and 100. Preview is non-mutating; committed costs cap
Risk at 100 without rejecting the ability or spending HP/MP. Offensive costs
gain `chainStep * chainStep / 10` from step 10; defensive costs do not.
These are cost tests, not all-ability effect acceptance.

Exact timing tests cover all three buttons, normal/hard difficulty, offensive
attacks, physical and magical defense, queue depths 0/3/24/99, and both sides
of every result boundary. The success interval is strictly after the window
and inclusive of `window + margin`; margins shrink to a minimum of 2.
Tests also cover simultaneous-button priority, absent/early/repeated input,
unmapped buttons, no/positional/self/dummy/inappropriate targets, inability to
continue defenses, and defensive score saturation at 65535.

Real-input Heavy Shot runs use the same all-abilities encounter as above:

| Check | Debug run | Release run |
| --- | --- | --- |
| Successful continuation, exact damage and Risk | `1792685582328028-0` | `1792685756776909-0` |
| Early input does not chain | `1792685719037260-0` | `1792685779484385-0` |
| Late input does not chain | `1792685732216378-0` | `1792685792667623-0` |
| Cancel ordinary attack selection | `1792685745501815-0` | `1792685805742540-0` |

Debug's sampled Circle is at tick 233, timer 38 / window 34. Heavy Shot deals
3 HP (676 -> 673), exactly the truncated 70% of the initial 5-HP weapon hit,
adds 1 Risk, and returns at 407. Release includes two correctly timed but
missed/charged Heavy Shots before a positive third attempt: native Circle
samples are 229/479/725, timers 38/40/38 against window 34. The last Heavy Shot
deals 2 HP (674 -> 672), from a 3-HP initial hit, and returns at 899. Each
attempt costs 1 Risk; misses are retained rather than counted as successes.
Only the ability-use flag changes; unrelated progression and inventory stay
unchanged. Early/late input performs no ability, and cancellation spends no
Risk. All eight sessions finalize without error events or trace failure.

The initial early-input probe incorrectly waited for an unarmed window: this
weapon's window is already 34 at animation timer 0. The corrected probe sends
input before its success interval. No game change was needed for that
diagnostic assumption or for legitimate misses.

Both final full suites pass **430 cases / 1,067,689 assertions**. Formatting,
cpplint, Ruff and clone checks are clean; focused clang-tidy retains one
existing finding, zero new. No baseline/suppression changes. Local evidence:
`build\quality\ability-behavior-*`, `live-chain-22-*-Debug.json`,
`live-chain-22-*-Release.json`; replay command
`build\quality\live-restart-check.ps1 -Configuration Debug|Release -Chain 22
-ChainTiming success|early|late|cancel`.
The following checkpoint covers effects, live defense, chain defeat and
earned acquisition.

## Ability effects, defense and defeat checkpoint

The next tests use gameplay revision `2d231c81`, with maintained test changes
in `Native\Tests\src\CombatTestCases.cpp`. No production effect implementation,
generated output or progression policy was changed for this checkpoint.

All 28 named abilities have deterministic component tests through the original
stat calculators and effect mediators. Numeric effects then use production
stat application; status cases use production infliction/removal without the
menu-label rendering path. These do not substitute for full hit-roll/target
resolution, every runtime-table binding, audiovisual parity, or story/save
acceptance. Random/equipment-wear processing is disabled for these component
contracts, not in the live replays.

The fixture's previous target hit is 101 combined HP/MP, incoming damage is
110, and offensive queue offset is 2. Ashley starts at HP 600/1000, MP 100/300,
weapon DP 100/1000 and PP 70/255; the target starts at HP 800, MP 200.

| Actions | Verified component effect |
| --- | --- |
| 22 Heavy Shot | 72 target HP damage |
| 23 Gain Life / 24 Mind Assault / 25 Gain Magic | 32 Ashley HP / target MP damage / Ashley MP |
| 26 Raging Ache / 27 Mind Ache | 42 target HP / MP damage from Ashley's missing resource |
| 28 Temper | 42 target HP damage and weapon DP 100 -> 304 |
| 29 Crimson Pain | 82 target HP damage and 20 Ashley HP cost |
| 30 Instill | 12 target HP damage and weapon PP 70 -> 173 |
| 31 Phantom Pain | 72 target HP damage and weapon PP 70 -> 0 |
| 32 Paralysis Pulse / 34 Numbing Claw / 35 Dulling Impact / 37 Snake Venom | Paralysis / Numbness / Silence / Poison |
| 40 Ward | Removes Paralysis and Numbness, preserves Poison |
| 41 Siphon Soul | 12 MP from incoming spell cost 25; no non-spell recovery |
| 42 Reflect Magic / 43 Reflect Damage | 44 HP reflected only for the matching attack type and damage effect |
| 44 Absorb Magic / 45 Absorb Damage | 22 HP restored only for the matching attack type and damage effect |
| 46 Impact Guard through 52 Demonscale | 55 HP restored for each matching affinity; all mismatches and healing inputs checked |
| 53 Phantom Shield | 90 HP restored and 90 shield PP consumed only after damage |

Additional tests cover HP/MP/DP/PP capacity and MP-drain clamps. Timing cases
now explicitly include action IDs 1/54/99/141: the original extended defensive
window is an **action-ID >= 54** rule, not simply a spell-type rule.

### Live Ward and lethal chain

| Check | Debug run | Release run |
| --- | --- | --- |
| Actual enemy attack, timed Ward and Paralysis cure | `1792687747745849-0` | `1792687763174288-0` |
| Heavy Shot kills Zombie, collect loot, return to field | `1792688551760571-0` | `1792688605774947-0` |

Ward uses the all-abilities fixture above and Lab-only Paralysis setup. Both
builds show Paralysis active at tick 163, a real enemy hit taking HP 999 -> 903,
the input observation at 289, cure at 357 and normal return at 437. Risk rises
0 -> 1; the ability-used flag is set and the original acquisition point rises
0 -> 1. Other progression and inventory are unchanged. Unlike offensive
chains, the original defensive point path does not require Minotaur flag
0x82 >= 10; this source-backed asymmetry is preserved.

The lethal-chain fixture uses the original Zone 9 room 0 Zombie definition 2,
Ashley at (4,9), enemy at (4,10), all-abilities, HP/MP 999 and other stats
unchanged. A real weapon hit does 28 HP, followed by alternating Heavy Shot
and Gain Life. Five Heavy Shots do 19/21/23/25/27 HP; the last kills the
121-HP enemy. Death occurs at Debug tick 1279 / Release 1277. After normal
Square take-all, item 323 increases 10 -> 12 and field control returns at
1613 / 1611 with Ashley alive, recovery zero, Risk 24 and the dead actor
removed. No HP, event, loot or completion flags were written by the driver.

An earlier diagnostic timeout was the normal loot menu awaiting input, not a
demonstrated combat deadlock. Emptying its rows through take-all follows the
original automatic leave path. The corrected replay verifies the inventory
delta rather than incorrectly expecting unchanged inventory after a kill.

### Earned acquisition and validation

Component tests cover all 22 original cumulative point thresholds, below/at/
above each boundary, the 22-unlock cap, all ten weapon categories and four
Break Art thresholds, Break Art priority, and non-mutating ineligible reward
initialization. Fixtures restore shared tables, definitions and progression.

Real-input earned acquisition passes Debug `1792689722375744-0` and Release
`1792690148434786-0`. The separate fixture is Zone12 room0, Minotaur Zombie
definition1, Ashley(9,10), enemy(9,11), `post-minotaur`, HP/MP/STR/INT999,
original AGI, seed1 and isolated room events. Twenty-one successful Ward
defenses earn points0->20, then remain capped at20 pending reward selection;
no ability is prematurely learned or acquisition counter advanced.

The enemy retreats beyond ordinary weapon range, so the profile's actual
learned attack spell97 finishes it through the production magic menu and
target selector. A1254-HP result defeats the681-HP enemy at Debug tick8339 /
Release8337, with MP999->974. Normal loot controls precede the real ability
reward menu. That menu reserves exactly one unlock; cancelling its ability
selection neither grants an action nor advances the counter again.
Reselecting the first available chain ability grants only Gain Magic25 at
9347 /9343; normal field control returns at9627 /9623. All previously learned
actions remain learned. Points stay20, unlock count stays1, the32 other
art/weapon-counter bytes remain unchanged, and original intro/Minotaur/
Teleport/Break-Art flags are preserved. Only the actual spell/ability-use
flags change. No progression, HP, loot or event writes bypassed gameplay.

Earlier diagnostic attempts incorrectly assumed melee proximity and then
Fireball99 rather than this profile's learned97. A Release40-ms key press
was not sampled during an action with four-tick input cadence; holding100ms
spans that cadence without changing the game's timing window. Failed
attempts are not counted as acquisition acceptance. Both accepted sessions
finish without errors or trace failure.

Final Debug and Release builds and full suites pass **448 cases / 1,074,484
assertions**. Formatting, cpplint, Ruff and clone checks are clean; focused
clang-tidy reports **one existing finding, zero new**, without baseline changes.
Validation uses the documented MSBuild commands, each configuration's
`VS.Tests.exe`, and
`quality-native.cmd --files Native\Tests\src\CombatTestCases.cpp --tidy`.

Local evidence: `build\quality\ability-effects-*`,
`build\quality\ability-acquisition-*`, `live-defense-ward-*.json`,
`live-chain-defeat-*.json`, `live-ability-acquisition-*.json`, and each run's
events/manifest/session log.
The real-input driver selections are
`build\quality\live-restart-check.ps1 -Configuration Debug|Release -Defense`
and `-ChainDefeat` / `-Acquire`. These results do not promote C++ ownership,
retire compatibility bridges, or certify the separate real-story/save gates
in #17/#18.

## Break Art menus and resource checkpoint (2026-10-22)

[#120](https://github.com/JEschete/VS-NativePort/issues/120) fixes the
four-art normal menu writing/reading its fifth **View Arts** ID outside
G01005. The unfixed production-menu regression returned action **5** instead
of navigation **0xFFFF**, failing 20 assertions across all ten categories.
F02610's copy fixup now reserves five entries before static hoisting, so
both storage and reload initialization agree. Original source dimensions
remain recorded separately in the data ledger; generated output was not
patched manually.

The maintained tests cover every category, normal/shortcut menus,
equipped/unequipped restrictions, both Cursor Memory settings, the fourth
art, and fifth navigation row. All forty original art IDs also exercise
HP 0 / cost-1 / cost / cost+1 / 999, preview nonmutation, rejection without
payment, and payment leaving at least 1 HP. MP, Risk and weapon PP are not
charged. Eligibility checks distinguish unlearned/sheathed/Paralysis,
recovery and insufficient HP from Silence (which does not block arts).
These are component contracts, not execution of every damage/secondary
effect.

Actual window/controller navigation at speed 1 uses the new **all-break-arts**
solo profile in Zone12 room0, seed1, Ashley(9,10), HP/MP999:

| Configuration | Run | Fourth-art no-target preview / menu return | Shortcut fourth-art cancel / field return |
| --- | --- | --- | --- |
| Debug | `1792693091210881-0` | 3730 / 3864 | 4134 / 4325 |
| Release | `1792693210368049-0` | 3712 / 3846 | 4118 / 4307 |

Both runs open the normal five-row menu and navigate **View Arts** through
all ten categories. Four Sword rows are enabled; all four rows in each
unequipped category remain disabled, while its navigation row stays usable.
The fourth Sword art is correctly action191 (Papillon Reel). Confirming an
empty target selection returns without execution; the shortcut has exactly
four rows and target cancellation restores field control. HP/MP remain999,
Risk0, and acquisition/story counters stay unchanged. F10, fresh all-art
entry, and return to Prologue preserve inventory and remove advanced grants.
Both logs end normally with `trace_failed:false` and no errors.

The first diagnostic attempt sent Triangle during the weapon-draw animation;
the second incorrectly moved past the first unlocked root row into Inventory.
The accepted driver waits for drawing and checks the actual root selection.
Neither diagnostic established a native menu defect.

Commands/evidence on `a320c82e` plus the scoped dirty worktree:

- Standard Debug/Release MSBuild commands and both `VS.Tests.exe` full suites:
  **455 cases / 1,078,556 assertions** each.
- `.venv-3\Scripts\python.exe tools\port\copy_to_native.py` twice:
  second manifest unchanged and all **86** registered owned paths preserved.
- `.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py`:
  **11 tests pass**.
- Focused `quality-native.cmd --files ... --tidy` checks cover the changed
  profile/menu/test C++ surfaces and both maintained Python files. The final
  amended `CombatTestCases.cpp` recheck reports **one existing finding,
  zero new**; no baseline or suppression changes.
- `build\quality\live-restart-check.ps1 -Configuration Debug|Release -ArtMenus`.
  Local fixture: `build\quality\all-break-arts-solo.json`; observations:
  `build\quality\live-break-art-menus-*.json`; logs:
  `build\quality\break-art-menu-*` and the run directories above.

### Mesh10 import and live Sword execution checkpoint (2026-10-22)

[#121](https://github.com/JEschete/VS-NativePort/issues/121) exposed a Release
Cherry Ronde crash in the shared Mesh10 renderer. The seven plugin bindings
(069, 077, 129, 137, 153, 154 and 260) had unresolved source sizes: their
tentative native arrays reserved one element and their source data was not
imported. A regression failed for all **14 missing destinations** before the
repair. The original template consumes **10 eight-byte vertices** and
**16 six-byte triangles**. Correcting those existing ledger extents produces
complete storage and module imports without altering the renderer or masking
invalid indices. Original declaration/provenance and verification/promotion
status remain separate.

The content tests check every module name, source offset, import size and
native capacity. With local extracted data available, they compare all loaded
bytes to the original modules, check color selectors, vertex indices and UV
offsets, and repeat the import. These checks do not require committed assets.

Actual controller verification uses speed1, seed1, Zone12 room0, the
`all-break-arts` profile, original Fandango, HP/MP999, Ashley(9,10), and a
Minotaur Zombie at(9,11):

| Configuration/action | Run | Acceptance |
| --- | --- | --- |
| Debug Cherry Ronde190 | `1792696331113100-0` | Three encounters, four casts including one zero-damage result and retry; HP55 per cast; positive damage2/3/4 |
| Release Cherry Ronde190 | `1792696162985589-0` | Three encounters and casts; HP55 each; damage2/4/2 |
| Release Papillon Reel191 | `1792696417809149-0` | HP75, damage1, normal recovery |

Both Cherry Ronde runs include two F10 exits with both actors alive and fresh
re-entry in the same host. Every cast checks actual HP payment, reported
damage against the target HP delta, and return of control. MP/Risk, inventory,
learned actions and unrelated progression remain unchanged. All three logs
end with `trace_failed:false` and no errors. This repairs the reproduced
Mesh10 crash; it does not reproduce or close the broader intermittent #82.

Validation on `dded4994` plus the scoped Mesh10 changes and separate #11
acquisition tests:

- Standard Debug/Release solution builds; final test-project rebuilds and
  full `VS.Tests.exe` suites: **458 cases / 1,082,195 assertions** each.
- Focused `[content][module]`: **6 cases / 1,831 assertions** each.
- Two `copy_to_native.py` runs: stable second manifest; existing owned paths
  preserved. `test_encounter_fixups.py`: **11 tests pass**.
- `quality-native.cmd --files Native\Tests\src\ContentTests.cpp
  Native\Tests\src\CombatTestCases.cpp --tidy`: **four existing findings,
  zero new**. No baseline or suppression changes.
- `build\quality\live-restart-check.ps1 -Configuration Debug|Release
  -Art 190 -ArtRuns 3`, then `-Configuration Release -Art 191`.
  Local evidence: `build\quality\mesh10-*`, `live-break-art-*.json`, and
  the run directories above. The two #11 acquisition tests separately cover
  qualifying lethal actions and thirteen rejection/cap gates:
  **2 cases / 1,860 assertions**.

All four Sword arts have now completed through actual controls in both
configurations. This is not all-forty execution or positive Poison acceptance:
the Zombie did not demonstrate Vile Scar's Poison. The separate tiled-ring
color repair is recorded below under
[#122](https://github.com/JEschete/VS-NativePort/issues/122). Neither this
storage repair nor those successful actions establish complete visual parity
or close the particle-path/trail acceptance in #76.

### Tiled-ring source-data checkpoint (2026-10-22)

[#122](https://github.com/JEschete/VS-NativePort/issues/122) corrects thirteen
more omitted source tables: PLG049, 069, 073, 085, 137, 138, 143, 146, 148,
150, 160, 242 and 265. The original shared template defines eight CVECTOR
colors and selects modes0-7. Each required source extent is32 bytes, not one
tentative native element. The new regression failed all **13 bindings**
before their existing ledger sizes were corrected and output regenerated.

All thirteen imports now have checked module/offset/size/capacity and
byte-for-byte source comparisons across repeated loading. PLG139 shares the
PLG085 binding; both original tables at offset6752 are byte-identical, and
the test checks that alias against PLG139 too. The already source-initialized
PLG044 table remains a separate control. Thus all fifteen shared template
instances have covered source storage, without changing renderer algorithms.

Real-input Cherry Ronde190 verification repeats three encounters and two
live-enemy F10/re-entry cycles per configuration:

| Configuration | Run | Damage | Live tiled-ring evidence |
| --- | --- | --- | --- |
| Release | `1792697927456019-0` | 4/4/2 | 155 samples per entry, ages1-39 |
| Debug | `1792698004510448-0` | 4/2/4 | 156 samples per entry, ages1-39 |

The ignored diagnostic resolves callbacks and checks native field offsets
from the current PDBs, then only reads the effect lists and sends ordinary
controller input. It observes the actual PLG137 tiled-ring state advancing;
all32 live color bytes match the original module on every sampled update.
Every cast pays HP55, applies its exact reported damage and recovers control.
Unrelated resources/progression remain unchanged. Both sessions end normally
with `trace_failed:false` and no errors. Two earlier diagnostic attempts
stopped on PowerShell record/output handling after successful casts; they
were not native failures and are not accepted re-entry runs.

Validation on `25831e66` plus the scoped dirty repair:

- Standard Debug/Release solution builds and full native suites:
  **459 cases / 1,082,480 assertions** each.
- `[content][module]`: **7 cases / 2,116 assertions** each.
- Stable second generation; existing owned paths preserved; all **11**
  copier regressions pass.
- `quality-native.cmd --files Native\Tests\src\ContentTests.cpp --tidy`:
  **three existing findings, zero new**, with no baseline/suppression changes.
- `build\quality\live-restart-check.ps1 -Configuration Debug|Release
  -Art 190 -ArtRuns 3 -ObserveTiledRing`. Local evidence:
  `build\quality\tiled-ring-*`, `live-tiled-ring-*.json`, and the runs above.

This certifies the repaired source tables and the observed live consumer,
not every affected effect's appearance. Particle motion/trails and full
reference visual parity remain #76; full forty-art acceptance remains #11.

### Original art inventory and acquisition requirements

The resident action table (`SLUS_010.40`, file offset0x3C1DC, 52-byte records)
and original MENU1 category ordering define the following forty arts.
Tier costs are **25 / 40 / 55 / 75 HP** for every category. Thresholds are
the original cumulative weapon-category kill counts at resident0x405E4.

| Category / IDs | Tier 1 | Tier 2 | Tier 3 | Tier 4 | Thresholds |
| --- | --- | --- | --- | --- | --- |
| Dagger / 184-187 | Whistle Sting | Shadoweave | Double Fang | Wyrm Scorn | 20/90/235/425 |
| Sword / 188-191 | Rending Gale | Vile Scar | Cherry Ronde | Papillon Reel | 25/110/260/485 |
| Greatsword / 192-195 | Sunder | Thunderwave | Swallow Slash | Advent Sign | 18/80/210/420 |
| Axe/Mace / 196-199 | Mistral Edge | Glacial Gale | Killer Mantis | Black Nebula | 20/100/245/465 |
| Greataxe / 200-203 | Bear Claw | Accursed Umbra | Iron Ripper | Emetic Bomb | 15/90/215/410 |
| Staff / 204-207 | Sirocco | Riskbreak | Gravis Aether | Trinity Pulse | 20/95/205/385 |
| Heavy Mace / 208-211 | Bonecrusher | Quickshock | Ignis Wheel | Hex Flux | 15/95/220/405 |
| Polearm / 212-215 | Ruination | Scythe Wind | Giga Tempest | Spiral Scourge | 20/95/230/430 |
| Crossbow / 216-219 | Brimstone Hail | Heaven's Scorn | Death Wail | Sanctus Flare | 30/105/250/460 |
| Bare hands / 220-223 | Lotus Palm | Vertigo | Vermillion Aura | Retribution | 10/65/175/340 |

Qualifying BaseAttack/Ability/BreakArt kills count once, excluding Ashley,
dummy actors and already-counted deaths. The original Minotaur script flag
must be at least10; each category stops at its pending threshold or four
unlocks. Bare hands is category10 but counter index0. Normal reward handling
grants `184 + (category - 1) * 4 + tier`. These gates/thresholds have component
coverage; the Lab grants do not advance or certify them in a real campaign.
[#11](https://github.com/JEschete/VS-NativePort/issues/11) still requires broader
actual art execution, damage/secondary effects and target/range coverage;
real-story/save acceptance remains separately tracked in #17/#18.

### Weapon-family setup and empty-slot spawn checkpoint

The Lab selector described above is exercised through ordinary controller
input, not live category/action-ID edits. The following first-tier arts charge
their original 25 HP, produce positive damage matching the actual target HP
delta, and return control in both configurations:

| Choice / action | Debug run | Release run |
|---|---|---|
| Dagger / 184 | `1792700918452054-0` | `1792700625662438-0` |
| Sword / 188 | `1792700943618587-0` | `1792700666124780-0` |
| Greatsword / 192 | `1792700377365987-0` | `1792700538152386-0` |
| Axe/Mace / 196 | `1792700963422493-0` | `1792700726681466-0` |
| Greataxe / 200 | `1792700982967485-0` | `1792700750998140-0` |
| Staff / 204 | `1792701023378917-0` | `1792700775095309-0` |
| Heavy Mace / 208 | `1792701048886399-0` | `1792700800394889-0` |
| Polearm / 212 | `1792701068087802-0` | `1792700819450328-0` |
| Crossbow / 216 | `1792701091620109-0` | `1792700842904377-0` |
| Bare hands / 220 | `1792700459155502-0` | `1792700580270050-0` |

These use zone 12, room 0, seed 1, Ashley (9,10), Minotaur Zombie definition 1
at (9,11), all-break-arts and starting HP/MP 999. All except the first Debug
greatsword run explicitly set starting AGI 200; STR/INT remain original.
Greatsword and bare-hands runs each contain two entries separated by F10 with
both actors alive; Release dagger/sword also have two. All accepted traces
end with `trace_failed:false` and no error/fatal records.

An earlier default-AGI bare-hands attempt (`1792700253486406-0`) produced two
misses, then lost a menu shortcut to an enemy action and eventually game over.
It is not accepted positive-damage evidence. No health, action, category or
progression memory was edited to obtain subsequent results.

Live editor resets in Debug `1792701230063476-0` (greatsword) and Release
`1792701259882248-0` (bare hands) return to default and restore Fandango:
category 2, blade 14, grip 96, model 16. Local captures
`build/quality/weapon-editor.png` and `weapon-model-{192,216}.png` show the
readable setup row and native greatsword/crossbow scenes.

Related #123 corrects F01856's uninitialized material arguments when a weapon
or shield is absent, through `copy_fixups.py`. The pre-fix empty-slot producer
regression fails; all twelve copier regressions pass after regeneration.
The second generation is stable and preserves the owned encounter sources.
This is a bounded behavior repair, not C++ promotion or compatibility retirement.

Validation at base `5272b755` plus these scoped dirty changes:
Debug/Release solution builds and full native suites pass **466 cases /
1,082,778 assertions** each. Profile coverage checks every family against
original source records, production stat round-trips, both shield states,
unchanged stock links/capacities/other items, repeat/default setup, bare-hands
profile grants, editor navigation and invalid JSON/stock links.
Logs use `build/quality/weapon-{debug,release}-{build,tests}.log`,
`weapon-quality.log` and `weapon-final-generation*.log`.
The final scoped format, cpplint, ruff, clone and tidy checks pass with zero
findings/new findings. The selection includes the encounter model and source
headers, encounter/catalog implementations, Lab editor/trace, both encounter
test files and the two producer/regression Python files; no baseline changes.
The ignored ordinary-input driver was invoked as
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\live-restart-check.ps1 -Configuration <Debug|Release> -Weapon <choice> -Agility 200 -Art <id>`;
the repeated runs add `-ArtRuns 2`, and editor-reset runs add `-ResetWeapon`.

This establishes equipment setup and representative per-family execution,
**not** all forty arts, all secondary effects, visual parity, #76 or #82.
#11 remains open for the remaining acceptance gates.

### Thunderwave remainder repair and higher-tier checkpoint

At published `91f446c6`, ordinary Greatsword action 193 (Thunderwave) crashed
with `0xC0000094` at tick 851 in Debug run `1792702303877631-0` (#124).
The shared ribbon renderer reused the previous ribbon's final half-width
as the next spline-jitter divisor, which could be zero.

Original PLG045 `func_800FA760` confirms this reuse: `DIV`/`MFHI` at
`0x800FAA74`/`0x800FAA78`, width updates at `0x800FABB4`, `0x800FACCC` and
`0x800FB01C`, and the back edge `0x800FB2E8 -> 0x800FAA08` without reloading
the initial width. The [R3000A division contract](https://psx-spx.consoledev.net/cpuspecifications/#multiplydivide)
leaves the dividend in HI for a zero divisor; native C `% 0` instead traps.
Splitting or repeatedly clamping the width would change the original behavior.
The F01867 producer fix retains the random sample unchanged for zero width,
otherwise taking its remainder, with exactly one RNG call per sample.
The copier now applies its existing reviewed-fixup pass to shared template
bodies as well as ordinary functions. References remain read-only.

The native regression fails before the fix with the same divide-by-zero.
Afterward it exercises all twelve renderer instances, initial extents
0/16/-16, final ribbon half-widths 0/6 and zero through three ribbons. It checks
exact sample-derived UVs and RNG progression, packet counts and native links,
texture advancement and storage guards. The second generation is stable;
owned particle-render and encounter implementations are preserved.
This is bounded behavior verification, not C++ promotion or compatibility
retirement.

Higher-tier ordinary-input evidence uses the same original-data equipment,
room, target, starting stats and controller driver as the first-tier table:

| Action / HP cost | Debug run / damage | Release run / damage |
|---|---|---|
| Dagger 185 / 40 | `1792702041304012-0` / 5 | `1792702200811712-0` / 5 |
| Dagger 186 / 55 | `1792702066934572-0` / 3 | `1792702226323079-0` / 7 |
| Dagger 187 / 75 | `1792702088383033-0` / 3 | `1792702252692436-0` / 1 |
| Greatsword 193 / 40 | `1792703376728435-0` / 2, 4 | `1792703466908989-0` / 5, 5 |
| Greatsword 194 / 55 | `1792703589953818-0` / 3 | `1792703739938947-0` / 3 |
| Greatsword 195 / 75 | `1792703614501626-0` / 3 | `1792703764325378-0` / 3 |

Thunderwave uses `-ArtRuns 2`: each run contains two completed casts separated
by F10 with the actors alive. Its effect/control-return ticks are 909/1041
and 2159/2289 in Debug, 907/1039 and 2153/2283 in Release. All rows check exact
HP costs, positive target HP deltas and recovery, without runtime resource or
progression edits. Every accepted trace has one final `session.end`,
`trace_failed:false` and zero error-level records.
Dagger 185 did **not** apply Paralysis to the Zombie in the inspected Debug
trace; positive secondary-effect acceptance needs a suitable target.

Validation at base `91f446c6` plus the scoped #124 changes (unrelated dirty
documentation, solution/tool projects and reference state preserved):

- Debug/Release solution builds pass. `[effects]`: **15 cases / 13,381
  assertions**; full native suites: **467 cases / 1,094,874 assertions** each.
- `python -m unittest discover -s tools\port -p test_encounter_fixups.py`:
  **13 tests pass**. Two `copy_to_native.py` runs confirm stable output.
- Scoped quality with `--tidy` covers `ParticleRenderTests.cpp`,
  `copy_fixups.py`, `copy_to_native.py` and `test_encounter_fixups.py`:
  **0 new findings** (23 existing tooling findings); no baseline changes.
- Local evidence: `build/quality/thunderwave-{debug,release}-{build,tests,full,live}.log`,
  `thunderwave-ribbon-before.log`, `thunderwave-generation*.log`,
  `thunderwave-quality.log`, `thunderwave-trace-audit.log`,
  `greatsword-{debug,release}-{194,195}.log`, and the run directories above.
  The original disassembly is local-only in
  `build/quality/thunderwave-particle-original.asm`.

This does not establish all forty arts, positive secondary effects, complete
particle visual parity (#76), or resolution of the intermittent #82 report.

### Iron Ripper cone-emission repair

On published `8d66cf88`, Greataxe 202 (Iron Ripper) crashed at tick907 in
Debug run `1792704517072958-0`, in `Effects_EmitParticleEvent` (#125).
This is a different path from #124: cone sampling divides its radial
coordinates by the interpolated height, which can be zero.

Original BATTLE.PRG `func_800D2ADC` loads that height from scratch+0xAC.
At `0x800D33F4`, zero squared height skips the two wide-range RNG calls and
passes zero to the square root. Both subsequent dividends are therefore zero.
Raw signed DIV/MFLO at `0x800D34BC` and `0x800D34F8` has no exception guard;
original 0/0 leaves -1 in LO. F01515's producer fix preserves the original
(-1,0,-1) spawn vector for zero height, not a height clamp or skipped emission.
Nonzero scaling and the RNG sequence are unchanged.

The new native regression first reproduces the divide-by-zero. It then checks
all three cone sampling modes, heights0/32/-32 and two seeds, including exact
final Q12 particle positions, allocation/list state and RNG progression.
The existing empty/full-capacity regression shares the same emission fixture.
Original disassembly remains local-only:
`build/quality/iron-ripper-emitter-original.asm`.

Further ordinary-controller runs use the same original equipment/room/target
and starting stats as the preceding tables:

| Action / HP cost | Debug run / damage | Release run / damage |
|---|---|---|
| Sword 189 / 40 | `1792704155505905-0` / 2 | `1792704263778625-0` / 5 |
| Sword 190 / 55 | `1792704180944698-0` / 4 | `1792704288864676-0` / 4 |
| Sword 191 / 75 | `1792704210885933-0` / 1 | `1792704318464012-0` / 1 |
| Axe/Mace 197 / 40 | `1792704385777127-0` / 2 | `1792704438830729-0` / 2 |
| Axe/Mace 199 / 75 | `1792704411812390-0` / 3 | `1792704464690623-0` / 3 |
| Greataxe 201 / 40 | `1792704491491807-0` / 5 | `1792705469391615-0` / 5 |
| Greataxe 202 / 55 | `1792705287321021-0` / 5, 3 | `1792705333411997-0` / 1, 3 |
| Greataxe 203 / 75 | `1792705442707695-0` / 3 | `1792705494631337-0` / 3 |

Iron Ripper adds `-ArtRuns 2`: each configuration includes two completed casts
and F10 re-entry with actors alive. Debug effect/control-return ticks are
909/1027 and2141/2259; Release907/1025 and2136/2256. All rows verify original
HP costs, exact positive HP damage and recovery. All sixteen accepted traces
have final `session.end`, `trace_failed:false` and no error-level records.
Sword/Axe-Mace and the first Debug201 runs use published `8d66cf88` native code;
Iron Ripper and the remaining Greataxe runs use that base plus the scoped #125
changes. Unrelated dirty documentation/projects/reference state is preserved.

Debug/Release solution builds pass. `[effects]` passes **16 cases /13,507
assertions**, and each full suite passes **468 cases /1,095,000 assertions**.
Fourteen producer regressions pass; second generation is stable and preserves
owned sources. Scoped format/cpplint/Ruff/clones/tidy for the test and two
producer/regression files has **zero findings/new findings**, with no baseline
changes. Evidence uses `build/quality/iron-ripper-{debug,release}-{build,tests,full,live}.log`,
`iron-ripper-cone-before.log`, `iron-ripper-generation*.log`,
`iron-ripper-quality.log`, `iron-ripper-trace-audit.log`, and the run directories.

Positive execution coverage is now **24/40 arts** in both configurations.
MP damage, positive secondary effects, remaining arts and eligibility/target
gates still require #11 acceptance. This repair does not establish complete
visual parity, C++ promotion, compatibility retirement, or resolution of #82.

### Further art effects and Heaven's Scorn audio initialization (#126)

The following ordinary-controller runs use published `54c62f9a`, except
Heaven's Scorn, which adds the scoped #126 initialization repair. The original
weapon fixtures, HP/MP999 and AGI200 are unchanged. All accepted final traces
have one `session.end`, `trace_failed:false` and no error-level records.

| Action / HP cost | Debug run / HP damage | Release run / HP damage |
|---|---|---|
| Staff 206 / 55 | `1792705858803243-0` / 3 | `1792705914558541-0` / 3 |
| Staff 207 / 75 | `1792705886136965-0` / 1 | `1792705941743300-0` / 1 |
| Heavy Mace 209 / 40 | `1792706082613957-0` / 5 | `1792706157464890-0` / 2 |
| Heavy Mace 210 / 55 | `1792706108251707-0` / 4 | `1792706182807394-0` / 9 |
| Heavy Mace 211 / 75 | `1792706133751022-0` / 17 | `1792706208241735-0` / 12 |
| Killer Mantis 198 / 55 | `1792706540104270-0` / 49 | `1792706827008737-0` / 49 |
| Riskbreak 205 / 40 | `1792706757859308-0` / 3 | `1792706801367369-0` / 3 |
| Heaven's Scorn 217 / 40 | `1792707904346821-0` / 4, 5 | `1792708067805725-0` / 8, 5 |

Killer Mantis uses a separate all-break-arts living fixture: zone9 room0 seed1,
Ashley (4,9), Sackheim definition6 at (4,11), original enemy HP150/MP15.
Both builds apply **13 MP damage** as well as the reported HP damage, with the
exact immediate MP delta verified. The original item fixture is not modified.
Riskbreak uses the real Lab F3 input to establish Risk25, then reduces Ashley's
Risk to24: the original source-side effect is half of the preceding damage,
truncated. It occurs four ticks after the target damage, not in the same
frame. An initial harness assertion at the target boundary was premature;
the game required no Riskbreak repair. Natural vitals decay runs in control
modes0/1, whereas this observed change is in mode2.

Positive ailment checks use the same living fixture and require a newly
applied flag, not an already-present status:

| Art / effect | Debug run / HP damage | Release run / HP damage |
|---|---|---|
| Shadoweave185 / Paralysis | `1792706863540543-0` / 44 | `1792707052127721-0` / 45 |
| Vile Scar189 / Poison | `1792706888414492-0` / 49 | `1792707076783698-0` / 49 |
| Accursed Umbra201 / Curse | `1792706912829224-0` / 50 | `1792707101131744-0` / 47 |
| Quickshock209 / Numbness | `1792706937529827-0` / 83 | `1792707125516404-0` / 84 |
| Vertigo221 / Numbness | `1792706962171925-0` / 38 | `1792707150002854-0` / 38 |

The exact original initial timer values are15 in slot3 for Paralysis and
slot4 for Numbness; Poison uses interval2 in slot5. Curse has no duration
timer. The flags are respectively0x2000,0x4000,0x10000 and0x8000. These checks
establish art delivery; they do not replace the separate status-mechanics
coverage. Commands add `-ArtLivingTarget -RequireSecondary` for these cases
and198, or only `-RequireSecondary` for205.

Heaven's Scorn initially crashed at tick928 in
`Audio_Mixer_UpdateChannelSlidesAndDelays`, run `1792707252049210-0`.
A read-only PDB-layout-checked observer reproduced it at tick926 in
`1792707507227388-0`: voice15 had no auto-pan waveform, counter0, and no
auto-pan enable flag before executing the original `DF 48 01` depth slide.
The bytes are in E164.P at offset0x173B. Original resident instructions
0x8001427C-0x80014318 also read the waveform whenever that counter is not1.
The raw zero-address read is not portable to native pointers.

Owned `Audio_Driver_ResetPlaybackState` now establishes the pending
first-sample phase1 for unconfigured vibrato, tremolo and auto-pan oscillators
in all32 music and12 effect channels. This uses the same pending phase as
the original B4/B8/BC enable commands. Already-configured waveforms and phase
counters remain untouched. No waveform, sound program, depth progression,
enabled modulation or effect emission is skipped or replaced. This is an
explicit native initialization contract, not emulation of arbitrary low-RAM
reads or a claim of complete reference waveform parity.

The regression fails in all six music/effect modulation combinations before
the fix. It verifies all44 channels, each depth-slide step, subsequent genuine
waveform playback and exact modulation values, cached-state preservation and
repeat reset. Debug/Release solution builds pass; `[audio]` passes **71 cases /
162,977 assertions** each, and full suites pass **469 cases /1,095,006
assertions** each. Scoped format/cpplint/Ruff/clones/tidy has **zero new
findings** (38 existing test findings), without baseline/suppression changes.
No generator change is needed for this registered owned implementation.

Each accepted Heaven's Scorn run contains two casts and live-enemy F10
re-entry. Effect/control-return ticks are Debug897/1059 and2161/2323,
Release893/1055 and2157/2319. Evidence is in the run directories and
`build/quality/audio126-{debug,release}-{build,tests,full,live}.log`,
`audio126-before-tests.log`, `audio126-observed-repro.log`, and
`audio126-quality.log`. The observer's local sequence bytes are not committed.

Positive execution is now **33/40 arts in both builds**. Additional Debug-only
Polearm214/215 runs `1792707195424468-0` and `1792707221602121-0` each deal1
damage and recover, with clean final traces. The complete forty-record audit
also identifies remaining positive secondary coverage: Thunderwave193
Paralysis, Glacial Gale197 Numbness, Scythe Wind213 equipment-down, and
Iron Ripper202/Giga Tempest214 extra armor wear. Nine arts have a second
physical-damage hit. Remaining arts, these effects and eligibility/target
gates keep #11 open; #76 and #82 are not resolved by these checks.

### Forty-art damage and secondary-effect checkpoint

Positive ordinary-controller execution now covers **all40 arts in both Debug
and Release**. The remaining runs below use `07953575` and the same original
weapon fixtures, seed1 and HP/MP999/AGI200. Every accepted trace has exactly
one `session.end`, `trace_failed:false` and no error-level engine entries.
HP costs, immediate reported damage, recovery and unchanged inventory and
acquisition/story progression were checked by the live driver.

| Art / HP cost | Debug run / HP damage | Release run / HP damage |
|---|---|---|
| Giga Tempest214 / 55 | `1792707195424468-0` / 1 | `1792708623983606-0` / 5 |
| Spiral Scourge215 / 75 | `1792707221602121-0` / 1 | `1792708649879492-0` / 1 |
| Death Wail218 / 55 | `1792708513516595-0` / 9 | `1792708680207524-0` / 9 |
| Sanctus Flare219 / 75 | `1792708540704239-0` / 9 | `1792708707225182-0` / 9 |
| Torpedo222 / 55 | `1792708569135588-0` / 2 | `1792708735568615-0` / 8 |
| Retribution223 / 75 | `1792708596134006-0` / 3 | `1792708762333533-0` / 3 |

Three further living-target runs complete positive delivery of all eight
status-inflicting arts:

| Art / newly applied status | Debug run / HP damage | Release run / HP damage |
|---|---|---|
| Thunderwave193 / Paralysis | `1792708900706133-0` / 54 | `1792708973838771-0` / 55 |
| Glacial Gale197 / Numbness | `1792708925480198-0` / 47 | `1792708998430754-0` / 48 |
| Scythe Wind213 / equipment-down | `1792708950350244-0` / 49 | `1792709023262919-0` / 46 |

All three cost40HP. Original status19 uses mask0x80000 and timer slot2=15;
the other status contracts are listed above.

Iron Ripper202 and Giga Tempest214 also pass positive armor-wear acceptance
in the living Sackheim fixture. A read-only observer checks the compiled
actor/armor layouts against the current PDB before reading them. It samples
DP after normal hit-equipment wear, at HP-cost payment but before impact,
and again after recovery. Enemy HP is checked at both boundaries.

| Art | Debug run | Release run | HP damage | Selected limb0 DP | Shield DP |
|---|---|---|---|---|---|
| Iron Ripper202 | `1792709260707089-0` | `1792709350998616-0` | 54 | 3446 -> 3230 | 5296 -> 5188 |
| Giga Tempest214 | `1792709325935945-0` | `1792709375332840-0` | 48 | 3452 -> 3260 | 5302 -> 5206 |

These are the original **4*damage limb / 2*damage shield** additional losses:
effect35 queues 2*damage in both the per-limb field and the general armor
field. HP application subtracts the former, then the armor-DP switch applies
the latter to the selected limb and shield. This is not a newly introduced
double-subtraction defect. The four final traces were separately audited
clean. The DP snapshots are in the local driver result files, not ordinary
native frame telemetry.

The maintained `[art-effects]` tests add two cases / **1,521 assertions**:

- All nine two-physical-hit arts (186/194/210/211/216/217/218/219/222), with
  original damage types, affinities, factors and dispatch IDs checked against
  the resident action table and BATTLE.PRG tables. Factor0 means100% of base
  attack, not zero damage. Independently calculated expectations verify each
  hit and accumulation. The production target resolver also produces their
  sum, rejects canceled target results through the second-hit prerequisite,
  and commits the resolved HP delta. This is deterministic component
  coverage; the actual-controller executions above remain separate evidence.
- Both armor-wearing arts, all six limb slots, and DP values
  0/1/39/40/41/79/80/81/65535: both original subtraction stages saturate at
  zero, shield loss is exact, and unrelated limbs stay unchanged.

Validation on `07953575` plus these scoped tests: Debug/Release test-project
builds and full native suites pass **471 cases / 1,096,527 assertions** each.
Scoped format/cpplint/Ruff/clones/tidy reports **0 new findings**, with one
existing test finding and no baseline changes. Commands are the documented
MSBuild invocation targeting `Native\Tests\VS.Tests.vcxproj`, then each
`build\{Debug,Release}\VS.Tests.exe`, and
`.\.venv-3\Scripts\python.exe tools\lint\quality.py --files Native\Tests\src\CombatTestCases.cpp --tidy`.
Local logs use `build/quality/art-effects-*`. Unrelated worktree changes were
preserved; no production behavior or compatibility-retirement status changed.

### All-art live failure and cancellation acceptance

The expanded sweep passes **60 live encounters / 400 gate selections**:
each of the40 original arts in both Debug and Release independently passes
empty-target confirmation, empty-target cancellation, insufficient HP,
out-of-range confirmation, and cancellation with a valid enemy target.
All60 final traces were audited for exactly one `session.end`,
`trace_failed:false`, no error-level engine entries and **no art result
executed during a rejected/canceled selection**. Each result file was also
checked for the exact four action IDs in every required phase.

All encounters use zone12 room0, seed1, Ashley(9,10), **all-break-arts**,
the selected original weapon family and AGI200:

- **Solo:** HP975/MP999, no enemies. All four arts confirm an empty target
  list and return without payment, then cancel from targeting without
  payment. Actual Lab F1 inputs reduce HP to1; all four rows become disabled
  and all four confirmation attempts stay in the menu without payment.
  Natural field HP regeneration remains enabled and was observed by the
  driver. Exact cost-1/cost/cost+1 boundaries for every art remain separately
  covered by the maintained component tests.
- **Range:** HP/MP999, Minotaur Zombie definition1 at(9,16). All four art
  previews contain no eligible target and confirmation returns to the menu
  without payment.
- **Valid cancel:** HP/MP999, the same enemy at(9,11). Each art first exposes
  a valid enemy limb, then actual cancel returns without payment. Normal
  enemy actions after leaving the menu are allowed to finish; they are not
  misclassified as an unexpected player action.

The checks preserve inventory, learned actions and acquisition/story
progression. HP/MP/Risk/statuses are compared across the target selections,
and field control recovers. No process-memory writes, forced completion,
event-flag edits or enemy-HP changes are used. All owned hosts closed normally.

Each row covers its first art and the next three IDs:

| Family / first art | Debug solo | Debug range | Debug valid cancel | Release solo | Release range | Release valid cancel |
|---|---|---|---|---|---|---|
| Dagger184 | `1792710696173852-0` | `1792710834452291-0` | `1792710858375735-0` | `1792711717601532-0` | `1792711764315286-0` | `1792711787766854-0` |
| Sword188 | `1792710383679562-0` | `1792710892161165-0` | `1792710916022939-0` | `1792711820992073-0` | `1792711867850780-0` | `1792711891360867-0` |
| Greatsword192 | `1792710949715270-0` | `1792710996899535-0` | `1792711020618147-0` | `1792711924452702-0` | `1792711971318239-0` | `1792711994759752-0` |
| Axe/Mace196 | `1792711054152786-0` | `1792711101349890-0` | `1792711125206079-0` | `1792712027942091-0` | `1792712074830428-0` | `1792712098290892-0` |
| Greataxe200 | `1792711158923035-0` | `1792711206137175-0` | `1792711229805759-0` | `1792712131693442-0` | `1792712178633593-0` | `1792712201978468-0` |
| Staff204 | `1792711263126101-0` | `1792711310099752-0` | `1792711333772484-0` | `1792712235320514-0` | `1792712282091114-0` | `1792712305521334-0` |
| Heavy Mace208 | `1792711362334712-0` | `1792711409305521-0` | `1792711433102521-0` | `1792712334321822-0` | `1792712381276998-0` | `1792712404791657-0` |
| Polearm212 | `1792711466683888-0` | `1792711513933375-0` | `1792711537469301-0` | `1792712437988658-0` | `1792712485040102-0` | `1792712508529948-0` |
| Crossbow216 | `1792711570957927-0` | `1792710512551943-0` | `1792710624124207-0` | `1792712541761048-0` | `1792712588742286-0` | `1792712612404404-0` |
| Bare hands220 | `1792711618285959-0` | `1792711665510567-0` | `1792711689075547-0` | `1792712645454127-0` | `1792712692353187-0` | `1792712715684502-0` |

Reproduction uses the ignored local driver:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\live-restart-check.ps1 -Configuration Debug -Weapon sword -Agility 200 -Art 188 -ArtGate solo`.
Select Debug/Release, the family/first art, and solo/range/cancel. The bounded
batch is `build/quality/run-art-gates.ps1`; its audited index is
`build/quality/art-gates-batch.json`. Logs and result snapshots are
`art-gates-{gate}-{configuration}-{first-art}.log` and
`live-break-art-{first-art}-{gate}-{configuration}.json` under `build/quality`.
Native session logs remain under the corresponding Combat Lab run directory.

The unchanged production hosts are built from `07953575`; the worktree
advanced to tests/documentation-only `f432f475` during the sweep, preserving
unrelated dirty edits. That revision's Source Quality
[39870437965](https://github.com/JEschete/VS-NativePort/actions/runs/39870437965)
passed every step. Host SHA-256 values:

- Debug: `dce76c4bb089fa35cc92bf77d982475e3c550924fd6057b32d9b6b624bb7ef16`.
- Release: `c143f8ea673b21b0eed271bcbd9873fa519223ed28da22dbf42e73d21a54d74c`.

Together with the all-category menu/equipment checks, all40 cost/eligibility
and acquisition contracts, positive damage/recovery runs and secondary-effect
coverage above, this completes #11's bounded functional acceptance. Full
particle/trail visual parity (#76) remains open. GitHub records the separate
F10 report (#82) as closed at2026-10-22T21:58:16Z, independently of this
acceptance; these gates establish no new F10 root cause or lifecycle fix.

## Loot-transfer capacity checkpoint (#127)

While tracing #12, a maintained regression reproduced a rejected miscellaneous
loot claim changing an existing99 stack to100, then duplicating that item on
retry: after freeing one slot, the total was102 instead of101. The original
64 slots and100-per-stack limit are unchanged. The original decomp has the same
mutation order; this is a native transactional correction, not a claimed
assembly-translation mismatch.

F02511 (`vs_mainMenu_copyItem`, `MENU/MAINMENU.PRG/2D10.c:1334`) now probes the
existing non-writing capacity path before merging a write. A rejected claim
therefore leaves every stack untouched and its source loot row intact. The
repair is in `tools/port/copy_fixups.py`, not a manual generated-C edit.
Equipment copying and drop selection/RNG are unchanged.

Acceptance uses source `98cdc051` plus the scoped generator repair/tests,
preserving unrelated dirty documentation, project files and reference state:

- Before repair: `[inventory-capacity]` failed2 of7 assertions, measuring both
  the rejected mutation and the duplicated retry.
- After repair: **3 cases /28,767 assertions** in both Debug and Release.
  Coverage includes96 combinations of partial/full matching stacks, incoming
  counts and free capacity; four unmatched-item occupancy cases; and the
  original reject/free/retry regression. Queries and rejected writes preserve
  every slot, successful copies conserve quantity, source stacks remain owned
  by the caller, and successful one-based return indices are checked.
- Debug and Release solution builds and full suites pass **474 cases /
  1,125,294 assertions** each. Scoped quality/tidy has one existing finding,
  zero new findings; no baseline or suppression changes.
- `copy_to_native.py` regenerates only the intended inventory implementation.
  All86 owned files are unchanged; a second generation preserves all751 output
  hashes. All15 `test_encounter_fixups.py` regressions pass.

Real controller replays use zone9/room0, seed1, the original Zombie definition2,
`all-abilities`, HP/MP999, Ashley(4,9) and Zombie(4,10). Heavy Shot ends an actual
nine-window chain, then Square/take-all delivers item323 x2 through the ordinary
loot screen, increasing inventory10->12:

| Configuration | Run | Defeat tick | Field-return tick |
|---|---|---:|---:|
| Debug | `1792714592030967-0` |1279|1613|
| Release | `1792714659101918-0` |1273|1605|

Both traces have one normal final record, `trace_failed:false`, no error-level
engine entries and normally closed hosts. No HP, loot or event flags were
written to manufacture defeat or collection. These runs verify successful
live delivery; full-capacity rejection/retry is component coverage, not a
claimed live full-inventory replay. Wider original-drop acceptance remains #12.

Commands: the documented solution build in each configuration,
`build\<configuration>\VS.Tests.exe "[inventory-capacity]"`, the full test
executable, `python -m unittest discover -s tools\port -p test_encounter_fixups.py`,
and `build\quality\live-restart-check.ps1 -Configuration <Debug|Release> -ChainDefeat`.
Scoped quality selects `Native\Tests\src\CombatTestCases.cpp`,
`tools\port\copy_fixups.py` and `tools\port\test_encounter_fixups.py` with `--tidy`.
Build/test/generation/quality logs are `build\quality\loot127-*`; controller
results are `live-chain-defeat-{Debug,Release}.json`. Host SHA-256 values:

- Debug: `c0bf2b0f3ee1214f05f5a7f7fb275679cfcae940408d4d6d918093953684936b`.
- Release: `9984fb32ff715e64bce1df2f1bd3d500e13054af6fc8b24bd68a22211e0cd7c1`.

## Enemy-item drop acceptance checkpoint (#12)

Enemy miscellaneous loot comes from each original MPD spawn, not a universal
enemy-ID table. `Actors_SpawnRoomEnemy` copies the two guaranteed ID/count pairs
and the third ID/rate into actor stats. Within each40-byte spawn, the IDs are
at offsets0x1A/0x1C/0x1E, the guaranteed counts at0x20/0x21, and the third
**rate**, not quantity, at0x22. A successful third entry contributes one item.

Read-only Workshop inspection of the supplied SLUS-01040 disc and the native
catalog's original spawn provenance agree on these representative entries:

| Definition | Original source | Guaranteed entries | Optional entry/rate |
|---|---|---|---|
| Zombie, zone9 definition2 | `MAP011.MPD`, spawn1 |323 x2|none|
| Bat, zone9 definition5 | `MAP009.MPD`, spawn0 |none|none|
| Dullahan, zone11 definition0 | `MAP025.MPD`, spawn0 |349 x1;350 x1|391 /255|
| Minotaur, zone12 definition0 | `MAP026.MPD`, spawn0 |466 x1;425 x1|432 /255|

The Workshop session closed with zero changes. No asset bytes, disc files,
captures or dumps are published. Lab placement retains the original items,
rates, stats, equipment and AI; only Ashley's documented test profile/stat
overrides and encounter placement are configured.

### Original contracts and deterministic coverage

Eight new `[drop-rules]` cases cover **106,926 assertions**:

- Both guaranteed entries preserve their individual counts and consume no RNG.
- For every possible roll0..254, test rate0, the equal threshold, the next
  threshold and255. Success is strictly `roll < rate`; rate0 never succeeds,
  rate255 always succeeds, and an absent ID consumes no draw. Failed candidates
  retain an ID in scratch but have zero count/flags and never become claimable.
- Queue order consumes weapon, shield and accessory rolls even for blank/rate0
  loadouts, then one roll per nonempty limb armor, then the optional item roll.
  Compare against the existing native C++ random sequence, including the
  following raw draw; test zero/one/three/six armored limbs and zero/one/two
  guaranteed entries.
- Staging filters missing IDs, zero counts and both difficulty masks, then
  transfers accepted rows through the shared inventory producer. No-drop,
  single and multiple miscellaneous entries are covered.
- `Battle_ResolveEnemyDefeat` rejects spawn0x80 and repeated defeat before RNG,
  including actors retained for a story event. A direct duplicate queue call
  has different original behavior: it rolls first, rejects the duplicate node,
  and releases that allocation. Distinct actors retain separate nodes.
- Prologue progression0..2 suppresses presentation without discarding queued
  loot. Clearing the list restores all allocated heap units and is repeatable.

The copied C uses the existing `/J` unsigned-char configuration; test-side
high-bit rate assignments preserve those serialized byte patterns. Their
passing0..255 behavior is not evidence of a signed-rate defect.

Together with #127's capacity/retry checks, the focused selection
`"[drop-rules],[inventory-capacity]"` passes **11 cases /135,693 assertions**
in Debug and Release. Full suites pass **482 cases /1,232,220 assertions** each.
Scoped quality/tidy has one existing finding and zero new findings, without
baseline or suppression changes. Equipment success/composite ownership and
capacity acceptance remain #13; these tests cover their RNG consumption only.

### Fresh controller acceptance

Production code and host hashes are unchanged from the preceding #127
checkpoint (`a3a8cf1e`); this checkpoint adds tests, ledger notes and evidence,
with unrelated dirty work preserved. Zombie's real weapon/Heavy Shot defeat
and Square/take-all runs are recorded above.

The following six sessions add twelve real Fireball defeats, one F10/re-entry
per session, original drop collection and field return. Ashley uses
`all-spells`, HP/MP/INT999 and AGI200; enemy data is unmodified. Dullahan uses
Ashley(8,13)/enemy(8,14) in zone11/room0; Minotaur uses(9,10)/(9,12) in
zone12/room0; Bat uses(4,9)/(4,10) in zone9/room0. All use seed1 and isolated
room events. A flying target must descend into range before opening the real
selector; the driver does not move it or bypass eligibility.

| Enemy | Debug run; field-return ticks | Release run; field-return ticks |
|---|---|---|
| Minotaur | `1792716281788772-0`;1343/2899 | `1792716433466051-0`;1343/2895 |
| Dullahan | `1792716338442156-0`;1205/2618 | `1792716489889274-0`;1203/2616 |
| Bat | `1792716596414869-0`;1063/2337 | `1792716700327985-0`;1063/2338 |

Each boss delivers exactly the three entries in the source table, once per
encounter. Minotaur's item466 changes its original acquisition flag904 from0
to1 in both fresh instances. Bat delivers no miscellaneous items and leaves
that inventory unchanged. Every returned frame has an empty pending-loot list.
All six traces have exactly one normal final record, no trace failure and no
error-level engine entries; all hosts close normally.

Commands: the documented test-project builds and full test executables in
both configurations; the focused selection above; quality/tidy selecting
`Native\Tests\src\CombatTestCases.cpp`; and the ignored live driver:

```powershell
build\quality\live-restart-check.ps1 -Configuration Debug `
  -DropScenario build\quality\drop12-minotaur.json `
  -DropExpected '466:1,425:1,432:1' -DropRuns 2
```

Repeat for Release. Dullahan uses `drop12-dullahan.json` and
`'349:1,350:1,391:1'`; Bat uses `drop12-bat.json` without `-DropExpected`.
Evidence is `build\quality\drop12-*`, `live-drop12-*-{Debug,Release}.json` and
the corresponding native run directories. `drop12-live-audit.json` records
the six audited sessions. Initial attempts exposed harness assumptions
(requiring living enemies after victory, integer JSON keys, opening the
selector before Bat descended), not demonstrated native defects.

This completes #12's item-selection/delivery criteria with original data,
deterministic rules and representative ordinary/special/boss encounters. It
does not certify every enemy/room permutation, original boss-story scripts,
door traversal with the acquired key, equipment drops #13 or C++/compatibility
retirement. Isolated Lab boss-retention policy is documented separately above.

## Equipment-drop and staging acceptance (#13; #128)

The initial, pre-repair checkpoint used
`bc76bfc68d36424d972f623f34bdb531b26f68ff`, unchanged from
`a3a8cf1e`, with uncommitted equipment tests. Eight `[equipment-drops]`
cases pass547,962 assertions in Debug and Release. Full native suites at that
checkpoint pass490 cases/1,780,182 assertions each. Scoped quality/tidy reports
one existing finding, zero new, without changing baselines.

Coverage includes every roll0..254 against rates0/equal/next/255 for weapon,
shield, armor and accessory; source ownership; independently checked IDs,
models, materials, DP/PP, signed modifiers, types, six classes/seven affinities,
ranges, gem effects/slots and24-byte names. The original drop writers use
embedded component DP/PP, not the aggregate battle-worn loadout values; the
reference C distinguishes this from storing player equipment. Tests preserve
that contract. All six eligible limb rolls still occur after the original
two armor outputs fill.1080 weapon/shield capacity combinations cover zero to
three gems, exact last-slot claims, full component bags, query/rejected-write
immutability and rewritten component ownership. Armor/accessory tests cover
their shared16-slot bag; staged rows cover both difficulty masks and
availability refresh as space runs out.

The pre-repair live input used all-spells Ashley, HP/MP/INT999 and AGI200, real Fireball
selection/casting and Square/take-all. Enemy definitions, rates and equipment
are original. PDB-checked observation only reads memory: it compares actor
component properties to pending loot, then claimed inventory and owner
indices. It never edits enemy HP, inventory, random state or event flags.
Source inspection used the read-only Workshop disc session and extracted
original ZND/MPD records; the session closed with no changes.

| Enemy / original definition | Debug run; return ticks | Release run; return ticks |
|---|---|---|
| Mandel, zone9/0 | `1792718882365880-0`;1203/2619 | `1792719195631906-0`;1203/2610 |
| Minotaur Zombie, zone12/1 | `1792718948158223-0`;1309/2829 | `1792719324190495-0`;1307/2828 |
| Lizardman, zone14/0 | `1792719030173223-0`;1207/2634 | `1792719411874537-0`;1207/2624 |
| Dullahan, zone11/0 | `1792718758997320-0`;1213 | `1792719507770480-0`;1205 |
| Zombie Knight, zone40/10, seed42 | `1792719644198662-0`;1205 | `1792719733695916-0`;1201 |

Mandel delivers weapon blade15/grip96/material3. Lizardman delivers weapon
blade71/grip116/material4 and armor162/material2, plus original key467.
Minotaur Zombie delivers accessory224, plus items324x3/349x1. Dullahan's
weapon rate0 delivers no equipment. Knight's original shield132 rate32/255
delivers that shield with material5 in Debug; Release seed42 instead delivers
armor164/180. Different timed input runs are not asserted to consume identical
random draws. Debug Knight seed1 (`1792719117691360-0`, three encounters) and
seed2 (`1792719282697110-0`, one encounter) deliver no equipment. These are
twenty real defeats across twelve sessions, including fresh F10/re-entry.
Live attached-gem delivery and live full-player-bag rejection are not claimed;
their coverage is component-level.

Host hashes remain the #12 hashes listed above. Exact commands use the
documented test-project builds/full executables, `[equipment-drops]`, and:

```powershell
build\quality\live-restart-check.ps1 -Configuration Debug `
  -DropScenario build\quality\drop13-mandel.json -ObserveEquipment -DropRuns 2
```

Repeat for Release. The Minotaur Zombie preset adds
`-DropExpected '324:3,349:1'`; Lizardman adds `'467:1'`; Dullahan uses the
existing `drop12-dullahan.json` and `'349:1,350:1,391:1'`. Knight presets use
`-DropOptional '332:32'`, which permits only the original optional item at
count1 and still rejects unexpected IDs/counts. Evidence is
`build\quality\equipment13-*`, `live-equipment13-*.json`, the ignored bounded
observer `equipment13-observe.ps1` and the native run folders. The first new
Dullahan observer result is named `live-equipment13-drop12-dullahan-Debug.json`;
the earlier #12 Dullahan run itself remains in its original native run folder.

The subsequently added `[staging-capacity]` regression initially failed:
1 case/366 assertions,14 failures, exit42, recorded in
`equipment13-staging-red.log`. Each staging helper rejects its final valid
slot; the caller appends a zero staging result as an invalid item index.
#128 records this before repair. Correcting only those bounds is insufficient:
component staging must be atomic, and batches exceeding temporary storage
must retain excess loot without silent loss or duplication. Original Lich/
Lich Lord records can yield two armors plus an accessory; eight supported Lab
enemies can exceed the temporary16 armor slots. No live crash was deliberately
triggered or claimed. #128 was logged before repair.

### Bounded staging repair

The copier now uses every original staging slot and preflights complete
weapon/shield component storage before writing. Failed staging never emits
an invalid row. Excess records remain pending for successive batches;
**player bag capacities and temporary item arrays are unchanged**.

Taking all rows advances to the next batch after claim processing finishes.
With rows remaining, the existing positive END confirmation discards only
the visible batch and then presents the next batch. Cancel leaves the current
batch intact. Existing mandatory-key protection still prevents abandonment.
Each new batch starts on its first row. Source references are reset before
source nodes or working storage are freed. Single-page fallback chest data
remains unchanged when the menu is abandoned.

Only row metadata and its matching organization backups grow from160 to176
entries, covering the original physical slots even after disassembly. This
is a bounded compatibility repair, not #58's native inventory modernization,
C++ promotion, bridge retirement or permission to enlarge player inventories.

Final local verification used `bc76bfc6` plus this scoped dirty repair,
preserving unrelated documentation/project/reference edits:

- Debug and Release:27 cases/688,244 assertions with
  `[equipment-drops],[staging-capacity],[drop-rules],[inventory-capacity]`.
- Full native suites:498 cases/1,784,771 assertions each.
- All seven final-slot/full-rejection cases and1080 composite-staging
  combinations pass. A24-armor source batch surfaces every ID exactly once
  across16+8 rows without extra RNG draws. Claim and confirmed-discard variants
  are separate; discarded records are not counted as delivered.
- Three `[loot-paging-ui]` component cases call the production menu state
  machine for cancel, positive leave, take-all, key protection and first-row
  reset. They caught and fixed a next-page cursor error after skipped source
  records. These are not live-controller overflow replays: animation readiness
  is supplied by the fixture, and freeing bag space between batches is a
  component precondition rather than a tested organization-menu interaction.
- 18 copier regressions pass. A second generation is byte-stable across751
  outputs and preserves87 maintained paths, including the new paging header.
- Scoped format/cpplint/Ruff/clone/tidy validation reports one existing finding
  and zero new; no baseline was changed. Paging-header style also passes.

Commands use the documented Debug/Release test-project and Combat Lab
MSBuild invocations with `/nodeReuse:false /p:VcpkgManifestInstall=false`.
Additional checks:

```powershell
.\.venv-3\Scripts\python.exe -m unittest discover -s tools\port -p test_encounter_fixups.py
.\.venv-3\Scripts\python.exe tools\port\copy_to_native.py
.\.venv-3\Scripts\python.exe tools\lint\quality.py --files Native\Tests\src\CombatTestCases.cpp tools\port\copy_fixups.py tools\port\test_encounter_fixups.py --tidy
.\.venv-3\Scripts\python.exe tools\lint\quality.py --files Native\Game\src\Menu\Loot\Paging.h
```

Evidence is under `build\quality\staging128-*`. Final rebuilt Combat Lab
SHA-256 values:

- Debug:`a959e115b467f9b553cf9f5694a50413b3c255ff155f77043adad1a83f2a5a3a`.
- Release:`b137b23347345bd7054f81bcf38f59b9f7b33cac1ad4cc425fb91a6fd0476f0c`.

The earlier equipment-run table above belongs to the pre-repair hosts, not
these binaries. Final-host ordinary-controller acceptance passed20 real
Fireball defeats across12 sessions:

| Scenario / menu operation | Debug run; return ticks | Release run; return ticks |
| --- | --- | --- |
| Mandel: END, cancel, take-all; F10/re-entry | `1792722824230910-0`;1325/2861 | `1792723097018253-0`;1315/2843 |
| Mandel: positively leave equipment behind | `1792722879802372-0`;1267 | `1792723152301108-0`;1263 |
| Lizardman: key-protected END, cancel, take-all; F10/re-entry | `1792722908823420-0`;1331/2876 | `1792723181002783-0`;1323/2860 |
| Minotaur Zombie: accessory take-all; F10/re-entry | `1792722964672507-0`;1303/2815 | `1792723236325913-0`;1301/2813 |
| Dullahan: zero-rate equipment plus ordinary items | `1792723019540287-0`;1203 | `1792723291052989-0`;1199 |
| Zombie Knight seed42: probabilistic outcomes; F10/re-entry | `1792723047527643-0`;1197/2489 | `1792723318866241-0`;1197/2609 |

Read-only observations validate the compiled layouts, original actor component
properties, pending records, newly occupied inventory slots and ownership links.
Cancel retains pending equipment; positive leave delivers none. Key467 is
claimed exactly once and its acquisition flag changes only on pickup. All
owned hosts exit. No enemy HP/rate/loot flags were edited to obtain this
acceptance. Timed seeds do not guarantee identical rolls between builds:
the final Debug Knight first encounter yields a weapon and its second no
equipment; Release yields armor then no equipment. The earlier shield132
pickup remains pre-repair evidence, not a new final-host shield replay.

The ignored driver uses `-ObserveEquipment -DropChoice cancel -DropRuns 2`
for Mandel/Lizardman, `-DropChoice leave` for the single Mandel discard, and
normal claims for the remaining cases. Expected misc entries remain
`467:1`, `324:3,349:1`, and `349:1,350:1,391:1` respectively; Knight allows only
its original optional `332:32` candidate. Evidence is in
`staging128-live-*.log`, `live-staging128-*.json` and the native run folders.
See #13/#128 for the published revision and exact-revision CI; no issue is
closed on local test results alone.

## Learned-spell acceptance checkpoint (2026-10-22)

This closes the bounded representative acceptance of
[#9](https://github.com/JEschete/VS-NativePort/issues/9), not every spell/target
permutation. Replays use published `9b76aabffeb674f55c29ad446a4cdb76545324ba`;
native code is unchanged from fully validated `ad6b2adb`. Unrelated dirty
documentation, solution/tool-project files and reference state were preserved.
The relevant implemented fixes are the all-spells fixture (`fa794fab`),
elevation plane (`65f3c8f1`), rank selector (`5aa6adab`) and actual-caster
telemetry (`b6e0a84a`). No new gameplay patch or progression bypass was needed.

Original contracts checked against production code and source:

- [Item selection](../Native/Game/src/Menu/Inventory/Inventory.c) maps a book
  to its spell and advances only through the four original ranked action IDs.
  A book can teach an unlearned action; a committed use consumes a copy.
  [Learned selection](../Native/Game/src/Menu/Magic.c) instead lists unlocked
  actions/ranks and checks the complete action restriction flags. It consumes
  MP, not inventory, and does not grant another rank.
- [Action restrictions](../Native/Game/src/Battle/ActionResolution.c) distinguish
  unavailable resources (bit 2) from unlearned/Silence restrictions (bit 4).
  Original action records supply costs, ranges, areas and calculation dispatch.
  The checked costs below are byte 3 of the 52-byte SLUS records at file offset
  `0x3C1DC`, not amounts inferred from final MP after passive recovery.
- [Commit/presentation](../Native/Game/src/Battle/ActionFlow.c) preserves the
  cancellation boundary and sets the magic-used flag for a committed Ashley
  spell, including a miss. The existing item checkpoint separately verifies
  first/already-learned book use, rank learning and book consumption.
- Original intro, Minotaur, Teleport, ability-used and Break-Art-used flags,
  plus the 36-byte physical-art/ability progression record, remain unchanged
  across these learned casts. Magic-used changes `0 -> 1`, then stays 1.
  Rejected/cancelled attempts leave it 0. These fixtures do not manufacture
  Minotaur completion: its flag stays 0. The full real-story boundary audit
  remains [#17](https://github.com/JEschete/VS-NativePort/issues/17).

All runs below use real native-window controller keys at speed 1. No memory,
enemy HP, event flags or result records were written. Default STR/INT/AGI100
and HP/MP999 are used except the deliberately capped MP1 rejection fixture
and INT150 for the final ranked/offensive fixture (Debug Fireball uses INT100).
Positive offensive fixtures use zone12/room0, Ashley9,10 and Minotaur Zombie
definition1 at9,12. Recovery/support are solo. Range rejection moves that enemy
to9,16, outside Fireball's original horizontal extent of576 native units.

| Check | Debug run | Release run |
| --- | --- | --- |
| Fireball99, MP25 | `1792680710660736-0` | `1792681280349938-0` |
| Heal96, MP5 | `1792680761115780-0` | `1792681304556948-0` |
| Herakles60, MP12 | `1792680782301952-0` | `1792681325760550-0` |
| Flame Sphere114-117, MP36/44/52/60 | `1792681176530414-0` | `1792681345935960-0` |
| Unlearned class, original prologue profile | `1792680570406517-0` | `1792680626880351-0` |
| Insufficient MP | `1792680581486383-0` | `1792680637932388-0` |
| Silence | `1792680595605230-0` | `1792680652065183-0` |
| Cancel before commitment | `1792680611716486-0` | `1792680667217956-0` |
| Out-of-range/no target | `1792681789464823-0` | `1792681805596294-0` |

Measured effect boundaries:

- Fireball: Debug enemy681 ->677 after two explicitly recorded zero-effect
  casts; Release681 ->642. Every committed attempt costs25MP and returns
  normally. A miss is not cancellation or permission to refund the cost.
- Heal: Debug600 ->658; Release600 ->659, exactly matching the reported58/59
  restoration. The earlier599 setup sample precedes one passive HP recovery;
  it is not used as the spell's effect boundary.
- Herakles: actual STR100 ->110 in both builds.
- Flame Sphere: all four ranks cast in one encounter per build, using the real
  rank chooser and height/horizontal area controls. Debug enemy HP boundaries:
  681 ->680,680 ->679,679 ->672,673 ->633. Release:
  681 ->677,677 ->676,676 ->671,671 ->612. The gap before Debug rankIV is passive
  enemy recovery, not an edited value. All same-frame limb results are summed
  before comparing actual actor HP; one limb's message is not the whole cast.
- Unlearned classes do not open. Low MP and Silence retain the disabled spell
  list. No-target confirmation returns to the spell menu, matching original
  target-selection state16/17; it does not cast or remain indefinitely in
  targeting. All five negative/cancel cases preserve MP, inventory and learning.

The rank/elevation visuals accepted with #85/#86 remain on the same native
implementation. Fresh replays exercise presentation, result feedback and
control return; native audio logs record instrument/note playback. This is
functional feedback evidence, not a new audiovisual parity certification.
Particle-path fidelity remains [#76](https://github.com/JEschete/VS-NativePort/issues/76).

Local evidence is under `build\<Configuration>\combat-lab\runs\<run>` and
`build\quality\live-spell-<action>-<gate>-<Configuration>.json`. The local driver
is `build\quality\live-restart-check.ps1 -Configuration <Debug|Release>
-Spell <99|96|60|114>`;114 runs all four ranks. Fireball restriction variants
use `-SpellGate <unlearned|low-mp|silence|cancel|range>`. It uses ordinary
F1/F8 preconditions, observes memory read-only, records original misses and
retries input only after actual idle control. Short20ms key probes, assuming
one area preview/limb, and assuming all casts hit were rejected diagnostic
approaches, not native defects.

All18 final sessions end with zero error events and `trace_failed=false`.
Fresh Debug/Release Catch2 selection
`"[items],[profile],[status],[stats],[encounter]"` passes55 cases /2,098
assertions each. Native executable invocations use separate stdout/stderr
redirection and checked exit codes; expected negative-test warnings are not
test failures. Full native code remains identical to the previously validated
416-case build; no baseline or generated output changed.

**Coverage limits:** direct execution sampled7 of71 battle-spell action IDs.
The remaining direct-entry IDs are54-58,61-63,66-73,75-77,79-80,82-83,85,87-88,
90-95,97-98,100-113 and118-133. Their separately qualified grimoire observations
are not substitutes for individual learned-entry tests. Retain those entries
for [#16](https://github.com/JEschete/VS-NativePort/issues/16), along with broader
targets/ranges/effect permutations. Save persistence, Teleport, matching
keys/sigils and the full #17 progression audit are not closed by this checkpoint.

## Production contracts and current limits

- Rooms and enemy stats come from the extracted ZND/MPD definitions. Each accepted
	enemy also needs a hostile MPD placement profile in that zone.
  Its AI, hostility, drops and variant fields are preserved. Unavailable entries
  carry a reason; enumerating them is not a claim they work.
- Boss profiles retained for original story death events are now accepted. Only
  the lab's copied placement clears defeat retention, using normal production
  defeat/removal because story events are isolated. Original MPD data and normal
  story sessions are unchanged. Existing ordinary-profile choices remain stable.
  Guildenstern's orbiting form remains unavailable with a specific arena-support
  reason: its specialized orbit-state allocator is still unported.
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
  depend on the explicit testing profile above. The two item profiles and
  all-spells profile provide fixed grants; there is no arbitrary loadout or
  spell/rank unlock editor or complete ability coverage matrix yet.
  Items/spells/abilities that are available use the
  original menus and gameplay path; unavailable actions are not fabricated.
- Story and door events are explicitly isolated. Advanced profiles set the
  documented startup menu stage only; no room event is completed to obtain it.
  Ordinary room lock records and containers are retained independently of
  those event sections. Clef has unlocked the real MAP176 chest through normal
  inputs (zone37, room0, chest tile9,11; Ashley at9,10 facing toward it).
  Keys/sigils still require matching lock-record and real-use evidence; event
  isolation alone does not prove these interactions unavailable.
  This mode does not verify prologue combat or story encounters.
- Audio/effect/AI stubs still exist. Defeat/rewards, chains, Break Arts, all items,
  spells and ranks, equipment combinations, and original-game parity are unverified.
  An idle enemy, clean exit or passing unit suite is not acceptance of those features.

## Hostile boss-profile loading checkpoint

Source: `30f5369504d9438bc15b91f5a440383a4c6fba44` plus the scoped encounter
changes, built in `build/quality/boss-profiles/publish-check`. The isolated
snapshot excludes unrelated dirty audio, generator, dependency, documentation
and reference-tree changes. Evidence remains local under
`build/quality/boss-profiles/`; extracted assets and captures are not committed.

- The old filter confused MPD `actorStateByte` with script-only activation.
  Original `Field_SpawnRoomActors` / `func_80076F24` instead map it to
  `retainAfterDefeat`. Discovery, reload and preparation now agree on hostility;
  preparation clears retention only on the lab copy and logs both policies.
- `audit.py` and the executable's `--catalog-only` output cover 455 definitions:
  **315 previously eligible, 53 newly eligible, 368 total**. The remaining one
  of 54 retained-hostile candidates needs the orbit controller. All 315 previous
  source asset/row selections are preserved. Definitions without a same-zone
  hostile record remain unavailable; specific asset errors are no longer
  overwritten by the missing-profile diagnostic.
- Debug and Release x64 MSBuild solution builds pass. Direct `VS.Tests.exe`
  console/JUnit runs each pass **312 cases / 828575 assertions**; Test Explorer
  did not discover the Catch2 tests. Regressions exercise retained and ordinary
  hostile profiles, unchanged source bytes/AI/loot, and rejected nonhostile and
  invalid encodings. Exact commands/results are in `build-*.log`, `tests-*.log`
  and `tests-*.xml`.
- `quality-native.cmd --files` selecting `EncounterCatalog.cpp`,
  `EncounterSpawnProfile.hpp`, `Boot/Encounter.cpp` and `EncounterTests.cpp`,
  with `--tidy`, passes with zero new findings and no baseline changes. See
  `quality-expanded.log`; the report separates exact test-fixture literals.
- `replay.py Release` and `replay.py Debug` run 1500 ticks with `--start --speed 4
  --scripted`. D'tok (zone 6 definition 0), Dullahan (11/0) and Minotaur (12/0)
  initialize real hostile AI and execute Bite (3), Drain Heart (94) and Giga
  Rush (177), respectively. Presets and `*-result.json` retain exact arguments,
  positions, action results and executable-relative run paths.
- `lifecycle.py` sends actual F6/F10/editor navigation/Enter/F10 keyboard events
  to all three bosses in both configurations. Six sessions return and restart
  with identical freshly initialized actor slots, HP/MP and positions. See
  `lifecycle-results.json`; the earlier scripted-device attempt did not verify
  restart and is superseded by these live-key results.
- A retained hostile soldier (zone 3 definition 0) receives real player attacks
  and is removed at tick 1312 in both configurations. The 4000-tick replay presses
  Circle every 80 ticks from 140 through 3740, with no HP or event edits. This
  exercises retained-profile defeat cleanup, not every boss death sequence.
- D'tok also receives real player damage, but the bounded follow-up replays do
  not defeat it as it retreats out of attack range. No claim is made for complete
  boss defeat/loot coverage, all newly eligible actions, group dependencies,
  room traversal or original story-event parity. #16 remains open.

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

### Reviewing manually played item tests

Manual controller/keyboard play uses the same automatic logs; no scripted run,
custom capture tool or separate recording step is required. Leave the player's
window, inputs and encounter lifecycle alone while reading a run. Identify its
directory from the startup message and check `manifest.json` and `encounter-N.json`
for the build, dirty revision, profile, overrides and target setup.

For each use, correlate the inventory decrement and selected action with the
`combat.result` messages, adjacent actor snapshots, learned-action changes and
return to field control. Check permanent items against base attributes or
resource maxima, not just current values. Compare the frames immediately around
the result: passive MP recovery and Risk decay during menu navigation can
otherwise be mistaken for item effects. A requested recovery amount may exceed
the actual gain because the actor reaches its maximum.

Keep consumption, learning and positive target effects as separate observations.
A miss or zero-effect result does not establish damage, and an animation still
resolving at the end of a read is not a failed action. For a live file, read only
complete JSON lines and record the last inspected tick/sequence; later events
remain unreviewed. Four ranked uses must share an encounter, with all four
learned flags retained afterward. The training dummy suppresses magic effects
and is not a valid offensive-spell acceptance target.

Use the original enemy class rather than its displayed name to select fixtures:
Mandel (zone9, definition0) is Undead; Sackheim (zone9, definition6) is Human
and has MP. Drains reject Undead, while Exorcism requires Undead. Ghost
(zone13, definition0) is an eligible Banish target, not its only eligible class.
Area books require three confirmations: height, horizontal area position, then
the final target list. Check the actual target-preview point against the chosen
area; an adjacent root tile alone does not guarantee inclusion. After defeating
the final enemy, confirm the ordinary loot dialog before checking field return.

Ghost's secondary-body model now comes from the same live zone table as its
primary body (#118), preserving the original model index and companion offset.
Debug/Release real-input checks load models26/27, use Banish to defeat the Ghost
67 -> 0, consume one book, charge25MP, learn action105 and return through loot.
Separate three-encounter checks stop/re-enter with the Ghost alive in both builds.
These checks do not reproduce or close the separate F10 crash report #82.

### Item-use acceptance checkpoint (2026-10-22)

The bounded #8 item-use criteria now have production-path evidence, separate
from the wider learned-spell/fidelity work in #9 and save/load work in #18.
Consumption covers84/84 named consumables/books; all ten permanent-stat items
have actual base/max-stat comparisons, and all seven ranked grimoires have
four uses in one encounter. The owner supplied the faster broad manual run;
the additional runs below only fill specific effect/eligibility gaps.

- Fireball, Exorcism, Banish, both drains, Degenerate, Stun Cloud, Poison Mist
  and Curse have positive eligible-target results in Debug and Release.
  Four cure books remove their actual matching ailments. Clef unlocks the
  real MAP176 chest when Ashley faces it.
- **Fixate444/action79:** zone9 room5/MAP014, Ashley11,12, solo profileB.
  Read-only observation proves the original platform moves before use,
  remains stationary while the freeze timer decreases with both room and
  platform updates unpaused, then moves again after expiry. One book and3MP
  are spent; action79 is learned. Runs: Debug `1792676535074920-0`,
  Release `1792676584085327-0`.
- **Eureka447/action93:** zone9 room6/MAP015, Ashley8,7, solo profileB.
  The real trap at15,9 changes activation0 ->1 and marker fade0 ->128.
  One book and6MP are spent; action93 is learned. Runs: Debug
  `1792676277768454-0`, Release `1792676304258865-0`.
- **Uncapped recovery and mixed cures:** Debug `1792677272197860-0`,
  Release `1792677331620738-0`. Item329 restores100/99MP respectively;
  item335 restores100HP+100MP /99HP+101MP. Adjacent-frame gains equal the
  actual result amounts, below the999 maxima. Original `_itemRecovery`
  (`146C.c:12109`) uses `factor*5 + smoothed(11)-5`, so factor20 is95..105,
  not a fixed100. Item344 separately cures INT Down and AGI+Equipment Down,
  restoring affected attributes90 ->100 while preserving unrelated Poison.
  STR/INT/AGI modifiers share an original cancellation group; do not construct
  an impossible simultaneous three-debuff fixture.
- **First/already-learned use and progression:** Debug
  `1792678085128728-0`, Release `1792678132098137-0` use Fireball twice,
  consuming10 ->8 and25MP per cast. Only action99 becomes learned; the second
  use retains it. Read-only observations preserve intro stage3, locked
  Teleport0 and unrelated ability/Break Art counters; the real magic-used
  flag changes0 ->1. Debug's second zero-effect result still consumes at the
  original commit boundary and returns normally; Release deals damage.
  Earlier run `1792654884015949-0` verifies cancellation without consumption,
  first/repeated unranked use, non-stacking STR and an unusable Bronze Key.
  Prologue/profile tests preserve normal startup gates. Item use does not
  grant the separate story/save-point Teleport feature.

All eight final environmental/recovery/progression traces finalize without
error events (`trace_failed=false`). The same native code passes both full
suites:416 cases /1,060,702 assertions each; copier tests23/23; pushed Source
Quality run39787313511 reports195 existing findings and zero new. No baseline
or production behavior was changed to pass these observations. Local command:
`build\quality\live-restart-check.ps1 -Configuration <Debug|Release>` with
`-Book 444`, `-Book 447`, `-Recovery`, or `-Book 388 -GateChecks`.
Positive matching-key/sigil door use remains an additional owner-requested
follow-up; these checks do not establish that, #82's root cause, all learned
spell fidelity, native save persistence or compatibility retirement.

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
in commit `155a7438`; exact-revision Source Quality CI `39599618892` passed and
#77 is closed. It does not establish all flight-enemy coverage.

**AI, initial drop and defeat-flow acceptance (#6):** Current Debug and Release
Combat Lab runs `1792588127503092-0` and `1792588245223129-0` replay the original
Zone 9 Zombie definition with 32 ordinary Circle inputs at 40-tick intervals.
The Zombie moves toward Ashley, targets actor 0 and executes attacks that lower
Ashley HP from 250 to 230. Its HP falls from 121 to 49 by tick 1100 and 0 by
1150; actor 2 is removed by 1350, when its real source drop (item 323, count 2)
is queued. Ashley survives. The original New Game reward gate remains false
and inventory unchanged: this verifies the initial drop queue, not inventory
delivery or complete drop-table coverage. Exact inputs and JSONL traces are in
the Combat Lab run folders.

Fresh full New Game prologue replays use 839 scheduled controls and only Start
at tick 1000 to skip the intro. Debug/Release each complete 48000 ticks and all
22 captures with exit 0; logs load MAP004 then MAP005 (Debug 20425/23120;
Release 22561/25240), and final captures show the live battle. Neither route
edits HP, position, RNG, event or progression state.

Fresh player-defeat/fade replays each complete 26000 ticks and eight captures
with exit 0. After natural defeat, the real Game Over menu appears; Down at
21000 and Start at 21100 choose Title, and the title-session transition is
logged at 21177. Exact inputs, manifests, logs and captures for both replay
groups are under `build/quality/issue6-final-{victory,gameover}-<config>/`.

F02771's original raw host-pointer truncation failed the focused regression:
`LegacyGpu_Pointer` returned null instead of resolving the fade packet. The
copy-generator fix stores `LegacyGpu_Handle(prim)`, and the test now verifies
the ordering-table head resolves to that packet. Debug and Release both pass
**311 cases / 828561 assertions**; `quality-native.cmd --changed --tidy`
reports zero new findings (6 baseline), and the second copier pass is stable.
Fix commit `ae2e30e1` passed exact Source Quality CI `39603967134`; docs
revision `b7f97ce0` passed `39604755566`. All explicit #6 acceptance is verified
and the issue is closed. Complete drop-table coverage remains at priorities 09
and 10; broader roster coverage remains separately tracked.

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
