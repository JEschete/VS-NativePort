// The action menu's tables (ActionMenu.hpp).
#include "Battle/ActionMenu.hpp"

namespace VS::Battle {
// decomp: _spellClassCounts (_spellClassCounts@BATTLE/BATTLE.PRG); contents:
// module image (ModuleData.cpp)
std::array<std::uint8_t, kSpellClassCount> spellClassCounts;
// decomp: _spellIds (_spellIds@BATTLE/BATTLE.PRG); contents: module image,
// relocated (ModulePointers.cpp)
std::array<std::uint8_t*, kSpellClassCount> spellClassSpellIds;
// decomp: vs_battle_chainAbilityOffsets
// (vs_battle_chainAbilityOffsets@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::int8_t, kAbilityActionIdBytes> chainAbilityActionIds;
// decomp: vs_battle_defenseAbilityOffsets
// (vs_battle_defenseAbilityOffsets@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::int8_t, kAbilityActionIdBytes> defenseAbilityActionIds;
}  // namespace VS::Battle
