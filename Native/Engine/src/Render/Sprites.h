#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_SPRITES_H
#define VS_OWNED_RENDER_SPRITES_H
#include "Legacy/Psx.h"

u_long* Render_QueueDefaultSprite(int wh, int xy);

u_long* Render_QueueSpriteWithTexturePage(int brightness_transparency, int xy,
                                          int wh, u_long* next_prim);

u_long* Render_QueueSprite(int brightness_transparency, int xy, int wh,
                           u_long* next_prim);

void Render_LoadFrameViewMatrix(void);

#endif  // VS_OWNED_RENDER_SPRITES_H
