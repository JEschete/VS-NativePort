// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTFIELD_H_TYPES
#define VS_EVENT_SCRIPTFIELD_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTFIELD_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTFIELD_H_DECLS)
#define VS_EVENT_SCRIPTFIELD_H_DECLS
#include "Event/Weather.h"

extern u_char Event_kMenuTransitionModes[320];
extern Event_WeatherTween Event_weatherTween;
int Event_ScriptPreloadRoom(u_char* arg0, short arg1);
int Event_ScriptSetHudVisible(u_char* arg0, short arg1);
int Event_ScriptRunMenuTransition(u_char* script, short arg1);
int Event_ScriptSetGimmickVisibility(u_char* arg0, short arg1);
int Event_ScriptOverrideDepthCueDistance(u_char* arg0, short arg1);
int Event_ScriptSetRoomLightingMode(u_char* arg0, short arg1);
int Event_ScriptShowGeometryGroup(u_char* arg0, short arg1);
int Event_ScriptHideGeometryGroup(u_char* arg0, short arg1);
int Event_ScriptSwapGeometryGroupFlags(u_char* arg0, short arg1);
int Event_ScriptSetCollisionTileMode(u_char* arg0, short arg1);
int Event_ScriptChangeRoom(u_char* arg0, short arg1);
int Event_ScriptSetRoomBackgroundVisible(u_char* arg0, short arg1);
int Event_ScriptLoadRoomBackground(u_char* arg0, short arg1);
int Event_ScriptWaitForRoomBackground(u_char* arg0, short arg1);
int Event_ScriptFreeRoomBackground(u_char* arg0, short arg1);
int Event_ScriptSetEnvironmentParticles(u_char* arg0, short arg1);
int Event_ScriptSetParticleVariationEnabled(u_char* arg0, short arg1);
int Event_ScriptMuteEnvironmentSound(u_char* arg0, short arg1);
int Event_ScriptControlRoomAnimation(u_char* arg0, short arg1);
int Event_ScriptSetBackgroundOffset(u_char* arg0, short arg1);
int Event_ScriptSetFirstPersonView(u_char* arg0, short arg1);
int Event_ScriptAdjustGeometryDepthBias(u_char* arg0, short arg1);
int Event_ScriptSetBackgroundParallax(u_char* arg0, short arg1);

#endif // VS_EVENT_SCRIPTFIELD_H_DECLS
