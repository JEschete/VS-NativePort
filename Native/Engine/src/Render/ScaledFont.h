#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_SCALEDFONT_H
#define VS_OWNED_RENDER_SCALEDFONT_H
#include "Battle/TextBoxes.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/TextBoxes.h"

#ifdef __cplusplus
extern "C" {
#endif

int Render_DrawScaledFontChar(int char_id, int x, int y, int width, int height,
                              int scale);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_RENDER_SCALEDFONT_H
