#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct ActorAiState;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the room's AI frame, the battle
// actor list, the moving platforms and the movement code to the typed flight
// movement (FlightMovement.cpp). Each aborts with a diagnostic when the typed
// routine refuses its input; the original read past the room's maps, the
// actor table or a missing model.
namespace VS::Battle::Ai {
// func_800E02B4. Returns 0, 1 for a moving platform or a blocking actor's
// ID + 1.
int IsLiveFlightDirectionObstructed(VS::Battle::Ai::ActorAiState* state,
                                    int direction, int ahead);

// func_800E0678.
int CanLiveActorFlyInDirection(VS::Battle::Ai::ActorAiState* state,
                               int direction);

// func_800E0918. A nonzero mode moves toward the destination tile.
int UpdateLiveFlightGroundTransition(VS::Battle::Ai::ActorAiState* state,
                                     int angle, int mode);

// func_800E0A68.
void UpdateLiveFlightMovement(VS::Battle::Ai::ActorAiState* state, int angle);

// func_800E1238.
void UpdateLiveFlightDirectionalMovement(VS::Battle::Ai::ActorAiState* state,
                                         int angle);
}  // namespace VS::Battle::Ai
