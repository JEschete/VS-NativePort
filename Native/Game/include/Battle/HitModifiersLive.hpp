#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_ActorStats;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
std::int16_t AttackLiveGemHitBonus(Battle_ActionDefinition* action,
                                   Battle_ActorStats* stats);
std::int16_t DefenseLiveGemHitBonus(Battle_ActionDefinition* action,
                                    Battle_ActorStats* stats, int hit);
int PreviousLiveHitSuccessThreshold(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
int LiveGemHitChance(Battle_ActionDefinition* action,
                     VS::Battle::HitResult* source,
                     VS::Battle::HitResult* target, int hit, int execute);
int LiveRiskHitChance(Battle_ActionDefinition*, VS::Battle::HitResult* source,
                      VS::Battle::HitResult*, int, int);
int LiveChainHitChance(Battle_ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int);
}  // namespace VS::Battle
