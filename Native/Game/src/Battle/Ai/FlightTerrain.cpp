// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/FlightTerrain.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/RoomAiMap.hpp"
#include "Field/TerrainHeight.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint32_t kAngleFractionMask = 0x3FF;
constexpr int kDirectionShift = 9;
constexpr std::uint32_t kCardinalDirectionMask = 6;
constexpr int kDiagonalDirectionCount = 3;
constexpr int kDirectionMask = kFlightDirectionCount - 1;
constexpr int kSegmentSampleShift = 4;
}  // namespace

// decomp: BATTLE.PRG func_800E0850 (src/BATTLE/BATTLE.PRG/771A8.c:942).
int LowestReachableGroundHeight(const FlightNeighborhood& neighborhood,
                                std::uint32_t movement_angle) {
  int minimum_height = 0;
  const int direction_count =
      (movement_angle & kAngleFractionMask) != 0 ? kDiagonalDirectionCount : 1;
  int direction = static_cast<int>((movement_angle >> kDirectionShift) &
                                   kCardinalDirectionMask);
  for (int checked = 0; checked < direction_count; ++checked) {
    if (neighborhood.can_fly && neighborhood.can_fly(direction)) {
      const DirectionStep step =
          neighborhood.steps[static_cast<std::size_t>(direction)];
      const int ground_height = Field::GroundHeightAtTile(
          neighborhood.terrain, neighborhood.origin.x + step.dx,
          neighborhood.origin.z + step.dz);
      if (ground_height < minimum_height) minimum_height = ground_height;
    }
    direction = (direction + 1) & kDirectionMask;
  }
  return minimum_height;
}

// decomp: BATTLE.PRG func_800E6A6C (src/BATTLE/BATTLE.PRG/7E05C.s:187). Each
// sample keeps the low 15 bits of func_800E4318's value (sll/sra 17), the
// narrowing GroundHeightAtWorldPosition applies (#343).
int MinimumAiGroundHeightAlongSegment(Field::TerrainGrid terrain, int start_x,
                                      int start_z, int end_x, int end_z) {
  const int step_x = (end_x - start_x) >> kSegmentSampleShift;
  const int step_z = (end_z - start_z) >> kSegmentSampleShift;
  int minimum_height = 0;
  int x = start_x;
  int z = start_z;
  for (int sample = 0; sample < kGroundSegmentSampleCount; ++sample) {
    x += step_x;
    z += step_z;
    const int ground_height = Field::GroundHeightAtWorldPosition(terrain, x, z);
    if (ground_height < minimum_height) minimum_height = ground_height;
  }
  return minimum_height;
}

// decomp: BATTLE.PRG func_800E75EC (src/BATTLE/BATTLE.PRG/7E05C.s:1041).
int RoomNavigationCeilingHeight(const RoomAiRuntime& runtime) {
  const std::optional<RoomAiSettings> settings = runtime.PublishedSettings();
  return settings ? settings->flight_ceiling : 0;
}
}  // namespace VS::Battle::Ai
