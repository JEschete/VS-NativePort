#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionRequest;

namespace VS::Effects {
struct LiveActionEffectRequest;
}  // namespace VS::Effects

namespace VS::Battle {
void StartLiveActionAnimation();
void AdvanceLiveActionEffectPhase();
void CommitLiveSelectedAction(int target_selection_mode);
void StartLiveActionPresentation();
// Same production builder used by StartLiveActionPresentation, without
// dispatch. Missing used owners or >16 compacted targets are diagnosed.
void BuildLiveActionPresentationRequest(
    Battle_ActionRequest* action, Effects::LiveActionEffectRequest* request);
}  // namespace VS::Battle
