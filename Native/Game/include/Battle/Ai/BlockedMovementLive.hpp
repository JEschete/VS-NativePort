#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_Ai_BlockedRecoveryView;
struct Battle_Ai_MoveTargetView;
struct Battle_Ai_PerceptionUpdateView;

// Bindings from the actor AI state table, the room's AI frame and the
// movement code to the typed blocked movement (BlockedMovement.cpp). The views
// are the actor's AI state. Each aborts with a diagnostic when the typed
// routine refuses its input; the original read past the frame's direction
// codes.
namespace VS::Battle::Ai {
// func_800D8400.
unsigned int FindLiveAdjacentMovementDestination(
    Battle_Ai_MoveTargetView* state, int first, int second,
    unsigned int direction);

// historyStalled, inlined in func_800D8EF4.
int IsLiveMovementHistoryStalled(Battle_Ai_PerceptionUpdateView* state);

// func_800D9538.
void SaveLiveMovementRandomState();

// func_800DB0BC.
int TryRecoverLiveBlockedMovement(Battle_Ai_BlockedRecoveryView* state);
}  // namespace VS::Battle::Ai
