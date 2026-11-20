#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (cameraWorkspace), the
// tick speed and the shared camera scratch (EventCameraScratch) to the typed
// lens and shake rules (EventCameraLens.hpp). Each entry keeps the original
// signature so the camera update, the module bindings and the opcode table can
// use it in place of the copy. An entry refuses a missing workspace or record;
// the original wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
struct EventCameraScratch;
struct LensTween;
struct ShakeOscillator;

// decomp: cameraShakeScratch (scratchpad 0x1F800088-0x1F8000EE): the words
// the lens tweens, the screen tweens (EventScreenTweensLive.cpp) and the shake
// share. The session owns it: an oscillator with an unknown envelope shape
// reuses the last eased fraction, so it lives across frames and scenes and is
// reset only at a session boundary (ResetLiveEventCameraScratch).
[[nodiscard]] EventCameraScratch& LiveEventCameraScratch() noexcept;
// Restores the cold-boot state (all zero) at a session boundary.
void ResetLiveEventCameraScratch() noexcept;

// decomp: BATTLE.PRG func_800BDC9C (F01232, 4A0A8.c:3862), opcodes 0xEA-0xEC
// and 0xEF are bound by VS::Event::CommandCameraLensOrShakeOpcode
// (ScriptControlOpcodes.hpp).

// decomp: BATTLE.PRG func_800BDF6C (F01233, 4A0A8.c:3927)
void UpdateEventLensTweenEntry(LensTween* lens_tween);
// decomp: BATTLE.PRG func_800BE01C (F01234, 4A0A8.c:3945)
// One of CameraState::shake_oscillators (workspace offsets 0x1A6 and 0x1B2).
int UpdateEventShakeOscillatorEntry(ShakeOscillator* oscillator);
// decomp: BATTLE.PRG func_800BE180 (F01235, 4A0A8.c:3983)
void UpdateEventCameraShakeEntry();
}  // namespace VS::Event
