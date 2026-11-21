#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (cameraWorkspace), the
// field camera override (skipNextFieldCameraRestore), the field camera
// placement and the saved field view's script flags to the typed stop,
// follow and field returns (EventCameraControl.hpp). Each entry keeps the
// original signature so the module bindings and the callers can use it in
// place of the copy; the opcode handlers are declared with the other typed
// opcodes in Event/ScriptControlOpcodes.h. A missing workspace or
// instruction is refused where the original wrote or read through a null
// pointer.
#include <cstdint>

namespace VS::Event {
// decomp: BATTLE.PRG func_800BE36C (F01236, src/BATTLE/BATTLE.PRG/4DEA8.c:4668)
// Each request keeps its low byte.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetEventCameraFollowingEntry(int look_at_follow, int eye_follow);
// decomp: BATTLE.PRG func_800BE3A0 (F01237, src/BATTLE/BATTLE.PRG/4DEA8.c:4676)
void StopEventCameraMotionEntry();
// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4DEA8.c:3823)
// The distance mode narrows to the original's halfword. A set override is
// consumed without the workspace, as the original did.
void RestoreEventFieldCameraEntry(std::int16_t field_facing,
                                  int field_distance_mode);
// decomp: BATTLE.PRG func_800BE3D0 (F01238, src/BATTLE/BATTLE.PRG/4DEA8.c:4689)
// Reads the saved field view from script flags 0xAF and 0xB0 and warns for a
// distance index past the height offsets.
void StartEventCameraOrbitReturnEntry(char orbit_side);
}  // namespace VS::Event
