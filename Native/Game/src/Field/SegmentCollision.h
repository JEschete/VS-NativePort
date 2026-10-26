#pragma once

#include <stddef.h>

#include "Legacy/Psx.h"

typedef struct Field_PlatformContactPoint Field_PlatformContactPoint;

#ifdef __cplusplus
extern "C" {
#endif

int Field_IsPointOutsideSolidVolumes(const VECTOR* point,
                                     const Field_PlatformContactPoint* volumes,
                                     size_t count);
SVECTOR* Field_TraceActorAndTerrainCollision(const SVECTOR* from,
                                             const SVECTOR* to, SVECTOR* output,
                                             int ignored_actor);

#ifdef __cplusplus
}
#endif
