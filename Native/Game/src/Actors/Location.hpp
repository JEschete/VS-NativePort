#pragma once
// An actor's map location: the tile it stands on (packed into one word) and its
// world position, plus the query record that names the actor.

#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"

namespace VS::Actors {

// decomp: vs_TilePosition (src/include/tilePosition.h:4); the last byte is
// the facing in a placement.
struct TileTarget {
  std::uint32_t tile_x : 8;
  std::uint32_t tile_layer : 8;
  std::uint32_t tile_z : 8;
  std::uint32_t facing : 8;
};
inline constexpr std::size_t kTileTargetBytes = 0x4;
static_assert(sizeof(TileTarget) == kTileTargetBytes,
              "TileTarget must keep its PS1 layout");

// The original handles a tile as one 32-bit word too: -1 clears it and the word
// compares whole (VS_TILE_CLEAR / VS_TILE_EQ, tilePosition.h:18-29).
union PackedTileTarget {
  TileTarget target;
  std::int32_t word;
};
inline constexpr std::size_t kPackedTileTargetBytes = 0x4;
static_assert(sizeof(PackedTileTarget) == kPackedTileTargetBytes,
              "PackedTileTarget must keep its PS1 layout");

// decomp: ActorLocation (src/BATTLE/BATTLE.PRG/actorPosition.h:6)
struct ActorLocation {
  PackedTileTarget packed_tile;
  SVECTOR position;
};
inline constexpr std::size_t kActorLocationBytes = 0xC;
static_assert(sizeof(ActorLocation) == kActorLocationBytes,
              "ActorLocation must keep its PS1 layout");

// decomp: vs_ActorPosition (src/BATTLE/BATTLE.PRG/actorPosition.h:11)
struct ActorLocationQuery {
  ActorLocation location;
  std::int32_t actor_index;
};
inline constexpr std::size_t kActorLocationQueryBytes = 0x10;
static_assert(sizeof(ActorLocationQuery) == kActorLocationQueryBytes,
              "ActorLocationQuery must keep its PS1 layout");

}  // namespace VS::Actors
