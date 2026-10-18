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
intptr_t Event_CommandPlayActorEffect(u_char* arg0, short arg1);
intptr_t Event_CommandAdvanceEffectStage(u_char* arg0, short arg1);
intptr_t Event_CommandWaitEffectState(u_char* arg0, short arg1);
intptr_t Event_CommandYieldEffectRelease(u_char* arg0, short arg1);
intptr_t Event_CommandWaitEffectContinuation(u_char* arg0, short arg1);
intptr_t Event_CommandWaitEffectRelease(u_char* arg0, short arg1);
intptr_t Event_CommandMarkEffectStageReady(u_char* arg0, short arg1);
intptr_t Event_CommandPlayVibrationPattern(u_char* arg0, short arg1);
intptr_t Event_CommandSetActorTrail(u_char* arg0, short arg1);
intptr_t Event_CommandWaitScreenEffectLoad(u_char* arg0, short arg1);
intptr_t Event_CommandSetScreenDistortion(u_char* arg0, short arg1);
intptr_t Event_CommandLoadScreenEffects(u_char* arg0, short arg1);
intptr_t Event_CommandReservedScreenEffect(u_char* arg0, short arg1);
intptr_t Event_CommandFreeScreenEffects(u_char* arg0, short arg1);
intptr_t Event_CommandSetScanlineOverlay(u_char* arg0, short arg1);
void Event_DrawEventScanlineOverlay(void);
void Event_UpdateEventImageTweens(void);
void Event_UpdateEventWeatherTween(void);

#endif // VS_EVENT_SCRIPTEFFECTS_H_DECLS
