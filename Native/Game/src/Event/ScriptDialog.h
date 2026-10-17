// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTDIALOG_H_TYPES
#define VS_EVENT_SCRIPTDIALOG_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTDIALOG_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTDIALOG_H_DECLS)
#define VS_EVENT_SCRIPTDIALOG_H_DECLS
#include "Battle/TextBoxes.h"
#include "Event/Dialog.h"

extern u_char Event_kDialogPlacementOrder[324];
int Event_ScriptInitDialogBox(u_char* script, short arg1 __attribute__((unused)));
int Event_ScriptShowDialogText(u_char* script, short arg1 __attribute__((unused)));
int Event_ScriptDismissDialogBox(u_char* arg0, short arg1);
int Event_CalcDialogAnchorPlacement(
    SVECTOR* origin, u_char* script, Event_DialogPlacement* result, u_char direction);
int Event_ScriptInitActorDialogBox(u_char* script, short arg1 __attribute__((unused)));
int Event_ScriptWaitForDialogDismissal(u_char* arg0, short arg1);
int Event_ScriptSetDialogInteger(u_char* arg0, short arg1);
int Event_ScriptSetDialogTransitionDuration(u_char* arg0, short arg1);
int Event_ScriptSelectDialogChoice(u_char* script, short arg1 __attribute__((unused)));
int Event_ScriptSetDialogSpeaker(u_char* arg0, short arg1);

#endif // VS_EVENT_SCRIPTDIALOG_H_DECLS
