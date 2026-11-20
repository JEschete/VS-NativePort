#pragma once
// The battle actor slots and the damage calculation's tables and results:
// the blade PP bonuses, the stat calculators, the last calculated value, the
// critical hit flag and each limb's armor DP loss. Damage.cpp defines them;
// the module image fills the tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct Actor;

struct HitResult;

using StatCalculator = std::int16_t (*)(VS::Battle::ActionDefinition*,
                                        HitResult*, HitResult*, int, int);

inline constexpr std::size_t kBladeCategoryCount = 12;
inline constexpr std::size_t kStatCalculatorCount = 27;
inline constexpr std::size_t kBattleActorSlotCount = 42;
inline constexpr std::size_t kArmorDpLossSlotCount = 8;

// decomp: D_800E82F4 (src/BATTLE/BATTLE.PRG/146C.c:299)
extern std::array<std::uint16_t, kBladeCategoryCount> bladePpDamageBonuses;
// decomp: _statCalculators, upstream statCalculators
// (src/BATTLE/BATTLE.PRG/146C.c:303)
extern std::array<StatCalculator, kStatCalculatorCount> statCalculators;
// decomp: vs_battle_actors (src/BATTLE/BATTLE.PRG/actor.h:172)
extern std::array<VS::Battle::Actor*, kBattleActorSlotCount> actors;
// decomp: _lastValue, upstream lastValue (src/BATTLE/BATTLE.PRG/146C.c:340)
extern std::uint32_t lastActionStatValue;
// decomp: D_800F1A0C (src/BATTLE/BATTLE.PRG/146C.c:341)
extern std::int32_t criticalHitOccurred;
// decomp: _armorDpAdjustmentAmounts, upstream armorDpAdjustmentAmounts
// (src/BATTLE/BATTLE.PRG/146C.c:342)
extern std::array<std::int16_t, kArmorDpLossSlotCount> limbArmorDpLoss;

}  // namespace VS::Battle
