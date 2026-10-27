// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOMSTATE_H_TYPES
#define VS_FIELD_ROOMSTATE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_ROOMSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOMSTATE_H_DECLS)
#define VS_FIELD_ROOMSTATE_H_DECLS

extern int Field_isRoomVisitPending;
void Field_MarkCurrentRoomVisited(void);
void Field_ClearSavedRoomEnemies(int mapId, int zoneId);
void Field_ShowPrimaryRoomGeometry(void);
void Field_HidePrimaryRoomGeometry(void);
void Field_CancelPendingRoomVisit(void);
int Field_FindLocationId(int zoneId, int mapId);
int Field_CurrentRoomId(void);
void Field_InitializeRoomRuntimeState(void);

#endif // VS_FIELD_ROOMSTATE_H_DECLS
