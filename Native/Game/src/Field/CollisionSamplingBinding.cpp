// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <span>

#include "Field/CollisionSampling.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

std::span<const std::byte> VS::Field::LiveRoomCollisionMap() {
  if (Field_roomData.collisionMap == nullptr) return {};
  return {reinterpret_cast<const std::byte*>(Field_roomData.collisionMap),
          static_cast<std::size_t>(Field_roomData.header.collisionMapLength)};
}
