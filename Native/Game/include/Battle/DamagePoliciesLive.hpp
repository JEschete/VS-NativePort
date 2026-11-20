#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct ActorStats;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
std::int16_t LiveApplyDamageGemMultiplier(VS::Battle::ActionDefinition* action,
                                          std::int16_t damage,
                                          VS::Battle::ActorStats* source,
                                          VS::Battle::ActorStats* target);
void LiveDistributeHitLimbDamage(VS::Battle::ActionDefinition* action,
                                 VS::Battle::HitResult* hit,
                                 std::int16_t amount);
void LiveDistributeLimbRecovery(VS::Battle::ActionDefinition* action,
                                VS::Battle::HitResult* hit,
                                std::int16_t amount);
std::int16_t LiveLastActionStatValue(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleLastActionStatValue(VS::Battle::ActionDefinition*,
                                          VS::Battle::HitResult*,
                                          VS::Battle::HitResult*, int, int);
std::int16_t LiveTargetRisk(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                            int);
}  // namespace VS::Battle
