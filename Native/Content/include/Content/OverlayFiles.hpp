#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Content {

// MAINMENU.PRG's first disc sector, as the battle program hard-codes it
// (BATTLE.PRG 6F014.c:804: lui/ori 0x14C08).
inline constexpr std::int32_t kMainMenuOverlaySector = 0x14C08;

// The sector the effect runtime reloads the main menu overlay from.
[[nodiscard]] std::int32_t MainMenuOverlayDiscSector() noexcept;

}  // namespace VS::Content
