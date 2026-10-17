// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_TIMERS_H_TYPES
#define VS_STATE_TIMERS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_STATE_TIMERS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_TIMERS_H_DECLS)
#define VS_STATE_TIMERS_H_DECLS
#include "State/SceneTimer.h"

extern State_SceneTimer State_sceneTimer;
extern int State_tickSpeed;
void State_StartSceneTimer(int arg0, int arg1, int arg2);
void State_ResetSceneTimer(void);
void State_SetSceneTimerPaused(int arg0);
int State_SetTickSpeed(int arg0);
int State_UpdateEventTimer(void);

#endif // VS_STATE_TIMERS_H_DECLS
