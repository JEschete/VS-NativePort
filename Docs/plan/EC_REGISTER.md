# Native Source Port Plan: Emulation-compatibility register (EC-01–EC-03)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · EC-04 and its T-shim ledger: [EC_REGISTER_2.md](EC_REGISTER_2.md) ·
EC-05–EC-09 and §15.4: [EC_REGISTER_3.md](EC_REGISTER_3.md)

## 15. Emulation-compatibility register

Stable `EC-*` IDs for the remaining hardware-shaped behavior. The [§14](WORK_PACKAGES.md#14-implementation-work-packages)
packages and #18 reference these IDs. Each entry identifies the behavior its package must address, the native
foundation already in place, the replacement target and the retirement gate. This section was previously
`Docs/EMULATION_COMPATIBILITY_AUDIT.md`.

**Baseline:** entries record source inspection, not a fresh build, playthrough, performance measurement or
certification of parity. Recheck the cited functions when updating an issue; runtime logs from older builds do not
establish current status.

### 15.1 Classification and policy

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
and a dependency boundary. No such new approval is implied by this register.

**Asset adoption clarification (owner, 2026-11-23):** reconstructing a legacy asset in memory is still a T-shim.
Native JSON/CSV/glTF input, native allocation, or a passing byte comparison does not retire the original-layout
consumer. Byte equality is an **offline import-preservation test**, not a runtime architecture or a completion gate.
Production consumes typed definitions and owned instances. New native content need not be exportable to the original
format; deliberate gameplay limits must be distinguished from format limits.

### 15.2 What is already native

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

### 15.3 Issue register

| ID | Area | Current classification | Owning systems | Plan packages |
|---|---|---|---|---|
| EC-01 | GPU packets, ordering tables and page image | Behavioral emulation | Engine/Render | M3, M4, M7, M10 |
| EC-02 | SPU registers, sample memory and transfers | Native sample/noise/FM/reverb/effect/XA output connected; unused DMA and other device-shaped consumers remain | Engine/Audio | M5, M10 |
| EC-03 | Stateful GTE-style math interface | Native arithmetic with coprocessor-style compatibility | Engine/Core, Render, Game/Actors | M4, M7, M10 |
| EC-04 | Sector requests and module-address relocation | Storage adapter plus original data-layout coupling | Content, Game/Boot | M1, M4-M9, M10 |
| EC-05 | Scratchpad and overlapping overlay storage | Memory-layout compatibility | Engine/Core, Game/Boot and resource owners | M1, M3, M7, M9, M10 |
| EC-06 | VSync, callbacks and root-counter behavior | Retired (#44): logical-tick waits, sound timer, fiber cancellation, error transport and explicit session modes | Engine/Core, Platform, Game/Boot | M2, M3, M5, M10 |
| EC-07 | Original controller receive buffers | Protocol adapter retired (#52); copied gameplay and menus still read the legacy pad-word view (#489) | Engine/Input, Game/Title | M3, M9, M10 |
| EC-08 | Movie backend and MDEC-shaped presentation | Native AV1/FLAC decoding connected; page-space presentation and lifecycle retirement remain | Engine/Movie, Content | M6, M10 |
| EC-09 | Memory-card API and native persistence | Initial schema3 save/load milestone delivered; imports, clear saves, campaign and final adapter retirement remain | Engine/Save, Game/State | M9, M10 |

EC-01–EC-03 follow; [EC-04](EC_REGISTER_2.md#ec-04-original-resource-addresses-and-module-relocation) is in
EC_REGISTER_2.md and [EC-05–EC-09](EC_REGISTER_3.md) in EC_REGISTER_3.md.

#### EC-01: GPU packet interpreter and page-image renderer

**Evidence:** `Native/Engine/Legacy/Render/LibGpu.cpp` (`SubmitPacket`, `VS::Legacy::GpuWalkList`),
`Native/Engine/include/Render/PageSpace.hpp`, `Native/Engine/src/Render/PageSpace.cpp`, and
`Native/Engine/src/Render/Device.cpp` (`PresentImage`).

The game builds PS1 command packets. The shim follows ordering-table links and executes command words against a
1024x512 space of 16-bit pixels, preserving texture-page, palette, mask, blend, upload/readback and display behavior.
The desktop uploads the resulting RGBA image and presents it through SDL GPU. It is a functional compatibility
renderer, not just a texture lookup table and not a complete cycle-accurate GPU emulator.

**Issue:** native geometry submission is still packet-shaped; x64-safe ordering-table handles fix pointer truncation
but do not replace ordering tables. Host GPU presentation alone does not satisfy the target rendering architecture.

**Progress (#50):** room geometry and the sky background submit typed `Render::DrawPolygon` items through
`Render::FrameDrawList` and `PageSpace::Draw`, with no GP0 packets or ordering-table links. Links into the frame
buffers' primitive and ordering-table memory are frame-relative and generation-checked, so `GpuWalkList` refuses a
stale link instead of following it. Static and heap packets outside those regions keep run-lifetime handles until
their callers migrate.

**Retain:** draw order, palette animation, blending, clipping, required readbacks and selectable original visual rules.
**Replace:** packet construction/traversal with typed draw items and explicit sort keys; use texture/material handles
and a defined render-target capture API. Preserve an isolated comparison path until replacement outputs are verified.

**Retirement gate:** migrated callers emit no GP0 packets or ordering-table links. Captures cover title/UI, room and
actor geometry, all blend modes in use, palette updates, clipping and framebuffer-dependent effects. Shipping the old
backend, if later approved, must not make the modern path depend on it.

#### EC-02: SPU register and sample-memory model

**Evidence:** `Native/Engine/Legacy/Audio/LibSpu.cpp` (`spuRegisters`, `spuRam`, `SpuWrite`, `SpuRead`),
`Native/Engine/src/Audio/Driver/SoundDriver.hpp`, `Driver/PlaybackResetLive.cpp` (the live binding of the typed
reset), `SoundEffects/SoundEffects.c`, and `Native/Engine/src/Audio/AudioDevice.cpp`.

Remaining compatibility operations redirect register offsets into an ordinary memory block and use a 512 KiB
SPU-shaped transfer block. The native path binds imported sample assets and connects music/SFX channel commits to
typed `VoiceEngine` controls, sfizz rendering and `AudioDevice::Submit`; its output, threading and fidelity contracts
are stated once in [M5](WORK_PACKAGES_2.md#m5--sequenced-musicsfx-and-native-audio-output). Canonical zone-music field
views (G00248-G00250 within G00246) drive normal music loading. Counter 2 advances at the host's exact NTSC
field-rate ratio; this repairs timing debt but does not retire the counter bridge. F03159, F03163-F03169 and F03178
are owned C++ bridges, not full behavior verification or EC retirement. Effect-package streams replace their
production DMA/IRQ start path with owned lifecycle/control callbacks. Unused chunk-upload/refill APIs, unused SDK
stubs, other device-shaped consumers and broader reference acceptance remain; behavior coverage, C++ promotion and
compatibility retirement are separate gates. Open work: #253.

**Issue:** copied voice control still communicates through physical voice bits, register offsets and sample addresses.
A register block plus successful transfers is neither a completed synthesizer nor verified music playback.

**Retain:** AKAO sequencing, priorities, cancellation, tempo changes, pitch/volume slides, loops and transition rules.
**Replace:** driver register writes with typed voice operations, decoded sample handles and a defined mixer/output
contract. D23 selects sfizz/SFZ+WAV; do not build a competing SPU synthesizer or a second event sequencer.

**Retirement gate:** supported music/SFX paths make no SPU register or sample-RAM accesses. Event traces and audible
output cover simultaneous effects/music, voice release/stealing, loops, fades and reverb decisions. Unsupported features
are reported explicitly, not silently treated as working.

#### EC-03: GTE-style implicit state

**Evidence:** `Native/Engine/include/Core/Gte.hpp` and `Native/Engine/src/Core/Gte.cpp` (`GteState`, load/execute/store
operations, MAC/IR values, screen/depth FIFOs and flags).

Arithmetic executes natively, but callers still rely on a persistent coprocessor-shaped state machine.

**Status (#51, explicit transform slice):** `Core/Math/Projection.hpp` gives RTV0TR and RTPS as functions
(`RotateTranslateVertex`, `ProjectVertex`) of a `Transform3`, a `PerspectiveProjection` and a vertex, returning MAC,
IR, the screen point, depth, depth cue and FLAG with the 44-bit wrap and saturation flags; the bridge's `Rtps` and
`Rtv0Tr` run the same functions. Room, sky, model, shadow, transition and effect transformers project through them
with their own view and a per-draw projection snapshot and load no register. The shared projection (screen offset,
distance, depth cue, set through `SetGeomOffset`, `SetGeomScreen` and `SetDepthCue`) is the one remaining owned
context. Open work: #495 (remaining register-bridge call sites).

**Issue:** renaming GTE operations does not remove hidden ordering/state dependencies. This also makes isolated tests
and independent transform contexts harder to express.

**Retain:** fixed-point rounding, saturation, clipping and projection rules proven necessary for gameplay/visual parity.
**Replace:** implicit register transactions with explicit transform inputs and result values, or an explicitly owned
math context where state is genuinely required. Do not substitute floating point indiscriminately.

**Retirement gate:** migrated callers use typed math rather than load/execute/store sequences. Boundary-value vectors
exercise saturation, near-plane behavior, winding, negative coordinates and overflow-sensitive cases; independent
contexts do not interfere with one another.
