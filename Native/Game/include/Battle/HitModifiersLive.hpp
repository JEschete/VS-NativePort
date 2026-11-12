#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_ActorStats;
struct Battle_HitResult;

namespace VS::Battle {
std::int16_t AttackLiveGemHitBonus(Battle_ActionDefinition* action,
                                   Battle_ActorStats* stats);
std::int16_t DefenseLiveGemHitBonus(Battle_ActionDefinition* action,
                                    Battle_ActorStats* stats, int hit);
int PreviousLiveHitSuccessThreshold(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
int LiveGemHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
                     Battle_HitResult* target, int hit, int execute);
int LiveRiskHitChance(Battle_ActionDefinition*, Battle_HitResult* source,
                      Battle_HitResult*, int, int);
int LiveChainHitChance(Battle_ActionDefinition*, Battle_HitResult*,
                       Battle_HitResult* target, int, int);
}  // namespace VS::Battle
