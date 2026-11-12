#pragma once
// Hand-maintained (tools/port/owned.csv).
struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
void DeriveLiveLimbInjuryStatuses(Battle_HitResult* hit);
void ApplyLiveActionHitEffect(Battle_ActionDefinition* action,
                              Battle_HitResult* source,
                              Battle_HitResult* target, int execute, int hit);
}  // namespace VS::Battle
