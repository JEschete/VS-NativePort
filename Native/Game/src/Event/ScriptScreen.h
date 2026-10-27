// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTSCREEN_H_TYPES
#define VS_EVENT_SCRIPTSCREEN_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTSCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTSCREEN_H_DECLS)
#define VS_EVENT_SCRIPTSCREEN_H_DECLS
#include "Event/Splash.h"

extern Event_SplashTween Event_splashTweens[6];
intptr_t Event_ScriptWaitForSplashFile(u_char* instruction, short thread_index);
intptr_t Event_ScriptLoadSplashImage(u_char* instruction, short thread_index);
intptr_t Event_ScriptConfigureSplashLayer(u_char* instruction, short thread_index);
intptr_t Event_ScriptTweenSplashLayer(u_char* instruction, short thread_index);
intptr_t Event_ScriptFreeSplashImage(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForSplashTween(u_char* instruction, short thread_index);
intptr_t Event_ScriptFadeOutAndWait(u_char* instruction, short thread_index);
intptr_t Event_ScriptFadeIn(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetWipeAngle(u_char* instruction, short thread_index);
intptr_t Event_ScriptConfigureScreenTransition(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetRoomDisplayFlags(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForScreenTransition(u_char* instruction, short thread_index);
intptr_t Event_ScriptTogglePaletteHook(u_char* instruction, short thread_index);
intptr_t Event_ScriptTransformPalettes(u_char* instruction, short thread_index);

#endif // VS_EVENT_SCRIPTSCREEN_H_DECLS
