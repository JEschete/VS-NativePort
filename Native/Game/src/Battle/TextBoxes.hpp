#pragma once
// BATTLE.PRG's door and chest messages (src/BATTLE/BATTLE.PRG/5BF94.c).
// ModuleData.cpp fills the table from the module image; Content/StringTables.h
// names its entries.
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
inline constexpr std::size_t kDoorStringWords = 190;

// decomp: D_800EBDDC (src/BATTLE/BATTLE.PRG/5BF94.c:389): a halfword offset
// per message, then the encoded messages; entry 0 is the no-target message.
extern std::uint16_t doorStrings[kDoorStringWords];
}  // namespace VS::Battle
