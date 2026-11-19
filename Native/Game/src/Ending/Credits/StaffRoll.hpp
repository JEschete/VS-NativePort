#pragma once
// ENDING.PRG's staff roll globals that outlive one staff roll: the module
// image's illustration hold frames, which seed Credits::State, and the
// finished and skip flags that the scene inspection and the Combat Lab's
// scene trials read.
#include <cstddef>
#include <cstdint>

#include "Ending/Credits/StaffRollCore.hpp"

namespace VS::Ending::Credits {
// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
extern std::uint16_t illustrationHoldFrames[kIllustrationCount];
// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
extern std::uint8_t hasFinished;
// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
extern std::uint8_t wasSkipRequested;
// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
extern std::uint32_t xaResumeFrameCount;
}  // namespace VS::Ending::Credits
