// The battle actor slots and damage calculation state declared in Damage.hpp.
#include "Battle/Damage.hpp"

#include <array>
#include <cstdint>

namespace VS::Battle {

// decomp: D_800E82F4 (D_800E82F4@BATTLE/BATTLE.PRG)
// Contents: module image (ModuleData.cpp).
std::array<std::uint16_t, kBladeCategoryCount> bladePpDamageBonuses;

// decomp: _statCalculators (_statCalculators@BATTLE/BATTLE.PRG)
// Contents: module image, relocated (ModulePointers.cpp).
std::array<StatCalculator, kStatCalculatorCount> statCalculators;

// decomp: vs_battle_actors (vs_battle_actors@BATTLE/BATTLE.PRG)
std::array<Battle_Actor*, kBattleActorSlotCount> actors;

// decomp: _lastValue (_lastValue@BATTLE/BATTLE.PRG)
std::uint32_t lastActionStatValue;

// decomp: D_800F1A0C (D_800F1A0C@BATTLE/BATTLE.PRG)
std::int32_t criticalHitOccurred;

// decomp: _armorDpAdjustmentAmounts
// (_armorDpAdjustmentAmounts@BATTLE/BATTLE.PRG)
std::array<std::int16_t, kArmorDpLossSlotCount> limbArmorDpLoss;

}  // namespace VS::Battle
