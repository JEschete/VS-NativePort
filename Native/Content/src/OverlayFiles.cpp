// Hand-maintained (tools/port/owned.csv).
#include "Content/OverlayFiles.hpp"

#include <cstdint>

namespace VS::Content {

// decomp: BATTLE.PRG vs_battle_getMainMenuLba
// (src/BATTLE/BATTLE.PRG/6F014.c:804)
std::int32_t MainMenuOverlayDiscSector() noexcept {
  return kMainMenuOverlaySector;
}

}  // namespace VS::Content
