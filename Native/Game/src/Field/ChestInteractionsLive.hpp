#pragma once

#include "Field/ChestInteractions.hpp"
#include "Field/InteractionTarget.hpp"
#include "Items/ItemRecords.hpp"

struct Field_MpdLockTrigger;
struct Actors_ActorLocation;
struct Actors_ActorLocationQuery;
struct Actors_ActorRecord;
struct Effects_ActionEffectRequest;
struct SVECTOR;

namespace VS::Field {
inline constexpr int kEntranceRecordBytes = 12;
int CurrentSavePointIdEntry();
void RefreshAshleyInteractionTileEntry(Actors_ActorRecord* record);
void StoreEntranceDoorForReloadEntry();
void ApplyUnlockEffectToRoomObjectEntry(SVECTOR* target_position);
void SetEntranceDoorRecordEntry(char (*record)[kEntranceRecordBytes]);
void FindEntranceDoorNearTileEntry(int tile_x, int tile_z, int layer);
int CheckAshleyTileTriggersEntry(int actor, unsigned char* location);
int CheckActorTrapAndFallTriggersEntry(int actor,
                                       Actors_ActorLocation* location);
Field_MpdLockTrigger* FindFacingRoomObjectEntry(int actor);
VS::Items::TreasureChest* FindFacingUnlootedChestEntry(int actor);
int TryTriggerFacingTileEventEntry(int actor);
int TileInFrontOfActorEntry(Actors_ActorLocationQuery* actor,
                            unsigned char* output);
int WaitForChestCameraFocusEntry();
int StepTrapCameraFocusEntry();
int IsChestAnimationReadyEntry();
int TrackInteractionCameraToAshleyEntry();
void TriggerActorTrapEntry(Field_MpdLockTrigger* address,
                           Actors_ActorLocationQuery* location, int actor);
void BeginObjectInteractionWaitEntry();
void EndObjectInteractionWaitEntry();
int InteractWithLockedRoomObjectEntry(Field_MpdLockTrigger* address);
void TryOpenChestEntry(Field_MpdLockTrigger* address, int actor);
void BeginChestInteractionEntry(Field_MpdLockTrigger* address, int actor);
void FinishChestInteractionEntry();
void OpenChestContentsEntry();
int UpdateChestContentsSelectionEntry();
void DispatchTrapOrChestInteractionEntry();
void FinishTrapInteractionEntry();
void BeginTrapInteractionEntry(Field_MpdLockTrigger* address,
                               Actors_ActorLocation* location, int actor);
std::uint8_t LiveCurrentSavePointId();
bool RefreshLiveAshleyInteractionTile(const Actors_ActorRecord* record);
void SetLiveEntranceDoor(const std::optional<DoorRecord>& door);
void FindLiveEntranceDoorNearTile(std::int32_t tile_x, std::int32_t tile_z,
                                  std::int32_t layer);
bool CheckLiveAshleyTileTriggers(std::int32_t actor,
                                 const Actors::ActorPlacementView& placement);
ActorTileTriggerResult CheckLiveActorTrapAndFallTriggers(
    std::int32_t actor, const Actors::ActorPlacementView& placement);
bool TryOpenLiveChest(ChestTarget target);
bool BeginLiveChestInteraction(ChestTarget target, std::int32_t actor);
bool BeginLiveTrapInteraction(TrapTarget target,
                              const Actors::ActorPlacementView& placement,
                              std::int32_t actor);
bool TriggerLiveActorTrap(TrapTarget target, const TrapPosition& point,
                          std::int32_t actor);
Effects_ActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect);
std::optional<RoomObjectTarget> FindLiveFacingRoomObject(std::int32_t actor);
std::optional<ChestTarget> FindLiveFacingUnlootedChest(std::int32_t actor);
Field_MpdLockTrigger* ResolveLiveRoomObjectTarget(RoomObjectTarget target);
RoomObjectInteractionResult InteractLiveRoomObject(RoomObjectTarget target);
}  // namespace VS::Field
