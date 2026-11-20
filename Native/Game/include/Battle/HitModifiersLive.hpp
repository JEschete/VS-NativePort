#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct ActorStats;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
std::int16_t AttackLiveGemHitBonus(VS::Battle::ActionDefinition* action,
                                   VS::Battle::ActorStats* stats);
std::int16_t DefenseLiveGemHitBonus(VS::Battle::ActionDefinition* action,
                                    VS::Battle::ActorStats* stats, int hit);
int PreviousLiveHitSuccessThreshold(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
int LiveGemHitChance(VS::Battle::ActionDefinition* action,
                     VS::Battle::HitResult* source,
                     VS::Battle::HitResult* target, int hit, int execute);
int LiveRiskHitChance(VS::Battle::ActionDefinition*,
                      VS::Battle::HitResult* source, VS::Battle::HitResult*,
                      int, int);
int LiveChainHitChance(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int);
}  // namespace VS::Battle
