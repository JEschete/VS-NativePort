#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>
#include <vector>

#include "Field/PlatformPlacement.hpp"
#include "Field/Platforms.h"

// Binding from the current room's moving platforms to the typed contact
// lookup. Contacts are returned by value; nothing is borrowed.
namespace VS::Field {
void InitializeLiveMovingPlatforms();
void DrawLiveMovingPlatformShadows();
bool SetLivePlatformFreezeAppearance(std::int32_t requested);
std::optional<MovingPlatformWaypoint> LiveMovingPlatformTargetWaypoint(
    int index);
bool UpdateLiveMovingPlatforms();
std::vector<PlatformContact> CollectLiveMovingPlatformPositions();
bool SetLiveMovingPlatformActor(int platform_index, std::int32_t control);
bool ResetLiveMovingPlatformWaypoint(int platform_index, std::int32_t waypoint);
void SetLiveMovingPlatformsPaused(std::int32_t paused);
bool FreezeLiveMovingPlatforms(std::int32_t freeze_frames);
bool FreezeLiveRoomGimmicksTemporarily();
bool SetLiveMovingPlatformVisibility(int platform_index,
                                     std::int32_t visibility);
std::optional<std::uint16_t> LiveMovingPlatformDwellTicks(int platform_index);

// The current room's authored platform on `support_layer`, hidden or not.
std::optional<Field_PlatformContactPoint> LiveMovingPlatformPlacement(
    int support_layer);

// The current room's visible platform on `support_layer` (func_800A4A24).
std::optional<Field_PlatformContactPoint> FindLiveMovingPlatformById(
    int support_layer);

void InitializeMovingPlatformsEntry();
void DrawMovingPlatformShadowsEntry();
void SetPlatformFreezeAppearanceEntry(int requested);
void* MovingPlatformTargetWaypointEntry(int index);
int UpdateMovingPlatformsEntry();
int CollectMovingPlatformPositionsEntry(
    Field_PlatformContactPoint output_contacts[]);
int SetMovingPlatformActorEntry(int platform_index, int control);
void ResetMovingPlatformWaypointEntry(int platform_index, int waypoint);
void SetMovingPlatformsPausedEntry(int paused);
void FreezeMovingPlatformsEntry(int freeze_frames);
void FreezeRoomGimmicksTemporarilyEntry();
void SetMovingPlatformVisibilityEntry(int platform_index, int visibility);
unsigned int MovingPlatformDwellTicksEntry(int platform_index);
}  // namespace VS::Field
