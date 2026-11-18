#pragma once
// Hand-maintained (tools/port/owned.csv).
// The effect camera as the copied effect runtime and PLG128 read and write
// it. CameraLive.cpp defines it and mirrors it into the typed
// EffectCameraState (Effects/Camera.hpp) around each call.

#include <stdint.h>

#include "Effects/Camera.h"
#include "Legacy/Psx.h"

namespace VS::Effects {
// decomp: D_800F54B8 (BATTLE/BATTLE.PRG 5BF94.c:6474): the four shake
// channels. The generated declaration had 75 entries; the original's are
// the four bytes before D_800F54D0 (6 bytes each).
inline constexpr int kCameraShakeChannelCount = 4;
extern Effects_CameraShakeChannel cameraShakeChannels[kCameraShakeChannelCount];
// decomp: D_800F54D0 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// current frame.
extern int16_t cameraMotionFrame;
// decomp: D_800F54D8 (BATTLE/BATTLE.PRG 5BF94.c:6681): the effect camera
// the transition and the shake move.
extern Effects_EffectCamera effectCamera;
// decomp: D_800F5518 (BATTLE/BATTLE.PRG 5BF94.c:6564): bit n set while
// shake channel n runs.
extern char enabledShakeChannels;
// decomp: D_800F5520 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// start and end poses.
inline constexpr int kCameraMotionEndCount = 2;
extern Effects_EffectCamera cameraMotionEnds[kCameraMotionEndCount];
// decomp: D_800F55A0 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// length in frames.
extern int16_t cameraMotionDuration;
// decomp: D_800F55A8 (BATTLE/BATTLE.PRG 5BF94.c:8323): the action camera
// RestoreActionEffectCamera eases back to.
extern Effects_EffectCamera savedActionCamera;
// decomp: D_800F55E8 (BATTLE/BATTLE.PRG 5BF94.c:6575): the transition mode
// (0 none, 1 linear, 2 cosine, 3 hold).
extern u_int cameraTransitionMode;
}  // namespace VS::Effects
