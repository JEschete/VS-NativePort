#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct PerceptionView;
}  // namespace VS::Battle::Ai

// Bindings from the AI state tables, the battle actor list, the actor
// records, the gimmicks, the AI frame context and the room AI state to the
// typed perception (ActorPerception.cpp). The views are the actor's AI state.
// Each aborts with a diagnostic when the typed routine refuses its input; the
// original read through a missing pointer or past a table there.
namespace VS::Battle::Ai {
// func_800D87E8.
void UpdateLiveTargetPerception(VS::Battle::Ai::PerceptionView* state,
                                int target);

// func_800D8EF4: the frame's perception pass over the battle actor list.
void UpdateLiveActorPerception();
}  // namespace VS::Battle::Ai
