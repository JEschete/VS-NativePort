#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <optional>

#include "Actors/ActorInstances.hpp"
#include "Actors/ActorSpawning.hpp"

namespace VS::Battle {
struct Actor;
}  // namespace VS::Battle
namespace VS::Field {
struct MpdEnemySpawn;
}  // namespace VS::Field

// Bindings from VS::Battle::actors, the actor allocations,
// VS::Field::zoneContext, the room enemy memory, Ashley's snapshot
// (VS::State::ashleySnapshot) and the inventory (VS::Items::inventory) to the
// typed spawn rules (ActorSpawning.cpp). Generated callers keep the original
// call shapes; refusals are reported on the "actors" channel and return what
// the original returned for an occupied slot (null or 0).
namespace VS::Actors {
struct CharacterInit;
struct RoomActorRecord;

// Creates unpublished, value-initialized native components. Only a successful
// owned load may publish this identity. Discard requires settled/cancelled
// loads and released model bindings; it refuses published or busy instances.
[[nodiscard]] SpawnedActor AllocateLiveActorInstance(ActorDefinition definition,
                                                     std::size_t slot);
void DiscardLiveActorInstance(SpawnedActor actor);
[[nodiscard]] VS::Battle::Actor* ResolveLiveActorInstance(SpawnedActor actor);
[[nodiscard]] std::optional<ActorDefinition> LiveActorDefinition(
    SpawnedActor actor);
[[nodiscard]] std::optional<ActorInstanceComponents> LiveActorComponents(
    SpawnedActor actor);
// Releases a component's owned binding without touching a foreign occupant.
void ReleaseLiveActorRecord(SpawnedActor actor, ActorRecordPart part);
// Convert a current, borrowed actor pointer at a call boundary. Retain the
// returned identity, not the pointer, across callbacks or actor replacement.
[[nodiscard]] SpawnedActor LiveActorIdentity(const VS::Battle::Actor* actor);
[[nodiscard]] SpawnedActor LiveActorIdentityAt(std::size_t slot);
// Completes an already-claimed removal after its resources have been released.
// Only storage allocated by the native spawn host is freed.
void FinalizeLiveActorRemoval(SpawnedActor actor);

// Invalidates the native identity before the matching legacy allocation is
// freed, or before a gameplay session clears the actor table.
void InvalidateLiveActorHandle(std::size_t slot, VS::Battle::Actor* actor);
void ResetLiveActorHandles();
// Cancels and discards a scripted actor that has not completed loading yet.
[[nodiscard]] bool CancelLivePendingScriptedActorSpawn(std::size_t slot);
// Native (#55): starts removing the installed `actor` in `slot` and returns
// the slot resources its definition owns, once; nothing when `actor` is not
// the slot's actor or is already being removed. An actor the spawn host did
// not allocate owns none and is reported on the "actors" channel.
[[nodiscard]] std::optional<ActorResources> ClaimLiveActorRemoval(
    std::size_t slot, const VS::Battle::Actor* actor);
// Native (#48): cancels the queued asset requests the installed actor owns;
// false, changing nothing, when the queue cannot abandon its head.
[[nodiscard]] bool CancelLiveActorAssets(std::size_t slot,
                                         const VS::Battle::Actor* actor);
// Runs the asset queue until the installed actor owns no queued request.
void FinishLiveActorAssets(std::size_t slot, const VS::Battle::Actor* actor);
// Native (#48): reports and forgets the actor loads that have ended, once per
// frame after the queue update, so each ticketed load completes explicitly.
void SettleLiveActorLoads();
// Native (#48): queues `request` for the actor installed in `slot`, with a
// ticket the actor load table settles, so removing that actor cancels it.
// Unowned, as the original queued it, when the slot is empty or the request
// kind is not ownable. The queue index, or -1.
int QueueLiveSlotActorAsset(std::size_t slot,
                            const Content::ActorAssetRequest& request);
// Native (#55): submits an installed actor's ownable request with a ticket
// retained for its consumer to inspect and forget.
[[nodiscard]] std::optional<Content::ActorAssetTicket> SubmitLiveSlotActorAsset(
    std::size_t slot, const Content::ActorAssetRequest& request);
[[nodiscard]] std::optional<Content::ActorAssetTicket>
SubmitLiveActorOwnedAsset(const VS::Battle::Actor* actor,
                          const Content::ActorAssetRequest& request);
// As QueueLiveSlotActorAsset, for `actor` wherever it is installed or while
// the spawn host's allocation of it is pending; unowned when the actor table
// and the pending allocations do not hold it.
int QueueLiveActorOwnedAsset(const VS::Battle::Actor* actor,
                             const Content::ActorAssetRequest& request);
// Native (#48): the memory the actor named by `owner` keeps for its `record`
// of `table`, where an owned request binds it: the model memory of its body or
// gimmick, the weapon model memory of its second body or weapon, the shield
// model memory of its shield. kNone for a stale or foreign owner, an actor
// without a spawn definition, or a record that is not one of the actor's parts.
[[nodiscard]] Content::AssetAddress LiveActorRecordMemory(
    Content::ActorAssetOwner owner, Content::ActorRecordTable table,
    int record);
// Native (#55): the shape the spawn host allocated `actor` with, while it is
// installed in a spawn slot or its allocation is pending; nothing for storage
// the host did not allocate. Native code reads this instead of the actor's
// allocation kind word (0x40).
[[nodiscard]] std::optional<ActorShape> LiveActorShape(
    const VS::Battle::Actor* actor);
// Native (#55): records `actor`, storage its caller owns and frees, as an
// installed instance of `shape` in `slot`, as the spawn host's Allocate and
// Install record a spawned actor; kNone, changing nothing, when the slot keeps
// another live or pending instance. Spawns allocate through the spawn host;
// this defines an actor whose storage the host does not own.
[[nodiscard]] SpawnedActor InstallLiveActorInstance(std::size_t slot,
                                                    VS::Battle::Actor* actor,
                                                    ActorShape shape);

// The typed records of the legacy layouts.
[[nodiscard]] CharacterTemplate DecodeCharacterTemplate(
    const CharacterInit& init);
[[nodiscard]] RoomSpawn DecodeRoomSpawn(const VS::Field::MpdEnemySpawn& spawn);
[[nodiscard]] RememberedSpawn DecodeRemembered(const RoomActorRecord& record);

// func_800765B0 (146C.c:6064). `unused` is unused, as in the original.
VS::Battle::Actor* CreateLiveAuxiliaryActor(int index, int actor_id,
                                            const SpawnPlacement* placement,
                                            int material, int unused);
// func_800774FC (146C.c:6562).
VS::Battle::Actor* CreateLiveActorWithEquipment(
    int actor_id, int model_id, int blade_model, int blade_material,
    int shield_model, int shield_material, const SpawnPlacement* placement,
    int flags);
// func_800775C0 (146C.c:6578).
VS::Battle::Actor* SpawnLiveRoomEnemy(int actor_id,
                                      VS::Field::MpdEnemySpawn* spawn,
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
