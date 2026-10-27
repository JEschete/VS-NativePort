// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORLOADING_H_TYPES
#define VS_ACTORS_ACTORLOADING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_ACTORLOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ACTORLOADING_H_DECLS)
#define VS_ACTORS_ACTORLOADING_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/ActorModelSlot.h"
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
    int index, int actorId, Field_ActorPlacement* placement, int material, int arg4);
void Actors_ApplyActorInitialStats(
    int actor_id, Battle_ActorStats* actor, Actors_CharacterInit* init, int randomize_stats);
void Actors_InitializeActorDefaults(u_int id, int model_id, int blade_id, int shield_id, int spawn_flags);
void Actors_InitializeActorFromTemplate(
    int id, Actors_CharacterInit* character_init, int blade_id, int shield_id, int spawn_flags, int randomize_stats);
int Actors_QueueActorModelLoad(Battle_Actor* actor, int index, int wepId,
    Field_ActorPlacement* placement, int load_flags);
void Actors_QueueEquipmentModelLoad(
    Battle_Actor* actor, int actorId, int wepId, int isShield, int material);
Actors_ActorAllocation* Actors_AllocateActorWithEquipment(int actor_id, int model_id, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* placement, int load_flags);
void Actors_RegisterLoadedActor(
    Battle_Actor* actor, int actor_id, int model_id, int blade_id, int shield_id, int spawn_flags);
Battle_Actor* Actors_CreateActorWithEquipment(int actor_id, int model_id, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* placement, int spawn_flags);
Battle_Actor* Actors_SpawnRoomEnemy(
    int id, Field_MpdEnemySpawn* spawn, Actors_CharacterInit* init, int flags);
int Actors_ActorLoadProbeResult(void);
int Actors_QueueAshleyModelLoad(int model_id);
int Actors_QueueScriptedActorSpawn(u_int actor_id, int model_id, Field_ActorPlacement* placement);
void Actors_LoadActorEquipmentModels(int actorId, int bladeWepId, int shieldWepId);
void Actors_RefreshActorEquipmentModels(int actor_id);
int Actors_FirstFreeAssetLoadSlot(void);
void Actors_RemoveScriptedActor(int actor_id);

#endif // VS_ACTORS_ACTORLOADING_H_DECLS
