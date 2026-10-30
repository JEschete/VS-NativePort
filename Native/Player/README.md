# External test player

`Player` is a separate Windows process, not game AI or a replacement for
production combat. It uses a local model for tactical planning and a fast,
source-derived reactor for chains/defenses. It controls only its own frozen
game executable through ordinary keyboard messages. It never writes process
memory, HP, flags, positions, loadouts, outcomes or event completion directly.

## Run

From the repository root, using the existing Python 3.12+ environment:

```powershell
.\.venv-3\Scripts\python.exe Native\Player\player.py bootstrap
.\.venv-3\Scripts\python.exe Native\Player\player.py doctor

.\.venv-3\Scripts\python.exe Native\Player\player.py play `
  --game-exe build\Debug\VS.CombatLab.exe --data Native\data `
  --scenario <declared-lab-preset.json> --frames 3600 `
  --require-chains 1 --require-defenses 1

.\.venv-3\Scripts\python.exe Native\Player\player.py play `
  --game-exe build\Debug\VS.Desktop.exe --data Native\data `
  --save-file <unchanged-native-save.vssave> --frames 6000 `
  --door-case z009-r017-d002

.\.venv-3\Scripts\python.exe Native\Player\player.py play `
  --game-exe build\Release\VS.Desktop.exe --data Native\data `
  --new-game --frames 18000
```

Release uses the corresponding built executable/PDB. The tool copies the
selected image, matching PDB and DLLs into a private run, verifies hashes, and
starts one owned window. Do not mix configurations or rebuild over frozen
evidence. Desktop uses normal Start/Continue/menu inputs with a byte-identical
private copy of the explicitly selected save. The source save is never edited.
Alternatively, explicit `--new-game` selects the original New Game in an empty
private save directory, preserving original equipment, learning and progression.
These source modes are mutually exclusive. Startup sends Start only during the
opening title movie and Circle to choose New Game; it sends no later story-scene
skip. Ordinary dialogue, combat and original scene producers then remain in
control. Starting directly in a later Lab room does not establish inherited
music banks or other predecessor state.
Desktop tick-limit observation is not claimed to finish a campaign or execute
full in-game shutdown; its host/trace completion is reported separately.

Lab scenarios that declare `ashley_stats` are rejected unless
`--allow-stat-overrides` is passed. That owner-approved assist is recorded in
the run metadata and result, and a passing assisted run is not stored as
learned matchup experience. It gets the route through hard rooms; it is not
evidence that an unassisted Ashley can win those fights.

Lab scenarios that declare `story_progress` (campaign script flag `0x82`) are
likewise rejected unless `--allow-story-progress` is passed. The declared value
is recorded in the run metadata and result, and a passing run is not stored as
learned experience. Use it only when original placements require campaign
progress that a direct Lab start cannot earn. For example, MAP256's
Neesa/Tieger placements need progress 58. The progress is declared fixture
state, not earned progression.

With a `--door-route`, `--claim-room-chests` first navigates to the source
room's unlocked original chest. Ashley approaches from its authored facing,
turns until the chest is on her front tile and presses Cross; the native
loot screen is then claimed with the normal loot policy. Locked, opened or
empty chests are skipped. A chest that stays unopened after three presses, or
after 24 turn inputs, blocks the route with an explicit reason; the doors are
not attempted. When a `--scene-expectation` is also set, accepting the route
does not end the run. The Player keeps its normal combat policy in the
destination room, so original encounters after a boss intro can be fought
before the scene contract is checked.

With a scene expectation, the final route leg is also accepted when an
original event takes control immediately after the observed destination room
load (an active event phase with Ashley alive). Its `scene_handoff_tick` is
recorded, and the native scene contract remains the completion gate. A living
hostile that an original event removes from its room while the event owns
playback is recorded under `scripted_withdrawals`; it does not hold
`--clear-enemies-first` open, and it is never counted as a defeat.

For a bounded production-room scene investigation, optional
`--scene-expectation <source-scene.json>` embeds the existing Combat Lab
`events`, `event_source`, `return_zone`, `return_room`, `preconditions` and
optional `confirm_dialogue` contract in the owned native sequence. Its original
VM/control observer ends the case after real playback and120 consecutive
living-control ticks, rather than leaving the model in a completed room until
the frame budget or closing its window to manufacture completion. The exact
expectation is copied into the run and audit. Acceptance requires its matching
native scene pass, measured control interval, finalized trace and actual
teardown; a cancelled case, budget exit or generic pass cannot substitute.
Default combat, Desktop and physical-door behavior is unchanged. This option
requires an explicit Lab production-room fixture; it does not earn campaign
progression or replace per-definition original-byte/branch verification.
This option supports the field-control subset only. The native Lab's separate
`terminal: ending_to_title` goal is not a player completion contract and is
explicitly rejected here; it cannot bypass the player's existing120-tick gate.

For one original room-local spatial producer, add `--scene-target-tile X Z`.
This opt-in requires exactly one source2 event expectation and is exclusive
with physical-door objectives. The existing terrain navigator plans from the
actual post-entry player position, current camera and observed collision
geometry, rather than assuming entry scripts preserve the fixture position.
If Ashley is already on the predicate tile, normal movement first leaves it
and then returns to earn the required tile change. Native scene/control
ownership invalidates pending routes. Movement, route arrival and the target
coordinate never establish a pass: the actual native source/selector must
activate and the unchanged playback/control/teardown contract must finish.
Only this opt-in goal may try supported observed patch frontiers after the
nearest-progress endpoints are exhausted; normal door/combat navigation is
unchanged. `--clear-enemies-first` also gates this voluntary scene approach on
actual observed source-room deaths, not actor disappearance. Strategic
preparations cannot start while a native cinematic owns input; event-owned
reward selection remains interactive and transaction timeouts are unchanged.
The option changes no game state or core-game policy; default play is unchanged.

When caster debuffs are available, the model also receives otherwise-legal
self buffs such as Magic Ward in the same constrained choice set. Debuff
availability must not hide a learned, affordable ward. Existing restriction,
status, failed-action feedback and MP-reserve checks still apply; urgent
healing and useful defensive-loadout preparation retain their priority.
Exposing a choice is not proof that the spell was selected or took effect.
Source-verified HP-damage spells (actions97–103 and106–130) are offered only
when learned, unrestricted, affordable and targeted at an observed hostile.
Actions104–105 are excluded from this set because their native hit effect is
not HP damage. Blocked or no-effect feedback suppresses repeating a cast at
that target; verify actual HP loss before treating a spell as effective.
Against an observed hostile caster with a known HP-damage spell, eligible
Action105 is prioritized before other offense when the target does not already
have or resist native status29. A blocked or no-effect attempt falls back to
other choices; verify its actual status or death effect rather than treating
it as HP damage.
Area-targeted spells now retry native Circle input, at most eight times, while
the game is in area-selection phase 2. In original `func_800C4794`, Cross in
the default sphere mode marks the area center cancelled and returns to the
action menu; Circle first enters positioning mode, and a second Circle confirms
it. The disc BATTLE.PRG disassembly matches this, and input is ignored while
the sphere transition counter is nonzero. The controller does not reposition the
current area center; native target filtering must still show the chosen actor
before the cast can be claimed. A targeting phase that remains stuck surfaces a
bounded timeout rather than waiting indefinitely.
Recovery remains available above the low-health boundary only when its
verified maximum can restore the observed two-hit margin. Below60% HP, or when
one observed hit can be survived only after recovery within the native maximum
HP, urgent healing retains priority. With a living spell-capable hostile, an
observed two-hit margin also makes recovery urgent, so attacks cannot crowd
out healing before a known caster acts again. Otherwise the model compares
useful attacks, prevention or movement instead of repeating recovery that
cannot outpace actual damage. An incoming forecast above maximum HP cannot be
made survivable merely by forcing a heal.
Combat goals and per-enemy experience accrue observed opponents/threats only
outside native cinematic ownership. Temporary story actors that carry a hostile
classification are not automatically battle requirements. Actual zero-HP
evidence for a previously observed combat actor is still recorded when its
death starts a cinematic; disappearance alone never establishes a kill.
When native attack targeting refuses the selected hostile, the required
reposition immediately requests the existing observed-terrain pursuit routes,
instead of waiting for the ordinary approach timeout. Refusal evidence is
scoped to the scene and target identity and ceases to require reposition after
16 units of actual movement. Ordinary approach without refusal is unchanged;
route arrival and approximate weapon reach never establish native targetability.

Managed player runs now request `--verification-log`: all native player frames,
command/result/flag records and errors remain available, without per-draw Trace
noise or redundant structured engine echoes. Native executables must be rebuilt
with this option. Use full native tracing for a bounded rendering/audio
investigation rather than keeping it enabled for every long gameplay run.

For bounded Lab scene prerequisites, `--scene-investigation --speed 2` or
`--scene-investigation --speed 4` uses the native fixed-tick multiplier without
changing game rules or state. Normal play remains1x with strict reactions.
Accelerated mode rejects reaction-count requirements, Desktop/save/New Game
sources and physical-door objectives. Every missed/unconfirmed reaction remains
recorded and still disqualifies the player run's overall `passed` result;
`reaction_timing_certification_eligible` and `reaction_coverage_demonstrated`
cannot certify accelerated timing. Source-bound scene verification remains an
independent unchanged gate. All rates and their qualifications persist in run
metadata and reusable experience.

Use `--door-route <original-id> <original-id> ...` instead of `--door-case`
for continuous legitimate traversal in the same game/window. Each leg must
match the live authored destination, observe a real loading cycle and hold
the unchanged 120-tick settled-control gate before the next leg starts.
Accepted legs are checkpointed as `physical_door.accepted` audit events;
`physical_route.cases` retains partial results if a later leg is blocked.
This does not restore state, teleport between cases or relabel transit legs
as new historical-matrix coverage.
An initially unsatisfied native door flag no longer ends a run while original
hostiles remain alive. The existing combat planner continues fighting, onward
navigation is withheld, and the live flag is refreshed from observations.
Only the actual native threshold permits traversal. Enemy death alone never
grants a flag: a still-unsatisfied gate is reported after native events and
120 normal rendered-control ticks settle. Already-unlocked doors retain their
existing traversal behavior; no universal requirement to clear every room is
added.
For explicitly enemy-gated investigations add `--clear-enemies-first` to a
physical-door run. This withholds the model's onward-navigation option until
the original source-room hostiles have observed zero-HP transitions; removed
actors alone do not qualify. The actual native gate, loading cycle and
settled-control checks must still pass afterward. This opt-in does not change
ordinary unlocked-route testing or defeat-all Lab goals.
For an original combined-defeat trigger (several actors dead in the same
trigger evaluation) add `--simultaneous-defeat SLOT SLOT...` to a scene
expectation run. Other hostiles are fought first with ordinary single-target
choices; while every required slot lives, only learned area damage spells
targeting them are offered. The controller commits the area only when the
native target preview lists every required slot with a cached HP-damage
forecast at least its current HP, and otherwise cancels with Cross and waits
or approaches before retrying. Learning those spells is a declared Lab profile
assist (for example `all-spells`); no HP, flag or trigger is edited.
After actual source-room deaths and native rewards/dialogue settle, the runner
explicitly asks the model for the observed physical exit and records
`goal.post_combat_exit`; empty enemy lists no longer suppress that planning
request. Model-selected terrain routing and the authored door remain the only
navigation source of truth.

For source-backed scene fixtures, `--setup-press "120:Up,122:Up"` may establish
an original tile-change producer before the model takes over combat. This uses
the existing Lab input schedule, not position or flag writes. Only ordinary
directions are accepted; combat, interaction, jump and skip buttons are rejected.
Pulses must be ordered, non-overlapping and finish before the observation budget.
The tactical planner waits until the final two-tick pulse and two neutral ticks
have elapsed; the guarded reaction controller retains its normal behavior.
The exact schedule and handoff tick are persisted with the run. This opt-in
requires unchanged production-room actors/events and cannot be combined with
physical-door assertions or Desktop saves. It does not assert scene completion:
actual source commands, handoff, living control and teardown still need proof.

The 100ms observation freshness limit still gates every model/controller input.
A transient stale frame releases keys and waits for fresh state, with a bounded two-second
recovery failure instead of issuing stale actions. Visible dialogue permits
only a leased Circle confirmation in native cutscene control3; other movement
or mixed-button leases are released immediately, and closing the dialogue
also releases the confirmation. Scripted story redirects remain distinct
from fixed authored-door acceptance.
Observed walking stalls permit at most two directed Square jump attempts near
the same position, using the production movement input and its native flag
guard, followed by at most one short correction along each world axis.
Continued lack of progress becomes an explicit blocker, not thousands
of repeated movement inputs. A blocked objective or fully accepted route
releases keys and closes only the owned window; final host/trace evidence
still decides whether shutdown completed normally.

No system model service or global controller configuration is installed.
The runner explicitly supplies `--player-observations` to its owned host.
This version requires player observation schema 2; rebuild the selected host
after native diagnostic changes. Older frozen images are rejected explicitly.
Player-only collectors and types live in the game's Diagnostics subsystem;
ordinary gameplay inspection and normal traces do not collect or encode them.
See [the diagnostic interface](../Game/include/Diagnostics/PlayerObservation.hpp)
and [its read-only producers](../Game/src/Diagnostics/PlayerObservation.cpp).
The opt-in observations contain facts only. Model inference, navigation,
retrieval, tactics and input execution remain entirely in this external tool.
Per-process SDL settings exclude hardware gamepads and prevent automatic
window activation. Input messages target only the verified child PID/window;
the tool does not move foreground focus or send global keyboard input.
Windows job objects own child lifetimes, including model-runner descendants.
Ctrl+C cancels the observation and pins its evidence.

`bootstrap` downloads the checksum-pinned portable Ollama runtime and explicit
Qwen3-4B-Instruct-2507 quantization recorded in [runtime.json](runtime.json).
Do not substitute the generic thinking-model tag. The official runtime and
model licenses remain in their downloaded distributions; weights/binaries
are not repository content. `doctor` checks the pinned weights, two actual
instruction-following controls, local inference and measured GPU allocation.
The default budget is 4 GiB model VRAM plus 2 GiB free headroom; one model,
one inference request, an 8K context and bounded batches avoid unbounded
competition with other applications. A readiness check is not a gameplay test.

Up to four games may share one warmed model instead of each loading its own:

```powershell
.\.venv-3\Scripts\python.exe Native\Player\player.py serve --model-port 11561 --parallel 4
.\.venv-3\Scripts\python.exe Native\Player\player.py play --model-port 11561 --shared-model 4 ...
```

`serve` owns the runtime with `OLLAMA_NUM_PARALLEL` slots (budget 4 GiB plus
1 GiB per extra slot; measured 5.2 GB for four slots) and keeps the weights
resident until Ctrl+C. `--shared-model` borrows that server and fails if it is
not serving; each game still has its own window, input target and run folder.

`--setup-interaction` additionally permits lone ordinary `Cross` setup pulses,
only with a room-local or external `--scene-expectation`; it never applies to
room-entry scenes and still rejects Circle, Start, Triangle and mixed buttons.

## OODA and source of truth

* **Observe:** read complete, ordered native JSONL frames. Shared read-only
  diagnostics expose actor identity/position/hostility, exact weapon shape,
  current learned actions/costs/restrictions, mappings, live status masks,
  menu state and incoming damage/type/affinity. Missing or stale facts fail
  explicitly; they are not replaced with guessed enemies or ranges.
* **Orient:** [combat-knowledge.json](combat-knowledge.json) describes verified
  semantics, not just names. The local model receives current options,
  constrained valid choices, completed milestones and verified past outcomes.
  Scene/spawn changes invalidate stale plans. Failed model responses are
  recorded and receive at most two explicit correction attempts.
* **Decide:** the model selects strategy/target, preparation, spell or mapping.
  Legal choices come from actual learned options. Known caster debuffs,
  self-buffs and healing check current status/immunity/resource facts.
  Model latency does not control reaction timing.
* **Act/observe again:** a fast reactor uses the native strict interval
  `window < timer <= window + margin`, eligibility and previous-button
  restrictions. The game still performs all action/cost/result calculations.
  Normal menus assign abilities and select spells; native targeting confirms
  actual selectable actors. A button-send acknowledgment is never a successful
  cast, chain, defense or door.

Ready multi-part target menus trigger a fresh model choice of an actual native
actor/limb instead of blindly confirming the first row. Opt-in diagnostics copy
cached target forecasts, including hit thresholds and supported damage/recovery
amounts; they never recalculate attacks or consume RNG. Missing forecasts and
unsupported effect amounts remain unknown. A limb that disappears during
inference invalidates the plan.

Actor observations preserve the native unsigned weapon-action byte, including
unclassified values such as128 on scripted noncombat actors. These do not imply
physical attacks or make an actor hostile. Defense advice reports unclassified
values explicitly instead of guessing their effect.

Original enemy-clear events can own the real loot/ability-reward menus while
field control remains with the event. Read-only `menu.event_owned` identifies
this context. Only an actually ready loot menu permits Square, and only enabled
ability-reward rows permit their normal selection/confirmation keys. This does
not authorize arbitrary cutscene movement, clear loot directly or bypass a VM
wait. Event challenge menus have not been independently certified.

Planning prompts use lossless compact JSON, not truncated observations. A local
server HTTP failure includes its status, operation and bounded response detail
in the persisted failure. The exact MAP360 request that exceeded the8192-token
context at8218 tokens completes at6732 prompt tokens after compaction, with all
prompt facts unchanged. This is a request repair, not combat or scene acceptance.
Large rooms still exceeded 8192 tokens (MAP150 local5: 8277), so the pinned
runtime context is 16384 tokens; the observation facts are not truncated.

An attack target rejected by the native selector retains its scene/spawn
identity and the actual player position in feedback. The controller must move
at least 16 world units, including vertical movement, before querying again
from that position. This is retry pacing, not a guessed hit volume or an
override of native eligibility. Learned chains can fill empty saved mappings
through their normal menu; nothing is granted.

Successful ordinary combat approach is preserved. Only measured lack of
progress invokes model-selected terrain pursuit of the same live target;
scene/spawn changes invalidate it, and native targeting, rewards, preparation
and reactions preempt movement. Steep descents can require sheathing too.
For an original physical-door objective, the model can explicitly choose
onward navigation while foes remain alive. This does not bypass story/lock
rules or declare those foes dead. Combat Lab defeat-all goals do not offer
that door choice.

Reflection and absorption require matching spell/nonspell damage; affinity
defenses additionally require matching affinity. Integer rounding, missing
resources, shield PP and native status masks matter. Ward cures paralysis/
numbness, not ordinary HP damage. Siphon Soul needs an incoming spell and
missing MP. No useful equipped defense is recorded as a coverage gap, not a
reason to press an ineffective button.

Native timing acknowledgments and observed effects are separate. Effect probes
match the actual action and queue identity, and retain zero/unconfirmed effects.
A chain accepted just as its target dies is not claimed to have executed.
An observed defeated target cannot receive another offensive continuation.
Target-defeat cancellation is logged separately from timing acceptance/failure;
it never counts as an accepted chain or useful effect, and a real native
rejection remains a rejection even if that target also dies.
The required defense count also requires an observed useful effect. "Perfect"
means zero measured missed/rejected windows in the stated observation, not a
universal scheduling guarantee. An uninterrupted boss-killing chain is a
possible strategy, not an acceptance requirement.

Navigation uses model-selected candidate paths through bounded live support/
clearance samples, including stationary stacked objects. The executor checks
new best waypoint distance rather than counting sideways jitter as progress.
Takeoff alignment, native movement/climb state and normal weapon sheathing
matter; merely sending Square is not proof of a jump or completed climb.
Opening an object and walking onto a transition tile are separate operations.
The model can also select enabled native rows in a legitimately earned ability
reward menu. Newly learned actions must then appear in native observations;
the tool never unlocks them directly.
If no healing spell is learned, currently owned HP-recovery consumables are
available through the normal L2+Up item shortcut. Selection follows observed
inventory IDs, not guessed row names. A useful item outcome requires its native
action, one-item count decrement and HP recovery within the source-backed
minimum (clamped to missing HP). Healing is prioritized against actual observed
large-hit damage as well as a low-HP threshold. Native ready loot screens can
claim currently fitting drops with Square; inventory capacity remains native.

## Persistent state and cleanup

Default state: `%LOCALAPPDATA%\VS-NativePort\Player`. Override it only with a
new dedicated `--state-dir` **before** the subcommand. Existing unowned
directories and linked/junction paths are refused.

SQLite retains decisions, inputs, validation errors, timing acknowledgments,
effects, outcomes and verified reusable experience. Run folders retain native
logs, setup, model knowledge, image hashes and failure diagnostics. Only
verified passed outcomes become reusable experience; cleanup does not erase
those compact lessons.

Model weights are fixed: this is retrieval-augmented decision making, not
online fine-tuning. Every request is a fresh bounded prompt containing current
observations and retrieved evidence. Combat retrieval uses enemy/weapon context.
Spatial retrieval uses room, original door ID and an exact observed-geometry
fingerprint. Measured local waypoint completion and lack of progress survive
raw-log cleanup, with the run/image provenance and their limited claims.
An observed obstruction is not proof of permanent impassability, and reaching
a waypoint is not proof that the full door route passed.

Cleanup runs at startup and every 15 minutes while playing:

* Keep the latest 10 successful runs for up to 7 days, within a 10 GiB raw-log
  budget. Prune only direct, ownership-marked, completed successful run folders.
* Active, blocked, failed and cancelled evidence is pinned and never silently
  deleted. Unrelated game logs, owner saves, models and runtime files are
  outside cleanup scope.
* A run reaching 512 MiB stops explicitly rather than dropping diagnostics.
  `play --run-log-mb N` declares a larger active-run limit for actor-dense
  rooms whose frame trace legitimately exceeds it; the run metadata records it.
  If pinned evidence exceeds the total budget, further tests refuse to start.
* Parallel Players share the store. A cross-process `cleanup.lock` gives one
  process each cleanup pass; the others skip it instead of deleting the same
  folders concurrently.

```powershell
.\.venv-3\Scripts\python.exe Native\Player\player.py cleanup
.\.venv-3\Scripts\python.exe Native\Player\player.py pin --run-id <32-character-run-id>
.\.venv-3\Scripts\python.exe -m unittest discover -s Native\Player\tests -v
```

Pin a successful run used as issue-acceptance evidence before routine cleanup
ages it out. Pinning protects its raw logs and frozen image, not just its compact
SQLite outcome.

The owner's explicit2026-10-28 pruning authorization cold-archived older raw
logs, including pinned evidence, without changing outcomes or deleting saves,
assets or summaries. The verified path/digest index is
`%LOCALAPPDATA%\VS-NativePort\evidence-archive\20261028\index.jsonl`.
An archived path is not a new successful observation; restore/read its original
bytes when needed. Do not keep raising raw-log budgets or replay the prologue
again to replace evidence that already exists.

## Verified scope and remaining limits

Local dirty `b1703e1d` observations demonstrate Debug/Release menu assignment
of Reflect Damage, useful reflected HP damage, legal offensive chains, enemy
defeat and 120-tick settled control. Separate Debug observations verify Herakles,
Prostasia and actual Heal recovery through native menus without stat overrides.
The isolated healing stress test survives but does not defeat its enemy within
budget and remains blocked. Test profiles are explicit Lab preconditions, not
earned story progression.

The production Continue observation uses the unchanged postboss native save:
physical 9/17 to 9/12, original enemies, two useful reflections, no engine errors
or forced skips. A physical door still needs its authored flags/lock/terrain
conditions; an unavailable/redirected/unsettled case stays blocked. Navigation
currently follows observed door/target coordinates, not a complete obstacle,
jump, platform or campaign solver. Ranked multi-level/area spell UI and every
caster/status matchup are not yet certified.

#151 is closed with its bounded loader/traversal evidence and explicit remaining
limits. Do not rewrite its historical loader matrix or count a successful Combat
Lab sequence exit as physical/story acceptance. #149 remains open for actual
source-bound scene verification; physical observations and necessary legitimate
traversal retain separate provenance.
