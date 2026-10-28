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
```

Release uses the corresponding built executable/PDB. The tool copies the
selected image, matching PDB and DLLs into a private run, verifies hashes, and
starts one owned window. Do not mix configurations or rebuild over frozen
evidence. Desktop uses normal Start/Continue/menu inputs with a byte-identical
private copy of the explicitly selected save. The source save is never edited.
Desktop tick-limit observation is not claimed to finish a campaign or execute
full in-game shutdown; its host/trace completion is reported separately.

Use `--door-route <original-id> <original-id> ...` instead of `--door-case`
for continuous legitimate traversal in the same game/window. Each leg must
match the live authored destination, observe a real loading cycle and hold
the unchanged 120-tick settled-control gate before the next leg starts.
Accepted legs are checkpointed as `physical_door.accepted` audit events;
`physical_route.cases` retains partial results if a later leg is blocked.
This does not restore state, teleport between cases or relabel transit legs
as new historical-matrix coverage.
Observed walking stalls permit at most two directed Square jump attempts near
the same position, using the production movement input and its native flag
guard, followed by at most one short correction along each world axis.
Continued lack of progress becomes an explicit blocker, not thousands
of repeated movement inputs. A blocked objective or fully accepted route
releases keys and closes only the owned window; final host/trace evidence
still decides whether shutdown completed normally.

No system model service or global controller configuration is installed.
The runner explicitly supplies `--player-observations` to its owned host.
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
  If pinned evidence exceeds the total budget, further tests refuse to start.

```powershell
.\.venv-3\Scripts\python.exe Native\Player\player.py cleanup
.\.venv-3\Scripts\python.exe Native\Player\player.py pin --run-id <32-character-run-id>
.\.venv-3\Scripts\python.exe -m unittest discover -s Native\Player\tests -v
```

Pin a successful run used as issue-acceptance evidence before routine cleanup
ages it out. Pinning protects its raw logs and frozen image, not just its compact
SQLite outcome.

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

#151 remains open. Do not rewrite its historical loader matrix or count a
successful Combat Lab sequence exit as physical/story acceptance. Use saved
passed checkpoints for loader retries; physical observations and necessary
legitimate traversal have separate provenance.
