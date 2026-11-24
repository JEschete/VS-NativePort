#pragma once
// Mutable battle/status and menu state. Immutable status rules and timing
// definitions are owned by Content::StatusCatalog, not module images.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Actors {
struct WeaponModel;
}  // namespace VS::Actors

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
struct CursorMemory;
}  // namespace VS::Menu::Score

namespace VS::Battle {

inline constexpr std::size_t kStatusLimbCount = 4;

// decomp: D_800E8204 (src/BATTLE/BATTLE.PRG/146C.c:298)
extern std::array<std::int32_t, kStatusLimbCount> limbStatusFlags;
// decomp: D_800E8490 (src/BATTLE/BATTLE.PRG/146C.c:304)
extern std::int32_t vitalsTickCounter;
// decomp: D_800E8494 (src/BATTLE/BATTLE.PRG/146C.c:305)
extern std::int32_t regeneratingLimb;
// decomp: D_800E8498 (src/BATTLE/BATTLE.PRG/146C.c:306)
extern std::int32_t targetingRiskTimer;
// decomp: vs_battle_sceneBuffer (src/BATTLE/BATTLE.PRG/mpd.h:257)
extern void* sceneBuffer;
// decomp: vs_battle_menu9CursorMemory (src/BATTLE/BATTLE.PRG/menuState.h:39)
extern VS::Menu::Score::CursorMemory menu9CursorMemory;
// decomp: vs_battle_submenuState (src/BATTLE/BATTLE.PRG/menuState.h:40)
extern std::int32_t submenuState;
// decomp: D_800F4590 (src/BATTLE/BATTLE.PRG/battleUnk.h:34): the original's
// alias of the preview weapon model slots; its D_800F4590_t is a partial view
// of the weapon model it points at.
extern std::array<Actors::WeaponModel*, 1> previewWeaponModelAliases;
// decomp: D_800F4594 (src/BATTLE/BATTLE.PRG/battleUnk.h:35): the original's
// alias of the preview shield model slot.
extern std::int32_t previewShieldModelAlias;

}  // namespace VS::Battle
