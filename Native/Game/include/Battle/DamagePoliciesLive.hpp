#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_ActorStats;
struct Battle_HitResult;

namespace VS::Battle {
std::int16_t LiveApplyDamageGemMultiplier(Battle_ActionDefinition* action,
                                          std::int16_t damage,
                                          Battle_ActorStats* source,
                                          Battle_ActorStats* target);
void LiveDistributeHitLimbDamage(Battle_ActionDefinition* action,
                                 Battle_HitResult* hit, std::int16_t amount);
void LiveDistributeLimbRecovery(Battle_ActionDefinition* action,
                                Battle_HitResult* hit, std::int16_t amount);
std::int16_t LiveLastActionStatValue(Battle_ActionDefinition*,
                                     Battle_HitResult*, Battle_HitResult*, int,
                                     int);
std::int16_t LiveScaleLastActionStatValue(Battle_ActionDefinition*,
                                          Battle_HitResult*, Battle_HitResult*,
                                          int, int);
std::int16_t LiveTargetRisk(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult*, int, int);
}  // namespace VS::Battle
