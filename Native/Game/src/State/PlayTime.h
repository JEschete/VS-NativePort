// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_PLAYTIME_H_TYPES
#define VS_STATE_PLAYTIME_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_STATE_PLAYTIME_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_PLAYTIME_H_DECLS)
#define VS_STATE_PLAYTIME_H_DECLS
#include "State/GameTime.h"

extern State_PlayTime State_playTime;
int State_UpdatePlayTimeAndLoading(int vsync);

#endif // VS_STATE_PLAYTIME_H_DECLS
