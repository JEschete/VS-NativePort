#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.hpp) and
// the field camera override (Event_skipNextFieldCameraRestore) to the typed
// ownership rules (EventCameraOwnership.hpp). Each entry keeps the original
// signature so the opcode table, the module bindings and the callers can use
// it in place of the copy. An entry that needs the workspace refuses a
// missing one; the original wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
// decomp: BATTLE.PRG func_800BB38C (F01195, 4A0A8.c:2774), opcode 0xE8 is
// bound by VS::Event::CommandSetCameraOriginActorOpcode
// (ScriptControlOpcodes.hpp). decomp: BATTLE.PRG func_800BBDDC (F01215,
// 4A0A8.c:3082) only forwarded to
// Core::Memory::FreeLiveFromHighEnd(Event_camera) and is retired.

// decomp: BATTLE.PRG func_800BBBE8 (F01214, 4A0A8.c:3014)
// Allocates a new workspace from the heap's high end without releasing a
// previous one, as the original did; a refused allocation aborts.
void InitializeEventCameraEntry();
// decomp: BATTLE.PRG func_800BBE04 (F01216, 4A0A8.c:3084)
void SetEventFieldCameraOverrideEntry(int field_camera_override);
// decomp: BATTLE.PRG func_800BBE10 (F01217, 4A0A8.c:3086)
void SetEventCameraOriginActorEntry(std::uint16_t origin_actor);
// decomp: BATTLE.PRG func_800BBE94 (F01218, 4A0A8.c:3101)
void UpdateEventCameraOriginEntry();
}  // namespace VS::Event
