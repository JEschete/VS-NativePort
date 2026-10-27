#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_ROOMLIGHTING_H
#define VS_OWNED_RENDER_ROOMLIGHTING_H
#include <stdint.h>

#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"
#include "Render/Render.h"

// decomp: 2EA3C.s:1857, eight words per imported lighting triangle.
enum {
  kRoomLightTriangleVertices = 3,
  kRoomLightPositionAxes = 2,
  kRoomLightTriangleBytes = 32
};

typedef struct Render_RoomLightTriangle {
  int16_t minimum_x, minimum_z, maximum_x, maximum_z;
  int16_t points[kRoomLightTriangleVertices][kRoomLightPositionAxes];
  uint32_t colors[kRoomLightTriangleVertices];
} Render_RoomLightTriangle;

#ifdef __cplusplus
static_assert(sizeof(Render_RoomLightTriangle) == kRoomLightTriangleBytes);
extern "C" {
#else
_Static_assert(sizeof(Render_RoomLightTriangle) == kRoomLightTriangleBytes,
               "room lighting triangle disk layout");
#endif

int Render_InterpolateRoomLightTriangle(
    uint32_t packed_position, Render_ColorCode* color,
    const Render_RoomLightTriangle* triangle);

void Render_SampleRoomLighting(int packed_position, int layer,
                               Render_ColorCode* color);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_RENDER_ROOMLIGHTING_H
