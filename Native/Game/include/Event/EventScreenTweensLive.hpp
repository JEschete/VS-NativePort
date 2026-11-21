#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (cameraWorkspace), the
// tick speed and the live captured screen and radial warp to the typed screen
// tweens (EventScreenTweens.hpp). Each entry keeps the original signature so
// the opcode tables, the module bindings and the callers can use it in place
// of the copy. A missing workspace or tween is logged and skipped; the
// original wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
struct AngleTween;
struct TripleTween;

// decomp: BATTLE.PRG vs_battle_script_E1 (F01198,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3442; was func_800BB450), opcode 0xE1, and
// vs_battle_script_E2_E3_camera (F01226, src/BATTLE/BATTLE.PRG/4DEA8.c:4251;
// was func_800BD444), opcodes 0xE2 and 0xE3, and
// vs_battle_script_B9_BA_BF_E4_E5_E6_E7_ED (F01229,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4341; was func_800BD6C4), opcodes 0xB9, 0xBA,
// 0xBF, 0xE4-0xE7 and 0xED, are bound by the Event_*Opcode entries in
// ScriptControlOpcodes.h.

// decomp: BATTLE.PRG func_800BD57C (F01227, src/BATTLE/BATTLE.PRG/4DEA8.c:4297)
void UpdateEventAngleTweenEntry(AngleTween* tween);
// decomp: BATTLE.PRG func_800BDAB4 (F01230, src/BATTLE/BATTLE.PRG/4DEA8.c:4462)
void UpdateEventScreenTransformsEntry();
// decomp: BATTLE.PRG func_800BDBB4 (F01231, src/BATTLE/BATTLE.PRG/4DEA8.c:4488)
// Returns 1 when the tween advanced, else 0.
int UpdateEventTripleTweenEntry(TripleTween* tween);

// Missing workspaces, tweens and instructions the entries skipped.
[[nodiscard]] std::uint64_t RefusedEventScreenTweenCount() noexcept;
}  // namespace VS::Event
