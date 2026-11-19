#pragma once
// ENDING.PRG's staff roll state, which EndingLive.cpp publishes from the
// core's Credits::State at every frame yield.
#include <cstddef>
#include <cstdint>

#include "Ending/Credits/StaffRollCore.hpp"

namespace VS::Ending::Credits {
// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
extern std::uint16_t illustrationHoldFrames[kIllustrationCount];
// decomp: D_800DBB18 (D_800DBB18@ENDING/ENDING.PRG)
extern std::int32_t ripplePhaseAngle;
// decomp: D_800DBB1C (D_800DBB1C@ENDING/ENDING.PRG)
extern std::int32_t rippleScaleFrame;
// decomp: D_800DBB28 (D_800DBB28@ENDING/ENDING.PRG)
extern std::uint8_t rippleDistanceGrid[kRippleRows][kRippleColumns];
// decomp: D_800DBB68 (D_800DBB68@ENDING/ENDING.PRG)
extern std::int16_t illustrationBrightness;
// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
extern std::uint8_t hasFinished;
// decomp: D_800DC189 (D_800DC189@ENDING/ENDING.PRG)
extern std::uint8_t isBackgroundFading;
// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
extern std::uint8_t wasSkipRequested;
// decomp: D_800DC18C (D_800DC18C@ENDING/ENDING.PRG)
extern std::int32_t elapsedFrames;
// decomp: _textStreamDelay (_textStreamDelay@ENDING/ENDING.PRG)
extern std::int32_t scriptDelayFrames;
// decomp: _creditsBytestreamHead (_creditsBytestreamHead@ENDING/ENDING.PRG)
extern std::uint8_t* scriptCursor;
// decomp: _scrollSpeed (_scrollSpeed@ENDING/ENDING.PRG)
extern std::int16_t scrollSpeedFixed8;
// decomp: D_800DC19C (D_800DC19C@ENDING/ENDING.PRG)
extern std::uint16_t backgroundBrightness;
// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
extern std::uint32_t xaResumeFrameCount;
}  // namespace VS::Ending::Credits
