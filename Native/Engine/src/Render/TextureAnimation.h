// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_TEXTUREANIMATION_H_TYPES
#define VS_RENDER_TEXTUREANIMATION_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:98
#define vs_getTag(type, addr) (((sizeof(type) / 4) << 24) | ((addr) & 0xFFFFFF))

// decomp: src/include/gpu.h:101
#define vs_getTpageRaw(tpageVal) ((0xE1 << 24) | ((tpageVal) & 0xFFFF))

#endif // VS_RENDER_TEXTUREANIMATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_TEXTUREANIMATION_H_DECLS)
#define VS_RENDER_TEXTUREANIMATION_H_DECLS

void* Render_AppendTexturePage(int tPage, int otOffset);
void Render_ParseTimImage(u_int* data, TIM_IMAGE* tim);
void Render_UploadPackedRectangle(int xy, void* arg1, int wh);
void Render_ReadPackedRectangle(int xy, void* buffer, int wh);
void Render_DrawImmediateSprite(int xy, int uvClut, int wh, int tpage);
u_int Render_BlendRgb(u_int colour1, u_int colour2, u_int factor);
void Render_DrawTextureTintFade(int x, int y, int texId, CVECTOR colors[]);
int Render_BlendFlashComponent(int component, int weight);
void Render_DrawTextureFlashFade(int x, int y, int texId, CVECTOR colors[]);
void Render_DrawTextureWipe(int x, int y, int texId, CVECTOR arg3[], int arg4);

#endif // VS_RENDER_TEXTUREANIMATION_H_DECLS
