#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_FIELD_ROOMTRIGGERMARKERS_H
#define VS_OWNED_FIELD_ROOMTRIGGERMARKERS_H

#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Field_DrawRoomTriggerMarkers(void);
void Field_DrawRoomTriggerMarker(int world_x, int world_z, int16_t height,
                                 u_int brightness);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_FIELD_ROOMTRIGGERMARKERS_H
