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
int Event_LoadRoomEventSections(void* clearedSection, u_short clearedSectionLen,
    void* scriptSection, u_short scriptSectionLen, void* doorSection,
    u_short doorSectionLen, int startState);
void Event_FreeRoomEventSections(void);
void Event_ReleaseZoneEventResources(void);
short Event_EvaluateRoomEventTriggers(int arg0, int arg1, u_char* arg2, int arg3);

#endif // VS_EVENT_TRIGGERS_H_DECLS
