#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ActorSpawning.hpp"

struct Battle_Actor;
namespace VS::Field {
struct MpdEnemySpawn;
}  // namespace VS::Field

// Bindings from Battle_actors, the actor allocations, VS::Field::zoneContext,
// the room enemy memory, Ashley's snapshot (VS::State::ashleySnapshot) and the
// inventory (VS::Items::inventory) to the typed spawn rules
// (ActorSpawning.cpp). Generated callers keep the original call shapes;
// refusals are reported on the "actors" channel and return what the original
// returned for an occupied slot (null or 0).
namespace VS::Actors {
struct CharacterInit;
struct RoomActorRecord;

// The typed records of the legacy layouts.
[[nodiscard]] CharacterTemplate DecodeCharacterTemplate(
    const CharacterInit& init);
[[nodiscard]] RoomSpawn DecodeRoomSpawn(const VS::Field::MpdEnemySpawn& spawn);
[[nodiscard]] RememberedSpawn DecodeRemembered(const RoomActorRecord& record);

// func_800765B0 (146C.c:6064). `unused` is unused, as in the original.
Battle_Actor* CreateLiveAuxiliaryActor(int index, int actor_id,
                                       const SpawnPlacement* placement,
                                       int material, int unused);
// func_800774FC (146C.c:6562).
Battle_Actor* CreateLiveActorWithEquipment(int actor_id, int model_id,
                                           int blade_model, int blade_material,
                                           int shield_model,
                                           int shield_material,
                                           const SpawnPlacement* placement,
                                           int flags);
// func_800775C0 (146C.c:6578).
Battle_Actor* SpawnLiveRoomEnemy(int actor_id, VS::Field::MpdEnemySpawn* spawn,
                                 CharacterInit* init, int flags);
// func_8007C8F8 (146C.c:8926): the queue index plus one.
int QueueLiveAshleyModelLoad(int model_id);
// func_8007C928 (146C.c:8936): the queue index plus one, or 0.
int QueueLiveScriptedActorSpawn(unsigned actor_id, int model_id,
                                const SpawnPlacement* placement);
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
