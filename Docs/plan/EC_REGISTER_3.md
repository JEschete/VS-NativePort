# Native Source Port Plan: Emulation-compatibility register (EC-05–EC-09)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · §15 policy and EC-01–EC-03: [EC_REGISTER.md](EC_REGISTER.md) ·
EC-04: [EC_REGISTER_2.md](EC_REGISTER_2.md)

#### EC-05: Scratchpad and overlay-offset preservation

**Evidence:** `Native/Engine/include/Core/Arena.hpp` (`VS::Core::overlayArena`, `VS::Core::OverlayAddress`),
`Native/Engine/src/Core/Overlays.cpp`, and `Native/Game/src/Boot/OverlayReset.cpp`.

**Scratchpad: retired (#45).** The shared 1 KiB block (`Core/Scratch.hpp`, `ScratchBase`) is deleted. Its users
now own typed workspaces with explicit lifetimes ([§6.8.2](SYSTEMS_2.md#682-state-ownership-and-reset-boundaries-45)):
`Render::FrameScratchpad()`, `Event::LiveEventCameraScratch()`, `Effects::EmitterWorkspace()` and
`Battle::LiveSphereWorkspace()`. No code addresses a scratchpad offset; provenance comments still name the original
offsets.

**Remaining: overlay storage.** Overlay slots retain original relative addresses and overlaps inside native storage.
The modules execute as native code, but their original loading/memory relationships still influence behavior.
Open work: #488.

**Issue:** lifetimes, aliasing and reload resets remain implicit. A native arena itself is not emulation; preserving PS1
slot offsets so original assumptions keep working is the compatibility layer.

**Retain:** only intentional state reset and resource lifetime behavior. Do not blindly clear every static on module entry.
**Replace:** scratch offsets with typed workspaces, shared slot ownership with explicit buffers, and overlay entry with
owned module state and reviewed `Init/Reset/Update/Shutdown` transitions.

**Retirement gate:** repeated title/menu/room/effect transitions pass state checks without original offsets. Tests cover
second entry, reset versus persistent state, cancellation, stale handles and x64 layouts. Native arenas may remain when
their capacity and ownership no longer encode a PlayStation memory map.

#### EC-06: Hardware-shaped scheduling and counters

**Evidence:** `Native/Engine/src/Platform/LogicalTicks.cpp`, `Native/Engine/src/Audio/Driver/TickTimer.cpp`,
`Native/Engine/src/Platform/GameFiber.cpp`, `Native/Game/src/Boot/GameApp.cpp`, and
`Native/Engine/src/Core/FrameClock.cpp`.

The libetc shim is deleted (#44). Game waits call `Platform::WaitTicks`, which yields the game fiber and completes one
logical tick per yield; `Platform::ElapsedTicks` replaces `VSync(-1)`. The sound driver's 240 Hz timer
(`Audio::TickTimer`) is a tick listener that advances by root counter 2's clocks per NTSC field, so sequencer timing is
a function of logical ticks. The horizontal-line query (`VSync(1)`) was a constant natively, so its consumers are
removed with provenance comments. `SetVideoMode(MODE_NTSC)` and the empty vertical-blank callback are deleted.
`tick_timer::ReadClock` keeps the sequencer's tick-cost diagnostic on a steady clock, outside game time.

**Lifecycle contract:** no gameplay caller reads a root counter or scanline, and wall-clock readings stay in
diagnostics. Destroying or cancelling the fiber throws `Platform::FiberCancelled` from the wait it is blocked in, so
every frame unwinds with its destructors; an exception that ends the game is logged where it was thrown and rethrown to
the host by `GameFiber::Resume`; and session transfers are `Boot::SessionTransfer` exceptions caught by
`Boot::RunSession`, replacing `setjmp`/`longjmp`.

**Cadence contract:** presentation cadence is outside game time. `Core::FrameClock` runs at most four catch-up ticks per
present and drops the remaining backlog after a stall, so a stall pauses game time instead of compressing it; a host
pauses by not resuming the fiber, and a step resumes it for exactly one tick (CombatLab F6/F7).

**Status: retired (#44).** Replay state is repeatable under varied presentation speed, and soft resets, exits,
CombatLab pause/step/stop/rerun and window close reach the host without a stranded wait (`VR-BOOT-SESSION-SLICE` in
VERIFY_LEDGER; evidence on #44).

**Retain:** original logical tick rates, event ordering and gameplay delays. A fiber can remain an implementation detail
if its lifecycle and yield contract are explicit; removing fibers is not itself a modernization requirement.
**Replace:** game-facing hardware waits with explicit tick scheduling/state transitions; separate diagnostics from
simulation time and specify pause/stall/catch-up behavior.

**Retirement gate:** replay state is repeatable under varied presentation cadence; reset, pause, exit and error paths do
not strand execution in a blocking wait. No gameplay caller requires root-counter or scanline semantics.

#### EC-07: Controller protocol translation

**Evidence:** `Native/Engine/src/Input/Pad.cpp`, `Native/Engine/src/Input/ControllerLive.cpp` (`SubmitLivePad`,
`LiveRumble`), and `Native/Game/src/Boot/GameApp.cpp`.

**Status (#52, protocol adapter retired):** the libpad shim and both 34-byte receive buffers are deleted. The host
submits each tick's pad as a typed `PadSample` to a native pad link, which answers the game's controller requests
(`ControllerHost`), and registered motor commands become a `Rumble` request applied through
`InputSystem::RequestRumble`. Hosts reset the link when their game ends. Physical controls resolve to logical actions
through validated `Input::Bindings` ([§6.5](SYSTEMS.md#65-input-engineinput)); the title menu reads `LiveActions()` and
typed `DeviceSettings` and no legacy controller globals. Copied gameplay, menu, event and ending files still read the
legacy view (`ControllerLiveState.hpp`: pad-word masks, sticks, ports and the packed settings record).
Open work: #489.

**Issue:** game input still depends on the pad-word bit layout, port IDs and legacy connection/mode conventions.
**Retain:** button edges/repeat timings, analog behavior, disconnect semantics and game-specific input rules.
**Replace:** consumers with native pad/action state; map rumble to the platform implementation with explicit lifetime.

**Retirement gate:** title and gameplay use native input APIs; tests cover keyboard/gamepad equivalence, edges, repeat,
analog axes, hot-plug and rumble cancellation. The legacy receive-buffer writer can be removed after its last consumer.

#### EC-08: Owned movie playback with native frame presentation

**Evidence:** `Native/Engine/src/Movie/Intro/Player.cpp`, `Native/Engine/src/Movie/MoviePlayer.cpp`,
`Native/Engine/src/Movie/Intro/PlayerLive.cpp`, `Native/Desktop/src/main.cpp`, and
`Native/Tools/Extract/src/main.cpp`.

The title intro now owns a move-only `PlaybackSession` over incremental Matroska, AV1 and FLAC decoding. Its audio
source queue is capped at 100 ms; consumed samples drive the presentation clock, with logical-tick and monotonic
fallbacks when output is unavailable or fails. The desktop consumer presents session-owned RGBA frames through the
GPU path instead of writing movie pixels into page space. After skip or natural picture completion, the title host
continues pumping audio without decoding hidden video through the 4401-tick soundtrack tail. Pause and seek are not
supported; closing releases the session, and a new session starts from the beginning.

**Status:** #53 verifies the adopted title-movie slice, including synchronization bounds, skip, natural completion,
audio-tail lifetime, device loss/re-entry and cleanup. The title's legacy timing globals and display-environment
setup remain in its control path; broader EC-08 compatibility retirement is not claimed.

The old MDEC-facing intro routines (TITLE 22C.c:3433-3577), their strip buffers, ring stream and `MovieData_t`
state, the `libpress` shims and the `libds` ring-stream shims are retired (#276).

**Retain:** picture duration, skip rules, attract-loop behavior and the soundtrack tail continuing under the title.
**Remaining boundary:** no pause or seek API; the title still binds its original scheduling and display state.

**Retirement gate:** missing or invalid converted assets fail explicitly; black placeholder frames do not pass movie
parity. The converted title-movie path is verified, while broader movie-asset parity and compatibility retirement
remain separate from #53.

#### EC-09: Memory-card stubs versus native persistence

**Evidence:** `Native/Engine/Legacy/Save/LibCard.cpp`, `Native/Engine/src/Save/SaveStore.cpp`,
`Native/Game/src/Save/NativeSnapshot.cpp`, `RuntimeSnapshot.cpp`, `FileMenu.cpp`, `FileMenuLive.cpp` and
`NativeResume.cpp`.

Production Save/Load/Continue menus use the native file store, schema3 semantic JSON, validated staged resume,
explicit backup recovery/replacement consent and session cancellation; the initial native-only milestone's evidence is
on #18. Legacy file/card operations still contain stubs and have not been retired.

The schema rejects unsupported versions rather than migrating them. Clear-save conversion explicitly reports
unavailable without changing files. PS1 import, old-schema migration, full-story/NG+ round trips, unsupported
transient/support states and final card-adapter retirement remain open. Component tests or successful Continue
checks do not establish those separate gates; no memory-card emulator is required or introduced.

**Retain:** semantic game state and import compatibility with user-supplied PS1 saves.
**Replace:** hardware/card events with versioned serialization, stable IDs, validation and recoverable file operations.
Import is an explicit decoder, not a requirement to retain card hardware behavior at runtime.

**Retirement gate:** save/load semantic round trips, interrupted writes, corrupt/unsupported versions, missing asset IDs,
PS1 imports and repeated Continue operations are tested. Serialization contains no native pointers or ABI-dependent structs.

### 15.4 Tracking and closure

Each implementation change updates the relevant issue with: revision, migrated consumers, remaining dependencies,
verification artifact, required/optional failure policy and retirement evidence. A zero SDK-name count alone cannot close
an issue if the same hardware model has merely moved behind a different name or outside `Legacy`. Per-EC consumer counts
come from [EC_BRIDGES.csv](../ledgers/EC_BRIDGES.csv) through `tools\ledger\gates.py`; a listed bridge that disappears
fails that check. Counts report consumers; they do not close an EC.

Code is **integrated** when exercised through its real caller. It is **verified** only when a reproducible test/replay or
reference comparison passes. Compatibility-backed behavior may be verified before it is architecturally modernized;
record those achievements separately. Do not mark any gate passed merely because this document or an API exists.
