// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_UIPRIMITIVES_H_TYPES
#define VS_RENDER_UIPRIMITIVES_H_TYPES
#include "Legacy/Psx.h"


// decomp: vs_scratch (vs_scratch@src/include/scratch.h): the start of scratch memory
#define Render_frameScratch (*(Render_FrameScratch*)Core_ScratchBase())

// decomp: src/include/gpu.h:99
#define vs_getTpage(x, y, tp, abr, dtd)                                                  \
    ((0xE1 << 24) | (((dtd) & 1) << 9) | getTPage((tp), (abr), (x), (y)))

#endif // VS_RENDER_UIPRIMITIVES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_UIPRIMITIVES_H_DECLS)
#define VS_RENDER_UIPRIMITIVES_H_DECLS

extern int Render_kStatGaugeColors[4];
void Render_AppendOverlayPrimitive(int* arg0);
void Render_SplicePrimitiveList(u_long* arg0, int* arg1);
u_int Render_BlendRgbWithBrightness(u_int color0, u_int color1, int brightness, int blendFactor);
void Render_DrawNumericGlyph(int font, int xy, int digit, u_long* before);
int Render_DrawIntegerRightAligned(int font, int xy, int value, u_long* before);
void Render_DrawStatGauge(int colorIndex, int w, u_long* before, int xy);
void Render_InsertThreeWordPrimitive(u_long* before, int rgb0, int xy, int wh);
void Render_InsertDrawModeCommand(int tpage, u_long* before);
void Render_UploadImageRectangle(int xy, void* buffer, int wh);

#endif // VS_RENDER_UIPRIMITIVES_H_DECLS
