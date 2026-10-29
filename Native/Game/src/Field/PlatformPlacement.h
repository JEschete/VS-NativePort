#pragma once

#include "Field/Platforms.h"

#ifdef __cplusplus
extern "C" {
#endif

int Field_CopyMovingPlatformPlacement(int support_layer,
                                      Field_PlatformContactPoint* contact);

// Borrowed until the next lookup on this thread; placement copies its value.
Field_PlatformContactPoint* Field_FindMovingPlatformById(int support_layer);

#ifdef __cplusplus
}
#endif
