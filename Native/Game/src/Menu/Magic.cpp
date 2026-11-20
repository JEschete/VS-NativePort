#include "Menu/Magic.hpp"

#include <cstdint>

namespace VS::Menu::Magic {  // NOLINT(readability-identifier-naming)
// decomp: vs_battle_warlockSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:285):
// filled from the module image (ModuleData.cpp).
std::uint8_t warlockSpellIds[kWarlockSpellIdCount];
// decomp: vs_battle_shamanSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:286)
std::uint8_t shamanSpellIds[kShamanSpellIdCount];
// decomp: vs_battle_sorcererSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:287)
std::uint8_t sorcererSpellIds[kSorcererSpellIdCount];
// decomp: vs_battle_enchanterSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:288)
std::uint8_t enchanterSpellIds[kEnchanterSpellIdCount];

// decomp: magicStrings (src/MENU/MENU0.PRG/84.c:93): filled from the disc
// (ModuleData.cpp).
std::uint16_t magicStrings[kMagicStringWords];
// decomp: teleportationStrings (src/MENU/MENU0.PRG/84.c:98): filled from the
// disc (ModuleData.cpp).
std::uint16_t teleportationStrings[kTeleportationStringWords];
}  // namespace VS::Menu::Magic
