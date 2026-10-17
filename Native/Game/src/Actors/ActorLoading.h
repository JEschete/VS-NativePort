// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORLOADING_H_TYPES
#define VS_ACTORS_ACTORLOADING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_ACTORLOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ACTORLOADING_H_DECLS)
#define VS_ACTORS_ACTORLOADING_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/CharacterInit.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Field/Actors.h"
#include "Field/Mpd.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

extern Actors_CharacterInit* Actors_zoneCharacterInitData;
Battle_Actor* Actors_CreateAuxiliaryActor(
    int index, int actorId, Field_ActorPlacement* arg2, int material, int arg4);
void Actors_ApplyActorInitialStats(
    int arg0, Battle_ActorStats* actor, Actors_CharacterInit* init, int arg3);
void Actors_InitializeActorDefaults(u_int id, int arg1, int arg2, int arg3, int arg4);
void Actors_InitializeActorFromTemplate(
    int id, Actors_CharacterInit* arg1, int arg2, int arg3, int arg4, int arg5);
int Actors_QueueActorModelLoad(Battle_Actor* arg0, int index, int wepId,
    Field_ActorPlacement* arg3, int arg4);
void Actors_QueueEquipmentModelLoad(
    Battle_Actor* actor, int actorId, int wepId, int isShield, int material);
Actors_ActorAllocation* Actors_AllocateActorWithEquipment(int arg0, int arg1, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* arg6, int arg7);
void Actors_RegisterLoadedActor(
    Battle_Actor* arg0, int arg1, int arg2, int arg3, int arg4, int arg5);
Battle_Actor* Actors_CreateActorWithEquipment(int arg0, int arg1, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* arg6, int arg7);
Battle_Actor* Actors_SpawnRoomEnemy(
    int id, Field_MpdEnemySpawn* spawn, Actors_CharacterInit* init, int flags);
int Actors_ActorLoadProbeResult(void);
int Actors_QueueAshleyModelLoad(int arg0);
int Actors_QueueScriptedActorSpawn(u_int arg0, int arg1, Field_ActorPlacement* arg2);
void Actors_LoadActorEquipmentModels(int actorId, int bladeWepId, int shieldWepId);
void Actors_RefreshActorEquipmentModels(int arg0);
int Actors_FirstFreeAssetLoadSlot(void);
void Actors_RemoveScriptedActor(int arg0);

#endif // VS_ACTORS_ACTORLOADING_H_DECLS
