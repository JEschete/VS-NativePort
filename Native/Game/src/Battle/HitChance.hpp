#pragma once
// The hit chance function table: the action's hit selector picks the rule
// that turns the source and target into a success threshold. HitChance.cpp
// defines it; the module image's relocated pointers fill it at start-up.

#include <array>
#include <cstddef>

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {

using HitFunction = int (*)(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult*, int, int);

inline constexpr std::size_t kHitFunctionCount = 8;

// decomp: _hitFunctions, upstream hitFunctions
// (src/BATTLE/BATTLE.PRG/146C.c:300)
extern std::array<HitFunction, kHitFunctionCount> hitFunctions;

}  // namespace VS::Battle
