#pragma once
// TITLE.PRG's new-game state: the starting inventory record and the battle
// abilities learned at the start.
#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Title {
// The starting inventory record, copied whole into the game's inventory.
inline constexpr std::size_t kInventoryTemplateRows = 15;
inline constexpr std::size_t kInventoryTemplateWords = 64;
inline constexpr std::size_t kLearnedActionWords = 8;
using InventoryTemplateRow = std::array<std::int32_t, kInventoryTemplateWords>;

// decomp: D_80074C24 (D_80074C24@TITLE/TITLE.PRG)
extern InventoryTemplateRow newGameInventoryTemplate[kInventoryTemplateRows];
// decomp: actionsLearned (actionsLearned@TITLE/TITLE.PRG)
extern std::int32_t initialLearnedActionBits[kLearnedActionWords];
}  // namespace VS::Title
