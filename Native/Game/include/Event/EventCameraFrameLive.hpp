#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (cameraWorkspace), the
// field camera, the renderer, the GTE and the scratch words to the typed
// camera frame step (EventCameraFrame.hpp). Each entry keeps the original
// signature so the script VM, the module bindings and the opcode table can
// use it in place of the copy. A missing workspace or instruction is logged
// and skipped; the original read through a null pointer.
#include <cstdint>

namespace VS::Event {
// decomp: BATTLE.PRG vs_battle_script_E0 (F01197,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3437; was func_800BB41C), opcode 0xE0 (the
// camera wait), is bound by VS::Event::CommandWaitCameraTracksOpcode
// (ScriptControlOpcodes.hpp).

// decomp: BATTLE.PRG applyCameraState (F01219,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3767; was vs_battle_applyCameraState)
void ApplyEventCameraStateEntry();
// decomp: BATTLE.PRG func_800BC9E0 (F01222, src/BATTLE/BATTLE.PRG/4DEA8.c:4021)
void UpdateEventCameraTracksEntry();
// decomp: BATTLE.PRG func_800BD610 (F01228, src/BATTLE/BATTLE.PRG/4DEA8.c:4313)
// The completion bits, or 0 without a workspace.
int EventCameraCompletionMaskEntry();

// Missing workspaces and instructions the entries skipped.
[[nodiscard]] std::uint64_t RefusedEventCameraFrameCount() noexcept;
}  // namespace VS::Event
