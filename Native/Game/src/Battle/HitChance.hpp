#pragma once
// The hit chance function table: the action's hit selector picks the rule
// that turns the source and target into a success threshold. HitChance.cpp
// defines it; the module image's relocated pointers fill it at start-up.

#include <array>
#include <cstddef>

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {

using HitFunction = int (*)(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                            int);

inline constexpr std::size_t kHitFunctionCount = 8;

// decomp: _hitFunctions, upstream hitFunctions
// (src/BATTLE/BATTLE.PRG/146C.c:300)
extern std::array<HitFunction, kHitFunctionCount> hitFunctions;

}  // namespace VS::Battle
