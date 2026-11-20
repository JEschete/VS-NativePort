#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <span>

#include "Battle/Ai/ActionRecords.hpp"
#include "Battle/Ai/ConditionSnapshot.hpp"

// Binds the live battle actor list to the typed condition snapshot.
namespace VS::Battle::Ai {
// Captures every listed battle actor's condition into its planner row.
void CaptureBattleActorConditions(
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows);
}  // namespace VS::Battle::Ai
