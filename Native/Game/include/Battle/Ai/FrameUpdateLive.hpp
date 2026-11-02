#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/FrameUpdate.hpp"

struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_AiCreationView;

// Binding from the battle AI tables and the frame context (FrameUpdate.h) to
// the typed frame update. The signatures keep the module-image entries'
// calling shape, so relocated BATTLE.PRG addresses resolve to these.
namespace VS::Battle::Ai {
// Caches the actor and room tables into the frame context; returns the packed
// room bounds. A room whose auxiliary section cannot hold the terrain region
// header aborts with a diagnostic.
int CacheLiveActorAiWorkingState();
// Runs the frame's AI stages; a frame with a room map but no room state or
// linked actor list aborts with a diagnostic.
void RunLiveActorAiUpdateStages();
void TickLiveActorDecisionTimers(Battle_Ai_PerceptionUpdateView* view);
void InitializeLiveActorLocomotionMode(Battle_Ai_AiCreationView* view);
// An actor ID outside the AI table aborts; an empty slot is left alone.
void SetLiveActorVerticalMovementSteps(int actor_id, int walk_step,
                                       int run_step);
}  // namespace VS::Battle::Ai
