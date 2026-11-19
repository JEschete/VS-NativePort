#pragma once
// The saved enemy states of the four most recently visited rooms.
#include <cstdint>

#include "State/RoomCache.hpp"

namespace VS::State {
// decomp: D_80060064 (D_80060064@SLUS_010.40)
// The recentRoomEnemyStates entry for the current room.
extern std::int32_t currentRoomStateSlot;
// decomp: D_80061078 (D_80061078@SLUS_010.40)
extern RoomEnemyStates recentRoomEnemyStates[kRecentRoomCount];
}  // namespace VS::State
