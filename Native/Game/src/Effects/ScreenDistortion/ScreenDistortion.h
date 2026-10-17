// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES
#define VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS)
#define VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS

void Effects_ScreenDistortion_SetFeedbackLayer(int arg0);
void Effects_ScreenDistortion_DrawFeedbackTiles(void);
void Effects_ScreenDistortion_SetRadialWarp(short arg0, short arg1);
void Effects_ScreenDistortion_WarpPolygonVertices(void);
void Effects_ScreenDistortion_SetVignetteEnabled(int arg0);
int Effects_ScreenDistortion_EllipsePoint(int arg0, int arg1);
int* Effects_ScreenDistortion_AppendVignetteCorner(int arg0, int arg1, int* arg2, void** arg3);
void Effects_ScreenDistortion_DrawVignette(void);

#endif // VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS
