#pragma once
// MENU9.PRG's encyclopedia tables (src/MENU/MENU9.PRG/592C.c): each enemy's
// ZUD file and palette flag, and the enemy list's navigation state.
#include <array>
#include <cstdint>

#include "Menu/Score/EncyclopediaPreview.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// decomp: _enemyNavigationState (src/MENU/MENU9.PRG/238.c:1103)
extern std::int32_t enemyNavigationState;
// decomp: paletteFlags (src/MENU/MENU9.PRG/592C.c:158; was _paletteFlags)
extern std::uint8_t paletteFlags[kEnemyPreviewFiles];
// decomp: zudLbas (src/MENU/MENU9.PRG/592C.c:28; was _zudLbas)
extern std::array<std::int32_t, kEnemyPreviewFiles> zudLbas;
// decomp: zudSizes (src/MENU/MENU9.PRG/592C.c:93; was _zudSizes)
extern std::array<std::int32_t, kEnemyPreviewFiles> zudSizes;
}  // namespace VS::Menu::Score
