#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>

#include "Field/RoomState.hpp"

// Binding from the location table (Field_locationDestinations), the visit
// records (State_roomVisitProgress and Field_isRoomVisitPending), the loaded
// room (Field_roomData) and the session's room globals to the typed room
// bookkeeping rules (RoomState.hpp). Each adapter keeps the original call
// shape for the generated callers (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Field {
// The location table as zone and room pairs.
[[nodiscard]] std::array<LocationDestination, kLocationCount>
LiveLocationTable();

// decomp: BATTLE.PRG _setRoomSeen (src/BATTLE/BATTLE.PRG/146C.c:3205)
void LiveMarkCurrentRoomVisited();
// decomp: BATTLE.PRG func_8007C4D4 (src/BATTLE/BATTLE.PRG/146C.c:8803)
void LiveCancelPendingRoomVisit();
// decomp: BATTLE.PRG _getLocationId (src/BATTLE/BATTLE.PRG/146C.c:8899)
int LiveFindLocationId(int zone_id, int room_id);
// decomp: BATTLE.PRG vs_battle_getCurrentRoomId
// (src/BATTLE/BATTLE.PRG/146C.c:8912)
int LiveCurrentRoomId();
// decomp: BATTLE.PRG func_8008AB80 (src/BATTLE/BATTLE.PRG/146C.c:15756)
int LiveEnableOwnedRoomRendering(int owner_room_index);
// decomp: BATTLE.PRG func_8008ABB8 (src/BATTLE/BATTLE.PRG/146C.c:15765)
int LiveDisableOwnedRoomRendering(int owner_room_index);
// decomp: BATTLE.PRG func_8007BF2C (src/BATTLE/BATTLE.PRG/146C.c:8619)
void LiveShowPrimaryRoomGeometry();
// decomp: BATTLE.PRG func_8007BF4C (src/BATTLE/BATTLE.PRG/146C.c:8621)
void LiveHidePrimaryRoomGeometry();
// decomp: INITBTL.PRG func_800FA6B8 (src/BATTLE/INITBTL.PRG/18.c:595)
void LiveInitializeRoomRuntimeState();
}  // namespace VS::Field
