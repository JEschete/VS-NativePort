# Current Handoff

Updated 2026-11-12. Rows 28 (battle AI) and 29 (damage/status) of the C++ promotion work (#276) are source-complete and published; row 30 (battle-actions) is active. The earlier perception promotion is the
F01638-F01640/F01643 perception group in `33ad4f3e`
([CI 43073456695](https://github.com/JEschete/VS-NativePort/actions/runs/43073456695)). Each published row-28
commit and its exact-revision CI run are recorded on [#276](https://github.com/JEschete/VS-NativePort/issues/276)
and in the ledgers. The per-function publish list, Latest Work and Verified sections that used to be here are in the
[archived handoff](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF.md).
The step-by-step procedure is [WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md).
See [Active continuation](#active-continuation-2026-11-11) for current evidence and the next group.

## Active continuation (2026-11-11)

The owner requested completion of the original 900-function cohort and progress
tracking in `REMAINING_FUNCTIONS.csv`. Work resumed from clean `0e8989ad`;
exact-revision Source Quality `43105524312` passed. The earlier stopping point
below is retained as historical evidence, not an instruction to pause.

### Latest row-30 group

F00469-F00475 are locally verified on `7b797783` in `BasicActionEffects`.
Five typed policies cover seven original addresses, preserving the three
source-success aliases, queued instant-death/armor effects and script-flag
side effects. Live mediators use the actual VOID5 type. All 18 old-symbol
references are gone.

- Original span: 0x80084228-0x80084390, 90 instructions, plus the real original
  status-removal helper. Per-ID hashes and evidence:
  `build\scratch\basic-action-effects\ledgers-final.json`.
- `basic_action_effects`: 14,336 vectors, zero mismatches, 31/31 mutants.
  Per-ID native variants detect 1024/410/1024/1024/819/819/1024 mismatches
  respectively over 1,024 vectors per ID.
- Group `build\promotion-check\group-20261112-050611`, 434 seconds:
  Debug/Release each pass 6,060 cases/12,841,649 assertions; coupled selection
  104 cases/19,486 assertions; own tests 20/17,758. Both replays, copier
  stability and controls pass; tidy five existing findings, zero new.
- Thirteen diagnostic refusal probes, post-helper target/HP rereads, unused
  argument paths and actual-table ABI rejection pass. Script-flag calls are
  controlled services with exact index/value traces; status removal executes
  real original instructions.
- Evidence: `build\differential\basic-action-effects-full.log`, native variant
  logs and `build\scratch\basic-action-effects\cohort-audit.json`. Publication
  and exact-revision CI pending. No row30, gameplay or EC/M8 completion claim.

Row 29 is published in `7b797783`, green on
[CI 43193683736](https://github.com/JEschete/VS-NativePort/actions/runs/43193683736):
all 88 IDs reconciled, 85 promoted and three retired. [#351 is closed](https://github.com/JEschete/VS-NativePort/issues/351#issuecomment-6820264328).
Its supported healing contract and undefined-input refusals remain verified;
no original numeric return behavior was invented for retired sentinels.

C6 damage policies are published in `85fb812e`, green on
[CI 43188974850](https://github.com/JEschete/VS-NativePort/actions/runs/43188974850),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6819687952).

B8 incoming-stat factors are published in `d29b8d86`, green on
[CI 43183509421](https://github.com/JEschete/VS-NativePort/actions/runs/43183509421),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6818895374).

A8 stat factors are published in `93b3ac07`, green on
[CI 43179698344](https://github.com/JEschete/VS-NativePort/actions/runs/43179698344),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6818328299).

The hit/stat dispatch group is published in `d592578e`, green on
[CI 43175458376](https://github.com/JEschete/VS-NativePort/actions/runs/43175458376).
[#350 is closed](https://github.com/JEschete/VS-NativePort/issues/350#issuecomment-6817641744).
Those three sentinel retirements remain distinct from C++ promotion and
numeric behavior verification.

The six hit modifiers are published in `42acb462`, green on
[CI 43170979793](https://github.com/JEschete/VS-NativePort/actions/runs/43170979793),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6817073762).

The actor-vitals group is published in `d1dd80f4`, green on
[CI 43166632406](https://github.com/JEschete/VS-NativePort/actions/runs/43166632406),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6816362626).

The lifecycle group is published in `c888aab4`, green on
[CI 43160888226](https://github.com/JEschete/VS-NativePort/actions/runs/43160888226),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6815535713).

The seven supporting leaves are published in `8e21f5b7`, green on
[CI 43156876978](https://github.com/JEschete/VS-NativePort/actions/runs/43156876978),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6814998528).

The defense/cure group is published in `fcc50d5d`, green on
[CI 43152730395](https://github.com/JEschete/VS-NativePort/actions/runs/43152730395),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6814680374).

The preceding ailment/attack group is published in `fff60643`, green on
[CI 43149478721](https://github.com/JEschete/VS-NativePort/actions/runs/43149478721),
with evidence on [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6813988874).

The preceding F00443-F00450 group and #349 are published in `e8565059`;
[CI 43146186212](https://github.com/JEschete/VS-NativePort/actions/runs/43146186212)
passed. [#349 is closed](https://github.com/JEschete/VS-NativePort/issues/349#issuecomment-6813404271)
with compiler-negative-control and actual typed-dispatch evidence. Its
16-function differential is archived at
`build\differential\status_queue-F00443-F00450.log`. Incidental MIPS returns
remain driver-only projections, not production callback APIs.

### First row-29 group (published)

F00405/F00406/F00437-F00442 are locally verified on `b6c5c42a` in the
`StatusQueue` core/live module. Typed rules, target status/immunity/exemption
and queued additions/removals replace the global reads; six status handlers
share the rule kernel while preserving their own rule and feedback choices.
The eight old symbols have zero references (62 before). Production encounter
and generated action-resolution consumers use the checked live API.

- Original spans: 0x80081020-0x80081130 (68 instructions, SHA-256
  `64025fc62cd0f26206a1c079b5b0c41878fb9aab80a366befe9215f89c23c9c6`)
  and 0x80083998-0x80083B78 (120 instructions, SHA-256
  `107dfe5fc9b8f9c2f2dae0acd89f076721670639ec0685cedce907b363ae23b9`).
- `status_queue`: 8,192 vectors, zero mismatches; 46/46 mutants detected.
  Original primitives execute through the handlers without status-logic
  stubs. Eight native broken variants detect 823/152/154/155/157/159/149/166
  mismatches of 2,048 vectors respectively.
- Final group `build\promotion-check\group-20261111-212737`, 384 seconds:
  Debug/Release each pass 5,786 cases/12,652,032 assertions, with 23 focused
  cases/239 assertions. Both canonical replays match in both configurations;
  copier stability/controls pass; tidy reports five existing findings, zero
  new. The first group failed only the case's import sorting; it was fixed,
  the differential rerun, and the complete group gate repeated.
- Canonical ledgers and progress refresh pass. Local evidence:
  `build\differential\status_queue.log`, eight `status_queue-F*-variant.log`
  reports and `build\scratch\status-queue\ledger.json`.
- Limits: distinct valid synthetic input storage in the differential; missing
  rules and invalid live pointers/indices refuse diagnostically. The live
  32-rule and 42-actor tables remain compatibility dependencies. No complete
  interactive status coverage or EC-04/M8 retirement claim. Published in
  `1c420d3f`; exact-revision
  [CI 43142573923](https://github.com/JEschete/VS-NativePort/actions/runs/43142573923)
  passed.
- Subsequent caller tracing confirmed [#349](https://github.com/JEschete/VS-NativePort/issues/349):
  the real effect-mediator table takes `void` callbacks, but the six promoted
  Live handlers retained misleading original `int` declarations. A native
  compiler assignment probe rejects the types; no gameplay crash is claimed.
  Commit `e8565059` repaired those six bindings and its eight new handlers,
  with compile-time and actual typed-dispatch tests; #349 is now closed.
  Green builds/replays alone did not prove this contract. The two status
  primitives retain their consumed `int` masks.

### Completed battle-AI groups

The candidate-movement batch is published in `fc0e2525`; exact-revision Source
Quality [43136464204](https://github.com/JEschete/VS-NativePort/actions/runs/43136464204)
passed. Publication and detailed evidence are recorded on
[#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6812029760).

The first batch is F01649-F01654 in `CandidateMovement.hpp/.cpp` and
`CandidateMovementLive.hpp/.cpp`: candidate movement, combat pursuit,
planned-target approach, close follow, enemy pursuit and guard movement.
The six old symbols had 24 code references; all are removed. Generated callers
and all six original module entries use the checked live bindings.

**Progress: 105 of the original 900 resolved (102 promoted, 3 retired); 795
remain** (794 generated plus F01416 owned WIP). Modernization is 2,946/3,741;
C++ promotion is 2,947 plus
one WIP. These remain separate measures.

- Original range: 0x800D9E18-0x800DAA6C (exclusive), 789 instructions,
  SHA-256 `9f02fb28f761f8b846c643243bf60e6bc5cc335caec80929103dbb087cee375d`.
- `differential.py candidate_movement`: 6,144 vectors (1,024/function),
  zero mismatches; 73/73 mutants detected on 1,152 vectors each. External
  callees are queued stubs, including mutation effects; original scalar math
  instructions are followed. Evidence: `build\differential\candidate_movement.log`.
- Native broken variants detect 2/47/27/111/78/40 mismatches respectively
  for F01649-F01654 over 1,152 vectors each. Reports:
  `build\differential\candidate_movement-F01649-variant.log` through
  `candidate_movement-F01654-variant.log`.
- Initial focused expectations incorrectly assumed exact integer square roots.
  Running the original instructions confirmed 10000 -> 99, 40000 -> 199 and
  90000 -> 298; the tests now use those values and the actual range boundaries.
  The differential also caught stale candidate reads across callbacks; the
  core preserves the entry-time target and range while rereading the fields
  the original reloads. Dedicated tests cover both callback cases.
- The first group check passed both builds, full suites and replays but failed
  tidy on optional access and signed-byte diagnostics. These were corrected
  without changing baselines; signed linked IDs intentionally preserve the
  original `lb` semantics rather than converting negative IDs to unsigned.
- Final group gate: `promotion_check.py group --focused "[candidate-movement]"`,
  `build\promotion-check\group-20261111-203535`, 371 seconds. Debug and Release
  each pass 32 focused cases/174 assertions and 5,753 full-suite cases/
  12,651,820 assertions. Both `silver_wolves` (2,400 frames) and
  `first_lever_room` (900 frames) match in both configurations. Copier
  stability and controls pass; tidy reports four existing findings, zero new.
- Canonical gates, modernization and hazards sync/check pass. The progress CSV
  retains 900 rows with six `done`/`promoted` rows. All 18 promotion-tool
  controls pass, including original-cohort retention, unchanged snapshot,
  retirement versus WIP, byte-stable rerun and failure-before-write.

The live boundary retains the actor-record, AI/frame and room tables. Missing
required records, negative linked IDs and invalid rosters are refused with
diagnostics. Empty/unused candidate paths remain lazy. Neither these tests nor
the canonical replays establish every pursuit/guard gameplay path, complete
battle-AI/M8 behavior, or EC-04 retirement.

The final row-28 batch, F01656-F01659, is now locally verified on `fc0e2525`.
`ActionExecution` and its checked live binding implement candidate execution,
planned actions and frame-budget dispatch; escort movement shares the follow
prefix in `CandidateMovement`. Generated `Behavior.cpp/.h` are retired and
their four old symbols have zero consumers (17 before).

- Original range: 0x800DAAD8-0x800DB0BC (exclusive), 377 instructions,
  SHA-256 `1559513320faef91480dbd464ee03abfb539cad367eca3e435866f70ea2408d3`.
- `differential.py action_execution`: 4,096 vectors, zero mismatches;
  47/47 mutants detected. Four native broken variants detect 81/212/84/82
  mismatches over 1,024 vectors each. The shared `candidate_movement`
  differential was rerun: 6,144 vectors, zero mismatches, 73/73 mutants.
- Group `build\promotion-check\group-20261111-205725`, 385 seconds:
  Debug/Release each pass 5,773 cases/12,651,925 assertions; combined focused
  tests pass 52 cases/279 assertions. Both canonical replays match in both
  configurations; copier stability and 272 tool controls pass; tidy reports
  five existing findings, zero new.
- Row-28 audit: all 89 original cohort IDs are promoted; no generated
  function remains in the slice. Plan row 28 is marked done. This is source
  promotion, not M8 or EC-04 completion. External differential callees remain
  queued stubs, and #337 remains suspected with diagnostic refusals preserved.
- Local evidence: `build\differential\action_execution.log`, the four
  `action_execution-F01656-variant.log` through `F01659` reports, and
  `build\scratch\pursuit\action_execution_ledgers.json`. Published in
  `b6c5c42a`; exact-revision
  [CI 43139398025](https://github.com/JEschete/VS-NativePort/actions/runs/43139398025)
  passed, recorded on
  [#276](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6812436244).

Continue ordered row 30 (`battle-actions`, 84 of 91 still remaining) with
F00486-F00493, eight random-stat producers. Their 153 instructions are read
in `build\scratch\status-queue\action_effect_leaves.txt`. Preserve actual
VOID mediator types, unconditional bounded-four draws even during preview,
post-RNG pending-field reads, signed-halfword wrapping and the distinct
increase/decrease mode bits. Reuse typed result records. Further triggered
recovery and dispatch groups have source/assembly analysis recorded; do not
silently jump ahead of this ordered package.

[#351](https://github.com/JEschete/VS-NativePort/issues/351) records F00421's
latent healing-affinity defect: drawn encoded-affinity 1 reads an unwritten
stack halfword. The shared original-MIPS probe returns 100 versus 200 when
only that synthetic residue changes; both original healing definitions use
encoded affinity 6 and avoid the path. The D5 supported contract and explicit
refusals are published and CI-verified; #351 is closed.
No zero intermediate or gameplay-failure claim is invented. Evidence:
`build\scratch\status-queue\healing-affinity-probe.py`
and its log.
Finish the remaining damage/hit functions afterward. Keep every original
cohort row in the progress CSV, including
resolved ones; the `snapshot` column stays `44fa8847`. F01416 remains the one
owned WIP and must not disappear from the remaining count.

## Prior checkpoint (before this continuation)

- Branch `imp/01-ma04-gates`. Last code commit: `33ad4f3e` (perception group). It is followed by documentation commits
  only: `44fa8847` (handoff), `8db4f46e` (the remaining-functions snapshot) and the Docs cleanup described below.
- **900 functions remain** (after the perception group in `33ad4f3e`).
  Modernization: 2,844 done of 3,744. C++ promotion: 2,845 promoted plus one
  frame-orchestrator WIP. These are separate measures. Row 28 has 10
  generated functions left. [REMAINING_FUNCTIONS.csv](ledgers/REMAINING_FUNCTIONS.csv)
  lists all 900, with purpose, location, plan row and status, as a snapshot at
  `44fa8847`. `promote.py ledgers` now refreshes its progress columns after
  the canonical ledger checks pass. It retains all 900 original rows,
  including resolved entries, and keeps their original snapshot/provenance.
- Original 3,479-function cohort at `ffd1a8e1`: 2,505 promoted, 16 retired,
  957 generated, one WIP; 2,521 resolved. This continuation resolved 72 more
  than `6c8ac0b9`.
- Plan row 27, field gimmicks/doors, is done for C++ ownership: its original
  51 IDs audit to 46 promoted and five retired. Row 28, battle AI, is active.

### Docs cleanup (2026-11-11)

On owner request, obsolete and duplicate documents were removed from `Docs`. Their last versions remain at
`https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/<file>`.

| Removed | Where it went |
| --- | --- |
| `HANDOFF_HISTORY.md` (847 KB) | Archive link only; it held old checkpoints and evidence paths |
| `CodeQC Reports\Code_Quality_Report_20261022.md` | Its findings map to existing issues; the one untracked finding, QC-18, is #348 |
| `CUTSCENE_PLAYTEST_REMAINING.md` | The 41 unchecked scenes are tracked in #346 |
| `DATA_MIGRATION_PLAN.md` | Summarized on #47, its home issue |
| `ASSEMBLY_GAPS.md`, `ASSEMBLY_GAPS.csv`, `ASSEMBLY_GAP_COVERAGE.csv` | `audit_assembly_gaps.py` writes to `build\quality\assembly-audit`; `--publish` is opt-in |
| `RUNTIME_TRACING.md` | Deferred work tracked in #347; `TRACE_QUEUE.csv` stays |
| `EMULATION_COMPATIBILITY_AUDIT.md` | Merged unchanged into the plan as [§15](NATIVE_SOURCE_PORT_PLAN.md#15-emulation-compatibility-register) |
| `ledgers\SOURCE_FILE_LEDGER.md` | Never updated after generation; `build_ledgers.py` no longer writes it |
| `ledgers\NATIVE_SYMBOL_REVIEW.csv` | Drifted and not in CI (#273); the inventory tool stays |

`SDK_API_LEDGER.md` is kept: it is maintained by hand as shims retire. `COMBAT_LAB.md` is now a usage guide, and
`NAMING_CONVENTIONS.md` §11.3 keeps only the rules from the old dated logs. The owner's `Decomp Process.md` notes and
the Reddit draft were local, untracked files: the notes moved to `C:\Tools\Decomp Process.md` and the draft was deleted.

## Stopping Point

The continuation is active, not paused. Row 28 is published and green. The
row-29 checkpoint has all 88 IDs resolved (85 promoted, three retired),
published and green in `7b797783`. #349, #350 and #351 are closed.
Row 30's seven basic effect producers are locally verified; publish and confirm
CI, then continue with the eight random-stat producers.
The supporting group covers F00339/F00340/F00342/F00343/F00347/F00348; all
353 original instructions are read in `build\scratch\status-queue\hit_modifiers.txt`.
[#350](https://github.com/JEschete/VS-NativePort/issues/350) tracks the separate
F00341/F00407/F00436 empty-callback return/ABI defects. The source/definition
audit and typed retirement guards are published and CI-verified in `d592578e`;
the issue is closed. No original numeric return behavior is claimed.
The later F00344/F00345/F00346 attribute modifiers have their 433 original
instructions read in `build\scratch\status-queue\hit_attribute_modifiers.txt`;
reuse `Core::Math` wrapping arithmetic and `R3000Quotient`, including its
defined zero-divisor behavior. Keep F00412/F00414/F00426 and F00418/F00428
aliases together when grouping the subsequent stat calculators.
The perception checkpoint below is historical.

Stopped on owner request (2026-11-11) after the perception group, published
in `33ad4f3e` on base `3aa5b806` and green on the exact revision
([CI 43073456695](https://github.com/JEschete/VS-NativePort/actions/runs/43073456695);
[#276 comment](https://github.com/JEschete/VS-NativePort/issues/276#issuecomment-6803558605)):

- F01638 `IsTargetOccludedAtBodyHeight`, F01639
  `IsTargetOccludedAtPointHeight`, F01640 `UpdateTargetPerception` and F01643
  `UpdatePerceivingActor` are typed in the pure `ActorPerception` module over
  `TargetPerceptionWorld` and `ActorPerceptionWorld`. `ActorPerceptionLive`
  binds them at the module addresses 0x800D87E8 and 0x800D8EF4, and
  `FrameUpdateLive` calls `UpdateLiveActorPerception`. The generated
  `Perception.cpp` keeps only three data definitions (`D_800F16F4`,
  `D_800F58D4`, `D_800F58E0`). The four old symbols had 18 code references
  and have none.
- Differential `perception`: one 852-instruction range (0x800D87E8-0x800D9534,
  SHA-256 98f4c68b...6013), 4,096 vectors, 0 mismatches and 100/100 mutants.
  The interpreter gained the GTE `GPF` instruction for the facing and distance
  arithmetic. The terrain segment test and the pass's 14 callees are stubbed
  with queued answers.
- One native broken variant per function is detected (of 1,024 vectors): F01638
  body extent plus one (172), F01639 default point height 65 (46), F01640
  marking without activity (174), F01643 detour step wrapping to 0 (60) and the
  HP rule boundary (29).
- Limits: the differential aliases the actor-record table to the battle-actor
  models, so the live binding's resolution of targets and occluders through
  `Battle_actors[id]->model` (kept for auxiliary AI targets without an
  actor-record entry) is not compared. The live bindings refuse a missing
  model, pose, link, HP or listed actor where the original read through them.
  No complete EC-04 or battle-AI/M8 retirement claim.
- Group check on base `3aa5b806` (`promotion_check.py group --focused
  "[actor-perception]"`, `build\promotion-check\group-20261111-124014`, 998 s):
  focused 54 cases and 327 assertions in Debug and Release; the full suite
  5,721 cases and 12,651,646 assertions in both configurations with 0
  failures; `silver_wolves` (2,400 frames) and `first_lever_room` (900 frames)
  match their baselines in both configurations; copier tests,
  `test_encounter_fixups.py` (211) and `test_differential` (30) pass;
  changed-file tidy 177 existing findings and 0 new. The 177 are in unchanged
  translation units (`SequenceTests.cpp`, `LayoutAsserts.cpp` and others) that
  include the changed `Distance.hpp`.

Next groups (row 28 has 10 generated functions left, all in the generated
`Behavior.cpp`, `BATTLE/BATTLE.PRG/71618.c`):

1. F01649-F01654 (`func_800D9E18`-`func_800DA738`, 71618.c:511-957): the
   pursuit, flee and follow movement choices for an action candidate.
2. F01656-F01659 (`func_800DAAD8`-`func_800DAED0`, 71618.c:958 onward): the
   retreat hostility adjustment and the per-frame action and movement decision
   within the frame budget.

Row 28 then needs its completion audit (its original IDs promoted or retired)
before row 29 starts.

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
interactive, sequenced, device and cited run (those cited at the time in
`COMBAT_LAB.md` or the now-archived `HANDOFF_HISTORY.md`). Since `71973e51` (#339, closed)
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
   only the first N vectors, which keeps large cases fast. Reports go to `build\differential\<case>.log`.
   `--replace SOURCE OLD NEW` builds a native broken variant from a patched
   copy of one source (the tree is untouched) and expects mismatches; use it
   for each function's broken-variant check instead of editing and reverting
   the source. The F01813
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

Result: from the tooling commit `48ab62e3` (2026-11-10 21:44) to `3aa5b806`
(2026-11-11 10:42), 11 commits promoted 54 functions in about 13 hours, about
14 minutes per function against the earlier 33-63, and every function still
has its own MIPS differential, mutants and broken variant. With the perception
group (`33ad4f3e`, 12:59) the total is 58 functions in 12 commits in about
15 hours, including writing the procedures and splitting oversized tests.
A group check takes 15-18 minutes, about 11-12 of them in changed-file tidy,
so lint first with `quality.py --files` (seconds) to avoid a second run for a
style finding. The checklist, including the test-size and ledger re-apply
rules learned on the perception group, is
[WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md).

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

Earlier handoffs, including `Archived Handoff: 2026-11-09 / f6ba998e`, are in the
[archived HANDOFF_HISTORY.md](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF_HISTORY.md).
Their current/next/pending statements are historical, not instructions or fresh verification.

## Next Work

1. Recheck `git status`, HEAD, and the exact-revision CI before resuming.
   Preserve intervening owner edits; archived results do not verify a new tree.
2. Publish the seven basic action effects and confirm exact-revision CI.
   Keep #337 open until its in-game reachability is established. Do not close
   #276: 795 remain.
3. Continue row 30 with the eight random-stat producers above, following
   [PROMOTION_PLAN.csv](ledgers/PROMOTION_PLAN.csv).
   F01630/F01636/F01638-F01644/F01660/F01678-F01680/F01705-F01710/F01716/
   F01717/F01719/F01721-F01751/F01793/F01810-F01818/F01846-F01849 are verified
   dependencies, not proof of remaining AI behavior or gameplay.
4. Read original source and actual assembly, then trace callers, random-state
   use, candidate-list mutation, failure and reset paths before implementing.
   Reuse the owned candidate, evaluation, stat-copy and lifecycle policies.
5. Test the deciding core first, then native/caller/module bindings. Make
   generated changes through the copier or owned registry, never by patching
   generated output. Remove only adaptations made obsolete by the owned body
   (`promote.py prune-adaptations`).
6. Follow [WORK_PROCEDURES.md](../.github/WORK_PROCEDURES.md) and the
   [Promotion Throughput](#promotion-throughput) workflow: focused
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
  match delivered scope. Do not declare the remaining 900 functions complete.
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
- The [archived HANDOFF_HISTORY.md](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/HANDOFF_HISTORY.md) contains old failed attempts and
  evidence paths. Consult it only for a specific question; do not reload it wholesale.
