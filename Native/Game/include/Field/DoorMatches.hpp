#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Transition door lookup by tile. Doors are passed as their tile locations and
// matches come back as indices, so the query reads no room global or PS1
// address.
namespace VS::Field {
// Floor layers from this one up are moving-platform surfaces.
inline constexpr int kFirstPlatformLayer = 2;

struct DoorTile {
  std::uint8_t x = 0;
  std::uint8_t z = 0;
  std::uint8_t layer = 0;
};

// Replaces `matches` with the indices, in order, of the doors at tile (x, z)
// on `layer`, or of every door on `layer` when it is a platform surface
// (func_8008BAC8).
void TransitionDoorsAtTile(std::span<const DoorTile> doors, int x, int z,
                           int layer, std::vector<std::size_t>& matches);
}  // namespace VS::Field
