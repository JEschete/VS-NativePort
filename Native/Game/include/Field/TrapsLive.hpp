#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

// Binds the typed trap rules (Traps.hpp) to the battle actor records, the
// room's trap level and the game's random stream, in the shapes the copied
// callers use.
namespace VS::Field {
// The player's risk roll that breaks a lock: 1 when it breaks, otherwise 0.
// A missing player record takes no roll and returns 0.
int RollLiveRiskLockBreak();

// BATTLE.PRG stores these addresses in its stat calculator table
// (VS::Battle::statCalculators[12] and [14]), so they keep the table's
// signature. A missing record, a hit index outside the action's hits, a target
// outside the actor table or limbs, or an unauthored affinity takes nothing
// (0).
std::int16_t CalculateLiveTrapPanelDamage(
    VS::Battle::ActionDefinition* action,
    VS::Battle::HitResult* attacker_result,
    VS::Battle::HitResult* target_result, int hit_index,
    int add_random_variation);
std::int16_t CalculateLiveHealingPanelRecovery(
    VS::Battle::ActionDefinition* action,
    VS::Battle::HitResult* attacker_result,
    VS::Battle::HitResult* target_result, int hit_index,
    int add_random_variation);
}  // namespace VS::Field
