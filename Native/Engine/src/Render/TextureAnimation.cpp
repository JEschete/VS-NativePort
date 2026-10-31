// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Files.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/TextureAnimation.h"

// decomp: ENDING.PRG _insertTpage (src/ENDING/ENDING.PRG/D4.c:1103)
void* Render_AppendTexturePage(int tPage, int otOffset)
{
    void** scratch;
    DR_TPAGE* drTpage;

    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    drTpage = (DR_TPAGE *)(scratch[0]);
    SetDrawTPage(drTpage, 0, 1, tPage & 0xFFFF);
    AddPrim(((u_char*)(scratch[1])) + (otOffset * 4), drTpage++);
    return drTpage;
}

// decomp: ENDING.PRG _parseTim (src/ENDING/ENDING.PRG/D4.c:2235)
void Render_ParseTimImage(u_int* data, TIM_IMAGE* tim)
{

    ++data;
    tim->mode = *data;
    ++data;

    if (tim->mode & 8) {
        tim->crect = (RECT *)((void*)(data + 1));
        tim->caddr = (u_long *)((void*)(data + 3));
        data += data[0] / 4;
    } else {
        tim->crect = 0;
        tim->caddr = 0;
    }

    tim->prect = (RECT *)((void*)(data + 1));
    tim->paddr = (u_long *)((void*)(data + 3));
}

// decomp: MENU7.PRG _renderImage (src/MENU/MENU7.PRG/260.c:199)
void Render_UploadPackedRectangle(int xy, void* pixels, int wh)
{
    RECT rect;

    *(int*)&rect.x = xy;
    *(int*)&rect.w = wh;
    LoadImage(&rect, (u_long *)(pixels));
}

// decomp: MENU7.PRG _readImage (src/MENU/MENU7.PRG/260.c:212)
void Render_ReadPackedRectangle(int xy, void* buffer, int wh)
{
    RECT rect;

    *(int*)&rect.x = xy;
    *(int*)&rect.w = wh;
    StoreImage(&rect, (u_long *)(buffer));
}

// decomp: MENU7.PRG _drawSprt (src/MENU/MENU7.PRG/260.c:1188)
void Render_DrawImmediateSprite(int xy, int uvClut, int wh, int tpage)
{
    Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.sprt, Render_LegacyPrimitiveAddress_Null);
    Menu_Files_primBuf.prim.sprt.texturePageCommand = vs_getTpageRaw(tpage & 0x9FF);
    Menu_Files_primBuf.prim.sprt.colorCommand =
        vs_getRGB0Raw(Render_LegacyPrimitiveCode_Sprite, ((0x80 - (tpage >> 16)) * 0x010101));
    Menu_Files_primBuf.prim.sprt.position = xy;
    Menu_Files_primBuf.prim.sprt.uvPalette = uvClut;
    Menu_Files_primBuf.prim.sprt.size = wh;
    DrawPrim(&Menu_Files_primBuf);
}

// decomp: MENU7.PRG _interpolateRGB (src/MENU/MENU7.PRG/260.c:1718)
u_int Render_BlendRgb(u_int colour1, u_int colour2, u_int factor)
{
    int component;
    u_int i;
    u_int colourOut;

    colourOut = 0;
    for (i = 0; i < 3; ++i) {
        component = ((colour1 >> 16) & 0xFF) * (8 - factor);
        colour1 <<= 8;
        colourOut =
            (colourOut << 8) + ((component + (((colour2 >> 16) & 0xFF) * factor)) >> 3);
        colour2 <<= 8;
    }
    return colourOut;
}

// decomp: renderTextures.h _adjust (src/renderTextures.h:54)
int Render_BlendFlashComponent(int component, int weight)
{
    int ret = (component * weight) + ((4 - weight) * 192);
    return ret / 4;
}

}  // extern "C"
