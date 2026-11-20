#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct BlockedRecoveryView;
struct MoveTargetView;
struct PerceptionUpdateView;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the room's AI frame and the
// movement code to the typed blocked movement (BlockedMovement.cpp). The views
// are the actor's AI state. Each aborts with a diagnostic when the typed
// routine refuses its input; the original read past the frame's direction
// codes.
namespace VS::Battle::Ai {
// func_800D8400.
unsigned int FindLiveAdjacentMovementDestination(
    VS::Battle::Ai::MoveTargetView* state, int first, int second,
    unsigned int direction);

// historyStalled, inlined in func_800D8EF4.
int IsLiveMovementHistoryStalled(VS::Battle::Ai::PerceptionUpdateView* state);

// func_800D9538.
void SaveLiveMovementRandomState();

// func_800DB0BC.
int TryRecoverLiveBlockedMovement(VS::Battle::Ai::BlockedRecoveryView* state);
}  // namespace VS::Battle::Ai
