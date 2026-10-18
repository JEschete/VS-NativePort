// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Stream.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Ending/ClearResults.h"
#include "Ending/Credits.h"
#include "Ending/Credits/StaffRoll.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "State/PlayTime.h"

// decomp: _illustrationTimers (_illustrationTimers@ENDING/ENDING.PRG)
u_short Ending_Credits_illustrationHoldFrames[16]; /* contents: module image (ModuleData.c) */

// decomp: D_800DBB18 (D_800DBB18@ENDING/ENDING.PRG)
int Ending_Credits_ripplePhaseAngle;

// decomp: D_800DBB1C (D_800DBB1C@ENDING/ENDING.PRG)
int Ending_Credits_rippleScaleFrame;

// decomp: D_800DBB28 (D_800DBB28@ENDING/ENDING.PRG)
u_char Ending_Credits_rippleDistanceGrid[4][16];

// decomp: D_800DBB68 (D_800DBB68@ENDING/ENDING.PRG)
short Ending_Credits_illustrationBrightness;

// decomp: _creditsElements (_creditsElements@ENDING/ENDING.PRG)
Ending_Credits_Element Ending_Credits_elements[32];

// decomp: D_800DC188 (D_800DC188@ENDING/ENDING.PRG)
u_char Ending_Credits_hasFinished;

// decomp: D_800DC189 (D_800DC189@ENDING/ENDING.PRG)
u_char Ending_Credits_isBackgroundFading;

// decomp: _skipInvoked (_skipInvoked@ENDING/ENDING.PRG)
u_char Ending_Credits_wasSkipRequested;

// decomp: D_800DC18C (D_800DC18C@ENDING/ENDING.PRG)
int Ending_Credits_elapsedFrames;

// decomp: _textStreamDelay (_textStreamDelay@ENDING/ENDING.PRG)
int Ending_Credits_scriptDelayFrames;

// decomp: _creditsBytestreamHead (_creditsBytestreamHead@ENDING/ENDING.PRG)
u_char* Ending_Credits_scriptCursor;

// decomp: _scrollSpeed (_scrollSpeed@ENDING/ENDING.PRG)
short Ending_Credits_scrollSpeedFixed8;

// decomp: D_800DC19C (D_800DC19C@ENDING/ENDING.PRG)
u_short Ending_Credits_backgroundBrightness;

// decomp: vs_main_frameDuration (vs_main_frameDuration@SLUS_010.40)
u_int Ending_Credits_xaResumeFrameCount;

// decomp: ENDING.PRG _renderPhantomPain (src/ENDING/ENDING.PRG/D4.c:218)
void Ending_Credits_DrawPhantomPainRipple(Ending_Credits_Element* element)
{
    SVECTOR rot;
    SVECTOR vertices[4];
    MATRIX matrix;
    long p;
    long flag;
    u_short brightness;
    u_short scale;
    u_short i;
    u_short j;
    POLY_FT4* poly;
    int dx;
    int dy;
    int shift;
    int d;

    poly = *(POLY_FT4**)((uintptr_t)Core_ScratchBase() + 0x000);
    memset(&rot, 0, sizeof(rot));

    switch (element->phase) {
    case 1:
        Ending_Credits_ripplePhaseAngle = 0;
        element->animationFrame = 0;
        element->phase = 2;
        Ending_Credits_rippleScaleFrame = 0;

        for (i = 0; i < 4; ++i) {
            shift = 2;
            d = 2 - i;
            dy = (ABS(d) << shift) * (ABS(d) << shift);
            for (j = 0; j < 16; ++j) {
                dx = 8 - j;
                dx = ABS(dx) * 18;
                Ending_Credits_rippleDistanceGrid[i][j] = Core_Math_SquareRootInteger(dx * dx + dy);
            }
        }
        /* fallthrough */

    case 2:
        brightness = (element->animationFrame << 7) / 90;
        ++Ending_Credits_rippleScaleFrame;
        if (++element->animationFrame == 90) {
            element->animationFrame = 0;
            element->phase = 3;
        }
        break;

    case 3:
        brightness = 128;
        if (++element->animationFrame == 120) {
            element->animationFrame = 0;
            element->phase = 4;
        }
        break;

    case 4:
        brightness = ((90 - element->animationFrame) << 7) / 90;
        if (++element->animationFrame == 90) {
            element->phase = -1;
        }
        break;
    }

    Ending_Credits_ripplePhaseAngle += 200;
    scale = (Core_Math_EvaluateEase(3, 90 - Ending_Credits_rippleScaleFrame, 90) * 65) >> 10;

    Core_Gte_RotMatrix(&rot, &matrix);
    matrix.t[0] = matrix.t[1] = 0;
    matrix.t[2] = 512;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 16; ++j) {
            vertices[0].vx = vertices[2].vx = j * 9 - 72;
            vertices[1].vx = vertices[3].vx = (j + 1) * 9 - 72;
            vertices[0].vy = vertices[1].vy = i * 4 - 8;
            vertices[2].vy = vertices[3].vy = (i + 1) * 4 - 8;
            vertices[0].vz =
                (Core_Math_SineFromTable(Ending_Credits_ripplePhaseAngle + Ending_Credits_rippleDistanceGrid[i][j] * 400) * scale) >> 12;
            vertices[1].vz =
                (Core_Math_SineFromTable(Ending_Credits_ripplePhaseAngle + Ending_Credits_rippleDistanceGrid[i][j + 1] * 400) * scale) >> 12;
            vertices[2].vz =
                (Core_Math_SineFromTable(Ending_Credits_ripplePhaseAngle + Ending_Credits_rippleDistanceGrid[i + 1][j] * 400) * scale) >> 12;
            vertices[3].vz =
                (Core_Math_SineFromTable(Ending_Credits_ripplePhaseAngle + Ending_Credits_rippleDistanceGrid[i + 1][j + 1] * 400) * scale)
                >> 12;

            SetPolyFT4(poly);
            setSemiTrans(poly, 1);
            Core_Gte_RotTransPers4(&vertices[0], &vertices[1], &vertices[2], &vertices[3],
                (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2, (long*)&poly->x3,
                &p, &flag);
            poly->u0 = poly->u2 = j * 9;
            poly->u1 = poly->u3 = (j + 1) * 9;
            poly->v0 = poly->v1 = i * 4 + 180;
            poly->v2 = poly->v3 = (i + 1) * 4 + 180;
            poly->clut = getClut(896, 83);
            poly->tpage = 46;
            poly->r0 = poly->g0 = poly->b0 = brightness;
            AddPrim(((void**)((uintptr_t)Core_ScratchBase() + 0x000))[1] + 12, poly++);
        }
    }
    *(POLY_FT4**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;
}

// decomp: ENDING.PRG _renderFin (src/ENDING/ENDING.PRG/D4.c:325)
void Ending_Credits_DrawFinCaption(Ending_Credits_Element* arg0)
{
    int temp_a0_2;
    int temp_a0;
    int temp_v0;
    u_char var_s0;
    void** p;
    int new_var;

    SPRT* sprt = *(SPRT**)((uintptr_t)Core_ScratchBase() + 0x000);

    switch (arg0->phase) {
    case 1:
        arg0->animationFrame = 0;
        arg0->phase = 2;
        /* fallthrough */

    case 2:
        temp_v0 = arg0->animationFrame;
        arg0->animationFrame = temp_v0 + 1;
        var_s0 = (temp_v0 << 7) / 60;

        if (arg0->animationFrame == 60) {
            arg0->animationFrame = 0;
            arg0->phase = 3;
        }
        break;

    case 3:
        var_s0 = 128;

        if (++arg0->animationFrame == 240) {
            arg0->animationFrame = 0;
            arg0->phase = 4;
        }
        break;

    case 4:
        temp_a0 = arg0->animationFrame;
        arg0->animationFrame = temp_a0 + 1;
        var_s0 = ((0x3C - temp_a0) << 7) / 60;

        if (arg0->animationFrame == 60) {
            arg0->phase = -1;
        }

        break;
    }

    SetSprt(sprt);
    setXY0(sprt, 248, 184);
    setUV0(sprt, 0, 160);
    setWH(sprt, 36, 17);
    sprt->r0 = sprt->g0 = sprt->b0 = var_s0;
    setClut(sprt, 896, 82);

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    setaddr(sprt, ((u_long*)p[1])[1]);
    setaddr(p[1] + 4, sprt++);

    new_var = 0;
    p[new_var] = sprt;
    p[new_var] = Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 896, 0), 1);
}

// decomp: ENDING.PRG _renderVagrantStory (src/ENDING/ENDING.PRG/D4.c:390)
void Ending_Credits_DrawScrollingTitleLogo(Ending_Credits_Element* element)
{
    SPRT* sprt;
    int var_fp;
    void** s0;
    int new_var;

    sprt = *(SPRT**)((uintptr_t)Core_ScratchBase() + 0x000);

    switch (element->phase) {
    case 1:
        element->xFixed8 = 61 << 8;
        element->yFixed8 = 80 << 8;
        element->animationFrame = 0;
        element->phase = 2;
        /* fallthrough */

    case 2:
        var_fp = (Ending_Credits_backgroundBrightness * ++element->animationFrame) / 60;

        if (element->animationFrame == 60) {
            element->phase = 3;
            element->animationFrame = 0;
        }
        break;

    case 3:
        var_fp = Ending_Credits_backgroundBrightness;

        if (++element->animationFrame == 60) {
            element->phase = 4;
        }
        break;

    case 4:
        var_fp = Ending_Credits_backgroundBrightness;
        element->yFixed8 += Ending_Credits_scrollSpeedFixed8;

        if (element->yFixed8 < -(60 << 8)) {
            element->phase = -1;
            return;
        }
        break;
    }

    SetSprt(sprt);
    setSemiTrans(sprt, 1);
    setXY0(sprt, element->xFixed8 >> 8, element->yFixed8 >> 8);
    setUV0(sprt, 0, 96);
    setWH(sprt, 208, 48);
    setClut(sprt, 896, 80);
    sprt->r0 = sprt->g0 = sprt->b0 = var_fp;

    s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    setaddr(sprt, getaddr(s0[1] + 12));
    setaddr(s0[1] + 12, sprt++);

    new_var = 0;
    s0[new_var] = sprt;

    sprt = Render_AppendTexturePage(0x2E, 3);

    SetSprt(sprt);
    setSemiTrans(sprt, 1);
    setXY0(sprt, element->xFixed8 >> 8, element->yFixed8 >> 8);
    setUV0(sprt, 0, 0x60);
    setWH(sprt, 0xD0, 0x30);
    setClut(sprt, 896, 81);
    sprt->r0 = sprt->g0 = sprt->b0 = var_fp;

    setaddr(sprt, getaddr(s0[1] + 12));
    setaddr(s0[1] + 12, sprt++);

    s0[new_var] = sprt;
    s0[new_var] = Render_AppendTexturePage(0x4E, 3);
    return;
}

// decomp: ENDING.PRG _inverseDistanceFromScreencenter (src/ENDING/ENDING.PRG/D4.c:468)
int Ending_Credits_DistanceFromScreenCenter(int arg0, int arg1)
{
    arg0 -= 160;
    arg1 -= 112;
    return Core_Math_SquareRootInteger(arg0 * arg0 + arg1 * arg1);
}

// decomp: ENDING.PRG func_80069388 (src/ENDING/ENDING.PRG/D4.c:475)
void Ending_Credits_ApplyRadialVertexFade(Ending_Credits_TexturedGradientQuadView* arg0, short arg1, short arg2)
{
    short temp_s0;
    short v0;

    if (arg2 == 0) {
        temp_s0 = (arg1 * 128) / 80;
        v0 = temp_s0
           + (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexZeroX, arg0->vertexZeroY)) << 7)
               / 400);

        if (v0 < 0x81) {
            arg0->vertexZeroBlue = v0;
        } else {
            arg0->vertexZeroBlue = 128;
        }

        arg0->vertexZeroRed = arg0->vertexZeroGreen = arg0->vertexZeroBlue;

        v0 = temp_s0
           + (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexOneX, arg0->vertexOneY)) << 7)
               / 400);

        if (v0 < 0x81) {
            arg0->vertexOneBlue = v0;
        } else {
            arg0->vertexOneBlue = 128;
        }

        arg0->vertexOneRed = arg0->vertexOneGreen = arg0->vertexOneBlue;

        v0 = temp_s0
           + (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexTwoX, arg0->vertexTwoY)) << 7)
               / 400);

        if (v0 < 0x81) {
            arg0->vertexTwoBlue = v0;
        } else {
            arg0->vertexTwoBlue = 128;
        }

        arg0->vertexTwoRed = arg0->vertexTwoGreen = arg0->vertexTwoBlue;

        v0 = temp_s0
           + (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexThreeX, arg0->vertexThreeY)) << 7)
               / 400);

        if (v0 < 0x81) {
            arg0->vertexThreeBlue = v0;
        } else {
            arg0->vertexThreeBlue = 128;
        }

    } else {

        temp_s0 = ((0x50 - arg1) << 7) / 80;
        v0 = temp_s0
           - (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexZeroX, arg0->vertexZeroY)) << 7)
               / 400);

        if (v0 >= 0) {
            arg0->vertexZeroBlue = v0;
        } else {
            arg0->vertexZeroBlue = 0;
        }

        arg0->vertexZeroRed = arg0->vertexZeroGreen = arg0->vertexZeroBlue;

        v0 = temp_s0
           - (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexOneX, arg0->vertexOneY)) << 7)
               / 400);

        if (v0 >= 0) {
            arg0->vertexOneBlue = v0;
        } else {
            arg0->vertexOneBlue = 0;
        }

        arg0->vertexOneRed = arg0->vertexOneGreen = arg0->vertexOneBlue;

        v0 = temp_s0
           - (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexTwoX, arg0->vertexTwoY)) << 7)
               / 400);

        if (v0 >= 0) {
            arg0->vertexTwoBlue = v0;
        } else {
            arg0->vertexTwoBlue = 0;
        }

        arg0->vertexTwoRed = arg0->vertexTwoGreen = arg0->vertexTwoBlue;

        v0 = temp_s0
           - (((200 - Ending_Credits_DistanceFromScreenCenter(arg0->vertexThreeX, arg0->vertexThreeY)) << 7)
               / 400);

        if (v0 >= 0) {
            arg0->vertexThreeBlue = v0;
        } else {
            arg0->vertexThreeBlue = 0;
        }
    }

    arg0->vertexThreeRed = arg0->vertexThreeGreen = arg0->vertexThreeBlue;
}

// decomp: ENDING.PRG _renderllustration (src/ENDING/ENDING.PRG/D4.c:583)
void Ending_Credits_DrawIllustration(Ending_Credits_Element* element)
{
    SPRT* sprt;
    short temp_v0;
    void** p;

    temp_v0 = element->phase - 1;

    switch (temp_v0) {
    case 0:
        if ((element->illustrationIndex >= 4) && (element->illustrationIndex != 15)) {
            Ending_Credits_QueueIllustrationDecode(Ending_Credits_illustrationTimWorkspace, Ending_Credits_illustrationResources[element->illustrationIndex + 1]);
        }

        element->animationFrame = 0;
        element->phase = 2;
        Ending_Credits_illustrationTexturePageX = (element->illustrationIndex % 5) * 192;
        Ending_Credits_illustrationPaletteY = (element->illustrationIndex % 5) + 480;
        /* fallthrough */

    case 1:
        Ending_Credits_illustrationBrightness = (Ending_Credits_backgroundBrightness * element->animationFrame) / 80;

        if (++element->animationFrame == 80) {
            int var_a0 = 3;
            element->animationFrame = 0;
            if (element->illustrationIndex == 15) {
                var_a0 = 5;
            }
            element->phase = var_a0;
        }
        break;

    case 2:
        Ending_Credits_illustrationBrightness = 128;

        if (++element->animationFrame == Ending_Credits_illustrationHoldFrames[element->illustrationIndex]) {
            element->animationFrame = 0;
            element->phase = 4;
        }

        break;

    case 3:
        Ending_Credits_illustrationBrightness = (Ending_Credits_backgroundBrightness * (80 - element->animationFrame)) / 80;

        if (++element->animationFrame == 80) {
            if (++element->illustrationIndex >= 5) {
                Ending_Credits_UploadTexture(Ending_Credits_illustrationTimWorkspace);
            }
            element->phase = 1;
        }
        break;

    case 4:
        Ending_Credits_illustrationBrightness = 0x80;
        break;
    }

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    sprt = p[0];

    SetSprt(sprt);
    setXY0(sprt, 0, 0);
    setWH(sprt, 0xA0, 0xE0);
    setUV0(sprt, 0, 0);
    setSemiTrans(sprt, 1);
    setClut(sprt, 0, Ending_Credits_illustrationPaletteY);
    sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_illustrationBrightness;

    AddPrim(p[1] + 0x10, sprt++);

    p[0] = sprt;
    sprt = Render_AppendTexturePage(getTPage(1, 2, Ending_Credits_illustrationTexturePageX, 256), 4);

    SetSprt(sprt);
    setXY0(sprt, 0xA0, 0);
    setWH(sprt, 0xA0, 0xE0);
    setUV0(sprt, 0x20, 0);
    setSemiTrans(sprt, 1);
    setClut(sprt, 0, Ending_Credits_illustrationPaletteY);
    sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_illustrationBrightness;

    AddPrim(p[1] + 0x10, sprt++);

    p[0] = sprt;
    p[0] = Render_AppendTexturePage(getTPage(1, 2, Ending_Credits_illustrationTexturePageX + 64, 256), 4);
}

// decomp: ENDING.PRG _creditsStrWidth (src/ENDING/ENDING.PRG/D4.c:672)
int Ending_Credits_MeasureTextWidth(u_char* str)
{
    short width;
    short i;
    unsigned short charTableOffset;
    short len;
    int c;
    int new_var;

    len = *str++;
    width = 0;
    charTableOffset = 0;
    i = 0;

    for (i = 0; i < len; ++i, ++str) {

        c = *str;

        if (c < 32u) {
            switch (c) {
            case 1:
                charTableOffset = 0;
                break;
            case 2:
                charTableOffset = 112;
                break;
            case 31:
                --width;
                break;
            }
        } else {
            width += Ending_Credits_glyphWidths[(charTableOffset - (new_var = 32)) + c] - 1;
        }
    }

    return width;
}

// decomp: ENDING.PRG _renderText (src/ENDING/ENDING.PRG/D4.c:711)
void Ending_Credits_DrawScrollingText(Ending_Credits_Element* element)
{
    void** p;
    SPRT* sprt;
    POLY_FT4* poly;
    u_char* str;
    short i;
    short len;
    u_char c;
    u_char glyph;
    u_char charTableOffset;
    short underline;
    short x;
    short y;
    u_short width;
    short glyphWidth;

    sprt = *(SPRT**)((uintptr_t)Core_ScratchBase() + 0x000);
    charTableOffset = 0;
    underline = 0;

    if (element->phase == 1) {
        width = element->width = Ending_Credits_MeasureTextWidth(element->textBytes);
        element->xFixed8 = ((320 - width) >> 1) << 8;
        element->yFixed8 = 224 << 8;
        element->phase = 2;
    }

    element->yFixed8 += Ending_Credits_scrollSpeedFixed8;

    if (element->yFixed8 < -(12 << 8)) {
        element->phase = -1;
        return;
    }

    y = element->yFixed8 >> 8;
    str = element->textBytes;
    len = *str++;
    x = element->xFixed8 >> 8;

    for (i = 0; i < len; ++i, ++str) {
        c = *str;
        if (c < 32u) {
            switch (c) {
            case 1:
                charTableOffset = 0;
                break;
            case 2:
                charTableOffset = 112;
                break;
            case 3:
                if (charTableOffset != 0) {
                    underline = 18;
                } else {
                    underline = 17;
                }
                break;
            case 31:
                --x;
                break;
            }
        } else {
            glyph = charTableOffset + c - 32;
            setSprt(sprt);
            setXY0(sprt, x, y);
            setUV0(sprt, (glyph & 0xF) * 16, (glyph >> 4) * 16);
            glyphWidth = Ending_Credits_glyphWidths[glyph];
            setWH(sprt, glyphWidth, 16);
            setClut(sprt, 960, 480);
            x += glyphWidth - 1;
            sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;
            addPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 3, sprt);
            ++sprt;
        }
    }

    if (underline != 0) {
        p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        x = 160 - ((element->width + 32) >> 1);

        SetSprt(sprt);
        y += underline;
        setXY0(sprt, x, y);
        setUV0(sprt, 0, 208);
        setWH(sprt, 16, 3);
        setClut(sprt, 960, 480);
        sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;
        AddPrim(p[1] + 12, sprt++);

        poly = (POLY_FT4*)sprt;
        SetPolyFT4(poly);
        x += 16;
        poly->x0 = poly->x2 = x;
        x += element->width;
        poly->y0 = poly->y1 = y;
        poly->y2 = poly->y3 = y + 3;
        poly->u0 = poly->u2 = 8;
        poly->v2 = poly->v3 = 211;
        poly->u1 = poly->u3 = 16;
        poly->v0 = poly->v1 = 208;
        poly->tpage = 31;
        poly->clut = getClut(960, 480);
        poly->x1 = poly->x3 = x;
        poly->r0 = poly->g0 = poly->b0 = Ending_Credits_backgroundBrightness;
        AddPrim(p[1] + 12, poly++);

        sprt = (SPRT*)poly;
        SetSprt(sprt);
        setUV0(sprt, 168, 208);
        setXY0(sprt, x, y);
        setWH(sprt, 16, 3);
        setClut(sprt, 960, 480);
        sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;
        AddPrim(p[1] + 12, sprt++);
    }

    *(SPRT**)((uintptr_t)Core_ScratchBase() + 0x000) = sprt;
    *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = Render_AppendTexturePage(31, 3);
}

// decomp: ENDING.PRG _renderCopyright (src/ENDING/ENDING.PRG/D4.c:831)
void Ending_Credits_DrawScrollingCopyright(Ending_Credits_Element* arg0)
{
    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    SPRT* sprt = p[0];
    int new_var;

    if (arg0->phase == 1) {
        arg0->xFixed8 = 100 << 8;
        arg0->yFixed8 = 224 << 8;
        arg0->phase = 2;
    }

    arg0->yFixed8 += Ending_Credits_scrollSpeedFixed8;

    if (arg0->yFixed8 < -(12 << 8)) {
        arg0->phase = -1;
        return;
    }

    SetSprt(sprt);
    setXY0(sprt, arg0->xFixed8 >> 8, arg0->yFixed8 >> 8);
    setUV0(sprt, 0, 0x30);
    setWH(sprt, 0x78, 0x10);
    setClut(sprt, 960, 64);
    sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;

    setaddr(sprt, ((u_long*)p[1])[3]);
    setaddr(p[1] + 12, sprt++);

    new_var = 0;
    p[new_var] = sprt;
    p[new_var] = Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 960, 0), 3);
}

// decomp: ENDING.PRG _renderBackground (src/ENDING/ENDING.PRG/D4.c:865)
void Ending_Credits_DrawBackground(Ending_Credits_Element* arg0 __attribute__((unused)))
{
    TILE* tile;
    POLY_G4* poly;
    SPRT* sprt;
    void** data;

    data = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    sprt = data[0];
    SetSprt(sprt);
    setXY0(sprt, 0, 10);
    setUV0(sprt, 0, 10);
    setWH(sprt, 256, 204);
    setClut(sprt, 780, 255);
    sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;
    AddPrim(data[1] + 0x1FFC, sprt++);
    data[0] = sprt;

    sprt = (SPRT*)Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 768, 0), 0x7FF);
    SetSprt(sprt);
    setUV0(sprt, 0, 10);
    setXY0(sprt, 256, 10);
    setWH(sprt, 64, 204);
    setClut(sprt, 780, 255);
    sprt->r0 = sprt->g0 = sprt->b0 = Ending_Credits_backgroundBrightness;
    AddPrim(data[1] + 0x1FFC, sprt++);
    data[0] = sprt;

    poly =
        (POLY_G4*)Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 832, 0), 0x7FF);
    SetPolyG4(poly);
    poly->x0 = poly->x2 = 0;
    poly->x1 = poly->x3 = 320;
    poly->y0 = poly->y1 = 10;
    poly->y2 = poly->y3 = 30;
    poly->r0 = poly->r1 = poly->g0 = poly->g1 = poly->b0 = poly->b1 = 255;
    poly->r2 = poly->r3 = poly->g2 = poly->g3 = poly->b2 = poly->b3 = 0;
    setSemiTrans(poly, 1);
    AddPrim(data[1] + 8, poly++);

    SetPolyG4(poly);
    poly->x0 = poly->x2 = 0;
    poly->x1 = poly->x3 = 320;
    poly->y0 = poly->y1 = 194;
    poly->y2 = poly->y3 = 214;
    poly->r0 = poly->r1 = poly->g0 = poly->g1 = poly->b0 = poly->b1 = 0;
    poly->r2 = poly->r3 = poly->g2 = poly->g3 = poly->b2 = poly->b3 = 255;
    setSemiTrans(poly, 1);
    AddPrim(data[1] + 8, poly++);

    tile = (TILE*)poly;
    SetTile(tile);
    tile->r0 = tile->g0 = tile->b0 = 0;
    setWH(tile, 320, 10);
    setXY0(tile, 0, 0);
    AddPrim(data[1] + 8, tile++);

    SetTile(tile);
    tile->r0 = tile->g0 = tile->b0 = 0;
    setWH(tile, 320, 10);
    setXY0(tile, 0, 0);
    AddPrim(data[1] + 8, tile++);

    SetTile(tile);
    tile->r0 = tile->g0 = tile->b0 = 0;
    setWH(tile, 320, 10);
    setXY0(tile, 0, 214);
    AddPrim(data[1] + 8, tile++);
    data[0] = tile;

    data[0] = Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Subtract, 0, 0), 2);
}

// decomp: ENDING.PRG _backgroundFadeIn (src/ENDING/ENDING.PRG/D4.c:939)
void Ending_Credits_FadeInBackground(Ending_Credits_Element* element)
{
    if (element->phase == 1) {
        element->animationFrame = 0;
        element->phase = 2;
    }

    if (++element->animationFrame == 128) {
        Ending_Credits_isBackgroundFading = 0;
        element->phase = -1;
    }

    Ending_Credits_backgroundBrightness = (u_int)Core_Math_EvaluateEase(Ending_Credits_EasingMode_Linear, element->animationFrame, 128) / 32;
}

// decomp: ENDING.PRG _backgroundFadeOut (src/ENDING/ENDING.PRG/D4.c:954)
void Ending_Credits_FadeOutBackground(Ending_Credits_Element* arg0)
{
    if (arg0->phase == 1) {
        arg0->phase = 2;
        arg0->animationFrame = 0;
        Ending_Credits_isBackgroundFading = 1;
    }

    if (++arg0->animationFrame == 128) {
        if (arg0->width != 0) {
            Ending_Credits_hasFinished = 1;
        }
        arg0->phase = -1;
    }

    Ending_Credits_backgroundBrightness = 128 - ((Core_Math_EvaluateEase(Ending_Credits_EasingMode_Linear, arg0->animationFrame, 128) << 7) >> 0xC);
}

// decomp: ENDING.PRG func_8006A4D8 (src/ENDING/ENDING.PRG/D4.c:972)
void Ending_Credits_FadeOutForSkip(Ending_Credits_Element* element)
{
    void** s2 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    TILE* temp_s0 = s2[0];

    if (element->phase == 1) {
        element->animationFrame = 0;
        element->phase = 2;
    }

    if (++element->animationFrame == 128) {
        Ending_Credits_hasFinished = 1;
    }

    SetTile(temp_s0);
    setSemiTrans(temp_s0, 1);
    temp_s0->r0 = temp_s0->g0 = temp_s0->b0 =
        (Core_Math_EvaluateEase(Ending_Credits_EasingMode_EaseOut, element->animationFrame, 128) * 255) >> 0xC;
    setWH(temp_s0, 320, 240);
    setXY0(temp_s0, 0, 0);

    AddPrim(s2[1], temp_s0++);

    s2[0] = temp_s0;
    s2[0] = Render_AppendTexturePage(getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Subtract, 0, 0), 0);
}

// decomp: ENDING.PRG vs_ending_exec (src/ENDING/ENDING.PRG/D4.c:999)
void Ending_Credits_RunStaffRoll(void)
{
    Content_DiscFile file;
    short i;
    Content_DiscQueueSlot* slot;

    Ending_Credits_AllocateRenderBuffers();
    Core_Gte_SetGeomOffset(0xA0, 0x70);
    Core_Gte_SetGeomScreen(0x200);
    Ending_Credits_ConfigureDrawEnvironment(Render_legacyDrawEnvironments);
    Ending_Credits_ConfigureDrawEnvironment(&Render_legacyDrawEnvironments[1]);

    Render_legacyDisplayEnvironments[1].isinter = 0;
    Render_legacyDisplayEnvironments[0].isinter = 0;

    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureOne);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureTwo);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureThree);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureFour);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureFive);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureSix);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureSeven);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureEight);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureNine);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureTen);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureEleven);
    Ending_Credits_UploadTexture(&Ending_Credits_embeddedTextureTwelve);

    for (i = 5; i < 16; ++i) {
        file.lba = Ending_Credits_illustrationDiscLocations[i - 5];
        file.size = Ending_Credits_illustrationDiscSizes[i - 5];
        Ending_Credits_illustrationResources[i] = Core_Memory_AllocateFromHighEnd(file.size);
        slot = Content_AllocateDiscRequest(&file);
        Content_EnqueuePriorityDiscRequest(slot, Ending_Credits_illustrationResources[i]);

        do {
            State_UpdatePlayTimeAndLoading(0);
        } while (slot->state != 4);

        Content_ReleaseDiscRequest(slot);
    }

    Ending_Credits_backgroundBrightness = 0;
    Ending_Credits_wasSkipRequested = 0;

    Ending_Credits_ResetElements();
    Ending_Credits_AddElement(Ending_Credits_DrawBackground);
    SetDispMask(1);

    Ending_Credits_scriptCursor = Ending_Credits_scriptBytes;
    Ending_Credits_scrollSpeedFixed8 = -128;
    Ending_Credits_hasFinished = 0;
    Ending_Credits_scriptDelayFrames = 0;
    Ending_Credits_elapsedFrames = 0;
    Ending_Credits_isBackgroundFading = 1;

    Ending_Credits_StartMusic();
    Ending_Credits_RunFrames();
    Audio_RequestXaStreamStop();
    SetDispMask(0);

    for (i = 5; i < 16; ++i) {
        Core_Memory_FreeFromHighEnd(Ending_Credits_illustrationResources[i]);
    }

    Ending_Credits_WaitForMusicEnd();
    Ending_UpdateClearProgress();
    Ending_ShowClearResults();
}

// decomp: ENDING.PRG _setDrawEnv (src/ENDING/ENDING.PRG/D4.c:1070)
void Ending_Credits_ConfigureDrawEnvironment(DRAWENV* drawenv)
{
    drawenv->isbg = 1;
    drawenv->dtd = 0;
    drawenv->dfe = 0;
    drawenv->b0 = 0;
    drawenv->g0 = 0;
    drawenv->r0 = 0;
    drawenv->tpage = 0x20;
}

// decomp: ENDING.PRG func_8006A888 (src/ENDING/ENDING.PRG/D4.c:1081)
void Ending_Credits_RunFrames(void)
{
    while (Ending_Credits_hasFinished == 0) {

        Ending_Credits_decodeBudgetStartTick = VSync(1);

        Ending_Credits_BeginFrame();
        Ending_Credits_ExecuteScript();
        Ending_Credits_DrawElements();
        Ending_Credits_DecodeIllustrationStep();

        if ((Input_buttonsPressed.allButtons & PADstart) && (Ending_Credits_wasSkipRequested == 0)) {
            Ending_Credits_AddElement(Ending_Credits_FadeOutForSkip);
            Ending_Credits_wasSkipRequested = 1;
        }

        Ending_Credits_PresentFrame(*(u_long**)((uintptr_t)Core_ScratchBase() + 0x004) + 0x7FF);

        ++Ending_Credits_elapsedFrames;
    }
}

// decomp: ENDING.PRG func_8006A9C0 (src/ENDING/ENDING.PRG/D4.c:1115)
void Ending_Credits_UploadTexture(Ending_Credits_TimHeader* arg0)
{
    RECT rect;
    Ending_Credits_TimImageBlockHeader* p = &arg0->firstBlock;

    if (arg0->mode & 8) {
        rect.x = p->uploadRect.x;
        rect.y = p->uploadRect.y;
        rect.w = p->uploadRect.w;
        rect.h = p->uploadRect.h;
        LoadImage(&rect, (void*)p->pixelBytes);
        p = (Ending_Credits_TimImageBlockHeader*)((void*)arg0 + (p->blockSizeBytes + 8));
    }

    rect.x = p->uploadRect.x;
    rect.y = p->uploadRect.y;
    rect.w = p->uploadRect.w;
    rect.h = p->uploadRect.h;

    LoadImage(&rect, (void*)p->pixelBytes);
}

// decomp: ENDING.PRG _resetElements (src/ENDING/ENDING.PRG/D4.c:1137)
void Ending_Credits_ResetElements(void)
{
    short i;
    Ending_Credits_Element* element = Ending_Credits_elements;
    for (i = 0; i < 32; ++i, ++element) {
        element->phase = 0;
    }
}

// decomp: ENDING.PRG _renderElements (src/ENDING/ENDING.PRG/D4.c:1146)
int Ending_Credits_DrawElements(void)
{
    short i;
    short var_s3 = 0;
    Ending_Credits_Element* element = Ending_Credits_elements;

    for (i = 0; i < 32; ++i, ++element) {
        if (element->phase < 0) {
            element->phase = 0;
        } else if (element->phase > 0) {
            ++var_s3;
            element->render(element);
        }
    }

    return var_s3;
}

// decomp: ENDING.PRG _addRenderer (src/ENDING/ENDING.PRG/D4.c:1164)
Ending_Credits_Element* Ending_Credits_AddElement(void (*renderer)(Ending_Credits_Element*))
{
    short i;
    Ending_Credits_Element* element = &Ending_Credits_elements[1];

    for (i = 1; i < 32; ++i, ++element) {
        if (element->phase == 0) {
            element->render = renderer;
            element->phase = 1;
            element->animationFrame = 0;
            return element;
        }
    }

}

// decomp: ENDING.PRG _replaceMainRenderer (src/ENDING/ENDING.PRG/D4.c:1181)
Ending_Credits_Element* Ending_Credits_ReserveMainElement(void (*renderer)(Ending_Credits_Element*))
{
    if (Ending_Credits_elements[0].phase == 0) {
        Ending_Credits_elements[0].render = renderer;
        Ending_Credits_elements[0].phase = 1;
        Ending_Credits_elements[0].animationFrame = 0;
        return &Ending_Credits_elements[0];
    }

    return NULL;
}

// decomp: ENDING.PRG _parseCreditsScript (src/ENDING/ENDING.PRG/D4.c:1193)
void Ending_Credits_ExecuteScript(void)
{
    enum Ending_Credits_ScriptOpcode { Ending_Credits_ScriptOpcode_Wait = 1 };

    u_char* bytestream;
    int op;
    int delay;

    if (Ending_Credits_scriptDelayFrames != 0) {
        --Ending_Credits_scriptDelayFrames;
        return;
    }

    while (1) {
        bytestream = Ending_Credits_scriptCursor;

        switch (bytestream[0]) {

        // 0x1 Wait: u8 duration
        case Ending_Credits_ScriptOpcode_Wait:
            Ending_Credits_scriptCursor = bytestream + 1;
            delay = bytestream[1] * 2;
            Ending_Credits_scriptCursor = bytestream + 2;
            Ending_Credits_scriptDelayFrames = delay;
            return;

        // 0x2 Render text: u8 len
        case 2:
            Ending_Credits_scriptCursor = bytestream + 1;
            Ending_Credits_AddElement(Ending_Credits_DrawScrollingText)->textBytes = Ending_Credits_scriptCursor;
            Ending_Credits_scriptCursor += Ending_Credits_scriptCursor[0] + 1;
            break;

        // 0x3 Enable background illustrations
        case 3:
            Ending_Credits_scriptCursor = bytestream + 1;
            Ending_Credits_AddElement(Ending_Credits_DrawIllustration)->illustrationIndex = 0;
            break;

        // 0x4 Render special: u8 secondary op
        case 4:
            Ending_Credits_scriptCursor = bytestream + 1;
            op = bytestream[1];
            Ending_Credits_scriptCursor = bytestream + 2;

            switch (op) {
            case 0:
                Ending_Credits_AddElement(Ending_Credits_DrawScrollingCopyright);
                break;

            case 1:
                Ending_Credits_AddElement(Ending_Credits_DrawScrollingTitleLogo);
                break;

            case 2:
                Ending_Credits_AddElement(Ending_Credits_DrawFinCaption);
                break;

            case 3:
                Ending_Credits_AddElement(Ending_Credits_DrawPhantomPainRipple);
                break;
            }
            break;

        // 0x5 Fade in
        case 5:
            Ending_Credits_scriptCursor = bytestream + 1;
            Ending_Credits_ReserveMainElement(Ending_Credits_FadeInBackground);
            break;

        // 0x6 Fade out
        case 6:
            Ending_Credits_scriptCursor = bytestream + 1;
            Ending_Credits_ReserveMainElement(Ending_Credits_FadeOutBackground)->width = 0;
            break;

        // 0x7 Freeze scrolling
        case 7:
            Ending_Credits_scriptCursor = bytestream + 1;
            Ending_Credits_scrollSpeedFixed8 = 0;
            break;

        // 0x8 Set illustration timer: u8 illustration id, u16 duration
        case 8:
            *(Ending_Credits_illustrationHoldFrames + bytestream[1]) =
                (bytestream[2] + (bytestream[3] << 8)) - 160;
            Ending_Credits_scriptCursor = bytestream + 4;
            break;

        // 0xFF End of stream
        case 255:
            Ending_Credits_hasFinished = 1;
            return;

        default:
            return;
        }
    }
}

// decomp: ENDING.PRG func_8006AF44 (src/ENDING/ENDING.PRG/D4.c:1307)
void Ending_Credits_QueueIllustrationDecode(Ending_Credits_TimHeader* arg0, void* arg1)
{
    Ending_Credits_illustrationDecodePhase = 1;
    Ending_Credits_queuedCompressedImage = arg1;
    Ending_Credits_queuedTimDestination = arg0;
}

// decomp: ENDING.PRG func_8006AF64 (src/ENDING/ENDING.PRG/D4.c:1317)
int Ending_Credits_DecodeIllustrationStep(void)
{
    short i;
    u_char op;

    if (Ending_Credits_illustrationDecodePhase == 0) {
        return 0;
    }

    if (Ending_Credits_illustrationDecodePhase == 1) {
        Ending_Credits_illustrationDecodePhase = 2;
        Ending_Credits_decodeInputCursor = Ending_Credits_queuedCompressedImage;
        Ending_Credits_decodeOutputCursor = (u_char*)Ending_Credits_queuedTimDestination;
    }

    while (1) {
        if ((VSync(1) - Ending_Credits_decodeBudgetStartTick) > 200) {
            return 0;
        }

        op = *Ending_Credits_decodeInputCursor;

        if (!(op & 0x80)) {
            if (op == 0x7F) {
                Ending_Credits_illustrationDecodePhase = 0;
                return 1;
            }
            if (op == 0x7D) {
                // 0x7D: u8 count, u8 value: fill count + 1 bytes
                ++Ending_Credits_decodeInputCursor;
                Ending_Credits_decodeRunLength = *Ending_Credits_decodeInputCursor++;
                Ending_Credits_decodeFillByteZero = *Ending_Credits_decodeInputCursor++;
                for (i = 0; i <= Ending_Credits_decodeRunLength; ++i) {
                    *Ending_Credits_decodeOutputCursor++ = Ending_Credits_decodeFillByteZero;
                }
            } else if (op == 0x7E) {
                // 0x7E: u8 count, u8 value0, u8 value1: fill count + 1 byte pairs
                ++Ending_Credits_decodeInputCursor;
                Ending_Credits_decodeRunLength = *Ending_Credits_decodeInputCursor++;
                Ending_Credits_decodeFillByteZero = *Ending_Credits_decodeInputCursor++;
                Ending_Credits_decodeFillByteOne = *Ending_Credits_decodeInputCursor++;
                for (i = 0; i <= Ending_Credits_decodeRunLength; ++i) {
                    *Ending_Credits_decodeOutputCursor++ = Ending_Credits_decodeFillByteZero;
                    *Ending_Credits_decodeOutputCursor++ = Ending_Credits_decodeFillByteOne;
                }
            } else {
                // 0x00-0x7C: copy op + 1 literal bytes
                Ending_Credits_decodeRunLength = *Ending_Credits_decodeInputCursor++;
                for (i = 0; i <= Ending_Credits_decodeRunLength; ++i) {
                    *Ending_Credits_decodeOutputCursor++ = *Ending_Credits_decodeInputCursor++;
                }
            }
        } else {
            // 1LLLOOOO OOOOOOOO: copy L + 3 bytes from offset O + 1 back,
            // repeated while followed by 0x7C
            Ending_Credits_decodeRunLength = ((op >> 4) & 7) + 3;
            Ending_Credits_decodeBackreferenceOffset = ((Ending_Credits_decodeInputCursor[0] & 0xF) << 8) + Ending_Credits_decodeInputCursor[1];
            Ending_Credits_decodeInputCursor += 2;
            Ending_Credits_decodeBackreferenceCursor = Ending_Credits_decodeOutputCursor - Ending_Credits_decodeBackreferenceOffset - 1;
            for (i = 0; i < Ending_Credits_decodeRunLength; ++i) {
                *Ending_Credits_decodeOutputCursor++ = *Ending_Credits_decodeBackreferenceCursor++;
            }
            while (*Ending_Credits_decodeInputCursor == 0x7C) {
                Ending_Credits_decodeBackreferenceCursor = Ending_Credits_decodeOutputCursor - Ending_Credits_decodeBackreferenceOffset - 1;
                ++Ending_Credits_decodeInputCursor;
                for (i = 0; i < Ending_Credits_decodeRunLength; ++i) {
                    *Ending_Credits_decodeOutputCursor++ = *Ending_Credits_decodeBackreferenceCursor++;
                }
            }
        }
    }
}

// decomp: ENDING.PRG func_8006B324 (src/ENDING/ENDING.PRG/D4.c:1390)
void Ending_Credits_DrawSmallText(short arg0, short arg1, int arg2, u_char* arg3)
{
    int temp_v0;
    u_char var_v1_2;
    int var_v1;
    SPRT_8* var_a2;
    u_long** t4;

    var_v1 = *arg3;
    var_a2 = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

    while (var_v1 != 0) {

        temp_v0 = var_v1 - 32;
        var_v1_2 = temp_v0;

        if (((temp_v0 & 0xFF) < 96u) && (var_v1_2 != 0)) {

            if (var_v1_2 == 60) {
                var_v1_2 = 15;
            }

            if (var_v1_2 > 64) {
                var_v1_2 -= 32;
            }

            setSprt8(var_a2);
            setShadeTex(var_a2, 1);
            setXY0(var_a2, arg0, arg1);
            setUV0(var_a2, (var_v1_2 & 0xF) * 8, (var_v1_2 & 0xF0) / 2);
            setClut(var_a2, 960, arg2 + 32);

            t4 = (u_long**)((uintptr_t)Core_ScratchBase() + 0x000);
            setaddr(var_a2, *t4[1] & 0xFFFFFF);
            setaddr(t4[1], var_a2);

            ++var_a2;
        }

        ++arg3;
        var_v1 = *arg3;
        arg0 += 7;
    }

    *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = var_a2;
    *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = Render_AppendTexturePage(0xF, 0);
}

// decomp: ENDING.PRG func_8006B450 (src/ENDING/ENDING.PRG/D4.c:1438)
void Ending_Credits_InitializeDisplay(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    RECT rect;
    int temp_s1;

    rect = (RECT) { 0, 0, 1024, 512 };

    SetDispMask(0);
    Core_Gte_SetGeomOffset(arg0 / 2, (arg1 + (int)((u_int)arg1 >> 0x1F)) >> 1);
    Core_Gte_SetGeomScreen(arg2);

    temp_s1 = arg1 - 0x10;

    SetDefDrawEnv(Render_legacyDrawEnvironments, 0, 0, arg0, temp_s1);
    SetDefDispEnv(Render_legacyDisplayEnvironments, arg0, 0, arg0, temp_s1);
    SetDefDrawEnv(&Render_legacyDrawEnvironments[1], arg0, 0, arg0, temp_s1);
    SetDefDispEnv(&Render_legacyDisplayEnvironments[1], 0, 0, arg0, temp_s1);

    setRECT(&Render_legacyDisplayEnvironments[0].screen, 0, 8, 256, 224);
    setRECT(&Render_legacyDisplayEnvironments[1].screen, 0, 8, 256, 224);

    Render_legacyViewportLeft = 0;
    Render_legacyViewportTop = 0;
    Render_viewportWidth = arg0;
    Render_viewportHeight = temp_s1;
    Render_legacyDrawEnvironments[1].dtd = 0;
    Render_legacyDrawEnvironments[0].dtd = 0;
    Render_legacyDrawEnvironments[1].isbg = 1;
    Render_legacyDrawEnvironments[0].isbg = 1;
    setRGB0(&Render_legacyDrawEnvironments[0], arg3, arg4, arg5);
    setRGB0(&Render_legacyDrawEnvironments[1], arg3, arg4, arg5);

    ClearImage(&rect, 0, 0, 0);
    PutDispEnv(&Render_legacyDisplayEnvironments[Render_frameBuf]);
    PutDrawEnv(&Render_legacyDrawEnvironments[Render_frameBuf]);

    Ending_Credits_xaResumeFrameCount = 0;
}

// decomp: ENDING.PRG _drawScreen (src/ENDING/ENDING.PRG/D4.c:1480)
int Ending_Credits_PresentFrame(u_long* arg0)
{
    int _[2];
    int temp_s1;

    Render_frameBuf = Render_frameBuf == 0;


    temp_s1 = State_UpdatePlayTimeAndLoading(0);

    PutDispEnv(&Render_legacyDisplayEnvironments[Render_frameBuf]);
    PutDrawEnv(&Render_legacyDrawEnvironments[Render_frameBuf]);
    DrawOTag(arg0);

    ++Ending_Credits_xaResumeFrameCount;

    return temp_s1;
}

// decomp: ENDING.PRG func_8006B884 (src/ENDING/ENDING.PRG/D4.c:1539)
void Ending_Credits_AllocateRenderBuffers(void)
{
    Ending_Credits_InitializeDisplay(0x140, 0xF0, 0x200, 0, 0, 0);
    Core_Memory_InitializeHeap((Core_HeapBlockHeader*)Core_GameHeapArena(), CORE_GAME_HEAP_SIZE);
    Render_legacyPrimitiveBuffers[0] = Core_Memory_AllocateFromLowEnd(0x20000);
    Render_legacyPrimitiveBuffers[1] = Core_Memory_AllocateFromLowEnd(0x20000);
    Render_legacyOrderingTables[0] = Core_Memory_AllocateFromLowEnd(0x2088);
    Render_legacyOrderingTables[1] = Core_Memory_AllocateFromLowEnd(0x2088);
}

// decomp: ENDING.PRG func_8006B930 (src/ENDING/ENDING.PRG/D4.c:1551)
void Ending_Credits_BeginFrame(void)
{
    void* temp_a0;

    Input_UpdateControllers();
    temp_a0 = Render_legacyOrderingTables[Render_frameBuf];
    Render_frameScratch.overlayOrderingTable = temp_a0 + 8;
    Render_frameScratch.orderingTable = temp_a0 + 0x48;
    ClearOTagR((u_long*)temp_a0, 0x822);
    Render_frameScratch.primitiveCursor = Render_legacyPrimitiveBuffers[Render_frameBuf];
}

// decomp: ENDING.PRG func_8006B9B4 (src/ENDING/ENDING.PRG/D4.c:1563)
void Ending_Credits_StartMusic(void)
{
    Audio_BeginXaStream(VS_ENDING_XA_LBA, 0x7FFF); // Partial playback?
}

// decomp: ENDING.PRG _checkStreamXaEnd (src/ENDING/ENDING.PRG/D4.c:1568)
void Ending_Credits_WaitForMusicEnd(void) { Audio_RequestXaStreamStop(); }

