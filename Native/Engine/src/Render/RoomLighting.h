#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_ROOMLIGHTING_H
#define VS_OWNED_RENDER_ROOMLIGHTING_H
#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"
#include "Render/Render.h"

void Render_SampleRoomLighting(int packed_position, int layer,
                               Render_ColorCode* color);

#endif  // VS_OWNED_RENDER_ROOMLIGHTING_H
