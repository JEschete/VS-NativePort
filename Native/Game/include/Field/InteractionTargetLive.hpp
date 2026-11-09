#pragma once

#include <optional>
#include <vector>

#include "Field/InteractionTarget.hpp"

struct Field_MpdLockTrigger;
struct Field_MpdFloorTrigger;
struct Items_TreasureChest;

namespace VS::Field {

std::vector<RoomTriggerInfo> LiveActiveRoomTriggerInfo();
std::optional<TrapTarget> FindLiveRoomTriggerAtTile(
    const RoomTriggerTile& tile);
bool SetLiveRoomTriggerEnabled(std::optional<TrapTarget> target,
                               std::int32_t activation);
bool DisableLiveTrapAtPosition(TrapWorldPosition position);

std::optional<TrapTarget> FindLiveTrapTarget(
    const Field_MpdFloorTrigger* record);
std::optional<TrapTarget> FindLiveTrapTargetAddress(const void* address);
std::optional<ChestTarget> FindLiveChestTarget(
    const Items_TreasureChest* record);
std::optional<ChestTarget> FindLiveChestTargetAddress(const void* address);
Field_MpdFloorTrigger* ResolveLiveTrapTarget(TrapTarget target);
Items_TreasureChest* ResolveLiveChestTarget(ChestTarget target);
InteractionTargetState& LiveInteractionTargetState();
InteractionTargetDomain LiveInteractionTargetDomain();
void InvalidateLiveInteractionTargets();

}  // namespace VS::Field
