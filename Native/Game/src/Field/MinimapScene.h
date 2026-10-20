#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_FIELD_MINIMAPSCENE_H
#define VS_OWNED_FIELD_MINIMAPSCENE_H
#include "Field/Map.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
#define FIELD_MAP_THROWS noexcept(false)
extern "C" {
#else
#define FIELD_MAP_THROWS
#endif

int Field_ValidateMapScene(const void* data, size_t size);
void Field_DrawMinimapScene(void* data, size_t size) FIELD_MAP_THROWS;
Field_MapRoomName* Field_ResolveSceneRoomData(void* data,
                                              size_t size) FIELD_MAP_THROWS;

#ifdef __cplusplus
}
#endif
#undef FIELD_MAP_THROWS

#endif  // VS_OWNED_FIELD_MINIMAPSCENE_H
