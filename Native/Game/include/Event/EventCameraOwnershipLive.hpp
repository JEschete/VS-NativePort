#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (cameraWorkspace) and
// the field camera override (skipNextFieldCameraRestore) to the typed
// ownership rules (EventCameraOwnership.hpp). Each entry keeps the original
// signature so the opcode table, the module bindings and the callers can use
// it in place of the copy. An entry that needs the workspace refuses a
// missing one; the original wrote through a null pointer.
#include <cstdint>

namespace VS::Event {
// decomp: BATTLE.PRG vs_battle_script_E8 (F01195,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3423; was func_800BB38C), opcode 0xE8 is bound
// by VS::Event::CommandSetCameraOriginActorOpcode (ScriptControlOpcodes.hpp).
// decomp: BATTLE.PRG func_800BBDDC (F01215, src/BATTLE/BATTLE.PRG/4DEA8.c:3732)
// only forwarded to Core::Memory::FreeLiveFromHighEnd(cameraWorkspace) and is
// retired.

// decomp: BATTLE.PRG func_800BBBE8 (F01214, src/BATTLE/BATTLE.PRG/4DEA8.c:3664)
// Allocates a new workspace from the heap's high end without releasing a
// previous one, as the original did; a refused allocation aborts.
void InitializeEventCameraEntry();
// decomp: BATTLE.PRG func_800BBE04 (F01216, src/BATTLE/BATTLE.PRG/4DEA8.c:3734)
void SetEventFieldCameraOverrideEntry(int field_camera_override);
// decomp: BATTLE.PRG func_800BBE10 (F01217, src/BATTLE/BATTLE.PRG/4DEA8.c:3736)
void SetEventCameraOriginActorEntry(std::uint16_t origin_actor);
// decomp: BATTLE.PRG func_800BBE94 (F01218, src/BATTLE/BATTLE.PRG/4DEA8.c:3751)
void UpdateEventCameraOriginEntry();
}  // namespace VS::Event
