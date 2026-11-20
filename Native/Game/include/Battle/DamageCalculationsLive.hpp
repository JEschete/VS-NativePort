#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle
namespace VS::Battle {
std::int16_t LiveCalculatePhysicalHitDamage(Battle_ActionDefinition* action,
                                            VS::Battle::HitResult* source,
                                            VS::Battle::HitResult* target,
                                            int hit, int random,
                                            int apply_statuses);
std::int16_t LivePhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
                                              VS::Battle::HitResult* source,
                                              VS::Battle::HitResult* target,
                                              int hit, int random);
std::int16_t LiveCalculateSpellHitDamage(Battle_ActionDefinition* action,
                                         VS::Battle::HitResult* source,
                                         VS::Battle::HitResult* target, int hit,
                                         int random);
std::int16_t LiveCalculateSpellHealing(Battle_ActionDefinition* action,
                                       VS::Battle::HitResult* source,
                                       VS::Battle::HitResult* target, int hit,
                                       int random);
std::int16_t LivePhysicalDamageWithDebuffs(Battle_ActionDefinition* action,
                                           VS::Battle::HitResult* source,
                                           VS::Battle::HitResult* target,
                                           int hit, int random);
}  // namespace VS::Battle
