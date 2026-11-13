#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {
void WaitForLiveActionEffectReady();
void StartLiveReadyActionAnimation();
void WaitForLiveActionResolution();
void WaitForLiveTargetRecovery();
void FinishLiveActionSequence();
}  // namespace VS::Battle
