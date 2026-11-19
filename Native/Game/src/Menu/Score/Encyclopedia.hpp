#pragma once
// MENU9.PRG's encyclopedia tables (src/MENU/MENU9.PRG/592C.c): each enemy's
// ZUD file and palette flag, and the enemy list's navigation state.
#include <cstdint>

#include "Menu/Score/EncyclopediaPreview.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// decomp: _enemyNavigationState (src/MENU/MENU9.PRG/238.c:1103)
extern std::int32_t enemyNavigationState;
// decomp: _paletteFlags (src/MENU/MENU9.PRG/592C.c:186)
extern std::uint8_t paletteFlags[kEnemyPreviewFiles];
// decomp: _zudLbas (src/MENU/MENU9.PRG/592C.c:178)
extern std::int32_t zudLbas[kEnemyPreviewFiles];
// decomp: _zudSizes (src/MENU/MENU9.PRG/592C.c:180)
extern std::int32_t zudSizes[kEnemyPreviewFiles];
}  // namespace VS::Menu::Score
