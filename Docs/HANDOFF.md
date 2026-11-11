# Current Handoff

Updated 2026-11-11. Row 28 (battle AI) resumed after the cleanup checkpoint.
The condition terms, quality repairs and three candidate-planning promotions
and the two planning-stat lifecycle promotions are published with green
exact-revision CI. F01707 fallback selection and F01713 planning simulation are
also published with green exact-revision CI. F01712 candidate enumeration is
published with green exact-revision CI. F01716 action-plan orchestration is
published with green exact-revision CI. F01717 completed-action reconciliation,
including the #336 fix, is published in `e9a85172` with green exact-revision
CI, and #336 is closed. F01793 target forgetting is published in `31f0bdca`
with green exact-revision CI. F01810 action-slot metadata is published in
`deae1369` with green exact-revision CI. F01811 linked-actor resolution is
published in `b6dab817` with green exact-revision CI. The #338 promotion-gate
fix is published in `e78c4062`. F01812 saved AI settings flags is published
in `5c914a97` with green exact-revision CI. F01813 actor AI settings is
published in `ffd1a8e1`
([CI 42896326975](https://github.com/JEschete/VS-NativePort/actions/runs/42896326975)).
The #340 throughput tooling is published in `48ab62e3` and the #339 replay
cleanup in `71973e51`. F01818 actor AI state creation is published in
`eb9443c2`
([CI 42954096352](https://github.com/JEschete/VS-NativePort/actions/runs/42954096352)),
and the first grouped promotion, F01814-F01817 special terrain heights and room
navigation entry, in `fa47431b`
([CI 42955909989](https://github.com/JEschete/VS-NativePort/actions/runs/42955909989)).
F01721-F01728 route gates and area routing are published in `53d04fe5`
([CI 42962718861](https://github.com/JEschete/VS-NativePort/actions/runs/42962718861)),
and the F01708-F01710, F01740, F01746 and F01747 waypoint and area-movement
group in `7fde4472`
([CI 42969176657](https://github.com/JEschete/VS-NativePort/actions/runs/42969176657)).
The F01738/F01739 height-filter and F01741-F01745 wander group is published in
`8e520fb4`
([CI 42980477174](https://github.com/JEschete/VS-NativePort/actions/runs/42980477174)),
and the F01735-F01737 radial-movement and F01750 recovery group follows it. See
[Stopping Point](#stopping-point) for the next group.

## Checkpoint

- Base revision: `90e5fbfe6d5559e0e79591db722eae2f4dc48092`, the documentation
  cleanup over code checkpoint `f6ba998e` (F01697,
  [Source Quality run 42710468317](https://github.com/JEschete/VS-NativePort/actions/runs/42710468317)).
  Three scoped commits follow it on `imp/01-ma04-gates`: #333 is fixed in
  `64ade58c` ([CI 42747075433](https://github.com/JEschete/VS-NativePort/actions/runs/42747075433));
  F01700-F01703 are promoted in `db3c0337`
  ([CI 42747751480](https://github.com/JEschete/VS-NativePort/actions/runs/42747751480));
  #335 is fixed in `ba968d04`
  ([CI 42748445178](https://github.com/JEschete/VS-NativePort/actions/runs/42748445178)).
  All three exact-revision runs passed.
- **928 functions remain** (after the radial-movement and recovery group on
  `8e520fb4`). Modernization: 2,816 done of 3,744. C++ promotion: 2,817
  promoted plus one frame-orchestrator WIP. These are separate measures.
- Original 3,479-function cohort at `ffd1a8e1`: 2,505 promoted, 16 retired,
  957 generated, one WIP; 2,521 resolved. This continuation resolved 72 more
  than `6c8ac0b9`.
- Plan row 27, field gimmicks/doors, is done for C++ ownership: its original
  51 IDs audit to 46 promoted and five retired. Row 28, battle AI, is active.
- Issues #325-335 are closed with fixing revisions and verified CI.
  #334 is fixed in `8709974d`
  ([CI 42751897850](https://github.com/JEschete/VS-NativePort/actions/runs/42751897850)).
- F01698, F01699 and F01704 are integrated in the main worktree; the temporary
  isolated worktree was removed. No owned build, replay or quality run remains
  active. The candidate milestone is published in `ceaee217`
  ([CI 42756911275](https://github.com/JEschete/VS-NativePort/actions/runs/42756911275)).
  Local verification used base `8709974d` plus that commit's implementation,
  tests, generated bindings and related ledgers.
- F01714/F01715 are published in `bd1f96fc`
  ([CI 42763174803](https://github.com/JEschete/VS-NativePort/actions/runs/42763174803)).
  Local verification used base `109620c2` plus that commit's lifecycle
  implementation, tests, generated callers and related ledgers.
  Their ownership and behavior claims are recorded separately from the retained
  exported snapshot pointer, G00589.
- The lifecycle evidence update `8b2eedfd` also passed
  [CI 42763848334](https://github.com/JEschete/VS-NativePort/actions/runs/42763848334).
  F01707 is published in `e41bbbd5`
  ([CI 42767581728](https://github.com/JEschete/VS-NativePort/actions/runs/42767581728)).
  Local verification used base `8b2eedfd` plus that commit's fallback policy,
  native binding, migrated workspace caller, module entry and related evidence.
- The fallback evidence update `23902fe7` passed
  [CI 42768591783](https://github.com/JEschete/VS-NativePort/actions/runs/42768591783).
  F01713 is published in `6ae86ecd`
  ([CI 42772302695](https://github.com/JEschete/VS-NativePort/actions/runs/42772302695)).
  Local verification used base `23902fe7` plus that commit's simulation policy,
  checked binding, generated caller/module entry, tests and related ledgers.
  Its native core keeps the wrapping readiness clock, signed halfword stores
  before clamps, poison/regeneration order, inclusive byte-timer expiry and
  per-actor restriction/slot refresh order. Shared scalar helpers preserve
  word multiplication and original zero-divisor behavior. Native tests keep
  the simulated actor's stats distinct from the callback actor's stats.
  The old function symbol retires separately from the retained room clock,
  status tables and legacy records.
- The simulation evidence update `91717ee8` passed
  [CI 42773496563](https://github.com/JEschete/VS-NativePort/actions/runs/42773496563).
  F01712 is published in `79c2af10`
  ([CI 42777655682](https://github.com/JEschete/VS-NativePort/actions/runs/42777655682)).
  Local verification used that base with the enumeration core,
  checked binding, generated planner calls/module entry, tests and ledgers.
- The enumeration evidence update `9c79316e` passed
  [CI 42778988160](https://github.com/JEschete/VS-NativePort/actions/runs/42778988160).
  F01716 is published in `6ad0b1fc`
  ([CI 42787061366](https://github.com/JEschete/VS-NativePort/actions/runs/42787061366)).
  Local verification used that base with the actor-plan core, native
  binding, migrated workspace and lifecycle-test callers, generated module
  entry and related evidence. Its old symbol has five former source consumers
  and zero remaining consumers. The exported pass/cache/exclusion storage
  remains; its retirement is not implied by this function promotion.
- The action-plan evidence update `5c10d866` was the F01717 base. F01717 is
  published in `e9a85172`
  ([CI 42863138438](https://github.com/JEschete/VS-NativePort/actions/runs/42863138438)).
  Local verification used that base with the completed-plan core, checked live
  binding, migrated workspace caller, generated module entry, tests and ledgers.
  The old symbol has four former source consumers and zero remaining.
- `e9a85172` was the F01793 base. F01793 is published in `31f0bdca`
  ([CI 42867473657](https://github.com/JEschete/VS-NativePort/actions/runs/42867473657)).
  Local verification used that base with the target-forget core, checked live
  binding, generated perception and blocked-movement callers, module entry,
  tests and ledgers. The old symbol has six former source consumers and zero
  remaining.
- `31f0bdca` was the F01810 base. F01810 is published in `deae1369`
  ([CI 42874511974](https://github.com/JEschete/VS-NativePort/actions/runs/42874511974)).
  Local verification used that base with the action-metadata core, checked
  live binding, generated actor-state caller, module entry, tests and ledgers.
  The old symbol has four former source files and zero remaining; the
  generated `ActionSelection.cpp/.h` pair is removed.
- `deae1369` was the F01811 base. F01811 is published in `b6dab817`
  ([CI 42880218716](https://github.com/JEschete/VS-NativePort/actions/runs/42880218716)).
  Local verification used that base with the linked-actor core, checked live
  binding, actor-state host, generated actor-state caller, module entry, tests
  and ledgers. The old symbol has four former source files and zero remaining.
- `b6dab817` was the #338 base. The gate fix is published in `e78c4062`
  ([CI 42884695914](https://github.com/JEschete/VS-NativePort/actions/runs/42884695914)).
  On that revision alone, `test_gates.py` (96) passes and the gate,
  modernization and hazard checks report zero violations with 2,784 done, so
  the fix changes no existing ledger row.
- `e78c4062` was the F01812 base. F01812 is published in `5c914a97`
  ([CI 42885758362](https://github.com/JEschete/VS-NativePort/actions/runs/42885758362)).
  Local verification used that base with the saved-settings core, checked live
  binding, generated actor-state caller, module entry, project generator entry,
  tests and ledgers. The old symbol has three former source files and zero
  remaining.
- `5c914a97` was the F01813 base. F01813 is published in `ffd1a8e1`
  ([CI 42896326975](https://github.com/JEschete/VS-NativePort/actions/runs/42896326975)).
  Local verification used that base with the settings-decode core, checked
  live binding, generated actor-state caller, module entry, tests and ledgers.
  The old symbol has three former source files and zero remaining.

## Stopping Point

HEAD `8e520fb4` (height filter and wander) is pushed and green
([CI 42980477174](https://github.com/JEschete/VS-NativePort/actions/runs/42980477174)).
The commit after it holds the radial movement group:

- F01735 `ProjectClearRadialDestination`, F01736 `MoveToRadialDestination`,
  F01737 `MoveAwayFromReference` and F01750 `UpdateMovementRecovery` live in
  the new pure `RadialMovement` module. The checked `RadialMovementLive`
  bindings are `ProjectLiveClearRadialDestination`,
  `MoveLiveToRadialDestination`, `MoveLiveAwayFromReference` and
  `RecoverLiveMovement`; they keep the original module addresses 0x800E1388,
  0x800E153C, 0x800E1764 and 0x800E2CCC. `RoomTileBounds` moved to
  `NavigationPrimitives` and `LiveRoomTileBounds` to `FrameUpdateLive`, shared
  with `Wander`.
- The generated callers in `Behavior.cpp`, `OrbitingBoss.cpp`,
  `Perception.cpp` and `Movement.cpp`, and the `GroundNavigationLive` and
  `WanderLive` hooks, call the bindings. The four old symbols had 33
  references at `8e520fb4` and have none now.
- #341: the original projection kept bytes 1 and 3 of its caller's `$s0` in
  the destination, and the generated copy stored indeterminate bytes there.
  Native writes zero; no consumer reads them. The differential seeds `$s0`
  with zero for direct projections and ignores those bytes after F01736 and
  F01737.
- The `radial_movement` differential covers 470 original instructions
  (0x800E1388-0x800E184C and 0x800E2CCC-0x800E2F58): 4,096 vectors, 0
  mismatches, 39/39 mutants. The planned `quadrant-not-wrapped` mutant was
  equivalent, because the loop masks the direction with 7 at 0x800E2E78; it was
  replaced by `quadrant-wrapped-to-1`. The `wander` rerun still passes (4,096
  vectors, 43/43 mutants). `differential.py` gained `pack_fields` and
  `Machine.load_fields`, which both cases share.
- Focused `[radial-movement]`: 26 cases, 197 assertions; the group selection
  has 209 cases and 796,358 assertions. Full suite: 5,500 cases, 12,650,071
  assertions in Debug and Release; the replays matched the baseline.
  Changed-file quality: 48 existing tidy findings, 0 new.
- A native broken variant of each function fails its tests (F01735 6 cases,
  F01736 3, F01737 1, F01750 1).

Next group: blocked movement and movement history, F01636, F01641, F01642,
F01644 and F01660. The design:

- F01636 `FindAdjacentMovementDestination` (`6FC00.c:430`, 0x800D8400)
  returns 0 for a disabled mover. With equal directions it probes the given
  direction; otherwise it probes the nearest axis direction to the heading
  (rounded up past 512 in the quadrant) and then `first`, or `second` when
  `first` is that axis. Each probe checks the edge
  (`CanTraverseLiveTileEdge`). A clear edge then asks occupancy: negative is
  blocked, nonzero returns the tile, and zero returns 0. A blocked edge returns
  blocked, and any other result tries the gap landing, where status 1 returns
  and nonzero is blocked. The result clears the tag byte, and blocked sets
  status 2.
- F01641 `HasRepeatingMovementHistory` and F01642 `IsMovementHistoryStalled`
  are inlined in func_800D8EF4 at 0x800D927C-0x800D92B8 (15 instructions).
  The history is stalled when the mask (0x81) is 0xFF and both halves of the
  x history (0x70/0x74) and the z history (0x78/0x7C) repeat. They are
  verified like F01700-F01703, by interpreting that inlined range.
- F01644 `SaveMovementRandomState` (0x800D9538, 5 instructions) copies the
  scratch random state at 0x1F8003F8 to `Battle_Ai_savedMovementRandomState`
  (0x800F58C4). Its callers are `FrameUpdateLive` and
  `NavigationPrimitivesLive`.
- F01660 `TryRecoverBlockedMovement` (`71618.c:1212`, 0x800DB0BC) only acts
  without the arena orbit and when the tile's x and z equal the previous
  tile's. It then marks the mover stuck. At detour step 0 it picks the two
  lateral directions from the angle to the destination's tile centre and their
  opposites, in an order drawn from `NextLiveActorRandomMasked(2)`, stores the
  four directions, and records the first one whose neighbour is free (step
  index plus one). At step 5 or more it forgets the target, clears 0x1C and
  the step, and returns 0. Otherwise it moves the destination to the detour
  neighbour and copies 0xFB to the movement target. A free destination
  dispatches locomotion mode 0; otherwise the step advances. It returns 1
  while detouring.
- The nearest-axis rounding is shared by F01636, F01660 and the existing
  `FacingAxisDirection` in `RadialMovement.cpp`. Extract one helper.

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
interactive, sequenced, device and cited run (those cited in
`COMBAT_LAB.md`/`HANDOFF_HISTORY.md`). Since `71973e51` (#339, closed)
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
   only the first N vectors, which keeps large cases fast. Reports go to `build\differential\<case>.log`. The F01813
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

The entire previous handoff was preserved verbatim in
[HANDOFF_HISTORY.md](HANDOFF_HISTORY.md), under
`Archived Handoff: 2026-11-09 / f6ba998e`. Its old current/next/pending statements
are historical, not instructions or fresh verification.

## Latest Work

- F01813 now owns actor AI settings decoding through
  [ActorSettings.hpp](../Native/Game/include/Battle/Ai/ActorSettings.hpp) and
  [ActorSettings.cpp](../Native/Game/src/Battle/Ai/ActorSettings.cpp).
  `ApplyActorAiSettings` turns the settings word, the spawn group and flags
  words (actor `0x30`/`0x34`), the sub-type, collision radius, movement flags
  and the room waypoints into typed results. It covers the perception range
  (the near range is capped at `0x240000`), the view angle and the cosine of
  half of it, and the step, facing-delay, wander and patrol tables. The
  terrain reject mask changes only while movement flags `0xC0` are clear. The
  action-movement bit is kept only when a room waypoint has bit 15 set.
  Sub-types `0xAC`-`0xAD` are orbiting bosses.
- [ActorSettingsLive.cpp](../Native/Game/src/Battle/Ai/ActorSettingsLive.cpp)
  reads the real BATTLE.PRG tables. It also checks that the waypoint pointers
  belong to the active room's auxiliary section (offset `0x440`) and aborts
  with a diagnostic otherwise. It writes the decoded fields in the original
  order and installs the orbit callback and its orbit state, or the
  group-selected decision callback (`nullptr` past the table, #272). It then
  clears the linked actor to 16, resolves links and rebuilds the action-slot
  metadata. The generated actor-state creation caller (F01818) calls the
  native binding, and `0x800E5A9C` retains the registered entry.
- The actual 265-instruction original matches the native core over 8,192
  vectors. Each vector compares four things:
  - the full `0x180`-byte state image against the decoded fields at their PS1
    offsets;
  - the call order and arguments;
  - the native cosine of the original's `rcos` argument;
  - the decision slot.

  The vectors include orbiting sub-types, rooms with and without active
  waypoints, kept terrain masks, capped near ranges, large radii and decision
  indexes past the table. In that last case the native core stores no callback
  (#272); the original reads past the table. The following mutants are
  rejected:
  - an uncapped near range;
  - a `0x80` radius threshold;
  - a missing terrain guard;
  - a wander shift of 5;
  - three orbiting sub-types;
  - waypoint bit 14;
  - an unhalved view angle.
- F01812 now owns the saved AI settings flags through
  [ActorSettings.hpp](../Native/Game/include/Battle/Ai/ActorSettings.hpp) and
  [ActorSettings.cpp](../Native/Game/src/Battle/Ai/ActorSettings.cpp). Bits
  1-11 of the actor's saved settings flags (the room spawn's halfword at actor
  word `0x34`) replace settings bits 21-31; the enemy template's bits 0-20
  stay. Flag bit 0 (the caller's enable test) and bits 12-15 are ignored.
- [ActorSettingsLive.cpp](../Native/Game/src/Battle/Ai/ActorSettingsLive.cpp)
  reads only the saved pair's halfword at +4 and writes only the 4-byte
  settings word. The generated actor-state creation caller (F01818) keeps the
  enable-bit test and calls the native binding; `0x800E5A74` retains the
  registered entry. Null arguments abort with a diagnostic.
- The actual 10-instruction original matches the native core over 65,552
  vectors: every flags halfword with random settings and pair bytes, plus
  boundary values. The original reads only the settings word and pair+4 and
  stores only the settings word. Mutants that keep the enable bit, shift by
  21, keep only bits 0-19 or read pair+6 are rejected.
- #338: the promotion gate read `return (expr);` as a forwarding call to
  `return`, so F01812's single-expression body lost `vs_namespace` and failed
  the modernization check. Keyword-led expressions are no longer forwarding
  calls; parenthesized forwarding calls still are. Measured over all 2,787
  records, only F01812 changes. The new controls fail against the old pattern.
- F01811 now owns AI linked-actor resolution through
  [Relations.hpp](../Native/Game/include/Battle/Ai/Relations.hpp) and
  [Relations.cpp](../Native/Game/src/Battle/Ai/Relations.cpp). It walks the
  battle actor list after Ashley. Each actor whose signed linked-actor byte
  (`0x16`) is 16 links to the first listed actor, itself included, whose 4-bit
  group (word `0x30` bits 0-3) equals its 5-bit linked group (bits 9-13). The
  link is reread per entry, so a linked group of 16 or more matches nothing
  and existing links stay.
- [RelationsLive.cpp](../Native/Game/src/Battle/Ai/RelationsLive.cpp) reads
  the real actor list and AI state table and writes only the linked-actor
  byte. Actor creation and re-attach call the native binding, and
  `0x800E5998` retains the registered entry. A listed actor without an AI
  state is refused with a diagnostic before any write; the original reads
  through the null table entry. That case is logged as
  [#337](https://github.com/JEschete/VS-NativePort/issues/337), and its
  in-game reachability is unconfirmed.
- The actual 55-instruction original matches the native core over 4,096
  vectors, comparing every listed state and the store addresses. The vectors
  include repeated IDs, linked groups of 16 or more and non-16 link bytes.
  Mutants that compare a 4-bit linked group or search from the list head are
  rejected. The null-state vectors (#337) are excluded from that comparison.
- F01810 now owns AI action-slot metadata through
  [PlanningPrimitives.hpp](../Native/Game/include/Battle/Ai/PlanningPrimitives.hpp)
  and [PlanningPrimitives.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp).
  The player (actor 0) only sets the action-cost threshold to 1. Other actors
  rebuild all 24 slots in row order: squared range (stored before the area
  call), 16-bit area reach, long range above `0x51000` unless the action is
  radial, and the fallback flag from the room byte, action type and category
  14, reread per slot. Self-exclusion and obstruction flags are only set; the
  kind changes only for a category that has one. Spell costs raise the
  threshold. Usable and reserved record bits are kept.
- The live host reads the real stats slots, action table, room AI state and
  the shared targeting range helpers. A new stats-based range entry in
  [TargetingLive.cpp](../Native/Game/src/Battle/TargetingLive.cpp) reads the
  weapon range from the same stats pointer as the original. Actor-state setup
  calls the native binding and `0x800E5710` retains the registered entry. An
  action ID of 256 or more is a diagnosed refusal; the original reads past the
  table.
- The actual 162-instruction original matches the native core over 4,096
  vectors, comparing every slot record, the threshold and the call order. A
  mutant using `>=` for the long-range test is rejected. The generated views
  in `AiState.h` remain unused; their removal is separate cleanup.
- F01793 now owns target forgetting through
  [PlanningPrimitives.hpp](../Native/Game/include/Battle/Ai/PlanningPrimitives.hpp)
  and [PlanningPrimitives.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp).
  It writes only the previous-tile word (`0x38`) as -1, so remembered-target
  readers see none. The remembered destination (`0x3C`) and lifetime (`0x40`)
  stay. It reads no ranking, settings or placement. Perception, blocked
  movement and completed-action reconciliation call the native binding, and
  `0x800E4CE8` retains the registered entry. The actual 3-instruction original
  matches the native core over 4,096 full-state vectors with one 4-byte store.
- F01717 now owns completed-action reconciliation through
  [PlanningWorkspace.hpp](../Native/Game/include/Battle/Ai/PlanningWorkspace.hpp)
  and [PlanningWorkspace.cpp](../Native/Game/src/Battle/Ai/PlanningWorkspace.cpp).
  Only opcode-24 commands scan. A match needs the full candidate action ID
  plus the command's row and column halves. Every match replaces the detour
  target and rescans the primary hit results, skipping position targets.
  Absent objects or kinds 2/4 forget memory and return; otherwise memory is
  remembered once after the scan, including when nothing matched.
- #336: the original reads the performer state's actor before its null check.
  The native policy returns for an absent state before any actor read. Live
  tests reach that path with an empty state slot and with the real state,
  gimmick and actor tables; `0x800DEB10` retains the registered entry.
- The actual 94-instruction original matches the native core over 4,096
  vectors, comparing the detour byte, read order and forget/remember calls.
  A mutant that stops after the first match is rejected. Invalid counts and
  storage are diagnosed refusals. Both F01717 hazard rows are gone.
- F01716 now owns action-plan orchestration through
  [PlanningWorkspace.hpp](../Native/Game/include/Battle/Ai/PlanningWorkspace.hpp)
  and [PlanningWorkspace.cpp](../Native/Game/src/Battle/Ai/PlanningWorkspace.cpp).
  It preserves cache/exclusion reset before the actor-zero exit, unsigned
  recovery timing, cumulative simulation, pass quotas with spare middle-pass
  capacity, rejected exclusions and pass-order merging. Empty-plan fallback
  copies receive weight 128 without changing the cache.
- A scoped snapshot restores stats and resets readiness before promotion,
  including refusal and callback-exception paths. Snapshot failure retains
  #274's initialized selection counts. Threshold spacing and jitter preserve
  halfword wrapping and reread rankings/counts after RNG; readiness and
  movement precede late arena reads. The arena binding captures the canonical
  actor and reads the full 32-bit command argument, not its 16-bit view.
- The owned workspace caller and `0x800DE3E4` now use the native planner.
  The generated implementation and three obsolete candidate-copy adaptations
  are removed through the copier. Native tests cover real snapshot allocation
  and exhaustion, restoration, simulation, special-action enumeration,
  pass/ranking storage, readiness, movement and arena command width.
- F01712 now owns candidate enumeration through typed slot/target values and a
  checked live host. Four scratch records become local candidates. The working
  candidate retains fields across slots and targets; all 24 slots, actor-list
  advances, eligibility/exclusions and late metadata reads keep their original
  order. Direct, limb and aimed evaluation retain distinct mode arguments.
  Direct mode also uses the existing limb preview; it is not a no-preview mode.
- Limb/aimed passes keep strict signed-score comparisons and first-winner ties,
  prefer the probabilistic winner over the score-only fallback, capture chance
  before RNG but copy the candidate afterward, and cache before appending.
  Empty-limb fallback still draws a random part; negative limb maxima qualify.
  Aimed height is the absolute signed halfword narrowed to the 12-bit field.
- `0x800DD918` retains the registered entry. The former generated planner
  calls were migrated with F01716 to the native enumeration API.
  Two obsolete argument adaptations were removed with F01712.
  Native special-action fixtures exercise real evaluation/cache/ranking,
  separate selection and target stats, repeat-call exclusions and unchanged
  old scratch bytes. They are not ordinary-preview or gameplay verification.
- F01707 now refreshes all 24 action slots through a typed host, selecting the
  first greatest positive **unsigned** range. It preserves action capture before
  refresh, metadata reads after refresh, exact packed writes and action-only
  clearing when no slot qualifies. Native selection reads `state.stats`, while
  usability still reads the actor's stats; those sources are deliberately not
  collapsed. The owned workspace caller uses the native API and the original
  `0x800DD604` module address remains bound to its registered entry.
- Actual original F01707 instructions match the native core over 4,096 vectors,
  including all candidate words, refresh order, definition reads and visibility
  of prior winners. Usability is a controlled callback in that comparison; native
  tests separately cover real usability, packed fields and distinct stats sources.
- F01714/F01715 now use typed stat rows and explicit snapshot storage.
  Native bindings preserve the `0x9640` high-end allocation before actor reads,
  empty lists, ordered duplicate slots, original partial-field copies, current
  actor-list restoration and release ordering. The original 28/25-instruction
  blocks match the actual native policies over 512 vectors, comparing every
  snapshot and restored-record byte; allocator/copy callees are controlled.
- Reentry, malformed/aliased rows and uncaptured restore slots are refused.
  Release clears the legacy binding, and production session invalidation forgets
  metadata without touching reclaimed storage. These are native safety
  differences, not claims about original malformed-input behavior. The copier
  removes both old bodies/prototypes and the obsolete allocation-cast adaptation;
  neither inlined helper had a standalone module entry. G00589 remains a
  compatibility bridge, not retired data.
- F01698/F01699/F01704 now use typed candidate simulation, hit weighting and
  evaluation policies. They preserve aimed halfword coordinate wrapping,
  first-nonignored-match byte weighting, rare rejection, special-mode bypass,
  relation signs, signed-halfword ratios and wrapped final scores. The native
  evaluator owns its condition rows instead of using `0x1F80018C`.
- A shared native decoder checks the complete result tail and 255-target
  capacity. Preview writes require a complete destination separate from the
  candidate, AI state and condition rows. Original module addresses remain
  bound; generated callers use registered entries and no old symbol remains.
- The new slice and `VR-AI-CANDIDATE-EVALUATION` record the separate promotion,
  verification and limited old-symbol retirement claims. The actual original
  instructions match the native policies over 12,608 vectors with zero
  mismatches. External calls use controlled callbacks; root vectors use exact
  powers-of-four distances. This does not establish full live preview or
  controller/reference gameplay parity.
- #334: container save enum constants and their consumers now use `k` prefixes;
  wire-edge multiplication starts in `std::size_t`; the ten-frame soft-reset
  wait is named; extraction is separated from command dispatch. The existing
  published-name policy in [QUALITY.md](QUALITY.md) now explicitly includes
  `ScreenDistortion` in the specific subsystem-namespace naming list. No
  baseline was changed. Full tidy reports 244 existing findings and zero new.
- F01700-F01703, the four condition terms inlined into `EvaluateActionCandidate`,
  are the preceding code checkpoint. `ActorStatusConditionScore` sums the signed
  weight of each set status bit 0-31; bit 12 counts only when the row's own AI
  state enables action cost. It then adds -128 for zero HP and -84 for HP below
  44/128 of maximum, comparing signed 32-bit products. `ActorHpRatio`,
  `ActorMpRatio` and `ActorEquipmentDpRatio` return `value*128/max`, truncated
  toward zero and not narrowed to 16 bits, zero without a maximum; the equipment
  term is weapon plus shield. The original looks the AI state up by the row's own
  id, not the loop slot, and the native binding does the same.
- The original 99 inlined instructions at `800DCDD4-800DCF5C` were interpreted
  from the actual BATTLE bytes against the real native cores over 20,000 vectors
  (the exact 44/128 boundary, zero maxima, bit 12 on and off, negative HP and
  results past 16 bits) with zero mismatches. The harness stays out of the
  repository because it derives from disc data.
- #333: CI had reported success while `hazards.py --check` failed on a clean
  `HEAD` export. `HAZARD_LEDGER.csv` was last synced in `34cdf8b7`, and a
  multi-line PowerShell `run:` step reports only its last native command's exit
  code, so the later `regression.py static` hid it. Each former multi-command
  step in `quality.yml` is now one command, [test_workflows.py](../tools/lint/test_workflows.py)
  rejects any multi-command step, and the ledger was resynced (763 to 746 sites).
  Thirteen new sites were recorded `open` and are untriaged; 30 rows left with
  rewritten code, all `open` with no notes.
- #335: `quality.py --files X --tidy` silently dropped every diagnostic outside
  the listed files, so a finding in a header reached through the listed source
  printed `0 findings` and exited 0. `main()` aliased `selected = scope` and then
  intersected in place, shrinking the scope that bounds `check_tidy`; it now
  copies the set. `--changed` and full runs were never affected. The new
  `test_selected_files_still_report_header_findings` puts a magic number in a
  header, selects only the including source, and requires that header finding
  and exit 1. It does not repair #334's findings, and whether this defect left
  four of those five files un-baselined is unconfirmed.
- F01637, `RefreshActionCandidateReadiness`, was published in `376c7969`;
  [CI 42705380792](https://github.com/JEschete/VS-NativePort/actions/runs/42705380792)
  passed. It preserves callback rereads, packed candidate bits, non-compacting
  first-survivor promotion, movement ordering, and the 16-bit threshold rules.
- F01697, `ApplySimulatedActionResults`, was the previous code checkpoint
  (`f6ba998e`). It applies cost types 1/3/6 first, then performer and ordered
  target effects. Repeated actor slots apply repeatedly; only operation 2
  restores; status clearing follows setting. Producer evidence identifies HP,
  MP, weapon DP, and shield DP. Native decoding validates the complete flexible
  target tail and 255-target capacity, preserving row IDs, maxima, and reserved
  bytes.
- These policies live in the existing
  [PlanningPrimitives.hpp](../Native/Game/include/Battle/Ai/PlanningPrimitives.hpp),
  [PlanningPrimitives.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitives.cpp),
  and [PlanningPrimitivesLive.cpp](../Native/Game/src/Battle/Ai/PlanningPrimitivesLive.cpp).
  Production callers and original C/module entries are retained. The
  obsolete function-local adaptations were removed through the copier; the
  owned F01704 evaluator now calls the typed condition terms directly.
- Borrowed outside-arena inputs, broader AI lifecycle, full battle-AI/M8
  gameplay, controller/reference parity, and EC/global/data/type retirement
  remain open. Local function promotion does not satisfy those wider gates.

## Verified

For actor-plan orchestration on base `9c79316e`:

- Final Debug and Release rebuilds and full suites pass: **5,269 cases /
  12,645,295 assertions each**, zero failures/skips. Both unchanged canonical
  replays match in each configuration; zero regressions.
- `[actor-plan],[actor-plan-live]`: 21 cases / 181 assertions;
  `[ai][planning]`: 178 cases / 189,399 assertions per configuration.
- Actual 459-instruction/native comparison: 4,096 vectors, zero mismatches,
  seed 1716, maximum 2,822 steps. It compares phase/callback traces, all
  candidate/pass/cache words, all ranking handles, exclusions, clock and arena
  outputs. Cases include counts 0-16, empty fallback, unsigned recovery
  extremes, threshold wrap, RNG ranking/count mutation, timing-pointer
  replacement and the captured actor across arena flag callbacks.
- Original `800DE3E4-800DEB0C` SHA256:
  `4c6aa5d18b74efc24ec22c91e9aea2f4fe740949700723c41631f114975bdceb`.
  Callees are controlled, including stat copies, enumeration, sorting,
  promotion and readiness. Failed-allocation selection stack words are
  initialized explicitly for #274, not claimed as undefined-stack parity.
- All 238 copier controls, all three ledger gates and static regression pass.
  Final scoped tidy: 48 existing findings, zero new; baseline unchanged.
  No new whole-tree quality scan was run. Hazard synchronization preserves
  741 sites and only updates three source locations.
- Two supported copy/project generations preserve **1,804 unique sampled
  paths and hashes**, including verified native source bytes. Manifest:
  `87385cb9d19430db799309357729923a1feadbb3548aeccd096ef862e8e31da0`.
- Evidence is in the session-local files directory: `planner_build_*`,
  `planner_focused_*`, `planner_planning_*`, `planner_regression_*`,
  `planner_differential.log`, `planner_copier_tests.log`, `planner_quality.log`,
  `planner_generation1/2.log`, `planner_generation_stability.log`, and the
  three gate logs. The session-local differential links the owned workspace,
  planning primitives, decision primitives, scalar math and distance sources.
- Native special-action fixtures do not establish ordinary-preview/controller
  gameplay. Invalid counts/storage are explicit diagnosed refusals; borrowed
  outside-arena inputs remain a caller contract. Pass/cache/exclusion globals
  and legacy actor/action records remain. No full battle-AI/M8 or complete
  EC-04 retirement claim.

For candidate enumeration on base `91717ee8`:

- Final Debug and Release rebuilds and full suites pass: **5,248 cases /
  12,645,114 assertions each**, zero failures/skips. Both unchanged canonical
  replays match in each configuration; zero regressions.
- `[candidate-enumeration],[candidate-enumeration-live]`: 17 cases / 186
  assertions; `[ai][planning]`: 157 cases / 189,218 assertions per configuration.
- Actual 454-instruction/native comparison: 4,096 vectors, zero mismatches,
  seed 1712. It compares all evaluation input/output words and original
  mode/action arguments, plus harmful/beneficial/eligibility, RNG, limb/height,
  cache and append traces. Cases include all 24 slots and 0-16 actors, signed
  score extremes/ties, zero/negative limb maxima, signed-height narrowing,
  late metadata mutation, candidate mutation during RNG and append exclusions.
  Maximum executed path: 61,821 instruction steps. External callees are
  controlled; this does not establish full planner/controller/reference parity.
- Original `800DD918-800DE02C` SHA256:
  `e7ba81487e25fd71ccdb0889b37e74f7ac942a8606ba5a9dff8e179a45122f9f`.
- All 238 copier controls, all three ledger gates and static regression pass.
  Final scoped tidy: 48 existing findings, zero new; baseline unchanged.
  No new whole-tree quality scan was run.
- Two supported copy/project generations preserve **1,804 unique sampled
  paths and hashes**, using the same deduplicated inventory as simulation.
  Verified native source bytes are unchanged. Manifest:
  `5851cac45720e10672fb1895203e60f773f5928654d8d5da80660cb97b76a1c5`.
- Evidence is in the session-local files directory: `enumeration_build_*`,
  `enumeration_focused_*`, `enumeration_planning_*`, `enumeration_regression_*`,
  `enumeration_differential.log`, `enumeration_copier_tests.log`,
  `enumeration_quality.log`, `enumeration_generation1/2.log`,
  `enumeration_generation_stability.log`, and the three gate logs.
- Only the old function symbol and its four local scratch candidates retire.
  Global tables and actor/action/candidate records remain. Malformed inputs
  are diagnosed native refusals; borrowed outside-arena inputs remain a caller
  contract. No full battle-AI/M8 or complete EC-04 retirement claim.

For planning simulation on base `23902fe7`:

- Debug and Release rebuilds and full suites pass: **5,231 cases /
  12,644,928 assertions each**, zero failures/skips. Both unchanged canonical
  replays match in each configuration; zero regressions.
- `[simulation-advance],[simulation-advance-live]`: 17 cases / 212 assertions;
  `[ai][planning]`: 140 cases / 189,032 assertions per configuration.
- Actual 237-instruction/native comparison: 4,096 vectors, zero mismatches.
  Every stat-record byte, readiness and all 25 callbacks per actor are compared,
  including callback-visible HP/MP/statuses. Vectors include 0-16 actors,
  aliased records, signed extremes, arbitrary masks/timers and zero intervals.
  Refresh callees are controlled here; native tests exercise real refreshes.
- All 237 copier controls, all three ledger gates and static regression pass.
  Final scoped tidy: 48 existing findings, zero new; baseline unchanged.
  This is not a new whole-tree quality scan.
- Two supported copy/project generations preserve **1,804 unique sampled paths
  and hashes**: tracked/untracked-not-ignored paths under `Native`, `tools/port`,
  `tools/scaffold`, `Docs/ledgers`, plus native projects/filters/properties and
  root solutions/properties. This deduplicated inventory is not the earlier
  1,812-entry count. Verified native source bytes are unchanged by regeneration.
  Manifest: `b148d74255c239aaf9d79f60aec498aa1e479869e0ec80abd91c520c71908ea7`.
- Evidence is in the session-local files directory: `simulation_build_*`,
  `simulation_focused_*`, `simulation_planning_*`, `simulation_regression_*`,
  `simulation_differential.log`, `simulation_copier_tests.log`,
  `simulation_quality.log`, `simulation_generation1/2.log`,
  `simulation_generation_stability.log`, and the three gate logs.
- No full planner/controller/reference gameplay or EC/global/data/type
  retirement claim. Borrowed outside-arena inputs remain a caller contract;
  malformed lists/records/needed limb indices are diagnosed native refusals.

For fallback selection on base `8b2eedfd`:

- Debug and Release rebuilds and full suites pass: **5,214 cases /
  12,644,716 assertions each**, zero failures/skips. Both unchanged canonical
  replays match in each configuration; zero regressions.
- `[fallback-selection],[fallback-selection-live]`: 10 cases / 548 assertions;
  `[ai][planning]`: 123 cases / 188,820 assertions per configuration.
- Original 114-instruction/native comparison: 4,096 vectors, zero mismatches.
  The verification ledger records the byte hash and callback limits.
- All 237 copier controls pass; scoped tidy has 48 existing findings and zero
  new. No baseline changes. The earlier full-quality result below belongs to the
  preceding lifecycle milestone, not a new whole-tree scan of this revision.
- All three ledger gates and static regression pass. Two supported generations
  preserve all 1,812 sampled hashes. Manifest:
  `6ba8d08f4d68b87cc4132cd1f024e24b07aa0e45ea4137f83848f5f9bb976daa`.
- Evidence is in the same session-local files directory: `fallback_build_*`,
  `fallback_focused_*`, `fallback_planning_*`, `fallback_regression_*`,
  `fallback_differential.log`, `fallback_copier_tests.log`,
  `fallback_quality.log`, `fallback_generation1/2.log`, and the three gate logs.

For the stat lifecycle milestone on base `109620c2`:

- Debug and Release rebuilds pass. Each full suite passes **5,204 cases /
  12,644,168 assertions**, with zero failures or skips. Both unchanged canonical
  replays match in each configuration; zero regressions.
- `[stats-lifecycle],[stats-lifecycle-live]`: 10 cases / 45,851 assertions.
  `[ai][planning]`: 113 cases / 188,272 assertions per configuration.
- Original-instruction/native comparison: 512 vectors, zero mismatches;
  allocation failure, request size, list order, slot stride and release order
  also checked. The verification ledger records exact byte hashes and limits.
- All 236 copier controls pass. Full quality: 244 existing findings, zero new;
  final changed-scope tidy: 48 existing findings, zero new. No baseline changes.
- Ownership/promotion, modernization, hazards and static regression gates pass.
  Hazard synchronization refreshes affected line numbers without new triage.
- Two supported copy/project generations preserve all 1,812 sampled hashes,
  including owned sources, generated output, ledgers and projects. Manifest:
  `50952e30b2210934ba1e1c2199cfe0d6dc18c27379d846838cd4767ec16c270a`.
- Evidence is in the same session-local files directory: `stats_build_*`,
  `stats_focused_*`, `stats_planning_*`, `stats_regression_*`,
  `stats_differential.log`, `stats_copier_tests.log`, `stats_quality_full.log`,
  `stats_quality_final.log`, `stats_generation1/2.log`, and the three gate logs.
  The only source edit after full quality/runtime verification clarifies a
  live-binding header comment; final scoped tidy includes it.

For the candidate milestone on base `8709974d`:

- Debug and Release rebuilds pass. Each full suite passes **5,194 cases /
  12,598,317 assertions**, with zero failures or skips. Canonical
  `silver_wolves` (2,400 frames) and `first_lever_room` (900 frames) match
  unchanged baselines in both configurations; zero regressions.
- `[candidate-evaluation],[candidate-evaluation-live]`: 17 cases / 4,696
  assertions. `[ai][planning]`: 103 cases / 142,421 assertions per configuration.
- Original-instruction comparison: 2,000 simulation, 4,608 hit-weight and
  6,000 evaluation vectors; zero mismatches. Source byte hashes and callback
  limits are recorded in the verification ledger.
- All 235 copier controls pass, including the relocated owned snapshot
  producer and three original-address entries. Full tidy: 244 existing
  findings, zero new. Changed-scope quality after the Python control update:
  zero findings. No baseline was changed.
- Ownership/promotion, modernization and hazard gates each report zero
  violations after synchronization. Static regression checks also pass.
  The hazard sync removes the old F01698 unchecked lookup and refreshes
  affected line numbers; it adds no new site or triage claim.
- Repeated supported generation preserves owned sources and the complete
  tracked diff. Manifest SHA256:
  `68e429c54019d243b056deacf957c73db66655b1a8b782198e6817d8dd0995f1`.
- Logs are under the session `5d4d5fa9-cd6f-4ab6-a214-97513ed24383` files
  directory: `candidate_build_*`, `candidate_planning_*`,
  `candidate_regression_*`, `candidate_differential.log`,
  `candidate_copier_tests.log` and `candidate_quality_*`.

For the #334 repair on base `5e172a60`: Debug and Release rebuilds pass, and
`[container-save],[sphere][terrain],[screen-warp],[session],[audio][stream],[content]`
passes 273 cases / 1,636,003 assertions in each configuration. Six synthetic
extractor CLI cases per configuration preserve the pre-refactor exit codes and
all ten output files byte-for-byte, including manifest, raw files and stream
assets, plus explicit no-stream and write-error paths. Full
`python tools\lint\quality.py --tidy --jobs 4` reports 244 findings, zero new,
exit 0; `baseline.json` is unchanged. Ownership, modernization and hazard
checks each report zero violations. Logs are session-local
`quality_334_full.log`, `build_334_<config>.log`, and `tests_334_<config>.log`.

These are results for the working tree on base `90e5fbfe` subsequently published
unchanged in the three commits above (the F01700-F01703 code, the #333 CI fix and
the #335 quality gate fix), not new runtime verification performed by editing this document. The
#335 change touches only `tools/lint`; the build, test and replay results below
were taken before it and do not depend on it. All three exact-revision CI runs
passed; the hazard check and workflow-exit controls now pass as independent steps.

- Full rebuilt Debug and Release: **5,177 cases / 12,593,621 assertions each**;
  zero failures or skips. Canonical `silver_wolves` (2,400 frames) and
  `first_lever_room` (900 frames) match unchanged baselines; zero regressions.
- F01700-F01703: 10 focused cases / 117 assertions (seven pure-core, three
  live-binding); `[ai][planning]` 86 cases / 137,725 assertions; `[ai]` 525
  cases / 3,131,567 assertions. F01697 was 11 cases / 95 assertions.
- Original MIPS evidence from actual BATTLE bytes: the 99 inlined instructions
  at `800DCDD4-800DCF5C`, SHA256
  `6506a43a5c906fd492cbb9c09cc37ef7510d4a206c026a9b021cfe5badb1d50c`; all of
  F01704, 219 instructions at `800DCC94-800DCFFC`, SHA256
  `f19b68e0b0c641f6fa5a5a53bcdb015009a69dbaadf03a87cc221813fe92237f`; the
  32-byte status table at `0x80069BD0`, SHA256
  `5d33fbf3c199b41f6535b3a0331f542b1e430fb6acff5f0dc218f054a01ea38a`. The
  20,000-vector differential run had zero mismatches.
- All 202 copier controls pass. Changed-scope `quality.py --changed --tidy`:
  48 findings, all baselined, zero new, exit 0; source-scoped quality: zero
  findings. **Full-scope `quality.py --tidy` exits 1**: 267 findings, 23 new,
  all in five files this work does not touch (`NativeContainer.hpp`,
  `SphereTerrain.hpp`, `RadialWarp.hpp`, `Session.cpp`, `Extract/main.cpp`) and
  identical on a pristine export of `90e5fbfe`; this work adds none (#334). No
  baseline or suppression was changed.
- `gates.py`, `modernization.py` and `hazards.py` pass `--sync --check` with
  zero violations (746 hazard sites). `test_quality.py` (31) and
  `test_workflows.py` (5) pass. Two final supported generation runs preserve
  all 1,803 hashed paths (tracked or untracked-not-ignored files under
  `Native`, `tools/port`, `tools/scaffold`, `Docs/ledgers` and the solution;
  this is not the definition behind the older 1,761-path record). Manifest
  SHA256: `df4fcfc298dc8be0cceb20fc04ce96638da67aa0b496c9b38a5de9c808248e9b`.
- #335: the new positive control fails on a pristine `90e5fbfe` export (every
  stage reports 0 findings, exit 0) and passes on the fix. `quality.py --files
  Native/Game/src/Save/NativeContainer.cpp --tidy` printed `0 findings` before
  and now prints the 19 header findings and exits 1. `quality.py --changed`
  reports 0 findings and 0 new; `baseline.json` is byte-identical.

## Next Work

1. Recheck `git status`, HEAD, and the exact-revision CI before resuming.
   Preserve intervening owner edits; archived results do not verify a new tree.
2. Continue with the F01636/F01641/F01642/F01644/F01660 blocked-movement and
   movement-history group from the [Stopping Point](#stopping-point), then
   update #276. Keep #337 open until its in-game reachability is established.
   Do not close #276: 928 remain.
3. Continue ordered row 28 with the flight group (F01729, F01730,
   F01732-F01734), then target approach, facing and detour (F01748, F01749,
   F01751), Behavior, Movement, OrbitingBoss and Perception as listed by
   [PROMOTION_PLAN.csv](ledgers/PROMOTION_PLAN.csv).
   F01708-F01710/F01716/F01717/F01721-F01728/F01735-F01747/F01750/F01793/
   F01810-F01818 are verified dependencies, not proof of remaining AI behavior
   or gameplay.
4. Read original source and actual assembly, then trace callers, random-state
   use, candidate-list mutation, failure and reset paths before implementing.
   Reuse the owned candidate, evaluation, stat-copy and lifecycle policies.
5. Test the deciding core first, then native/caller/module bindings. Make
   generated changes through the copier or owned registry, never by patching
   generated output. Remove only adaptations made obsolete by the owned body
   (`promote.py prune-adaptations`).
6. Use the [Promotion Throughput](#promotion-throughput) workflow: focused
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
  match delivered scope. Do not declare the remaining 928 functions complete.
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
- [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md) contains old failed attempts and
  evidence paths. Consult it only for a specific question; do not reload it wholesale.
