// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_FONT_H_TYPES
#define VS_RENDER_FONT_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:96
#define vs_getXY_2(x, y) (((x) & 0xFFFF) | ((y) << 16))

// decomp: src/include/gpu.h:97
#define vs_getWH(w, h) vs_getXY((w), (h))

// decomp: src/include/gpu.h:102
#define vs_getRGB(r, g, b) vs_getRGB0(0, (r), (g), (b))

// decomp: src/include/gpu.h:103
#define vs_getRGB0(code, r, g, b)                                                        \
    (((code) << 24) | (((b) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((r) & 0xFF))

// decomp: src/include/gpu.h:105
#define vs_getRGB0Raw(code, rgb0) (((code) << 24) | (rgb0))

// decomp: src/include/gpu.h:109
#define vs_getUV0Clut(u, v, x, y) (((u) | ((v) << 8)) | (getClut((x), (y)) << 16))

#endif // VS_RENDER_FONT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_FONT_H_DECLS)
#define VS_RENDER_FONT_H_DECLS
#include "Battle/TextBoxes.h"
#include "Menu/Framework/TextBoxes.h"

extern u_short Render_kGlyphPixelExpansion[16];
extern char Render_fontPaletteOffset;
extern char Render_fontTableSelect;
extern int Render_fontColor;
extern u_char Render_activeTextBoxIndex;
void Render_DrawCompactTextColor(char const* text, int x, int color, u_long* nextPrim);
void Render_DrawCompactText(char const* text, int xy, void* nextPrim);
void Render_TerminateEncodedString(char* str);
char* Render_FormatEncodedText(char* dest, char* src);
int Render_MeasureEncodedTextLine(char* str);
int Render_DrawVariableWidthGlyph(u_int glyphId, int x, int y, u_long* ot);
void Render_SetMenuFontStyle(int palette_offset);
void Render_DrawProgressiveTextBoxText(Menu_TextBoxState* text_box);
void Render_DrawScaledTextBoxText(Menu_TextBoxState* ctx, int scale);
void Render_BlendGlyphIntoPixels(u_short* destination, u_int glyph, int stride, u_int alpha);
void Render_BlendTextBoxIntoFramebuffer(Menu_TextBoxState* text_box, int glyph_alpha);

#endif // VS_RENDER_FONT_H_DECLS
