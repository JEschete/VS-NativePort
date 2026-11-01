#include "Core/Math/Distance.hpp"

#include <bit>
#include <cstdint>

namespace VS::Core::Math {
namespace {
// GTE IR registers hold signed halfwords: only the low 16 bits of a delta
// enter the square.
std::int32_t GteComponent(std::uint32_t bits) noexcept {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
}

std::uint32_t Square(std::int32_t component) noexcept {
  const auto magnitude =
      static_cast<std::uint32_t>(component < 0 ? -component : component);
  return magnitude * magnitude;
}
}  // namespace

// decomp: BATTLE.PRG func_800E4660 (src/BATTLE/BATTLE.PRG/7BDD4.s:68).
// mtc2 IR1-IR3, SQR 0, then LIR sums of MAC1-MAC3 with a wrapping addu/add.
std::uint32_t SquaredVectorLength(std::int32_t delta_x, std::int32_t delta_y,
                                  std::int32_t delta_z) noexcept {
  return Square(GteComponent(static_cast<std::uint32_t>(delta_x))) +
         Square(GteComponent(static_cast<std::uint32_t>(delta_y))) +
         Square(GteComponent(static_cast<std::uint32_t>(delta_z)));
}

// decomp: BATTLE.PRG func_800E4624 (src/BATTLE/BATTLE.PRG/7BDD4.s:49).
// Loads both short vectors, raises y by each offset and falls through to the
// shared square; the original word loads become decoded Position3 values.
std::int32_t SquaredDistanceWithHeightOffsets(
    Position3 source, Position3 target, std::int32_t source_height,
    std::int32_t target_height) noexcept {
  const auto raised = [](std::int16_t y, std::int32_t height) {
    return static_cast<std::uint32_t>(y) + static_cast<std::uint32_t>(height);
  };
  const auto delta = [](std::uint32_t first, std::uint32_t second) {
    return std::bit_cast<std::int32_t>(first - second);
  };
  return std::bit_cast<std::int32_t>(SquaredVectorLength(
      delta(static_cast<std::uint32_t>(source.x),
            static_cast<std::uint32_t>(target.x)),
      delta(raised(source.y, source_height), raised(target.y, target_height)),
      delta(static_cast<std::uint32_t>(source.z),
            static_cast<std::uint32_t>(target.z))));
}

// decomp: BATTLE.PRG func_800E4604 (src/BATTLE/BATTLE.PRG/7BDD4.s:35).
// The assembly entry took the first tile's components in t0/t1; they are now
// explicit parameters and the second tile is decoded by the caller.
std::uint32_t SquaredTileDistanceFromComponents(
    std::int32_t first_x, std::int32_t first_z,
    TileCoordinate second) noexcept {
  const auto delta_x = static_cast<std::uint32_t>(first_x) - second.x;
  const auto delta_z = static_cast<std::uint32_t>(first_z) - second.z;
  return SquaredVectorLength(GteComponent(delta_x), 0, GteComponent(delta_z));
}

// decomp: BATTLE.PRG func_800E45F4 (src/BATTLE/BATTLE.PRG/7BDD4.s:23).
// C counterpart: rood-boi pc/src/battle_movement.c:153 (CC0-1.0).
// Unpacks the first tile into t0/t1 and falls through to the component entry.
std::uint32_t SquaredTileDistance(TileCoordinate first,
                                  TileCoordinate second) noexcept {
  return SquaredTileDistanceFromComponents(first.x, first.z, second);
}
}  // namespace VS::Core::Math
