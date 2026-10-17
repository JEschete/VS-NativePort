// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTEFFECTS_H_TYPES
#define VS_EVENT_SCRIPTEFFECTS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTEFFECTS_H_DECLS)
#define VS_EVENT_SCRIPTEFFECTS_H_DECLS

extern u_short Event_scanlineOverlayFrame;
extern u_char Event_isEffectWaitPending;
extern void* Event_vibrationPatternBuffer;
int Event_CommandPlayActorEffect(u_char* arg0, short arg1);
int Event_CommandAdvanceEffectStage(u_char* arg0, short arg1);
int Event_CommandWaitEffectState(u_char* arg0, short arg1);
int Event_CommandYieldEffectRelease(u_char* arg0, short arg1);
int Event_CommandWaitEffectContinuation(u_char* arg0, short arg1);
int Event_CommandWaitEffectRelease(u_char* arg0, short arg1);
int Event_CommandMarkEffectStageReady(u_char* arg0, short arg1);
int Event_CommandPlayVibrationPattern(u_char* arg0);
int Event_CommandSetActorTrail(u_char* arg0, short arg1);
int Event_CommandWaitScreenEffectLoad(u_char* arg0, short arg1);
int Event_CommandSetScreenDistortion(u_char* arg0, short arg1);
int Event_CommandLoadScreenEffects(u_char* arg0, short arg1);
int Event_CommandReservedScreenEffect(u_char* arg0, short arg1);
int Event_CommandFreeScreenEffects(u_char* arg0, short arg1);
int Event_CommandSetScanlineOverlay(u_char* arg0, short arg1);
void Event_DrawEventScanlineOverlay(void);
void Event_UpdateEventImageTweens(void);
void Event_UpdateEventWeatherTween(void);

#endif // VS_EVENT_SCRIPTEFFECTS_H_DECLS
