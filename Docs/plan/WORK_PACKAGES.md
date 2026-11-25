# Native Source Port Plan: Work packages M0–M4

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · M5–M10: [WORK_PACKAGES_2.md](WORK_PACKAGES_2.md)

## 14. Implementation work packages

### 14.1 Execution contract and dependencies

These packages refine [§1](ROADMAP.md#1-phases-and-sub-phases); they do not replace its full-content/parity
obligations. Status is **not accepted** until the package gate has evidence. Existing code is a starting point, not a
reason to mark the package completed.

Every package records: scope/non-goals, owning systems and affected files, prerequisites, API/data contracts,
generated-versus-owned implementation responsibility, failure policy, tests/reference artifacts, EC dependencies,
remaining work and retirement evidence. Generated files are edited in place, keeping their `// decomp:` provenance
(D24); the copier is retired and must not be rerun. Proposed APIs/types below are design deliverables, not claims
they exist today.

| Package | Prerequisites | Product gate / compatibility scope |
|---|---|---|
| M0 Baseline and boundary inventory | Current source/ledgers | All phases; EC-01–EC-09 |
| M1 Safe module data and dispatch | M0 | Boot/title prerequisite; EC-04/05 |
| M2 Reproducible verification | M0; M1 for integrated boot runs | All phases; EC-06 |
| M3 Interactive title and lifecycle | M1, M2 | Phase 1b input/control; EC-05/06/07 |
| M4 Native rendering and typed visual assets | M1, M2; M3 title checkpoint | Phase 1b/1c visuals; EC-01/03/04 |
| M5 Sequenced audio and streaming | M1, M2; Q5 decision | Phase 1b and later audio; EC-02/04/06 |
| M6 Movie conversion and playback | M1, M2, M3; M5 source-output contract | Phase 1b FMV; EC-04/08 |
| M7 One complete field/actor slice | M3, required M4/M5/M6 slice outputs | Phase 1c field; EC-01/03/04/05 |
| M8 One complete combat slice | M7, required M5 audio | Phase 1c battle; rendering/audio/data consumers |
| M9 Menus, inventory and persistence | M7, M8; Q6 decision for saves | Phase 1d; EC-04/05/07/09 |
| M10 Full coverage and retirement closure | Accepted M3–M9 slices | Phase 1e and Phase 2e; all EC issues |

After M2, rendering/audio/converter implementation can proceed independently once shared contracts are agreed.
M6 depends on M5's stream lifecycle/clock contract, not completion of every music/SFX feature. Decoder work is
incremental by slice; all-content conversion remains a Phase 1 exit requirement. Do not block a minimal validated
decoder on unrelated external tooling, or claim an unimplemented format is supported.

### M0 — Baseline, exceptions and ownership

**Scope/files:** the EC register ([§15](EC_REGISTER.md#15-emulation-compatibility-register)), this plan,
`BUILDING.md`, ledgers, `tools/lint`, generated-file ownership checks.

1. Record the exact revision/configuration, source status and existing build/test/capture evidence separately.
2. Inventory hardware-shaped behavior both inside and outside `Legacy`: original addresses, byte workspaces,
   packet/voice protocols, callbacks and renamed adapters. Map every exception to an EC ID and owning package.
3. Record required versus optional behavior at each checkpoint. Decide which diagnostics invalidate a gate.
4. Reconcile Q4 and document deferred decisions with the package they block. Update build instructions when actual
   prerequisites/CLI/output behavior differs; never publish planned commands as implemented.
5. Establish narrow lint baselines and generation-ownership checks without overwriting user edits blindly.

**Gate:** every known exception has an owner, consumers, replacement and retirement gate. Snapshot claims link to
evidence. Neither an architecture decision nor a passing test is inferred from a documentation change.

### M1 — Validated module data and dispatch initialization

**Scope/files:** `Content/ModuleData.hpp`, `Content/src/ModuleLoader.cpp`, generated `Game/src/Boot/ModuleData.cpp`
and `ModulePointers.cpp` and their source/layout metadata (produced by the retired copier, now edited in place, D24);
EC-04/05. This hardens the bridge, not the final asset architecture.

1. Verify each required table's source module, offset, element count and extent against declarations and source
   evidence. Resolve overlapping symbols/aliases explicitly; do not interpret an entire inferred data span as pointers.
2. Separate plain bytes, typed fields, data references and callable references. Check source bytes and destination
   capacity independently; original sizes and `sizeof(native)` are not interchangeable for pointer-bearing types.
3. Resolve functions through signature-correct registrations. For interior data references, use a proven identical
   layout or an explicit field/element mapping; never assume original byte offsets survive x64 expansion.
4. Produce structured errors naming module/table/index/source value. Validate required dependencies before running
   consumers; a null replacement plus a warning is not a successful initialization policy.
5. Test null entries, missing modules, truncated files, out-of-range extents, module-local/resident resolution,
   aliases, bad call targets and interior pointers into expanded arrays/structs using synthetic fixtures.

**Gate:** required boot/title tables load with zero unexplained failures; known optional exclusions are enumerated.
Debug/Release tests pass and a bounded boot run reaches its checkpoint without invalid dispatch. Retire original
address resolution incrementally as later packages replace raw module content with typed assets/behavior IDs.

### M2 — Checkpoint replay and semantic verification

**Scope/files:** `Desktop/src/main.cpp`, `Core/FrameClock`, input scripting, `Native/Tests`, planned verification ledger;
EC-06. Existing scripted presses and captures remain the entry point.

1. Define a versioned input/checkpoint record: initial seed/state, press/release durations, analog/connection state,
   expected ticks, asset/extractor revision and exit condition. Validate malformed input rather than silently accepting it.
2. Add canonical state serialization and field-level diffs ([§10.1](VERIFICATION_AND_RISKS.md#101-evidence-and-failure-rules)).
   Specify the subset observable in the original before promising cross-runtime hashes; keep native repeatability
   distinct from reference parity.
3. Classify wall-clock/root-counter consumers as diagnostics or simulation. Define deterministic tick delivery,
   pause/stall behavior and callback order for capture/replay; do not hash diagnostic timings.
4. Store checkpoint manifests and test logic in source control; keep user-derived payloads and captures ignored.
   Add timeout/abnormal-exit reporting so a fiber blocked before its next tick cannot look like a completed run.

**Gate:** repeated scripted runs yield the same canonical state at defined checkpoints; reference comparisons state
their observable fields/tolerances. A deliberately perturbed input/state produces an actionable first-divergence report.

### M3 — Interactive title, input and explicit lifecycle

**Scope/files:** `Game/src/Boot`, `Game/src/Title`, `Engine/src/Input`, `GameFiber`/`LogicalTicks`;
EC-05/06/07. This is not the complete music/movie gate.

1. Create separate checkpoints for logos, title art, menu selection, settings, idle/attract transition and New Game.
   Capture current behavior before replacing its control flow.
2. Define Boot/Title entry, update, reset and exit ownership. Audit generated overlay resets against original initialized
   versus persistent state; test second entry instead of clearing every static indiscriminately.
3. Migrate input consumers to native pad/action state while preserving edges, repeat and analog/connection semantics.
   Add disconnect/reconnect and keyboard/gamepad equivalence tests; give rumble a cancel-on-exit lifecycle.
4. Replace required SDK waits/callbacks with explicit tick/lifecycle contracts. A fiber may remain private scheduling
   machinery; game-facing hardware timing semantics must not be required by the migrated path.

**Gate:** repeated title/settings/attract/New Game control cycles pass scripted and interactive input checks, including
reset and exit. Required control-flow stubs are absent. M4-title, M5-title and M6 must also pass for full Phase 1b closure.

### M4 — Typed rendering, visual assets and math boundary

**Scope/files:** `Engine/Render`, `Legacy/Render/LibGpu.cpp`, `Core/Gte`, Content decoders and title/field/actor render
callers; EC-01/03/04. Preserve the current renderer as an isolated comparison path during migration, subject to Q4.

1. Define draw items, stable sort keys, texture/palette handles, blend modes, clipping and resource lifetime. Specify
   capture/readback operations for title backgrounds and framebuffer-dependent effects before replacing `StoreImage`.
2. Decode the title's required images/palettes into validated assets. Record schema/converter versions and source IDs;
   progressively add room/model/animation formats rather than treating raw extraction as completed conversion.
3. Implement typed native submission and host-GPU primitive rendering. Migrate title sprites/text first, then room
   geometry and actor models. Legacy translation must not leak packet layouts into the public render API.
4. Replace GTE load/execute/store call clusters with explicit math inputs/results or owned contexts. Preserve proven
   fixed-point rules; test saturation, projection/clipping, winding and independent contexts before changing precision.
5. Add focused visual fixtures for draw ordering, transparency modes, palette changes, texture windows, clipping,
   dithering and readbacks. Define reference tolerances/regions before accepting new output.

**Gate:** each migrated slice renders without constructing GP0 packets or following ordering-table links; visual and
math checks pass. Native presentation of a software-rendered image alone does not pass. Remove each unused adapter;
full EC-01/03 closure waits for all consumers, not just the title.
