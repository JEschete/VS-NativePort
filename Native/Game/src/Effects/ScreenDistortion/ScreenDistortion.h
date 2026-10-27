// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES
#define VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS)
#define VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS

void Effects_ScreenDistortion_SetFeedbackLayer(int ordering_layer);
void Effects_ScreenDistortion_DrawFeedbackTiles(void);
void Effects_ScreenDistortion_SetRadialWarp(short initial_scale_q12, short radial_growth_q12);
void Effects_ScreenDistortion_SetVignetteEnabled(int enabled);
int Effects_ScreenDistortion_EllipsePoint(int angular_step, int radius);
int* Effects_ScreenDistortion_AppendVignetteCorner(int packed_corner_xy, int first_angular_step, int* packet_words, u_long* ordering_head);
void Effects_ScreenDistortion_DrawVignette(void);

#endif // VS_EFFECTS_SCREENDISTORTION_SCREENDISTORTION_H_DECLS
