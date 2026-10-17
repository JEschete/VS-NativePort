// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ASHLEY_H_TYPES
#define VS_ACTORS_ASHLEY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_ASHLEY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ASHLEY_H_DECLS)
#define VS_ACTORS_ASHLEY_H_DECLS

void Actors_AshleySpawnCompletionHook(void);
int Actors_AshleyModelLoadQueueSlot(void);
void Actors_RestoreAshleyRuntimeState(void);
void Actors_SpawnAshleyWithEquipment(void);

#endif // VS_ACTORS_ASHLEY_H_DECLS
