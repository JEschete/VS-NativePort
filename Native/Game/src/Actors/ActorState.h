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
void Actors_EnableActorModel(int actor_id);
void Actors_EnableAllActorModels(void);
void Actors_DisableActorModel(int actor_id);
void Actors_DisableAllActorModels(void);
void Actors_SetActorHostile(int actor_id, int hostile);
int Actors_ActorHpPercentage(int actor_id);
void Actors_RestoreActorVitals(int actor_id);
int Actors_FindActorByModelId(int model_id);
Battle_Actor* Actors_ActorAtSlot(int actorId);
int Actors_CountActorsByDisposition(int actor_kind);
char Actors_IsActorRetainedAfterDefeat(int actor_id);
int Actors_SetActorLogicEnabled(int actor_id, int enabled);
int Actors_SetOtherActorLogicEnabled(int enabled);
void Actors_SetActorDetached(int actor_id, int detached);
void Actors_FinishActorPlacement(void);
void Actors_ResetActorMotionState(int actor_id, int arg1);
void Actors_ResetAllActorMovement(void);
void Actors_BeginActorDeath(int actorId, void* context, int animate);
void Actors_SetActorCompanion(int actor_id, int companion_slot, Render_ColorCode* companion_state, Render_ColorCode* companion_color);
void Actors_AttachActorToParentBone(int actor_id, int parent_actor_id, int parent_bone_index, int arg3);
int Actors_ReleaseActorAttachmentLock(int actor_id, int arg1);
void Actors_SetActorDisplayFlags(int actorId, int display_flags);
u_int Actors_ActorStateFlags(int actorId);
int Actors_ActorPlacement(int actor_id, void* placement_out);
void Actors_PlaceActorInRoom(int actorId, Field_ActorPlacement* placement, int height, int mode);
int Actors_FindActorAtFlightPosition(int actor_id, SVECTOR* position);
void Actors_SeparateActorFlightPlacement(int actor_id, SVECTOR* position);
void Actors_ActorSavedPlacement(int actor_id, int movement_class, Actors_ActorLocationQuery* placement_out);
int Actors_FindNearestActorAhead(int actor_id, int horizontal_range, int vertical_range);

#endif // VS_ACTORS_ACTORSTATE_H_DECLS
