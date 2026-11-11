#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_Ai_PerceptionView;

// Bindings from the AI state tables, the battle actor list, the actor
// records, the gimmicks, the AI frame context and the room AI state to the
// typed perception (ActorPerception.cpp). The views are the actor's AI state.
// Each aborts with a diagnostic when the typed routine refuses its input; the
// original read through a missing pointer or past a table there.
namespace VS::Battle::Ai {
// func_800D87E8.
void UpdateLiveTargetPerception(Battle_Ai_PerceptionView* state, int target);

// func_800D8EF4: the frame's perception pass over the battle actor list.
void UpdateLiveActorPerception();
}  // namespace VS::Battle::Ai
