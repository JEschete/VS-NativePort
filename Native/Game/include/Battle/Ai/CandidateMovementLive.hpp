#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle::Ai {
struct ActorAiState;
struct ActionCandidateRecord;
struct CandidateMovementView;
struct CandidateEntryView;
struct EscortView;

int UpdateLiveEscortCombatBehavior(VS::Battle::Ai::EscortView* state);
int MoveLiveForActionCandidate(VS::Battle::Ai::CandidateMovementView* state,
                               VS::Battle::Ai::CandidateEntryView* candidate);
int UpdateLiveCombatPursuit(VS::Battle::Ai::ActorAiState* state);
int TryApproachLivePlannedTarget(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate);
int UpdateLiveCloseFollowBehavior(VS::Battle::Ai::EscortView* state);
int UpdateLiveEnemyPursuitBehavior(VS::Battle::Ai::EscortView* state);
int UpdateLiveGuardMovementBehavior(VS::Battle::Ai::EscortView* state);
}  // namespace VS::Battle::Ai
