#pragma once
// The magic menus' shared tables. The four spell-id lists are BATTLE.PRG data
// (src/BATTLE/BATTLE.PRG/battleUi.h:285-288): the module image fills them, and
// the HUD shortcut check and MENU0.PRG's school menus read them. MENU0.PRG's
// string tables (src/MENU/MENU0.PRG/84.c) are filled from the disc. The
// original compiled its char data unsigned (-funsigned-char).
#include <cstddef>
#include <cstdint>

namespace VS::Menu::Magic {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kWarlockSpellIdCount = 20;
inline constexpr std::size_t kShamanSpellIdCount = 8;
inline constexpr std::size_t kSorcererSpellIdCount = 20;
inline constexpr std::size_t kEnchanterSpellIdCount = 8;
inline constexpr std::size_t kMagicStringWords = 2462;
inline constexpr std::size_t kTeleportationStringWords = 1599;

// decomp: vs_battle_warlockSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:285)
extern std::uint8_t warlockSpellIds[kWarlockSpellIdCount];
// decomp: vs_battle_shamanSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:286)
extern std::uint8_t shamanSpellIds[kShamanSpellIdCount];
// decomp: vs_battle_sorcererSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:287)
extern std::uint8_t sorcererSpellIds[kSorcererSpellIdCount];
// decomp: vs_battle_enchanterSpellIds (src/BATTLE/BATTLE.PRG/battleUi.h:288)
extern std::uint8_t enchanterSpellIds[kEnchanterSpellIdCount];

// decomp: magicStrings (src/MENU/MENU0.PRG/84.c:93): halfword offsets, then
// the strings.
extern std::uint16_t magicStrings[kMagicStringWords];
// decomp: teleportationStrings (src/MENU/MENU0.PRG/84.c:98)
extern std::uint16_t teleportationStrings[kTeleportationStringWords];
}  // namespace VS::Menu::Magic
