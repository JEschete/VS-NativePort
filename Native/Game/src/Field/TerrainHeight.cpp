// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:663-673,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Field/TerrainHeight.h"

#include <bit>
#include <cstdint>
#include <cstdlib>

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

int GroundHeight(int raw) {
  const auto value = static_cast<unsigned>(raw) & kHeightMask;
  return (value & kHeightSign) != 0 ? static_cast<int>(value) - kHeightModulus
                                    : static_cast<int>(value);
}
}  // namespace

// decomp: func_800E4320 (src/BATTLE/BATTLE.PRG/7B5DC.s:413).
int Field_GroundHeightFlagsAtTile(int x, int z) {
  const auto* frame = Battle_Ai_FrameState();
  if (frame->heightMap == nullptr || x < 0 || z < 0 || x >= kAiFrameGridSide ||
      z >= kAiFrameGridSide) {
    Core_Debugf("ai", "Terrain height requires a grid and valid tile (%d,%d)",
                x, z);
    std::abort();
  }
  const auto cell = frame->heightMap[z][x];
  const auto height = std::bit_cast<int16_t>(
      static_cast<std::uint16_t>(static_cast<unsigned>(cell) << kHeightShift));
  return height - static_cast<int>((cell & kLayerMask) >> kLayerShift);
}

// decomp: func_800E42EC (src/BATTLE/BATTLE.PRG/7B5DC.s:387).
int Field_GroundHeightAtTile(int x, int z) {
  return GroundHeight(Field_GroundHeightFlagsAtTile(x, z));
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
