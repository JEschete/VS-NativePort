#pragma once
// The action camera's settled distance and ease mode, which the action
// sequence reads when it frames an action. The original never writes them, so
// they keep their module-image values. ActionExecution.cpp defines them.

#include <cstdint>

namespace VS::Battle {

// decomp: D_800F19D8 (src/BATTLE/BATTLE.PRG/146C.c:336)
extern std::int32_t settledCameraDistance;
// decomp: D_800F19EC (src/BATTLE/BATTLE.PRG/146C.c:337)
extern std::int32_t cameraEaseMode;

}  // namespace VS::Battle
