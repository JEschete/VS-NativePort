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
intptr_t Event_ScriptPreloadRoom(u_char* arg0, short arg1);
intptr_t Event_ScriptSetHudVisible(u_char* arg0, short arg1);
intptr_t Event_ScriptRunMenuTransition(u_char* script, short arg1);
intptr_t Event_ScriptSetGimmickVisibility(u_char* arg0, short arg1);
intptr_t Event_ScriptOverrideDepthCueDistance(u_char* arg0, short arg1);
intptr_t Event_ScriptSetRoomLightingMode(u_char* arg0, short arg1);
intptr_t Event_ScriptShowGeometryGroup(u_char* arg0, short arg1);
intptr_t Event_ScriptHideGeometryGroup(u_char* arg0, short arg1);
intptr_t Event_ScriptSwapGeometryGroupFlags(u_char* arg0, short arg1);
intptr_t Event_ScriptSetCollisionTileMode(u_char* arg0, short arg1);
intptr_t Event_ScriptChangeRoom(u_char* arg0, short arg1);
intptr_t Event_ScriptSetRoomBackgroundVisible(u_char* arg0, short arg1);
intptr_t Event_ScriptLoadRoomBackground(u_char* arg0, short arg1);
intptr_t Event_ScriptWaitForRoomBackground(u_char* arg0, short arg1);
intptr_t Event_ScriptFreeRoomBackground(u_char* arg0, short arg1);
intptr_t Event_ScriptSetEnvironmentParticles(u_char* arg0, short arg1);
intptr_t Event_ScriptSetParticleVariationEnabled(u_char* arg0, short arg1);
intptr_t Event_ScriptMuteEnvironmentSound(u_char* arg0, short arg1);
intptr_t Event_ScriptControlRoomAnimation(u_char* arg0, short arg1);
intptr_t Event_ScriptSetBackgroundOffset(u_char* arg0, short arg1);
intptr_t Event_ScriptSetFirstPersonView(u_char* arg0, short arg1);
intptr_t Event_ScriptAdjustGeometryDepthBias(u_char* arg0, short arg1);
intptr_t Event_ScriptSetBackgroundParallax(u_char* arg0, short arg1);

#endif // VS_EVENT_SCRIPTFIELD_H_DECLS
