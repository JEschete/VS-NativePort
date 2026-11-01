// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <cstdint>

#include "Actors/SoundTransition.hpp"

namespace VS::Actors {
// decomp: BATTLE.PRG func_800AEEC4 (src/BATTLE/BATTLE.PRG/44F14.c:886; MIPS
// 800AEEC4-800AEF90).
SoundTransition StepSoundTransition(SoundTransition current,
                                    SoundTransition target, int tick_speed) {
  const int remaining = current.frames_remaining;
  if (current.mode != kSoundTransitionGliding || remaining == 0) {
    return current;
  }
  const int step = std::min(tick_speed / 2, remaining);
  const auto glide = [step, remaining](std::uint8_t from, std::uint8_t to) {
    return static_cast<std::uint8_t>(from + ((to - from) * step / remaining));
  };
  current.volume = glide(current.volume, target.volume);
  current.pan = glide(current.pan, target.pan);
  current.frames_remaining = static_cast<std::uint8_t>(remaining - step);
  return current;
}
}  // namespace VS::Actors