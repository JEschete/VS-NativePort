// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/CurrentTerrain.hpp"

#include <span>

#include "Field/TerrainHeight.hpp"

extern "C" {
#include "Battle/Ai/FrameUpdate.h"
}

namespace VS::Battle::Ai {
Field::TerrainGrid CurrentTerrain() noexcept {
  const auto* frame = Battle_Ai_FrameState();
  if (frame->heightMap == nullptr) return {};
  return Field::TerrainGrid{
      std::span<const Field::TerrainRow, Field::kTerrainGridSide>(
          frame->heightMap, Field::kTerrainGridSide)};
}
}  // namespace VS::Battle::Ai
