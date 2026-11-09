#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
struct Actors_ActorLocation;
struct Actors_ActorLocationQuery;
struct Actors_ActorRecord;
struct Effects_ActionEffectRequest;
struct SVECTOR;
struct Items_TreasureChest;
enum { kFieldEntranceRecordBytes = 12 };
int Field_CurrentSavePointIdEntry(void);
void Field_RefreshAshleyInteractionTileEntry(struct Actors_ActorRecord* record);
void Field_StoreEntranceDoorForReloadEntry(void);
void Field_ApplyUnlockEffectToRoomObjectEntry(struct SVECTOR* target_position);
void Field_SetEntranceDoorRecordEntry(
    char (*record)[kFieldEntranceRecordBytes]);
void Field_FindEntranceDoorNearTileEntry(int tile_x, int tile_z, int layer);
int Field_CheckAshleyTileTriggersEntry(int actor, unsigned char* location);
int Field_CheckActorTrapAndFallTriggersEntry(
    int actor, struct Actors_ActorLocation* location);
struct Field_MpdLockTrigger* Field_FindFacingRoomObjectEntry(int actor);
struct Items_TreasureChest* Field_FindFacingUnlootedChestEntry(int actor);
int Field_TryTriggerFacingTileEventEntry(int actor);
int Field_TileInFrontOfActorEntry(struct Actors_ActorLocationQuery* actor,
                                  unsigned char* output);
int Field_WaitForChestCameraFocusEntry(void);
int Field_StepTrapCameraFocusEntry(void);
int Field_IsChestAnimationReadyEntry(void);
int Field_TrackInteractionCameraToAshleyEntry(void);
void Field_TriggerActorTrapEntry(struct Field_MpdLockTrigger* address,
                                 struct Actors_ActorLocationQuery* location,
                                 int actor);
void Field_BeginObjectInteractionWaitEntry(void);
void Field_EndObjectInteractionWaitEntry(void);
int Field_InteractWithLockedRoomObjectEntry(
    struct Field_MpdLockTrigger* address);
void Field_TryOpenChestEntry(struct Field_MpdLockTrigger* address, int actor);
void Field_BeginChestInteractionEntry(struct Field_MpdLockTrigger* address,
                                      int actor);
void Field_FinishChestInteractionEntry(void);
void Field_OpenChestContentsEntry(void);
int Field_UpdateChestContentsSelectionEntry(void);
void Field_DispatchTrapOrChestInteractionEntry(void);
void Field_FinishTrapInteractionEntry(void);
void Field_BeginTrapInteractionEntry(struct Field_MpdLockTrigger* address,
                                     struct Actors_ActorLocation* location,
                                     int actor);
#ifdef __cplusplus
}
extern "C++" {
#include "Field/ChestInteractions.hpp"
#include "Field/InteractionTarget.hpp"
namespace VS::Field {
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
}
#endif
