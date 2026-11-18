#pragma once

#include "Field/InteractionTarget.hpp"
#include "Field/RoomSwitches.hpp"

struct Field_MpdLockTrigger;

namespace VS::Field {
Field_MpdLockTrigger* InteractiveDoorAtTileEntry(int tile_x, int tile_z,
                                                 int floor_layer);
void MarkSigilDoorUnlockedEntry(int sigil_index);
void ToggleDoorCollisionEntry(Field_MpdLockTrigger* door);
int BeginDoorToggleEntry(Field_MpdLockTrigger* door);
int StartDoorAnimationEntry(Field_MpdLockTrigger* door);
int FinishDoorAnimationEntry(Field_MpdLockTrigger* door);
int IsRoomSigilDoorUnlockedEntry(int zone_id, int map_id,
                                 unsigned int sigil_index);
void ToggleRoomSwitchEntry(Field_MpdLockTrigger* switch_trigger);
void BeginSwitchToggleEntry(Field_MpdLockTrigger* switch_trigger);
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
