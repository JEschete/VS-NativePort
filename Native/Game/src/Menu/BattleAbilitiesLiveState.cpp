#include "Menu/BattleAbilitiesLiveState.hpp"

#include <cstdint>

namespace VS::Menu::BattleAbilities {  // NOLINT(readability-identifier-naming)
namespace {
// 64.c:54-59 initialize the ability list's statics.
constexpr std::int32_t kInitialHighlightedAbility = 0;
constexpr std::uint32_t kInitialAbilityMenuState = 0;
constexpr std::uint8_t kInitialSelectedAbilityType = 0;
constexpr std::uint8_t kInitialMenuAbilityCount = 0;
constexpr std::uint8_t kInitialFirstAbility = 0;
}  // namespace

// decomp: battleAbilityStrings (src/MENU/MENU2.PRG/64.c:49): filled from the
// disc (ModuleData.cpp).
std::uint16_t battleAbilityStrings[kBattleAbilityStringWords];
// decomp: battleAbilityMenuStrings (src/MENU/MENU2.PRG/143C.c:16): filled from
// the disc (ModuleData.cpp).
std::uint16_t battleAbilityMenuStrings[kBattleAbilityMenuStringWords];

// decomp: abilityText (src/MENU/MENU2.PRG/64.c:54)
std::uint16_t* abilityText = nullptr;
// decomp: highlightedAbility (src/MENU/MENU2.PRG/64.c:55)
std::int32_t highlightedAbility = kInitialHighlightedAbility;
// decomp: abilityMenuState (src/MENU/MENU2.PRG/64.c:56)
std::uint32_t abilityMenuState = kInitialAbilityMenuState;
// decomp: selectedAbilityType (src/MENU/MENU2.PRG/64.c:57)
std::uint8_t selectedAbilityType = kInitialSelectedAbilityType;
// decomp: menuAbilityCount (src/MENU/MENU2.PRG/64.c:58)
std::uint8_t menuAbilityCount = kInitialMenuAbilityCount;
// decomp: firstAbility (src/MENU/MENU2.PRG/64.c:59)
std::uint8_t firstAbility = kInitialFirstAbility;

// decomp: unlockedChainAbilities (src/MENU/MENU2.PRG/64.c:61)
std::uint8_t unlockedChainAbilities[kAbilityCapacity];
// decomp: unlockedDefenseAbilities (src/MENU/MENU2.PRG/64.c:62)
std::uint8_t unlockedDefenseAbilities[kAbilityCapacity];

void ResetMenu2Statics() {
  abilityText = nullptr;
  highlightedAbility = kInitialHighlightedAbility;
  abilityMenuState = kInitialAbilityMenuState;
  selectedAbilityType = kInitialSelectedAbilityType;
  menuAbilityCount = kInitialMenuAbilityCount;
  firstAbility = kInitialFirstAbility;
}
}  // namespace VS::Menu::BattleAbilities
