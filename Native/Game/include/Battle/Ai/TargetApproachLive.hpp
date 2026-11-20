#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct MovementDecisionView;
struct TargetApproachView;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the actor models, the battle actor
// list and the movement code to the typed target approach
// (TargetApproach.cpp). The views are the actor's AI state. Each aborts with a
// diagnostic when the typed routine refuses its input; the original read past
// the actor tables.
namespace VS::Battle::Ai {
// func_800E24EC.
int UpdateLiveTargetApproachMovement(VS::Battle::Ai::TargetApproachView* state,
                                     int option);

// func_800E2B2C.
int UpdateLiveTargetFacing(VS::Battle::Ai::MovementDecisionView* state,
                           int target);
}  // namespace VS::Battle::Ai
