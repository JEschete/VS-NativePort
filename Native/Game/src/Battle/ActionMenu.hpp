#pragma once
// The action menu's tables: each spell class's spell count and spell ids, and
// the chain and defense ability action ids. ActionMenu.cpp defines them; the
// module image fills them at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {

inline constexpr std::size_t kSpellClassCount = 4;
// The bytes of the module image each ability action id table spans.
inline constexpr std::size_t kAbilityActionIdBytes = 16;

// decomp: _spellClassCounts, upstream spellClassCounts
// (src/BATTLE/BATTLE.PRG/5BF94.c:386)
extern std::array<std::uint8_t, kSpellClassCount> spellClassCounts;
// decomp: _spellIds, upstream spellIds (src/BATTLE/BATTLE.PRG/5BF94.c:387);
// relocated pointers (ModulePointers.cpp)
extern std::array<std::uint8_t*, kSpellClassCount> spellClassSpellIds;
// decomp: vs_battle_chainAbilityOffsets (src/BATTLE/BATTLE.PRG/battleUi.h:290)
extern std::array<std::int8_t, kAbilityActionIdBytes> chainAbilityActionIds;
// decomp: vs_battle_defenseAbilityOffsets
// (src/BATTLE/BATTLE.PRG/battleUi.h:291)
extern std::array<std::int8_t, kAbilityActionIdBytes> defenseAbilityActionIds;

}  // namespace VS::Battle
