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

Native save-menu integration is tracked separately in #18. The Lab uses
`combat-lab-saves` inside SDL's per-user VagrantStory directory, separate from
Desktop's `saves` directory. Pass `--save-dir <directory>` for an explicitly
isolated replay root or intentional cross-host testing. Native saves are not
PS1 card images; import/migration and full gameplay round-trip acceptance
remain open. Do not point automated tests at owner files.

Save, Load and Continue use three original-style file cards with room artwork,
names and snapshot summaries. Up/Down scrolls all100 native files; Circle
opens confirmation and Cross returns. Confirmations are vertical Yes/No,
defaulting to No: use Up/Down and Circle, not Left/Right. The footer shows the
original controller symbols. Recovered files are explicitly marked and require
separate affirmative consent to replace an invalid primary. Normal Save still
requires an enabled original save point; the Lab is not a save-eligibility bypass.

### External event components

An isolated, empty-enemy preset may specify `"external_event": 80` to request
that original external EVT through the production direct-event and cutscene
APIs after the room is ready. The editor labels this **COMPONENT PLAYBACK** and
locks override groups: the event owns its scripted actors. The normal window
and controller input remain interactive; sequence presets support the same
field for bounded reset/reentry checks.

Only initial room entry is isolated. Once the external event starts, its normal
room-event and actor handoffs use production policies, including chained scenes
and returns to the initial room. An earlier event may be required to establish
the original audio or actor context; a file number alone does not certify those
preconditions.

This mode does not assign story flags, simulate a boss defeat or claim legitimate
story progression. The room/profile/event must be source-aligned by the caller.
It cannot be combined with production MPD entry events or explicit enemy groups.
The exact original sequential disc location, raw format and fixed event size
are checked, and the file is read before startup; unavailable files are errors.
Existing presets without this field retain their previous behavior.

For the post-Minotaur component, event80 in zone12/room0 (Ashley7,2) establishes
SEP26/WAVE99 before the original room scripts hand off to event82 in zone19/room0
and event81 back in zone12/room0. Starting82 alone omits those audio prerequisites.
This is component coverage, not evidence of defeating the boss. Release replay
1792893708848660-0 uses only real Circle presses at original dialogue waits:
event82 releases parent2 while prop7 still borrows bone2;72 dependent draws are
deferred, then the chain returns normal control at14468 and remains settled
through24000. No forced skip or engine error occurs. Speed8 audio queue warnings
are not a listening/fidelity test. Local evidence:
`build/quality/family174-through81-Release.*`.

Repeated full-chain controls: Debug1792894544327667-0 and
Release1792895226944470-0 each complete two fresh80/82/81 chains in one window,
then ordinary isolated and production presets. Each original return remains at
normal control for over11000 ticks before teardown; each82 has exactly72
dependent draws deferred after parent2 disappears. Bank99 is mapped and used
on both entries, including cached content on the second. All four scripts
complete normal teardown without engine errors, Start or forced skip.
Local assertion reports are `build/quality/scene174-verified-<run-id>.json`;
acknowledged dialogue inputs are retained in `family179-fixed-reentry-*`.
Interrupted-box4 controls for #179: Debug1792895435337715-0 and
Release1792895991286486-0 deliberately withhold further Circle input once81 box4
is waiting, retain it through15011, then request normal budget teardown. Fresh
80/82/81 playback in the same window completes and retains normal control through
39023, followed by ordinary isolated/production cases. The strict reports verify
that the first scene really is interrupted with box4 active, not already complete,
and that the later chain has no crash, engine error, forced skip or adjustment.
Session UI borrows are invalidated before heap replacement; original in-scene
close behavior is unchanged. Evidence is in `family179-interrupted-reentry-*`
and `scene174-verified-<run-id>.json` under `build/quality`.

### Encounter setup

The menu uses the extracted SYSTEM.DAT font. Select zone, room, enemy, quantity,
Ashley tile/elevation, group offsets, spacing and RNG seed. Add multiple groups
for mixed enemy definitions; clear groups to return to the current selection.
Set **QUANTITY: 0 (SOLO)** and clear any explicit groups to enter with Ashley
alone. An empty-enemy preset stays solo when loaded in the default isolated
mode. Explicit production-room presets instead retain the original room actors
and events; their editor labels say **ORIGINAL ROOM**, not **SOLO**.
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
  Use normal speed (`--speed 1`) for listening/fidelity acceptance. Accelerated
  native audio uses the bounded speaker-output policy documented in the
  [#83 checkpoint](#accelerated-native-audio-timing-83), not full listening parity.
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

### Original box-fallback control (#175)

Original `71618.c:1093` tries ordinary ranked actions first. Only after those
fail does it try its cached fallback against registered kind2/4 special actors,
retaining nearby-hostile, range, cooldown, readiness and visibility conditions.
Boxes do not compete randomly as equal-priority ordinary targets.

The following local preset preserves original actors and crates. An isolated
placement override would remove those props and is not an equivalent control.
Save this JSON locally, for example as `build\quality\box-cover.json`:

```json
{
  "version": 1,
  "zone": 9,
  "room": 11,
  "seed": 1,
  "ashley_profile": "post-prologue",
  "player": {"x": 1, "z": 2, "elevation": 0, "direction": 0},
  "enemies": [],
  "room_events": "production"
}
```

For the verified source6db8d24d, this actual-controller replay climbs the original
stack without changing live actor state:

```powershell
$press = @('120:Right+Square')
for ($tick = 123; $tick -le 228; $tick += 3) {
    $press += ('{0}:Right' -f $tick)
}
& .\build\Debug\VS.CombatLab.exe --data Native\data `
    --scenario build\quality\box-cover.json --start --frames 1200 `
    --speed 1 --scripted --press ($press -join ',')
```

For interactive play, wait for normal field control, then jump right and keep
moving toward the stack. Inputs during loading/action resolution are not a jump.
Debug1792907616391601-0 and Release1792907657305767-0 establish Ashley on the
stack at[318,-384,320]. His signed-aim distance209979 exceeds the original
wolf's squared reach50176; original box6 is19646 within reach, while box7 is67262
outside it. Wolf3's real single-target action1 damages box6 HP1->0 at496.
The AI navigation target0 is not the cached/executed fallback target6.

Extended Debug1792907737516702-0 / Release1792907776647105-0 continue through1200:
destroyed6 deregisters, the upper box drops128 units, Ashley settles on it and
the wolf climbs up through actual command10 movement at612..625 (not a claimed
jump arc). Ground controls1792907849223984-0 /
1792907882485500-0 omit the inputs: ordinary Ashley attacks succeed and both
boxes remain intact through900. All six runs have zero engine errors and normal
host shutdown with explicitly interrupted encounters, not victory/complete
encounter claims. Local strict proof is `build\quality\box175-verified.json`.
This is source-permitted occasional fallback acceptance, not a gameplay change
or proof for every enemy/room/action combination.

### Multi-body-part verification (#80, 2026-10-25)

Current runtime `b2a29e6b` passes a controller-only player case as well as the
previously measured enemy breath147 and Lich130/126 cases. No target-expansion,
damage or effect-selection change was needed for this verification.

The isolated fixture is zone12/room0, seed1, original Minotaur Zombie
definition1 at9,12 and Ashley9,10 with `all-spells`, HP/MP999 and unchanged
default attributes/equipment. Normal-speed Debug1792968266440918-0 and
Release1792968266549016-0 select Magic/Warlock/ExplosionIV (action109), adjust
the original height/horizontal sphere controls to1216,-299,1585, and select
exactly enemy2's six limbs0-5, not six different actors. All six receive result
records at2251: four original blocked/missed results and two successful damage
values4/3. Enemy HP681->674 exactly equals their sum7. Ashley is excluded;
MP999->939 is charged once at1903 and remains939 through effect completion.
Both builds restore ordinary control at2431, finish2700 ticks and report0
engine errors. Four zero-damage results are not four omitted limbs.

The exact schedule opens Triangle at140, Circle250/400, nine Down presses at
550..950 in50-tick steps, Circle1100, Right1250/1300/1350, Circle1450, holds
Up1500-1539, Circle1600, holds Up1650-1671, then Circle1750/1900.
Use `--scenario <fixture> --start --scripted --frames 2700 --speed 1`;
the held ranges are represented by consecutive two-tick scheduled presses.
Bounded `--trace-effect-paths 2100:2400` records every rendered frame.
Earlier menu scouts during entry fade or enemy combat are not acceptance.

Read-only Debug1792968898071584-0 independently checks the actual effect
request against compiled PDB member offsets and its232-byte size: E056,
source0, count1, target actor2/whole actor, no others. Multiple limb results
deliberately deduplicate to one actor-level visual request, matching original
`func_8007138C`; they do not require six effect children. Release cancellation
1792968751357217-0 uses Cross before commitment, returns to the Warlock list,
retains999MP and produces no action109 result.

`VS.Tests.exe "[combat][sandbox]"` covers24 result-list combinations:
0/1/2/6 matching limbs, queue indices0/5/7, with/without another actor and a
world-point entry. Every matching HP/MP/limb contribution is retained once,
and nonmatches/empty results preserve resources. Both full suites pass768
cases/3,390,555 assertions; targeted tidy has0 new findings. Local reports
`multilimb80-verified.json`, `multilimb80-request-mapping-Debug.json` and
`multilimb80-*` commands/logs remain in session evidence. The runtime was not
changed; the dirty test/docs and unrelated owner edits are not recast as a
new gameplay fix. This is bounded current conformance, not every-action or
original-game visual parity certification. A new failing action/enemy needs
its own concrete reproduction.

### Actor sound-override native leaf (#42, 2026-10-25)

F00995/F01007 are two catalog occurrences of original `func_800AEEC4`,
not two new audible defects. Production actor/companion rendering already
contained equivalent inline arithmetic. Both copies now call the registered
owned `Audio_UpdateActorSoundTransition` with the correct `void(actor*)` ABI;
the old untyped `int()` stub is removed. Their surrounding frozen-render
conditions and the original override producer/reset paths are unchanged.

The source-backed leaf lives with its Game-owned actor state in
[`SoundOverrides.cpp`](../Native/Game/src/Actors/SoundOverrides.cpp).
Original44F14.c:886 and MIPS800AEEC4-800AEF90 specify unsigned-byte pan/volume,
signed division toward zero, `min(tickSpeed/2, remaining)` and consuming the
timer even when both values already match. No source offsets, raw addresses,
hardware output or new effect/state format is introduced.

The original API fails the new compile-time signature and call controls.
Fixed `[sound-transition]` controls verify modes0/1/2, zero timer/speed,
odd speed, increasing/decreasing values and fractional rounding, large-step
clamping, final endpoints, replacement targets and fresh reset. A full255-tick
control checks every intermediate unsigned timer/pan/volume value, including
both byte boundaries. Target state and an unrelated actor field stay intact.
All three focused cases pass292 assertions in each configuration. Full native
suites pass771 cases/3,390,847 assertions each;87 port and29 quality-tool
contracts pass, with0 new focused tidy or maintained default findings.
This is component/caller-contract verification, not a new listening or full
actor-sound/reference-parity certification.

Generator ownership, stable repeated output, preserved owner edits and the
shared scaffold/analyzer C-header rule are checked separately. The original
186-entry inventory advances only these two IDs to114 complete/72 remaining.
The source remains a typed C ABI over the compatibility actor record; whole
audio/actor-state C++ promotion and compatibility retirement are not claimed.
Local `actor-sound-transition-*` commands, ABI-red output and proof reports
remain in session evidence; publication and exact CI are recorded on#42.

### Relocation request constructors (#42, 2026-10-25)

F04184/F04185/F04186 now use the existing owned MotionRequests API instead of
three no-op stubs. Original4A0A8.c:490-549 and its consumers establish relocation
requests, despite a misleading turn description in the alternate reference.
The original command21/22/23 callers and teleport/travel update/reset paths
are unchanged.

Teleport accepts only reaction0, clears turn frames and the complete original
reaction/pose/drawn byte, stores the four-byte packed tile plus zero/facing
tail, then propagates to companions>=2. Setting the reaction before recursion
preserves the original self/cyclic-link termination. A busy parent does not
start its companion; special slots0/1 are not traversed. Tile/timed travel
capture all eight route bytes before replacing facing and setting pose1/4,
preserving reaction/drawn bits and existing turn frames. Signed16-bit
boundaries, packed high bytes and aliased target storage are retained.

The initial four red cases fail17/69 assertions against the old stubs. Final
controls additionally cover companion-slot boundaries, busy-parent isolation
and both route aliases; full Debug/Release777 cases/3,390,932 assertions and88
port contracts pass, with0 new focused compile findings. Static controls check
the original adjacent command words and eight-byte vector contract. Two real
copier runs preserve300 protected files and repeat identically; only generated
Movement.c/.h changes. No project/source-file additions or owner-solution
rewrites are needed.

The original tracker advances only these three IDs to117/186,69 remaining.
This is source-backed constructor/component verification, not acceptance of
every teleport/travel animation, AI action or scene. No whole actor-motion C++
promotion or compatibility retirement is claimed. Exact private commands and
reports use `relocation42-*`; publication and CI are recorded on#42.

### Interactive combat-settled room entry (#151, 2026-10-25)

Release1792978233864775-0 on5671e3b5 covers the previously combat-interrupted
9/7->9/6 route through ordinary interactive production-room mode. Source9/7
uses Ashley3,1, seed1, `post-prologue`, original equipment/stats/room actors/
events and no overrides. Actual Cross input opens the authored door;9/6
arrives at230. Circle draws the weapon, opens the sphere and confirms ordinary
targets. Actor2 starts73HP and takes46/42 (73->27->0, clamped normally) at
962/1272; actor3 starts42HP and takes54 to0 at1652. These are actual source0
attack/result/death records, not edited health or animation-only inference.

The original readiness/event/quiet predicate remains unchanged: available
scene and event phase0, ready destination9/6, control1, transition0 for120 ticks.
The measured interval is1840..10000; acceptance is first reached1960.
There are0 engine errors, no forced skip and no live memory/story/readiness/
result writes. Ashley survives and later recovers HP passively.

The direct-request fixture's75/39 initial enemy HP is not substituted for this
ordinary approach's actual RNG-dependent73/42 values. Inactive event PC337 is
still populated at phase0. The first private controller's shutdown proxy
incorrectly required every PC null, so it ran to10000; validation uses the exact
existing `UpdateDoorTrial`/`DoorTrial::Observe` predicate and the recorded frames,
not that proxy or a weakened game requirement.

Deterministic `--door-transitions` intentionally disables live devices and
rejects extra presses; a physical-key probe in that mode delivered no game input
and is excluded. The successful run instead uses `--scenario <production-preset>
--start --frames 10000 --speed 1` with real owned-window keyboard actions.
The original automated row stays combat-blocked; this separate interactive
Release result is not a blanket1001-door pass or a mixed-revision aggregation.
Local `door151-room6-production-controller.json` retains all acknowledged
actions/commands; `door151-room6-production-verified.json` contains strict
HP, death, route and quiet-interval checks.

### Shared original-width room-light triangle kernel (#190/#42, 2026-10-26)

F00768 (`func_80098C3C`, original2EA3C.s:1857) now exports the typed
kernel used by the existing owned Render_SampleRoomLighting path.
The imported triangle remains32 scalar bytes, with bounds separate from
its three coordinate/color pairs. This removes the obsolete generated
stub without introducing a second interpolation implementation.

The original tests positive signed32 NCLIP MAC0 area and nonnegative
wrapped sub-areas. Channel products/additions wrap unsigned32 before
unsigned division and the original signed-result/byte clamps. Native
math now preserves those widths without depending on hidden GTE register
state. Geometric rejection leaves the direct output untouched; success
writes only RGB. The room sampler retains its bounds, two-layer lookup,
ambient fallback and complete fallback-word behavior.

The prior64-bit kernel was observably different: a triangle spanning
-32768..32767 has mathematical area4294836225 but original signed32
MAC0 -131071. Original code rejects it; the old sampler accepted it,
returning vertex55/66/77/code5A instead of ambient11/22/33/code0.
The new native red fixture fails all4 assertions before the fix.
Another golden case checks positive area1024000000: constant255 colors
produce3 after original unsigned32 numerator wrap, not widened255.
This is confirmed arithmetic conformance, not a demonstrated cause of
an owner-visible lighting report.

Verification is parent7a4ca39e plus the scoped worktree and preserved owner
edits; exact commands/results are private `roomlight190-*`:

- `build\<configuration>\VS.Tests.exe "[room-lighting],[lighting]"
  --reporter compact` passes7 cases/12,380 assertions in Debug/Release.
  The new direct tests contribute6 cases/12,379, with the existing sampler
  case retaining both layers, interpolation and fallback behavior.
- Corners and inclusive edges have independent golden RGB values.
  Negative winding, degeneracy, outside points and the signed-area
  overflow preserve rejected output. Guard words, code bytes and every
  input-triangle byte stay unchanged where required.
- 4096 deterministic signed16 tuples compare against independently
  expanded determinants and explicit wide-sum-to32-bit narrowing.
  Both accepted/rejected populations exceed128; this local generator
  does not alter any gameplay RNG.
- The actual BATTLE80098C3C resolver selects the typed shared kernel,
  invokes it and rejects its interior address.
- Both x64 host builds link; full Debug/Release suites pass819 cases/
  4,091,150 assertions each. All105 port controls and0-new focused/default
  quality pass, without changing baselines.
- Two native generations preserve296 maintained/owner paths and both
  original instruction/C sources, and repeat identically. Only generated
  RoomGeometry.c/.h change. Tests projects render selectively and repeat,
  leaving the owner solution untouched.

The original cohort reaches135/186,51 remaining (16 C/C++ references,
25 assembly-only,10 missing listings), preserving all IDs/order and
prior134 checks. No all-room visual parity, complete renderer promotion
or compatibility retirement is inferred. Publication and exact CI are
recorded on#190/#42.

### Native script weapon-mode transition (#42/#148, 2026-10-26)

F01130 (`func_800B9DB4`, original4A0A8.c:2158 and the matching
800B9DB4-800B9F24 instruction listing) now belongs to Event/WeaponMode.
The read-only rood-boi e59320ca C counterpart agrees with the instruction
branches. The public decomp scratch returned403; implementation evidence
comes from these local original instructions and C, not an assumed
animation or visibility shortcut.

The command retains the original state machine:

- With no pending transition, story flag0xAC gates every actor change.
  The operand indexes the existing native weapon-state table and calls
  the production battle-mode/HUD helper. Bounds use that table's physical
  extent, not an invented two-value logical domain.
- No animation, or skipState2, applies exactly the original low16-bit
  visibility mask: set/clear0x1800, preserve other low flags, then snap
  both weapon visibility blends through the existing APIs.
- Animated playback requests `(variant-1)*2 + (mode^1) +109` with10-frame
  blending and the original draw/sheath sound helper, then yields while
  the skeletal pose lacks public state bit0x80.
- Normal completion requests idle101/6 and clears the pending byte without
  forcing skip-only visibility or restarting an already-held identical
  idle. Skipping a live transition clears the wait and snaps visibility
  without replaying its animation or rechecking the entry-only story gate.

Original D800F4C4E/G00473 is now named weaponTransitionPending. Its address,
shared-byte behavior and existing event-initialization reset are preserved;
the proposal output and low-confidence trace queue agree with the proven
reader. Missing command/model/battle context, an out-of-bounds table index
or an explicit animation failure logs and terminates. Returning a negative
error to this VM would instead be interpreted as a branch address.

Verification is from parent4c4e464d plus the scoped worktree and preserved
owner edits; exact commands and local evidence are in `weaponmode42-*`:

- Original stub red:5 cases/72 assertions,51 failures. A dependent VM test
  precondition was corrected before this bounded red result was captured.
- Debug/Release `build\<configuration>\VS.Tests.exe "[weapon-mode]"
  --reporter compact`:7 cases/471 assertions each. Synthetic common/battle
  sequence banks run through the real animation selection and skeletal
  advancement code, never an animation-lock or forced-completion flag.
- Exact timing covers synthetic length4 poses holding after5 advances.
  Outside active event playback, variant2 retains the battle helper's
  queued idle: after5 advances the idle starts at0 and the opcode must
  still yield; it holds after10. Drawn idle101 already held at4 is not
  restarted by the final identical animation request. These source-backed
  distinctions corrected two overly narrow fixture assumptions, not
  production timing or acceptance thresholds.
- Actual normal/skip VM dispatch holds the original opcode cursor, then
  advances to the production stop-context handler. The normal handler is
  resolved through the real BATTLE800B9DB4 symbol registry; its interior
  address is rejected. Tests also cover the entry gate, skip during a live
  wait, both weapons' final blends, both playback phases and the last
  physical state-table slot without claiming that slot is authored content.
- Both x64 solution configurations link both hosts; full native suites
  pass813 cases/4,078,771 assertions each. All104 port and32 naming
  controls pass, with0-new focused/default quality and unchanged baselines.
- Two native generations preserve owned/reference/owner inputs and repeat
  identically, changing only ModulePointers.c, ScriptActors.c/.h and
  ScriptVm.c/.h. Game/Tests projects render selectively and repeat without
  rewriting the owner solution. Native C-header interop is registered for
  the new translation unit.

Full trace-queue regeneration exposed unrelated pre-existing drift:
17 removals including G00473,7 additions and4 namespace-label changes.
Its candidate output is retained privately and the discovery is on#148.
This batch removes only the now-resolved G00473 entry, preserving every
other queue record rather than folding unrelated semantic work into it.

The original cohort reaches134/186,52 remaining (17 C/C++ references,
25 assembly-only,10 missing listings). Preserve original IDs/order and
prior133 checks. This is component/VM verification, not every authored
cutscene, audible output parity, whole-state C++ promotion or compatibility
retirement. Publication and exact-revision CI are on#42/#148.

### Typed orbit-state construction and teardown (#42, 2026-10-25)

F01845 (`func_800E7660`, original7E05C.s:1080-1095 and read-only
rood-boi battle_movement.c:1914 at e59320ca) now uses the existing owned
ActorCleanup lifecycle. It allocates the native scalar motion record from
the low arena, value-constructs the complete record and sets its cooldown
to900. The settings argument remains unused, including a null argument.
Allocation failure logs and terminates rather than publishing a usable-looking
null record. The existing special-category AC/AD actor teardown releases
this allocation and clears its shared binding.

This is a fresh-state constructor, not a replacement for teardown or a
promise that two live orbit owners are supported. The original shared
ownership model is retained; neither an exact boss identity nor a complete
boss fight is inferred from the source.

Evidence is from parent20cd187f plus this scoped dirty worktree, preserving
the owner edits. Exact commands and private results are in `orbit42-*`:

- Debug red `build\Debug\VS.Tests.exe "[orbit-state]" --reporter compact`
  fails both cases,4 of8 assertions, against the original constructor stub.
- The constructor is tested after poisoning and freeing the exact reused
  allocation. Every one of its32 bytes must match original zero-fill
  plus the little-endian cooldown at0x0C; the unused caller input remains
  byte-identical and the real low-heap capacity is recovered after release.
- The existing special-category teardown test now calls the real
  constructor, tests AC and AD, reconstructs after full release and checks
  that the shared pointer clears and all heap units return on each cycle.
- Debug/Release x64 solution builds link both hosts.
  `build\<configuration>\VS.Tests.exe "[orbit-state],[cleanup]"
  --reporter compact` passes13 cases/198 assertions in each configuration;
  the full native suites pass806 cases/4,078,300 assertions each.
- All100 port/audit controls pass. Focused native/tidy and default quality
  have0 actionable/0 new findings, without changing baselines.
- Two copier runs preserve292 tracked maintained-native/owner paths plus
  both original instruction/C reference files, and repeat identically.
  Only generated OrbitingBoss.c/.h and the existing ActorState.c consumer
  include change. No solution/project regeneration is needed.

The original186-entry cohort reaches133 implemented entries,53 remaining:
18 C/C++ reference bodies,25 assembly-only and10 missing instruction
listings. Existing order/prior132 checks are retained. Component ownership
verification, whole-state C++ promotion and compatibility retirement stay
separate; publication and exact CI are recorded on#42.

### Exact native hostility transition (#42, 2026-10-25)

F01695 (`func_800DC784`, original73C84.s:246-284 and read-only
rood-boi battle_helpers.c at e59320ca) now belongs to the existing owned
ActorCleanup reaction lifecycle. It sets the requested target's
classification to3 and clears only the seven-bit counter at bits8-14,
preserving every other bit and relation. This reset also applies to an
already-hostile target.

Only a newly hostile target consumes the original BIOS `rand()` draw;
the following masked comparison always succeeds. The same branch sets
byte0x18 to1, uses the existing temporary indicator API with6/40, and
requests reaction2 with range128 through the existing delayed-reaction
API. That second API advances the distinct signed-halfword actor RNG
accumulator even when a live navigation override or decision delay
prevents changing the actor command. An already-hostile target must not
draw either RNG, restart the indicator, or request another reaction.
The existing transient AI views are retained, not declared retired.

The source revision is parent fe094c32 with the scoped hostility changes
and the preserved owner edits listed by the worktree. Commands/evidence
are retained privately as `hostility42-*`:

- Debug red: `build\Debug\VS.Tests.exe "[hostility]" --reporter compact`
  fails both cases,36 of68 assertions, against the original stub.
- Debug green: `build\Debug\VS.Tests.exe
  "[hostility],[lifecycle-leaves]" --reporter compact` passes7 cases/
  115,047 assertions; the hostility subset adds2 cases/16,436 assertions.
  Tests cover all128 counter values, four classifications, targets0/1/7/15,
  complementary unaffected bits, whole-state and other-relation canaries,
  independent BIOS/actor RNG state, all four busy-guard combinations and
  repeat calls after changing the indicator and frame tick.
- MSBuild Debug/Release x64 solution builds link both hosts;
  `build\<configuration>\VS.Tests.exe --reporter compact` passes805
  cases/4,078,278 assertions in each configuration.
- `python -m unittest discover -s tools\port -p test_*.py` passes99
  controls. The published assembly audit has54 stubs:19 C/C++ reference
  bodies,25 assembly-only and10 missing instruction listings.
- Focused native/tidy and default quality both have0 actionable/0 new
  findings with unchanged baselines. Catch macro expansion initially
  exceeded the busy-case function-size limit; extracting its exact checks
  preserves all16,436 assertions. The final focused analysis selects
  ActorCleanup.cpp, AiLifecycleLeafTests.cpp and test_encounter_fixups.py.
  The unchanged public ABI is covered by both full builds and default
  header checks, without repeating all-owned-TU analysis for a relocated
  declaration.
- Two copier runs start without generated-file conflicts, preserve111
  registered-owned/owner/test paths and repeat identically. Only generated
  Perception.c/.h change, removing the old stub/declaration in favor of the
  owned provider; existing production action-reaction and relationship
  propagation callers retain the same typed public ABI.

The original cohort advances132/186,54 remaining, with prior131 entries
and the original order preserved. This establishes the component contract,
not all NPC relationship gameplay, whole-state promotion or compatibility
retirement. Publication and exact-revision CI are recorded on#42.

### Exact dynamic navigation tile marker (#42, 2026-10-25)

F01828 (`func_800E68EC`, original7E05C.s:72 and read-only
battle_movement.c) now belongs to NavigationPrimitives. It guards on
tileFlagMap availability, reuses the existing32x32 tile validation and
sets exactly u16 bit0x400 with OR. Existing Field trap/interaction callers
already supply this typed native API. No other flag is cleared or changed,
and repeat calls are idempotent; this is not an occupancy counter.

Red controls have3 failed cases/1031 failed assertions out of1032.
Final3 cases/529,408 assertions cover every65536 source word at all
four grid corners, repeat marking, untouched-cell canaries, all1024
coordinates, missing-grid calls and live grid rebinding. The exact
obstacle-category interpretation remains inferred; these tests establish
the source-backed bit contract, not a new trap/AI gameplay fix.

Full Debug/Release suites pass803 cases/4,061,842 assertions each;98
copier/audit controls and focused/default quality pass with0 new findings.
Both hosts link. Two copier runs preserve306 protected paths and repeat
identically, changing only Navigation.c/.h and the existing interaction
consumer include. Occupant add/remove and workspace-building stubs
remain separate to avoid enabling an incomplete counting lifecycle.
Private `tilemarker42-*` retains commands, red/green and ownership evidence.
The original cohort advances131/186,55 remaining, with original order/
prior130 checks preserved. No whole-state promotion or retirement is
claimed; publication/exact CI are on#42.

### Shared native room screen rectangle (#187/#42, 2026-10-25)

F00756 (`func_800981B8`, original2EA3C.s:1101) now updates one signed16
screen rectangle used by both ordinary and subdivided room polygon
emitters. Source func80098014 compares half-open bounds with original
default0/0/320/224. Actual red fixtures project all Y into224..255:
ordinary triangle/quad paths wrongly emit40/52 bytes under their
hardcoded240 bottom while the subdivided paths reject. That test has
2 failed assertions out of148, not a guessed screenshot artifact.

The ordinary-only default240 is intentionally corrected to the original
224-row room clip. Physical framebuffer/sky/camera offsets and the near/
far/LOD and memory-bound rules are unchanged. Callers may explicitly set
a240-bottom rectangle; tests exercise that supported behavior too. The
native setter wraps all four parameters exactly as original halfword
instruction immediates, without modifying code or inventing fallback
bounds. Both emitters call the same projected-bounds predicate.

80 additional production packet checks cover all four edges and
equalities, tight full windows, wholly hidden/overlapping windows,
signed16 truncation (including negative halves), explicit alternate
rectangles, default224 rejection and memory canaries across ordinary/
subdivided triangles and quads. The full room selection passes6 cases/
248 assertions in each configuration. These are emitted outputs, not
only stored-state checks.

Full Debug/Release suites pass800 cases/3,532,434 assertions each;97
copier/audit controls pass. Default quality is0 new; focused analysis
retains8 existing LegacyGpuTests findings and adds0, with no baseline
change. Both hosts link. Two copier runs preserve306 protected paths
and repeat identically, changing only generated RoomGeometry.c/.h.
Private `clip187-*` retains original source, actual red/green and
ownership evidence.

This advances130/186 original entries,56 remaining. No owner-visible
popping cause, every-scene audiovisual parity, whole renderer C++
promotion or compatibility retirement is claimed. Publication and
exact CI are recorded on#187/#42.

### Native room far-depth setting (#42, 2026-10-25)

F00754 (`func_8009817C`, original2EA3C.s:1078) now updates the existing
owned RoomDraw state through a void(int) API. Original halfword instruction
patching means the cutoff is signed16; the native wrapping helper preserves
that rule without modifying code. The exclusive comparison occurs after
the material depth bias in ordinary and subdivided polygon emission.
The independent physical ordering-buffer limit remains enforced even when
the configured cutoff is larger. Default2047 and all existing near/LOD/
camera-scale/screen behavior are unchanged; F00756 is separate work.

Red ABI compilation rejects the old int() stub. Actual emitted packet
counts receive114 additional checks across ordinary/subdivided triangles
and quads: below/equal/above boundaries, signed biases-1/0/1, signed16
truncation (including0x10100/0x10101/0x8000), physical-buffer rejection
under a permissive cutoff and unchanged memory canaries. These are packet
outputs, not just getter-state tests. The complete room selection passes
6 cases/168 assertions in each configuration.

Full Debug/Release suites pass800 cases/3,532,354 assertions each;96
copier/audit controls pass. Default quality remains0 new; focused compile
analysis retains8 existing LegacyGpuTests findings and adds0, without
baseline changes. Both hosts link. Two copier runs preserve306 protected
paths and repeat identically, changing only generated RoomGeometry.c/.h;
owned RoomDraw code is not overwritten.

This advances129/186 original entries,57 remaining, without a broad
visual-fidelity, whole renderer C++ promotion or compatibility-retirement
claim. Private `farclip42-*` retains source, exact commands, red/green
and ownership evidence; publication/exact CI are recorded on#42.

### Shared native signed-key pointer sort (#42, 2026-10-25)

F01829 (`func_800E6974`, original7E05C.s:114) is now an owned Core
implementation with one kernel shared by the existing native AI ranking
and weighted-promotion sort. Prior Game code already implemented the exact
midpoint partition while the exported Core entry remained an int() stub.
The refactor removes that duplication rather than replacing the original
tie ordering with std::sort/stable_sort.

The typed native API sorts an inclusive pointer-array range within the
original byte-index limits. Keys are signed32 first words read safely with
memcpy; pointer strides and ownership are native, not PS1 words. It
permutes only pointers. Empty/single ranges need no record read; invalid
nonempty contracts are explicit errors, not success-shaped defaults.
The AI adapter uses a bounded16-entry void-pointer staging array and
compile-time key-layout/type checks, keeping Engine independent of Game
without type-punning a typed pointer array.

Red ABI compilation rejects the old int() export. Core pointer tests pass
4 cases/1195 assertions: signed/tied identity order, outside-range
canaries, empty/single cases, full256 entries, signed extrema, record/
pointer preservation and every partial range of a16-entry list. Existing
AI ranking/promotion controls retain tie ordering and RNG behavior;
combined pointer/planning selection is21 cases/4679 assertions in both
configurations. Full Debug/Release suites pass800 cases/3,532,240
assertions each;95 copier/audit controls and focused/default quality
are0 new, with both hosts linked.

Two copier runs preserve306 protected paths and repeat identically.
Obsolete generated Containers.c/.h disappear under registered ownership;
the original module symbol keeps its binding via the new provider header.
Only Engine/Tests projects are rendered with existing scaffold APIs,
preserving the dirty owner solution. Original C, assembly and retained
M2C output establish the algorithm; M2C's guest-pointer and recursive
register artifacts are not copied into native storage.
Private `sort42-*` retains commands, red/green and generation evidence.
This advances128/186 original entries,58 remaining, without whole
planning gameplay, AI-state C++ promotion or compatibility retirement.
Publication and exact CI are recorded on#42.

### Complete authored scene30 return chain (#151/#149, 2026-10-25)

The3600-tick observation is not evidence of a permanent event hang:
its thread PC progresses665->1031->1248 at2000/3000/3600. A separately
bounded Release run1792989783417541-0 on runtime961cc1e8
(documentation-only descendant af1435ce) retains original dialogue controls
and grants enough observation time, without changing the acceptance gate:

```powershell
build\Release\VS.CombatLab.exe --door-transitions --door-from z048-r006-d000 --door-limit 1 --frames 36000 --speed 8 --scripted
```

Scene30 is at32/17/event phase4/PC233 at1050; by9000 its authored handoff
has reached scene26 at40/21, still phase4. Normal field control returns13320.
All121 recorded frames13320..13440 are ready40/21/control1/transition0/
event phase0, satisfying the unchanged120-tick gate; normal teardown13452,
0 engine errors and0 forced-skip frames. There are280 original waiting-
dialogue confirmations, no Start skip and no actor/HP/story/PC writes.
Inactive PC1481 remains populated at phase0 and is not an error.

Strict `doors151-scene30-long-verified.json` retains the continuous gate
interval and state-aligned1050/9000/13320/13440 observations. The original
short-budget rows stay blocked. This is one current authored chain/door
completion, not normal-speed audiovisual parity, an all-scene result or
the original #167 ACFD matching-image persistent reproduction/producer fix.

Independent Debug1792992005781732-0 on6ab2aef8
(exe557691c7/PDBdb76150f) uses the same36000/speed8 and original dialogue.
It exactly matches control13320, quiet gate13440, teardown13452 and280
confirmations, with0 errors/forced skips. Strict
`scene30-debug-long-verified.json` verifies every frame of the same
121-frame destination interval. Both builds therefore complete this
current standalone authored chain; the unavailable original ACFD image/
exception/owning-producer requirements remain separate.

The other four reported event-budget cases need state-aware classification,
not an assumed VM hang. Samples show49/1 phase0 with changing control1/2
and later phase2 reentry,51/26 already hands off to51/4 with phase0 and
changing control1/2, and53/10 likewise has phase0 with control1/2.
`doors151-event-budget-observations.json` preserves these distinctions.
Neither their reason strings nor a longer arbitrary timeout prove a defect;
combat/entry acceptance remains open and no predicate is weakened.

### Final directed-door inventory slice (#151, 2026-10-25)

Release run1792988508566012-0 uses a frozen e9f0dbab image
(exe a0294afe/PDB10b89faa), not the later lifecycle worktree.

```powershell
build\Release\VS.CombatLab.exe --door-transitions --door-from z048-r006-d000 --door-limit 410 --frames 3600 --speed 64 --scripted
```

The selected IDs/order exactly equal original inventory indices591..1000.
All410 selections finish teardown:305 pass the unchanged production gate,
105 remain honestly blocked,0 engine errors,0 failed and0 unrun.

| Blocked reason | Records |
| --- | ---: |
| Fixture defeated before transition acceptance | 93 |
| Destination entry event exceeds budget | 4 |
| Source entry event exceeds budget | 1 |
| Destination zone unavailable | 5 |
| Destination room unavailable | 2 |

The seven availability-blocked records are not initialized. Their raw
destinations are0/255 for z048-r022-d003, z053-r030-d000,
z069-r000-d000, z070-r000-d000 and z100-r000-d000; z250-r000-d000/
d001 target254/0. They need separate source/availability classification,
not an assumed unused-record exclusion, fabricated load or passing result.
Combat defeats remain real defeats; no HP/story/readiness writes, skipped
events or synthetic completion make them pass.

All default trace logs and frames are retained under the frozen image's
private run directory. The7200-second outer watchdog is distinct from
the3600-tick per-case budget. An initial attempt added unsupported
--log-level; startup rejected it before gameplay. The supported retry
above uses full default logging, not a partially initialized game failure.

Strict `doors151-remaining-release-verified.json` records counts, exact
selection and all seven initialization limits. This is one Release slice,
not a current all1001 result assembled from earlier revisions, normal-speed
visual parity or the original #167 full persistent history. #151 and its
remaining combat/event/availability acceptance stay open.

### Shared AI persistence and lifecycle leaves (#42, 2026-10-25)

F01820/F01831/F01835 now belong to the existing ActorCleanup API. Original
7DE94.s:9-40 and the already-working native RuntimeSnapshot projection agree
on the exact delay/relation/behavior packing. The copied room-cache export
was still a zero-returning stub. Both native save staging and room-cache
storage now use one typed int16 projection, preserving missing-state-1
without mutating live AI state. Native save schema, cache ownership and
save/apply consent are unchanged.

Original7E05C.s:241-252 clears only actor hold byte0x1A; its native caller
ignores the assembly return, so the owned API is void(int). Source
7E05C.s:504-512 is an immediate-return hook whose delay slot changes only
caller-saved t0: its native body is truly empty rather than falsely reporting
an unported function. No incomplete navigation producer/updater is enabled.

The red selection has4 failed cases/278 failed assertions out of293.
Final `VS.Tests.exe "[lifecycle-leaves]" --reporter compact` passes5
cases/98,611 assertions: all8192 persistence combinations across boundary
slots, ignored relation bits, full8-bit delay data, missing-state sentinel,
byte-for-byte source immutability, all16 present/absent reset slots and
repeated no-op calls with no unported hit. Existing runtime-snapshot tests
also retain their exact enemy AI flags under the shared helper.

Full Debug/Release suites pass796 cases/3,531,045 assertions each;94
copier/audit controls and focused/default quality are0 new. Both hosts
link. Two generations preserve303 protected paths and repeat identically,
changing ActorState.c/.h plus the existing Boot, movement and room-cache
consumer includes. The hand-maintained snapshot replaces duplicate logic
with the same canonical projection. Private `lifecycle42-*` retains exact
commands and red/green/ownership evidence.

This advances127/186 original entries,59 remaining, preserving original
order and prior124 checks. Component/source/caller conformance is not
full room-transition persistence gameplay, a new save-format milestone,
whole AI-state C++ promotion or compatibility retirement. Publication
and exact CI are recorded on#42.

### Resident SDK scalar trig exports (#42, 2026-10-25)

F03400/F03401 (`rcos`/`rsin`, SLUS31724.s:9/12) have instruction bodies
identical to the BATTLE scalar pair after label normalization and use the
same original table. They now use the existing canonical native alias
mechanism too; no lookup/math algorithm or working ending implementation
changes. Native declarations are checked as int(int), replacing old int()
stubs. M2C private nop/fallthrough extraction and its known unsigned16
negation inference limit are retained as `resident-trig42-*`, not copied
into runtime semantics.

Both Debug and Release pass6 cases/41,062 assertions. Both scalar families
independently match the complete original signed-output fingerprint
8E620CA359EB4481; all4096 results also compare across families. Extreme int
inputs preserve low12 wrapping. Original resident80040F24/80040F28 entries,
alongside the BATTLE entries, resolve to and invoke the correct typed
canonical bodies and reject+1 interior function addresses.

Full suites pass791 cases/3,432,434 assertions each;93 copier/audit controls
and focused quality are0 new, with both hosts linked. Two copier runs
preserve302 protected paths and repeat identically, changing only
FixedPoint.c/.h. Existing loader rows remain unchanged because #186's
generic fix already includes the two new macro aliases; all11 configured
native redirects retain their original-address bindings. No original table
payload is committed. This advances124/186 original entries,62 remaining,
without whole GTE/matrix/ending promotion or compatibility retirement.
Publication and exact CI are recorded on#42.

### Native redirect loader bindings (#186, 2026-10-25)

The trig source aliases exposed a distinct loader regression: the copier's
emitted-function set included generated bodies and owned functions, but not
redirect macros. Original-address module symbols disappeared even though
the alias targets were real native functions. Actual production
`ResolveModuleAddress` calls over `Content_moduleSymbols` returned null for
BATTLE80098AB4/80098AB8: one red case/two failed assertions. This is a
demonstrated generator/resolver contract defect, not a guessed gameplay bug.

The function-symbol set now includes every configured native redirect name.
All9 original bindings are emitted through the existing providers/macros,
without synthetic bodies or a new guest-memory model. Native controls
resolve both trig entries to the correct canonical function, invoke them
with the proper int ABI and reject an interior+1 function address. The
resolver itself, data extents and ordinary unrelated symbols are unchanged.
A copier regression requires a binding for every configured redirect.

Full Debug/Release suites pass791 cases/3,424,199 assertions each;93
copier/audit controls and focused/default quality are0 new. Both shipping
hosts link. Two copier generations preserve302 protected paths and repeat
identically, changing only ModulePointers.c. Private `redirect186-*`
retains commands, red/green and ownership evidence; publication/exact CI
are on#186. This completes the #42 alias wiring, but does not establish
particular gameplay completion or whole compatibility-layer retirement.

### Canonical fixed trigonometry exports (#42, 2026-10-25)

F00764/F00765 now use the established native-function redirect mechanism to
alias the existing typed `Core_Gte_Cos`/`Core_Gte_Sin` bodies. Original
2EA3C.s:1731/1735-1766 and Ending D4.c:187 establish the same12-bit angle
wrapping, quarter-wave samples, cosine phase and signed32 negation.
Read-only SLUS table bytes at199184 have SHA256
5334d250971879c21613208379a26d7c3a66fee2ea6f2e797af707da3d992660.
All4096 canonical samples match the existing native formula, so no duplicate
lookup implementation or change to the working ending helper is warranted.

Red ABI checks reject both old int() stubs. Actual native Debug/Release
`VS.Tests.exe "[fixed-trig]" --reporter compact` passes5 cases/32,819
assertions. This includes the full original signed-output fingerprint:
all4096 ordered sine/cosine pairs, each signed32 little endian, yield
FNV1a64 **8E620CA359EB4481**. Tests store only that non-reconstructive
fingerprint, not a copied table or other disc-derived data. Additional
controls cover alias identity, mathematical octants, every angle's range/
phase/sign/period and minimum/maximum int wrapping without signed overflow.
The computed-register rotation dispatchers remain separate unimplemented
entries; they are not certified by these scalar controls.

The raw M2C attempt ignores the reference's .nop spelling and reports a
missing return delay slot. Private extraction normalizes it to nop and
retains cosine's actual fallthrough. Both functions then decompile, but
M2C incorrectly narrows the negated result to unsigned16. Original `negu`
and the existing signed ending routine are authoritative; no blind M2C
output is copied. Raw failure, derived inputs, successful C, source/table
hashes and original-output fingerprint provenance are private `trig42-*`.

Full Debug/Release suites pass790 cases/3,424,191 assertions each;92
copier/audit controls and focused/default quality pass with0 new findings.
Both shipping hosts link. Two copier runs preserve302 protected paths
and repeat identically, changing only Trig.c/.h and ModulePointers.c.
The original cohort advances122/186 with64 gaps remaining, preserving all
prior120 checks and the original186 IDs/order. No every-scene fidelity,
whole GTE C++ promotion or compatibility retirement follows from this
bounded scalar-alias verification; publication and exact CI are on#42.

### Navigation presence, status and copy leaves (#42, 2026-10-25)

F01801/F01802/F01803 are now owned in the existing NavigationPrimitives API.
Original7C7E0.s:122-193 and read-only rood-boi e59320ca
actor_update.c:57-90 establish the two shared-entry contracts: boolean
grid/state presence, or missing2/reached0/moving1 with only tile X/Z bytes
compared through FF00FF. The old catalog note inverted0/1. Both native entry
points require an explicit actor ID; the copied scripted-path consumer now
forwards its argument instead of relying on a still-live PS1 a0 register.

The copy helper uses the full typed native record, not a fixed0x484-byte
PS1 memcpy. It preserves borrowed native pointers and untouched state, then
sets actor ID, step height320, step distance3, navigation flags0, route
marker0, settings bits3 and movement-enabled1. It remains valid without a
grid, as the original copy routine does not gate on navigation availability.
The existing16-slot native state bound and explicit contract errors are
retained; no sentinel alias into adjacent globals is introduced.

Red ABI compilation rejects all three old int() exports and typed calls.
`VS.Tests.exe "[override]" --reporter compact` covers5 cases/183 assertions:
all16 slots with present/absent grid/state, missing route, ignored layer/tag
bytes, both horizontal mismatches, full native copy and borrowed pointer
width, source/self aliases, canaries and repeated resets. A real
`Actors_IsScriptedActorPathFinished(7)` control checks moving/reached/missing
results and clears only the reached actor's script-collision bypass.
Full Debug/Release suites pass785 cases/3,391,372 assertions each;90 copier
contracts, focused compilation analysis and default quality are0 new.
Two copier runs preserve302 protected paths and repeat identically, changing
only Navigation.c/.h and the Movement.c caller. Source/commands and red/green
reports are retained locally in `navigation42-*`; publication/CI are on#42.

This advances the original cohort to120/186,66 remaining. It does not complete
scripted navigation: F01805 request construction and F01806 frame updates
remain separate stubs. No complete scene traversal, native AI-state C++
promotion or compatibility retirement follows from these leaf contracts.

M2C51C8 independently recovers the copy and all seven resets. The original
5158 listing ends before its `jr ra` delay slot, so a raw M2C attempt fails.
A private derived input includes the exact following original instruction
(`lui v1` at the51C8 entry); M2C then succeeds and agrees with both shared
entry contracts. Raw diagnostics, derived input/hash and successful C remain
in `navigation42-m2c-*`; no resource or runtime instruction was changed.

### Vignette ordering-link repair (#184, 2026-10-25)

The unchanged failing Debug image4f30cd49/PDB331f0f8a repeats the original
persistent-history failures at144918/144920. Coherent query/read-only snapshots
prove that both overlay entries at OT+8 contain native eight-byte pointers to
the final vignette ring packet (E1000220/3AA0A0A0). The high word0x7FF6 overwrites
the adjacent OT entry at+12. With39287 registered handles that becomes a valid
unrelated old packet, closing the captured410-node cycles. The observer only
briefly pauses its owned process for coherent reads; no code, actor, HP, story,
PC or input writes establish this provenance.

F02461/F02462 (`func_800F9EBC`/`func_800F9FB8`, original
GIM/SCREFF2.PRG/5E8.c:23/41) incorrectly treat a four-byte ordering link as
`void**` and re-register an already encoded previous head. The copier now
emits `u_long*`, reads that encoded head directly and publishes exactly one
`LegacyGpu_Handle(packet)` word. Existing error/return-1 cycle detection is
unchanged; no skipped draw, default packet, buffer expansion or scene bypass
is a repair. Original F02460 angle/radius arithmetic is already correct and
is not changed.

`VS.Tests.exe "[vignette]" --reporter compact` fails two of three cases before
the producer correction and passes257 assertions afterward in Debug/Release.
Controls cover all16 corner triangles and64 elliptical ring packets, every
geometry/color/mode word,752-word cursor advance, packet/adjacent-entry
canaries, previous-tail retention, reverse packet order/lengths and an actual
82-node GPU walk. Fixed cardinal controls also preserve the original ellipse.
Full native suites pass780 cases/3,391,189 assertions in each configuration;
89 copier contracts, focused compilation analysis and default quality pass
without new findings or baseline edits. Two copier runs are identical and
preserve301 protected paths; generated changes are only ScreenDistortion.c/.h.

Actual acceptance uses the original persistent history:

```powershell
build\Debug\VS.CombatLab.exe --door-transitions --door-from z040-r008-d000 --door-limit 132 --frames 3600 --speed 64 --scripted
```

Release uses the same selection, budget and predicates. The outer watchdog
is7200 seconds, not the earlier insufficient900-second budget. Frozen fixed
Debug1792982973682278-0 uses exe3ca80430/PDB8ecd4651; Release1792982973680555-0
uses exea772e506/PDB2403d431. Private `vignette184-fixed-<configuration>*`
records the full command, hashes, parent revision3c7982ee and dirty source
state; `packet184-*` retains the unchanged-image graph proof.

Debug completes the original114-case acceptance prefix:82 passed/32
budget-blocked,0 engine errors and114 complete teardowns. Script114 now
renders through3612 instead of failing306. Release completes all132:
97 passed/35 blocked,0 errors/all teardowns. Both retain field rendering at
original144918/144920 (script ticks294/296), scene30/event0/source1/phase4,
thread0 PC233 and models0/170/169/10. Release script132 also matches the
separately tracked #167 interval at1050/wait70, but that interval was already
not reproduced by pre-fix controls; do not claim a causal #167 repair.
Normal-speed Release1792983547181492-0 renders600 ticks with0 errors and
complete teardown; event completion remains honestly budget-blocked.
Root fixing260f121b has exact successful Source Quality40261350623.

The full Debug132 selection is NOT clean: the next script115
`z047-r000-d000` (47/0->48/26) faults at149109/script_tick873 with access
violationC0000005, reading7FF66D81D670 at7FF66F3F4BA6. Its last durable frame
is ready48/26/control1/transition0, scene19/event0/source1/phase0, models0/89/89.
It remains running in the result file, with17 later cases unrun. This newly
confirmed failure is #185, not a rewritten passing cohort or assumed shared
root with #184/#167. A private watcher intended to request normal close after
114 completion found no owned window after the native exit and sent no
WM_CLOSE. No owned replay remains active.

Strict reports `vignette184-verified-Debug.json` and
`vignette184-verified-Release.json` distinguish the accepted prefix from the
whole selection. The delivered #184 scope is the proven single-word vignette
producer and original entry interval; it is not normal-speed pixel parity,
all-scene/all-door completion or compatibility retirement. The later #185
failure is repaired below; its old cohort remains interrupted, not rewritten.

### Full-width transition tile-translation calls (#185, 2026-10-25)

History is not required: unchanged3ca80430/PDB8ecd4651 standalone
1792985556391334-0 repeats the same access violation. A local Windows Debug
API observer captures the first-chance fault, module base and private dump
without gameplay memory writes or target code patches. RVA214BA6 resolves
against the matching PDB to `Actors_TranslateActorToTileAtRate`; the
unwind-layout-validated return2149F8 identifies
`Actors_UpdateActorTeleportTransition`, not the ordinary event caller.

Original3A1A0.c:31 locally declares this function as
`u_int(u_char, void*, int)`, conflicting with its actual40564.c:661
`void(int, Actors_TileTarget*, int)` definition. The copier preserves that
wrong local view in three transition/cancel/travel function-pointer casts.
Fault disassembly writes only CL from slot2, leaving upper argument bits
undefined; the callee reads a full int and sign-extends it for indexing.
Captured RCXFFFFFFFFFFBE1002 and actor-array base exactly explain the bad read.
This is not a missing optional actor to skip or a bad ID to clamp.

F00896/F00897/F00898 copier fixes call the existing typed function directly,
with explicit packed-target data views. All three incompatible call casts
are removed. The original motion/rate/facing/animation logic and optional
binding behavior remain unchanged. A red source-contract test rejects the
old casts and verifies all three direct calls. Final91 copier tests pass;
two generations preserve302 protected paths and repeat identically, changing
only Movement.c. Full native Debug/Release suites pass785 cases/3,391,372
assertions each; default quality is0 new with the existing baseline unchanged.

Actual fixed Debug1792986145027095-0 (exe832d086c/PDBbbfec8e0) and Release
1792986145026276-0 (exe435e23f7/PDB9c5c1cdf) use the same single directed
door selection,3600/speed64 and original scripted dialogue. Both arrive48/26
at233, restore control797 and satisfy the unchanged120-tick readiness/
scene-event quiet predicate917, then complete normal teardown929 with0
engine errors. Recorded frames872/873/874/917 retain ready48/26, control1,
transition0/event phase0 and real models0/89/89; the wolves continue moving.
Commands, hashes, strict route results and boundary checks are private
`actor185-*`; unchanged-image debugger/dump evidence is `packet185-*`.

The old persistent Debug132 selection is still incomplete after script115;
its counts are not rewritten. These bounded standalone results do not
certify every door, cancellation/travel presentation, all scenes, C++
promotion, compatibility retirement or a causal #167 repair.

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

Battle-ability credit and learning are separate. Successful offensive timing
retains its credit on a lethal hit (#183), without queuing another strike at a
dead target. Successful defenses retain their existing credit path. Credit is
global; filling a threshold on a surviving target does not learn an ability.
Further successful inputs keep banking credit toward later abilities rather
than being discarded while a reward is pending. The persistent 16-bit counter
saturates at65535, and no further credit is needed once all22 unlocks are learned.
The normal enemy-death reward can release pending unlocks after killing the
same or a different target.

The original offensive progression gate, story flag0x82>=10, still applies.
These isolated Lab profiles do not manufacture completed story events to meet
it. Defensive progress does not use that particular gate. Component tests
cover all22 lethal thresholds, overflow banking, saturation and non-kill
deferral; a Lab run below the story gate is not proof of eligible offensive
credit accumulation.

One original-game controller acceptance from an untouched copy of a real
post-Minotaur save (Debug1792958740259860-0) earns25 points on the9/17 dummy
without learning, then opens the authored door with Cross and defeats a
different9/12 enemy at8360. Action24 is learned at8836. Successful lethal
timing still adds point46 at8390 with the victim at0HP; defensive successes
add26,47 and48. All48 increments are retained, followed by120 normal-control
frames and exit0 at9313. No live HP, story, PC or progression edits were used;
the owner's save is unchanged. This is a bounded Debug gameplay acceptance,
not a claim of another full campaign or identical Release replay.
  This supports [#11](https://github.com/JEschete/VS-NativePort/issues/11),
  not a claim that every art or acquisition path is already verified.

Profiles retain the real weapon, resource, status and target restrictions.
In particular, draw the weapon before selecting a Break Art; its menu entry
is unavailable with the weapon sheathed. These profiles do not implement a
loadout editor or complete the wider spell/ability/equipment trackers.
Enabling a menu gate does not repair that submenu. The reproduced Score crashes
and their bounded native repair are documented in the #65 checkpoint below.

Presets and encounter logs include `ashley_profile`. Existing version-1 presets
without that key still select `prologue`; unknown profile names are rejected.
F10 followed by Start initializes a fresh player. Changing the ASHLEY row back
to Prologue also removes the advanced profile's grants on the next start.

Other options: `--data <directory>`, `--frames <ticks>`, `--speed <1..64>`,
`--capture <bmp-path>`, `--catalog-only`, `--scripted` (ignore gameplay devices),
and `--press <tick:Button[,tick:Button...]>`. A scripted press lasts two ticks;
`+` combines buttons. Host function keys remain available during scripted runs.

### Detailed logging in the actual Desktop game

The normal game and Combat Lab now use the same shared gameplay trace writer
and frame/actor encoding. In
[`Native/Desktop/src/main.cpp`](../Native/Desktop/src/main.cpp), set:

```cpp
constexpr bool kEnableDetailedGameLogging = true;
```

This bool is near the top of the file. It defaults to **true** for failure
investigation in both Debug and Release; change it to **false and rebuild**
for the previous lightweight Desktop logging behavior. No new runtime option
is required.

When enabled, each actual-game launch creates its own
`build\<Configuration>\game-logs\<run-id>` directory:

- `manifest.json`: build, source revision/dirty status (or explicit unavailable
  provenance), full arguments and compatibility-log location.
- `session.log`: shared engine messages, at Trace/all channels by default.
- `events.jsonl`: monotonically sequenced, tick/elapsed-time-correlated engine
  events and the same per-tick input, actor resources/status/AI, targeting,
  menus, inventory and room state used by Combat Lab.
- Actual-game frames additionally contain `scene`: event/thread cursors and
  waits, dialogue wait state, camera parameters/view matrix, effect progress
  and model/animation pose state, including cutscene actors without combat
  statistics.

The current session's text is also mirrored to the existing `vs-native.log`
beside the executable, preserving existing replay/error tools. That convenience
mirror is replaced on launch, **but prior run directories are not overwritten**.
Read the run folder for retained evidence. Existing crash errors/symbolized
stacks go through the same logger; the watchdog's dedicated `vs-native-hang.log`
retains its existing executable-directory location.

Structured frames flush the JSONL and both text sinks. Write/flush failures are
explicitly reported and the Desktop returns a failure result. Startup argument
errors, fatal exceptions and host results have their own records. CLI
`--log-level` / `--log-channels` still filter engine messages if deliberately
specified; they do not disable the bool-controlled structured snapshots.

Snapshots are read-only. Title/loading/unload/reset/teardown frames mark scene
data unavailable rather than dereferencing released room/actor storage. The
build-time false path does not create run folders or inspect gameplay state;
normal Info logging and existing CLI filtering remain available.

Detailed traces are high-volume and add disk/inspection overhead. They are
diagnostic evidence, not proof that a scene completed correctly. This supplies
normal-game logging parity and useful scene state; #149's complete cutscene
inventory/playback mode and acceptance remain separate.

#### Bounded effect and particle motion paths

Both Desktop and Combat Lab accept `--trace-effect-paths first:last`, an
inclusive range of positive **host logical ticks**. It is off by default and
requires normal `--speed 1`; malformed ranges and accelerated runs fail
explicitly. Trace-level logging and the `effect-path` channel must be enabled
(the normal detailed-log defaults already do this).

```powershell
.\build\Release\VS.Desktop.exe --speed 1 --trace-effect-paths 37800:42000
.\build\Debug\VS.CombatLab.exe --speed 1 --trace-effect-paths 1:1200
```

The regular run `session.log` and engine records in `events.jsonl` receive
JSON messages on `effect-path`:

- `frame`: an explicit marker for every selected logical tick, including ticks
  with no particle or translucent draw.
- `particle`: pool/slot/instance, effect ID, age/lifetime, emission flags,
  attachment index, raw fixed-point position/velocity, local and world points,
  attachment/view/camera matrices, projection offset/distance/flags, projected
  center/depth, culling outcome and emitted command addresses.
- `primitive`: final translucent polygon, sprite/rectangle and line vertices,
  texture coordinates/colors/page/palette/blend, clipping/draw offset and display
  state. This catches screen-space effects that do not use the generic particle
  pool. `screen` is the GPU command coordinate before framebuffer draw offset;
  `raster` is the page-space coordinate after that offset. Buffer flips must not
  be mistaken for effect movement.

Join particle `command_addresses` to primitive `command_address` **within the
same run/tick** to follow local/world motion through actual emitted geometry.
Pool slots, instance addresses and packet storage can be reused; these are not
globally unique lifetime IDs. Use effect context, age/lifetime and the adjacent
samples when distinguishing reuse. Asset filenames remain in the existing
content-load messages; a numeric effect ID is not a guessed asset path.

Recording paths does not prove flare placement correct. Observe the whole
effect at normal speed, retain continuous frames/video and align event/camera
state; do not discard the transient interval with a pose filter or relocate a
flare from one selected screenshot. Room/model effects without particle
instances have primitive paths but not an invented particle/world origin.

#### Post-learning particle origins

The gold celebration after **Mastered the spell** is common secondary effect 20
(`Effects_SpawnAbilityLearnedEffect`), not the preceding spell/item effect.
For a repeatable production-menu fixture, use original zone 9/room 17, seed 1,
empty enemies, Ashley at tile 2,4 and profile `all-items-a`. That profile stocks
Heal grimoire 425 without pre-learning Heal. Hold L2 and press Up to open the
original Items/MISC shortcut; choose **Grimoire Guerir**, confirm Ashley as the
target and let the normal item/result/learning sequence finish. Do not invoke
Heal directly or toggle its learned bit to test this burst.

The native parent-particle prefix correction in F01535 is covered by exact
position/appearance assertions for tick, lifetime and texture expiration.
Actual Debug/Release Lab runs consume 425 from 10 to 9 and learn action 96 at 1392.
Matched-seed Release before/after traces preserve all 4205 effect-20 records and
the same learning timing; parent-relative children now inherit Ashley's real
particle coordinates instead of widened link/texture-pointer bytes. The
active fixed capture restores the gold burst around Ashley without suppressing
particles, clamping their spread or changing the earlier E043 effect.

Local evidence is `learn20-*` in session files, including active before/after
captures and exact trajectory reports. These are Lab fixtures, not imported
saves or claims of campaign progression. The related E211 window effect is
verified in full production replays, not a solo external-event7 Lab fixture:
that fixture omits Dtok's required actor context and is not an equivalent control.
Published fix `e939db22` has green Source Quality 40217651083. Actual 1x Debug
1792952304769540-0 and Release 1792949707855559-0 defeat Dtok at 38142, retain
all 4501 optical ticks 40500..45000 and reach 120 consecutive normal-control
frames, exiting 0 at 68612. All decoded video RGB frames match their source
captures, and every Debug/Release frame matches. E211 phase4 is 42132..43243;
cleanup ends at 43248.

Original control `rays168-reference-normal11` uses actual New Game/combat,
normal speed and a DPI-correct full physical client (849x655). Its 6509 decoded
frames preserve 111.024 seconds; the maximum measured host capture gap is
49.7464ms, not a guarantee of every guest VSync. At aligned camera/hero position,
rotation differs by at most 7/4096 and translation by 0; source and colored
ghosts agree at the window. This is not full body-pose or pixel-perfect scene
parity. Local `window211-*` reports retain exact alignment and capture limits.
The owner accepted #168 as resolved on 2026-10-25; broader #76/#149 scope remains
separate.

#### Local logging verification

Verified on local `508b4bd79d58e37421fdd9c693c92937f2b5de0e` with these
logging changes and preserved owner edits **uncommitted**. Per-run manifests
retain the actual dirty-file list; this is not published-revision CI evidence.

- Built `Native\Desktop\VS.Desktop.vcxproj`,
  `Native\Tools\CombatLab\VS.CombatLab.vcxproj` and
  `Native\Tests\VS.Tests.vcxproj` for Debug and Release, using the documented
  MSBuild with `/m /nodeReuse:false /p:Configuration=<Configuration>
  /p:Platform=x64 /p:VcpkgManifestInstall=false`.
- Final `build\<Configuration>\VS.Tests.exe
  "[trace],[sequence],[diagnostics]" --reporter compact`: **176 assertions /
  21 cases pass in each configuration**, including cached-snapshot rejection
  after teardown. Copier tests: **36 pass**; repeat generation stable. Scoped
  format/cpplint/ruff/clones/tidy: **0 findings / 0 new**.
- Final Debug `VS.Desktop.exe --frames 2400 --speed 32 --scripted --press
  1000:Start,1300:Circle`: **2400 frames / 986 ready / 0 errors**, valid changing
  camera matrices, complete session and byte-identical live text mirror.
  Run: `build\Debug\game-logs\1792803286381062-0`; summary:
  `build\quality\desktop-trace-final-enabled-evidence.json`.
- Release lifecycle replay: **7800 frames / 6367 ready / 0 errors**, including
  rooms `1/0 -> 8/0 -> 9/0` and menu entry. Evidence:
  `build\quality\desktop-trace-release-lifecycle.json`. This is logging/lifecycle
  evidence, not acceptance of skipped story scenes.
- A real Debug build with the bool false created **no detailed session**,
  retained prior run files and kept default Info logging. Both final Desktop
  builds were restored to true. Evidence:
  `build\quality\desktop-trace-disabled-evidence.json`.
- Missing-data and invalid-log-level probes exited **1** with retained failure
  records: `build\quality\desktop-trace-failure-evidence.json`.

### Single-window script sequences

Use `--sequence <json-path>` instead of launching a new process for each preset.
One window, renderer, audio device and encounter catalog remain alive for the
whole sequence. Each entry still uses a fresh production encounter; reaching
its frame budget requests the normal stop and waits for teardown before the
next entry starts. This does not retain live enemies, modified inventory or
resources from the previous encounter.

Save a manifest beside the existing preset files, for example:

```json
{
  "version": 1,
  "scripts": [
    {
      "name": "first encounter",
      "scenario": "last-scenario.json",
      "frames": 900,
      "press": "200:Triangle,300:Cross"
    },
    {
      "name": "repeat with a fresh world",
      "scenario": "last-scenario.json",
      "frames": 900
    }
  ]
}
```

```powershell
.\build\Debug\VS.CombatLab.exe --sequence build\Debug\combat-lab\sequence.json --speed 8 --scripted
```

Preset paths are relative to the manifest, not the working directory. Each
script requires a positive integer `frames`; `name` and `press` are optional.
Input ticks restart for each script and include room loading, just like a
standalone `--press` run. Unknown buttons, malformed entries, missing presets,
invalid setups and presses outside their budget are errors, not skipped cases.
Use `--speed 1` for listening and interactive fidelity checks.

Do not combine `--sequence` with `--scenario`, `--start`, `--frames`, `--press`
or `--catalog-only`: frame budgets and button schedules belong in the manifest.
`--data`, `--speed`, `--scripted` and the normal pause/step controls remain
available. **F10 cancels the sequence**, tears down the current encounter and
leaves later scripts unrun. The ordinary editor's F10 behavior is unchanged.
Closing the window also cancels a sequence. Cancellation returns a nonzero
exit code; no later script is reported as completed.

The run directory retains the master engine log, structured trace and
`sequence-results.json`. Each `script-0001`, `script-0002`, etc. directory has
its own full `events.jsonl`, setup, manifest and result. Records include both
the host tick and the script-relative `script_tick`. A completed result means
that initialization, the requested observation budget and normal teardown
finished without engine errors; it is **not** evidence of victory or complete
gameplay correctness. Rejection, engine errors and failed teardown stop the
sequence rather than silently advancing.

### Every directed door: persistent loader/entry-event tests (#151)

```powershell
build\Debug\VS.CombatLab.exe --door-transitions --catalog-only
build\Debug\VS.CombatLab.exe --door-transitions --speed 64
build\Debug\VS.CombatLab.exe --door-transitions --door-case z009-r005-d001 --speed 64
build\Debug\VS.CombatLab.exe --door-transitions --door-from z009-r017-d003 --door-limit 20 --frames 3600 --speed 64
```

Use Release equivalents to test that configuration. `--door-case` selects one
stable source-zone/room/record ID; `--door-from` resumes inclusively in inventory
order, and `--door-limit` bounds the selected range. Invalid IDs and conflicting
scenario, sequence, input or selector options fail explicitly. `--frames` is a
per-case budget (default **3,600 logical ticks**), not a successful outcome.

The original local-disc inventory contains **1,001 directed door records** in
**255 ZNDs / 421 room references**. Aliases, duplicate links, conditional doors,
self-loops and cross-zone doors retain their original record identity.
`door-inventory.json` always contains the complete inventory, even when runtime
selection is filtered. Unavailable destinations, unsupported source fixtures
and unreadable catalog data are reported, not silently omitted. The initial
inventory has 27 preflight-blocked records; runtime story prerequisites can
block additional cases.

This is **loader + production entry-event + restored-control coverage**. It
does not prove physical controller approach, locks, switches or campaign
reachability. A case starts with the post-prologue profile on a supported
non-door source tile, retaining original actor placements and event scripts.
It never writes story flags, injects missing actors or forces event completion.
The requested door gate must actually be satisfied.

The request API accepts an original door index only in an idle ready field. It
queues the index and source IDs, not a borrowed pointer into room storage. The
game fiber rechecks source ownership and gates, then uses the production
room/zone handoff and entrance placement. A stop request takes precedence;
reset/unload revokes diagnostic readiness and pending requests.

A **pass** requires an observed loss of readiness during loading, the exact
destination, an idle original entry event, field control restored, and **120
uninterrupted observation ticks**. A pending transition bit alone cannot pass a
self-loop. Restarted events, taken control or revoked readiness restart the
observation interval. Actual defeat, an unmet gate or an unsettled event does
not become success when the budget expires.

The runner uses one persistent window for the selected range, with production
stop/cleanup between cases. It continues after nonfatal failed/blocked cases.
Physical gameplay devices and stat/status adjustment hotkeys cannot alter
automated trials. F6/F7 still pause/step; F10 cancels the remaining sequence.
Only an actual waiting dialogue box receives brief Circle confirmations
(two ticks every 30); **Start/scene skipping is never injected**. Scene JSON
contains exactly the event VM's eight logical `dialogue_waits` values, not the
legacy declaration's overbroad 80-byte extent.
Likewise, thread snapshots contain the four dispatched VM threads, using their
four-entry wait-frame table rather than the six-entry legacy cursor extent.

Each run saves the full inventory, selected `sequence-results.json`, per-case
text/JSONL traces and manifests. Results start as `not_run`, then record the
active case and final `passed`, `blocked` or `failed` state, reason, request/
arrival/control ticks, destination observation count, dialogue pulses and last
room/control state. A fatal process crash leaves the active/unrun checkpoint;
resume using its ID after investigating the failure. An interrupted batch is
not full coverage. Any blocked/failed/cancelled selection returns **2**;
configuration/tool errors return **1**. Engine errors terminate the current
case as failed before the next production cleanup.

Presets may explicitly set `"room_events": "production"` for interactive
original-room investigation. Omission or `"isolated"` preserves ordinary
Combat Lab behavior. Production policy rejects nonempty custom enemy lists;
group editing is disabled and the editor does not mislabel unpreviewed
original placements as zero enemies or solo Ashley.

Asset-independent parser/planner, lifecycle, input, editor and native-request
regressions run with the regular suite. The opt-in disc test independently
decodes and compares every original door field:

```powershell
build\Debug\VS.Tests.exe '[doors]'
build\Debug\VS.Tests.exe '[disc-door-inventory]'
```

The latter requires local extracted data and is hidden from ordinary CI:
**41,459 assertions / one case** validate all 1,001 records, not runtime
execution of their scenes. Do not commit or attach the original assets.

#### Local repairs exposed by the matrix

All following work is local to `508b4bd7` plus the recorded dirty worktree;
publication/CI/issue-closure gates remain open. Earlier historical suite totals
are not fresh verification of the final source.

| Issue | Reproduced defect and bounded repair |
| --- | --- |
| #152 | The original 9/6 entry script references an optional absent slot4. Movement wrappers now diagnose/no-op before dereferencing, preserving valid movement, rate and skip behavior. Original 9/5-to-9/6 entry/control restoration was replayed in Debug/Release. |
| #153 | Perception treated auxiliary actors as ordinary actor-record owners. Target/occluder queries now use their authoritative `Battle_Actor::model` prefixes; separate red target and blocker controls became green. |
| #154 | Mutable actor kind left perception membership and allocated AI state alive at retirement. Cleanup now retires actual tracked membership/allocation and cached aliases, retaining Ashley and wider non-AI lifetime rules. |
| #155 | Session heap reset reclaimed allocations without invalidating native AI globals. Post-reset invalidation clears bindings without freeing/dereferencing expired storage. A history-dependent six-door crash reproducer then completed all six cases. |
| #156 | Gimmick teardown scanned above a missing stack top. It now finishes that scan after preserving preceding member updates. Missing-top, actor-fall and remaining-member controls pass; the unported AI occupant-removal warning remains explicit. |
| #157 | Private animation SFX with no selected bank underflowed an unsigned selector and indexed 32 GiB beyond its table. Unselected/optional-missing banks warn without unrelated fallback; invalid selection/IDs report errors before lookup. Valid routing/channel rotation and C diagnostic severity/tick/filter controls pass; audible parity is not claimed. |
| #158 | E222 was queued for missing model53, resolved to reserved sentinel1, then dereferenced a null bone matrix. The event command now validates reference kind and model ownership, then diagnoses/yields before changing playback state. Four valid actor/object controls (including a genuinely matching slot1) and six refused-binding controls pass. A separately red sentinel-collision control proves that a different model in slot1 cannot satisfy a missing model reference. The original 12/0 Minotaur spawn requires scene flag1==1 and story flag130==8: the no-write fixture does not meet these prerequisites. Its explicit engine-error failure is **not** completed cutscene acceptance. Nullable matrix consumers outside this producer remain a separate audit limit. |
| #159 | Original event28 unloads shared bank3 before a later scripted room handoff restores Ashley's idle pose. The outstanding vertex override then restarted through a null bank. The Game release boundary now preflights borrowers, undoes real vertex events while storage is live, retires event/cache bindings, and only then releases Content storage, retaining skeletal pose. Unwind/release failures propagate rather than clearing obligations or advancing the event. Standard/private/missing/empty streams, two owners, unrelated owners, atomic refusal, invalid IDs and timing queries after retirement are covered. |

Acceptance tests also caught two runner false positives (eight failed
assertions before repair): accepting a self-loop from a pending bit without
loading, and counting an interrupted control interval as settled observation.
Both now have direct lifecycle regressions. The full-disc inventory is
exhaustive; runtime batches are still being investigated and must retain every
blocked, failed and unrun record. This does not close #149's broader cutscene
inventory/playback/reference work.

Current local validation: Debug/Release full suites each **599 cases /
1,813,278 assertions**; hidden original-disc inventory **41,459 / one** each;
copier **44 tests**. Both Desktop configurations and Release Combat Lab build.
The broad maintained audit found 18 new findings in the new test/preview code;
typed pointer save/restore, same-object byte snapshots and bounded rendering/
assertion helpers repair them without baseline changes. Final affected-file
quality/tidy reports **131 existing / zero new**. Invalid zero door limits and
unknown selectors were checked with waited GUI-subsystem processes: both
return1 with explicit diagnostics and no modal windows. Runtime matrix
coverage remains incomplete and separately checkpointed.

#### Shared-bank lifetime checkpoint (#159)

The first strict-acceptance batch, `1792819672204643-0`, finished **87 passed /
46 blocked / two failed** records before crashing in active case136,
`z015-r010-d002` (15/10 to32/0); 64 of its 200 selected cases were still unrun.
Fault image SHA-256 and symbolized stack are preserved in
`build\quality\sequence159-original-symbols.log`. RVA215718 is F00998's null
header read, reached through a real event-script room change, not the queued
test request itself. Event28's bank-release opcode82 at142903 precedes the
crashing idle/vertex restoration. Original code has the same release-before-
relookup lifetime shape; native code must not depend on low-address reads.

The new retirement regression was red before repair. Its valid stream runs
actual animation opcode36 and verifies the applied vertex X11 returns to10
before detachment, while Y/Z/pad and the frozen bone pose remain unchanged.
Additional controls retain all borrowers if a later preflight fails, preserve
an unrelated current bank, unwind two actors, and make the EVM release yield
on failure. Missing lookup/entry/event cases preserve the outstanding override
and pose in both existing unwind callers. Native release and unwind APIs now
return explicit status; generated headers/callers are rebuilt together.

Current Debug/Release full suites pass **602 cases / 1,813,292 assertions**
each, with hidden disc inventory **41,459 / one** each. Focused animation,
sequence, door, scene and effect selections pass **437 / 32**; scoped maintained
quality/tidy is **131 existing / zero new**. These are local results, not
published CI or C++/compatibility retirement.

Fresh Debug replay `1792822646791332-0` re-enters the same door record and reaches
scripted room32/1 with **zero engine errors and completed teardown**, but uses
entry event0 rather than the failing history's event28. It therefore **does not
verify the old failure branch**. Its entry chain does not settle before3,600
ticks, so its outcome remains **blocked**, not passed. A state-aligned full
prefix/history replay is still required; do not force scene/story flags to
manufacture that state. The one-window run continues the remaining65-case
tail; keep its live checkpoint separate from prior-revision totals.

The full original prefix was then replayed unmodified in
`1792823796837457-0`: **87 passed / 47 blocked / two known failed**, normal
exit2, all136 selected cases finished. Crucially, case136 actually observes
event28/source1 in32/0, successfully releases bank3 at142903, passes the former
idle/vertex-unwind fault and reaches32/1 still in event28. It has **zero engine
errors and completed teardown**, but remains blocked because the longer scene
has not settled by3,600 ticks. This is state-aligned Debug crash acceptance,
not full scene/door acceptance. Exact command, frozen-image hash, scene tuples,
release records and outcome are in
`build\quality\sequence159-aligned-evidence.json`. No flags or Start skipping
were used.

Release original-prefix run1792829725153757-0 independently matches87 passed /
47 blocked / two known failed. Its target observes event28 in32/0, releases
bank3 at142903 and crosses the old fault into32/1 still in event28, with zero
target errors and complete teardown. The longer entry chain remains blocked,
not passed cutscene acceptance. Exact frozen image/command/scenes/results are
in `build\quality\sequence159-aligned-Release-evidence.json`.

The earlier65-case resumed tail stopped at its49th record after33 passed /
15 blocked, with16 unrun, at separately tracked #160 (23/4 ->24/15, E084).
PLG053's ring dereferences texture-info pointer0x74 at dataIndex+2. Original
MIPS confirms both texture-table producer and consumer base0x98, definition
stride204 and texture selector bytesCA/CB; do not guess a one-based index fix.
A separately confirmed native reset-width defect is repaired: the original
32-byte clear left the40-byte native state's last pointer uninitialized.
Poison/canary regression was7/8 then8/8; the existing C entry signature remains
tracked migration debt. Full suites now603/1,813,300 each, actual
particle-render/reset/lifetime selection94/12, copier49, scoped texture
fixture/tooling quality0/0.

That reset repair **does not by itself fix #160**: matching fresh Debug
run1792825963767984-0 still reaches E084/24/15/event0 and the same low-pointer
read. Missing persisted initialization diagnostics were not proof that they
did not execute: #161 exposed buffered warnings lost immediately before an AV.

Core text sinks and the shared Desktop/Lab tracer now flush Warn+ while
retaining ordinary-message buffering and Error-only error counters. The red
regression failed all four live-file checks before any frame, destructor or
script boundary; the corrected selection passes112 assertions/15 cases,
including a separate Core-only/no-observer check. Full Debug/Release suites
pass605 cases/1,813,310 assertions each; scoped warning quality is0/0.

Actual AV run1792827063353609-0 persists all four `effects.texture` records in
both complete JSONL streams and session text before RVA36F8F3/read0x76. Birth
already selects missing table1/program1, definition16, packed0x101E under
primary package84/count1; frame word0x0002001D produces info0x74. Primary
instance creation does not initialize the logged secondary-only effect field,
so its value56 does not demonstrate wrong package ownership.

Original PLG084 and PLG053 are byte-identical; original E084 definitions15-18
contain selectors1/1 and its renderer tracks select those same definitions.
The original binder populates only table0. The next discriminating observation
is the original running scene's table1 binding/frame consumption. This remains
an unresolved #160 gate, not justification for a fallback texture, zero-info
struct, changed index base, substituted renderer or success-shaped skipped
draw.

#### E265 textured-dispatch checkpoint (#162)

The next persistent tail1792830197070889-0 passed five records, then crashed
during z024-r000-d000's original24/0 source entry, leaving94 selected records
unrun. This was a different F01592 failure: program+cursor read0xFEDF in the
PLG265 particle-ribbon renderer, not #160's unavailable texture-info frame.

Original PLG265 MIPS shows its first, line-only dispatcher allocating0x10 and
its second, textured dispatcher allocating0x30 and initializing texture state.
The reference source leaves `VS_PARTICLE_UNTEXTURED` active between them.
The copier now applies a source-backed F02351 instance correction before
emitting bindings, rather than editing generated code or the reference.
The genuine line dispatcher requests its complete native prefix through
`offsetof`; current heap rounding already covered that24-byte prefix, so no
line-path overflow is claimed. The textured red control demonstrates actual
capacity24 instead of required64; the corrected allocation/texture-init,
ordinary textured renderer and genuine line controls pass.

Exact six-record histories pass6/6 in Debug1792830601020715-0 and
Release1792830661742875-0. Both retain604 original E265 active-effect frames
in24/0/event0, then arrive24/3, restore control and retain159 destination
observation ticks, with zero errors and complete teardown. Exact command,
image hashes and results are in `build\quality\particle162-actual-evidence.json`.
No flags, HP or scene completion were forced. This is bounded loader/entry/
control acceptance, not full cutscene or visual parity.

Final Debug/Release suites pass607 cases/1,813,347 assertions each. Copier52
tests and a stable second generation preserve1,015 selected native source/
project files plus manifest. Affected fixture/tooling quality/tidy is0/0;
the copier module retains23 existing/0 new findings, with no baseline changes.
All new repairs remain local/unpublished; #160 remains a separate open gate.

#### Orbit-return command extent checkpoint (#163)

The repaired E265 continuation1792830723227930-0 completed26 passed / one
blocked, then stopped at z024-r008-d003 (24/8 ->39/0), with72 records unrun.
The exception was C0000094 at native RVA1E5C0A in event interpolation. Single
edge1792830988376901-0 reproduced it. Durable camera-track diagnostics then
showed special operationB7/09 starting a zero-duration look-at track at1078.

The producer, not the divisor, was wrong: original orbit-return commands at
800E9C28/800E9C2C contain four and three bytes respectively, but G00130/G00131
were copied as separate scalar opcodes. The generated module loader could not
copy their missing easing/duration payloads. The copier/data ledger now retain
complete command objects and properly decayed array callers. Interpolation
math, event flags and original85-frame timing are unchanged; no denominator
clamp, forced endpoint or skipped event was added.

Native extent controls fail3/3 before repair. The corrected35-assertion/
two-case selection exercises the real registered module-loader entries from
synthetic source images, validates full payloads, then runs85 native camera
steps. Fixed Debug1792831568527363-0 and Release1792831616664973-0 both reach
the same39/0/event0/scene30 B7/09 branch at1078, log easing1/duration85,
restore control and pass with zero errors, no zero-duration warning and
complete teardown. Exact commands/image hashes/scene steps/results:
`build\quality\camera163-actual-evidence.json`.

Latest full Debug/Release suites pass609 cases/1,813,382 assertions each;
copier54, camera fixture/tooling quality/tidy0/0, second generation stable
across1,015 selected native source/project files plus manifest. Both gameplay
hosts build in both configurations. A fresh persistent100-case continuation
uses the same history; this is not yet all1,001-door or all-cutscene coverage.
The unrelated E084/#160 blocker remains open.

#### Puzzle HUD storage and authoritative clock (#164/#165)

Continuation1792831745328564-0 completed62 passed / nine blocked, then
crashed in active z028-r003-d000 (28/3 ->28/4), leaving28 records unrun.
F01384 wrote through null G00211 at native RVA238D12/address6. The ledger
already replaced that pointer with G00614, but the native renderer still used
the pointer and the backing text was const. Native rendering now owns and
directly uses a complete mutable nine-byte buffer, following the existing
Files-clock ownership pattern. No null-pointer fallback or hidden timer was
introduced; the unused compatibility declaration remains tracked debt.

Visible timer verification then exposed a different ownership defect, #165:
the original packed word G01934 is the time-field subobject of G01933, but
was separate native storage. The HUD now reads `const State_SceneTimer*`
from its sole production caller instead of a detached alias. No packed pointer
view or copied counter was added. A regression rejects generating such a view
from the ledger; the original subobject is recorded as provenance only.

The combined370-assertion/five-case selection checks writable storage, exact
digit strings and all eight glyph packets/coordinates/UVs/native links,
overflow formatting, separators/terminator/buffer guards, repeated hide/show,
authoritative counter ownership and real pause/reset behavior. The original
ordering offset is four **bytes**, independently checked against MIPS.
The ownership red control produced five failures/40 assertions before repair.

Final live Debug1792834573551820-0 and Release1792834651270755-0 both observe
the same28/4/event0/scene23 timer entry, execute164 timer renders and pass
with restored control in701 ticks, zero errors and complete teardown. Every
logged minute/second matches its typed source clock, from00:30:00 through
00:26:92; hundredths/frame bounds are checked too. Owned-window captures taken
only after a real `hud.timer` record visibly show the readable00:29 countdown.
No gameplay flags, HP, timer values or event completion were forced.

Exact commands, source revision/dirty state, executable and capture hashes,
validated source/text pairs, per-run capture paths and results are local in
`build\quality\timer165-actual-evidence.json`. Sequence-end `--capture` shows
the editor after teardown, not the live timer; it is not timer evidence.
This verifies the bounded entry/render/control case, not puzzle victory or
reference-pixel parity.

Full Debug/Release suites pass614 cases/1,813,753 assertions each; copier56
and stable second generation preserve1,015 selected native source/project
files plus manifest. Scoped timer/camera fixture/tooling quality/tidy is eight
existing/zero new, with no baseline edits. Both Desktop/Lab configurations
build. The persistent200-case continuation retains original history and
honest blocked/unrun outcomes; #160 remains explicitly excluded and open.

#### Original music source-data waveform aliases (#166)

The next200-case continuation1792834906085352-0 completed67 passed / ten
blocked, then stopped at active z028-r005-d002 (28/5 ->32/6), with122 records
unrun. Native auto-pan read noncanonical012600DB00960060. A frozen PDB/read-only
snapshot identifies primary music channel6, shape65/rate2/depth0, not a missing
waveform initialization or #126's pending depth-slide phase.

Original MUSIC172 channel6 actually contains `BC 02 41`. Original dispatch,
operand lengths and header-relative track offsets establish that boundary.
The byte selector is not limited to the sixteen named curves: original slot65
reads adjacent source pointer entries and refers to80018BD8. The original
reader treats those bytes as short sample/control data in a bounded16-state
cycle. Native import must not index past a16-entry array or sample host
executable functions.

The catalogue now covers the encoded unsigned-byte domain. An explicit sample
owner imports the original immutable resident payload as data, separate from
normal native symbol/function relocation. Signed loop jumps are validated
before publication.231 programs are bounded and cyclic;25 would leave the
defined source image and remain explicitly unavailable. Selecting one reports
an error before mutating modulation state, not fabricated samples or silently
disabled audio. LBU operands, cached retriggers, zero-depth phase and normal
disable/re-enable behavior are retained. No guest RAM execution or reference
code is included/linked.

Independent original-disc controls compare1024 actual native/source samples
for EACH of231 bounded programs. Synthetic data-role/code-label collisions,
ownership/stride/bounds/atomic publication, byte selectors, rate0/128/255,
phase/loop/disable/re-enable and invalid-selection refusal are covered.
Full Debug/Release suites pass622 cases/3,150,240 assertions each; focused
audio/data is1,507,745/85. Copier58 and stable second generation preserve1,016
selected native source/project files plus manifest. Scoped main quality is26
existing/zero new; headers0/0, with no baseline/suppression changes.

The original entry remains active beyond3,600 and7,200 ticks; those runs were
blocked, not successful completion. After confirming script-PC progress,
18,000-tick controls finish the same event0 -> event42 chain in7,832 ticks:
Debug1792841911523801-0 and Release1792841730609451-0, restored control,
zero errors and complete teardown. Each records1,248 actual note-ons and three
source65 binds to the immutable sample owner at original offset0x8BD8, with no
forced skip. Only actual waiting dialogue receives Circle; HP, flags, timers
and event completion are untouched. Exact commands, executable hashes,
source/data-owner correlation and results:
`build\quality\music166-actual-evidence.json`.

This verifies the original long entry/music path, not complete reference PCM
parity or undefined source-memory behavior. Current persistent continuation
uses an explicit9,000-tick budget and retains blocked/unrun checkpoints.
All new source remains local/unpublished; publication/CI gates stay open.

### Startup feedback and timing

The Lab presents a readable loading screen before catalog scanning, audio/
controller discovery and Git provenance collection. These stages stay on the
owning thread; no background worker accesses legacy gameplay state. Scripted
runs do not initialize or open physical gamepads. Ordinary interactive runs
still initialize audio and controllers after the first loading presentation.

`startup.stage`, `startup.first_present` and `startup.interactive_present`
records include monotonic `elapsed_us` timestamps. They distinguish the first
visible loading frame from the editor/encounter presentation and identify slow
device, catalog or renderer initialization. A loading screen does not turn a
failed initialization into success; errors remain logged and abort startup.

### Sequence acceptance checkpoint (#143, 2026-10-23)

Debug run `1792780995004927-0` used one process, one visible HWND `66322990`,
one renderer/audio lifetime and one catalog for four scripts:

| Entry | Budget | Observed ready frames | Total ticks including teardown |
|---|---:|---:|---:|
| Solo, all-items A, HP222/MP333 | 600 | 566 | 612 |
| Original Lich24/7, living-enemy stop | 900 | 858 | 911 |
| Repeat solo with fresh state | 600 | 566 | 612 |
| Repeat Lich with fresh state | 900 | 858 | 911 |

Both solo entries deliver Triangle at script tick200 and Cross at300, despite
different host ticks. Their initial resources, positions, models, inventory
and learned actions match exactly. Both Lich entries likewise match initial
state, retain a living enemy at the stop, produce six actual combat-result
records and complete normal teardown. All four results are initialized/
completed, with zero engine errors and complete, separate session traces.
No live HP, event flags or process memory were edited.

Release run `1792781463758439-0` uses actual F10 input: the current entry
is cancelled at188 ticks with normal teardown and zero engine errors, the
second entry stays `not_run`, and the host returns2. Ordinary controller
discovery still connects the existing DualSense; scripted mode alone omits
physical gamepad initialization. The observer records one HWND `96865202`.

The local roster driver now generates one sequence and launches one native
process, rather than one per case. A no-window mock verifies three selected
cases cause exactly one launch and retain three correctly addressed results.
The owner-stopped all-definition corpus was **not** restarted.

Validation uses Debug/Release x64, source `5a2d0866` plus the scoped sequence/
platform changes and separately uncommitted Score/perception work. Full
worktree suites pass556 cases /1804672 assertions each; the focused
sequence/startup selection and every direct application-header consumer
pass the scoped quality check with zero new findings. The Desktop consumer
also builds without opening it. No quality baseline is refreshed.

Local evidence: `build\quality\lab-sequence-*-observation.json`,
`lab-sequence-{Debug,Release}-full.log`, `lab-sequence-quality-final.log`,
and `lab-sequence-source-audit.json`. The four-entry and cancellation manifests
and presets remain under `build\quality`. The same live runs measure readable
loading presentation at414ms Debug /373ms Release, before deferred device/
provenance work. The owner's entire ten-second delay is not claimed reproduced;
further startup investigation is deprioritized as requested. Publication and
exact-revision CI are recorded on #143; wider gameplay coverage remains #16.

### Original room-transition geometry repair (#145, 2026-10-23)

The owner-reported Desktop crash is reproduced through ordinary controller
movement on the same production room-loading path. Original SCEN001.ARM names
and embedded MPD tags identify **Worker's Breakroom** as zone9/room16
(`MAP027.MPD`) and **Hall of Struggle** as zone9/room3 (`MAP012.MPD`).
Scene room numbers are one-based; native room indices are zero-based.

Pre-fix Release run `1792784893231810-0`, host SHA256
`e87eaef5e0b2a1b2929e8834a2fcd2f075b0b01fd8527a895323ab684ff3cf32`,
crashes at254 after walking from Worker tile3,9 through its original door3,10.
The recovered stack matches the owner: UV offsetting, gimmick creation, actor
asset queue, auxiliary actor creation and normal room loading.

F00786 previously added a truncated host pointer to four serialized SHP
section offsets. Its consumer then used `VS_ONDISC`, adding the base again.
The native helper is now **Content_CopyModelGeometry**: geometry copies retain
their relative offsets. F00784's flat/Gouraud triangle/quad byte strides,
unsigned UV wrapping and original spread of UV changes are unchanged.
Verified parameter/local names survive the copy generator; the original
decomp symbol and source location remain in provenance. Naming output
`BATTLE_06` and `FUNCTION_CATALOG` agree, so regeneration cannot undo the name.

The coupled gimmick clear/shared-copy now uses the owned prefix boundary,
`offsetof(Field_GimmickInstance, reserved178)`. Compiled evidence is380 bytes,
inside the384 bytes owned after `Actors_ActorCore.model`; the entire6320-byte
actor-compatibility view is **not** cleared or allocated. A static assertion
guards that relationship.

Actual fixed routes in one window per configuration:

| Configuration / run | Worker -> Hall | Hall -> Worker | Worker -> Hall again |
|---|---:|---:|---:|
| Debug `1792785151378647-0` | 318 | 543 | 765 |
| Release `1792785174815286-0` | 316 | 543 | 765 |

Both target-room loads use the original spawn records, not fabricated actors.
Read-only, compiled-layout-guarded observations while F6-paused verify the two
kind4 gimmicks retain source offsets `[356,96,100,164]`, with the same source
model size356 on both entries. The sound section legitimately starts at the
end of an empty section; it is not clamped into the model. No runtime memory,
HP, door flags or terrain guards were edited. The initial solo setup isolates
the approach; after entering the unconfigured Hall the production loader uses
the original room actors. Original room event scripts remain isolated in Lab;
this is not a full story-event or every-gimmick acceptance claim.

Focused model-copy/header/guard and exact UV-format controls pass; full
Debug/Release worktree suites558 cases /1804686 assertions each pass. Copier
tests, second-generation stability, naming batch validation and the refreshed
full reference index pass:138 translation units,100172 identifier occurrences,
zero unresolved and zero text mismatches. The wider #148 symbol sweep remains
open; compiling or renaming does not imply C++ promotion or compatibility
retirement.

Local evidence: `build\quality\gimmick145-{Debug,Release}-live.json`,
`gimmick145-*-tests.log`, `gimmick145-*-full.log`,
`gimmick145-reference-index.log`, `gimmick145-generation-*.log`, and
`gimmick145-live.ps1`. Disc data and captures stay local. Publication and exact
revision CI are recorded on #145. #146/#147's D'tok reports are separate.

### Hall of Struggle Bat encounter crash (#150, 2026-10-23)

The owner's actual Desktop session
`build\Debug\game-logs\1792803538655350-0` ends at72233 in zone9/room3,
175 ticks after entry from Worker's Breakroom. The Bat is model41/slot2,
37HP and targeting Ashley. There is no persisted exception or normal shutdown
record. This is distinct from #77's flight-height abort and #145's load-time
geometry repair.

A bounded native-debugger replay reproduces the crash through the actual
Worker-to-Hall door, using the original Hall actor/gimmick spawns. A separately
configured Bat control survives real attacks to Ashley's defeat, so an isolated
enemy fixture alone does not cover this failure.

The fault is **F00887 / Actors_ActorBoneEndpointPosition**, native Debug
RVA`0x29D8A5` before repair: reading address`0x68` while resolving slot3.
That slot has a gimmick, not an `Actors_actorRecords` entry. The copied helper
reads `actor->instance.model` before checking for null and selecting the
existing gimmick fallback. Original `BATTLE.PRG` instructions at`800A1C50`
perform the early low-address read; `800A1C54` and`800A1C68..800A1C78` then
resolve the fallback. The unused low-address read is not a valid native access.

The copy-generator adaptation now reads the model only after resolving a
present actor or gimmick. Missing owners retain the original `-1` result;
actor priority and origin/tip/midpoint transforms are unchanged. No enemy
filter, terrain guard, HP, RNG or event flag is altered. This is a bounded
behavior repair, not C++ promotion or compatibility retirement.

Fresh local verification on `508b4bd79d58e37421fdd9c693c92937f2b5de0e` plus
uncommitted logging/repair and preserved owner edits:

- Before repair, direct owner regression: **SIGSEGV on gimmick origin after
  three passing actor controls**. After repair,24 owner/endpoint/re-entry
  controls pass, including absent owners without output writes.
- Debug and Release Desktop, Lab and test builds pass. Full suites:
  **571 cases /1812885 assertions each**. Copier: **37 tests pass**;
  second generation retains manifest SHA256
  `c013d67cabd96c4e85289e6a623b7aefab616b365066542fc09cc61687829cd2`.
  Scoped format/cpplint/ruff/clones/tidy: **131 existing /0 new**.
- One persistent Lab window per configuration runs two fresh Worker-to-Hall
  encounters under the native debugger. Each script has2000 logical ticks
  plus12 teardown ticks,1748 ready Hall frames,50 distinct Bat positions,
  actual attacks reducing Ashley250->103HP,0 engine errors and completed
  teardown. No fault is reported on any of the four entries.
- Debug run`1792805870299289-0`; Release`1792805931587671-0`.
  The fixture starts solo at Worker tile3,9 with the unmodified post-prologue
  profile. Scripted Up is held at180..229 and340..439; crossing the original
  door loads the original Hall actors. Sequence inputs and summaries:
  `build\quality\bat150-sequence.json` and
  `build\quality\bat150-fixed-live-evidence.json`. This verifies the faulting
  production room/actor path and reset, not full story-event/prologue coverage.

The missing exception record is reproducible under a debugger:
[Windows only invokes the top-level exception filter when not debugged](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter).
A separate no-window intentional fault probe, using the production trace and
crash handler without a debugger, persists the exception and eight stack
records in both text/JSONL. Local evidence:
`build\quality\bat150-crash-probe\1792806144620444-0`.
The owner's exact debugger/termination path remains unconfirmed; no logging
workaround or speculative first-chance exception interception was added.

Other evidence: `build\quality\bat150-debugger-{worker,addresses}.log`,
`bat150-red-test.log`, `bat150-{Debug,Release}-full.log`,
`bat150-quality-final.log` and `bat150-copier-tests.log`. The fix is local and
uncommitted; #150 remains open pending authorized publication and exact CI.

### Short authored AI direction tails (#169)

The first-lever-room owner crash is an AI-map validation abort, not an
established sfizz fault. Clean Desktop6de0a140, run1792855828990222-0, follows
original9/11 event4 into scripted19/0 (MAP212): load51834, abort51835.
Original MAP212 and MAP214 contain308 direction bytes after the0x498-byte
auxiliary header despite32x32 and13x25 collision dimensions. Original MIPS
initialization does not consume a complete direction grid at this boundary.

The native builder now validates the actual fixed header. Direction consumers
share checked, mutable native cells bounded by the real source tail; missing
bytes error/refuse explicitly, without fabricated padding or a success-shaped
direction fallback. Route, wander and temporary height-filter accesses are
rewritten through strict copier adaptations. Existing ordinary-ground behavior,
temporary-byte restoration and native frame cleanup remain unchanged.

Red shape regressions fail2/2. Pre-fix production run1792858131205962-0
loads MAP212 at40 and aborts41. Fixed Debug1792858558711300-0 and
Release1792858778063531-0 each observe1170 ready frames in original19/0,
repeat19/0 and companion21/0, zero errors and complete teardown. These are
direct original-room loader controls, not completed cutscenes: control remains3.
The ordinary9/11 control remains safe. A Circle interaction attempt does not
start event4, and two Up exit attempts1792858944213004-0 stay9/11 under
the post-prologue profile. Neither is claimed as the owner's actual transition.
The exact legitimate story/event context was still required at that checkpoint;
the later authored-input regression below now establishes it.

Both full suites pass624 cases/3,150,286 assertions. Copier60, stable
1,016 native source/template/project files plus manifest, and focused source
tidy zero findings/zero new pass. Controls cover both short-tail shapes, actual
mutable source pointers, last available/first missing bytes, negative/edge/null
queries, unchanged output pointers on refusal and adjacent canaries. The older
one-byte tail-shortening check now permits load, while missing-cell access
remains explicitly rejected; malformed fixed headers still fail.
Evidence: `build\quality\room169-actual-evidence.json` and `room169-*.log`.
No flags, HP, scene completion or resource data were patched.

#### Authored first-lever handoff and reentry (2026-10-25)

The maintained [room preset](../tools/test/scenarios/first_lever_room.json)
retains original production actors/events. Its existing crossbow setup is a
controlled equipment precondition, not an HP/range/progression override. Actual
action1 defeats bat4 HP41->0; wolf3 remains alive at76HP. Real sheathing, facing
and Cross open the original **Pull Lever** selector, and Circle accepts it.
The natural upper floor is-512; attempting interaction from the lower floor
would violate the original +/-128 vertical limit.

Original MAP020's embedded event4 requires local flag48==1, Ashley tile3,13 and
not-busy/grounded state. The flag comes from the original lever script. Earlier
routes reached the tile after the automatic closing event had cleared it and
are explicitly failed controls. Removing the bat's actual interference through
combat allows the recorded movement to reach the original timed trigger.
No script flag, live position, HP or completion state is written by the replay.

Run the two-entry [sequence](../tools/test/scenarios/first_lever_handoff.json)
and then its [semantic checker](../tools/test/verify_first_lever.py):

```powershell
.\build\Debug\VS.CombatLab.exe --data Native\data `
    --sequence tools\test\scenarios\first_lever_handoff.json --speed 1 --scripted
.\.venv-3\Scripts\python.exe tools\test\verify_first_lever.py `
    build\Debug\combat-lab\runs\<run-id> --revision <full-source-revision>
```

Use the Release paths for the other configuration. Each6000-tick case begins
a fresh world through normal teardown in the same window. Process completion
alone is not acceptance: the checker requires the real bat attack, unchanged
living wolf, source event4 at its actual tile, the catalog's32x32 MAP212
destination, natural9/12 progression and at least120 consecutive ordinary
living/control frames, with no engine errors or forced skip.

Debug1792915518582016-0 / Release1792915856293931-0 verify both entries.
Event4 starts1970/7982; MAP212 is ready2191/8203; normal9/12 control returns
4666/10678, with consecutive intervals126 frames each. Both cases have complete
teardown and zero errors. The first case physically reads MAP212 at2190; the
second correctly reuses cached asset data. The checker records that distinction
instead of requiring a redundant physical read. It rejects a clean-exit trace
that never performs the handoff.

Core bounds fix is f1bba2b0; this acceptance uses current native c470fd26 plus
the scoped test files and unrelated owner ignore/solution edits. Immutable
reports are `build\quality\lever169-sequence-*-verified.json`; captures/assets
stay local. This establishes the reported scripted loader boundary and repeat
entry, not every campaign door, all cutscene fidelity, or compatibility retirement.

### Owner pause: door coverage limits and next failure

Source fixes are committed in `7b6fb81e` and published through the
non-rewriting integration `309f8c78`. Exact
[Source Quality run40104975781](https://github.com/JEschete/VS-NativePort/actions/runs/40104975781)
passes. The stopping-checkpoint full suites pass622 cases/3,150,240 assertions
in each configuration; copier59 and full quality195 existing/zero new.
Verified defect tickets #150, #152, #153, #154, #155, #157, #158, #159,
#161, #162, #163, #164, #165 and #166 are closed with individual evidence.
#156 remains open: both original-prefix observations restore control at3572
but end before120 settled ticks, so its repair is not labeled complete door
acceptance. #151, #160, #167, the remaining #148 sweep and #149 cutscene work
remain open. Independent owner edits are not included in these commits.

The Debug binary through #166 (SHA256
`ACFD8139CAF681DECA9F508716F107A7BF817EB9945A0F1C405A031DF9005A07`)
completed199 of200 selected records in run1792842259573600-0:
161 passed/38 blocked/one interrupted when its outer two-hour debugger budget
expired. Every completed row has zero engine errors and complete teardown.
This is a harness budget, not a native exception. The known long32/6 entry
completes in7,832 ticks; the observation budget was explicitly9,000 per case.

Continuation1792849529393328-0 selected the remaining541 records with the
ordinary3,600-tick case budget. It completed94 passed/37 blocked, then failed
at script132 `z048-r006-d000`;409 remain unrun. The original event follows
its authored changes into32/17/event0/source1/phase4/scene30. At167127 the
renderer reports a truncated0x34 command in a56-word packet at offset53,
then exits with unhandled C++ exception E06D7363. This is tracked in #167;
a relationship to #26 is not established. Standalone, two-record and
eleven-record controls do not reproduce it and remain blocked observations.
No renderer skip, default packet or product fix was introduced.

The owner paused the exact-history diagnostic rerun1792852943838401-0 before
it reached that scene:32 passed/7 blocked/one cancelled/501 unrun, graceful
exit2. All owned replay windows are stopped. These cohorts are not a fresh
complete1,001-door acceptance run; #151 and #160 remain open.

Two later naming-only batches verify34 bindings in owned flight-terrain
queries and generated geometry-group commands. Exact source/header comparisons
preserve function types, constants, operators, bounds and behavior. Both
Debug/Release combined room-render/ground/flight controls pass155 assertions/
17 cases; copier59 passes and1,016 native source/template/project files plus
the manifest are stable on repeated generation. The frozen runtime image
predates those names. Current source, naming, behavior, promotion and
compatibility-retirement gates remain distinct.

### Score submenu and encyclopedia preview repair (#65, 2026-10-23)

Ordinary controller navigation reproduces the TITLE stack-buffer failure;
actually defeating the original Zombie then opening its unlocked encyclopedia
entry reproduces a second crash in actor placement. No entries are unlocked by
editing flags, HP or inventory. The fixes are generator-owned, with original
MENU9 source provenance retained:

- F02816/F02817 reserve the terminator for two-/three-digit numbers. F02820
  retains the complete fourteen-byte `NO.   000/000` header.
- Digit/banner renderers use typed native frame cursors and ordering pointers,
  not PS1 scratch-word offsets into widened pointers. F02799 retains full-width
  primitive handles. Its regression counts the original reachable packets,
  rather than accepting merely valid but disconnected links.
- F02839 allocates either two native actor records or one actor plus two
  equipment records. F02840 starts with a zero-initialized request and matching
  `sizeof`-based destinations. Original request kinds, target slots, disc-queue
  ownership and preview variant selection are unchanged. There is no null
  platform fallback: indeterminate placement bits are fixed at the producer.

The request regression fails five of twenty assertions before the producer
fix: arbitrary placement bits, two actor-boundary errors and two shield-boundary
errors across ordinary/companion cases. Native actor/equipment strides are
6440/1528, replacing serialized 6400/1512 assumptions. Preview allocations are
9496/12880 instead of9424/12800. The final Score selection passes2200 assertions
in five cases, including the request, complete headers, packet reachability and
native banner cursor controls.

Fresh live runs use **one window per configuration** for all four actual
submenus: TITLE, GAZETTE, RECORD TIME and ENCYCLOPAEDIA. Each enters twice,
scrolls up/down across its boundary and returns through the original controls.
Two real Fireball casts defeat the original zone9/actor2 Zombie; its genuine
unlock then permits two viewer entries with L2 rotation/R2 zoom inputs.

| Configuration / run | First viewer | Re-entered viewer | Engine errors |
|---|---:|---:|---:|
| Debug `1792788396368047-0` | 4557 | 5091 | 0 |
| Release `1792788491009214-0` | 4564 | 5101 | 0 |

Captures show the loaded original Zombie, readable complete identification and
original rotation/zoom prompts. Both traces end without trace failure; closing
the host while the second viewer remains open is recorded as encounter
interruption, not an F10/encounter-teardown acceptance claim. This does not
verify every encyclopedia model or the unrelated D'tok reports.

Validation: Debug/Release x64 full worktree559 cases /1804706 assertions each;
copier32 tests; two stable generation passes; focused maintained quality0 new.
Evidence source is local parent971c21dd plus these Score changes and separately
uncommitted AI field-name/perception work. No C++ promotion or compatibility
retirement is inferred. Binary SHA256:

- Debug `e50a77154db23f764696206419b258ead8804744fe461a99c691ed6aac3eacf0`.
- Release `30c8a7f4b632083cb2536f27246c60cac0c83d6bb5c5b7ac96670716fbee2aaa`.

Commands: build Tests/CombatLab/Desktop for Debug and Release x64 with the
standard MSBuild settings; `VS.Tests.exe "[menu][score]"`, then the full suites;
`python -m unittest discover -s tools\port -p test_encounter_fixups.py`;
`python tools\port\copy_to_native.py` twice. Local controller driver:
`build\quality\score65-live.ps1 -Configuration Debug -Row 3 -Scroll -Viewer -AllRows`
(repeat with Release). JSON observations, traces and captures remain local in
`build\quality\score65-*` and the named Combat Lab run directories. Scoped
publication and exact-revision CI are recorded on #65 before closure.

### Fire Breath script-register repair (#146, bounded, 2026-10-23)

The full original prologue reproduces D'tok's action148 announcements without
any damage results. A separate source-backed zone6 Wyvern fixture isolates the
same model58 / `3ASP00.SEQ` / `E095.P` production path. Actual F6/F7 controls
and compiled-layout-guarded **read-only** observations establish that the
91-frame special animation already advances, including timed events30/64/90.
Animation255 is the valid `0xFF` special-bank selector, not a missing animation.
Sixty-one distinct pose, blended-pose and rendered bone-matrix hashes rule out
a frozen-pose workaround. The native-pointer output repair in #64 is retained.

The broken producer is F01531's combined operand reads:
`registers[ReadByte(instance)] = ReadHalfword(instance)`. Native assignment
evaluation reads the halfword first. The original MIPS explicitly reads the
index first at800D4AFC, the halfword at800D4B08, then stores one byte at800D4B18.
E095's three entries should set register1 to0/1/2; instead they all leave it0,
so every stage loads the small first timeline. F01532's companion comparison
now also uses explicit ordered operands and the original unsigned-byte load.
The instance/registerIndex/value names are verified, generator-owned and
documented without changing signatures or serialized layouts.

Do **not** relax F01540's apparently suspicious `id < count` remap. Original
800D4E28/800D4E2C confirms the strict bound. E095's IDs `[1,2,5,0]` and five
timelines intentionally use raw register2 when the final remap is excluded.
Changing the bound would select the wrong stage. No effect IDs, timing,
animation events, targeting, HP or script flags are forced.

Direct native register/branch regressions cover all four registers, byte
truncation, comparison operands and taken/untaken branches. Before the fix:
46 failed /248 assertions; after:248 pass in two cases. Copier33 tests pass;
two generation passes are stable. Full Debug/Release suites561 cases /
1804954 assertions each pass; complete maintained quality has498 existing /
0 new findings, without a baseline update.

After the fix, Release Lab run `1792791081079977-0` loads timeline0/1/2 with
1/9/10 tracks at990/1118/1170. Action148 produces five original limb results
at1190:11/10/10/0/9 HP, with the zero result retaining impact6. Ashley's
observed HP falls997 ->957. The corresponding capture shows the real fiery
impact and damage numbers, not an injected result.

Fresh full-prologue controller replays, one window per configuration, use the
retained839-input schedule with **only Start1000**, not the story skip:

| Configuration | Ticks / captures | Complete breath actions / limb results | Exit / errors |
|---|---:|---:|---|
| Debug | 48000 /22 | 14 /70 | 0 /0 |
| Release | 48000 /22 | 10 /50 | 0 /0 |

First real results occur36992 Debug /38712 Release. Capture/loader timing
differs; action counts are observations, not an asserted parity threshold.
Release capture45900 shows GAME OVER and47900 shows the original Continue
path driven by the retained inputs. Restored HP after Continue is not a live
memory edit. Neither replay defeats the boss or verifies #147's death fade.
Source is parenta04a048d plus this dirty scoped repair and separate uncommitted
perception tests/owner documentation. Desktop binary SHA256:

- Debug `c781a3b69ef2aed2fc988236c4c063241956b30141f79c31e72be311187c0601`.
- Release `da45a8287f5b0598c61b25af658739c068dd8bf8362a5e9e7008ee997d9dbd99`.

Local evidence: `build\quality\dtok146-fixed-debug` and
`build\quality\dtok146-fixed-release`, their arguments/results/logs/captures, and
`dtok146-full-prologue-evidence.json`. Original source-order disassembly is
local in `dtok146-original-script-assembly.txt`; no disc bytes or images are
committed. The focused observer and preset are `dtok146-live.ps1/.json`;
the stepped pose report is `dtok146-pose-Release.json`.

An additional Debug visual observer exited normally during startup with
zero gameplay ticks and no logged error; it is excluded, not a passing check.
After the owner requested autonomous completion, Release live run
`1792793560646680-0` directly captured the active flame jet leaving the
original model's mouth. The native320x240 framebuffer and DPI-aware,
owned-window-only capture agree at special-pose frame~76, after the original
frame64 trigger. No desktop coordinates, injected effect events or damage
writes are needed. Evidence: `build\quality\dtok146-active-native.png`,
`dtok-Release-active-flame.png`, `dtok146-visual-pose.json` and the named run.
This completes the active-stream gate in addition to the full-prologue
Debug/Release damage proof. The unsafe appearance-tail/material contracts
found during #147 investigation are separate. Publication and exact CI are
recorded on the issues, not inferred from local runs.

### Original death-fade ownership and material repair (#147, 2026-10-23)

A real Fireball defeats the original zone6 Wyvern/model58 in the live Lab.
Pre-fix run `1792793719462282-0` reproduces the solid silhouette: appearance
states1/2/3/4/5 advance, but the actor's actual fade bytes remain zero while
the old compatibility-view byte increments. This is not a manufactured death
or a claim that another successful process exit proves the presentation.

F00980 is now registered owned **C**, with separate typed actor/companion and
gimmick entry points. Actors use their own fade bytes and selected palette rows:
the first160 entries of `basePalette` are the reference, the second160 are the
transition row. `currentPalette` is a separate pulse buffer. The former generic
gimmick view addresses actor fadeGreen/fadeRed24 bytes early and palette rows at
the wrong native locations. Original palette modes, state order, equipment
uploads, motion helpers and carried-object release behavior are retained.
F01008 calls the typed entries directly rather than hiding mismatches in
void-function casts.

The coupled canonical gimmick pulse field now agrees with existing ledger
rows T00255.072/T00469.006: sixteen16-bit CLUT colors, not thirty-two8-bit
elements. Its32-byte span/native offset336 and owned-prefix bounds do not grow.
Both the actor16/160-color cases and the separate gimmick path have guard
regressions. This does not retire every hardware-shaped gimmick view or
promote the implementation to C++.

Correct state ownership alone did **not** fix the image. The native renderer
also omitted original appearance-material dispatch. Source-binary evidence:

- 800B3680..800B38E0 selects override drawing for states4/5/7/8/15, with
  palette-pulse overrides retaining their original behavior.
- 800B51DC..800B51FC chooses subtractive page mode0x40 for nonzero states<7,
  additive0x20 otherwise. Polygon semi-transparency is enabled, including
  both adaptive split orientations.
- 800B5DD4..800B5DE0 repeats the supplied fade tint at every Gouraud vertex;
  retaining the original per-vertex colors incorrectly leaves a solid shape.
  Ordinary colored faces and companion mode1 retain their normal path.

No arbitrary alpha curve replaces this behavior. Exact packet controls cover
all16 appearance states with pulse/no-pulse, flat/Gouraud and both adaptive
splits. Missing blend dispatch is red80/128; missing uniform Gouraud tint is
independently red20/128. The owning actor byte/whole-record guard is red2/2.
Final combined appearance selection passes7878 assertions in four cases.

Fresh controller defeats and F10 cleanup/re-entry use **one window per
configuration**, not separate hosts for every observation:

| Configuration / run | Lethal Fireball result / first entry | Second entry | Normal cleanup |
|---|---:|---:|---:|
| Release `1792797118662852-0` | 1868 /94HP | 4167 /93HP | 2310 /4609 |
| Debug `1792797363044415-0` | 1866 /96HP | 4162 /93HP | 2306 /4604 |

All four genuine defeats pass through states1..5; actual fadeRed advances0..64,
the old-view tail stays untouched, and per-run captures show the floor becoming
visible through the source-defined subtractive silhouette before disappearance.
Every re-entry restores Ashley999HP/999MP and the original enemy at full rolled
HP. Its observed86HP is valid: original146C.c:6128 applies base85 plus an
averaged[-10,+10] roll. Catalog85 is not a valid exact runtime-HP assertion.
No HP, MP, RNG, position or event memory is edited after setup. Both sessions
have two normal encounter ends, one window and zero engine/trace errors.

Full Debug/Release suites pass565 cases /1812832 assertions each; copier35
tests and repeat-generation/owned-file preservation pass. Final owned appearance
quality is0 findings; the related scoped checks introduce no baseline changes.
The original-reference index and exact published CI are recorded with #147.
The source includes the scoped appearance/material repair on parent7a33fe33
and separately dirty owner documentation/perception tests. Actor palettes,
opaque and appearance packet controls remain independently verified.

Local evidence: `build\quality\dtok147-final-evidence.json`,
`dtok147-repeat-{Release,Debug}-pose.json`, the named run directories and their
`captures\entry-0` / `captures\entry-1` images, original disassembly in
`dtok147-material-*.txt` / `dtok147-override-*.txt`, and
`dtok147-{red,material-red,material-color-red}-*.log`. Source-derived images and
binary data stay local. This verifies the production model's defeat/fade path,
not completion of prologue story-event victory or all remaining rendering debt.

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

## Equipment-menu shield-model repair (#130; parent #14)

On source revision `e4d006694a188e996d66c390e2cb1ca23dd140a5`, the first-entry
Fandango preview is visible in the real Items -> Equip -> Weapon route.
Debug solo all-spells ZONE012/room0 run `1792724245959463-0` uses
`--start --scripted --speed 1 --frames 1700` and
`--press 800:Triangle,920:Down,1040:Circle,1250:Circle,1460:Circle`.
The local capture is `build\quality\equip14-first.png`. This does not establish
every preview transition or close #14.

A separate production-entry regression exposed a remaining model alias:
F02637 (`MENU3/68C4.c:12`) tested the independent `Battle_unusedGlobal2` integer
instead of the original preview shield pointer at `vs_battle_wepModels[3]`.
G00410's original address0x800F4594 is slot3 of the table at0x800F4588.
Consequently a two-handed selection could leave the previous source shield
and its texture reference loaded even though the equipment state unequips it.
The copier now reads `Actors_weaponModels[3]`; the independent declaration
remains inert compatibility debt, not a retired bridge.

`[menu][equipment][reload]` covers10 weapon categories, preview present/absent
and detached placeholder zero/nonzero:40 combinations and240 assertions.
Before repair it fails20 assertions (exit42); afterward the source-model
release, untouched preview ownership, texture reference count, one-handed
and bare-hands controls all pass. The test isolates reload phase0 with no
queued model/animation load; it is not a full live equipment swap.

Validation of this scoped change, with unrelated owner edits preserved:

- Build `Native\Tests\VS.Tests.vcxproj` and
  `Native\Tools\CombatLab\VS.CombatLab.vcxproj` in Debug and Release using the
  documented MSBuild command with `/nodeReuse:false` and
  `/p:VcpkgManifestInstall=false`.
- Each configuration: `VS.Tests.exe "[menu][equipment]"` passes5 cases/
  288 assertions; the full executable passes499 cases/1,785,011 assertions.
- `.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py` passes18
  regressions, including the canonical shield-slot check.
- A second `copy_to_native.py` run is byte-stable across751 generated outputs
  and preserves all86 existing unique owned paths enumerated from `owned.csv`.
- With the retained environment active,
  `quality-native.cmd --files Native\Tests\src\EquipmentDetailTests.cpp tools\port\copy_fixups.py tools\port\test_encounter_fixups.py --tidy`
  reports zero findings and zero new findings; no baseline changes.

Logs are local under `build\quality\equipment130-*`; the red regression uses
`equipment14-red-*`. The attempted live greatsword/shield cycle only reached
the unselectable NONE row: the stock New Game bag has no shield. Its
`equipment14-Debug-greatsword-shield-cycle-*` observations are an unsuccessful
setup attempt, not acceptance. A real shield acquisition followed by menu
swaps, broader equipment categories/properties, cancellation and restrictions
remain required for #14.

Accelerated runs also reconfirmed existing #83: speed8 aborts the audio queue
at tick802 (run `1792724088133050-0`), or tick800 with `--scripted`
(`1792724143538041-0`). Scripted input does not disable audio. The same initial
menu route at speed1 succeeds (`1792724167571071-0`); equipment work uses
speed1. No accelerated-audio repair or new #7 acceptance is claimed.

## Native equipment storage repair (#131; parent #14)

On `1837e1363a68344fcc51c9369741cee5df1a1e5d`, real-controller Debug run
`1792725454239918-0` acquired the original Zombie Knight shield132 through
Fireball and ordinary loot pickup. Equipping it beside Fandango worked, but
unequipping/re-equipping Fandango changed source and preview shield headers
to model0/material0 while the loadout still specified model101.

T00633 retained the original1512-byte, alignment1 runtime reserve.
`Actors_WeaponModel` is1528 bytes, alignment8 on native x64, so its full-size
clear/copy overwrote16 bytes of the next slot. The new registered owned C
`Actors_ActorModelSlot` contains the actual weapon model. Native allocation
size, array stride and alignment now follow that type; original source size
remains provenance, not an on-disk or native-storage contract. Secondary-body
allocation remains separately sized for an actor record. This is not C++
promotion, compatibility retirement or a new root-cause claim for #82.

The pre-fix regression fails both size/alignment checks without performing an
out-of-bounds write. Final tests cover each slot and clear/copy path with a
byte-exact expected allocation, including adjacent storage, padding and guards.
C assertions also cover capacity, alignment and the secondary-body start.

Final hosts, based on1837e136 plus this scoped repair:

- Debug SHA256 `7268bd6bc2abd3f2cade7168444c2642d9183b9876004005228cc1164f84a89d`.
- Release SHA256 `f7f6195b87a0af0a1f8e1e4d3b357acc8d6541d9688717050a0a93329b5458f2`.

| Acceptance | Debug run | Release run |
|---|---|---|
| Fandango/shield, two repeated swaps | `1792726450701178-0` | `1792726859832488-0` |
| All seven equipped stock slots, cancel/NONE/round-trip | `1792726969363175-0` | `1792727088894880-0` |
| Greatsword/shield, two repeated exclusive swaps | `1792729029518758-0` | `1792729093152495-0` |
| Ghost secondary body, three live-enemy exits/two re-entries | `1792727583786915-0` | `1792727598752178-0` |

The causal one-handed replay retains shield model101/material5/texture1 in
source and preview after each weapon reload; source slot stride is1528.
Field return retains source models0/1 and releases previews2/3. Greatsword
selection removes the shield, shield selection removes the greatsword, and
field return retains only the greatsword source model. Stock cycles preserve
component properties and ownership, reject NONE, and restore the original
loadout at tick6657. All eight sessions close normally without error-level
engine records or trace failures.

Live commands use the ignored `build\quality\equipment14-check.ps1`, speed1:

```powershell
build\quality\equipment14-check.ps1 -Configuration Debug `
  -Weapon default -Mode loot-shield -Seed 5
build\quality\equipment14-check.ps1 -Configuration Release `
  -Weapon default -Mode loot-shield -Seed 10
build\quality\equipment14-check.ps1 -Configuration Debug -Mode stock-cycle
build\quality\equipment14-check.ps1 -Configuration Release -Mode stock-cycle
build\quality\equipment14-check.ps1 -Configuration Debug `
  -Weapon greatsword -Mode replay-shield -Seed 42 `
  -ReplayEvidence D:\Dev\NativePort\VS-NativePort\build\quality\equipment14-Debug-greatsword-loot-shield-seed42
build\quality\equipment14-check.ps1 -Configuration Release `
  -Weapon greatsword -Mode replay-shield -Seed 4 `
  -ReplayEvidence D:\Dev\NativePort\VS-NativePort\build\quality\equipment14-Release-greatsword-loot-shield-seed4
build\quality\live-restart-check.ps1 -Configuration Debug -GhostRestart
build\quality\live-restart-check.ps1 -Configuration Release -GhostRestart
```

The exact replays reconstruct recorded held-button intervals through the
existing `--scripted --press` interface, not memory writes or synthesized loot.
They use the previous successful controller timelines after many fresh
original32/255 shield rolls produced none. No rates, enemy HP, inventory
capacities or loot flags were changed. The original evidence remains separate;
new records use `equipment131-*`. One early replay attempt failed in the
observer's JSON-array handling and is not acceptance.

A second observer failure was reproduced exactly: Release seed8888 changed
shield affinity index2 from3 to4 during Fireball. Run `1792729273006787-0`
records precombat3, defeat-time4, pending4 and unchanged claimed properties.
The observer now compares loot with the defeat-time source and retains the
precombat snapshot separately. This was not a native loot-corruption defect;
ignoring combat adaptation would reject correct instance preservation.

Validation, with unrelated owner edits preserved and no baseline changes:

- Rebuild tests and Combat Lab in Debug/Release using the documented MSBuild
  command, `/nodeReuse:false /p:VcpkgManifestInstall=false`.
- Each final `[menu][equipment]` selection:6 cases/300 assertions.
  Each final full suite:500 cases/1,785,023 assertions.
- `.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py`:19 pass.
- Final repeated `copy_to_native.py` runs preserve751 generated outputs and
  all87 existing owned source/header paths. All70 changed manifest hashes
  match output bytes;69 consumers change only include providers, with the
  remaining generated header relinquishing T00633 to the owned type.
- Header-triggered quality/tidy initially identified two guard-style and two
  typed-padding-comparison findings. All four were repaired. Final
  `quality-native.cmd --files Native\Tests\src\EquipmentDetailTests.cpp --tidy`
  reports0 findings/0 new; final style checks over the owned header, equipment
  tests, layout assertions and copier test report3 existing/0 new.
- Only the Game project/filter were regenerated with the existing scaffold
  renderers, adding the owned header and the already-published loot paging
  header. Owner-dirty solution/tools projects were not regenerated.

Local evidence is `build\quality\equipment131-*`, the native run directories
above, and the preserved broad `equipment131-tidy-first-report.json`.
This closes the storage-specific behavior gate, not broader #14 derived-value,
legal-category and progression/restriction acceptance.

## Early flying-actor terrain context (#142)

Source is `13531f550ac08789c2cc79f84613987e118a20b1` plus the owned room-lifecycle
fix and regressions. The published parent is merge `3a9ed355`; its native tree
matches13531f55. Unrelated owner edits remain dirty/excluded.

The all-definition corpus reached356 successful pre-fix initialization
records before Gremlin97/2 aborted. Those historical records are not fresh
verification of this changed lifecycle. The source-backed matrix contains368
eligible definitions and87 explicit unavailable definitions; eleven variants
after the abort still had no record at this checkpoint.

Original failing fixture: zone97 room0, `MAP428`, Gremlin definition2/source
hostile row1, Ashley9,10 and enemy9,13, elevation/layer0, seed1, `post-minotaur`,
starting maxHP/MP999 and the existing isolated-event policy. No source asset,
enemy/stat/AI definition, runtime memory, HP or story flag is edited.

Before-fix Release `1792773773981507-0` and Debug `1792774027535234-0` abort
with code3 at46, before any ready frame. Initial hover intent12 calls
`Field_GroundHeightAtWorldPosition` through `Battle_Ai_IssueActorCommand`.
The diagnostic reports tile9,13: valid coordinates, but the cached native
frame height map is null. This is not an out-of-bounds tile, missing source
floor, a reason to enlarge the grid, or permission to return height0.

The owned room map already publishes authoritative terrain globals before
actor creation, but the native frame view was not populated until the first
AI update. Room initialization now snapshots the existing context immediately
after publishing all room pointers, before dependent terrain/gate callbacks
or actor creation. Teardown frees actor AI states, invalidates the borrowed
frame view, then frees room storage. It does not refresh/reset working RNG in
the middle of an issued command or change height mathematics/hover payloads.
Original map initialization/free ordering was read in `7E05C.s`; the existing
native query equations retain the source `7B5DC.s` behavior.

Two new direct regressions fail before the fix on null/unpublished terrain.
Afterward they verify an actual hover command before any AI update, correct
height/position payloads, live working-RNG preservation, global RNG
preservation, complete pointer/cache invalidation and two successive rooms.
The final selection `VS.Tests.exe "[ai][room-context]"` passes **20 assertions /
2 cases** in Debug and Release; full suites pass **545 cases / 1802426
assertions** each. Scoped maintained quality/tidy for `RoomState.c` and
`ActorCommandTests.cpp` reports **0 findings / 0 new**, without baseline edits.
Copier tests remain26/26; repeated generation preserves all751 generated
outputs and93 owned paths without output drift. The three directly related
ledger locations/stub-presence markers now reflect their existing registered
owned C implementations, not a new C++ promotion or full behavior gate.

| Build | Autonomous7200-tick run | Normal-speed three-entry/F10 run |
| --- | --- | --- |
| Debug | `1792775021061322-0` | `1792775386956705-0` |
| Release | `1792775009541767-0` | `1792775405766255-0` |

Each normal-speed session reaches ready at46 and genuine hover/movement at136.
Subsequent fresh/movement ticks are396/486 and545/635 in Debug,395/485 and545/635
in Release. Actual F10 and editor Start give three begins/three ends per
build, with identical seeded HP191/MP88, source slots, model/weapon fields and
positions on each entry. All four accepted sessions finalize normally with
zero engine error/critical/trace failures.

An initial accelerated Release controller attempt `1792775262731783-0` is
excluded: the editor reached its explicit18000-tick budget during navigation,
logged `host.exit` at18000 and exited0. Repeating the same acceptance at normal
speed fixes that observer budget mismatch; no native limit or behavior was
weakened.

The long autonomous windows prove stable initialization and ongoing movement,
not offensive Gremlin fidelity: no combat-result record was observed in7200
ticks. That remaining behavior stays under #16 rather than being invented
into a fix or claimed complete. The startup abort is the bounded #142 defect.

Builds use the standard test/Combat Lab projects in both configurations with
`/m /nodeReuse:false /p:Platform=x64 /p:VcpkgManifestInstall=false`. Local commands:

```powershell
.\.venv-3\Scripts\python.exe build\quality\arena16-roster.py run --configuration Debug --all-definitions --tag z097-d02 --frames 7200 --speed 8
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\gremlin142-live.ps1 -Configuration Debug
```

Repeat with Release. Local evidence uses `build\quality\gremlin142-*`,
`arena16-roster` and the native run directories. Fixed host SHA-256: Debug
`b4d269dc947ddbbb981c368a05448ff6bc2f4480e1b28d2bf7ecf3b6f6e83fb3`,
Release `63b0a769192fbc22f648ceeb4b47b4eb5f95f89d02135c4f85a7f890310bafab`.
Do not upload/commit disc-derived assets, captures or dumps. Existing AI
compatibility views remain migration debt; no C++ promotion or broad
compatibility retirement is claimed. Publication/exact CI are recorded on #142.

## NPC area filtering without player sphere UI (#140)

Source is `a37ac5a087a97295c7bbfe13b39e64c0aa9ae78c` plus the scoped F00103
copier adaptation, generated targeting code and regressions. Unrelated owner
edits remain dirty/excluded. The new source-backed roster corpus reached34
clean Release initialization cases before original Lich definition7 crashed.
Initialization/movement observations are not all-enemy behavior acceptance.

Original fixture: zone24 room1, `MAP080`, source hostile row0, Lich definition7
at tile5,7; Ashley5,4; elevation0 and preset `direction:0` for both; seed1, `post-minotaur`, starting
`max_hp:999/max_mp:999`, existing isolated room events. Enemy definitions,
AI/actions, inventory, source assets and story flags are not edited.

The legacy-named position `direction` is the production floor-layer selector
(0/1), not a cardinal facing override. `Actors_PlaceActorInRoom` uses it for
`currentTileLayer`; heading still follows normal initialization/AI/controls.

Before-fix Release `1792768167460696-0` and Debug `1792768368479320-0` exit
`0xC0000005`. At tick361, `Battle_FilterAreaActionTargets` calls
`Battle_SetSecondarySphereVolume` with no player sphere, writing address0x10.
A new direct NPC-filter regression also fails with SIGSEGV before the fix.
The preceding ordinary attack1 legitimately deals75HP at231.

`Battle_BeginActorActionSelection` intentionally creates the renderer only
for `isPlayerAction`. Other NPC-capable collectors already gate visual-volume
updates on that flag; target binding/drawing also tolerate the absent optional
renderer. F00103 alone unconditionally updated it. The copier now gates that
UI update on `isPlayerAction`, retaining the area origin, geometric filtering,
preview computation, every retained target and normal phase transition.
It does not allocate an NPC targeting UI, add a fake renderer, return early
from filtering, guard only a raw dereference, or emulate writes to address0.

After-fix Debug `1792768984618836-0` and Release `1792768986232195-0`
complete1800 ticks identically:

| Action | Tick | Limb damage | Actual Ashley HP | Enemy MP debit |
| --- | --- | --- | --- | --- |
| Normal1 | 231 | limb0:75 | 999 ->924 | none |
| Area130 | 829 | 0:79;1:76;2:83;3:81;4:79 | 924 ->526; sum398 | 103 ->65 once at363 |
| Area126 | 1579 | 0:76;1:78;2:80;3:82;4:76 | 527 ->135; sum392 | 65 ->27 once at1115 |

All damage sums match the exact preceding/following native frames. Natural
HP regeneration explains526 ->527 between impacts; native MP regeneration
later changes27 ->28 at1741. These are not memory/HP/MP edits or five resource
charges for five limbs. Normal field control returns after the real casts
(including frame1788); no victory or complete Lich behavior claim is made.

Actual speed1 keyboard lifecycle sessions:

| Build | Run | First area field return | Mid-effect F10 | Third-entry area return |
| --- | --- | --- | --- | --- |
| Debug | `1792769467702248-0` | 1057 | 1830 | 2891 |
| Release | `1792769524387582-0` | 1057 | 1830 | 2894 |

Each performs three Start entries/three F10 ends, including cancellation
while the living Lich is casting area130. Fresh seeded HP/MP, source slots,
weapon/model fields and positions match on every entry. Both first and third
entries actually apply all five limb hits and return normally to the field.
All four accepted autonomous/live sessions end normally with zero engine
error/critical/trace failures. Frame-limited autonomous sessions remain
interrupted encounters, not defeated-enemy/loot evidence.

The earlier observer run `1792769290342565-0` is excluded: its copied `Ready`
helper lacked the expected-actor-count variable and waited while genuine
combat continued to game over. Correcting the observer to expect two actors
did not change native behavior or weaken the acceptance checks.

Commands use the standard Debug/Release MSBuild test and Combat Lab projects,
`/m /nodeReuse:false /p:Platform=x64 /p:VcpkgManifestInstall=false`, followed by:

```powershell
build\Debug\VS.Tests.exe "[battle][sphere][area-target]"
.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\lich140-live.ps1 -Configuration Debug
```

Repeat with Release. Final focused tests pass **14 assertions / 2 cases**;
full suites pass **543 cases / 1802406 assertions** each; copier tests pass26.
Repeated generation preserves **751 outputs / 93 owned paths**, changes only
generated `Targeting.c` and has identical second-run hashes. Maintained
quality/tidy for the sphere tests and two copier files reports **0 findings /
0 new**. Generated output is checked through generation/build/tests, not the
quality command's unsupported hand-maintained-only file selector. No quality
baseline is changed.

Fixed host SHA-256: Debug
`476b8ecb447a4a78b34dbccf592e49bba9171b8ee5fbad265bc7a8ba201e0af5`,
Release `d8d2d46eeeb2c1d6519d80d15d8c0e34cfb61a50179f4bd73cb1f62d094b362c`.
Before-fix hashes are the #139 checkpoint's hosts. Local evidence is
`build\quality\lich140-*`, source presets/results under
`build\quality\arena16-roster`, and the native run directories above. Do not
commit/upload disc-derived data or captures. Broader #16/#76/#80 and the
separately observed Wyvern148 effect wait remain open; no C++ promotion or
compatibility retirement is claimed. Publication/exact CI are recorded on #140.

## Original key and sigil lock acceptance (#139; supporting #16)

Source is published `63beaf26819b3f73a801baa0506cc70b051223f5`, with no
native/generator changes for this verification. Unrelated owner edits remain
dirty/excluded. All32 named IDs458-489 already fit the stock `all-items-b`
inventory; `all-items-a` supplies the missing-item controls. No extra equipment,
inventory capacity, stat override, synthetic lock, process write, forced flag
or event-policy change is used.

Original MPD records contain76 ordinary locks covering31 named IDs. Chest
Key465 instead uses the separate original container layout, with five matching
chests. The chosen32 approaches come from81 supported source candidates and
retain the production floor/facing rules: ordinary locks allow63 height units;
chests require Ashley at floor+96. Event-marked approach tiles use the existing
`room_events:"isolated"` policy, not a new story bypass.

The actual production interaction is **Cross while facing the lock**, not
selecting a key in the consumable menu. Each matching run first turns away and
presses Cross, verifies no lock/count/flag/inventory mutation, then faces the
original target and presses Cross. Dialog confirmations are bounded by the
actual interaction state; they are not blind extra Circle presses in the field.

- All seven ordinary keys458-464 open their matching lock and remain at10.
- All24 sigils466-489 open their matching lock, consume exactly10 ->9 and set
  the original persistent unlock flag at `item+0x1B6` to2.
- Chest Key465 opens the original MAP124 chest, remains at10 and sets flag903
  to1. Chest contents may legitimately alter other inventory, so this branch
  does not claim the door runs' full-inventory identity check.
- Every ordinary matching lock compares all3840 inventory bytes with the
  expected snapshot: no changes for a reusable key; only its count byte for
  a sigil. Missing-item controls preserve the complete original inventory.
- Missing ordinary key, Chest Key and sigil controls retain locked1/open0,
  count0 and flag0 in both configurations.
- Repeating Cross on the opened sigil lock leaves count9/flag2. Actual F10,
  editor Start selection and Enter restart the same preset: locked1/open0,
  count10/flag0 and the complete original stock inventory are restored.

All72 accepted sessions finalize with `session.end`, `trace_failed:false` and
zero error/critical engine or trace-failure records. This is64 matching runs,
six representative missing controls and two repeat/reset runs. The completed
all-named matrix has no unsupported or untested named item.

Original fixture and accepted matching runs follow. Scene is zone/room,
original MPD and zero-based record row; coordinates are tile X/Z. Row0 for465
is a container, not an ordinary-lock row. Spawn elevation/direction are0; turn
from the start tile toward the target. Seed1, speed1, solo Ashley and the
existing isolated-event policy apply throughout.

| Item | Scene; source; row | Start X,Z | Target X,Z | Debug run | Release run |
| --- | --- | --- | --- | --- | --- |
| 458 | 49/0; MAP247; 1 | 3,14 | 3,15 | `1792762910989695-0` | `1792763007910736-0` |
| 459 | 28/13; MAP118; 0 | 2,13 | 2,14 | `1792763793655338-0` | `1792764267675859-0` |
| 460 | 30/4; MAP143; 1 | 17,16 | 17,17 | `1792763809940447-0` | `1792764282909149-0` |
| 461 | 29/2; MAP126; 0 | 6,7 | 6,8 | `1792763825671681-0` | `1792764298143745-0` |
| 462 | 55/17; MAP376; 0 | 2,1 | 2,0 | `1792763841208606-0` | `1792764313352356-0` |
| 463 | 55/16; MAP375; 0 | 2,1 | 2,0 | `1792763857473134-0` | `1792764328573820-0` |
| 464 | 28/0; MAP105; 0 | 5,7 | 6,7 | `1792763873101689-0` | `1792764343849488-0` |
| 465 | 29/0; MAP124; 0 | 2,8 | 2,7 | `1792762941668264-0` | `1792763037832322-0` |
| 466 | 9/4; MAP013; 0 | 3,13 | 3,14 | `1792762975939303-0` | `1792763071630341-0` |
| 467 | 13/12; MAP040; 0 | 20,8 | 21,8 | `1792763888810056-0` | `1792764359046529-0` |
| 468 | 55/1; MAP360; 0 | 5,1 | 5,0 | `1792763906072301-0` | `1792764374950105-0` |
| 469 | 28/17; MAP122; 0 | 4,13 | 4,14 | `1792763922416991-0` | `1792764391116951-0` |
| 470 | 50/9; MAP269; 1 | 9,1 | 9,0 | `1792763938734392-0` | `1792764407160764-0` |
| 471 | 50/20; MAP280; 0 | 11,12 | 11,13 | `1792763955087413-0` | `1792764423326802-0` |
| 472 | 53/1; MAP312; 1 | 6,1 | 6,0 | `1792763971449457-0` | `1792764439337590-0` |
| 473 | 53/12; MAP323; 0 | 8,14 | 8,15 | `1792763988543215-0` | `1792764455375751-0` |
| 474 | 53/21; MAP332; 0 | 9,1 | 9,0 | `1792764005591247-0` | `1792764472303964-0` |
| 475 | 23/4; MAP077; 2 | 1,12 | 0,12 | `1792764021885317-0` | `1792764488471159-0` |
| 476 | 24/5; MAP084; 2 | 9,25 | 9,26 | `1792764038265629-0` | `1792764505204055-0` |
| 477 | 24/11; MAP090; 4 | 9,1 | 9,0 | `1792764054547433-0` | `1792764521981729-0` |
| 478 | 24/12; MAP091; 0 | 4,1 | 4,0 | `1792764070974684-0` | `1792764538149464-0` |
| 479 | 29/3; MAP127; 1 | 6,7 | 6,8 | `1792764087255578-0` | `1792764554080328-0` |
| 480 | 29/3; MAP127; 2 | 6,1 | 6,0 | `1792764103749886-0` | `1792764570266079-0` |
| 481 | 29/4; MAP128; 0 | 6,7 | 6,8 | `1792764120046859-0` | `1792764586263318-0` |
| 482 | 29/4; MAP128; 1 | 6,1 | 6,0 | `1792764136466670-0` | `1792764602378612-0` |
| 483 | 29/5; MAP129; 0 | 6,7 | 6,8 | `1792764152856989-0` | `1792764618496555-0` |
| 484 | 29/5; MAP129; 1 | 6,1 | 6,0 | `1792764169096710-0` | `1792764634440455-0` |
| 485 | 29/6; MAP130; 0 | 6,7 | 6,8 | `1792764185460570-0` | `1792764650582583-0` |
| 486 | 29/6; MAP130; 1 | 6,1 | 6,0 | `1792764202405687-0` | `1792764666776089-0` |
| 487 | 9/17; MAP409; 0 | 2,8 | 2,9 | `1792764218630892-0` | `1792764683121169-0` |
| 488 | 32/17; MAP164; 0 | 1,3 | 0,3 | `1792764235086541-0` | `1792764699164627-0` |
| 489 | 32/11; MAP158; 0 | 14,2 | 14,3 | `1792764251503413-0` | `1792764715426148-0` |

| Additional control | Debug run | Release run |
| --- | --- | --- |
| Missing458 | `1792762926615461-0` | `1792763023137733-0` |
| Missing465 | `1792762958005501-0` | `1792763053915958-0` |
| Missing466 | `1792762993030762-0` | `1792763087634385-0` |
| Repeat466 / F10 / fresh reset | `1792764752246225-0` | `1792764775422319-0` |

Local reproduction commands, with the retained controller/read-only observer
helpers in `build\quality`, are:

```powershell
python build\quality\key139-fixtures.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\key139-live.ps1 -Configuration Debug -Item 489 -FixtureFile build\quality\key139-fixtures\489.json
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\key139-live.ps1 -Configuration Debug -Item 465 -Missing
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build\quality\key139-live.ps1 -Configuration Debug -Item 466 -Repeat
```

Repeat with `-Configuration Release`; the all-named batch uses each458-489
fixture. Helpers/presets, observations, screenshots, console logs and final
72-run audit remain local under `build\quality\key139-*`; native traces remain
under each configuration's `combat-lab\runs`. The table also permits manual
editor reproduction without the local observer.

Current noninstrumented Combat Lab SHA-256: Debug
`840827e8ef5779bd68ab4c7c12452ce38a9dd939f93e6baa91662957b602159c`,
Release `1598c04e8094e30e32fed4f2b9ba526ac37c3d95de2d628bd30130e36dfb0480`.
Temporary fixture diagnostics were removed through the generator before
acceptance; native/copier sources are unchanged from the published revision.
The source's full539-case/1799864-assertion suites,25 copier tests and exact
green CI39959691765 are the preceding baseline, not a new suite execution for
this documentation-only acceptance.

The rejected lower-stair Bronze fixture in MAP160 correctly failed the
original63-unit height guard (observed71); it is not a native bug and the guard
was not weakened. This checkpoint verifies one original matching fixture for
every named item, not every duplicate lock, full campaign progression,
save/load persistence, C++ promotion or compatibility retirement. No new
production defect or key-policy change was needed. Publication and exact CI
are recorded on #139; broader #16/#76/#80 remain separate.

## Five-/nine-sample source trajectory regression (#76, bounded)

After #138, the existing PLG057 five- and nine-sample initialization/update
implementations were checked against the read-only original
`EFFECT/trails5.h:380/407`, `EFFECT/trails9.h:387/414` and the fixed-point
Lagrange coefficients in `BATTLE/BATTLE.PRG/5BF94.c:6388`. No production trail
algorithm or buffer count is changed.

Two maintained cases perform two complete12-step paths and collapse/re-entry
cycles per family, with two independent trails and an adjacent storage guard.
They verify exact spline progress/speed/coordinates, distinct5/9 history
capacities and modulo indices, bounded sample counts, collapse counters,
zero-path independence and complete repeat initialization. The12 coordinate
goldens were calculated independently from the original recurrence and
coefficient shifts, not read back from the implementation under test.

The original `ONE*2/3` truncation is preserved: the final sample is511/0/255
for a512/0/256 control endpoint, and intermediate cubic samples can overshoot
the control-point box. These source-defined values are not arbitrarily
clamped or snapped to make a path look tighter.

Focused `VS.Tests.exe "[effects][trails][reference]"` passes **508 assertions /
2 cases**. Full Debug/Release suites pass **539 cases / 1799864 assertions**
each after the standard test-project MSBuild commands with
`/p:VcpkgManifestInstall=false`. Scoped maintained quality/tidy for
`ParticleRenderTests.cpp` reports **0 findings / 0 new**. Source is
`f13f144d` plus only the new trail regressions; unrelated owner edits remain
dirty/excluded. Local logs: `build\quality\trail76-*`.

This adds source-trajectory/history evidence, not a new runtime defect or an
original-game frame comparison. Trail rendering, opacity, source/target
attachments, camera spaces and representative live original-frame fidelity
remain separate #76 gates. No C++ promotion or compatibility retirement is
claimed. Publication and exact CI are recorded on #76.

## Five-/nine-sample ribbon rendering reference (#76, bounded)

Source is `9d43d9cb7e33f8a2ebc2c0c4d5f6c42a8b6c3052` plus two appended
maintained render cases. Native runtime/generator code and trajectories are
unchanged; unrelated owner edits remain dirty/excluded. The read-only
`rood-reverse` revision5163208b supplies original `trails5.h:449` and
`trails9.h:456` render behavior.

The actual PLG057 render entry points run96 generated configurations across
both families. Each contains two independent ribbons and checks:

- World-camera versus selected effect-view projection, with transform index1
  and distinct translations so the wrong matrix cannot pass accidentally.
- Both history origins, wrapped five-/nine-sample traversal, full history,
  two-sample collapse and the one-sample no-ribbon state.
- Every emitted vertex, UV, RGB component, palette/page, semitransparent
  command, packet length, draw-mode restoration and depth256 ordering link.
  Horizontal32-unit samples at Z1024 project8 pixels apart; width8 yields
  +/-4-pixel edges. Transparency and texture coordinates follow the original
  distinct4-/8-segment denominators.
- Exact strict near clipping: depth256 renders with near255, not near256.
  Texture animation still advances when clipped or no ribbon is emitted.
- Normal allocation and exact final-buffer fits, checking cursor movement
  and both adjacent canaries for both native packet families.

The first fixture incorrectly expected texture timer7. Reading original
`func_800D6D24` confirms initial timer8, followed by2-unit decrements on later
calls; only the test expectation was corrected. No native timing change or
new production defect was established. Assertion arrays retain all exact
vertex/texture/color comparisons while keeping helpers within existing
maintained quality limits; no baseline or check was relaxed.

Final Debug and Release builds use the existing
`Native\Tests\VS.Tests.vcxproj` MSBuild command with
`/m /nodeReuse:false /p:Platform=x64 /p:VcpkgManifestInstall=false` and each
configuration. `VS.Tests.exe "[effects][trails][reference]"` passes **3036
assertions / 4 cases** in each build: the earlier508 trajectory assertions
plus2528 render assertions. Full suites pass **541 cases / 1802392
assertions** each, also checking shared renderer-state restoration. Scoped
`quality-native.cmd --files Native\Tests\src\ParticleRenderTests.cpp --tidy`
reports **0 findings / 0 new**; exact fixture literals remain separately
classified, not a refreshed baseline.

Local build/selection/full-suite/quality logs use
`build\quality\trail76-render-*`. Combat Lab binaries remain the #139 hashes;
this checkpoint is source/component evidence, not a new live effect replay
or aligned original-game frame comparison. Non-horizontal joins, complete
origin/target attachment callbacks, live opacity/trail appearance and all
representative original scenes remain broader #76 gates. No C++ promotion,
compatibility retirement, arbitrary clamp or blanket #76/#16 completion is
claimed. Publication and exact CI are recorded on #76.

## Native particle attraction field repair (#138; supporting #76)

Source: `d0002768` plus the scoped copier adaptation, generated F01554 and
motion regressions. No emitter definition, random distribution, spread,
velocity, mass, gravity, drag or original arithmetic/order is changed.

The PS1 `Effects_ParticleAttractionView` overlaid a complete120-byte particle.
The native particle is152 bytes after pointer widening. Compiled PDB evidence:

| Field | Stale PS1 view | Native particle |
| --- | --- | --- |
| Target X/Y/Z | 98 / 100 / 102 | 124 / 126 / 128 |
| Attraction strength | 112 | 140 |
| Native emitter flags | not the attraction field | 112 |

F01554 therefore used emitter flags as attraction strength and read target
coordinates from texture-animation pointer bytes. Its relative scalar motion
view still matches the pointer-free motion subobject; that is not the failing
absolute whole-node contract. The generator now uses the typed native
`attraction` and `attractionTarget[0..2]` fields. The historical view declaration
remains source provenance, not a valid native overlay or a retired EC claim.

Three new production-integrator regressions fail **15 / 109 assertions** before
repair. They prove that render/attachment flags must not invent attraction,
actual signed strength and per-axis native targets must affect force, and
linked particles must retain independent attraction state. Fixed focused
motion/emission/render coverage passes **355 assertions / 13 cases**; full
Debug/Release suites pass **537 cases / 1799356 assertions** each.

Original E094/Hellhound breath data provides a concrete live fixture: its12
particle definitions have nonzero flags but zero attraction ranges. The
original block traversal and source integration routine
`BATTLE/BATTLE.PRG/5BF94.c:7830` establish that, for these existing particles,
each force component should advance by its acceleration alone. The disc was
also inspected read-only through VS-Workshop: E094's11 actual sprite programs
are not a reconstruction of runtime emitter/world/camera behavior. That tool
was used only for original source inspection; its overlay remained clean and
the session was closed without changes.

Actual-window/controller observer:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build\quality\particle138-live.ps1 -Configuration Debug -Fixed
```

Repeat with Release; add `-DelayFrames 64` for the later flame burst. The
ignored scenario uses the owner's original zone11/room0, seed1, Prologue
Ashley8,12 and eight source-backed Hellhound3 definitions at5..12,13. No stat/
HP/event edits, process writes, fake live particles or forced outcomes are used.
Ordinary enemy AI starts breath147/E094. Actual F6/F7 pause and advance one
logical frame at a time, while a read-only observer compares stable particle
identities/ages and their original zero-attraction force increments.

| Configuration / window | Run | Component comparisons | Spurious-force mismatches |
| --- | --- | --- | --- |
| Debug before repair, ticks459-467 | `1792760032010956-0` | 90 | 89 |
| Release before repair, ticks459-467 | `1792760055612119-0` | 90 | 89 |
| Debug fixed, ticks459-467 | `1792760103122030-0` | 90 | 0 |
| Release fixed, ticks459-467 | `1792760126849209-0` | 90 | 0 |
| Debug fixed, later flame burst | `1792760262016302-0` | 1272 | 0 |
| Release fixed, later flame burst | `1792760287240181-0` | 1236 | 0 |

One measured native node has flags135216 and actual attraction0: before
repair it invents force64669/118710/-232 despite zero acceleration; the
fixed corresponding node retains force0/0/0. These are computational producer/
consumer comparisons, not guessed corrections from the appearance of a flame.
The actual effects complete and return normally to the field. All six final
paired/later sessions end normally with `trace_failed:false` and no engine
error/critical record. Inspected later captures show the original flame
program directed from the Hellhound toward Ashley; no replacement flash or
arbitrary particle clamp is used.

The native fixed host SHA-256 is Debug
`3f48f0ba0c3d410dcc1065f2151e91d167dd3b2f23f402eb352db088b195b093`
and Release
`1598c04e8094e30e32fed4f2b9ba526ac37c3d95de2d628bd30130e36dfb0480`.
Before-fix hosts match the #83 final hashes in the checkpoint below. Unrelated
owner work remains dirty/excluded. Standard MSBuild test/Combat Lab project
builds use `/p:VcpkgManifestInstall=false`; maintained
`quality-native.cmd --files` for the particle tests and copier files with
`--tidy` reports **0 findings / 0 new**. The copier suite passes **25 cases**;
second generation retains **751 outputs / 93 owned source/header files**.

Local evidence: `build\quality\particle138-*`, including both-window PNGs,
particle snapshots, live audit and build/test/quality logs, plus each native
run directory. Do not commit/upload disc-derived assets, pointer dumps or
captures. This resolves the concrete attraction alias, not all of #76:
aligned original-game visual reference, every emitter/attachment space and
the separate five-/nine-sample trail paths still require their own evidence.
Publication and exact CI are recorded on #138.

## Accelerated native audio timing (#83)

Fresh before-fix Debug run `1792758315183981-0` reproduces the reported
speed4 failure at tick1132: `native voice command queue exhausted`. This is
the same producer/consumer clock mismatch as the original #83 report, not
the separate F10 lifecycle path.

The desktop host already constructs `AudioDevice(speed == 1)`. Combat Lab
incorrectly always requested a realtime device, so faster sequencer commands
accumulated behind the real-time device callback. The Lab now uses the existing
desktop policy and logs the chosen mode:

- **Speed1:** unchanged device-demand rendering. The audio callback sustains
  voices while the game/control thread waits; existing stereo/mono controls,
  sample cadence and normal listening behavior remain in use.
- **Speed2-64:** sequencer-driven native PCM. Every control tick renders through
  the existing synth/envelope/reverb/noise/FM/stream path before the next tick;
  voice commands are not dropped and the command queue is not enlarged.
  Speaker playback remains at its physical rate. The existing device discards
  already-rendered stale speaker PCM once its250ms watermark is exceeded,
  explicitly logging the removal and restoring its50ms scheduling headroom.
  Maximum queued output is the250ms watermark plus one200-frame block
  (**97600 bytes / 254.17ms** at48kHz float stereo), not unbounded latency.
  Raw native mix capture is written before stale speaker output is removed.

This reuses an established native timing mode, not a new pitch-shifting or
time-stretch engine. Fast-forward listening can skip obsolete already-rendered
segments; use speed1 for auditory parity. Requested speed is a simulation
multiplier, not a promise that synth/render/trace CPU cost can deliver64x.
Movie/other independently bound device streams are not newly synchronized
or certified by this change.

Append-only maintained regression: a paused physical device receives4096
production stream/control ticks for both mono and stereo looping sources.
Every tick is consumed synchronously, live voices remain active, the actual
SDL PCM queue stays within97600 bytes, and resulting device samples are
audible in both channels. Existing normal-speed demand-versus-push negative
controls still prove that device demand sustains voices through a one-second
control-thread stall.

Reproduction/acceptance commands:

```powershell
build\Debug\VS.CombatLab.exe --data Native\data `
  --scenario build\quality\cure-books.json --start --scripted `
  --frames 1800 --speed 4
```

Repeat with Release, speeds1/64 and a second fresh-process speed4 run. The
ignored preset is original zone9/room0, all-items-a Ashley4,9, seed1, solo and
isolated events. These are music/queue tests, not consumable-effect acceptance.
`VS_AUDIO_CAPTURE` points each process to a local `.f32` mix file.

| Configuration | Speed1 | Speed4 first | Speed4 repeat | Speed64 |
| --- | --- | --- | --- | --- |
| Debug | `1792758810856954-0` | `1792758851152579-0` | `1792758884498092-0` | `1792758917675740-0` |
| Release | `1792758951074111-0` | `1792758991422506-0` | `1792759008481651-0` | `1792759025689446-0` |

All eight exact final-host runs complete1800 ticks, log the intended mode,
end with `trace_failed:false`, and contain no engine error/critical record or
command overflow. Speed1 has no stale-output warnings. Accelerated runs
explicitly log their expected stale speaker PCM removal.

All captured samples are finite and audible on both channels. Each accelerated
capture contains1436600 stereo frames (29.92917s); both speed4 repetitions and
speed64 are byte-identical within each configuration:

- Debug PCM SHA-256:
  `22b39895e941f1cd474980f92b9b72ecaf79a7b55a0858053b09ca121ec86e05`.
- Release PCM SHA-256:
  `6647e6c77b603b9dcda4c7e0e67584cf03ad666055d03cacb3158a5eaec54cd6`.

Normal-speed device-demand capture has its own callback timing and is not
asserted byte-identical to offline simulation output. Full final native suites
pass **534 cases / 1799247 assertions** each; focused accelerated plus unchanged
demand/stall cases pass **9463 assertions / 4 cases**. Scoped maintained
quality/tidy for the Lab and audio tests reports **0 findings / 0 new**.
No baseline, queue capacity, voice event, dependency or production sequencer
implementation is changed.

Final host SHA-256: Debug
`1a9dc68aa3e251ae71a67a2dbbf3d920834395bda63078cea115ab3397998476`;
Release
`09589642af8f5681da2e3ab788e33f682a9533276dc5d4b016c26ead07f27197`.
Source is `4bd0a3e7` plus the scoped Lab constructor/log and audio-regression
changes; unrelated owner edits remain dirty/excluded. Exact commands, audit
JSON, build/test/quality logs and raw PCM stay under `build\quality\issue83-*`
and the configuration's run directories. Do not commit/upload disc-derived
mixes or assets. Publication and exact pushed CI belong on #83.

## Selected-part palette glow acceptance (#81)

Source: `65ef4dee` plus the scoped owned model-renderer/caller and packet-test
changes. The original actor CLUT pulse producer and its selected-limb mask were
already active. Fresh baseline Debug run `1792756849456949-0` uses real
Fireball target controls to select soldier actor6's right arm, left arm and
head. Read-only compiled-layout observation shows masks128/64/32 and animated
palette values, but all175 body polygons keep the original CLUT and none use
the pulsed CLUT. This independently reproduces the owner's report.

The maintained actor-draw path submitted every polygon through the uniform
palette renderer. Original `4B16C.s` F01269/F01270 override routines use the
SHP header's eight per-limb counts for each triangle/quad/special partition.
Only faces belonging to an enabled mask bit use `baseCLUT+0x480`; unselected
and unpartitioned remainder faces retain the base palette.

`Render_DrawActorPolygons` now applies that rule to the typed actor instance.
The existing flat/colored entry points remain uniform and do not reinterpret
weapon-model matrices as an actor's pulse state. Adaptive split triangles
inherit their source polygon's selected palette. Malformed group totals are
logged and rejected rather than tinting all geometry. The actor caller reuses
its existing model, pose, palette animation and tint producers; this is not a
whole-enemy tint substitute or a new palette animation.

Red packet regression: **32 failures / 50 assertions**. Green tests cover all
eight limb bits, selected versus unrelated masks, flat and colored triangles,
both adaptive split orientations, unpartitioned remainders, exact UV/order/
color preservation and primitive-buffer guards. The tests retain pre-existing
Catch case identities; no expectations or quality baselines were weakened.

Final actual-window/controller commands:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build\quality\equipment14-check.ps1 `
  -Configuration Debug -Mode workshop-gate -Zone 32 `
  -WorkshopAction city -Seed 42 -HighlightProbe
```

Repeat with `Release`, and replace `-HighlightProbe` with
`-HighlightComposite` for the original zone13/room12 Ghost3. The latter
retains its original auxiliary model in slot6, rather than substituting an
ordinary single model. Ashley uses all-spells and the established
HP/MP/STR/INT/AGI999 test setup. No process writes, HP/flag manipulation or
forced outcomes establish acceptance.

| Configuration / model | Final run | Pulsed faces, arm/arm/head | Original-palette faces |
| --- | --- | --- | --- |
| Debug soldier | `1792758009859446-0` | 25 / 27 / 45 | 150 / 148 / 130 |
| Release soldier | `1792758084997631-0` | 25 / 27 / 45 | 150 / 148 / 130 |
| Debug two-model Ghost | `1792758053402526-0` | 18 / 17 / 17 | 56 / 57 / 57 |
| Release two-model Ghost | `1792758128007604-0` | 14 / 20 / 16 | 61 / 55 / 59 |

These are read-only submitted-packet observations, not assumed pixel areas.
Normal controls change selected limbs and, for the extended list, target-page
actors. The old actor's mask clears, the new actor's mask is active, cancellation
clears every live model slot's pulse, and re-entry restores the selected part.
Both model cases then commit real Fireball, apply lethal damage, reach ordinary
post-defeat/loot flow and clear target-page bindings. All four sessions end
normally with `trace_failed:false`, no error/critical records and no malformed
model-range rejection. Selected/cancelled captures are inspected locally.

- Full final Debug/Release suites: **533 cases / 1791001 assertions** each.
  Focused model cases:58 assertions/6 cases. Commands use the documented
  MSBuild test/Combat Lab projects with `/p:VcpkgManifestInstall=false`, then
  `VS.Tests.exe` and the actual-controller commands above.
- Maintained quality/tidy for both renderer/caller and test translation units
  reports0 native findings and **0 new** findings. The131 pre-existing test
  findings remain tracked separately; no baseline edits. The initial
  header-wide analysis ran before correcting the six new findings, then
  focused translation-unit analysis verifies all corrections.
- Copier24 tests pass; all751 generated outputs and93 owned files are
  unchanged on a generation run, preserving the owned implementation.
- Final host SHA-256: Debug
  `1983e5e51d425eadae31cd19ddf420dd18ee70bc6fd04e1676668bb459df9711`;
  Release
  `9d5b6abce4f7cb174ebbf8444f1580ca19a6e1e731e92cb94ce1400912576780`.
  Unrelated owner edits remain dirty and excluded.

Local-only evidence: `build\quality\issue81-*`,
`workshop15-{Debug,Release}-city-seed42-render-highlight*` and each run
directory. Do not commit/upload disc-derived data, packet dumps or captures.
This is bounded selected-part glow acceptance, not every model/appearance
override, C++ promotion, compatibility retirement or #80/#76 completion.
The unrelated action-state override stubs remain tracked debt. Exact
publication and pushed CI are recorded on #81.

## Eight-Minotaur overflow and repeat-entry acceptance (#78)

The historical fresh-process crash is
`build\Debug\combat-lab\runs\1792578930835602-0`, tick209:
`SetDrawArea <- Field_DrawRoomScene <- Boot_RunGameLoop`, reading
`0x557FECC05FB`. The final preceding frame has Ashley plus all eight configured
Minotaur Zombies, immediately before their first action starts. It is not just
an inherited post-defeat state.

Current matched source-backed startup controls use zone12/room0, seed1,
post-minotaur Ashley9,10 and definition1 at5..12,11. No stat overrides or
gameplay input are added. Fresh450-tick runs:

| Configuration | One enemy | Eight enemies |
| --- | --- | --- |
| Debug | `1792755693404643-0` | `1792755708739732-0` |
| Release | `1792755724407105-0` | `1792755739581297-0` |

All retain the configured2/9 actors, execute a real enemy attack and end
normally without engine error records. This negative reproduction alone was
not treated as root-cause proof.

A temporary generator-based producer trace then measures the first eight-enemy
normal attack, source actor9/action1: **36** candidates, with preview31 exactly
`SVECTOR{1531,-308,1367,0}` (Debug diagnostic run `1792756208327969-0`, tick214).
Packed as the native pointer that vector is **`0x557FECC05FB`**, byte-for-byte
the historical invalid render cursor. The original23-entry producer and
index31 scratch alias are the same contract independently proven in #136.
This aligns the action/state rather than pretending the changed startup
timestamps are identical. The repair is `073d58c5`; this checkpoint adds
explicit boundary/paging regressions and fresh #78 acceptance rather than a
second unrelated renderer fix.

The temporary `target78` hook is removed through the generator before final
builds. No temporary cursor/spawn diagnostics or old action-size clears remain.
Maintained regressions use the native preview type for the31 boundary/canary
and36 six-part candidates across24+12 visible pages, including absolute31/35
mapping. They do not reduce configured actors or change PS1/original data.

Final actual-controller observer:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build\quality\issue78-live.ps1 -Configuration Debug
```

Repeat with `Release`. The ignored scenario is
`build\quality\issue78-eight-native.json`. All three entries retain Ashley and
primary slots2..9, their original weapon models and source spawn indices;
the same seed reproduces every fresh HP/MP/max/model/slot value. No auxiliary
slot is substituted or actor silently omitted.

- Debug `1792756376145405-0`: real96HP enemy hit at306, F10 after recovery
  at392; exact eight-enemy re-entry at684; actual F6/F7 pause/single-step during
  live phase7, then F10 at840; third entry at927 and another real96HP hit at1159,
  followed by normal stop at1245.
- Release `1792756431965231-0`: the same control sequence; real hits at306/1157,
  stops392/837/1243 and fresh entries680/925. The paused step advances exactly
  one logical tick. Both runs log three normal encounter starts/ends and
  `trace_failed:false`, with no error/critical engine record.
- Final full Debug/Release suites: **532 cases / 1790951 assertions** each,
  built with the documented MSBuild test-project command and
  `/p:VcpkgManifestInstall=false`;24 copier tests pass. Scoped
  `quality-native.cmd --files Native\Tests\src\SphereTerrainTests.cpp --tidy`
  reports **0 findings / 0 new** without baseline edits.
- Exact final host SHA-256: Debug
  `e3f23d55540aac33608a800a55555b794e15e33e4530782b2df592c0ea95bc36`;
  Release
  `12c6d8c63b7b74560103a393dd3acb4569b2eeb49ed00fb845993e9544afdffc`.
  Native source is `a1dff228`/`073d58c5` plus only the new maintained tests;
  unrelated owner work remains dirty and excluded.

Local evidence uses `build\quality\issue78-*`, including the producer log,
startup audit, final lifecycle audit and both live observations, plus the
configuration's run directories. No disc-derived evidence is committed or
uploaded. This verifies the reported startup/cancel/re-entry contract, not
full Minotaur loot/defeat, every large group, multi-part spell fidelity or
#80/#81/#76 acceptance. Exact publication/CI is recorded on #78.

## Live original blade and shield combination acceptance (#15)

Native source: published `073d58c5680772beb7910a225d5439bde8305135`, exact Source
Quality39940975312 green. These runs use the same Debug/Release host hashes
recorded in the #136/#137 checkpoint below; unrelated owner edits remain
uncommitted. No native code, original data, inventory limits, event flags or
process memory are changed to obtain these results.

The ignored actual-controller observer is
`build\quality\equipment14-check.ps1`. Blade commands:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build\quality\equipment14-check.ps1 `
  -Configuration Debug -Mode workshop-gate -Zone 32 `
  -WorkshopAction city -Seed 42 -CombineCityLoot
```

Release uses `-Configuration Release -Seed 43`. Both start with the original
Sarjik/Bejart definitions plus six original Crimson Blades on supported MAP148
floor, Ashley8,16/all-spells/HP-MP-STR-INT-AGI999. Ordinary spell controls defeat
the enemies and normal loot delivery supplies the two guaranteed original
weapons. Normal movement and Cross open the original8,13 door to workshop43.
The complete3840-byte inventory is unchanged by that transition.

| Gate | Debug | Release |
| --- | --- | --- |
| Blade run | `1792752847953340-0` | `1792752847807210-0` |
| Actual blade confirmation | tick14721 | tick14205 |
| Repeated menu return / field | tick15554 | tick15042 |
| Shield run | `1792753985429352-0` | `1792753985229923-0` |
| Actual shield confirmation | tick7700 | tick7678 |
| Repeated menu return / field | tick8542 | tick8506 |

For blades, actual native selection combines the original iron Sabre15 and
bronze Rapier51 into bronze Guisarme64, matching the independently inspected
ordered SYD recipe. Two complete preview/confirmation-cancel/leave/re-enter
cycles preserve all inventory bytes. Actual confirmation then clears both
source blades, disassembles their two unequipped owning weapons, releases their
unchanged grips, and stores the complete preview in the first blade slot with
the correct loose ownership/index. Independent durability-average checks and
byte-exact expected inventory mutation pass. The equipped weapon and unrelated
properties/items remain unchanged. These bounded source weapons have no gems;
gem ownership has separate operation coverage below.

Shield acquisition uses eight original Crimson Blade4 definitions, seed47 in
both configurations. The original disc scan finds no guaranteed shield template;
their unmodified shield rate is26/255. Actual rank controls select ExplosionIV,
the original elevation/horizontal selectors retain the real area-filtered
targets, and ordinary Fireball finishes the out-of-range survivor. Ashley's
normal damage/cost and every original drop/result remain active. Both accepted
runs obtain two genuine loose bronze Bucklers127 through the loot menu, then
carry them through the same original workshop door. Nothing is injected.

Shield commands replace `-CombineCityLoot` with `-ShieldCityLoot` and use
`-Seed 47`. The original127/material3 pair previews127/material3. Two complete
cancel/re-entry cycles preserve all3840 bytes. Confirmation consumes both
source records, preserves the first index, and stores the full preview and
independently checked averaged DP. The exact inventory expectation, unchanged
unrelated equipment and two subsequent menu/workshop-to-field returns pass.

All four accepted runs end with `trace_failed:false`, no error/critical engine
records and no trace failures. Paths:
`build\quality\workshop15-{Debug,Release}-blade-accepted-*`,
`workshop15-{Debug,Release}-shield-accepted-run.txt`,
`workshop15-{Debug,Release}-city-seed47-render-shields-*`, and the corresponding
configuration's `combat-lab\runs` directories. Captures remain local. Shield
preview/return captures are inspected at physical client resolution; the
earlier blade captures had an observer DPI crop, not a claimed native UI fault.
No result is inferred from screenshots alone.

This completes the previously missing representative live combination
transactions. The other operation/location gates were also refreshed on this
same native revision, rather than relying on earlier checkpoints:

| Fresh gate | Debug run | Release run |
| --- | --- | --- |
| Two complete disassemble/assemble/name/re-equip cycles | `1792754279623919-0` | `1792754279455889-0` |
| Original stock armor cancel/combination | `1792754507618342-0` | `1792754506677457-0` |
| Real Grissom gem loot and two detach/rollback/reattach cycles | `1792754949053119-0` | `1792754588240947-0` |
| Real melee wear / original door / cancel / repair / field return | `1792755080588332-0` | `1792754721224865-0` |

Use the same observer command with `-WorkshopAction roundtrip`/`armor` in
zone42, `gems` in zone40, and `wear` in zone13, in each configuration.
Actual worn DP is12592/12600 in Debug and12598/12600 in Release; cancellation
preserves the worn inventory, confirmation restores12600, and the equipped
runtime DP refresh is checked. The different real hit/wear outcomes are not
normalized. The first concurrent Debug gem attempt timed out in shortcut
readiness and is discarded; the accepted isolated replay above passes without
changing data, state, tests or expected results.

Fresh `-Mode workshop-gate` / `-WorkshopAction none` runs in zones12 and42-47
verify outside-workshop rejection plus all six original workshop identities.
Their exact run IDs and the operation IDs above are collected in
`build\quality\workshop15-final-operation-audit.json`: all **26** accepted
sessions end normally, report no trace failure, and contain no error/critical
engine record. Console logs use `workshop15-final-{Debug,Release}-*`;
wear observer data retains its earlier `door135-*-zone13-gate-wear-held-*`
prefix. No evidence files or disc-derived captures are committed.

The earlier checkpoints below retain their independent source/limits. Full current
Debug/Release suites pass530 cases/1790939 assertions each; no C++ promotion,
compatibility retirement, exhaustive recipe coverage or broader #16/#76/#78/
#80/#81 acceptance is inferred from these operations. Publication and final
issue closure remain gated on exact-revision CI.

## Extended native targeting and planning storage (#136; #137)

Source: `813acea0` plus the scoped targeting/planning changes; unrelated owner
documentation, project and reference edits remain excluded. This is a native
runtime-capacity change, not an inventory expansion or an on-disc layout change.

The original eight-enemy city setup produces forty limb candidates. Candidate31
of the old23-entry preview array landed exactly on `Core_ScratchBase()` and
replaced the render cursor with a bone-position vector. The copied producer now
checks every append against the native255-entry index domain; `0xFF` remains
cancel. Action requests retain the same capacity. Allocation and clearing use
the actual native types rather than `0x2C10`/`0x84C`.

The visible sphere list still has24 slots. Registered owned F01296 keeps whole
actor groups together and provides L1/R1 pages only when necessary; the original
single-page behavior remains unchanged. Camera shoulder orbit is gated only
while extended target selection owns those buttons; Select zoom is preserved.
The compact `L1 n/n R1` hint is rendered in the actual game view. Structured Lab
frames retain all `target_previews` and expose `target_page` total/page/pages/
first/visible/selected metadata, with absolute selection indices.

During acceptance, the enlarged result clearer exposed the AI planner's fixed
7884-byte scratch allocation and offsets. It overwrote the cached original ZUD
header during actor4's AI initialization: battle-animation size17352 became0.
Later instances therefore had no battle sequence and stalled after real damage
while awaiting defeat animation. This coupled defect is tracked as #137.
Registered owned F01718 now owns a typed call-local action result, three
16-candidate passes, the16x256 exclusion table and16 fallback candidates.
Borrowed compatibility pointers are cleared before return. Existing AI views
remain tracked migration debt; this does not retire the broader MA-19/MA-21
contracts.

Fresh Debug/Release acceptance uses the actual native window/controller:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build\quality\equipment14-check.ps1 `
  -Configuration Debug -Mode workshop-gate -Zone 32 `
  -WorkshopAction city -Seed 42
```

Repeat with `-Configuration Release`. The ignored observer resolves PDB type
names rather than stale numeric record IDs, retaining its offset/size guards.
The scene is original MAP148/zone32 room1: Ashley8,16, Sarjik1 at8,17,
Bejart2 at7,17, and six original Crimson Blade4 definitions on the other
neighboring tiles. All-spells Ashley uses HP/MP/STR/INT/AGI999. No process writes,
forced HP/outcomes, injected loot, event-flag edits or stock-capacity changes
establish acceptance.

- Debug run `1792751783466397-0`: forty candidates, two whole-actor pages of20;
  R1/L1/R1 preserves camera yaw, cancellation clears the borrowed view, and
  re-entry starts on page1. Absolute preview20 commits actor6/limb0 and applies
  1287HP damage. All eight enemies then defeat normally, loot is claimed through
  the production menu, and gameplay returns at tick9259.
- Release run `1792751783218080-0`: the same paging/cancel/re-entry/absolute-index
  gates, actor6 takes1286HP damage, and all eight real defeats and original
  loot delivery pass; gameplay returns at tick9257. Different RNG
  consumption/outcomes are not normalized.
- Fresh inspected page2 captures: Debug `1792751680887233-0` and Release
  `1792751680777154-0`. Actual F10 while all nine actors remain alive in the
  forty-candidate selection returns normally to the editor in Debug
  `1792751765252629-0` and Release `1792751765143641-0`; native page bindings are
  explicitly reset at encounter exit. No same-preset editor restart is claimed.
- Both traces end normally with `trace_failed:false`, no error/critical engine
  records and14 pre-existing unported warnings each. Source paths, logs,
  observer JSON and captures remain under `build\quality\sphere136-*`,
  `build\quality\sphere136-137-*`, and each configuration's
  `combat-lab\runs` directory. Do not upload disc-derived evidence.
- Exact tested host SHA-256: Debug
  `3ec703c30332d078fefaad2c79ca3aad70eb145ec63eaf07635395c25e93d830`;
  Release
  `8f28ce860c2da2eff3d150232718d986db129fd4c408f6e5a58dc84006de3156`.
- Full Debug/Release suites pass **530 cases / 1790939 assertions** each after
  building the test and Combat Lab projects with the documented MSBuild x64
  commands and `/p:VcpkgManifestInstall=false`. Regressions cover native result
  clear boundaries, adjacent AI buffers, complete-actor pages, absolute indices,
  cancel/reset, shoulder ownership and direct label rendering.
- `tools\port\test_encounter_fixups.py`: **24 cases pass**. Second generation
  retains all **751 outputs / 93 registered owned files** byte-for-byte.
  Game-only project generation is stable;13 other solution/project files retain
  their pre-generation hashes. Temporary cursor/spawn instrumentation is absent.
  Scoped maintained quality/tidy passes with0 native findings and0 new
  findings; the scaffold's one existing SIM102 advisory remains unchanged.
  Publication and exact pushed quality evidence are recorded on #136/#137.

This does not close #78/#80/#81/#76 by association, nor prove #15's remaining
live blade/shield combination transactions. The lawful seed42 runs supplied
both guaranteed original boss blades and two/zero loose shields in
Debug/Release; no combination is claimed from acquiring those components.

## Native door-query storage and real repair acceptance (#135; #134)

Checkpoint source is `deb6c5b88d9ffa90e71d875fba715821c8da52cc` with the
registered owned door query uncommitted; unrelated owner edits remain
excluded. The placement-scope repair in that source has successful exact
Source Quality39917867716. This checkpoint completes the previously blocked
Debug/Release live route, pending scoped publication and exact-revision CI.

#135 confirms an independent allocation defect: generated T00232 used
`records[0]`, and G00330's compiled native object was only8 bytes. F00579 wrote
matching pointers beyond that object. Its red native regression fails both
storage assertions without deliberately executing an out-of-bounds write.
Read-only original MPD inspection finds at most8 doors per room and2 eligible
matches per query; the repair does not impose either as an arbitrary cap.

The registered owned implementation is `Field/DoorMatches.cpp` with its
C-compatible native view in `DoorMatches.h`. A native vector owns the pointer
list; returned record pointers borrow current-room records until the next
query/room unload. Original matching order, exact tile/layer rules, platform
layer matching and one raw RNG update are preserved. Empty/absent-room
queries reset the view safely, with no RNG draw for absent rooms. Partial
record lengths explicitly log an error and reject the query. No on-disc
layout, inventory capacity or story flag is changed.

Three `[field][doors][storage]` cases pass122 assertions in both full native
suites: allocated type/view, ordered single/multiple matches, repeated/empty
queries,40-record platform lists, raw RNG/opaque-byte behavior, absent rooms
and malformed lengths. Full Debug/Release each pass522 cases/1,790,782
assertions. The generator regression verifies owned F00579/G00330/T00232
registration, consumer includes and absence of regenerated implementations.
All22 copier tests pass; second generation is stable across751 outputs with
89 owned source/header files preserved.

Final maintained quality/tidy reports1 existing scaffold tooling finding and
0 new; native findings are0. The existing `gen_solution.py` SIM102 advisory is
not changed or suppressed.112 intentional fixture literals are reported
separately. The initial new native literal-index finding was corrected with
a named door-query random-byte index; no baseline was refreshed.

Only the clean Game project/filter files were regenerated through the
existing scaffold's `files_of`/`vcxproj`/`filters` functions. Its normal
`needs_c_headers` selection now includes the owned implementation. The full
solution generator was deliberately not run over the owner's dirty solution
and tool projects; the solution's pre/post SHA256 is unchanged. No project
file was hand-edited. A second Game-only generation is stable, and
`door135-scaffold-audit.json` also checks the owner's tool project/filter
hashes remain unchanged.

Rebuilt x64 Combat Lab host SHA256:

- Debug: `4f47064e2303aa7d4eb0ebd18c71de7f7db4ca9d7e2ea0c3ba52550f1abf17bd`.
- Release: `b879154fb534fd33018b3a84bba8560354b05388018eba1d008cc3c8125b73b9`.

| Real combat-to-workshop repair | Debug | Release |
|---|---|---|
| Accepted run | `1792741749151138-0` | `1792741827685837-0` |
| Actual worn weapon DP before transition | 12598/12600 | 12598/12600 |
| DP after original repair confirmation | 12600/12600 | 12600/12600 |
| Final field return tick | 4184 | 4168 |

Each run makes a real low-STR melee attack against original Zombie Knight18
in zone13 room12, defeats it through Fireball and claims normal loot. It
stows the weapon, opens the original door with Cross, walks into workshop42
and retains all3840 inventory bytes across the ZND change. The destination
log says `using original placements zone=42 room=0`, not source-enemy injection.
Repair cancellation preserves the worn inventory. Confirmation restores
actual DP, applies the original all-part PP calculation, refreshes equipped
runtime DP and returns to field with the exact expected inventory. No F10,
memory writes, forced HP/DP, event flags or synthesized loot establish this.

The formerly blocked Release doorway now passes this same bounded route
with owned storage, including a preceding successful Release pass before the
final pair. This verifies the reported route, not every campaign door.
An initial route probe used the wrong camera-relative direction; another
used compact menu row1 instead of original Items ID3. These observer errors
were corrected rather than logged as gameplay defects.

Commands use the existing ignored controller/read-only harness:

```powershell
build\Debug\VS.Tests.exe "[doors][storage]"
build\Debug\VS.Tests.exe
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 13 -WorkshopAction wear
# Repeat builds/tests/live commands in Release x64.
.\.venv-3\Scripts\python.exe tools\port\copy_to_native.py
.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py
. .venv-3\Scripts\Activate.ps1
.\quality-native.cmd --files Native\Game\src\Field\DoorMatches.h `
  Native\Game\src\Field\DoorMatches.cpp Native\Tests\src\EncounterTests.cpp `
  tools\port\test_encounter_fixups.py tools\scaffold\gen_solution.py --tidy
```

Local `build\quality\door135-live-audit.json` verifies the final two sessions:
normal `session.end`, no error-level engine records or trace failure, original
destination placement selection, actual worn/restored DP and field menu63.
Build/test, original-capacity, ownership/generation and controller evidence
remain under `build\quality\door135-*`; captures/assets are not committed.
F00579/G00330/T00232 locations and bounded evidence are updated separately
from promotion/compatibility-retirement status. DoorIds and broader typed
room ownership remain migration work, not claims of this repair.

The real worn-equipment repair gate for #15 is now demonstrated. Live
blade/shield combination consumption still remains; #15 is not closed by
this checkpoint.

## Cross-zone placement repair checkpoint (#134)

The real wear-to-workshop route reproduced a separate Lab crash on
`799d88545f40cef1f53fafaeff6aa07f0dcf3104` with the unchanged #133 hosts.
Debug run `1792738861766246-0` uses original zone13 room12, Zombie Knight18,
Ashley tile14,13 and enemy14,14; HP/MP999, STR1, INT999, AGI200, seed42.
A real melee attack wears the weapon, Fireball defeats the enemy and normal
loot is claimed. Camera-relative movement and Cross open the original door
to workshop42. At tick2261, after ZONE042/MAP205 load, the Lab reinserts the
source enemy18. `Field_SpawnRoomActors` faults at `RoomLoading.c:396`, reading
0x4F0A through the destination's empty character table.

The native placement hook now injects configured enemies only when both
current zone and room match their configured source. Other rooms retain
their supplied original placements, with an explicit log; inactive sessions
remain untouched. Readiness is invalidated on every active room load and
restored through the existing post-load path. This does not disable real
destination enemies or manufacture empty rooms.

Two focused `[placements]` regressions pass19 assertions in Debug/Release:
source, changed-zone, changed-room, both changed and source re-entry for
solo/one/eight enemies; inactive original-pointer/size and empty preservation.
Full suites pass519 cases/1,790,660 assertions each. Scoped maintained
quality/tidy is0 findings/0 new, with88 intentional fixture literals listed
separately. No baseline changed. Rebuilt host SHA256:

- Debug: `f38364c278b27a1d26a74de0da7454d2ca3f981ada300ef54a1dc0ffbe545191`.
- Release: `29cfe7659fad9f39bc932f419d0d90173e4423ba4f80e8337498266a36c2b91b`.

Debug `1792739660623754-0` now carries actual weapon DP12598/12600 through the
door, cancels repair without mutation, confirms repair to12600, refreshes
equipped runtime DP and returns to field with the exact expected inventory.
The initially wrong compact/root menu index was corrected to original Items
ID3; the weapon must be stowed to use equipment management. Updated PDB type
indices preserve all existing compiled offset checks; no guard was removed.

**Final live acceptance remains open.** Release reaches the opened doorway
but can stop at1856,0,1980 rather than transitioning, including a sustained
controller hold in `1792739974174070-0`. This is not bypassed or counted as a
successful transition. Investigation identified a separately confirmed
unallocated `Field_DoorMatchList.records[0]` native tail, logged as #135;
its relationship to this Release observation is not yet established.

Evidence is local under `build\quality\placement134-*`, plus the pre-fix
`workshop15-Debug-zone13-gate-wear-toward-door-*` scenario/run pointer.
Use the existing ignored harness with `-Mode workshop-gate -Zone 13
-WorkshopAction wear`, after rebuilding Tests and CombatLab in the selected
x64 configuration as above. Maintained checks:

```powershell
build\Debug\VS.Tests.exe "[placements]"
build\Debug\VS.Tests.exe
. .venv-3\Scripts\Activate.ps1
.\quality-native.cmd --files Native\Game\src\Field\EncounterSpawnProfile.hpp `
  Native\Game\src\Field\EncounterCatalog.cpp Native\Game\src\Boot\Encounter.cpp `
  Native\Tests\src\EncounterTests.cpp --tidy
```

This is a scoped safety checkpoint, not closure of #134/#135/#15, C++ promotion
or compatibility retirement. No process-memory writes, forced HP/DP, scene
flags, F10 inventory resets or copied disc-derived artifacts establish it.

## Workshop operations checkpoint (#15)

Source is `6df50baa79bc7e77e09c6fcaed57005b03e3a8e6` with new maintained
workshop regressions uncommitted. Production hosts are unchanged from the
#133 hashes below. Unrelated owner edits are present and excluded. No
additional production repair, inventory expansion or process-memory write
was needed for these operations.

| Actual-controller operation | Debug run | Release run |
|---|---|---|
| Armor preview, two cancellation cycles, confirmed combination and field return | `1792736280778761-0` | `1792736381577351-0` |
| Original gem-bearing loot, repeated detach/rollback/confirm/reattach | `1792736537534046-0` | `1792736675536155-0` |
| Stock repair cancellation/confirmation and field return | `1792737561023068-0` | `1792737599664594-0` |

The armor transaction uses the two original equipped leather Bandage pieces
(ID191). Original `MENU/ARMOR.SYD` maps the ordered pair65/65 and leather/
leather to subID65/material2. Preview preserves all inventory bytes. Both
cancel/undo cycles preserve the originals. Confirmation consumes both source
slots, writes the preview result to the first with the correct index, clears
the second while retaining its index, and unequips both original limbs.
All3840 inventory bytes match the expected transaction through field return.

The gem runs select original zone40 Grissom (definition1), preserving his
original weapon, rates and MPD spawn loot. A real Fireball defeats him; the
normal loot screen delivers weapon slot2 and its attached gem263. The
guaranteed misc entries440/451 and optional347 are independently checked.
Outside a workshop, two cycles verify staged gem removal, cancellation
rollback, confirmed removal and confirmed reattachment to the original
weapon. Byte-exact checks include the gem's owner2/free transitions; final
inventory equals the post-loot original. Assemble/Repair/Combine still reject
confirmation outside the workshop. This is real gem use, not injected loot.

Stock repair runs exercise two cancel/confirm cycles, preserve inventory,
refresh matching derived equipment values and return to field. Stock gear
starts at full DP: these runs are **not** evidence of live durability
restoration. Meaningful restoration is demonstrated separately by the
production repair-state-machine regression below.

Six additional inspected captures show readable armor result/stats, gem
slots/components and the original repair Yes/No prompt:

| Menu preview | Debug run | Release run |
|---|---|---|
| Armor combination | `1792737777255146-0` | `1792737877218440-0` |
| Gem attachment after removal | `1792737809478904-0` | `1792737909200525-0` |
| Repair confirmation | `1792737857022713-0` | `1792737956512786-0` |

Maintained component coverage adds twelve cases to the existing two workshop
cases:

- Repair:28 combinations of missing DP0/1/99/100/101/9999/10000 and PP0/1/50/200.
  The production confirmation state machine rejects Cross without mutation,
  then restores blade/shield/armor DP, spends integer `missingDp / 100` PP
  clamped at zero, excludes accessories, preserves every other inventory byte,
  and repeats idempotently. Six equipped cases additionally verify refreshed
  runtime weapon/shield/armor DP, weapon/shield PP and retained equipment links.
- Assembly/disassembly: zero through three gem slots, repeated assembly and
  disassembly at a non-first weapon slot, exact inventory/part preservation,
  component ownership, cleared names and gem-owner release. Shield gem
  detachment covers all four slot counts without destroying the shield.
- Recipe builders: asymmetric synthetic ordered ID/material tables, same and
  different materials, signed base/material modifiers, packed damage/cost
  fields, model/category/range, averaged maximum DP/PP, current PP, untouched
  sources and retained preview metadata. Armor intentionally retains PP from
  the first source; blade/shield average it, matching original MENUC.
- Original local `BLADE.SYD`, `SHIELD.SYD` and `ARMOR.SYD` representative pairs
  additionally produce the independently inspected IDs13/134/192 and
  materials4/5/3. Modifier signs are controlled synthetic fixture values;
  this does not claim exhaustive original recipe/content coverage.
- Class/affinity extrema, signed truncation, upper/lower bounds, six versus
  seven entries; physical dominant-type/material factors and untouched entry0.
- All147 combinations of three selectors, workshop0..6 and material1..7,
  plus exclusion of an already selected ingredient and accessory armor.
  These are original masks/filtering rules, not a new authoring format.

Final Debug/Release `[workshop]` selections each pass14 cases/5,108 assertions;
full suites pass517 cases/1,790,641 assertions each. Scoped maintained
quality/tidy reports0 findings/0 new, with191 intentional fixture literals
reported separately. No suppression or baseline changed. No generator changed
in this checkpoint, so the #133 stable751-output/87-owned audit still applies.

Initial synthetic repair testing faulted on prompt feedback because its sound
bank selectors were unbound. The fixture now follows the existing empty-slot
menu test pattern and restores those bindings; this was not a gameplay defect.
Blade tests also now check the stored signed2/3-bit representations by masks,
not an incorrect positive signed value. Neither required a production change.

Commands, repeated for Release:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Native\Tests\VS.Tests.vcxproj /m /nodeReuse:false `
  /p:Configuration=Debug /p:Platform=x64 /p:VcpkgManifestInstall=false
build\Debug\VS.Tests.exe "[workshop]"
build\Debug\VS.Tests.exe
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 42 -WorkshopAction armor
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 40 -WorkshopAction gems
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 42 -WorkshopAction repair
# Add -CaptureWorkshop to stop at the inspected menu preview.
. .venv-3\Scripts\Activate.ps1
.\quality-native.cmd --files Native\Tests\src\CursorMemoryTests.cpp --tidy
```

Local evidence is `build\quality\workshop15-*`, including scenarios, controller
traces, before/after bytes, original-data inspection, build/test/quality logs
and captures. `workshop15-operations-audit.json` verifies all twelve accepted
sessions end normally with no error-level engine records or trace failures.
No disc-derived captures or assets are committed.

#15 remains open: these results do not replace live worn-equipment repair or
live blade/shield combination consumption. The native rule/repair tests are
substantive component evidence, not substitutes mislabeled as those live
transactions. The next observation is a lawful retained-inventory route from
real combat wear/loot into a workshop; F10 starts a fresh encounter and cannot
satisfy that route. There is no C++ promotion or compatibility-retirement claim.

## Weapon naming rendering (#133)

The owner reported a background-only screen after confirming workshop
assembly. Reproduced on `cd2f911f9b88e88e5d87b500c43d47493d611574`: the
Debug controller capture at tick2866 contains only the original background.
MENU8 state and successful name/inventory writes were not evidence that the
editor rendered. This checkpoint adds explicit visual acceptance.

Original `MENU8.PRG/21A0.c` builds the command gradients, name/grid backdrop
and selection highlight using linked packets. Generated F02781/F02782 still
truncated native pointers to raw24-bit addresses. The copy generator now uses
the existing registered `LegacyGpu_Handle` links for those two producers.
Original strings, grid layout, animation, input, confirmation and name rules
are unchanged. No reference source or generated output was manually patched.

The new production renderer regression fails14 of26 assertions before repair:
no131 synthetic command/name/grid glyphs,48 command-gradient packets or4/5
backdrop packets are reachable, and the selection highlight breaks the lower
UI chain. Fixed Debug/Release pass30 assertions in two cases, including
offsets0/32/320, native addresses above32 bits, complete lower-chain traversal
and allocation guards. The two generated-source checks also fail before
repair and pass afterward.

Rebuilt Lab hosts, with the repair uncommitted on cd2f911f and unrelated owner
edits present, have SHA256:

- Debug: `92eacff6490e03c762bc3d03f3b19f8e564b595f8898a3bef537801c67f9dad9`.
- Release: `5f049e1e7c0c0f40c6bcfac2e384928cced96ac2963700a07cbaece7599d9d52`.

The following actual-controller captures were inspected, not inferred from
menu state. All show readable command labels, name field, full character grid
and selection cursor/highlight. Edited captures change Fandango to Aandango
in the buffer and move the grid selection down/right before capture.

| Rendered screen | Debug run | Release run |
|---|---|---|
| Assembly naming | `1792734975110326-0` | `1792735055683383-0` |
| Explicit Setup Rename | `1792735031754241-0` | `1792735112015359-0` |
| Edited name and moved selection | `1792735659331107-0` | `1792735685545071-0` |

Repeated complete transactions also pass: Debug `1792735242277701-0` and
Release `1792735466351908-0`. Each performs two cycles of disassembly
confirmation cancellation, actual disassembly of the equipped stock weapon,
provisional assembly cancellation, confirmed assembly through naming,
explicit rename cancellation, typed-name confirmation and re-equipping.
Empty assembly naming correctly assigns the blade's default Scimitar name;
explicit naming restores the original Fandango name. Component properties,
ownership and expected inventory bytes are checked at every transaction.
All3840 inventory bytes equal the initial state on final field return
(Debug tick12908; Release tick12854); copied selector lists are released.

The first rename-confirmation observer returned early because it consumed
queued frames preceding the latest input during fast typing. The final
round-trip observer acknowledges the actual pressed-button frame before
settling. This was a harness timing error, not a naming-input repair.
No process-memory writes, forced gameplay state or inventory expansion
establish acceptance.

Commands, repeated in Debug and Release x64:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Native\Tests\VS.Tests.vcxproj /m /nodeReuse:false `
  /p:Configuration=Debug /p:Platform=x64 /p:VcpkgManifestInstall=false
build\Debug\VS.Tests.exe "[rename]"
build\Debug\VS.Tests.exe
# Build Native\Tools\CombatLab\VS.CombatLab.vcxproj with the same settings.
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 42 -WorkshopAction roundtrip -CaptureNaming assembly
# Repeat with -CaptureNaming rename and -CaptureNaming edited.
build\quality\equipment14-check.ps1 -Configuration Debug -Mode workshop-gate `
  -Zone 42 -WorkshopAction roundtrip
.\.venv-3\Scripts\python.exe tools\port\copy_to_native.py
.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py
. .venv-3\Scripts\Activate.ps1
.\quality-native.cmd --files Native\Tests\src\EquipmentDetailTests.cpp `
  tools\port\copy_fixups.py tools\port\test_encounter_fixups.py --tidy
```

Final full native suites pass505 cases/1,786,663 assertions each. All21 copier
tests pass; second generation is stable across751 outputs with87 owned
source/header files preserved. Maintained quality/tidy is0 findings/0 new,
with60 intentional fixture literals reported separately; no baseline changed.
The eight accepted sessions have normal final records, zero error-level
engine entries and no trace failures. Evidence and captures stay local under
`build\quality\naming133-*`; `naming133-final-audit.json` additionally identifies
the two `workshop15-*-zone42-gate-roundtrip` sessions. The pre-fix capture is
`naming133-Debug-red-assembly.png`.

This establishes the bounded #133 rendering/editing acceptance and stock
assembly/rename round trips. #15 remains open for gem attachment/removal,
meaningful durability repair and representative material/result combination
transactions. This is not C++ promotion or compatibility retirement.

## Workshop gates and native selector storage (#15; #132)

Checkpoint source is `b45dfd3c5468adac115fdbff5f3c48cd31a5337d` with the
six-selector generator repair and two maintained workshop regressions
uncommitted. Unrelated owner edits remain excluded. The rebuilt x64 Lab
host SHA256 values are:

- Debug: `fec120654324f08620b01df4145686a90733bde0371e526b60344c1d74d6fea6`.
- Release: `82c759c2089560236dd5279a5d59e81a70efd3570f325fe230fe4453da331cd8`.

Original MENUC Setup logic and MPD environment records establish the location
gate: Assemble, Repair and Combine require a workshop; Gems, Disassemble and
Rename do not. Eight assembled weapons additionally disable Assemble, and
zero weapons disable Rename. The workshop value is an ID1..6, not a boolean;
its material masks are0x0E/0x18/0x38/0xC0/0x3E/0xFE.

The maintained gate regression drives production row construction for
workshop0..6, weapon counts0/1/7/8 and both cursor-memory settings:
56 combinations/728 assertions. The original stock inventory is not expanded.
Actual-controller gate runs use original room0 in each workshop:

| Zone / MPD / workshop | Supported tile x,z | Debug run | Release run |
|---|---|---|---|
| 42 / MAP205 / 1 | 3,4 | `1792731828447323-0` | `1792732217645121-0` |
| 43 / MAP206 / 2 | 3,4 | `1792731892746478-0` | `1792732238555652-0` |
| 44 / MAP207 / 3 | 5,2 | `1792732106334829-0` | `1792732259425243-0` |
| 45 / MAP208 / 4 | 4,3 | `1792732127312756-0` | `1792732280255764-0` |
| 46 / MAP209 / 5 | 4,3 | `1792732148284289-0` | `1792732301141392-0` |
| 47 / MAP210 / 6 | 3,5 | `1792732169167698-0` | `1792732322213352-0` |
| 12 / outside workshop | Existing solo preset | `1792731800597321-0` | `1792732189922137-0` |

These gate-only runs precede the selector repair. Outside the workshop,
confirmation of all three unavailable rows is rejected without inventory
changes. All runs return to field. Zone44's initial rejected tile5,3 had no
complete floor support; original collision data identified supported tile5,2.
This placement rejection was not a gameplay crash.

#132 was logged before repair. Six selectors embedded two native pointers per
row in unaligned PS1-sized temporary text allocations. At full legal capacity,
blade/grip/armor tables overran by136 bytes, shields by72 and gems by392.
The first396 row/content/inventory assertions passed despite corruption.
Adding a byte guard over the allocator's reserved final header produced
six failures, one for each production selector, out of402 assertions.
All six generated-source checks also failed before repair.

The copy generator now gives F02886/F02887/F02888/F02903/F02909/F02913 local
`char*` arrays of34/34/98/34/18/34 entries. Original text allocations,
capacities, component filtering, material rules and free calls are unchanged.
`Menu_Setup_BuildItemList` synchronously copies both strings into owned rows,
so it retains no pointer to these local arrays. Repaired full-capacity tests
pass402 assertions: counts, slot order, copied text, disabled placeholder,
enabled components, byte-exact inventory preservation and heap-tail guard.

Rebuilt hosts additionally pass first-workshop entry/return (Debug
`1792733485635319-0`, Release `1792733506585846-0`) and repeated real chooser
browsing (Debug `1792733764298314-0`, Release `1792733836872489-0`).
Each browse run opens/cancels blade, grip and armor-combination choosers twice.
The stock assembled weapon leaves only the disabled empty blade/grip row;
confirmation rejects it. Armor exposes six rows including the placeholder.
Every cancellation releases the copied list, restores Setup and preserves
inventory; both runs return to field at tick3827 with a null list pointer.
The observer was corrected to select the absolute combination category:
relative Down presses selected the wrong category on repeat entry because
cursor memory retained the previous category. No production change was needed.

Commands (repeat the build and live commands for Release):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Native\Tests\VS.Tests.vcxproj /m /nodeReuse:false `
  /p:Configuration=Debug /p:Platform=x64 /p:VcpkgManifestInstall=false
build\Debug\VS.Tests.exe "[menu][equipment]"
build\Debug\VS.Tests.exe
build\quality\equipment14-check.ps1 -Configuration Debug `
  -Mode workshop-gate -Zone 42 -WorkshopAction browse
.\.venv-3\Scripts\python.exe tools\port\copy_to_native.py
.\.venv-3\Scripts\python.exe tools\port\test_encounter_fixups.py
. .venv-3\Scripts\Activate.ps1
.\quality-native.cmd --files Native\Tests\src\CursorMemoryTests.cpp `
  tools\port\copy_fixups.py tools\port\test_encounter_fixups.py --tidy
```

Final Debug/Release full suites each pass503 cases/1,786,633 assertions after
the fixture's final type/naming edits. The preceding focused equipment suites
pass9 cases/1,910 assertions. Copier tests pass20 cases; a second generation
preserves all751 output hashes and all87 owned source/header files.
Maintained quality/tidy reports0 findings/0 new, with77 intentional fixture
literals listed separately. No baseline or suppression changed.

Evidence remains local under `build\quality\workshop15-*` and
`build\quality\workshop132-*`. `workshop132-final-audit.json` checks all18
accepted gate/browse sessions: normal `session.end`, zero error-level engine
records and no trace failure. Red/green tests, generation audit, quality,
scenarios, run pointers and read-only observations are retained there.
No process-memory writes establish acceptance; no captured gameplay crash
is claimed for #132.

This checkpoint verifies bounded native selector storage and workshop access,
not #15's modification transactions, exhaustive material/result tables,
C++ promotion or compatibility retirement. Stock Fandango has full DP and
a zero-gem-slot grip: a no-op repair or empty gem menu would not establish
durability restoration or gem-attachment acceptance. Those remain #15 work.

## Equipment-menu acceptance (#14)

Final acceptance uses `add47793bd3820b8d2859a6338a4dc9f8fca8e37` with the
new restriction regression uncommitted. The Combat Lab hosts are unchanged:
their SHA256 values match the #131 checkpoint above. Unrelated owner edits
remain present and are excluded from the acceptance commit. No further
production repair was required after #130 and #131.

| Acceptance | Debug run | Release run |
|---|---|---|
| All loose stock categories, cancel/NONE, restoration and derived values | `1792730072518340-0` | `1792730197982907-0` |
| One-handed weapon/shield repeated swaps and derived values | `1792730991139942-0` | `1792731069056081-0` |
| Two-handed weapon/shield exclusive swaps and derived values | `1792731146211280-0` | `1792731210087534-0` |
| Original prologue main-menu restriction | `1792730872129654-0` | `1792730902503424-0` |
| Actual Equipment Down management restriction | `1792730883257330-0` | `1792730913153035-0` |

The category matrix removes all seven stock pieces before checking all eight
equipment slots, so incompatible loose categories are present simultaneously.
Both arm slots offer both arm pieces; head, body, legs and accessory offer
only their matching category. The empty shield slot rejects NONE. Cancel
does not mutate inventory; re-equipping restores the exact original instances
and returns to field at tick7007. Earlier #131 stock cycles additionally
verify each individual unequip/cancel/NONE/re-equip round trip.

Every matrix/replay snapshot checks the nine equipment contribution blocks
(weapon, shield, accessory and six limbs), current/base STR/INT/AGI where
applicable, and the packed attack range. Fandango's current10/1/-4 becomes
0/0/0 when removed and returns to10/1/-4 when restored; its unsigned base
agility representation is65532. Accessory INT contributes1 independently.
Actor `totalStrength` is not an equipment-inclusive total: damage calculation
adds the separate equipment values. The original `vs_battle_nop0` callback
is intentionally empty, not an unimplemented recalculation.

The shield replays repeat both swap directions twice. One-handed equipment
keeps the shield; greatsword and shield selections exclude one another.
Source/preview model identities, material/texture headers, native slot stride,
instance ownership and properties remain intact. Field return releases
preview slots2/3 without losing the surviving source models.

The original prologue profile rejects Triangle without changing inventory.
In the all-spells profile, F4 eight times and F5 apply the actual Equipment
Down mask0x80000 through the existing Lab control. Both management rows are
disabled; confirmation is rejected; cancellation returns to field with the
same inventory. The effect remains active at every observed step. The new
maintained `[menu][equipment][restrictions]` test exercises production
`Menu_Inventory_UpdateItemList` initialization and update for80 combinations:
intro stages0..4, Tarnish absent/present, and restriction masks
0/1/2/4/7/8/0x80/0x200. Both Equip and Setup rows follow the original
`intro < 3 || tarnish || (restrictions & 7)` rule.

Two rejected harness assumptions were corrected, not treated as game bugs:
the closed-menu sentinel is63, not0; and reading a fixed number of queued
frames after F5 could still observe a frame preceding application. The final
observer waits for the actual `control.adjust` event and a subsequent frame.
No process-memory writes or changed gameplay flags establish acceptance.

Commands use the existing ignored harness at speed1. Run each in Debug and
Release:

```powershell
build\quality\equipment14-check.ps1 -Configuration Debug -Mode category-matrix
build\quality\equipment14-check.ps1 -Configuration Debug `
  -Mode restriction-gate -Gate prologue
build\quality\equipment14-check.ps1 -Configuration Debug `
  -Mode restriction-gate -Gate tarnish
```

Run `-Mode replay-shield` with the following existing recorded-controller
inputs, supplying the absolute `build\quality\` prefix to `-ReplayEvidence`:

| Configuration | Weapon | Seed | Evidence prefix |
|---|---|---|---|
| Debug | default | 5 | `equipment131-Debug-default-loot-shield-seed5` |
| Release | default | 10 | `equipment131-Release-default-loot-shield-seed10` |
| Debug | greatsword | 42 | `equipment14-Debug-greatsword-loot-shield-seed42` |
| Release | greatsword | 4 | `equipment14-Release-greatsword-loot-shield-seed4` |

Final validation:

- Rebuild `Native\Tests\VS.Tests.vcxproj` in Debug/Release x64 using MSBuild,
  `/m /nodeReuse:false /p:VcpkgManifestInstall=false`; execute each resulting
  `build\<configuration>\VS.Tests.exe`. Each full suite passes501 cases and
  1,785,503 assertions, including480 new gate assertions. This also exercises
  shared cursor-fixture allocation and global-state cleanup.
- After activating `.venv-3`, run
  `quality-native.cmd --files Native\Tests\src\CursorMemoryTests.cpp --tidy`:
  0 findings/0 new. Initial line-length/format findings were fixed; no baseline
  or suppression changed.
- All ten accepted sessions end with `session.end`, zero error-level engine
  records and no trace failures. Audit: `build\quality\equipment14-final-audit.json`.
  Console logs, scenarios, run pointers and observations are retained under
  `build\quality\equipment14-final-*`; quality evidence is
  `equipment14-gates-quality-report.json`.

This establishes the bounded #14 criteria, not exhaustive equipment-content
coverage, C++ promotion, compatibility retirement, the separate comparison
presentation request #112, or workshop modification #15. Accelerated-audio
#83 and particle parity #76 remain separate.

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
Positive matching-key/sigil door use was an additional owner-requested
follow-up, now verified in the [#139 checkpoint](#original-key-and-sigil-lock-acceptance-139-supporting-16)
above. These earlier checks do not establish #82's root cause, all learned
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

Post-boss game-clear MENUF also exposes optional `roulette` data: entry phase,
screen state/timer, animation speed, reel index, selection byte and full BIOS
random state. It is absent while inspection is unavailable, for other MENUF
subtypes and after the results menu closes. Sampling neither advances RNG nor
changes UI/award state. Compare these raw per-frame counters with the host tick
and actual controller edges when auditing stop cadence; do not infer logic-call
timing from presentation FPS alone.

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
