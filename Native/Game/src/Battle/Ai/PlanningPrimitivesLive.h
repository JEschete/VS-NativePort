#pragma once

struct Battle_Ai_CandidateReadinessView;
struct Battle_Ai_ActionTiming;
#ifdef __cplusplus
extern "C" {
#endif
int Battle_Ai_RefreshActionCandidateReadinessEntry(
    struct Battle_Ai_CandidateReadinessView* state);
void Battle_Ai_ApplySimulatedActionResultsEntry(
    struct Battle_Ai_ActionTiming* rows);
#ifdef __cplusplus
}
#endif
