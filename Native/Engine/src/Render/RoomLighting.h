#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_ROOMLIGHTING_H
#define VS_OWNED_RENDER_ROOMLIGHTING_H
#include "Legacy/Psx.h"
#include "Field/Room.h"
#include "Render/Render.h"
#include "Render/Palette.h"

void Render_SampleRoomLighting(int packedPosition, int layer, Render_ColorCode* color);

#endif // VS_OWNED_RENDER_ROOMLIGHTING_H
