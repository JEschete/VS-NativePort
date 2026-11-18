#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.hpp), the
// field camera, the renderer, the GTE and the scratch words to the typed
// camera frame step (EventCameraFrame.hpp). Each entry keeps the original
// signature so the script VM, the module bindings and the opcode table can
// use it in place of the copy. A missing workspace or instruction is logged
// and skipped; the original read through a null pointer.
#include <cstdint>

namespace VS::Event {
// decomp: BATTLE.PRG func_800BB41C (F01197, 4A0A8.c:2788), opcode 0xE0 (the
// camera wait), is bound by Event_CommandWaitCameraTracksOpcode
// (ScriptControlOpcodes.h).

// decomp: BATTLE.PRG vs_battle_applyCameraState (F01219, 4A0A8.c:3117)
void ApplyEventCameraStateEntry();
// decomp: BATTLE.PRG func_800BC9E0 (F01222, 4A0A8.c:3371)
void UpdateEventCameraTracksEntry();
// decomp: BATTLE.PRG func_800BD610 (F01228, 4A0A8.c:3663) The completion
// bits, or 0 without a workspace.
int EventCameraCompletionMaskEntry();

// Missing workspaces and instructions the entries skipped.
[[nodiscard]] std::uint64_t RefusedEventCameraFrameCount() noexcept;
}  // namespace VS::Event
