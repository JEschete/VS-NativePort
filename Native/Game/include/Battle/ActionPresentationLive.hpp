#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionRequest;
struct Effects_ActionEffectRequest;

namespace VS::Battle {
void StartLiveActionAnimation();
void AdvanceLiveActionEffectPhase();
void CommitLiveSelectedAction(int target_selection_mode);
void StartLiveActionPresentation();
// Same production builder used by StartLiveActionPresentation, without
// dispatch. Missing used owners or >16 compacted targets are diagnosed.
void BuildLiveActionPresentationRequest(Battle_ActionRequest* action,
                                        Effects_ActionEffectRequest* request);
}  // namespace VS::Battle
