#pragma once

#include "Field/Platforms.h"

#ifdef __cplusplus
extern "C" {
#endif
void Field_InitializeMovingPlatformsEntry(void);
void Field_DrawMovingPlatformShadowsEntry(void);
void Field_SetPlatformFreezeAppearanceEntry(int requested);
void* Field_MovingPlatformTargetWaypointEntry(int index);
int Field_UpdateMovingPlatformsEntry(void);
int Field_CollectMovingPlatformPositionsEntry(
    Field_PlatformContactPoint output_contacts[]);
int Field_SetMovingPlatformActorEntry(int platform_index, int control);
void Field_ResetMovingPlatformWaypointEntry(int platform_index, int waypoint);
void Field_SetMovingPlatformsPausedEntry(int paused);
void Field_FreezeMovingPlatformsEntry(int freeze_frames);
void Field_FreezeRoomGimmicksTemporarilyEntry(void);
void Field_SetMovingPlatformVisibilityEntry(int platform_index, int visibility);
unsigned int Field_MovingPlatformDwellTicksEntry(int platform_index);
#ifdef __cplusplus
}
#endif
