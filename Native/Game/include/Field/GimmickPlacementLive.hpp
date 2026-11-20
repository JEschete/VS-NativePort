#pragma once

#include <cstddef>
#include <optional>

#include "Field/GimmickPlacement.hpp"

namespace VS::Field {
struct GimmickSnapshot;
struct GimmickInstance;
}  // namespace VS::Field
namespace VS::Actors {
struct ActorRecord;
}  // namespace VS::Actors

namespace VS::Field {
bool IsLiveGimmickPrefixReadable(const VS::Field::GimmickInstance* record);
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
bool ApplyLiveGimmickInteractionCommand(VS::Actors::ActorRecord* actor,
                                        std::int32_t mode);

int GimmickPlacementEntry(int gimmick_slot,
                          VS::Field::GimmickSnapshot* output_snapshot);
int FindGimmickBelowEntry(const VS::Field::GimmickInstance* reference);
int FindTopGimmickAtTileEntry(int tile_x, int tile_z, int require_idle_mode);
int GimmickSubtypeEntry(int slot);
int AreGimmicksMovingEntry();
int SetGimmickOrientationEntry(int slot, int orientation);
int CanPlaceGimmickAtPointEntry(int x, int z, int minimum_height);
void DropUnsupportedGimmickStackEntry(int slot);
int FindInteractableGimmickAheadEntry(int actor_slot);
int BeginGimmickInteractionEntry(int actor_slot);
void ApplyGimmickInteractionCommandEntry(VS::Actors::ActorRecord* actor,
                                         int mode);
int MeasureTileStackEntry(int tile_x, int tile_z, int* height_offset);
int IsGimmickWithinHeightBandEntry(int tile_x, int tile_z, int minimum_height);
}  // namespace VS::Field
