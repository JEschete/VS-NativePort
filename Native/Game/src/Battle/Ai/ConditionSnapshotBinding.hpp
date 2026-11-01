#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <span>

#include "Battle/Ai/ConditionSnapshot.hpp"

extern "C" {
#include "Battle/Ai/AiViews.h"
}

// Binds the live battle actor list to the typed condition snapshot.
namespace VS::Battle::Ai {
// Captures every listed battle actor's condition into its planner row.
void CaptureBattleActorConditions(
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows);
}  // namespace VS::Battle::Ai
