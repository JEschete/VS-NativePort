#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>

#include "Field/RoomState.hpp"

// Binding from the location table (VS::Field::locationDestinations), the visit
// records (VS::State::roomVisitProgress and VS::Field::isRoomVisitPending), the
// loaded room (VS::Field::roomData) and the session's room globals to the typed
// room bookkeeping rules (RoomState.hpp). Each adapter keeps the original call
// shape for the generated callers (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Field {
// The location table as zone and room pairs.
[[nodiscard]] std::array<LocationDestination, kLocationCount>
LiveLocationTable();

// decomp: BATTLE.PRG setRoomSeen (src/BATTLE/BATTLE.PRG/146C.c:2908; was
// _setRoomSeen)
void LiveMarkCurrentRoomVisited();
// decomp: BATTLE.PRG func_8007C4D4 (src/BATTLE/BATTLE.PRG/146C.c:8760)
void LiveCancelPendingRoomVisit();
// decomp: BATTLE.PRG getLocationId (src/BATTLE/BATTLE.PRG/146C.c:8854; was
// _getLocationId)
int LiveFindLocationId(int zone_id, int room_id);
// decomp: BATTLE.PRG vs_battle_getCurrentRoomId
// (src/BATTLE/BATTLE.PRG/146C.c:8867)
int LiveCurrentRoomId();
// decomp: BATTLE.PRG func_8008AB80 (src/BATTLE/BATTLE.PRG/146C.c:15626)
int LiveEnableOwnedRoomRendering(int owner_room_index);
// decomp: BATTLE.PRG func_8008ABB8 (src/BATTLE/BATTLE.PRG/146C.c:15635)
int LiveDisableOwnedRoomRendering(int owner_room_index);
// decomp: BATTLE.PRG func_8007BF2C (src/BATTLE/BATTLE.PRG/146C.c:8562)
void LiveShowPrimaryRoomGeometry();
// decomp: BATTLE.PRG func_8007BF4C (src/BATTLE/BATTLE.PRG/146C.c:8564)
void LiveHidePrimaryRoomGeometry();
// decomp: INITBTL.PRG func_800FA6B8 (src/BATTLE/INITBTL.PRG/18.c:609)
void LiveInitializeRoomRuntimeState();
}  // namespace VS::Field
