#pragma once

#include <cstdint>

// Squared distances used by battle AI ranges, perception and pathing. Values
// keep the original GTE precision so existing ranges behave identically, but
// callers pass decoded positions and tiles instead of raw PS1 words or
// register state, and nothing is reachable through a module address.
namespace VS::Core::Math {
// A world-space point at the short-vector precision AI positions use.
struct Position3 {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};
};

// A room tile cell.
struct TileCoordinate {
  std::uint8_t x{};
  std::uint8_t z{};
};

// A record with vx/vy/vz members, such as an SVECTOR.
template <typename Vector>
concept VectorRecord = requires(const Vector& vector) {
  vector.vx;
  vector.vy;
  vector.vz;
};

// Reads a point from a vector record so callers never alias it as raw
// halfwords. Members wider than a halfword are rejected at compile time by the
// braced initialization.
template <VectorRecord Vector>
[[nodiscard]] constexpr Position3 PositionOf(const Vector& vector) noexcept {
  return Position3{vector.vx, vector.vy, vector.vz};
}

// Reads a point stored as three consecutive halfwords (x, y, z).
[[nodiscard]] constexpr Position3 PositionOf(
    const std::int16_t (&components)[3]) noexcept {
  return Position3{components[0], components[1], components[2]};
}

// Decodes a packed AI tile word: x in bits 0-7, z in bits 16-23. The other
// bits carry per-actor metadata that never takes part in a distance.
[[nodiscard]] constexpr TileCoordinate UnpackTileCoordinate(
    std::uint32_t packed) noexcept {
  constexpr std::uint32_t kByte = 0xFF;
  constexpr int kZShift = 16;
  return {static_cast<std::uint8_t>(packed & kByte),
          static_cast<std::uint8_t>((packed >> kZShift) & kByte)};
}

// Sum of squared deltas. Each delta is reduced to its low 16 bits as a signed
// value (the GTE IR inputs) and the 32-bit sum wraps, so the result is exact
// for deltas within +/-32767.
[[nodiscard]] std::uint32_t SquaredVectorLength(std::int32_t delta_x,
                                                std::int32_t delta_y,
                                                std::int32_t delta_z) noexcept;

// Squared distance between two points after raising each by its own vertical
// offset. Returns the wrapped sum reinterpreted as signed, as AI ranges
// compare it.
[[nodiscard]] std::int32_t SquaredDistanceWithHeightOffsets(
    Position3 source, Position3 target, std::int32_t source_height,
    std::int32_t target_height) noexcept;

// Squared horizontal distance from already decoded components to a tile.
// Component deltas share SquaredVectorLength's 16-bit reduction.
[[nodiscard]] std::uint32_t SquaredTileDistanceFromComponents(
    std::int32_t first_x, std::int32_t first_z, TileCoordinate second) noexcept;

// Squared horizontal distance between two tiles, at most 2 * 255^2.
[[nodiscard]] std::uint32_t SquaredTileDistance(TileCoordinate first,
                                                TileCoordinate second) noexcept;
}  // namespace VS::Core::Math
