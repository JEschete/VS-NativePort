#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/Navigation.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the room's AI frame, the battle
// actor list and the movement code to the typed radial movement
// (RadialMovement.cpp). Each aborts with a diagnostic when the typed routine
// refuses its input; the original read past the room's maps or the actor
// list.
namespace VS::Battle::Ai {
// func_800E1388.
void ProjectLiveClearRadialDestination(VS::Battle::Ai::ActorAiState* state,
                                       VS::Battle::Ai::TilePosition source,
                                       int angle, unsigned int distance);

// func_800E153C. The angle is unused, as in the original.
void MoveLiveToRadialDestination(VS::Battle::Ai::ActorAiState* state,
                                 int unused);

// func_800E1764.
void MoveLiveAwayFromReference(VS::Battle::Ai::ActorAiState* state, int angle);

// func_800E2CCC.
void RecoverLiveMovement(VS::Battle::Ai::ActorAiState* state);
}  // namespace VS::Battle::Ai
