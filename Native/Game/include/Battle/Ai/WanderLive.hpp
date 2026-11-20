#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/Wander.hpp"

namespace VS::Battle::Ai {
struct WanderView;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the room's AI frame and the
// movement code to the typed wander (Wander.cpp). The views are the actor's
// AI state. Each aborts with a diagnostic when the typed routine refuses its
// input; the original read past the room's maps.
namespace VS::Battle::Ai {
// func_800E1BB8. The fourth argument is unused, as in the original.
int ChooseNextLiveWanderTile(VS::Battle::Ai::WanderView* state,
                             unsigned int tile, unsigned int range, int unused);

// func_800E1EAC.
void UpdateLiveWanderMovement(VS::Battle::Ai::WanderView* state,
                              unsigned int tile, unsigned int range);
}  // namespace VS::Battle::Ai
