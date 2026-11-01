#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/FlightTerrain.hpp"
#include "Field/TerrainHeight.hpp"

struct Battle_Ai_ActorAiState;

// Bindings from the battle AI frame to the typed terrain and flight queries.
namespace VS::Battle::Ai {
// Terrain grid of the room the AI frame is updating; empty when the room has
// none.
[[nodiscard]] Field::TerrainGrid CurrentTerrain() noexcept;

// The frame's terrain, direction steps and cached flight test around an
// actor; valid while the actor's AI state and the frame are unchanged.
[[nodiscard]] FlightNeighborhood ActorFlightNeighborhood(
    Battle_Ai_ActorAiState& state);
}  // namespace VS::Battle::Ai
