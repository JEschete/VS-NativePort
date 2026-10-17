// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Ending/Results.h"
#include "Menu/Files.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/TextureAnimation.h"

// decomp: ENDING.PRG _insertTpage (src/ENDING/ENDING.PRG/D4.c:1103)
void* Render_AppendTexturePage(int tPage, int otOffset)
{
    void** scratch;
    DR_TPAGE* drTpage;

    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    drTpage = scratch[0];
    SetDrawTPage(drTpage, 0, 1, tPage & 0xFFFF);
    AddPrim(scratch[1] + (otOffset * 4), drTpage++);
    return drTpage;
}

// decomp: ENDING.PRG _parseTim (src/ENDING/ENDING.PRG/D4.c:2235)
void Render_ParseTimImage(u_int* data, TIM_IMAGE* tim)
{

    ++data;
    tim->mode = *data;
    ++data;

    if (tim->mode & 8) {
        tim->crect = (void*)(data + 1);
        tim->caddr = (void*)(data + 3);
        data += data[0] / 4;
    } else {
        tim->crect = 0;
        tim->caddr = 0;
    }

    tim->prect = (void*)(data + 1);
    tim->paddr = (void*)(data + 3);
}

// decomp: MENU7.PRG _renderImage (src/MENU/MENU7.PRG/260.c:199)
void Render_UploadPackedRectangle(int xy, void* arg1, int wh)
{
    RECT rect;

    *(int*)&rect.x = xy;
    *(int*)&rect.w = wh;
    LoadImage(&rect, arg1);
}

// decomp: MENU7.PRG _readImage (src/MENU/MENU7.PRG/260.c:212)
void Render_ReadPackedRectangle(int xy, void* buffer, int wh)
{
    RECT rect;

    *(int*)&rect.x = xy;
    *(int*)&rect.w = wh;
    StoreImage(&rect, buffer);
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

// decomp: renderTextures.h _renderTextureFadeInTint (src/renderTextures.h:3)
void Render_DrawTextureTintFade(int x, int y, int texId, CVECTOR colors[])
{
    POLY_GT4* poly;
    void** p;

    if (colors[0].cd == 0) {
        return;
    }

    poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

    setPolyGT4(poly);
    setXY4(poly, x, y, Ending_resultAtlasRegions[texId].w + x, y, x, Ending_resultAtlasRegions[texId].h + y,
        Ending_resultAtlasRegions[texId].w + x, Ending_resultAtlasRegions[texId].h + y);
    setUV4(poly, Ending_resultAtlasRegions[texId].x, Ending_resultAtlasRegions[texId].y, Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w,
        Ending_resultAtlasRegions[texId].y, Ending_resultAtlasRegions[texId].x, Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h,
        Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w, Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);

    if (colors[0].cd < 8) {
        setRGB0(poly, (colors[0].r * colors[0].cd) / 8, (colors[0].g * colors[0].cd) / 8,
            (colors[0].b * colors[0].cd) / 8);
        setRGB1(poly, (colors[1].r * colors[0].cd) / 8, (colors[1].g * colors[0].cd) / 8,
            (colors[1].b * colors[0].cd) / 8);
        setRGB2(poly, (colors[0].r * colors[0].cd) / 8, (colors[0].g * colors[0].cd) / 8,
            (colors[0].b * colors[0].cd) / 8);
        setRGB3(poly, (colors[1].r * colors[0].cd) / 8, (colors[1].g * colors[0].cd) / 8,
            (colors[1].b * colors[0].cd) / 8);
    } else {
        setRGB0(poly, colors[0].r, colors[0].g, colors[0].b);
        setRGB1(poly, colors[1].r, colors[1].g, colors[1].b);
        setRGB2(poly, colors[0].r, colors[0].g, colors[0].b);
        setRGB3(poly, colors[1].r, colors[1].g, colors[1].b);
    }

    setSemiTrans(poly, 1);

    if (colors[0].cd < 8) {
        poly->clut = Ending_resultAtlasRegions[texId].clut + 1;
        poly->tpage = Ending_resultAtlasRegions[texId].tpage | 0x20;
    } else {
        poly->clut = Ending_resultAtlasRegions[texId].clut;
        poly->tpage = Ending_resultAtlasRegions[texId].tpage;
    }

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    AddPrim(p[1] - 0x1C, poly++);

    p[0] = poly;
}

// decomp: renderTextures.h _adjust (src/renderTextures.h:54)
int Render_BlendFlashComponent(int component, int weight)
{
    int ret = (component * weight) + ((4 - weight) * 192);
    return ret / 4;
}

// decomp: renderTextures.h _renderTexturePopIn (src/renderTextures.h:60)
void Render_DrawTextureFlashFade(int x, int y, int texId, CVECTOR colors[])
{
    POLY_GT4* poly;
    void** p;

    if (colors[0].cd != 0) {
        poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

        setPolyGT4(poly);
        setXY4(poly, x, y, Ending_resultAtlasRegions[texId].w + x, y, x, Ending_resultAtlasRegions[texId].h + y,
            Ending_resultAtlasRegions[texId].w + x, Ending_resultAtlasRegions[texId].h + y);
        setUV4(poly, Ending_resultAtlasRegions[texId].x, Ending_resultAtlasRegions[texId].y,
            Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w, Ending_resultAtlasRegions[texId].y, Ending_resultAtlasRegions[texId].x,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h, Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);

        if (colors[0].cd < 8) {
            setRGB0(poly, (colors[0].r * colors[0].cd) / 8,
                (colors[0].g * colors[0].cd) / 8, (colors[0].b * colors[0].cd) / 8);
            setRGB1(poly, (colors[1].r * colors[0].cd) / 8,
                (colors[1].g * colors[0].cd) / 8, (colors[1].b * colors[0].cd) / 8);
            setRGB2(poly, (colors[0].r * colors[0].cd) / 8,
                (colors[0].g * colors[0].cd) / 8, (colors[0].b * colors[0].cd) / 8);
            setRGB3(poly, (colors[1].r * colors[0].cd) / 8,
                (colors[1].g * colors[0].cd) / 8, (colors[1].b * colors[0].cd) / 8);
        } else if (colors[0].cd == 8) {
            setRGB0(poly, 192, 192, 192);
            setRGB1(poly, 192, 192, 192);
            setRGB2(poly, 192, 192, 192);
            setRGB3(poly, 192, 192, 192);
        } else if (colors[0].cd == 9) {
            setRGB0(poly, 224, 224, 224);
            setRGB1(poly, 224, 224, 224);
            setRGB2(poly, 224, 224, 224);
            setRGB3(poly, 224, 224, 224);
        } else if (colors[0].cd < 14) {
            int temp_a0 = colors[0].cd - 10;
            setRGB0(poly, Render_BlendFlashComponent(colors[0].r, temp_a0), Render_BlendFlashComponent(colors[0].g, temp_a0),
                Render_BlendFlashComponent(colors[0].b, temp_a0));
            setRGB1(poly, Render_BlendFlashComponent(colors[1].r, temp_a0), Render_BlendFlashComponent(colors[1].g, temp_a0),
                Render_BlendFlashComponent(colors[1].b, temp_a0));
            setRGB2(poly, Render_BlendFlashComponent(colors[0].r, temp_a0), Render_BlendFlashComponent(colors[0].g, temp_a0),
                Render_BlendFlashComponent(colors[0].b, temp_a0));
            setRGB3(poly, Render_BlendFlashComponent(colors[1].r, temp_a0), Render_BlendFlashComponent(colors[1].g, temp_a0),
                Render_BlendFlashComponent(colors[1].b, temp_a0));
        } else {
            setRGB0(poly, colors[0].r, colors[0].g, colors[0].b);
            setRGB1(poly, colors[1].r, colors[1].g, colors[1].b);
            setRGB2(poly, colors[0].r, colors[0].g, colors[0].b);
            setRGB3(poly, colors[1].r, colors[1].g, colors[1].b);
        }

        setSemiTrans(poly, 1);

        if (colors[0].cd < 10) {
            poly->clut = Ending_resultAtlasRegions[texId].clut + getClut(16, 0);
            poly->tpage = Ending_resultAtlasRegions[texId].tpage | getTPage(0, 1, 0, 0);
        } else {
            poly->clut = Ending_resultAtlasRegions[texId].clut;
            poly->tpage = Ending_resultAtlasRegions[texId].tpage;
        }

        p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(p[1] - 0x1C, poly++);

        p[0] = poly;
    }
}

// decomp: renderTextures.h _renderTextureWipe (src/renderTextures.h:130)
void Render_DrawTextureWipe(int x, int y, int texId, CVECTOR arg3[], int arg4)
{
    int temp_a1;
    int var_a0;
    int i;
    char var_s6;
    POLY_GT4* poly;
    void** scratch;

    if ((x + Ending_resultAtlasRegions[texId].w) < (arg4 - 64)) {

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        poly = scratch[0];

        setPolyGT4(poly);
        setXY4(poly, x, y, Ending_resultAtlasRegions[texId].w + x, y, x, Ending_resultAtlasRegions[texId].h + y,
            Ending_resultAtlasRegions[texId].w + x, Ending_resultAtlasRegions[texId].h + y);
        setUV4(poly, Ending_resultAtlasRegions[texId].x, Ending_resultAtlasRegions[texId].y,
            Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w, Ending_resultAtlasRegions[texId].y, Ending_resultAtlasRegions[texId].x,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h, Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);
        setRGB0(poly, arg3[0].r, arg3[0].g, arg3[0].b);
        setRGB1(poly, arg3[1].r, arg3[1].g, arg3[1].b);
        setRGB2(poly, arg3[0].r, arg3[0].g, arg3[0].b);
        setRGB3(poly, arg3[1].r, arg3[1].g, arg3[1].b);
        setSemiTrans(poly, 1);

        poly->clut = Ending_resultAtlasRegions[texId].clut;
        poly->tpage = Ending_resultAtlasRegions[texId].tpage;

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(scratch[1] - 0x1C, poly++);

        scratch[0] = poly;
        return;
    }

    if (x < arg4) {

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        poly = scratch[0];
        var_s6 = Ending_resultAtlasRegions[texId].x;

        for (i = 0; i < Ending_resultAtlasRegions[texId].w; i += 12, x += 12, var_s6 += 12) {

            var_a0 = 12;

            if ((i + 12) >= Ending_resultAtlasRegions[texId].w) {
                var_a0 = Ending_resultAtlasRegions[texId].w - i;
            }

            setPolyGT4(poly);
            temp_a1 = x + var_a0;
            setXY4(poly, x, y, temp_a1, y, x, Ending_resultAtlasRegions[texId].h + y, temp_a1,
                Ending_resultAtlasRegions[texId].h + y);
            setUV4(poly, var_s6, Ending_resultAtlasRegions[texId].y, var_s6 + var_a0, Ending_resultAtlasRegions[texId].y,
                var_s6, Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h, var_s6 + var_a0,
                Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);

            var_a0 = arg4 - x;

            if (var_a0 > 64) {
                var_a0 = 64;
            }

            if (var_a0 < 0) {
                var_a0 = 0;
            }

            setRGB0(poly, (arg3[0].r * var_a0) / 64, (arg3[0].g * var_a0) / 64,
                (arg3[0].b * var_a0) / 64);
            setRGB2(poly, (arg3[0].r * var_a0) / 64, (arg3[0].g * var_a0) / 64,
                (arg3[0].b * var_a0) / 64);

            var_a0 = arg4 - temp_a1;

            if (var_a0 > 64) {
                var_a0 = 64;
            }

            if (var_a0 < 0) {
                var_a0 = 0;
            }

            setRGB1(poly, (arg3[1].r * var_a0) / 64, (arg3[1].g * var_a0) / 64,
                (arg3[1].b * var_a0) / 64);
            setRGB3(poly, (arg3[1].r * var_a0) / 64, (arg3[1].g * var_a0) / 64,
                (arg3[1].b * var_a0) / 64);

            setSemiTrans(poly, 1);

            poly->clut = (Ending_resultAtlasRegions[texId].clut + 1);
            poly->tpage = (Ending_resultAtlasRegions[texId].tpage | 0x20);

            scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            AddPrim(scratch[1] - 0x1C, poly++);
        }

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        scratch[0] = poly;
    }
}

