// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/Feedback.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "DiscQueue.h"
#include "Event/Splash.h"
#include "Menu/Framework/Framework.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"

// decomp: D_800EB9BC (D_800EB9BC@BATTLE/BATTLE.PRG)
Event_SplashLayerView* Render_gimLayers; /* data: contents loaded in Step 5 */

// decomp: D_800EC25C (D_800EC25C@BATTLE/BATTLE.PRG)
void (*Render_kGimLayerDrawRoutines[4])(Render_GimLayer*, int, u_long*); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800F51C8 (D_800F51C8@BATTLE/BATTLE.PRG)
char Render_gimBlendSelect;

// decomp: _gimLbas (_gimLbas@BATTLE/BATTLE.PRG)
int Render_legacyGimDiscFiles[] = { squashLba(VS_STAFF011_GIM), squashLba(VS_STAFF001_GIM),
    squashLba(VS_STAFF002_GIM), squashLba(VS_STAFF003_GIM), squashLba(VS_STAFF004_GIM),
    squashLba(VS_STAFF005_GIM), squashLba(VS_STAFF006_GIM), squashLba(VS_STAFF007_GIM),
    squashLba(VS_STAFF008_GIM), squashLba(VS_GAMEOVER_GIM), squashLba(VS_STAFF009_GIM),
    squashLba(VS_STAFF010_GIM), squashLba(VS_STAFF012_GIM), squashLba(VS_ENKEI_GIM),
    squashLba(VS_LOGO_G22_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_1142_GIM),
    squashLba(VS_N_LEA_GIM), squashLba(VS_LOGO_28_GIM), squashLba(VS_LOGO_29_GIM),
    squashLba(VS_LOGO_30_GIM), squashLba(VS_LOGO_31_GIM), squashLba(VS_VKP_2_GIM),
    squashLba(VS_VKP_1_GIM), squashLba(VS_VKP_0_GIM), squashLba(VS_JYO_GIM),
    squashLba(VS_HEADQUAT_GIM), squashLba(VS_DEMOENK_GIM), squashLba(VS_DADDY_GIM),
    squashLba(VS_BLACK_GIM), squashLba(VS_ENKEI2_GIM), squashLba(VS_JO_GIM),
    squashLba(VS_SUNRISE_GIM), squashLba(VS_MAIN03_GIM), squashLba(VS_EPI_INF_GIM),
    squashLba(VS_EPILOGUE_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM),
    squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM),
    squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM),
    squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM),
    squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM), squashLba(VS_STAFF013_GIM),
    squashLba(VS_STAFF013_GIM), squashLba(VS_EPI_E01B_GIM), squashLba(VS_EV_MASK_GIM),
    squashLba(VS_019XXX01_GIM), squashLba(VS_DEMO_001_GIM), squashLba(VS_DEMO_002_GIM),
    squashLba(VS_DEMO_003_GIM), squashLba(VS_DEMO_004_GIM), squashLba(VS_DEMO_005_GIM),
    squashLba(VS_DEMO_006_GIM), squashLba(VS_DEMO_007_GIM), squashLba(VS_DEMO_008_GIM),
    squashLba(VS_SQUARE_GIM), squashLba(VS_DEMOFIN_GIM) };

// decomp: BATTLE.PRG vs_battle_loadGim (src/BATTLE/BATTLE.PRG/5BF94.c:2347)
void Render_QueueGimImage(int id, int arg1)
{
    Content_DiscFile file;
    Render_GimLayer* var_s0;
    int i;

    if ((*(Render_GimLayer* (*))&Render_gimLayers) == NULL) {
        var_s0 = Core_Memory_AllocateFromHighEnd(sizeof *var_s0 * 3);
        (*(Render_GimLayer* (*))&Render_gimLayers) = var_s0;
        var_s0->fileData = Core_Memory_AllocateFromHighEnd((char)(*(u_int (*)[])&Render_legacyGimDiscFiles)[id] << 0xB);

        for (i = 0; i < 3; ++i) {
            *(u_short*)var_s0 = 0;
            var_s0->loading = 1;
            var_s0->loadPhase = 1;
            var_s0->vramPage = arg1;
            var_s0->brightness = 0x80;
            var_s0->imageId = id;
            var_s0->offsetX = 0;
            var_s0->offsetY = 0;
            var_s0->scaleAndOrder = 0x1000;
            ++var_s0;
        }
        file.lba = (*(u_int (*)[])&Render_legacyGimDiscFiles)[id] >> 8;
        file.size = (u_char)(*(u_int (*)[])&Render_legacyGimDiscFiles)[id] << 0xB;
        (*(Render_GimLayer* (*))&Render_gimLayers)->discRequest = Content_AllocateDiscRequest(&file);
        Content_EnqueueDiscRequest((*(Render_GimLayer* (*))&Render_gimLayers)->discRequest, (*(Render_GimLayer* (*))&Render_gimLayers)->fileData);
    }
}

// decomp: BATTLE.PRG func_800C8550 (src/BATTLE/BATTLE.PRG/5BF94.c:2377)
void Render_ActivateGimLayer(u_int arg0, void* arg1, u_char* arg2)
{
    Render_GimLayer* gim = &(*(Render_GimLayer* (*))&Render_gimLayers)[arg0];
    int step;
    int next;
    int i;

    gim->drawEnabled = 1;

    if (gim->drawRoutine == 2) {
        u_short* p = gim->scanlineWipeWidths;
        int angle = Core_Random_NextInterfaceRandomBits(9);
        int target = Core_Random_NextInterfaceRandomBits(10);

        for (i = 0; i < 240; ++i) {
            p[i] = (Core_Gte_Sin(angle) >> 3) + 320;
            step = Core_Random_NextInterfaceRandomBits(8) - 64;
            next = Core_Random_NextInterfaceRandomBits(10);

            if (target < angle) {
                angle -= step;

                if (target >= angle) {
                    target = next;
                }
            } else {
                angle += step;

                if (angle >= target) {
                    target = next;
                }
            }

            if (angle < 0) {
                angle = 128;
                target = Core_Random_NextInterfaceRandomBits(10);
            }
        }
    }

    if (gim->drawRoutine == 3) {
        Render_ReserveTexturePageRange((*(Render_GimLayer* (*))&Render_gimLayers)->vramPage + (*(Render_GimLayer* (*))&Render_gimLayers)->texturePageCount, 1, 6);
    }
}

// decomp: BATTLE.PRG func_800C86AC (src/BATTLE/BATTLE.PRG/5BF94.c:2422)
void Render_ReleaseGimImages(void)
{
    Render_GimLayer* var_s0 = (*(Render_GimLayer* (*))&Render_gimLayers);
    int i;

    if ((var_s0 != NULL) && (var_s0->loadPhase == 3)) {
        Render_RetireTexturePageRange(var_s0->vramPage, var_s0->texturePageCount, 6);
        for (i = 0; i < 3; ++i) {
            if (var_s0->drawRoutine == 3) {
                Render_RetireTexturePageRange((*(Render_GimLayer* (*))&Render_gimLayers)->vramPage + (*(Render_GimLayer* (*))&Render_gimLayers)->texturePageCount, 1, 6);
            }
            ++var_s0;
        }
        Core_Memory_FreeFromHighEnd((*(Render_GimLayer* (*))&Render_gimLayers));
        (*(Render_GimLayer* (*))&Render_gimLayers) = NULL;
    }
}

// decomp: BATTLE.PRG func_800C8778 (src/BATTLE/BATTLE.PRG/5BF94.c:2440)
void Render_UpdateGimImages(void)
{
    Render_GimLayer* gim = (*(Render_GimLayer* (*))&Render_gimLayers);
    Render_GimLayer* second = gim + 1;
    Render_GimLayer* third = gim + 2;
    int i;
    int j;
    int count;
    int width;
    int height;
    int _[2] __attribute__((unused));

    if (gim == NULL) {
        return;
    }

    switch (gim->loadPhase) {
    case 0:
        break;
    case 1:
        if (gim->discRequest->state != 4) {
            return;
        }

        Content_ReleaseDiscRequest(gim->discRequest);

        count = gim->fileData[0];
        width = count & 0xFF;
        j = count >> 8;
        count = width * j + 4;
        gim->columns = width;
        gim->rows = j;

        for (i = 0; i < count; ++i) {
            gim->tileTable[i] = gim->fileData[i];
        }

        if (gim->fileData[3] != 0) {
            int tiles;

            width = gim->fileData[count];
            tiles = width >> 8;
            second->columns = width;
            second->rows = tiles;
            width = (width & 0xFF) * tiles + 4;

            for (i = 0; i < width; ++i) {
                second->tileTable[i] = gim->fileData[i + count];
            }

            count += width;

            if (gim->fileData[3] == 2) {
                width = gim->fileData[count];
                tiles = width >> 8;
                third->columns = width;
                third->rows = tiles;
                width = (width & 0xFF) * tiles + 4;

                for (i = 0; i < width; ++i) {
                    third->tileTable[i] = gim->fileData[i + count];
                }

                count += width;
            }
        }

        count = (count + 1) & 0xFFFE;
        width = (gim->vramPage - 24) * 64;
        Render_UploadImageRectangle((width + 512) | 0x1FF0000, gim->fileData + count, 0x10040);

        for (j = 0; j < 3; ++j) {
            Render_GimLayer* current = &gim[j];

            switch (current->tileTable[2]) {
            case 0:
                current->textureMode = 0;
                current->backdropColor = gim->fileData[count + j * 16];
                break;
            case 1:
                current->textureMode = 1;
                current->backdropColor = gim->fileData[count + 64];
                break;
            case 2:
                current->textureMode = 1;
                current->backdropColor = gim->fileData[count + 320];
                current->ownsPalette = 1;
                Render_UploadImageRectangle(
                    (width + 512) | 0x1FF0000, gim->fileData + (count + 320), 0x10100);
                break;
            }
        }

        Render_LoadClutColors(gim->fileData + (count + 64), 3, 0, 256);

        i = gim->fileData[1];
        height = i >> 8;
        i &= 0xFF;
        j = (i + 16) / 17;
        gim->texturePageCount = j;
        second->texturePageCount = j;
        third->texturePageCount = j;
        height *= 16;
        count += height;
        Render_ReserveTexturePageRange(gim->vramPage, gim->texturePageCount, 6);

        while (i > 0) {
            Render_UploadImageRectangle((width + 512) | 0x1000000, gim->fileData + count,
                i >= 17 ? 0xFF0040 : ((i * 15) << 16) | 64);
            i -= 17;
            count += 0x3FC0;
            width += 64;
        }

        gim->loadPhase = 2;
        break;
    case 2:
        Core_Memory_FreeFromHighEnd(gim->fileData);

        for (j = 0; j < 3; ++j) {
            gim[j].loading = 0;
        }

        gim->loadPhase = 3;
        break;
    case 3:
        for (j = 0; j < 3; ++j) {
            if (gim[j].drawEnabled) {
                Render_DrawGimLayer(j);
            }
        }

        break;
    }
}

// decomp: BATTLE.PRG func_800CBBCC (src/BATTLE/BATTLE.PRG/5BF94.c:4056)
void Render_DrawGimTiles(Render_GimLayer* image, int clut, u_long* ot)
{
    int alpha = image->brightness;
    int background;
    int quadColor;
    u_short* tiles = &image->tileTable[4];
    int scale = image->scaleAndOrder & 0x3FFF;
    int columns = image->columns;
    int rows = image->rows;
    int color;
    // Byte-sized dimensions need at most 256 boundary coordinates per axis.
    short columnX[256];
    short rowY[256];
    int i;
    int j = image->offsetX + 160;

    if (columns * rows > (int)(sizeof image->tileTable / sizeof image->tileTable[0]) - 4) {
        Core_Debugf("render", "GIM grid %dx%d exceeds tile table capacity", columns, rows);
        return;
    }

    for (i = 0; i <= columns; ++i) {
        columnX[i] = (((i * 2 - columns) * scale) >> 7) + j;
    }

    j = image->offsetY + 120;

    for (i = 0; i <= rows; ++i) {
        rowY[i] = (((i * 2 - rows) * scale * 15) >> 13) + j;
    }

    color = image->backdropColor;

    if (color) {
        int r = ((color << 3) & 0xF8) * alpha;
        int g = ((color >> 2) & 0xF8) * alpha;
        int b = ((color >> 7) & 0xF8) * alpha;

        background =
            (r >> 7) | ((g >> 7) << 8) | ((b >> 7) << 16) | ((Render_gimBlendSelect + 48) << 25);

        i = rowY[0];

        if (i > 0) {
            Render_InsertThreeWordPrimitive(ot, background, 0, 320 | (i << 16));
        }

        i = rowY[rows];

        if (i < 240) {
            Render_InsertThreeWordPrimitive(ot, background, i << 16, 320 | (0xF00000 - (i << 16)));
        }

        i = columnX[0];

        if (i > 0) {
            Render_InsertThreeWordPrimitive(ot, background, 0, i | 0xF00000);
        }

        i = columnX[columns];

        if (i < 320) {
            Render_InsertThreeWordPrimitive(ot, background, i & 0xFFFF, (320 - i) | 0xF00000);
        }
    }

    quadColor = alpha | (alpha << 8) | (alpha << 16) | ((Render_gimBlendSelect + 22) << 25);
    alpha |= Render_gimBlendSelect << 8;

    for (j = 0; j < rows; ++j) {
        int top = rowY[j];
        int bottom;

        if (top >= 240) {
            continue;
        }

        bottom = rowY[j + 1];

        if (bottom < 0) {
            continue;
        }

        for (i = 0; i < columns; ++i) {
            int left = columnX[i];
            int right;
            int tile;

            if (left >= 320) {
                continue;
            }

            right = columnX[i + 1];

            if (right < 0) {
                continue;
            }

            tile = tiles[i + j * columns];

            if (tile) {
                u_long* prim;
                int v = tile * 64;
                int u = v & 0xC0;
                int tpage;

                v = (v & 0x1F00) * 15;
                tpage = getTPage(image->textureMode, 1,
                    ((image->vramPage - 18) * 64) + ((tile >> 2) & 0x1C0), 256);

                if (scale == ONE) {
                    prim = Render_QueueSprite(
                        alpha, left | (top << 16), vs_getWH(64, 15), ot);
                } else {
                    int x0 = left & 0xFFFF;
                    int u1 = u + 64;

                    u1 -= u1 >> 8;
                    prim = Render_frameScratch.primitiveCursor;
                    prim[0] = (*ot & 0xFFFFFF) | 0xA000000;
                    prim[2] = quadColor;
                    prim[3] = x0 | (top << 16);
                    prim[5] = right | (top << 16);
                    prim[6] = u1 | v | (tpage << 16);
                    prim[7] = x0 | (bottom << 16);
                    prim[8] = u | (v + 0xF00);
                    prim[9] = right | (bottom << 16);
                    prim[10] = u1 | (v + 0xF00);
                    *ot = LegacyGpu_Handle(prim);
                    Render_frameScratch.primitiveCursor = prim + 11;
                }

                prim[1] = tpage | _get_mode(0, 0, 0);
                prim[4] = u | v | clut;
            } else if (color && (color != 0x8000 || !Render_gimBlendSelect)) {
                Render_InsertThreeWordPrimitive(ot, background, (left & 0xFFFF) | (top << 16),
                    (right - left) | ((bottom - top) << 16));
            }
        }
    }
}

// decomp: BATTLE.PRG func_800CC128 (src/BATTLE/BATTLE.PRG/5BF94.c:4192)
void Render_DrawGimFramebufferFade(Render_GimLayer* arg0, int arg1, u_long* arg2)
{
    int var_s1;
    int new_var;

    int temp_s0 = arg0->brightness;

    if (temp_s0 == 0x80) {
        Render_DrawGimTiles(arg0, arg1, arg2);
        return;
    }

    Render_gimBlendSelect = 1;

    Render_DrawGimTiles(arg0, arg1, arg2);
    var_s1 = 0;

    if (Render_frameBuf == 0) {
        var_s1 = 0x140;
    }

    new_var = 0x120;
    Render_QueueSprite(0x80 - temp_s0, 0, 0xF00100, arg2)[1] =
        ((u_int)var_s1 >> 6) | new_var | 0xE1000000;
    Render_QueueSprite(0x80 - temp_s0, 0x100, 0xF00040, arg2)[1] =
        ((var_s1 + 0x100) >> 6) | new_var | 0xE1000000;
}

// decomp: BATTLE.PRG func_800CC204 (src/BATTLE/BATTLE.PRG/5BF94.c:4220)
void Render_DrawGimScanlineWipe(Render_GimLayer* image, int clut, u_long* ot)
{
    short left[240];
    short right[240];
    u_short* tiles = &image->tileTable[4];
    int mode = image->textureMode;
    u_short* widths = image->scanlineWipeWidths;
    int page = image->vramPage - 18;
    int color = image->backdropColor;
    int y;
    int column;
    int end = image->brightness;
    int x = image->scaleAndOrder & 0x3FFF;

    if (x == ONE) {
        image->brightness = 128;
        Render_DrawGimTiles(image, clut, ot);
        image->brightness = end;
        return;
    }

    if (x < ONE) {
        for (y = 0; y < 240; ++y) {
            column = (widths[y] * x) >> 12;

            if (end) {
                left[y] = 320 - column;
                right[y] = 320;
            } else {
                left[y] = 0;
                right[y] = column;
            }
        }
    } else {
        for (y = 0; y < 240; ++y) {
            column = (widths[y] * (x - ONE)) >> 12;

            if (end) {
                left[y] = 0;
                right[y] = 320 - column;
            } else {
                left[y] = column;
                right[y] = 320;
            }
        }
    }

    for (y = 0; y < 240; ++y) {
        for (column = 0; column < 5; ++column) {
            int tile;

            x = left[y];

            if (x >= (column + 1) * 64) {
                continue;
            }

            end = right[y];

            if (end <= column * 64) {
                continue;
            }

            if (x < column * 64) {
                x = column * 64;
            }

            if (end > (column + 1) * 64) {
                end = (column + 1) * 64;
            }

            tile = tiles[(y / 15) * 5 + column];

            if (tile) {
                int tpage = getTPage(mode, 1, (page + (tile >> 8)) * 64, 256);

            draw:
                do {
                    u_long* prim = Render_QueueSprite(128, x | (y << 16),
                        (x & 1) ? vs_getWH(1, 1) : ((end - x) | 0x10000), ot);

                    prim[1] = tpage | _get_mode(0, 0, 0);
                    prim[4] = (((tile & 3) * 64) + (x & 0x3F))
                            | ((((tile >> 2) & 0x1F) * 15 + (y % 15)) << 8) | clut;
                } while (0);
                if (x & 1) {
                    ++x;
                    goto draw;
                }
            } else if (color) {
                Render_InsertThreeWordPrimitive(ot,
                    ((color & 0x1F) << 3) | ((color & 0x3E0) << 6)
                        | ((color & 0x7C00) << 9) | 0x40000000,
                    (x & 0xFFFF) | (y << 16), (end - 1) | (y << 16));
            }
        }
    }
}

// decomp: BATTLE.PRG func_800CC580 (src/BATTLE/BATTLE.PRG/5BF94.c:4319)
void Render_SetGimWorkspaceDrawArea(u_long* arg0, int arg1)
{
    Render_InsertThreeWordPrimitive(
        arg0, arg1 | 0xE3040000, (arg1 + 0x3F) | 0xE407FC00, arg1 | 0xE5080000);
}

// decomp: BATTLE.PRG func_800CC5C0 (src/BATTLE/BATTLE.PRG/5BF94.c:4325)
void Render_SetGimFramebufferDrawArea(u_long* arg0, int arg1)
{
    Render_InsertThreeWordPrimitive(
        arg0, arg1 | 0xE3000000, (arg1 + 0x13F) | 0xE403BC00, arg1 | 0xE5000000);
}

// decomp: BATTLE.PRG func_800CC600 (src/BATTLE/BATTLE.PRG/5BF94.c:4331)
void Render_DrawGimCrossfade(Render_GimLayer* image, int clut, u_long* ot)
{
    int alpha;
    int inverse;
    u_short* tiles = &image->tileTable[4];
    int mode = image->textureMode;
    int columns;
    int column;
    int top;
    int fb = 0;
    int page;
    int height;
    int x;
    int y;

    if (Render_frameBuf == 0) {
        fb = 320;
    }

    alpha = image->brightness;
    page = (image->vramPage + image->texturePageCount - 16) * 64;

    if (alpha == 128) {
        Render_DrawGimTiles(image, clut, ot);
        return;
    }

    inverse = 128 - alpha;
    alpha |= 256;
    inverse += 7;
    alpha += 7;
    columns = image->columns;
    height = image->rows * 15;
    top = image->offsetY + 120 - (height >> 1);

    for (column = 0; column < columns; ++column) {
        u_long* prim = Render_QueueSprite(
            384, (column * 64) | (top << 16), 64 | (height << 16), ot);

        prim[1] = _get_mode(0, 0, getTPage(2, 1, page, 256));
        Render_SetGimFramebufferDrawArea(ot, fb);

        for (y = height - 15; y >= 0; y -= 15) {
            for (x = 32; x >= 0; x -= 32) {
                prim = Render_QueueSprite(384, x | (y << 16), vs_getWH(32, 15), ot);
                prim[1] = _get_mode(0, 0, getTPage(2, 2, page, 256));
                prim[4] = vs_getUV(x, y);
                Render_SetGimWorkspaceDrawArea(ot, page);
                prim = Render_QueueSprite(
                    alpha, (x + column * 64) | ((top + y) << 16), vs_getWH(32, 15), ot);
                prim[1] = _get_mode(0, 0, getTPage(2, 1, page, 256));
                prim[4] = vs_getUV(x, y);
                Render_SetGimFramebufferDrawArea(ot, fb);
            }
        }

        x = image->offsetY + 120;

        for (y = 0; y < image->rows; ++y) {
            int tile = tiles[y * columns + column];
            int atlas;
            int uv;
            int tx;

            if (!tile) {
                continue;
            }

            atlas = tile >> 2;
            prim = Render_QueueSprite(
                128, (((y * 30 - height) >> 1) + x - top) << 16, vs_getWH(64, 15), ot);
            uv = ((tile * 64) & 192) | ((atlas & 31) * 15 << 8) | clut;
            tx = ((image->vramPage - 18) * 64) + (atlas & 448);
            prim[4] = uv;
            prim[1] = _get_mode(0, 0, getTPage(mode, 0, tx, 256));
        }

        Render_SetGimWorkspaceDrawArea(ot, page);

        for (y = 0; y < height; y += 15) {
            for (x = 0; x < 64; x += 32) {
                prim = Render_QueueSprite(
                    inverse, (x + column * 64) | ((top + y) << 16), vs_getWH(32, 15), ot);
                prim[1] = _get_mode(0, 0, getTPage(2, 0, fb + column * 64, 0));
                prim[4] = vs_getUV(x, top + y);
                Render_SetGimFramebufferDrawArea(ot, fb);
                prim = Render_QueueSprite(128, x | (y << 16), vs_getWH(32, 15), ot);
                prim[1] = _get_mode(0, 0, getTPage(2, 0, fb + column * 64, 0));
                prim[4] = vs_getUV(x, top + y);
                Render_SetGimWorkspaceDrawArea(ot, page);
            }
        }

        Render_InsertThreeWordPrimitive(ot, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0), 0, 64 | (height << 16));
        Render_SetGimWorkspaceDrawArea(ot, page);
    }
}

// decomp: BATTLE.PRG func_800CCA90 (src/BATTLE/BATTLE.PRG/5BF94.c:4429)
void Render_DrawGimLayer(int arg0)
{
    Render_GimLayer* gim = &(*(Render_GimLayer* (*))&Render_gimLayers)[arg0];
    int clut = (gim->vramPage - 16) << 6;

    if ((gim->brightness == 0) && (gim->drawRoutine != 2)) {
        return;
    }

    if (gim->textureMode) {
        if (gim->ownsPalette) {
            clut = getClut(clut, 511);
        } else {
            clut = getClut(768, 227);
        }
    } else {
        clut = getClut(clut + arg0 * 16, 511);
    }

    Render_gimBlendSelect = 0;
    Render_kGimLayerDrawRoutines[gim->drawRoutine](gim, clut << 16,
        gim->scaleAndOrder & 0x4000 ? Render_frameScratch.orderingTable + 0x1FF8 - arg0 * 4
                           : Render_frameScratch.overlayOrderingTable + 0x18 - arg0 * 8);
}

