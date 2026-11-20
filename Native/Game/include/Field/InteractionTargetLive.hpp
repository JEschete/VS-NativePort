#pragma once

#include <optional>
#include <vector>

#include "Field/InteractionTarget.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Field {
struct MpdLockTrigger;
struct MpdFloorTrigger;
}  // namespace VS::Field
struct SVECTOR;

namespace VS::Field {

std::vector<RoomTriggerInfo> LiveActiveRoomTriggerInfo();
std::optional<TrapTarget> FindLiveRoomTriggerAtTile(
    const RoomTriggerTile& tile);
bool SetLiveRoomTriggerEnabled(std::optional<TrapTarget> target,
                               std::int32_t activation);
bool DisableLiveTrapAtPosition(TrapWorldPosition position);

std::optional<TrapTarget> FindLiveTrapTarget(
    const VS::Field::MpdFloorTrigger* record);
std::optional<TrapTarget> FindLiveTrapTargetAddress(const void* address);
std::optional<ChestTarget> FindLiveChestTarget(
    const VS::Items::TreasureChest* record);
std::optional<ChestTarget> FindLiveChestTargetAddress(const void* address);
VS::Field::MpdFloorTrigger* ResolveLiveTrapTarget(TrapTarget target);
VS::Items::TreasureChest* ResolveLiveChestTarget(ChestTarget target);
InteractionTargetState& LiveInteractionTargetState();
InteractionTargetDomain LiveInteractionTargetDomain();
void InvalidateLiveInteractionTargets();

// The original BATTLE.PRG entries over the typed queries above.
VS::Field::MpdFloorTrigger* RoomTriggerAtTileEntry(int x, int z, int layer);
void SetRoomTriggerEnabledEntry(VS::Field::MpdFloorTrigger* selected,
                                int activation);
void DisableTrapAtPositionEntry(SVECTOR* position);

}  // namespace VS::Field
