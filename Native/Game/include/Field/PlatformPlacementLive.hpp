#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>
#include <vector>

#include "Field/PlatformPlacement.hpp"

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
bool SetLiveMovingPlatformActor(int platform_index, std::int32_t rider);
bool ResetLiveMovingPlatformWaypoint(int platform_index, std::int32_t waypoint);
void SetLiveMovingPlatformsPaused(std::int32_t paused);
bool FreezeLiveMovingPlatforms(std::int32_t freeze_frames);
bool FreezeLiveRoomGimmicksTemporarily();
bool SetLiveMovingPlatformVisibility(int platform_index,
                                     std::int32_t visibility);
std::optional<std::uint16_t> LiveMovingPlatformDwellTicks(int platform_index);

// The current room's authored platform on `support_layer`, hidden or not.
std::optional<PlatformContact> LiveMovingPlatformPlacement(int support_layer);

// The current room's visible platform on `support_layer` (func_800A4A24).
std::optional<PlatformContact> FindLiveMovingPlatformById(int support_layer);

void InitializeMovingPlatformsEntry();
void DrawMovingPlatformShadowsEntry();
void SetPlatformFreezeAppearanceEntry(int requested);
void* MovingPlatformTargetWaypointEntry(int index);
int UpdateMovingPlatformsEntry();
// Writes the visible contacts; the original takes an SVECTOR array.
int CollectMovingPlatformPositionsEntry(PlatformContact output_contacts[]);
int SetMovingPlatformActorEntry(int platform_index, int rider);
void ResetMovingPlatformWaypointEntry(int platform_index, int waypoint);
void SetMovingPlatformsPausedEntry(int paused);
void FreezeMovingPlatformsEntry(int freeze_frames);
void FreezeRoomGimmicksTemporarilyEntry();
void SetMovingPlatformVisibilityEntry(int platform_index, int visibility);
unsigned int MovingPlatformDwellTicksEntry(int platform_index);
}  // namespace VS::Field
