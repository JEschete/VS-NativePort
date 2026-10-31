# Emulation-like behavior and native modernization audit

Companion to [NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md). Implementation packages and dependencies
are in that plan's §14; the stable `EC-*` IDs below identify the behavior each package must address.

**Baseline:** source inspection with HEAD `680baf18` (scripted desktop input), including local working-tree files
present during review. This is not a fresh build, playthrough, performance measurement, or certification of parity.
Recheck the cited functions when updating an issue; runtime logs from older builds do not establish current status.

## 1. Classification and policy

The project currently combines **native game execution, modern desktop services, PS1 compatibility behavior,
partial device-state emulation, and unfinished subsystems**. These categories are not interchangeable:

| Category | Meaning |
|---|---|
| Native execution | Game logic is compiled for x64; the inspected boot path calls native functions, not a MIPS interpreter. |
| Modern native system | Inputs, ownership and outputs are expressed as game/platform concepts rather than PS1 registers, packets or addresses. |
| Compatibility adapter | An original interface is translated into a native service, such as controller input or scheduling. |
| Behavioral emulation | Hardware-visible state or operations are reproduced so original hardware-shaped code can keep working. This need not be cycle-accurate or a full console emulator. |
| Stub | Required behavior is absent, even if a placeholder returns success or lets boot continue. |

A `.cpp` extension, a renamed function, a native allocation, or an SDL/D3D12 backend does not by itself establish
modernization. Conversely, fixed-point arithmetic, an original RNG sequence, event-script interpretation and AKAO
sequencing can be legitimate native game behavior.

**Planning assumption:** preserve the source-port plan's existing final goal of retiring hardware-shaped runtime
bridges. Existing implementations are recorded here, not retroactively declared compliant with that goal.
The `PageSpace` header records a page-image owner decision; its long-term shipping status must be reconciled with
plan D1/D8 and §2.2. Retaining an optional compatibility renderer requires an explicit decision, separate verification
and a dependency boundary. No such new approval is implied by this audit.

## 2. What is already native

- `Native/Desktop/src/main.cpp`: native initialization, event loop, presentation and capture orchestration.
  `--press` and `--scripted` support tick-indexed menu-driving input; `--frames`, `--capture` and `--at` support
  bounded capture runs. These are foundations for verification, not yet proof of deterministic replay parity.
- `Native/Engine/src/Platform/App.cpp`, `GameFiber.cpp`, and `CrashHandler.cpp`: native window/platform services,
  cooperative execution and crash diagnostics. A Windows fiber is not CPU emulation.
- `Native/Engine/src/Input/Pad.cpp`: SDL device input and native pad-state processing.
- `Native/Engine/src/Core/FrameClock.cpp` and `Random.cpp`: native timing/RNG implementations. Preserving original
  sequences and logical rates is intentional fidelity, not a reason to replace their algorithms.
- `Native/Engine/src/Render/Device.cpp`: native GPU resource management and presentation. The game image is currently
  produced elsewhere; this is not evidence that the game primitives are rendered by the host GPU.
- `Native/Engine/src/Audio/AudioDevice.cpp`: SDL output and source-stream infrastructure. Output availability and
  actual sequenced music/SFX playback are separate gates.
- `Native/Content/src/DataStore.cpp`: native filesystem access. Its callers still depend on legacy resource addressing.

## 3. Issue register

| ID | Area | Current classification | Owning systems | Plan packages |
|---|---|---|---|---|
| EC-01 | GPU packets, ordering tables and page image | Behavioral emulation | Engine/Render | M3, M4, M7, M10 |
| EC-02 | SPU registers, sample memory and transfers | Native sample/noise/FM/reverb/effect/XA output connected; unused DMA and other device-shaped consumers remain | Engine/Audio | M5, M10 |
| EC-03 | Stateful GTE-style math interface | Native arithmetic with coprocessor-style compatibility | Engine/Core, Render, Game/Actors | M4, M7, M10 |
| EC-04 | Sector requests and module-address relocation | Storage adapter plus original data-layout coupling | Content, Game/Boot | M1, M4-M9, M10 |
| EC-05 | Scratchpad and overlapping overlay storage | Memory-layout compatibility | Engine/Core, Game/Boot and resource owners | M1, M3, M7, M9, M10 |
| EC-06 | VSync, callbacks and root-counter behavior | Scheduling adapter plus hardware-shaped timing | Engine/Core, Platform, Game/Boot | M2, M3, M5, M10 |
| EC-07 | Original controller receive buffers | Protocol compatibility adapter | Engine/Input, Game/Title | M3, M9, M10 |
| EC-08 | Movie backend and MDEC-shaped presentation | Native AV1/FLAC decoding connected; page-space presentation and lifecycle retirement remain | Engine/Movie, Content | M6, M10 |
| EC-09 | Memory-card API and native persistence | Initial schema3 save/load milestone delivered; imports, clear saves, campaign and final adapter retirement remain | Engine/Save, Game/State | M9, M10 |

### EC-01: GPU packet interpreter and page-image renderer

**Evidence:** `Native/Engine/Legacy/Render/LibGpu.cpp` (`SubmitPacket`, `LegacyGpu_WalkList`),
`Native/Engine/include/Render/PageSpace.hpp`, `Native/Engine/src/Render/PageSpace.cpp`, and
`Native/Engine/src/Render/Device.cpp` (`PresentImage`).

The game builds PS1 command packets. The shim follows ordering-table links and executes command words against a
1024x512 space of 16-bit pixels, preserving texture-page, palette, mask, blend, upload/readback and display behavior.
The desktop uploads the resulting RGBA image and presents it through SDL GPU. It is a functional compatibility
renderer, not just a texture lookup table and not a complete cycle-accurate GPU emulator.

**Issue:** native geometry submission is still packet-shaped; x64-safe ordering-table handles fix pointer truncation
but do not replace ordering tables. Host GPU presentation alone does not satisfy the target rendering architecture.

**Retain:** draw order, palette animation, blending, clipping, required readbacks and selectable original visual rules.
**Replace:** packet construction/traversal with typed draw items and explicit sort keys; use texture/material handles
and a defined render-target capture API. Preserve an isolated comparison path until replacement outputs are verified.

**Retirement gate:** migrated callers emit no GP0 packets or ordering-table links. Captures cover title/UI, room and
actor geometry, all blend modes in use, palette updates, clipping and framebuffer-dependent effects. Shipping the old
backend, if later approved, must not make the modern path depend on it.

### EC-02: SPU register and sample-memory model

**Evidence:** `Native/Engine/Legacy/Audio/LibSpu.cpp` (`g_registers`, `g_ram`, `SpuWrite`, `SpuRead`),
`Native/Engine/src/Audio/Driver/SoundDriver.h`, `Driver/PlaybackReset.c`, `SoundEffects/SoundEffects.c`, and
`Native/Engine/src/Audio/AudioDevice.cpp`.

Remaining compatibility operations redirect register offsets into an ordinary memory block and use a 512 KiB
SPU-shaped transfer block. The first native slice instead binds imported sample assets and connects music/SFX
channel commits to typed `VoiceEngine` controls, sfizz rendering and `AudioDevice::Submit`. Real menu actions
produce captured PCM, including stereo/mono changes. Canonical zone-music field views (G00248-G00250 within
G00246) now let normal prologue events load MUSIC016/WAVE0069 and produce music alongside SFX. The native
stream retains 50 ms of priming, while SDL demand now renders active voices independently of gameplay stalls.
The sequencer publishes complete control ticks; program loading and retirement remain on the control thread,
and the renderer never runs the sequencer. Final-postmix stall tests and movie-source isolation tests cover
that boundary. Native capture is not a capture of physical device gaps. Native ADSR runs separately from sfizz sample playback.
Counter 2 now advances at the host's exact NTSC field-rate ratio, removing the long-run 60-versus-59.94 Hz
underproduction. This repairs existing timing debt; it does not retire the counter bridge.
F03159, F03163-F03169 and F03178 are owned C++ bridges; this is not full behavior verification or EC retirement.
Native noise and preceding-voice FM now render rather than stopping their voices. Reverb uses the original
presets and a native float delay network. Mixer noise/depth/preset controls and boot reverb enable/disable
call typed native APIs through generator fixups; the uninitialized SDK preset getter is no longer a consumer.
All 83 nonempty disc banks import, including the older articulation and explicit unused records.
Effect-package streams now use imported WAV/SFZ assets, canonical bindings, native-width commands and
the existing reserved voice pair. Owned lifecycle/control callbacks replace their production DMA/IRQ
start path. Ending XA uses lossless FLAC and an independent demand-fed SDL source, including CD gain,
fades, stereo/mono, EOF and reset/close. Saved music uses native-sized snapshots and source-backed
conflict release instead of its former no-op.
Unused chunk-upload/refill APIs, other device-shaped consumers and broader reference acceptance remain
migration debt. Unused SDK stubs have not been deleted; EC-02 remains open. Behavior coverage, owned C++
promotion and compatibility retirement are separate gates.

**Issue:** copied voice control still communicates through physical voice bits, register offsets and sample addresses.
A register block plus successful transfers is neither a completed synthesizer nor verified music playback.

**Retain:** AKAO sequencing, priorities, cancellation, tempo changes, pitch/volume slides, loops and transition rules.
**Replace:** driver register writes with typed voice operations, decoded sample handles and a defined mixer/output
contract. D23 selects sfizz/SFZ+WAV; do not build a competing SPU synthesizer or a second event sequencer.

**Retirement gate:** supported music/SFX paths make no SPU register or sample-RAM accesses. Event traces and audible
output cover simultaneous effects/music, voice release/stealing, loops, fades and reverb decisions. Unsupported features
are reported explicitly, not silently treated as working.

### EC-03: GTE-style implicit state

**Evidence:** `Native/Engine/include/Core/Gte.h` and `Native/Engine/src/Core/Gte.cpp` (`GteState`, load/execute/store
operations, MAC/IR values, screen/depth FIFOs and flags).

Arithmetic executes natively, but callers still rely on a persistent coprocessor-shaped state machine.

**Issue:** renaming GTE operations does not remove hidden ordering/state dependencies. This also makes isolated tests
and independent transform contexts harder to express.

**Retain:** fixed-point rounding, saturation, clipping and projection rules proven necessary for gameplay/visual parity.
**Replace:** implicit register transactions with explicit transform inputs and result values, or an explicitly owned
math context where state is genuinely required. Do not substitute floating point indiscriminately.

**Retirement gate:** migrated callers use typed math rather than load/execute/store sequences. Boundary-value vectors
exercise saturation, near-plane behavior, winding, negative coordinates and overflow-sensitive cases; independent
contexts do not interfere with one another.

### EC-04: Original resource addresses and module relocation

**Evidence:** `Native/Content/src/DataStore.cpp` (`FindBySector`, `Content_ReadDiscBytes`), `ModuleLoader.cpp`
(`LoadData`, `SymbolMap::ResolveIn`, `RelocatePointers`), `Native/Game/src/Boot/ModuleData.cpp`, `ModulePointers.c`,
`Native/Tools/Extract/src/main.cpp`, and `Native/Desktop/src/main.cpp`.

Regular disc files are extracted into a file tree and indexed by original LBA/size. The game still requests sectors;
module data is copied into globals and original addresses are resolved into native symbols. Desktop startup still opens
a disc as well as the extracted data. STR converts to Matroska (AV1/FLAC); effect streams convert to WAV/SFZ
and ending XA to FLAC under `audio/streams`. The original XA `files.tsv` row remains `stream` for
sector-addressed lookup, separately from the native asset index.

**Issue:** extraction is not yet conversion to the typed/versioned assets in plan D18. Interior-pointer resolution adds
an original byte offset to a native object; that is valid only where layout identity is established. Pointer-table extents,
aliases and function signatures require evidence. Unresolved pointers currently become null and boot logs a warning.

**Retain:** source provenance, stable original IDs for import/comparison, and source addresses inside extraction metadata
where needed to decode the original format. These are not runtime guest addresses after conversion.
**Replace:** sector-facing runtime APIs with asset IDs/handles; decode fields explicitly; map behavior tables to typed
registrations rather than serialized function addresses. Separate disk layout from native object layout.

**Retirement gate:** required tables resolve with validated bounds before their consumers run; malformed inputs produce
precise errors. Ultimately the shipping game runs from versioned converted assets with the original disc unavailable and
has no module-image copying, sector arithmetic or original-address relocation dependency.

### EC-05: Scratchpad and overlay-offset preservation

**Evidence:** `Native/Engine/src/Core/Scratch.cpp` (`g_scratch[1024]`), `Native/Engine/include/Core/Arena.h`
(`Core_overlayArena`, `Core_OverlayAddress`), `Native/Engine/src/Core/Overlays.cpp`, and
`Native/Game/src/Boot/OverlayReset.cpp`.

Scratch users share a byte block; overlay slots retain original relative addresses and overlaps inside native storage.
The modules execute as native code, but their original loading/memory relationships still influence behavior.

**Issue:** lifetimes, aliasing and reload resets remain implicit. A native arena itself is not emulation; preserving PS1
slot offsets so original assumptions keep working is the compatibility layer.

**Retain:** only intentional state reset and resource lifetime behavior. Do not blindly clear every static on module entry.
**Replace:** scratch offsets with typed workspaces, shared slot ownership with explicit buffers, and overlay entry with
owned module state and reviewed `Init/Reset/Update/Shutdown` transitions.

**Retirement gate:** repeated title/menu/room/effect transitions pass state checks without original offsets. Tests cover
second entry, reset versus persistent state, cancellation, stale handles and x64 layouts. Native arenas may remain when
their capacity and ownership no longer encode a PlayStation memory map.

### EC-06: Hardware-shaped scheduling and counters

**Evidence:** `Native/Engine/Legacy/Core/LibEtc.cpp`, `Native/Engine/src/Platform/GameFiber.cpp`,
`Native/Game/src/Boot/GameApp.cpp`, and `Native/Engine/src/Core/FrameClock.cpp`.

`VSync` yields the game fiber, tracks vertical blanks and invokes callbacks; a horizontal-line query returns a fixed
value. Root counter 2 is synthesized from a steady clock at the original counter frequency.

**Issue:** the host loop is native, but game control flow still depends on SDK waits/callbacks. Audit counter consumers
before asserting determinism: some readings are diagnostics, while any gameplay-affecting use needs a deterministic
replacement. Wall-clock measurements must not leak into canonical gameplay state.

**Retain:** original logical tick rates, event ordering and gameplay delays. A fiber can remain an implementation detail
if its lifecycle and yield contract are explicit; removing fibers is not itself a modernization requirement.
**Replace:** game-facing hardware waits with explicit tick scheduling/state transitions; separate diagnostics from
simulation time and specify pause/stall/catch-up behavior.

**Retirement gate:** replay state is repeatable under varied presentation cadence; reset, pause, exit and error paths do
not strand execution in a blocking wait. No gameplay caller requires root-counter or scanline semantics.

### EC-07: Controller protocol translation

**Evidence:** `Native/Engine/src/Input/Pad.cpp`, `Native/Engine/Legacy/Input/LibPad.cpp` (`FeedPad`), and
`Native/Game/src/Boot/GameApp.cpp`.

SDL input is encoded back into original active-low controller receive buffers so copied controller code can decode it.
This is a protocol adapter, not a full controller hardware emulator. Actuator calls are currently placeholders.

**Issue:** game input still depends on buffer layouts, port IDs and legacy connection/mode conventions.
**Retain:** button edges/repeat timings, analog behavior, disconnect semantics and game-specific input rules.
**Replace:** consumers with native pad/action state; map rumble to the platform implementation with explicit lifetime.

**Retirement gate:** title and gameplay use native input APIs; tests cover keyboard/gamepad equivalence, edges, repeat,
analog axes, hot-plug and rumble cancellation. The legacy receive-buffer writer can be removed after its last consumer.

### EC-08: Native decoding with compatibility-backed presentation

**Evidence:** `Native/Engine/src/Movie/Intro/Player.cpp`, `Native/Engine/src/Movie/MoviePlayer.cpp`,
`Native/Engine/Legacy/Movie/LibPress.cpp`, and `Native/Tools/Extract/src/main.cpp`.

The current player opens converted Matroska assets through DataStore, decodes AV1 with dav1d and FLAC with dr_flac,
and submits native audio. Frame advancement uses movie ticks converted to nanoseconds. Rendering still uploads RGB
data through page space; the global player and intro's existing scheduling/display contracts remain migration debt.
The old MDEC-facing functions are still present. This source audit corrects the earlier missing-decoder description;
it is not fresh frame-count, A/V-clock, skip, re-entry or soundtrack-tail acceptance.

**Retain:** picture duration, skip rules, attract-loop behavior and the soundtrack tail continuing under the title.
**Replace:** remaining page-coordinate outputs and implicit session lifecycle with explicit frame/audio ownership,
a native presentation contract and one defined synchronization clock.

**Retirement gate:** verified frame count/duration, A/V synchronization, skip, re-entry and audio-tail behavior. Missing
or invalid converted assets fail explicitly. Black placeholder frames do not pass movie parity.

### EC-09: Memory-card stubs versus native persistence

**Evidence:** `Native/Engine/Legacy/Save/LibCard.cpp`, `Native/Engine/src/Save/SaveStore.cpp`,
`Native/Game/src/Save/NativeSnapshot.cpp`, `RuntimeSnapshot.cpp`, `NativeMenu.cpp` and `NativeResume.cpp`.

Production Save/Load/Continue menus now use the native file store, schema3 semantic JSON, validated staged resume,
explicit backup recovery/replacement consent and session cancellation. The initial native-only milestone has
production-controller evidence recorded on #18; the former directory-helper-only description is obsolete.
Legacy file/card operations still contain stubs and have not been retired.

The schema rejects unsupported versions rather than migrating them. Clear-save conversion still explicitly reports
unavailable without changing files. PS1 import, old-schema migration, full-story/NG+ round trips, unsupported
transient/support states and final card-adapter retirement remain open. Component tests or successful Continue
checks do not establish those separate gates; no memory-card emulator is required or introduced.

**Retain:** semantic game state and import compatibility with user-supplied PS1 saves.
**Replace:** hardware/card events with versioned serialization, stable IDs, validation and recoverable file operations.
Import is an explicit decoder, not a requirement to retain card hardware behavior at runtime.

**Retirement gate:** save/load semantic round trips, interrupted writes, corrupt/unsupported versions, missing asset IDs,
PS1 imports and repeated Continue operations are tested. Serialization contains no native pointers or ABI-dependent structs.

## 4. Tracking and closure

Each implementation change updates the relevant issue with: revision, migrated consumers, remaining dependencies,
verification artifact, required/optional failure policy and retirement evidence. A zero SDK-name count alone cannot close
an issue if the same hardware model has merely moved behind a different name or outside `Legacy`. Per-EC consumer counts
come from [EC_BRIDGES.csv](ledgers/EC_BRIDGES.csv) through `tools\ledger\gates.py`; a listed bridge that disappears fails
that check. Counts report consumers; they do not close an EC.

Code is **integrated** when exercised through its real caller. It is **verified** only when a reproducible test/replay or
reference comparison passes. Compatibility-backed behavior may be verified before it is architecturally modernized;
record those achievements separately. Do not mark any gate passed merely because this document or an API exists.
