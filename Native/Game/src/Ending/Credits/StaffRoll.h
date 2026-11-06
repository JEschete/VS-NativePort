// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ENDING_CREDITS_STAFFROLL_H_TYPES
#define VS_ENDING_CREDITS_STAFFROLL_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ENDING_CREDITS_STAFFROLL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ENDING_CREDITS_STAFFROLL_H_DECLS)
#define VS_ENDING_CREDITS_STAFFROLL_H_DECLS
#include "Ending/Credits.h"

extern u_short Ending_Credits_illustrationHoldFrames[16];
extern int Ending_Credits_ripplePhaseAngle;
extern int Ending_Credits_rippleScaleFrame;
extern u_char Ending_Credits_rippleDistanceGrid[4][16];
extern short Ending_Credits_illustrationBrightness;
extern Ending_Credits_Element Ending_Credits_elements[32];
extern u_char Ending_Credits_hasFinished;
extern u_char Ending_Credits_isBackgroundFading;
extern u_char Ending_Credits_wasSkipRequested;
extern int Ending_Credits_elapsedFrames;
extern int Ending_Credits_scriptDelayFrames;
extern u_char* Ending_Credits_scriptCursor;
extern short Ending_Credits_scrollSpeedFixed8;
extern u_short Ending_Credits_backgroundBrightness;
extern u_int Ending_Credits_xaResumeFrameCount;

#endif // VS_ENDING_CREDITS_STAFFROLL_H_DECLS
