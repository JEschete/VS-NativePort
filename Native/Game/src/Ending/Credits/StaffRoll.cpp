// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Ending/Credits/StaffRoll.hpp"

// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Ending::Credits::illustrationHoldFrames
    [VS::Ending::Credits::kIllustrationCount];

// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::hasFinished;

// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::wasSkipRequested;

// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
std::uint32_t VS::Ending::Credits::xaResumeFrameCount;
