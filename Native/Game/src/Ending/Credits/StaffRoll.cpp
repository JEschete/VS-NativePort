// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Ending/Credits/StaffRoll.hpp"

// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
std::uint16_t VS::Ending::Credits::illustrationHoldFrames[VS::Ending::Credits::kIllustrationCount]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBB18 (D_800DBB18@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::ripplePhaseAngle;

// decomp: D_800DBB1C (D_800DBB1C@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::rippleScaleFrame;

// decomp: D_800DBB28 (D_800DBB28@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::rippleDistanceGrid[VS::Ending::Credits::kRippleRows][VS::Ending::Credits::kRippleColumns];

// decomp: D_800DBB68 (D_800DBB68@ENDING/ENDING.PRG)
std::int16_t VS::Ending::Credits::illustrationBrightness;

// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::hasFinished;

// decomp: D_800DC189 (D_800DC189@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::isBackgroundFading;

// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::wasSkipRequested;

// decomp: D_800DC18C (D_800DC18C@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::elapsedFrames;

// decomp: _textStreamDelay (_textStreamDelay@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::scriptDelayFrames;

// decomp: _creditsBytestreamHead (_creditsBytestreamHead@ENDING/ENDING.PRG)
std::uint8_t* VS::Ending::Credits::scriptCursor;

// decomp: _scrollSpeed (_scrollSpeed@ENDING/ENDING.PRG)
std::int16_t VS::Ending::Credits::scrollSpeedFixed8;

// decomp: D_800DC19C (D_800DC19C@ENDING/ENDING.PRG)
std::uint16_t VS::Ending::Credits::backgroundBrightness;

// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
std::uint32_t VS::Ending::Credits::xaResumeFrameCount;

