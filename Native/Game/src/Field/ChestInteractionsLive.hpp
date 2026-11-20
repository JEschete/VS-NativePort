#pragma once

#include "Field/ChestInteractions.hpp"
#include "Field/InteractionTarget.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Field {
struct MpdLockTrigger;
}
namespace VS::Actors {
struct ActorLocation;
}
namespace VS::Actors {
struct ActorLocationQuery;
}
namespace VS::Actors {
struct ActorRecord;
}
struct SVECTOR;

namespace VS::Effects {
struct LiveActionEffectRequest;
}  // namespace VS::Effects

namespace VS::Field {
inline constexpr int kEntranceRecordBytes = 12;
int CurrentSavePointIdEntry();
void RefreshAshleyInteractionTileEntry(VS::Actors::ActorRecord* record);
void StoreEntranceDoorForReloadEntry();
void ApplyUnlockEffectToRoomObjectEntry(SVECTOR* target_position);
void SetEntranceDoorRecordEntry(char (*record)[kEntranceRecordBytes]);
void FindEntranceDoorNearTileEntry(int tile_x, int tile_z, int layer);
int CheckAshleyTileTriggersEntry(int actor, unsigned char* location);
int CheckActorTrapAndFallTriggersEntry(int actor,
                                       VS::Actors::ActorLocation* location);
VS::Field::MpdLockTrigger* FindFacingRoomObjectEntry(int actor);
VS::Items::TreasureChest* FindFacingUnlootedChestEntry(int actor);
int TryTriggerFacingTileEventEntry(int actor);
int TileInFrontOfActorEntry(VS::Actors::ActorLocationQuery* actor,
                            unsigned char* output);
int WaitForChestCameraFocusEntry();
int StepTrapCameraFocusEntry();
int IsChestAnimationReadyEntry();
int TrackInteractionCameraToAshleyEntry();
void TriggerActorTrapEntry(VS::Field::MpdLockTrigger* address,
                           VS::Actors::ActorLocationQuery* location, int actor);
void BeginObjectInteractionWaitEntry();
void EndObjectInteractionWaitEntry();
int InteractWithLockedRoomObjectEntry(VS::Field::MpdLockTrigger* address);
void TryOpenChestEntry(VS::Field::MpdLockTrigger* address, int actor);
void BeginChestInteractionEntry(VS::Field::MpdLockTrigger* address, int actor);
void FinishChestInteractionEntry();
void OpenChestContentsEntry();
int UpdateChestContentsSelectionEntry();
void DispatchTrapOrChestInteractionEntry();
void FinishTrapInteractionEntry();
void BeginTrapInteractionEntry(VS::Field::MpdLockTrigger* address,
                               VS::Actors::ActorLocation* location, int actor);
std::uint8_t LiveCurrentSavePointId();
bool RefreshLiveAshleyInteractionTile(const VS::Actors::ActorRecord* record);
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
Effects::LiveActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect);
std::optional<RoomObjectTarget> FindLiveFacingRoomObject(std::int32_t actor);
std::optional<ChestTarget> FindLiveFacingUnlootedChest(std::int32_t actor);
VS::Field::MpdLockTrigger* ResolveLiveRoomObjectTarget(RoomObjectTarget target);
RoomObjectInteractionResult InteractLiveRoomObject(RoomObjectTarget target);
}  // namespace VS::Field
