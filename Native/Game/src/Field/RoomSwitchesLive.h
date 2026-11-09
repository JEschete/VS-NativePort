#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
struct Field_MpdLockTrigger* Field_InteractiveDoorAtTileEntry(int tile_x,
                                                              int tile_z,
                                                              int floor_layer);
void Field_MarkSigilDoorUnlockedEntry(int sigil_index);
void Field_ToggleDoorCollisionEntry(struct Field_MpdLockTrigger* door);
int Field_BeginDoorToggleEntry(struct Field_MpdLockTrigger* door);
int Field_StartDoorAnimationEntry(struct Field_MpdLockTrigger* door);
int Field_FinishDoorAnimationEntry(struct Field_MpdLockTrigger* door);
int Field_IsRoomSigilDoorUnlockedEntry(int zone_id, int map_id,
                                       unsigned int sigil_index);
void Field_ToggleRoomSwitchEntry(struct Field_MpdLockTrigger* switch_trigger);
void Field_BeginSwitchToggleEntry(struct Field_MpdLockTrigger* switch_trigger);
#ifdef __cplusplus
}
extern "C++" {
#include "Field/InteractionTarget.hpp"
#include "Field/RoomSwitches.hpp"
namespace VS::Field {
std::optional<RoomObjectTarget> FindLiveInteractiveDoorAtTile(
    const RoomDoorTileQuery& query);
bool MarkLiveSigilDoorUnlocked(std::uint32_t sigil_index);
bool ToggleLiveDoorCollision(RoomObjectTarget target);
bool BeginLiveDoorToggle(RoomObjectTarget target);
bool StartLiveDoorAnimation(RoomObjectTarget target);
bool FinishLiveDoorAnimation(RoomObjectTarget target);
std::uint8_t LiveRoomSigilDoorUnlockByte(const SigilDoorRoomQuery& query);
bool BeginLiveChestSwitch(ChestTarget target);
bool ToggleLiveChestSwitch(ChestTarget target);
}  // namespace VS::Field
}
#endif
