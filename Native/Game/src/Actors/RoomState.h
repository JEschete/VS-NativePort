// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ROOMSTATE_H_TYPES
#define VS_ACTORS_ROOMSTATE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_ROOMSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ROOMSTATE_H_DECLS)
#define VS_ACTORS_ROOMSTATE_H_DECLS

void Actors_ClearRoomActorStateCache(void);
void Actors_ClearCachedRoomActorStates(int mapId, int zndId);
void* Actors_FindCachedRoomActorState(int arg0);
void* Actors_AcquireCachedRoomActorState(int arg0);
void Actors_StoreRoomActorState(int arg0, int arg1);

#endif // VS_ACTORS_ROOMSTATE_H_DECLS
