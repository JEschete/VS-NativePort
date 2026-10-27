// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_FLAGS_H_TYPES
#define VS_STATE_FLAGS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_STATE_FLAGS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_FLAGS_H_DECLS)
#define VS_STATE_FLAGS_H_DECLS

void State_ResetZoneScriptFlags(int preserve_zone_flags);
char State_ScriptFlag(short flag_id);
void State_SetScriptFlag(short id, char value);

#endif // VS_STATE_FLAGS_H_DECLS
