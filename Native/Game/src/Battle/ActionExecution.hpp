#pragma once
// The action camera's settled distance and ease mode, which the action
// sequence reads when it frames an action. In the original they are
// vs_battle_cameraCurrentSpherical's values.distance and delta.mode
// (src/BATTLE/BATTLE.PRG/camera.h:7-20); the native sequence reads
// VS::Field::sphericalCamera instead. ActionExecution.cpp defines them.

#include <cstdint>

namespace VS::Battle {

// decomp: D_800F19D8 (src/BATTLE/BATTLE.PRG/146C.c:336)
extern std::int32_t settledCameraDistance;
// decomp: D_800F19EC (src/BATTLE/BATTLE.PRG/146C.c:337)
extern std::int32_t cameraEaseMode;

}  // namespace VS::Battle
