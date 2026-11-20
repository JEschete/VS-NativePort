#pragma once
// The event script VM's splash image layer tweens. ScriptScreen.cpp defines
// them.

#include <cstddef>

#include "Event/Splash.hpp"

namespace VS::Event {
// The copier's extent, not a capacity: the original runs three layers
// (kImageTweenCount) in the 64 bytes before D_800F4B70, but LiveSplashTween
// and SetLiveSplashTween accept any layer below this extent.
inline constexpr std::size_t kSplashTweenExtent = 6;

// decomp: gimTweens (D_800F4B30@BATTLE/BATTLE.PRG)
extern SplashTweenRecord splashTweens[kSplashTweenExtent];
}  // namespace VS::Event
