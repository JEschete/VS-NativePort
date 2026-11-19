#pragma once
// The battle ability screen's module statics (MENU2.PRG). The ability rows,
// cursor and learned-ability lists live here so the assignment terminal
// (BattleAbilitiesTerminalLive.cpp) and the unlock screens
// (BattleAbilitiesUnlockLive.cpp) share one copy; the typed state machines
// (BattleAbilities.hpp, BattleAbilitiesTerminal.hpp) receive them as views.
// The original compiled its char statics unsigned (-funsigned-char).
#include <cstddef>
#include <cstdint>

#include "Menu/BattleAbilities.hpp"

namespace VS::Menu::BattleAbilities {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kBattleAbilityStringWords = 1060;
inline constexpr std::size_t kBattleAbilityMenuStringWords = 292;

// decomp: battleAbilityStrings (src/MENU/MENU2.PRG/64.c:49): halfword
// offsets, then the strings; ModuleData.cpp fills it from the disc.
extern std::uint16_t battleAbilityStrings[kBattleAbilityStringWords];
// decomp: battleAbilityMenuStrings (src/MENU/MENU2.PRG/143C.c:16): the unlock
// screens' string table, filled the same way.
extern std::uint16_t battleAbilityMenuStrings[kBattleAbilityMenuStringWords];

// decomp: abilityText (src/MENU/MENU2.PRG/64.c:54): the ability list's row
// text, a game heap block of kAbilityRowWords per row; null while no list is
// held.
extern std::uint16_t* abilityText;
// decomp: highlightedAbility (src/MENU/MENU2.PRG/64.c:55)
extern std::int32_t highlightedAbility;
// decomp: abilityMenuState (src/MENU/MENU2.PRG/64.c:56)
extern std::uint32_t abilityMenuState;
// decomp: selectedAbilityType (src/MENU/MENU2.PRG/64.c:57): an
// AbilityMenuType value.
extern std::uint8_t selectedAbilityType;
// decomp: menuAbilityCount (src/MENU/MENU2.PRG/64.c:58)
extern std::uint8_t menuAbilityCount;
// decomp: firstAbility (src/MENU/MENU2.PRG/64.c:59)
extern std::uint8_t firstAbility;

// decomp: unlockedChainAbilities (src/MENU/MENU2.PRG/64.c:61): the learned
// chain action ids.
extern std::uint8_t unlockedChainAbilities[kAbilityCapacity];
// decomp: unlockedDefenseAbilities (src/MENU/MENU2.PRG/64.c:62)
extern std::uint8_t unlockedDefenseAbilities[kAbilityCapacity];

// MENU2.PRG reloads restore 64.c's initialized statics (PORTING_WATCHLIST.md
// §1).
void ResetMenu2Statics();
}  // namespace VS::Menu::BattleAbilities
