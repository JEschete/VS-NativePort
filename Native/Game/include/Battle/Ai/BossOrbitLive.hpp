#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct ActorAiState;
struct OrbitTrackingView;
struct RadialRetreatView;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state, the arena orbit (D_800F5920 and the two
// orbit globals), the player's record, the battle actor and the movement code
// to the typed orbiting boss (BossOrbit.cpp). The views are the actor's AI
// state. Each aborts with a diagnostic when the typed routine refuses its
// input; the original read through the missing pointer.
namespace VS::Battle::Ai {
// func_800E7698.
int UpdateLiveBossRadialRetreat(VS::Battle::Ai::RadialRetreatView* state);

// func_800E78F4.
int IsLiveBossActionReady(VS::Battle::Ai::ActorAiState* state);

// func_800E7960.
void UpdateLiveBossOrbitTracking(VS::Battle::Ai::OrbitTrackingView* state);

// func_800E7F8C: the boss's decision callback; always 1.
int UpdateLiveOrbitingBossBehavior(VS::Battle::Ai::ActorAiState* state);
}  // namespace VS::Battle::Ai
