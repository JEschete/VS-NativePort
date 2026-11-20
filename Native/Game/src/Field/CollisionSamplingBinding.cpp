// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <span>

#include "Field/CollisionSampling.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"

std::span<const std::byte> VS::Field::LiveRoomCollisionMap() {
  if (VS::Field::roomData.collision_map == nullptr) return {};
  return {reinterpret_cast<const std::byte*>(VS::Field::roomData.collision_map),
          static_cast<std::size_t>(
              VS::Field::roomData.header.collision_map_length)};
}
