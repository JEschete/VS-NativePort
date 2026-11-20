#pragma once
// The attack prerequisite function table: each action hit names the rule that
// decides whether it may apply to its target. AttackPrerequisiteTable.cpp
// defines it; the module image's relocated pointers fill it at start-up.

#include <array>
#include <cstddef>

struct Battle_ActionDefinition;

namespace VS::Battle {

using AttackPrerequisiteFunction = int (*)(Battle_ActionDefinition*,
                                           unsigned char*);

inline constexpr std::size_t kAttackPrerequisiteCount = 40;

// clang-format's alias continuation disagrees with cpplint.
using AttackPrerequisiteTable =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<AttackPrerequisiteFunction, kAttackPrerequisiteCount>;

// decomp: _attackPrerequisiteFunctions, upstream attackPrerequisiteFunctions
// (src/BATTLE/BATTLE.PRG/146C.c:301)
extern AttackPrerequisiteTable attackPrerequisiteFunctions;

}  // namespace VS::Battle
