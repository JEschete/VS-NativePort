#pragma once

#include "Field/InteractionTarget.hpp"
#include "Field/RoomSwitches.hpp"

namespace VS::Field {
struct MpdLockTrigger;
}  // namespace VS::Field

namespace VS::Field {
VS::Field::MpdLockTrigger* InteractiveDoorAtTileEntry(int tile_x, int tile_z,
                                                      int floor_layer);
void MarkSigilDoorUnlockedEntry(int sigil_index);
void ToggleDoorCollisionEntry(VS::Field::MpdLockTrigger* door);
int BeginDoorToggleEntry(VS::Field::MpdLockTrigger* door);
int StartDoorAnimationEntry(VS::Field::MpdLockTrigger* door);
int FinishDoorAnimationEntry(VS::Field::MpdLockTrigger* door);
int IsRoomSigilDoorUnlockedEntry(int zone_id, int map_id,
                                 unsigned int sigil_index);
void ToggleRoomSwitchEntry(VS::Field::MpdLockTrigger* switch_trigger);
void BeginSwitchToggleEntry(VS::Field::MpdLockTrigger* switch_trigger);
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
