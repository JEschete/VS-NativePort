#pragma once
// A splash image layer's tween, in the layout the event script VM keeps it.
// Event/ScriptEffectCommands.hpp's SplashTween is the value the tween update
// works on; ScriptEffectCommandsLive.cpp converts between the two.

#include <cstddef>
#include <cstdint>

namespace VS::Event {
// decomp: GimTween (src/BATTLE/BATTLE.PRG/4DEA8.c:163)
struct SplashTweenRecord {
  std::uint8_t reserved00;
  // Selects the interpolation curve.
  std::uint8_t easing_mode;
  std::uint8_t elapsed_frames;
  // Zero while the layer is idle.
  std::uint8_t duration_frames;
  // The initial screen offset and the change to the target.
  std::int16_t start_x;
  std::int16_t start_y;
  std::int16_t delta_x;
  std::int16_t delta_y;
  // The initial layer scale in 4.12 fixed point and the change to the target.
  std::int16_t start_scale_fixed12;
  std::int16_t delta_scale_fixed12;
  std::int16_t start_brightness;
  std::int16_t delta_brightness;
};
inline constexpr std::size_t kSplashTweenRecordBytes = 0x14;
static_assert(sizeof(SplashTweenRecord) == kSplashTweenRecordBytes);
}  // namespace VS::Event
