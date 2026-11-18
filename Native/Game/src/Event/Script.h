// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_ScriptBytePair;

// decomp: scriptBytePair (src/BATTLE/BATTLE.PRG/4A0A8.c:233)
struct Event_ScriptBytePair {
    u_char lowByte;
    u_char highByte;
};
static_assert(sizeof(Event_ScriptBytePair) == 0x2, "Event_ScriptBytePair must keep its PS1 layout");

extern u_char Event_specialSubroutineScript[468];
extern intptr_t (**Event_legacySkipHandlerCopy)(u_char*, short);
