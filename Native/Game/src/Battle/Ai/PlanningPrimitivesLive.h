#pragma once

struct Battle_Ai_CandidateReadinessView;
struct Battle_Ai_ActionTiming;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_ActionCandidate;
struct Battle_ActionDefinition;
#ifdef __cplusplus
extern "C" {
#endif
int Battle_Ai_RefreshActionCandidateReadinessEntry(
    struct Battle_Ai_CandidateReadinessView* state);
void Battle_Ai_ApplySimulatedActionResultsEntry(
    struct Battle_Ai_ActionTiming* rows);
void Battle_Ai_SimulateActionCandidateEntry(
    struct Battle_Ai_ActorAiState* state, struct Battle_Ai_ActionTiming* rows,
    struct Battle_Ai_ActionCandidate* candidate, int mode);
void Battle_Ai_UpdateCandidateHitWeightEntry(
    struct Battle_Ai_ActionCandidate* candidate);
void Battle_Ai_EvaluateActionCandidateEntry(
    struct Battle_Ai_ActorAiState* state,
    struct Battle_Ai_ActionCandidate* candidate, int mode,
    struct Battle_ActionDefinition* action);
#ifdef __cplusplus
}
#endif
