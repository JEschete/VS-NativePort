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
void Render_UploadPackedRectangle(int xy, void* pixels, int wh)
{
    RECT rect;

    *(int*)&rect.x = xy;
    *(int*)&rect.w = wh;
    LoadImage(&rect, pixels);
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
            int flash_blend_weight = colors[0].cd - 10;
            setRGB0(poly, Render_BlendFlashComponent(colors[0].r, flash_blend_weight), Render_BlendFlashComponent(colors[0].g, flash_blend_weight),
                Render_BlendFlashComponent(colors[0].b, flash_blend_weight));
            setRGB1(poly, Render_BlendFlashComponent(colors[1].r, flash_blend_weight), Render_BlendFlashComponent(colors[1].g, flash_blend_weight),
                Render_BlendFlashComponent(colors[1].b, flash_blend_weight));
            setRGB2(poly, Render_BlendFlashComponent(colors[0].r, flash_blend_weight), Render_BlendFlashComponent(colors[0].g, flash_blend_weight),
                Render_BlendFlashComponent(colors[0].b, flash_blend_weight));
            setRGB3(poly, Render_BlendFlashComponent(colors[1].r, flash_blend_weight), Render_BlendFlashComponent(colors[1].g, flash_blend_weight),
                Render_BlendFlashComponent(colors[1].b, flash_blend_weight));
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
void Render_DrawTextureWipe(int x, int y, int texId, CVECTOR colors[], int wipe_edge_x)
{
    int strip_right_x;
    int strip_width_or_fade_weight;
    int i;
    char texture_u;
    POLY_GT4* poly;
    void** scratch;

    if ((x + Ending_resultAtlasRegions[texId].w) < (wipe_edge_x - 64)) {

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        poly = scratch[0];

        setPolyGT4(poly);
        setXY4(poly, x, y, Ending_resultAtlasRegions[texId].w + x, y, x, Ending_resultAtlasRegions[texId].h + y,
            Ending_resultAtlasRegions[texId].w + x, Ending_resultAtlasRegions[texId].h + y);
        setUV4(poly, Ending_resultAtlasRegions[texId].x, Ending_resultAtlasRegions[texId].y,
            Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w, Ending_resultAtlasRegions[texId].y, Ending_resultAtlasRegions[texId].x,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h, Ending_resultAtlasRegions[texId].x + Ending_resultAtlasRegions[texId].w,
            Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);
        setRGB0(poly, colors[0].r, colors[0].g, colors[0].b);
        setRGB1(poly, colors[1].r, colors[1].g, colors[1].b);
        setRGB2(poly, colors[0].r, colors[0].g, colors[0].b);
        setRGB3(poly, colors[1].r, colors[1].g, colors[1].b);
        setSemiTrans(poly, 1);

        poly->clut = Ending_resultAtlasRegions[texId].clut;
        poly->tpage = Ending_resultAtlasRegions[texId].tpage;

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(scratch[1] - 0x1C, poly++);

        scratch[0] = poly;
        return;
    }

    if (x < wipe_edge_x) {

        scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        poly = scratch[0];
        texture_u = Ending_resultAtlasRegions[texId].x;

        for (i = 0; i < Ending_resultAtlasRegions[texId].w; i += 12, x += 12, texture_u += 12) {

            strip_width_or_fade_weight = 12;

            if ((i + 12) >= Ending_resultAtlasRegions[texId].w) {
                strip_width_or_fade_weight = Ending_resultAtlasRegions[texId].w - i;
            }

            setPolyGT4(poly);
            strip_right_x = x + strip_width_or_fade_weight;
            setXY4(poly, x, y, strip_right_x, y, x, Ending_resultAtlasRegions[texId].h + y, strip_right_x,
                Ending_resultAtlasRegions[texId].h + y);
            setUV4(poly, texture_u, Ending_resultAtlasRegions[texId].y, texture_u + strip_width_or_fade_weight, Ending_resultAtlasRegions[texId].y,
                texture_u, Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h, texture_u + strip_width_or_fade_weight,
                Ending_resultAtlasRegions[texId].y + Ending_resultAtlasRegions[texId].h);

            strip_width_or_fade_weight = wipe_edge_x - x;

            if (strip_width_or_fade_weight > 64) {
                strip_width_or_fade_weight = 64;
            }

            if (strip_width_or_fade_weight < 0) {
                strip_width_or_fade_weight = 0;
            }

            setRGB0(poly, (colors[0].r * strip_width_or_fade_weight) / 64, (colors[0].g * strip_width_or_fade_weight) / 64,
                (colors[0].b * strip_width_or_fade_weight) / 64);
            setRGB2(poly, (colors[0].r * strip_width_or_fade_weight) / 64, (colors[0].g * strip_width_or_fade_weight) / 64,
                (colors[0].b * strip_width_or_fade_weight) / 64);

            strip_width_or_fade_weight = wipe_edge_x - strip_right_x;

            if (strip_width_or_fade_weight > 64) {
                strip_width_or_fade_weight = 64;
            }

            if (strip_width_or_fade_weight < 0) {
                strip_width_or_fade_weight = 0;
            }

            setRGB1(poly, (colors[1].r * strip_width_or_fade_weight) / 64, (colors[1].g * strip_width_or_fade_weight) / 64,
                (colors[1].b * strip_width_or_fade_weight) / 64);
            setRGB3(poly, (colors[1].r * strip_width_or_fade_weight) / 64, (colors[1].g * strip_width_or_fade_weight) / 64,
                (colors[1].b * strip_width_or_fade_weight) / 64);

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

