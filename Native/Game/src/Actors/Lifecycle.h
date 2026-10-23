// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_LIFECYCLE_H_TYPES
#define VS_ACTORS_LIFECYCLE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_LIFECYCLE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_LIFECYCLE_H_DECLS)
#define VS_ACTORS_LIFECYCLE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

int Actors_LinkActorAfterAshley(Battle_Actor* actor);
int Actors_UnlinkActorFromRoomList(Battle_Actor* actor);
int Actors_RemoveActorAtSlot(u_int index, int arg1);
void Actors_FreezeAllActorAnimations(void);
void Actors_UnfreezeAllActorAnimations(void);
void Actors_InitializePlaceholderActorStats(int arg0, int arg1);

#endif // VS_ACTORS_LIFECYCLE_H_DECLS
