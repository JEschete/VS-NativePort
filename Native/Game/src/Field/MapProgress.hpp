#pragma once
// The map completion mask. MapProgress.cpp defines it.

#include <cstddef>

#include "Field/Map.hpp"

namespace VS::Field {
// The original's 16 mask words, as the module image fills them.
inline constexpr std::size_t kCountableRoomMaskBytes = 0x40;
static_assert(sizeof(RoomVisitFlags) == kCountableRoomMaskBytes);

// The locations map completion counts, one bit per location id.
// decomp: vs_battle_mapCompletionFlags (mpd.h:254,
// vs_battle_mapCompletionFlags@BATTLE/BATTLE.PRG)
extern RoomVisitFlags countableRoomMask;
}  // namespace VS::Field
