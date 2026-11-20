#pragma once
// Hand-maintained (tools/port/owned.csv).
struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void DeriveLiveLimbInjuryStatuses(VS::Battle::HitResult* hit);
void ApplyLiveActionHitEffect(Battle_ActionDefinition* action,
                              VS::Battle::HitResult* source,
                              VS::Battle::HitResult* target, int execute,
                              int hit);
}  // namespace VS::Battle
