// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "Title/TitleScreen.h"

// decomp: _titleScreenBg (_titleScreenBg@TITLE/TITLE.PRG)
u_int Title_titleScreenBackground[222932 / sizeof(u_int)]; /* contents: module image (ModuleData.cpp) */

// decomp: _menuCopyright (_menuCopyright@TITLE/TITLE.PRG)
u_short Title_copyrightImage[8640 / sizeof(u_short)]; /* contents: module image (ModuleData.cpp) */

// decomp: _4 (_4@src/TITLE/TITLE.PRG/22C.c)
char Title_movieLocationPadding[4];

// decomp: _5 (_5@src/TITLE/TITLE.PRG/22C.c)
char Title_movieStatePadding[28];

// decomp: _6 (_6@src/TITLE/TITLE.PRG/22C.c)
char Title_movieDecoderPadding[4];

// decomp: outlineTpages (outlineTpages@TITLE/TITLE.PRG)
int Title_outlineTexturePageWords[] = { vs_getTpage(0, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(64, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(128, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(448, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(0, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(64, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(128, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(448, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(448, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(448, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off) };

// decomp: outlineUvs (outlineUvs@TITLE/TITLE.PRG)
int Title_outlineUvClutWords[] = { vs_getUV0Clut(0, 192, 480, 394), vs_getUV0Clut(0, 192, 480, 394),
    vs_getUV0Clut(0, 192, 480, 394), vs_getUV0Clut(128, 0, 480, 394),
    vs_getUV0Clut(144, 192, 480, 394), vs_getUV0Clut(160, 192, 480, 394),
    vs_getUV0Clut(160, 192, 480, 394), vs_getUV0Clut(128, 64, 480, 394),
    vs_getUV0Clut(128, 0, 480, 394), vs_getUV0Clut(128, 64, 480, 394) };

// decomp: textTpages (textTpages@TITLE/TITLE.PRG)
int Title_textTexturePageWords[] = { vs_getTpage(512, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(576, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(576, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(640, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(768, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(704, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(640, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(704, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(768, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off),
    vs_getTpage(832, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off) };

// decomp: textUvs (textUvs@TITLE/TITLE.PRG)
int Title_textUvClutWords[] = { vs_getUV0Clut(128, 224, 480, 384), vs_getUV0Clut(0, 224, 480, 385),
    vs_getUV0Clut(128, 224, 480, 386), vs_getUV0Clut(0, 224, 480, 387),
    vs_getUV0Clut(0, 224, 480, 388), vs_getUV0Clut(0, 224, 480, 389),
    vs_getUV0Clut(128, 224, 480, 390), vs_getUV0Clut(128, 224, 480, 391),
    vs_getUV0Clut(128, 224, 480, 392), vs_getUV0Clut(0, 224, 480, 393) };

}  // extern "C"
