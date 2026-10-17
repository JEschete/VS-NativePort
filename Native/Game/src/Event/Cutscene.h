// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_CUTSCENE_H_TYPES
#define VS_EVENT_CUTSCENE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_CUTSCENE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_CUTSCENE_H_DECLS)
#define VS_EVENT_CUTSCENE_H_DECLS

void Event_PrepareCutsceneActors(void);
void Event_StartPreparedCutscene(void);
void Event_BeginQueuedCutscene(void);
void Event_EndFieldCutscene(void);

#endif // VS_EVENT_CUTSCENE_H_DECLS
