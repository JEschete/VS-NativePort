#pragma once
// The field's interaction focus and control mode. Interactions.cpp defines
// them.

#include <cstdint>

#include "Field/InteractionFocusLive.hpp"

namespace VS::Field {
// The actor and location the field's interactions act on.
// decomp: D_800F1910 (D_800F1910@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.c:331)
extern InteractionFocus interactionFocus;
// Which system drives the field: the field control modes
// (Boot/GameplayLoop.hpp).
// decomp: D_800F196C (D_800F196C@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.c:384)
extern std::uint32_t controlMode;
}  // namespace VS::Field
