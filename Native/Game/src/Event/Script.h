// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPT_H_TYPES
#define VS_EVENT_SCRIPT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_ScriptBytePair Event_ScriptBytePair;

// decomp: scriptBytePair (src/BATTLE/BATTLE.PRG/4A0A8.c:233)
typedef struct Event_ScriptBytePair {
    u_char lowByte;
    u_char highByte;
} Event_ScriptBytePair;
_Static_assert(sizeof(Event_ScriptBytePair) == 0x2, "Event_ScriptBytePair must keep its PS1 layout");

#endif // VS_EVENT_SCRIPT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPT_H_DECLS)
#define VS_EVENT_SCRIPT_H_DECLS

extern u_char Event_specialSubroutineScript[468];
extern int (**Event_legacySkipHandlerCopy)(u_char*, short);

#endif // VS_EVENT_SCRIPT_H_DECLS
