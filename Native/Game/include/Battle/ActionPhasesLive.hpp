#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Battle {
void ReleaseLiveQueuedActorAttacks();
void WaitForLivePostActionMovement();
void BeginLivePostActionRecovery(std::int32_t reward_mode);
void BeginLiveActionTrailFade();
void FinishLiveBattleAbilitySequence();
void ResetLiveActionPhase();
void BeginLiveAshleyWeaponToggle();
}  // namespace VS::Battle
