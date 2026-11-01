#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "Battle/Ai/RoomAiMap.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/TerrainHeight.hpp"

// Ground and ceiling limits for flying AI. Every query takes the terrain grid,
// tiles, direction steps and room runtime it reads, so nothing reaches the AI
// frame globals, a PS1 scratchpad table or an actor-state view cast.
namespace VS::Battle::Ai {
inline constexpr int kFlightDirectionCount = 8;
// Samples taken along a segment by MinimumAiGroundHeightAlongSegment.
inline constexpr int kGroundSegmentSampleCount = 16;

// One tile step in a compass direction (each component -1, 0 or 1).
struct DirectionStep {
  int dx = 0;
  int dz = 0;
};

using FlightDirectionSteps = std::array<DirectionStep, kFlightDirectionCount>;

// Whether the actor can fly one tile in a direction (0-7). The battle test
// caches its answers, so it is called once per direction, in order.
using FlightDirectionTest = std::function<bool(int direction)>;

// The tiles around a flying actor.
struct FlightNeighborhood {
  Field::TerrainGrid terrain;
  Core::Math::TileCoordinate origin{};
  FlightDirectionSteps steps{};
  FlightDirectionTest canFly;
};

// Lowest ground height (never above 0) among the flyable neighbours in the
// movement direction: one cardinal direction, or three when the 12-bit angle
// is between cardinals (func_800E0850).
[[nodiscard]] int LowestReachableGroundHeight(
    const FlightNeighborhood& neighborhood, std::uint32_t movement_angle);

// Lowest ground height (never above 0) of 16 samples stepping from the start
// toward the end world position, the start itself excluded (func_800E6A6C).
[[nodiscard]] int MinimumAiGroundHeightAlongSegment(Field::TerrainGrid terrain,
                                                    int start_x, int start_z,
                                                    int end_x, int end_z);

// The published room's flight ceiling, or 0 when no room AI map is published
// or the room has none (func_800E75EC).
[[nodiscard]] int RoomNavigationCeilingHeight(const RoomAiRuntime& runtime);
}  // namespace VS::Battle::Ai
