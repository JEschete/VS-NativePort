#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_Ai_ActorAiState;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_CandidateMovementView;
struct Battle_Ai_CandidateEntryView;
struct Battle_Ai_EscortView;

namespace VS::Battle::Ai {
int MoveLiveForActionCandidate(Battle_Ai_CandidateMovementView* state,
                               Battle_Ai_CandidateEntryView* candidate);
int UpdateLiveCombatPursuit(Battle_Ai_ActorAiState* state);
int TryApproachLivePlannedTarget(Battle_Ai_ActorAiState* state,
                                 Battle_Ai_ActionCandidate* candidate);
int UpdateLiveCloseFollowBehavior(Battle_Ai_EscortView* state);
int UpdateLiveEnemyPursuitBehavior(Battle_Ai_EscortView* state);
int UpdateLiveGuardMovementBehavior(Battle_Ai_EscortView* state);
}  // namespace VS::Battle::Ai
