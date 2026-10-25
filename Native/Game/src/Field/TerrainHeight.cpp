// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:663-673,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Field/TerrainHeight.h"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/FrameUpdate.h"
}

namespace {
constexpr int kTileShift = 7;
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;
constexpr int kHeightShift = 6;
constexpr unsigned kLayerMask = 0x1C00;
constexpr int kLayerShift = 3;
constexpr unsigned kHeightMask = 0x7FFF;
constexpr unsigned kHeightSign = 0x4000;
constexpr int kHeightModulus = 0x8000;
constexpr unsigned kZeroGroundFlag = 0x2000;
constexpr int kSegmentSamples = 16;
constexpr int kSegmentStepShift = 4;

unsigned TerrainCell(int x, int z) {
  const auto* frame = Battle_Ai_FrameState();
  if (frame->heightMap == nullptr || x < 0 || z < 0 || x >= kAiFrameGridSide ||
      z >= kAiFrameGridSide) {
    Core_Debugf("ai", "Terrain height requires a grid and valid tile (%d,%d)",
                x, z);
    std::abort();
  }
  return frame->heightMap[z][x];
}

int RawHeight(unsigned cell) {
  const auto height =
      std::bit_cast<int16_t>(static_cast<std::uint16_t>(cell << kHeightShift));
  return height - static_cast<int>((cell & kLayerMask) >> kLayerShift);
}

int CheckedCoordinate(std::int64_t value) {
  if (!std::in_range<int>(value)) {
    Core_Debugf("ai", "Terrain segment coordinate overflows source arithmetic");
    std::abort();
  }
  return static_cast<int>(value);
}

int GroundHeight(int raw) {
  const auto value = static_cast<unsigned>(raw) & kHeightMask;
  return (value & kHeightSign) != 0 ? static_cast<int>(value) - kHeightModulus
                                    : static_cast<int>(value);
}
}  // namespace

// decomp: func_800E4320 (src/BATTLE/BATTLE.PRG/7B5DC.s:413).
int Field_GroundHeightFlagsAtTile(int x, int z) {
  return RawHeight(TerrainCell(x, z));
}

// decomp: func_800E42EC (src/BATTLE/BATTLE.PRG/7B5DC.s:387).
int Field_GroundHeightAtTile(int x, int z) {
  return GroundHeight(Field_GroundHeightFlagsAtTile(x, z));
}

int Field_TryGroundHeightAtTile(int x, int z, int* ground_height) {
  if (ground_height == nullptr) {
    Core_Debugf("ai", "Terrain height query requires an output value");
    std::abort();
  }
  const auto* frame = Battle_Ai_FrameState();
  if (frame->heightMap == nullptr || x < 0 || z < 0 || x >= kAiFrameGridSide ||
      z >= kAiFrameGridSide) {
    return 0;
  }
  *ground_height = GroundHeight(Field_GroundHeightFlagsAtTile(x, z));
  return 1;
}

// decomp: func_800E4308 (src/BATTLE/BATTLE.PRG/7B5DC.s:399).
int Field_GroundHeightFlagsAtPackedTile(unsigned int tile) {
  return Field_GroundHeightFlagsAtTile(
      static_cast<int>(tile & kTileMask),
      static_cast<int>((tile >> kTileZShift) & kTileMask));
}

// decomp: func_800E4318 (src/BATTLE/BATTLE.PRG/7B5DC.s:407).
int Field_GroundHeightFlagsAtWorldPosition(int x, int z) {
  return Field_GroundHeightFlagsAtTile(x >> kTileShift, z >> kTileShift);
}

// decomp: func_800E42E0 (src/BATTLE/BATTLE.PRG/7B5DC.s:378).
int Field_GroundHeightAtPackedTile(unsigned int tile) {
  return GroundHeight(Field_GroundHeightFlagsAtPackedTile(tile));
}

// decomp: func_800E42D4 (src/BATTLE/BATTLE.PRG/7B5DC.s:371).
int Field_GroundHeightAtWorldPosition(int x, int z) {
  return GroundHeight(Field_GroundHeightFlagsAtWorldPosition(x, z));
}

// func_800E4478, BATTLE/7B5DC.s:527; C counterpart battle_helpers.c:510.
int Field_IsPositionAboveGround(int x, int y, int z) {
  const auto cell = TerrainCell(x >> kTileShift, z >> kTileShift);
  const int ground =
      (cell & kZeroGroundFlag) != 0 ? 0 : GroundHeight(RawHeight(cell));
  return y < ground;
}

// func_800E44E8, BATTLE/7B5DC.s:563; C counterpart battle_helpers.c:524.
int Field_IsTerrainSegmentBlocked(const SVECTOR* from, const SVECTOR* to,
                                  int from_height, int to_height) {
  if (from == nullptr || to == nullptr) {
    Core_Debugf("ai", "Terrain segment query requires both endpoints");
    std::abort();
  }
  int x = static_cast<std::uint16_t>(from->vx);
  int y = CheckedCoordinate(static_cast<std::int64_t>(from->vy) - from_height);
  int z = from->vz;
  const int end_y =
      CheckedCoordinate(static_cast<std::int64_t>(to->vy) - to_height);
  const int dx = (static_cast<int>(static_cast<std::uint16_t>(to->vx)) - x) >>
                 kSegmentStepShift;
  const int dy = CheckedCoordinate(static_cast<std::int64_t>(end_y) - y) >>
                 kSegmentStepShift;
  const int dz = (static_cast<int>(to->vz) - z) >> kSegmentStepShift;
  for (int sample = 0; sample < kSegmentSamples; ++sample) {
    x += dx;
    y = CheckedCoordinate(static_cast<std::int64_t>(y) + dy);
    z += dz;
    if (Field_IsPositionAboveGround(x, y, z) == 0) return 1;
  }
  return 0;
}
