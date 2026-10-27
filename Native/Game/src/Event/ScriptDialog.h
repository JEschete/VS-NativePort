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
intptr_t Event_ScriptInitDialogBox(u_char* script, short thread_index __attribute__((unused)));
intptr_t Event_ScriptShowDialogText(u_char* script, short thread_index __attribute__((unused)));
intptr_t Event_ScriptDismissDialogBox(u_char* instruction, short thread_index);
int Event_CalcDialogAnchorPlacement(
    SVECTOR* origin, u_char* script, Event_DialogPlacement* result, u_char direction);
intptr_t Event_ScriptInitActorDialogBox(u_char* script, short thread_index __attribute__((unused)));
intptr_t Event_ScriptWaitForDialogDismissal(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetDialogInteger(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetDialogTransitionDuration(u_char* instruction, short thread_index);
intptr_t Event_ScriptSelectDialogChoice(u_char* script, short thread_index __attribute__((unused)));
intptr_t Event_ScriptSetDialogSpeaker(u_char* instruction, short thread_index);

#endif // VS_EVENT_SCRIPTDIALOG_H_DECLS
