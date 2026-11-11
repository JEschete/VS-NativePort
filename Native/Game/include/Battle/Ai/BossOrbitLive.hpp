#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_Ai_ActorAiState;
struct Battle_Ai_OrbitTrackingView;
struct Battle_Ai_RadialRetreatView;

// Bindings from the actor AI state, the arena orbit (D_800F5920 and the two
// orbit globals), the player's record, the battle actor and the movement code
// to the typed orbiting boss (BossOrbit.cpp). The views are the actor's AI
// state. Each aborts with a diagnostic when the typed routine refuses its
// input; the original read through the missing pointer.
namespace VS::Battle::Ai {
// func_800E7698.
int UpdateLiveBossRadialRetreat(Battle_Ai_RadialRetreatView* state);

// func_800E78F4.
int IsLiveBossActionReady(Battle_Ai_ActorAiState* state);

// func_800E7960.
void UpdateLiveBossOrbitTracking(Battle_Ai_OrbitTrackingView* state);

// func_800E7F8C: the boss's decision callback; always 1.
int UpdateLiveOrbitingBossBehavior(Battle_Ai_ActorAiState* state);
}  // namespace VS::Battle::Ai
