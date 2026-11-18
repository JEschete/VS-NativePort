#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.hpp), the
// tick speed and the live captured screen and radial warp to the typed screen
// tweens (EventScreenTweens.hpp). Each entry keeps the original signature so
// the opcode tables, the module bindings and the callers can use it in place
// of the copy. A missing workspace or tween is logged and skipped; the
// original wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
struct AngleTween;
struct TripleTween;

// decomp: BATTLE.PRG func_800BB450 (F01198, 4A0A8.c:2793), opcode 0xE1, and
// func_800BD444 (F01226, 4A0A8.c:3601), opcodes 0xE2 and 0xE3, and
// func_800BD6C4 (F01229, 4A0A8.c:3691), opcodes 0xB9, 0xBA, 0xBF, 0xE4-0xE7
// and 0xED, are bound by the Event_*Opcode entries in ScriptControlOpcodes.h.

// decomp: BATTLE.PRG func_800BD57C (F01227, 4A0A8.c:3647)
void UpdateEventAngleTweenEntry(AngleTween* tween);
// decomp: BATTLE.PRG func_800BDAB4 (F01230, 4A0A8.c:3812)
void UpdateEventScreenTransformsEntry();
// decomp: BATTLE.PRG func_800BDBB4 (F01231, 4A0A8.c:3838) Returns 1 when the
// tween advanced, else 0.
int UpdateEventTripleTweenEntry(TripleTween* tween);

// Missing workspaces, tweens and instructions the entries skipped.
[[nodiscard]] std::uint64_t RefusedEventScreenTweenCount() noexcept;
}  // namespace VS::Event
