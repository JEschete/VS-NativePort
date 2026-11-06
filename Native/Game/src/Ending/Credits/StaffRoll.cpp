// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Ending/Credits.h"
#include "Ending/Credits/StaffRoll.h"

// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
u_short Ending_Credits_illustrationHoldFrames[16]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBB18 (D_800DBB18@ENDING/ENDING.PRG)
int Ending_Credits_ripplePhaseAngle;

// decomp: D_800DBB1C (D_800DBB1C@ENDING/ENDING.PRG)
int Ending_Credits_rippleScaleFrame;

// decomp: D_800DBB28 (D_800DBB28@ENDING/ENDING.PRG)
u_char Ending_Credits_rippleDistanceGrid[4][16];

// decomp: D_800DBB68 (D_800DBB68@ENDING/ENDING.PRG)
short Ending_Credits_illustrationBrightness;

// decomp: _creditsElements (_creditsElements@ENDING/ENDING.PRG)
Ending_Credits_Element Ending_Credits_elements[32];

// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
u_char Ending_Credits_hasFinished;

// decomp: D_800DC189 (D_800DC189@ENDING/ENDING.PRG)
u_char Ending_Credits_isBackgroundFading;

// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
u_char Ending_Credits_wasSkipRequested;

// decomp: D_800DC18C (D_800DC18C@ENDING/ENDING.PRG)
int Ending_Credits_elapsedFrames;

// decomp: _textStreamDelay (_textStreamDelay@ENDING/ENDING.PRG)
int Ending_Credits_scriptDelayFrames;

// decomp: _creditsBytestreamHead (_creditsBytestreamHead@ENDING/ENDING.PRG)
u_char* Ending_Credits_scriptCursor;

// decomp: _scrollSpeed (_scrollSpeed@ENDING/ENDING.PRG)
short Ending_Credits_scrollSpeedFixed8;

// decomp: D_800DC19C (D_800DC19C@ENDING/ENDING.PRG)
u_short Ending_Credits_backgroundBrightness;

// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
u_int Ending_Credits_xaResumeFrameCount;

}  // extern "C"
