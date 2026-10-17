// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTSCREEN_H_TYPES
#define VS_EVENT_SCRIPTSCREEN_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTSCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTSCREEN_H_DECLS)
#define VS_EVENT_SCRIPTSCREEN_H_DECLS
#include "Event/Splash.h"

extern Event_SplashTween Event_splashTweens[6];
int Event_ScriptWaitForSplashFile(u_char* arg0, short arg1);
int Event_ScriptLoadSplashImage(u_char* arg0, short arg1);
int Event_ScriptConfigureSplashLayer(u_char* arg0, short arg1);
int Event_ScriptTweenSplashLayer(u_char* arg0, short arg1);
int Event_ScriptFreeSplashImage(u_char* arg0, short arg1);
int Event_ScriptWaitForSplashTween(u_char* arg0, short arg1);
int Event_ScriptFadeOutAndWait(u_char* arg0, short arg1);
int Event_ScriptFadeIn(u_char* arg0);
int Event_ScriptSetWipeAngle(u_char* arg0, short arg1);
int Event_ScriptConfigureScreenTransition(u_char* arg0, short arg1);
int Event_ScriptSetRoomDisplayFlags(u_char* arg0, short arg1);
int Event_ScriptWaitForScreenTransition(void);
int Event_ScriptTogglePaletteHook(u_char* arg0, short arg1);
int Event_ScriptTransformPalettes(u_char* arg0, short arg1);

#endif // VS_EVENT_SCRIPTSCREEN_H_DECLS
