#pragma once
// The chain-ability animation tables and the special action's pitch.
// ActionAnimation.cpp defines them; the module image fills the group table.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {

// The bytes of the module image the group table spans.
inline constexpr std::size_t kChainAbilityAnimationGroupBytes = 168;

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using ChainAbilityAnimationGroupTable =
    std::array<std::uint8_t, kChainAbilityAnimationGroupBytes>;
// NOLINTEND

// decomp: D_800E8E80 (src/BATTLE/BATTLE.PRG/30DB0.c:68)
extern ChainAbilityAnimationGroupTable chainAbilityAnimationGroups;
// decomp: D_800F49D8 (src/BATTLE/BATTLE.PRG/30DB0.c:116)
extern std::int32_t chainAbilityAnimationSide;
// decomp: D_800F49E4 (src/BATTLE/BATTLE.PRG/30DB0Private.h:11)
extern std::int8_t specialActionPitch;

}  // namespace VS::Battle
