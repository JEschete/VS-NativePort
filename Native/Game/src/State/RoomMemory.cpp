// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "State/RoomMemory.hpp"

#include <cstdint>

// decomp: D_80060064 (D_80060064@SLUS_010.40)
std::int32_t VS::State::currentRoomStateSlot;

// decomp: D_80061078 (D_80061078@SLUS_010.40)
VS::State::RoomEnemyStates
    VS::State::recentRoomEnemyStates[VS::State::kRecentRoomCount];
