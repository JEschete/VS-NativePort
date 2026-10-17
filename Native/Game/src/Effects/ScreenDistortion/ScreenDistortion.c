// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffects.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"

// decomp: SCREFF2.PRG func_800F9800 (src/GIM/SCREFF2.PRG/0.c:8)
void Effects_ScreenDistortion_SetFeedbackLayer(int arg0)
{
    Effects_screenEffectOverlay->feedbackLayer = arg0;
    if (Effects_screenEffectOverlay->opaque1 == 0) {
        if (arg0 != 0) {
            Effects_screenEffectOverlay->opaque1 = 1U;
            Render_ReserveTexturePageRange(0x1B, 5, 6);
        }
    } else if (arg0 == 0) {
        Effects_screenEffectOverlay->opaque1 = 0U;
        Render_RetireTexturePageRange(0x1B, 5, 6);
    }
}

// decomp: SCREFF2.PRG func_800F986C (src/GIM/SCREFF2.PRG/0.c:22)
void Effects_ScreenDistortion_DrawFeedbackTiles(void)
{
    int temp_v0;
    int var_s0;
    int x;
    int var_s2;
    int y;
    u_long* temp_s4;
    u_long* var_t2;

    temp_s4 = Render_frameScratch.orderingTable;

    for (y = 192; y >= 0; y -= 32) {
        for (var_s2 = 0x100; var_s2 >= 0; var_s2 -= 64) {
            for (x = 0x40; x >= 0; x -= 0x20) {
                int s5;
                if (x == 0x40) {
                    var_t2 = Render_QueueSprite(
                        0x160, (var_s2 + 2) | ((y + 30) << 16), 0x40000 | 0x40, temp_s4);
                    var_s0 = (y + 0x1C) << 8;
                } else {
                    var_t2 = Render_QueueSprite(
                        0x160, (var_s2 + x + 0x1E) | ((y + 2) << 16), 0x1C0004, temp_s4);
                    var_s0 = (x + 0x1C) | (y << 8);
                }

                s5 = (((var_s2 + 0x2C0) & 0x3FF) >> 6) | 0x170;

                var_t2[1] = s5 | 0xE1000200;
                var_t2[4] = var_s0;
            }
        }
    }

    for (y = 192; y >= 0; y -= 32) {
        for (x = 0x120; x >= 0; x -= 32) {
            for (var_s0 = 0; var_s0 < 3; ++var_s0) {
                switch (var_s0) {
                case 0:
                    var_t2 = Render_QueueSprite(
                        0x160, (x + 2) | ((y + 2) << 16), 0x1C001C, temp_s4);
                    break;
                case 1:
                    var_t2 = Render_QueueSprite(
                        0x160, ((x - 2) & 0xFFFF) | ((y - 2) << 16), 0x200020, temp_s4);
                    break;
                case 2:
                    var_t2 =
                        Render_QueueSprite(0x60, x | (y << 0x10), 0x200020, temp_s4);
                    break;
                }

                temp_v0 = (((x & 0x1C0) + 0x2C0) & 0x3FF) >> 6;

                if (var_s0 == 0) {
                    temp_v0 |= 0x170;
                    var_t2[1] = temp_v0 | 0xE1000200;
                } else {
                    temp_v0 |= 0x110;
                    var_t2[1] = temp_v0 | 0xE1000200;
                }

                var_t2[4] = (x & 0x20) | (y << 8);
            }
        }
    }

    var_s0 = Effects_screenEffectOverlay->feedbackLayer - 3;
    var_t2 = Render_frameScratch.primitiveCursor;
    var_t2[0] = ((temp_s4[var_s0] & 0xFFFFFF) | 0x07000000);
    var_t2[1] = 0xE30402C0;
    var_t2[2] = 0xE4077FFF;
    var_t2[3] = 0xE50802C0;
    var_t2[4] = 0x60000000;
    var_t2[5] = 0;
    var_t2[6] = 0xE00140;
    var_t2[7] = 0xE6000001;
    temp_s4[var_s0] = ((uintptr_t)var_t2 << 8) >> 8;
    var_t2 += 8;

    x = Render_frameBuf == 0 ? 0x140 : 0;

    var_t2[0] = ((*temp_s4 & 0xFFFFFF) | 0x04000000);
    var_t2[1] = x | 0xE3000000;
    var_t2[2] = (x + 0x13F) | 0xE4037C00;
    var_t2[3] = x | 0xE5000000;
    var_t2[4] = 0xE6000000;

    *temp_s4 = ((u_long)var_t2 << 8) >> 8;
    Render_frameScratch.primitiveCursor = var_t2 + 5;
}

// decomp: SCREFF2.PRG func_800F9BC0 (src/GIM/SCREFF2.PRG/0.c:114)
void Effects_ScreenDistortion_SetRadialWarp(short arg0, short arg1)
{
    Effects_screenEffectOverlay->radialWarpArg0 = arg0;
    Effects_screenEffectOverlay->radialWarpArg1 = arg1;
}

// decomp: SCREFF2.PRG func_800F9BD8 (src/GIM/SCREFF2.PRG/3D8.s:0) [asm-file: not yet ported]
void Effects_ScreenDistortion_WarpPolygonVertices(void)
{
    VS_UNPORTED("Effects::ScreenDistortion::WarpPolygonVertices");
}

// decomp: SCREFF2.PRG func_800F9DE8 (src/GIM/SCREFF2.PRG/5E8.c:8)
void Effects_ScreenDistortion_SetVignetteEnabled(int arg0) { Effects_screenEffectOverlay->vignetteEnabled = arg0; }

// decomp: SCREFF2.PRG func_800F9DF8 (src/GIM/SCREFF2.PRG/5E8.c:10)
int Effects_ScreenDistortion_EllipsePoint(int arg0, int arg1)
{
    int var_s1;

    var_s1 = 8 - ((arg0 + 8) & 0xF);
    if (var_s1 < 0) {
        var_s1 = -var_s1;
    }
    var_s1 += 0x40;
    return ((((((Core_Gte_Cos(arg0 << 6) * arg1 * 0xB) >> 0xF) * var_s1) >> 6) + 0xA0) & 0xFFFF)
         | ((((((Core_Gte_Sin(arg0 << 6) * arg1) >> 0xC) * var_s1) >> 6) + 0x70) << 0x10);
}

// decomp: SCREFF2.PRG func_800F9EBC (src/GIM/SCREFF2.PRG/5E8.c:23)
int* Effects_ScreenDistortion_AppendVignetteCorner(int arg0, int arg1, int* arg2, void** arg3)
{
    int var_s2;

    for (var_s2 = arg1; var_s2 < arg1 + 4; ++var_s2) {
        arg2[0] = (LegacyGpu_Handle(*arg3) & 0xFFFFFF) | 0x06000000;
        arg2[1] = 0x32FFFFFF;
        arg2[2] = arg0;
        arg2[3] = 0xA0A0A0;
        arg2[4] = Effects_ScreenDistortion_EllipsePoint(var_s2 + 1, 0x80);
        arg2[5] = 0xA0A0A0;
        arg2[6] = Effects_ScreenDistortion_EllipsePoint(var_s2, 0x80);
        *arg3 = (void*)(((uintptr_t)arg2 << 8) >> 8);
        arg2 += 7;
    }
    return arg2;
}

// decomp: SCREFF2.PRG func_800F9FB8 (src/GIM/SCREFF2.PRG/5E8.c:41)
void Effects_ScreenDistortion_DrawVignette(void)
{
    int* var_s2;
    int i;
    void** temp_s4;

    temp_s4 = Render_frameScratch.overlayOrderingTable;
    var_s2 = Effects_ScreenDistortion_AppendVignetteCorner(0xE00140, 6, Render_frameScratch.primitiveCursor, temp_s4);
    var_s2 = Effects_ScreenDistortion_AppendVignetteCorner(0xE00000, 0x16, var_s2, temp_s4);
    var_s2 = Effects_ScreenDistortion_AppendVignetteCorner(0, 0x26, var_s2, temp_s4);
    var_s2 = Effects_ScreenDistortion_AppendVignetteCorner(0x140, 0x36, var_s2, temp_s4);

    for (i = 0; i < 0x40; ++i) {
        var_s2[0] = (LegacyGpu_Handle(*temp_s4) & 0xFFFFFF) | 0x09000000;
        var_s2[1] = 0xE1000220;
        var_s2[2] = 0x3AA0A0A0;
        var_s2[3] = Effects_ScreenDistortion_EllipsePoint(i + 1, 0x80);
        var_s2[4] = 0xA0A0A0;
        var_s2[5] = Effects_ScreenDistortion_EllipsePoint(i, 0x80);
        var_s2[6] = 0;
        var_s2[7] = Effects_ScreenDistortion_EllipsePoint(i + 1, 0x60);
        var_s2[8] = 0;
        var_s2[9] = Effects_ScreenDistortion_EllipsePoint(i, 0x60);
        *temp_s4 = (void*)(((uintptr_t)var_s2 << 8) >> 8);
        var_s2 += 10;
    }
    Render_frameScratch.primitiveCursor = var_s2;
}

