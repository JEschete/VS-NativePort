#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {
struct ActionRequestRecord;
}  // namespace VS::Battle

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
    VS::Battle::ActionRequestRecord* action,
    Effects::LiveActionEffectRequest* request);
}  // namespace VS::Battle
