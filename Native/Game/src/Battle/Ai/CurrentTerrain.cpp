// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/CurrentTerrain.hpp"

#include <cstddef>
#include <span>

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
#include "Battle/Ai/FlightMovementLive.hpp"
#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Field/TerrainHeight.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

namespace VS::Battle::Ai {
Field::TerrainGrid CurrentTerrain() noexcept {
  const auto* frame = &LiveFrameContext();
  if (frame->heightMap == nullptr) return {};
  return Field::TerrainGrid{
      std::span<const Field::TerrainRow, Field::kTerrainGridSide>(
          frame->heightMap, Field::kTerrainGridSide)};
}

FlightNeighborhood ActorFlightNeighborhood(Battle_Ai_ActorAiState& state) {
  FlightNeighborhood neighborhood;
  neighborhood.terrain = CurrentTerrain();
  neighborhood.origin = {state.tile.tile.tile_x, state.tile.tile.tile_z};
  const auto* deltas = reinterpret_cast<const VS::Battle::Ai::DirectionDelta*>(
      LiveFrameContext().directions);
  for (std::size_t direction = 0; direction < neighborhood.steps.size();
       ++direction) {
    neighborhood.steps[direction] = {deltas[direction].dx,
                                     deltas[direction].dz};
  }
  // The flight test caches its answers in the room's AI frame.
  auto* actor = &state;
  neighborhood.can_fly = [actor](int direction) {
    return CanLiveActorFlyInDirection(actor, direction) != 0;
  };
  // std::function keeps its small target inline; NRVO or the move rebinds it.
  // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
  return neighborhood;
}
}  // namespace VS::Battle::Ai
