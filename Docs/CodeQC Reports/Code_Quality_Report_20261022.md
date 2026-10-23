# Code and Game Architecture Quality Review

Date: 2026-10-22. Source: `work`, commit
`9b76aabffeb674f55c29ad446a4cdb76545324ba`.

Review mode: architecture and source inspection, with limited executable checks.
No native implementation, generator, project, baseline, reference tree, issue,
commit or branch was changed. The only authored changes are this report and
[../BUILDING.md](../BUILDING.md).

## 1. Findings

Severity describes engineering or release risk, not permission to reorder
[tracker #4](https://github.com/JEschete/VS-NativePort/issues/4).
An architectural gate can be incomplete without being a newly introduced bug.
No fresh gameplay crash, frame-time regression or memory leak was reproduced.

| ID | Severity | Finding | Evidence Classification | Existing Work |
| --- | --- | --- | --- | --- |
| QC-01 | High | Ending ripple rendering indexes outside its declared grid. | Confirmed source-level bounds defect; ending replay not performed. | M10, ending ownership #62; existing G00820 investigation. |
| QC-02 | High | Effect packages and event execution still lack complete bounded input/execution contracts. | Source-backed safety and hang exposure; malformed-input runtime reproduction pending. | M1/M8, #49/#59/#61. |
| QC-03 | High | Engine/Content/Game dependency boundaries are violated and not enforced by the current CI workflow. | Confirmed include/build-policy mismatch. | M0, #43. |
| QC-04 | High | Session, room, actor and temporary state remain global and lifecycle-coupled. | Confirmed architecture debt; individual cleanup fixes exist. | M3/M7/M9, #44/#45/#55/#60. |
| QC-05 | High | Typed content, stable catalogs and Def/Instance separation remain substantially unimplemented. | Confirmed target-versus-code gap. | M4/M7/M8, #47/#48/#58. |
| QC-06 | High | Native save/load is not implemented by the existing storage helper. | Confirmed missing capability, not observed save corruption. | M9, #18. |
| QC-07 | High | SDL GPU presents the game image, but does not yet render native game geometry. | Confirmed modernization gap; CPU compatibility rendering is explicitly staged. | D21, M4, EC-01, #50. |
| QC-08 | Medium | Fixed simulation exists; interpolated, refresh-independent geometry presentation does not. | Confirmed architecture gap, not proof that 60 Hz currently doubles game speed. | M2/M4, #44/#50. |
| QC-09 | Medium | First-use instruments, file reads, movie preparation and XA decoding can block the simulation/control thread. | Confirmed synchronous call paths; stall magnitudes unmeasured here. | M5/M6/M7, #48/#53. |
| QC-10 | Medium | Rendering contains recurring dynamic allocation and process-lifetime packet-address retention. | Confirmed allocation/lifetime mechanisms; cost and growth rate unmeasured. | M4, #45/#50. |
| QC-11 | Medium | Global GTE, model and navigation contexts prevent safe parallel execution and independent worlds. | Confirmed state sharing. | EC-03/05, #45/#51/#57. |
| QC-12 | Medium | Desktop replay arguments can silently accept invalid values, and finished game execution is not a host failure condition. | Confirmed source-level automation weakness; executable CLI cases not run. | M2, #44/#52. |
| QC-13 | Medium | Asset queue admission and dequeue have inconsistent capacity boundaries. | Confirmed source inconsistency; saturation behavior needs a focused native regression. | #48; inherited reference behavior. |
| QC-14 | Medium | Naming and folder placement are ahead of readability, type safety and C++ promotion. | Confirmed source/build-policy debt. | D5/D9, #46 and subsystem migrations. |
| QC-15 | Medium | Gameplay policy, presentation, text layout and original data encodings remain interlocked. | Confirmed extensibility/content-workflow debt. | #54/#56/#58/#60/#61. |
| QC-16 | Medium | Ledger statuses and current documents cannot be used as reliable completion percentages. | Fresh structured cross-check finds stale status relationships. | M0, #46. |
| QC-17 | Medium | Green CI does not currently establish native compilation, runtime safety, determinism or performance. | Confirmed workflow scope limitation. | M0/M2/M10. |
| QC-18 | Low | Desktop requests GPU debug mode unconditionally, including Release. | Confirmed configuration choice; overhead not measured. | Render/platform follow-up. |
| QC-19 | Medium | Combat Lab tracing is useful evidence but changes workload and has unbounded disk growth. | Confirmed instrumentation behavior; not a reopened stutter defect. | #16/#25; preserve #70's bounded acceptance. |

### QC-01: Ending Grid Bounds

[StaffRoll.c](../../Native/Game/src/Ending/Credits/StaffRoll.c#L32)
declares `Ending_Credits_rippleDistanceGrid[4][16]`. The draw loop visits
`i = 0..3` and `j = 0..15`, then reads `[i][j + 1]`, `[i + 1][j]` and
`[i + 1][j + 1]`
[in the vertex construction](../../Native/Game/src/Ending/Credits/StaffRoll.c#L141).
The last row/column therefore violate C array bounds. Contiguous neighboring
storage does not make those accesses valid.

Impact: the ending's visual calculation can consume unrelated data, and its
behavior is not a sound foundation for optimization or C++ promotion. This is
not evidence of a freshly observed crash or proof of the intended replacement
grid size. The existing runtime-tracing document already identifies G00820's
ripple extent as unresolved.

Action: resolve the boundary samples and storage extent against the original
assembly/data, then add a guarded highest-row/highest-column regression and an
actual ending visual checkpoint. Do not merely enlarge the array or clamp the
indexes: either could hide missing source data. Make a future fix through the
generator or registered ownership, never directly in generated output.

### QC-02: Bounded Packages and Execution

[Effects_BindEffectPackageBlocks](../../Native/Game/src/Effects/EffectResources.c#L403)
walks tagged blocks using a size encoded in `meta`, without an enclosing byte
extent. A tagged block with a zero low-halfword size cannot advance the cursor.
Curve/texture bindings also use embedded counts and offsets without a common
validated package boundary.
[P-file timeline accessors](../../Native/Content/src/PFile.c#L7)
similarly return offset-derived pointers without receiving a span or index range.
The newer block-13 audio work does not make the other blocks bounded.

[Event_ExecuteScriptThread](../../Native/Game/src/Event/ScriptVm.c#L932)
dispatches from raw cursors and uses an unbounded execution loop. Its `intptr_t`
results mix five control codes with branch addresses. Correct pointer width is
valuable, but it is not cursor validation, branch validation or an instruction
budget. Invalid script data can monopolize the cooperative game thread before
it yields to event pumping.

Action: validate packages into immutable bounded views before publishing any
bindings. Reject zero/nonprogressing sizes, truncated sections, invalid counts,
unknown required tags and branches outside the owning script. Use a typed VM
step result and explicit execution ownership. Diagnose nonprogressing execution
without silently changing legitimate script timing. Test malformed fixtures,
skip/branch behavior, cancellation, failed-load rollback and re-entry.

These are existing #49/#59 concerns, not a claim that every shipping package or
script fails. The checked module-loader and minimap decoders demonstrate that
bounded handling is already achievable in this codebase.

### QC-03: Dependency Direction

[Engine save code](../../Native/Engine/src/Save/Snapshot.c#L3)
includes actor, battle, field, inventory and Game State headers.
[Content actor loading](../../Native/Content/src/ActorAssets.c#L4)
calls actor, field and item behavior.
[Content system resources](../../Native/Content/src/SystemResources.c#L6)
depends on the game menu framework. These are direct policy dependencies from
lower layers into higher layers, not just benign format declarations.

[Directory.Build.props](../../Directory.Build.props#L16) exposes all projects'
private source trees to copied C. The
[solution generator](../../tools/scaffold/gen_solution.py#L60) also deliberately
permits Engine to see Content without a project reference. Static-library link
success therefore does not demonstrate an acyclic system architecture.

The [boundary scanner](../../tools/lint/native_boundary.py#L24) checks selected
SDK/address/name patterns, not system ownership, private-header access or shim-use
trends. The [CI workflow](../../.github/workflows/quality.yml#L1) does not invoke
that scanner as a retirement gate.

Action: separate file-format decoding from actor construction, and Game-owned
save snapshots from Engine-owned storage. Add exact, reviewed dependency
exceptions and reject new reverse/private includes. Do not hide this by moving
all shared types into an umbrella header or by creating more library projects
without changing their contracts.

### QC-04: State Ownership and Lifecycles

[GameApp](../../Native/Game/src/Boot/GameApp.cpp#L40) is a fiber wrapper rather
than the explicit mode controller described by its public API.
[CurrentMode](../../Native/Game/include/Boot/GameApp.hpp#L44) returns a member
initialized to `Boot`; the inspected implementation does not update it.
`IModule` is a target-shaped interface, not the active lifecycle abstraction.

[The session loop](../../Native/Game/src/Boot/Session.c#L33) uses a process-global
`jmp_buf` and nonlocal transitions. The source explicitly confines transitions to
the copied C graph. Preserve that restriction: jumping across a promoted C++
frame with live nontrivial automatic objects would violate its cleanup contract.
This review does not demonstrate that such an unsafe crossing currently occurs.

State ownership is visibly misplaced: the
[audio channel header](../../Native/Engine/src/Audio/Seq/Channel.h#L217) exposes
`Items_Inventory Audio_Seq_unusedInventory` and
`State_StateFlags Audio_Seq_unusedStateFlags`, which
[inventory operations](../../Native/Game/src/Items/Inventory.c#L28) actively use.
They are not unused audio data. Renaming alone will not repair their ownership.

Action: give one session ownership of persistent game state, room state, actor
identity and temporary workspaces. Make entry/exit/reset/shutdown explicit and
distinguish persistent from reload-reset state. Migrate one real transition at a
time, covering second entry, defeat, failed loads, stop during loading and stale
references. Preserve the existing
[AI teardown](../../Native/Game/src/Battle/Ai/ActorCleanup.cpp#L177) and event-reset
regressions instead of replacing them with indiscriminate global clearing.

### QC-05: Content Contracts and Designer Separation

[DataStore](../../Native/Content/src/DataStore.cpp#L46) loads an LBA/size/form/path
TSV, not the planned versioned content manifest. The
[module loader](../../Native/Content/src/ModuleLoader.cpp#L59) still resolves
original addresses into native symbols. This bridge now validates capacities,
native element strides and required pointer resolution; those improvements are
real, but the runtime still depends on original module images.

Fresh tracked-file checks find no committed content schemas, patch files or
content ledger. The audio bank/stream metadata is a genuine versioned exception:
[LoadAudioBank](../../Native/Content/src/AudioBank.cpp#L425) validates versions,
IDs and numeric ranges. Do not describe all native content as unvalidated.

[EncounterCatalog](../../Native/Game/src/Field/EncounterCatalog.cpp#L99) provides
useful checked discovery, but reads zone-local MPD/ZND records. It is not the
Species/EnemyDef/Loot/Spawn architecture in the accepted enemy design.

Action: implement immutable definitions, stable authored IDs, runtime handles and
explicit file-layout decoders through a small production slice. Keep mutable
instances separate. A content designer should be able to change one definition,
validate it, identify all consumers and preview the change without editing a PRG
or recompiling gameplay. Maintain exact original values/RNG order in the base
catalog; enhancements belong in reviewed patch layers.

### QC-06: Persistence Is Still a Product Gap

[SaveStore](../../Native/Engine/src/Save/SaveStore.cpp#L8) creates a directory,
formats slot paths and checks existence. It has no serialization, load,
migration, validation, atomic replacement or recovery operation. Directory and
existence errors are reduced to ignored error codes/false results.
[The card adapter](../../Native/Engine/Legacy/Save/LibCard.cpp#L8) retains stubs.

Action: settle the plan's Q6 encoding decision, then make Game own a versioned
semantic snapshot and Engine own recoverable storage. Include stable IDs, valid
references, corruption/version errors, interrupted-write recovery, supported PS1
import variants and repeated Continue tests. Capturing an Ashley snapshot or
opening the Data menu is not a save/load round trip. Save serialization latency
cannot be assessed for an implementation that is not present.

### QC-07: Renderer Modernization Has Not Landed

[LegacyGpu_WalkList](../../Native/Engine/Legacy/Render/LibGpu.cpp#L107) traverses
ordering-table links. Packet execution reaches
[PageSpace::Triangle](../../Native/Engine/src/Render/PageSpace.cpp#L377), which
rasterizes on the CPU using integer screen coordinates and affine UV
interpolation. [Device::PresentImage](../../Native/Engine/src/Render/Device.cpp#L95)
uploads/blits RGBA pixels; it does not supply native scene vertex/index buffers,
materials or a scene depth attachment.

Consequences: high-resolution output currently enlarges a low-resolution game
image. It does not remove vertex snapping or texture warping, provide native 4K
geometry, or make threat-sphere geometry resolution-independent. Native packet
handles solve x64 address truncation, not this architectural gap.

D21 explicitly stages this compatibility path. It is valid evidence of progress
toward Phase 1 behavior, but not acceptance of M4 or retirement of EC-01. D21's
Phase-2 renderer wording and M4's earlier native-rendering gate need an explicit
scope reconciliation before declaring a whole phase complete.

Action: adopt the planned typed render queue and texture/material/capture
contracts behind Engine Render. Migrate one vertical slice while retaining a
separately isolated comparison path. Preserve palette animation, mask/blend
rules, readbacks and transparent ordering in the comparison tests.

### QC-08: Simulation and Presentation

[FrameClock](../../Native/Engine/include/Core/FrameClock.hpp#L10) already runs at
`60000/1001` fields/second, approximately 59.94 Hz, with up to four catch-up ticks.
[Advance](../../Native/Engine/src/Core/FrameClock.cpp#L7) deliberately drops excess
backlog after a stall; an existing clock test specifies that behavior. It avoids
unbounded catch-up, but it is not a guarantee of real-time progress under overload.

[The desktop loop](../../Native/Desktop/src/main.cpp#L326) advances that clock
separately from presentation, but `game.Tick` performs legacy drawing and the
host converts the page image during ticks. Catch-up therefore repeats graphics
work, then presents the last image. There are no previous/current transform
snapshots or interpolation alpha. Swapchain waiting remains in the same host
thread. Accelerated `--speed` is a test facility, not uncapped gameplay rendering.

Action: preserve field/30/20-rate semantics, publish render snapshots at explicit
boundaries and interpolate presentation without changing combat windows, RNG or
event timing. Define pause, focus loss, long-stall and overload behavior. Test the
same semantic input sequence at 30/60/120/144 presentation rates and injected
stalls. Do not replace the fixed simulation with variable `dt`.

### QC-09: Loading, Residency and Real-Time Audio

[DiscReader](../../Native/Content/src/DiscReader.c#L45) advances a queue state,
but performs the filesystem read on its update thread.
[DataStore::Read](../../Native/Content/src/DataStore.cpp#L123) opens and reads a
file synchronously. [Room loading](../../Native/Game/src/Field/RoomLoading.c#L177)
polls request state and performs parsing/uploads/music preparation. An API named
queue or asynchronous-looking state machine is not a background I/O service.

[VoiceEngine::NoteOn](../../Native/Engine/src/Audio/VoiceEngine.cpp#L440) can call
`LoadProgram`, creating a synth and loading SFZ/sample resources on the producer
thread. That keeps loading out of the callback, but can still delay gameplay and
new sound events. [Movie opening](../../Native/Engine/src/Movie/MoviePlayer.cpp#L73)
reads packets and decodes/queues the complete soundtrack. The
[XA service](../../Native/Engine/src/Audio/XaPlayback.cpp#L83) decodes the complete
track into a vector before demand-fed playback.

The ending track's documented `443.52 s * 37800 Hz * 2 channels * 2 bytes`
requires 67,060,224 bytes, about 64 MiB, for PCM alone. This is a source-format
calculation, not a measured process high-water mark. These choices are feasible
on many PCs but need explicit budgets and are not general-purpose streaming.

Positive finding: [AudioDevice::Supply](../../Native/Engine/src/Audio/AudioDevice.cpp#L141)
renders active voices on SDL demand, while the bounded SPSC control queue and
program retirement keep file I/O/destruction off that callback. This is a sound
native boundary and materially better than tying all PCM production to gameplay.

Action: prewarm/cache bounded programs, load/decode on workers, publish results at
deterministic simulation boundaries and specify cancellation/failure ownership.
For long replacement audio/video, use bounded decode buffers with explicit EOF,
seek and teardown. Retain source isolation and the existing postmix stall tests.
Queue exhaustion currently aborts explicitly; define queue occupancy budgets and
stress coverage, including the separately tracked accelerated-audio #83 path.

### QC-10: Allocations, Pools and Memory Layout

[Each packet walk](../../Native/Engine/Legacy/Render/LibGpu.cpp#L107) constructs a
fresh `unordered_set` and inserts traversed addresses. This can allocate a node
per visited entry, including ordering-table entries.
[PageSpace::Copy](../../Native/Engine/src/Render/PageSpace.cpp#L69) allocates a
temporary pixel vector. The owned
[sphere terrain implementation](../../Native/Game/src/Battle/SphereTerrain.cpp#L66)
uses arrays of row vectors, with per-call growth/insertion during clipping. These
are recurring allocation sites, not measured explanations for a current stutter.

The [packet handle table](../../Native/Engine/Legacy/Render/LibGpu.cpp#L26) retains
addresses for the process lifetime. Address reuse can keep it stable; it is not
proof of a per-frame leak. However, it lacks an explicit resource-generation,
reclamation or memory-budget contract for future content churn.

Conversely, `Present` assigning an existing vector does not prove it allocates
every frame: capacity may be reused. GPU textures and transfer buffers are
reused until dimensions change. The
[particle system](../../Native/Game/src/Effects/Particles.c#L86) already allocates
a pool and reuses nodes. It would be incorrect to recommend adding object pooling
as though none exists.

Action: measure allocations/counts/bytes and allocator time first. Reuse bounded
frame/workspace storage where justified, preserve list-corruption detection and
pool-exhaustion behavior, and expose high-water/fragmentation metrics for the
custom two-ended heap. Its native arena is 16 MiB, but linked free-list searches
and raw-pointer ownership still require lifecycle tests. Do not replace every
small fixed array with a heap container in the name of modernization.

### QC-11: Math and Threading

[GteState](../../Native/Engine/src/Core/Gte.cpp#L16) is one implicit global
load/execute/store context.
[The model workspace](../../Native/Engine/src/Render/ModelWorkspace.c#L7) and
[AI frame context](../../Native/Game/src/Battle/Ai/FrameUpdate.c#L42) are also
shared. They prevent safe parallel transform/navigation jobs and make multiple
independent simulation instances difficult. Cooperative fibers do not create
parallel simulation; they preserve the same mutable context.

The problem is hidden state, not fixed-point arithmetic. The existing
[math/RNG/clock tests](../../Native/Tests/src/CoreTests.cpp#L16) cover signed
rounding, overflow-sensitive inputs, aliases, matrix operations and long-run
counter ratios. Preserve and extend them.

Action: expose pure transform/query inputs and outputs or explicitly owned
contexts. Establish golden vectors from original logs for saturation, clipping,
normalization, collision and targeting. Use float/SIMD math at the modern render
boundary where appropriate. GLM/DirectXMath will not automatically preserve the
game's integer rounding or remove an implicit register protocol.

Do not immediately put the existing AI or renderer on worker threads. Start with
immutable content decoding/preparation. Offload AI only if profiling justifies
it, after contexts, RNG order and deterministic result application are explicit.
A small actor roster does not itself justify an ECS or a job system.

### QC-12: Replay Failure Semantics

[ParseRunSettings](../../Native/Desktop/src/main.cpp#L272) ignores the result and
end pointer from `from_chars` for `--frames` and `--speed`. An invalid frame count
can remain zero, meaning unlimited execution; partial tokens are not rejected.
[ParseButtons](../../Native/Engine/src/Input/Pad.cpp#L59) silently ignores unknown
button names, so a misspelled recorded action can become no input.

[GameApp::Tick](../../Native/Game/src/Boot/GameApp.cpp#L55) increments logical
ticks even after the fiber reports completion. The desktop loop does not inspect
`Finished()` before accepting its tick limit. Advancing a counter and displaying
an old framebuffer is not proof of simulation progress.

Action: validate every option completely before creating runtime services,
reject missing/unknown/overflowing arguments and unknown buttons, and distinguish
expected completion from unexpected game return. Add CLI negative controls and
a controlled early-return host test. These are source-level observations; the
blocked local build prevented running those executable cases here.

### QC-13: Asset Queue Capacity

[Admission](../../Native/Content/src/ActorAssets.c#L46) scans all 16 request
slots. [Dequeue](../../Native/Content/src/ActorAssets.c#L135) shifts only indexes
1 through 14 and clears index 14. It never moves accepted slot 15 toward the
consumer at slot 0. The same loop exists in the read-only original reference;
this is not evidence of a newly introduced translation regression.

Action: establish whether the final slot was intended as a sentinel or a usable
request. Add a capacity-boundary regression that submits uniquely identifiable
requests and requires completion exactly once, including full/reuse/cancel
cases. If capacity is 15, reject the extra enqueue; if 16, dequeue must include
it. Do not claim a current room-load hang or alter the bound without that check.

### QC-14: Readability and Promotion

The module names and decomp provenance substantially improve navigation.
However, copied locals such as `arg0`, `temp_v0` and `var_s1`, casts through
unprototyped function pointers, opaque fields, binary masks and large switches
still require reverse-engineering knowledge to maintain. Generated types-only
include machinery resolves compilation dependencies without creating clean APIs.

Fresh file measurements identify large concentration points:
menu Files 3,558 lines; menu Framework 3,405; actor Movement 3,213;
module pointer bindings 3,289; actor Animation 2,085. These are text-line counts,
not function complexity scores. Generated bindings need different treatment from
hand-owned behavior: splitting generated tables into arbitrary smaller files
would not improve the runtime architecture.

[The generator](../../tools/scaffold/gen_solution.py#L151) grants every `.c`
translation unit Level1/nonfatal warnings, including owned C. The shared C++
defaults are Level4/warnings-as-errors. Extension-based policy is therefore a
weak substitute for ownership/migration status. Existing C++ implementations can
still depend on compatibility APIs; `.cpp` is not a retirement certificate.

Action: improve names, units, types and helper boundaries when taking ownership
of a behavior. Prefer semantic requests/results and named masks over raw offset
operations. Tighten diagnostics by reviewed maintained-code scope. Keep
provenance and focused tests. Do not mass-format generated code, rename every
export or convert extensions without the plan's actual promotion gates.

### QC-15: Gameplay and Content Workflows

[Boot_RunGameLoop](../../Native/Game/src/Boot/GameLoop.c#L105) coordinates input,
room transitions, event playback, targeting, menus, AI, camera and packet drawing
through shared flags and nested numeric state machines. Inventory operations
depend on UI indexes; actor records combine transforms, animation, palettes,
collision, sounds and original sequence references.

This is not a deep-inheritance problem: there is little modern class hierarchy
to flatten. It is a policy/ownership/composition problem. A `Player` class wrapped
around the same blob would not solve it.

Action: separate combat intent, prerequisites, deterministic results and
presentation. Compose actor capabilities around stable identity and explicit
resource ownership. Keep text IDs, control decoding, layout and rendering
separate. Preserve original inventory/limb/action limits in faithful gameplay;
only make them extensible where later approved content work requires it.

Per-tick input, animation and combat updates are appropriate. Do not introduce
an event bus for every getter or replace bounded scans solely because polling
looks old. Use explicit invalidation/events for expensive derived UI/assets once
their authoritative state owner exists.

### QC-16: Status and Documentation Drift

Fresh CSV/ownership checks find 4,207 function-catalog rows and 196 ownership
registrations across 43 implementation paths and 86 implementation/header paths.
147 registered mappings still have `copied=stub`, including implemented word-copy
helpers. There are 235 catalog `stub` labels overall, but the separate assembly
gap document reports 91 open entries at its own snapshot. They are not equivalent
inventories and must not be added or used as current feature counts.

The catalog has 28 `cpp=x` and seven `verified=x` rows; all `modernized` cells are
blank. These are recorded flags, not a fresh proof that only seven behaviors
have ever been tested or that all 28 promotion gates were met. Many concrete
tests and issue evidence exist outside those cells.

The compatibility audit still calls movie decoding a stub and describes weaker
module initialization than the current code. The handoff's lead section says
item-use #8 is next, whereas the current public tracker records #8 closed and #9
next. The asset design includes planned commands, output paths and encoder
settings that differ from the extractor's current implementation.

Action: reconcile statuses by evidence and revision, keeping ownership,
compilation, quality, behavior, promotion and retirement independent. Give each
document a clearly current capability summary and put historical checkpoints
below it. Do not reset baselines, delete history or manufacture verification
flags to make the tables agree.

### QC-17: Verification Gates

The current CI job runs Python tool controls, encounter source adaptations and
the fast style/duplication gate. It does not build the native solution, run Catch2,
perform the full compilation-based tidy analysis, verify generation stability,
measure frame times, or compare gameplay to the original.

Action: retain this fast gate and add a provisioned Windows Debug/Release native
build/test lane using synthetic fixtures. Add reviewed boundary/shim trend and
generation-ownership gates. Keep licensed-asset/reference checkpoints in an
appropriate owner-controlled lane; a skipped data-dependent case must not become
a release pass. Add memory instrumentation where the selected Windows toolchain
supports it, without assuming all sanitizer combinations are available.

### QC-18: GPU Debug Configuration

[Desktop initialization](../../Native/Desktop/src/main.cpp#L302) passes `true`
for the GPU debug flag without regard to build configuration. This can affect
driver validation behavior and makes an unqualified Release benchmark ambiguous.
It is not evidence of an actual frame-rate loss on a particular machine.

Action: make validation mode explicit in configuration and benchmark manifests,
retaining an opt-in diagnostic path. Compare like-for-like captures with and
without validation before attributing cost to the renderer.

### QC-19: Diagnostic Workload

[Combat Lab](../../Native/Tools/CombatLab/src/main.cpp#L470) creates inspection
vectors and structured snapshots per tick.
[Trace](../../Native/Tools/CombatLab/src/Trace.cpp#L113) serializes JSON under a
mutex and flushes at frame/control boundaries; full logging is automatic. Files
grow without rotation. This is intentional diagnostic work, not evidence that
the normal desktop does the same work.

Action: report simulation, inspection, tracing, presentation and complete host
frame time separately. Preserve full-trace acceptance evidence and explicitly
identify any separately approved benchmark logging policy. Bound disk/session
retention and detect write failure. Do not silently drop required trace data to
make a benchmark look faster. Existing #70 results remain bounded, historical
acceptance, not a new unresolved stutter claim.

## 2. Architectural Verdict

**The repository is a native-executing, compatibility-backed source port with
useful modern services and strong regression habits. It does not yet meet its
documented final native-system architecture.**

Organization is substantially better than an overlay/address-named decompilation:
Engine, Content, Game and independent hosts are identifiable; provenance and the
ownership/copier workflow preserve traceability. Maintainability is still
limited by global storage, cross-layer calls, original layouts and hardware-shaped
protocols. Modern scaffolding must not be mistaken for adoption by production
callers.

The right direction is a sequence of behavior-preserving vertical migrations,
not a whole-game rewrite, an immediate ECS conversion, a new audio middleware
stack or a different graphics backend. Continue #4's ordered gameplay work.
Use the existing #18 architecture issues when their packages are authorized.

### Decision Compliance

Authority: [the decision log and package gates](../NATIVE_SOURCE_PORT_PLAN.md#L28).
Proposed decisions are not represented here as new owner approvals.

| Decisions | Assessment at Reviewed Revision |
| --- | --- |
| D1: native source port | Native x64 execution exists; hardware-shaped page/GTE/scratch/sector/voice adapters mean the target architecture is not yet satisfied. No MIPS interpreter was identified in the reviewed runtime path. |
| D2/D16: read-only reference, no upstream renames | No native include/link dependency into the reference tree was found by the existing source boundary check. Reference reading/provenance is not a violation. No upstream changes were made by this review. |
| D3/D4: no matching requirement, Windows x64 | The solution targets x64 and intentionally widens native pointers. Runtime layout bridges remain; Windows LLP64 assumptions are not a cross-platform ABI contract. |
| D5: C++23 destination | C++ uses the selected toolset; production is still predominantly C17. Staged C is permitted, but zero-C/final promotion gates are not met. |
| D6/D9/D10: layout, placement, naming | System folders/catalog/provenance are useful foundations. Actual dependency/state ownership and semantic local names lag placement. Naming history is not fresh behavior verification. |
| D7: phased delivery | Appropriate, provided behavior and architecture gates stay separate. Phase-2/3 proposals do not authorize changing Phase-1 rules. |
| D8: SDK retirement | Compatibility bridges remain inside and outside Legacy. No automated live-use/nonincrease retirement gate was found in the current CI workflow. |
| D11/D17: data contracts and enemy scheme | Def/Instance, stable IDs, schemas, patch layering and global enemy catalogs remain mostly target designs. Checked Lab discovery and audio metadata are useful partial foundations. |
| D12: hardware knowledge is not a hardware runtime | Faithful integer math, AKAO sequencing and native DSP can legitimately retain original rules. Register/page/address interfaces still need their separate EC retirement gates. |
| D13/D15: compiler/third-party proposals | ClangCL and manifest-based dependencies are implemented. This is not a comprehensive legal audit or new approval of every reuse choice. |
| D14: SDL3 GPU | Correct selected backend family is used behind Engine Render. Current submission is a framebuffer blit, not the planned native primitive/mesh API. |
| D18/D20: extraction | Runtime files come from extracted output; raw PRGs/models/rooms and sector addressing remain the explicit bridge. Full converted/versioned asset acceptance is not met. Startup may optionally inspect a disc label, so the strict final extractor-only Disc boundary is not complete. |
| D19: build truth | Build instructions were corrected in this review. Fresh build verification is blocked by local C++ prerequisites, not claimed successful. |
| D21: page image | Implemented as documented for the staged compatibility slice. Resolve its lifetime/Q4 and the D21/M4 scheduling tension explicitly. |
| D22: AV1/Matroska | Real libwebm/dav1d/FLAC decoding exists. GPU YUV presentation, bounded streaming and a defined audio-master presentation clock remain incomplete. |
| D23: sfizz | sfizz/SFZ/WAV plus the existing AKAO sequencer is implemented. Keep it; do not introduce FMOD/Wwise/miniaudio merely to appear modern. Remaining register/DMA helpers do not establish full EC-02 retirement. |

## 3. Frame Budgets and Profiling

### What Is and Is Not Measured

There is no new CPU flame graph, GPU timeline, allocation capture or target-device
benchmark from this review. The native build cannot run locally in the present
environment. Source hot spots below are hypotheses to profile, not measured
bottlenecks. Existing timing aggregates are not flame graphs.

The [recorded four-wolf checkpoint](../COMBAT_LAB.md#L443) reports:

| Historical Configuration | Simulation Mean / Maximum | Maximum Present Interval |
| --- | --- | --- |
| Debug, run `1792568914935697-0` | 7.315 / 22.435 ms | 26.057 ms |
| Release, run `1792569400494834-0` | 1.910 / 4.863 ms | 26.526 ms |

Debug exceeded the document's 16.667 ms threshold on 41 of 600 samples. These
results belong to that source/asset/logging setup, not to a fresh profile of this
commit. A maximum present interval above 16.667 ms is not a smooth-60 guarantee,
even when no interval exceeds 33.333 ms. The owner nevertheless accepted the
bounded stutter fix; this review does not reopen it.

### Proposed Measurement Contract

These are recommended gates for future work, not newly accepted specifications:

| Surface | Budget or Required Observation |
| --- | --- |
| Compatibility simulation | Keep the exact `60000/1001` tick contract; one field is about 16.683 ms. Record dropped backlog, not just average update time. |
| 60 FPS presentation | 16.667 ms complete frame budget; report p50/p95/p99/max and missed intervals, with CPU work separated from waits. |
| 120/144 Hz presentation | 8.333/6.944 ms respectively; unchanged simulation state and input semantics. |
| Audio | 200 frames at 48 kHz represent 4.167 ms of audio. Compare callback CPU time with the actual requested block deadline and all bound sources, not the game's visual frame budget. |
| Allocations | Count hot-path calls/bytes and peak live bytes by owner, including sphere, packet traversal, Lab diagnostics and first-use audio. No blanket zero-allocation claim until captured. |
| Memory | Record process working set/commit, custom heap high-water/fragmentation, decoded audio, model/texture cache residency and GPU allocations. Current global budgets are not specified. |
| Room transition | Record input-to-ready latency and maximum frame stall for warm/cold loads, failure/cancel and return. Do not promise zero milliseconds or preload the entire game without a budget. |

### Capture Annotations to Produce

| CPU/GPU Capture Region | Source Anchor | Question to Answer |
| --- | --- | --- |
| Simulation and catch-up | Desktop `RunGame`, `GameApp::Tick`, original game loop | Does overload come from rules, rendering, loading, tracing or swapchain wait? |
| Packet submission/raster | `LegacyGpu_WalkList`, `PageSpace::Triangle/Plot` | What fraction is hash allocation, traversal, overdraw, texture lookup and interpolation? |
| Threat sphere | `TerrainRows`, wall/corner clipping and packet emission | How many temporary allocations and CPU milliseconds occur while targeting? |
| Asset preparation | `Content_UpdateDiscCommands`, `DataStore::Read`, actor parsing | Are filesystem, parsing, allocation or image uploads causing transition spikes? |
| Audio first use and steady state | `LoadProgram`, `VoiceEngine::Render`, SDL postmix | Separate producer stalls from callback deadline misses and queue occupancy. |
| Movie/XA | `Player::Open/Advance/ToRgb`, XA start/supply | Peak decode residency, startup blocking and A/V drift under slow frames. |
| Presentation | `PresentImage`, upload/copy, swapchain acquisition and blit | Separate CPU conversion/upload cost, GPU duration and present blocking. |
| Diagnostics | `InspectEncounter`, `Trace::Frame/Event`, file flush | Quantify observer effect and complete host-frame cost. |

Use Windows-native CPU/allocation profiling and GPU capture tools already
appropriate to the selected backend. The inventory's mention of Tracy is not
proof that Tracy instrumentation or a Profile configuration is integrated.
RenderDoc can inspect the current upload/blit, but it cannot attribute the CPU
software rasterizer without a CPU capture.

Capture title/idle, room traversal, target sphere, dense effects, first-use bank,
menu open/close, movie skip/re-entry and repeated encounter teardown. Include
the retained eight-Minotaur #78 case only with its safety blocker and source
revision recorded; do not benchmark around it by silently reducing actor count.
Store call stacks, hardware/driver, power mode, resolution, build/validation mode,
seed, inputs, asset versions, logging policy and dirty state with each capture.

## 4. Memory, Assets and Scenes

The language/runtime is native C/C++, not a managed-GC engine. The relevant
risks are heap churn, fragmented arenas, ownership, data locality, blocking
preparation and resource lifetime, not Unity-style GC collection pauses.

Useful existing choices include contiguous arrays for many tables, a particle
pool, persistent GPU upload resources, RAII in new platform/audio/movie code and
explicit native bounds checks in newer decoders. A linked list is not inherently
wrong for a tiny pool, but large actor records, split pointer-owned subobjects and
global alias views do not constitute a data-oriented actor architecture.

The CPU page image is exactly 1 MiB (`1024 * 512 * sizeof(uint16_t)`). The custom
game heap is 16 MiB. Neither is the application's complete memory budget. Native
vectors, SFZ programs/preloads, full soundtrack/ending PCM, staging buffers,
decoder state and driver resources live outside those totals.

Recommended ownership chain: application services own immutable asset registries
and a budgeted cache; a session owns game state; a scene owns its actor instances
and cancellable requests; a frame owns only short-lived draw/query data. Shared
assets need handles/reference ownership, eviction policy and observable residency.
Requests need generation/cancel tokens so a completed old-room load cannot bind
into a reused actor slot.

[Handle<T>](../../Native/Engine/include/Core/Handle.hpp#L12) is currently an index
wrapper with an invalid sentinel. That can be sufficient for an immutable
registry. It does not provide bounds resolution, registry ownership or stale
generation detection for recyclable live actors. Do not impose one identical
handle policy on both immutable definitions and mutable entities.

Test repeated enter/leave/re-enter, partial load failure, reset during decode,
menu preview teardown, effect cancel, audio device close and natural EOF. Compare
semantic state and resource high-water after warm-up; an unreclaimed process
cache is not automatically a leak, but unexplained continuing growth is a failure.

## 5. Modern Rendering and Fidelity

| Requested Modernization | Current State | Recommended Contract |
| --- | --- | --- |
| Perspective-correct textures with optional affine look | CPU affine interpolation; no exposed modern material switch found. | Native perspective-correct geometry with an explicitly selected fidelity material/shader mode when that enhancement is authorized. Preserve art/UV/palette tests. |
| Eliminate PS1 ordering tables | Native handles still link GPU-shaped packets. | Retire packet/OT submission for migrated callers. Retain explicit stable draw ordering for transparency, UI and effects; depth does not replace all ordering. |
| Float depth buffer | No scene depth attachment in the current game-image presenter. | Define/test a modern opaque depth path, preferably D32_FLOAT where supported. Specify projection/near/far policy, format support and transparency separately. |
| Float vertices/subpixel motion | Integer projected coordinates and global GTE semantics still dominate. | Preserve authoritative gameplay math; retain preprojection precision and use float presentation transforms/vertices. Casting already-snapped screen coordinates to float cannot undo wobble. |
| 4K/ultrawide threat sphere and UI | Scaled 4:3 image and fixed-resolution sphere scanlines/packet geometry. | Separate world-space targeting math from render geometry and UI safe areas. Test 4:3, 16:9 and 21:9, DPI/resize, selected-limb visibility and readable text without stretching. |
| Graphics backend abstraction | Engine Device is a useful boundary; game draw data is still packet-shaped. | Finish typed game-facing submission over SDL3 GPU. A virtual `IRenderer` is optional, not the architectural goal. No need for three bespoke backends now. |

A depth buffer does not automatically fix near-plane clipping, z-fighting,
transparent ordering, coplanar effects or incorrect UVs. Float storage alone
does not produce smooth animation without retained precision/interpolation.
Preserve the original visual comparison path until replacement fixtures cover
blend modes, palette changes, clipping, readbacks and scene composition.

SDL GPU already abstracts D3D12/Vulkan/Metal; adding direct D3D12, Vulkan and
OpenGL implementations would duplicate a boundary the owner already selected.
The supported shipping target remains Windows x64. More backends require a
demonstrated need and separate acceptance.

## 6. Gameplay Extensibility and Content Debt

The design destination is appropriate: immutable definitions plus mutable
instances, actor composition, named behaviors and stable content IDs. Its
production adoption is the missing work. Numeric legacy IDs are useful import
and comparison metadata, but should not remain the only public identity.

| Content Team Task | Current Constraint | Technical Debt / Acceptance |
| --- | --- | --- |
| Balance all variants of one enemy | Complete records are zone-local and selected through original placement profiles. | Implement Species/EnemyDef inheritance with explicit merge/cycle/reference validation and exact expansion tests. |
| Add or replace a model/animation | Runtime consumes SHP/SEQ/WEP/ZUD-shaped data and coupled actor state. | Validated open-format conversion with bones, events, root motion, attachments and body-part metadata preserved. Test menu previews and NPCs as well as combat. |
| Inject HD textures | Page/CLUT/UV placement remains part of runtime behavior. | Stable texture/material identity and replacement rules, including palette animation, transparency and nonstandard dimensions. PNG alone is not a mod architecture. |
| Author or rebalance equipment | Original IDs, packed records, fixed indexes and UI policy are intertwined. | Def/Instance split shared by Ashley/enemies/drops/saves, with semantic inventory operations and migration rules. Preserve original gameplay capacity until a later approved enhancement. |
| Localize or rescale menus | Encoded controls, glyph packets, string offsets and screen layout remain coupled. | Stable text IDs, validated controls, separate shaping/layout and rendering; test longest strings and nested formatting. |
| Preview a spell/effect | Original package/script/plugin loading and global contexts remain. | Bounded effect definitions and owned playback sessions with source/target transforms, cancellation, shared assets and visual checkpoints. |
| Modify music/instruments | Native WAV/SFZ is already useful; AKAO bytecode and synchronous preparation remain. | Document supported overrides, version validation, event timing and cache behavior without creating another sequencer. |
| Reproduce an encounter | Combat Lab provides presets, seeds, real controls and detailed outcomes. | Add stable asset fingerprints, portable canonical checkpoint comparison and broader roster/action coverage; Lab-only grants must remain explicit preconditions. |

Combat Lab is a strong practical asset: it reuses production Engine/Content/Game
rather than maintaining a parallel combat implementation. Its tile validation,
availability reasons, live controls and before/after records are useful patterns
for future designer tools. Its same-zone/profile limits and isolated story events
mean it is not a substitute for full prologue, progression, rewards or save tests.

Global animation services should share immutable clips, not one global mutable
animation state machine for all actors. Each actor still needs independent
playback, blending, events and interruption. Likewise, a global asset manager
should mean shared services with explicit ownership/budgets, not another
uncontrolled singleton containing the game's state.

## 7. Audio and Movies

Keep the selected sfizz + SDL architecture and the original sequencing rules.
Modern native DSP may preserve noise/FM/reverb behavior without reproducing an
SPU register file. The inspected typed controls, SPSC publication, off-callback
program retirement and independent movie/XA sources are meaningful improvements.

Remaining architectural work includes bounded first-use preparation, event
lateness policy, queue occupancy/stress coverage, complete consumer retirement
and repeatable perceptual/reference checks. A 24-voice compatibility allocation
policy is not itself a defect in a faithful port. Widening it or substituting
middleware is not required to modernize ownership and scheduling.

Movie pictures are requested using a 60-tick conversion while the host clock is
`60000/1001`; soundtrack output advances independently on the device. This is
a reason to define and measure one A/V clock contract, not sufficient evidence
of an audible/visible drift defect: callers' picture cadence, source timestamps,
skip/tail behavior and device latency must be measured together. Existing
decoding should not be mislabeled an unimplemented movie stub.

Require invalid-file tests, long-duration drift measurements, slow-frame and
skip/re-entry tests, natural picture end versus soundtrack tail, device-close
ordering and replacement-media size limits. Primary-mix capture excludes the
independent movie/XA sources and physical output gaps; full A/V acceptance needs
the appropriate postmix/device or paired reference evidence.

## 8. Testing, Build and Evidence

### Fresh Review Checks

Initial status: no tracked edits; the report directory was untracked and this
report was empty. The final report/build-guide changes are documentation only.
All commands below were run from the repository root unless noted.

| Command / Observation | Result and Meaning |
| --- | --- |
| `git status --short --branch` and `git log -1 --format="%H%n%ci%n%s"` | Branch `work`, exact revision above; source timestamp `2026-10-22 10:12:34 -0400`. No native edits were introduced. |
| Tracked Native source inventory via `git ls-files Native`, extension filtering and PowerShell line counting | 982 `.c/.cpp/.h/.hpp/.inl` files; 214,768 text lines. Includes generated code, headers, tests and tools, not extracted assets. |
| Ownership/catalog cross-check with PowerShell `Import-Csv`; manifest key count with `ConvertFrom-Json` | 196 registrations; 43 implementation paths / 86 implementation-and-header paths; 751 generated manifest paths; 147 owned mappings still labeled `copied=stub`. These are metadata observations, not feature percentages. |
| `python tools/lint/native_boundary.py --summary` | Exit 1: 982 files, 38 `ps1-address` matches; no other rule-category hits reported. Flag constants and exact compatibility exceptions require classification; do not call all 38 unsafe pointers. |
| `python tools/port/test_encounter_fixups.py` | Exit 0: 10 tests passed in this environment. This is a source-adaptation check, not C++ execution or all generator tests. |
| `.\build-native.cmd Debug` | Exit 1 before native compilation: MSB4019, missing `Microsoft.Cpp.Default.props` in the selected VS 18 Build Tools instance. Accompanying NU1503 restore warnings are not proof of a C++ source defect. |
| `.\quality-native.cmd` | Exit 1 before analysis: `ModuleNotFoundError: No module named 'yaml'`. No tools were installed and no baseline changed. |
| Local binary/dependency availability check | No checked Debug/Release test/extractor/desktop binaries, extracted `files.tsv`, native dependency directory or lint Node install were available at the inspected locations. |
| `gh issue view 4 ...` / `gh run list ...` | Both blocked by unauthenticated CLI. No credentials requested or exposed. Public web inspection supplied the observations below. |
| Focused PowerShell build-guide validation | Balanced code fences; extraction/test/cleanup placement checked; runnable build/test examples contain no inline CMD `::` annotations. |

Native Debug/Release suites, local full tidy, generation repeatability, sanitizer
runs, gameplay/reference captures, device audio, live resizing, 4K/ultrawide and
performance profiling were **not freshly verified**. The failed build is an
environment prerequisite blocker; repeating Release would not resolve missing
C++ targets. No installer, extraction, regeneration or reference update was run.

### Published Evidence Observed

The public [Source Quality run 39793715858](https://github.com/JEschete/VS-NativePort/actions/runs/39793715858)
shows **Success** for this exact `9b76aabf` commit. Its summary also contains
Node-action-runtime and Git exit-128 warnings; authenticated detailed logs were
not available here. They are warnings on a successful run, not a reproduced CI
failure or an explanation of the local build blocker.

Public [tracker #4](https://github.com/JEschete/VS-NativePort/issues/4) records
item-use #8 closed at this checkpoint and learned-spell #9 next. The
[item acceptance record](../COMBAT_LAB.md#L314) reports earlier Debug/Release
416-case / 1,060,702-assertion suites and concrete controller/state evidence.
Those are published/historical records, not suites rerun by this review.

### Strengths to Preserve

- Generated-file ownership and provenance make reverse-engineered fixes traceable.
- Whole-archive test linking catches missing/duplicate symbols across copied code.
- Synthetic malformed-data, native stride, high-address, audio lifecycle, layout
	and reset tests address real port risks rather than only happy-path screenshots.
- Required module-data/pointer failures now prevent game execution.
- Quality policy distinguishes new debt, retained debt, exact test fixtures and
	replacement-only style; a green baseline is explicitly not clean-code proof.
- Combat Lab uses production logic and records actual actions, results, RNG and
	lifecycle state. Defeat/loot/learning/effect acceptance are distinguished.
- Reference media/data remain local rather than being committed or uploaded.

### Verification Gaps

- No fresh all-content, ending, native persistence or complete learned-spell pass.
- No committed general semantic replay/hash/diff gate demonstrated by the inspected
	desktop host. Lab snapshots and pairwise evidence are valuable but narrower.
- No present-cadence-independent gameplay contract proved across varied refresh,
	focus, stalls and devices.
- No complete live boundary exception/shim retirement inventory enforced in CI.
- No demonstrated current allocation/CPU/GPU budgets or maintained hardware floor.
- No broad malformed package/script and capacity-boundary coverage established for
	the identified legacy consumers.

## 9. Platform Matrix

This is an evidence/support matrix, not a certification of untested devices.

| Target | Current Position | Work / Evidence Required |
| --- | --- | --- |
| Windows 10/11 x64, supported D3D12/Vulkan GPU | Owner-selected primary target. SDL3 GPU/API and native Windows host exist; local build here is prerequisite-blocked. | Provision the complete VS C++/ClangCL/SDK/vcpkg toolchain; build/test Debug and Release; test drivers, device errors and distribution dependencies. |
| Modest Windows PC / integrated GPU | No independently demonstrated minimum CPU/GPU/RAM specification in this review. | Representative cold/warm content, effects, full audio and memory captures; verify SDL GPU driver/feature requirements. CPU rasterization does not remove the current GPU-presenter requirement. |
| Windows high refresh, 4K, 21:9 | Window blit scales/letterboxes the image; not native high-resolution geometry or widescreen gameplay. | Typed geometry, subpixel transforms, interpolation, UI safe areas and per-scene camera/framing acceptance. |
| Windows ARM64 | Not the x64 build target. | Separate compiler/dependency/fiber/diagnostic validation and an approved support decision; x64 emulation is not native ARM64 certification. |
| Linux/macOS desktop | Out of current owner target. SDL abstraction alone does not make the host portable. | Replace/isolate Windows fibers/crash/toolchain assumptions; audit LLP64 versus LP64 and fixed-width disk scalars; validate dependencies/shaders/paths. |
| Switch or other consoles | Not supported/certified by this repository review. | Approved scope and platform SDK access, platform services, shader/dependency packaging, memory/streaming budgets and full target testing. Generic SDL backend availability is not a console port. |
| Low-end mobile | Out of scope; no touch/lifecycle/thermal/driver acceptance. | Budgeted streaming, foreground/background input/audio, thermal/frame pacing and platform packaging after an explicit product decision. |
| Headless CPU tests | Useful pure/synthetic tests exist, but the main test target links the full runtime and audio/movie dependencies. | Keep pure domain fixtures runnable without display/disc; distinguish device-dependent tests. A standalone Lab window is not a headless simulation service. |

No multiplayer runtime or replication model was identified in the inspected game
architecture/dependency manifest. Networking, prediction, RPC bandwidth,
server authority and anti-cheat are **not applicable** to this single-player
review. Do not add networking abstractions as speculative modernization.

## 10. Recommended Milestones

These refine existing packages. They do not supersede #4 or authorize any fixes
as part of this documentation-only request. Log newly confirmed defects before
future implementation and reuse existing trackers instead of duplicating them.

| Milestone | Bounded Deliverable | Measurable Exit |
| --- | --- | --- |
| Restore local verification prerequisites | Complete selected VS C++ targets/ClangCL/SDK and documented Python/Node dependencies. | Documented build/test/quality commands execute; distinguish tests that skip licensed assets. No baseline refresh to hide failures. |
| Reconcile architecture status, M0 | Refresh current capability/exception summaries and ownership-versus-status discrepancies. | Each exception has owner, consumers, package and evidence; selected implemented helpers no longer misreported as stubs; no behavior inferred from counts. |
| Finish ordered learned-spell work #9 | Preserve real input, targets, costs, rank/state changes and interruption semantics through shared production paths. | Issue acceptance plus focused regressions and exact-revision CI; do not substitute menu visibility or Lab grants for effect evidence. |
| Math verification and explicit contexts, M2/M4 | Extend existing tests with source-identified original vectors/logs for one transform/collision/targeting slice. | Boundary/alias/overflow vectors and independent-context tests; canonical state remains equal across presentation cadence. No wholesale float conversion. |
| Native renderer vertical slice, M4 | Typed title/UI submission, then one room/actor path with explicit materials, capture and depth policy. | Migrated slice emits no GP0/OT links; approved visual fixtures pass; CPU/GPU/allocation captures fit the chosen hardware budget. |
| Typed content and owned async loading, M1/M7 | One versioned room/model/enemy/item slice with stable IDs, checked definitions and cancellable requests. | Decode/export/load equivalence, invalid-reference failures, second entry/unload-during-load tests and measured memory/transition budgets. |
| Session/actor/effect lifecycle, M3/M7/M8 | Owned state and workspaces for one complete encounter/scene loop. | Entry, defeat, cancellation, failure and repeated teardown return to valid state without original slot offsets or stale handles in migrated consumers. |
| Recoverable persistence, M9 | Owner-selected save representation, semantic snapshots, validated load and recoverable commit. | Round-trip/migration/import/corruption/interrupted-write/repeated-Continue evidence; no pointer or padding serialization. |
| Full retirement, M10/Phase 2e | Remove remaining live bridges after consumer migration. | No renamed hardware model outside Legacy; no required stubs/unconverted assets; full-content/gameplay evidence and independent retirement evidence. |

## 11. Review Coverage and Limits

The review used the current source-port plan, build/quality/watchlist contracts,
asset/enemy designs, naming/tracing rules, compatibility/assembly audits,
handoff/Combat Lab evidence, tool inventory and Phase-2/3 scope documents. Ledger
CSV and ownership/manifest data were inspected structurally. The pasted build
output was treated as historical diagnostics, not current compiler output.

Native inspection combined a tracked-file inventory and repository-wide targeted
scans with detailed reads of the desktop/fiber/clock/session loop; GPU/page/packet
paths; module/data/disc/actor loading; actor/AI cleanup, contexts and records;
inventory, room loading, event VM, menu organization, particles/effects/sphere;
audio voice publication, callbacks, banks, XA and movie decoding; save helpers;
ending rendering; Combat Lab tracing; build generation; CI and neighboring tests.

This is an extensive architectural review with representative deep source
inspection, **not a claim that every one of the 214,768 text lines received an
independent semantic proof**. It is not a complete license/security audit,
all-content playthrough, leak certification or fresh performance profile.
Findings distinguish source facts, known historical evidence, acceptance gaps
and hypotheses requiring runtime measurements.

Only build instructions and this report were edited. Native fixes, architectural
decisions, issue updates, regeneration, commits and pushes remain outside this
review's scope.

### External References Consulted

- [SDL3 GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU): backend boundary,
	resource reuse/cycling, render passes, shader tooling and hardware requirements.
- [SDL3 audio callback contract](https://wiki.libsdl.org/SDL3/SDL_AudioStreamCallback):
	callback/thread synchronization and source lifetime.
- [Fix Your Timestep](https://gafferongames.com/post/fix_your_timestep/): fixed
	simulation, bounded catch-up and interpolated rendering, applied here without
	changing the game's field timing.
- [Ordered work #4](https://github.com/JEschete/VS-NativePort/issues/4),
	[architecture/persistence #18](https://github.com/JEschete/VS-NativePort/issues/18)
	and the exact-revision Source Quality run linked above: publicly observed
	tracker/CI state, separate from local execution.
