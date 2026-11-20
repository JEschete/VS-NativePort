#pragma once
// The player's battle actor and the point requirements of the Break Arts and
// Battle Abilities. AbilityState.cpp defines them; the SLUS module image fills
// the requirement tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

struct Battle_Actor;

namespace VS::Battle {

inline constexpr std::size_t kBreakArtCategoryCount = 15;
inline constexpr std::size_t kBreakArtsPerCategory = 4;
inline constexpr std::size_t kAbilityRequirementCount = 150;

using BreakArtPointRow = std::array<std::uint16_t, kBreakArtsPerCategory>;
using BreakArtPointTable = std::array<BreakArtPointRow, kBreakArtCategoryCount>;
using AbilityPointTable = std::array<std::uint16_t, kAbilityRequirementCount>;

// decomp: vs_battle_characterState (src/BATTLE/BATTLE.PRG/actor.h:173)
extern Battle_Actor* characterState;
// decomp: vs_main_artsPointsRequirements (src/SLUS_010.40/actions.h:65)
extern BreakArtPointTable breakArtPointRequirements;
// decomp: vs_main_battleAbilitiesPointsRequirements
// (src/SLUS_010.40/actions.h:66)
extern AbilityPointTable battleAbilityPointRequirements;

}  // namespace VS::Battle
