// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/CurrentTerrain.hpp"

#include <cstddef>
#include <span>

#include "Battle/Ai/FlightTerrain.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"
}

namespace VS::Battle::Ai {
Field::TerrainGrid CurrentTerrain() noexcept {
  const auto* frame = Battle_Ai_FrameState();
  if (frame->heightMap == nullptr) return {};
  return Field::TerrainGrid{
      std::span<const Field::TerrainRow, Field::kTerrainGridSide>(
          frame->heightMap, Field::kTerrainGridSide)};
}

FlightNeighborhood ActorFlightNeighborhood(Battle_Ai_ActorAiState& state) {
  FlightNeighborhood neighborhood;
  neighborhood.terrain = CurrentTerrain();
  neighborhood.origin = {state.tile.tile.tileX, state.tile.tile.tileZ};
  const auto* deltas = reinterpret_cast<const Battle_Ai_DirectionDelta*>(
      Battle_Ai_FrameState()->directions);
  for (std::size_t direction = 0; direction < neighborhood.steps.size();
       ++direction) {
    neighborhood.steps[direction] = {deltas[direction].dx,
                                     deltas[direction].dz};
  }
  // The flight test caches its answers in the actor's AI state.
  auto* cache = reinterpret_cast<Battle_Ai_DirectionCacheView*>(&state);
  neighborhood.canFly = [cache](int direction) {
    return Battle_Ai_CanFlyInDirection(cache, direction) != 0;
  };
  return neighborhood;
}
}  // namespace VS::Battle::Ai
