#pragma once
// Hand-maintained (tools/port/owned.csv).
namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void DeriveLiveLimbInjuryStatuses(VS::Battle::HitResult* hit);
void ApplyLiveActionHitEffect(VS::Battle::ActionDefinition* action,
                              VS::Battle::HitResult* source,
                              VS::Battle::HitResult* target, int execute,
                              int hit);
}  // namespace VS::Battle
