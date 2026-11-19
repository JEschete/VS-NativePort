#pragma once
// Hand-maintained (tools/port/owned.csv).
// The effect camera as the copied effect runtime and PLG128 read and write
// it. CameraLive.cpp defines it and mirrors it into the typed
// EffectCameraState (Effects/Camera.hpp) around each call.

#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.h"

namespace VS::Effects {
// decomp: D_800F54B8_t (src/BATTLE/BATTLE.PRG/5BF94.c:249): one shake
// channel in its PS1 layout; CameraShakeChannel (Camera.hpp) is its typed
// value.
struct LiveCameraShakeChannel {
  std::uint8_t phase;
  std::uint8_t shape_curve : 5;
  std::uint8_t curve_step : 1;
  std::uint8_t random_step : 1;
  // Not used by native code; kept for the original layout.
  std::uint8_t reserved_bit7 : 1;
  std::uint8_t step_curve;
  std::uint8_t random_step_curve;
  std::uint8_t amplitude;
  std::uint8_t angle;
};

// decomp: D_800F54D8_t (src/BATTLE/BATTLE.PRG/battleEffect.h:390): one
// effect camera in its PS1 layout; EffectCameraPose (Camera.hpp) is its typed
// value.
struct LiveEffectCamera {
  VECTOR position;
  VECTOR look_at;
  std::int32_t shake_lateral;
  std::int32_t shake_vertical;
  std::int32_t shake_depth;
  std::int32_t shake_roll;
  std::int32_t roll;
  std::int32_t near_clip;
  std::int32_t projection_distance;
  std::int32_t far_clip;
};

namespace layout {
inline constexpr std::size_t kShakeChannelBytes = 6;
static_assert(sizeof(LiveCameraShakeChannel) == kShakeChannelBytes);
inline constexpr std::size_t kShakeStepCurveOffset = 2;
static_assert(offsetof(LiveCameraShakeChannel, step_curve) ==
              kShakeStepCurveOffset);
inline constexpr std::size_t kShakeAngleOffset = 5;
static_assert(offsetof(LiveCameraShakeChannel, angle) == kShakeAngleOffset);
inline constexpr std::size_t kEffectCameraBytes = 0x40;
static_assert(sizeof(LiveEffectCamera) == kEffectCameraBytes);
inline constexpr std::size_t kEffectCameraShakeOffset = 0x20;
static_assert(offsetof(LiveEffectCamera, shake_lateral) ==
              kEffectCameraShakeOffset);
inline constexpr std::size_t kEffectCameraRollOffset = 0x30;
static_assert(offsetof(LiveEffectCamera, roll) == kEffectCameraRollOffset);
inline constexpr std::size_t kEffectCameraFarClipOffset = 0x3C;
static_assert(offsetof(LiveEffectCamera, far_clip) ==
              kEffectCameraFarClipOffset);
}  // namespace layout

// decomp: D_800F54B8 (src/BATTLE/BATTLE.PRG/5BF94.c:440): the four shake
// channels. Upstream declares it without a bound; the four 6-byte channels
// end at D_800F54D0.
inline constexpr int kCameraShakeChannelCount = 4;
extern LiveCameraShakeChannel cameraShakeChannels[kCameraShakeChannelCount];
// decomp: D_800F54D0 (src/BATTLE/BATTLE.PRG/battleEffect.h:409): the
// transition's current frame.
extern std::int16_t cameraMotionFrame;
// decomp: D_800F54D8 (src/BATTLE/BATTLE.PRG/5BF94.c:441): the effect camera
// the transition and the shake move.
extern LiveEffectCamera effectCamera;
// decomp: D_800F5518 (src/BATTLE/BATTLE.PRG/5BF94.c:443): bit n set while
// shake channel n runs.
extern std::uint8_t enabledShakeChannels;
// decomp: D_800F5520 (src/BATTLE/BATTLE.PRG/battleEffect.h:410): the
// transition's start and end poses.
inline constexpr int kCameraMotionEndCount = 2;
extern LiveEffectCamera cameraMotionEnds[kCameraMotionEndCount];
// decomp: D_800F55A0 (src/BATTLE/BATTLE.PRG/battleEffect.h:411): the
// transition's length in frames.
extern std::int16_t cameraMotionDuration;
// decomp: D_800F55A8 (src/BATTLE/BATTLE.PRG/5BF94.c:444): the action camera
// RestoreActionEffectCamera eases back to.
extern LiveEffectCamera savedActionCamera;
// decomp: D_800F55E8 (src/BATTLE/BATTLE.PRG/5BF94.c:445): the transition
// mode (0 none, 1 linear, 2 cosine, 3 hold).
extern std::uint32_t cameraTransitionMode;
}  // namespace VS::Effects
