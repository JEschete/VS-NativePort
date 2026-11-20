#pragma once
// The action table: the 256 action definitions of the module image.
// ActionTable.cpp defines it; ModuleData.cpp fills it at start-up.

#include <array>
#include <cstddef>

#include "Battle/Action.hpp"

namespace VS::Battle {

inline constexpr std::size_t kActionTableCount = 256;
using ActionDefinitionTable = std::array<ActionDefinition, kActionTableCount>;

// decomp: vs_main_actions (src/SLUS_010.40/actions.h:64)
extern ActionDefinitionTable actionDefinitions;

}  // namespace VS::Battle
