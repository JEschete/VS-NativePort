// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_PLAYBACK_H_TYPES
#define VS_EVENT_PLAYBACK_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_PLAYBACK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_PLAYBACK_H_DECLS)
#define VS_EVENT_PLAYBACK_H_DECLS
#include "DiscQueue.h"

extern short* Event_eventFileDiscRequest;
extern Content_DiscFile Event_eventFileDiscLocation;

#endif // VS_EVENT_PLAYBACK_H_DECLS
