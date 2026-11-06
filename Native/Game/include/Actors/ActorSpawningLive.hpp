#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ActorSpawning.hpp"

struct Actors_CharacterInit;
struct Actors_RoomActorRecord;
struct Battle_Actor;
struct Field_ActorPlacement;
struct Field_MpdEnemySpawn;

// Bindings from Battle_actors, the actor allocations, Field_zoneContext, the
// room enemy memory, Ashley's snapshot (State_ashleySnapshot) and the
// inventory (Audio_Seq_unusedInventory) to the typed spawn rules
// (ActorSpawning.cpp). Generated callers keep the original call shapes;
// refusals are reported on the "actors" channel and return what the original
// returned for an occupied slot (null or 0).
namespace VS::Actors {
// The typed records of the legacy layouts. Plain `char` fields are read as
// unsigned bytes, as the original's compiler read them.
[[nodiscard]] CharacterTemplate DecodeCharacterTemplate(
    const Actors_CharacterInit& init);
[[nodiscard]] RoomSpawn DecodeRoomSpawn(const Field_MpdEnemySpawn& spawn);
[[nodiscard]] SpawnPlacement DecodePlacement(
    const Field_ActorPlacement& placement);
[[nodiscard]] RememberedSpawn DecodeRemembered(
    const Actors_RoomActorRecord& record);

// func_800765B0 (146C.c:6064). `unused` is unused, as in the original.
Battle_Actor* CreateLiveAuxiliaryActor(int index, int actor_id,
                                       Field_ActorPlacement* placement,
                                       int material, int unused);
// func_800774FC (146C.c:6562).
Battle_Actor* CreateLiveActorWithEquipment(int actor_id, int model_id,
                                           int blade_model, int blade_material,
                                           int shield_model,
                                           int shield_material,
                                           Field_ActorPlacement* placement,
                                           int flags);
// func_800775C0 (146C.c:6578).
Battle_Actor* SpawnLiveRoomEnemy(int actor_id, Field_MpdEnemySpawn* spawn,
                                 Actors_CharacterInit* init, int flags);
// func_8007C8F8 (146C.c:8926): the queue index plus one.
int QueueLiveAshleyModelLoad(int model_id);
// func_8007C928 (146C.c:8936): the queue index plus one, or 0.
int QueueLiveScriptedActorSpawn(unsigned actor_id, int model_id,
                                Field_ActorPlacement* placement);
// func_8007CA20 (146C.c:8962).
void LoadLiveActorEquipmentModels(int actor_id, int blade_model,
                                  int shield_model);
// func_8007CAA4 (146C.c:8971).
void RefreshLiveActorEquipmentModels(int actor_id);
// func_8007D340 (146C.c:9256): RemoveLiveActorAtSlot without hiding the
// remembered enemy.
void RemoveLiveScriptedActor(int actor_id);
// func_800F9AD8 (INITBTL.PRG 18.c:310).
void RestoreLiveAshleyRuntimeState();
// func_800F9CCC (INITBTL.PRG 18.c:361).
void SpawnLiveAshleyWithEquipment();
}  // namespace VS::Actors
