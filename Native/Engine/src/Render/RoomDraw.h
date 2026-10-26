#pragma once
#include <stdint.h>
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_ROOMDRAW_H
#define VS_OWNED_RENDER_ROOMDRAW_H
#include "Legacy/Psx.h"
#include "Render/Render.h"

enum { kRoomSpatialDimensions = 3, kRoomGeometryGroupBytes = 64 };
typedef struct Field_RoomTint Field_RoomTint;

// decomp: _mpdGeometryHeader / 146C.h:31; transform access in 2EA3C.s:1265.
typedef struct Render_RoomGeometryGroup {
  unsigned flags;
  int16_t bounds[kRoomSpatialDimensions][2];
  int angles[kRoomSpatialDimensions];
  unsigned polygonOffset;
  MATRIX view;
} Render_RoomGeometryGroup;

// Native subdivision workspace, not the packed MPD vertex layout.
typedef struct Render_RoomVertex {
  SVECTOR position;
  unsigned color;
  uint16_t uv;
} Render_RoomVertex;

#ifdef __cplusplus
static_assert(sizeof(Render_RoomGeometryGroup) == kRoomGeometryGroupBytes);
extern "C" {
#else
_Static_assert(sizeof(Render_RoomGeometryGroup) == kRoomGeometryGroupBytes,
               "MPD group layout");
#endif

int Render_ValidateRoomGeometry(const void* data, size_t size);
void Render_UpdateRoomGroupTransforms(void* data);
void Render_BuildRoomGeometryViewMatrix(VECTOR* position, VECTOR* angles,
                                        MATRIX* output);
void Render_BuildScaleCompensatedRoomView(VECTOR* position, VECTOR* angles,
                                          MATRIX* output);
void Render_DrawRoomGeometry(void* data, int mode);
void Render_InterpolateRoomVertexMidpoint(const Render_RoomVertex* first,
                                          const Render_RoomVertex* second,
                                          Render_RoomVertex* output);
void Render_SetRoomPolygonNearClip(int depth);
void Render_SetRoomPolygonFarClip(int depth);
void Render_SetRoomPolygonScreenBounds(int left, int top, int right,
                                       int bottom);
void Render_SetRoomTextureLodDepth(int depth);
uint32_t Render_TintRoomPrimitiveColor(uint32_t color, int16_t strength,
                                       int16_t red_offset, int16_t green_offset,
                                       int16_t blue_offset);
void Render_TintRoomGeometryPrimitives(void* start, void* end,
                                       Field_RoomTint* tint, int flags);
void Render_DrawSkyDome(void* data);
void Render_GetProjectionScreenOffset(Render_ScreenOffset* offset);
void Render_SetProjectionScreenOffset(Render_ScreenOffset* offset);

#include "Field/Room.h"

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_RENDER_ROOMDRAW_H
