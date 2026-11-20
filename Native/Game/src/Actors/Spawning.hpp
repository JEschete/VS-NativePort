#pragma once
// The spawner's tables: Ashley's starting loadout, the placeholder actor name
// and which body models carry a second body. Spawning.cpp defines them; the
// last two come from the SLUS_010.40 module image.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/CharacterInit.hpp"

namespace VS::Actors {
inline constexpr std::size_t kPlaceholderActorNameBytes = 20;
inline constexpr std::size_t kBodyModelIds = 256;

// decomp: D_800FAB18 (src/BATTLE/INITBTL.PRG/18.c)
extern CharacterInit ashleyInitialLoadout;
// decomp: D_8004FDD0 (D_8004FDD0@SLUS_010.40)
extern std::array<char, kPlaceholderActorNameBytes> placeholderActorName;
// decomp: D_8004FE88 (D_8004FE88@SLUS_010.40): nonzero for the body models
// whose actor keeps a second body in its weapon model slot.
extern std::array<std::uint8_t, kBodyModelIds> usesSecondaryBodyModel;
}  // namespace VS::Actors
