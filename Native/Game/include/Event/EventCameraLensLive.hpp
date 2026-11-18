#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.hpp), the
// tick speed and the shared scratch words to the typed lens and shake rules
// (EventCameraLens.hpp). Each entry keeps the original signature so the
// camera update, the module bindings and the opcode table can use it in place
// of the copy. An entry refuses a missing workspace or record; the original
// wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
struct LensTween;
struct ShakeOscillator;

// decomp: BATTLE.PRG func_800BDC9C (F01232, 4A0A8.c:3862), opcodes 0xEA-0xEC
// and 0xEF are bound by Event_CommandCameraLensOrShakeOpcode
// (ScriptControlOpcodes.h).

// decomp: BATTLE.PRG func_800BDF6C (F01233, 4A0A8.c:3927)
void UpdateEventLensTweenEntry(LensTween* lens_tween);
// decomp: BATTLE.PRG func_800BE01C (F01234, 4A0A8.c:3945)
// One of CameraState::shake_oscillators (workspace offsets 0x1A6 and 0x1B2).
int UpdateEventShakeOscillatorEntry(ShakeOscillator* oscillator);
// decomp: BATTLE.PRG func_800BE180 (F01235, 4A0A8.c:3983)
void UpdateEventCameraShakeEntry();
}  // namespace VS::Event
