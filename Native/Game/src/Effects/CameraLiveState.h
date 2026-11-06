#pragma once
// Hand-maintained (tools/port/owned.csv).
// The effect camera as the copied effect runtime and PLG128 read and write
// it. CameraLive.cpp defines it and mirrors it into the typed
// EffectCameraState (Effects/Camera.hpp) around each call.

#include <stdint.h>

#include "Effects/Camera.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F54B8 (BATTLE/BATTLE.PRG 5BF94.c:6474): the four shake
// channels. The generated declaration had 75 entries; the original's are
// the four bytes before D_800F54D0 (6 bytes each).
enum { kEffectsCameraShakeChannelCount = 4 };
extern Effects_CameraShakeChannel
    Effects_cameraShakeChannels[kEffectsCameraShakeChannelCount];
// decomp: D_800F54D0 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// current frame.
extern int16_t Effects_cameraMotionFrame;
// decomp: D_800F54D8 (BATTLE/BATTLE.PRG 5BF94.c:6681): the effect camera
// the transition and the shake move.
extern Effects_EffectCamera Effects_effectCamera;
// decomp: D_800F5518 (BATTLE/BATTLE.PRG 5BF94.c:6564): bit n set while
// shake channel n runs.
extern char Effects_enabledShakeChannels;
// decomp: D_800F5520 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// start and end poses.
enum { kEffectsCameraMotionEndCount = 2 };
extern Effects_EffectCamera
    Effects_cameraMotionEnds[kEffectsCameraMotionEndCount];
// decomp: D_800F55A0 (BATTLE/BATTLE.PRG 5BF94.c:6681): the transition's
// length in frames.
extern int16_t Effects_cameraMotionDuration;
// decomp: D_800F55A8 (BATTLE/BATTLE.PRG 5BF94.c:8323): the action camera
// RestoreActionEffectCamera eases back to.
extern Effects_EffectCamera Effects_savedActionCamera;
// decomp: D_800F55E8 (BATTLE/BATTLE.PRG 5BF94.c:6575): the transition mode
// (0 none, 1 linear, 2 cosine, 3 hold).
extern u_int Effects_cameraTransitionMode;

#ifdef __cplusplus
}
#endif
