#pragma once
// The enemy states saved for each recently visited room.
#include <cstddef>
#include <cstdint>

namespace VS::State {
inline constexpr std::size_t kRecentRoomCount = 4;
inline constexpr std::size_t kRoomActorStateSlots = 8;
// One VS::Actors::RoomActorRecord.
inline constexpr std::size_t kRoomActorStateBytes = 0x28;
inline constexpr std::size_t kRoomEnemyStatesBytes = 0x148;

// decomp: D_80061078_t (src/SLUS_010.40/main.h:412)
// One room's saved enemies: how recently it was visited, its zone and map, and
// the next actor-state slot to fill.
struct RoomEnemyStates {
  /* 0x000 */ std::uint8_t recency;
  /* 0x001 */ std::uint8_t zone_id;
  /* 0x002 */ std::uint16_t map_id;
  /* 0x004 */ std::uint32_t next_slot;
  /* 0x008 */ std::uint8_t actor_states[kRoomActorStateSlots]
                                       [kRoomActorStateBytes];
};
static_assert(sizeof(RoomEnemyStates) == kRoomEnemyStatesBytes);
}  // namespace VS::State
