// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORSTATE_H_TYPES
#define VS_ACTORS_ACTORSTATE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_ACTORSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ACTORSTATE_H_DECLS)
#define VS_ACTORS_ACTORSTATE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Field/Actors.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern u_int Actors_stateFlagsScratch;
void Actors_EnableActorModel(int arg0);
void Actors_EnableAllActorModels(void);
void Actors_DisableActorModel(int arg0);
void Actors_DisableAllActorModels(void);
void Actors_SetActorHostile(int arg0, int arg1);
int Actors_ActorHpPercentage(int arg0);
void Actors_RestoreActorVitals(int arg0);
int Actors_FindActorByModelId(int arg0);
Battle_Actor* Actors_ActorAtSlot(int actorId);
int Actors_CountActorsByDisposition(int arg0);
char Actors_IsActorRetainedAfterDefeat(int arg0);
int Actors_SetActorLogicEnabled(int arg0, int arg1);
int Actors_SetOtherActorLogicEnabled(int arg0);
void Actors_SetActorDetached(int arg0, int arg1);
void Actors_FinishActorPlacement(void);
void Actors_ResetActorMotionState(int arg0, int arg1);
void Actors_ResetAllActorMovement(void);
void Actors_BeginActorDeath(int actorId, void* context, int animate);
void Actors_SetActorCompanion(int arg0, int arg1, Render_ColorCode* arg2, Render_ColorCode* arg3);
void Actors_AttachActorToParentBone(int arg0, int arg1, int arg2, int arg3);
int Actors_ReleaseActorAttachmentLock(int arg0, int arg1);
void Actors_SetActorDisplayFlags(int actorId, int arg1);
u_int Actors_ActorStateFlags(int actorId);
int Actors_ActorPlacement(int arg0, void* arg1);
void Actors_PlaceActorInRoom(int actorId, Field_ActorPlacement* placement, int height, int mode);
int Actors_FindActorAtFlightPosition(int arg0, SVECTOR* arg1);
void Actors_SeparateActorFlightPlacement(int arg0, SVECTOR* arg1);
void Actors_ActorSavedPlacement(int arg0, int arg1, Actors_ActorLocationQuery* arg2);
int Actors_FindNearestActorAhead(int arg0, int arg1, int arg2);

#endif // VS_ACTORS_ACTORSTATE_H_DECLS
