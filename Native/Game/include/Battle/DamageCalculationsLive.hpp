#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle
namespace VS::Battle {
std::int16_t LiveCalculatePhysicalHitDamage(
    VS::Battle::ActionDefinition* action, VS::Battle::HitResult* source,
    VS::Battle::HitResult* target, int hit, int random, int apply_statuses);
std::int16_t LivePhysicalDamageWithoutDebuffs(
    VS::Battle::ActionDefinition* action, VS::Battle::HitResult* source,
    VS::Battle::HitResult* target, int hit, int random);
std::int16_t LiveCalculateSpellHitDamage(VS::Battle::ActionDefinition* action,
                                         VS::Battle::HitResult* source,
                                         VS::Battle::HitResult* target, int hit,
                                         int random);
std::int16_t LiveCalculateSpellHealing(VS::Battle::ActionDefinition* action,
                                       VS::Battle::HitResult* source,
                                       VS::Battle::HitResult* target, int hit,
                                       int random);
std::int16_t LivePhysicalDamageWithDebuffs(VS::Battle::ActionDefinition* action,
                                           VS::Battle::HitResult* source,
                                           VS::Battle::HitResult* target,
                                           int hit, int random);
}  // namespace VS::Battle
