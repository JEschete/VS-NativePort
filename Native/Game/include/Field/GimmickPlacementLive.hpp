#pragma once

#include <cstddef>
#include <optional>

#include "Field/GimmickPlacement.hpp"

struct Field_GimmickInstance;
struct Actors_ActorRecord;

namespace VS::Field {
bool IsLiveGimmickPrefixReadable(const Field_GimmickInstance* record);
std::optional<GimmickQueryState> LiveGimmickQueryState(std::size_t slot);
std::optional<std::size_t> FindLiveTopGimmickAtTile(
    const GimmickTileQuery& query, bool require_idle_mode);
bool AreLiveGimmicksMoving();
bool SetLiveGimmickOrientation(std::int32_t slot, std::int32_t orientation);
bool CanPlaceLiveGimmickAtPoint(const GimmickPlacementPoint& point);
bool DropLiveUnsupportedGimmickStack(std::int32_t slot);
std::optional<std::size_t> FindLiveInteractableGimmickAhead(
    std::int32_t actor_slot);
bool BeginLiveGimmickInteraction(std::int32_t actor_slot);
bool ApplyLiveGimmickInteractionCommand(Actors_ActorRecord* actor,
                                        std::int32_t mode);
}  // namespace VS::Field
