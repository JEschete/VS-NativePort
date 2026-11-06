// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_TRIGGERS_H_TYPES
#define VS_EVENT_TRIGGERS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_TRIGGERS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_TRIGGERS_H_DECLS)
#define VS_EVENT_TRIGGERS_H_DECLS

extern char Event_isDirectEventRequest;
extern void* Event_roomEventTriggers;
extern void* Event_roomDoorEventData;
extern short Event_hasRoomEntryScript;
void Event_ReleaseZoneEventResources(void);

#endif // VS_EVENT_TRIGGERS_H_DECLS
