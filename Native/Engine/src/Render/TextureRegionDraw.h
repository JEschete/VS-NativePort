#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_TEXTUREREGIONDRAW_H
#define VS_OWNED_RENDER_TEXTUREREGIONDRAW_H
#include "Legacy/Psx.h"
#include "Render/Images.h"

// Each renderTextures.h inclusion used its overlay's _disMap (G00805 ENDING,
// G01780 MENUF). Keep atlas selection with the caller in the shared native
// copy.

// decomp: renderTextures.h _renderTextureFadeInTint (src/renderTextures.h:3)
void Render_DrawTextureTintFade(const Render_TextureRegion regions[], int x,
                                int y, int tex_id, CVECTOR colors[]);

// decomp: renderTextures.h _renderTexturePopIn (src/renderTextures.h:60)
void Render_DrawTextureFlashFade(const Render_TextureRegion regions[], int x,
                                 int y, int tex_id, CVECTOR colors[]);

// decomp: renderTextures.h _renderTextureWipe (src/renderTextures.h:130)
void Render_DrawTextureWipe(const Render_TextureRegion regions[], int x, int y,
                            int tex_id, CVECTOR colors[], int wipe_edge_x);

#endif  // VS_OWNED_RENDER_TEXTUREREGIONDRAW_H
