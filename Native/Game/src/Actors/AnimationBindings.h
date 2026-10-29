#pragma once

#include "Legacy/Psx.h"

typedef struct Actors_ActorRecord Actors_ActorRecord;

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Actors_FindBoundAnimationHeader(Actors_ActorRecord* actor,
                                         u_short** header_out);

#ifdef __cplusplus
}
#endif
