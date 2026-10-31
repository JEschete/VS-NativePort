// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
void Effects_ScreenDistortion_SetFeedbackLayer(int ordering_layer)
{
    Effects_screenEffectOverlay->feedbackLayer = ordering_layer;
    if (Effects_screenEffectOverlay->opaque1 == 0) {
        if (ordering_layer != 0) {
            Effects_screenEffectOverlay->opaque1 = 1U;
            Render_ReserveTexturePageRange(0x1B, 5, 6);
        }
    } else if (ordering_layer == 0) {
        Effects_screenEffectOverlay->opaque1 = 0U;
        Render_RetireTexturePageRange(0x1B, 5, 6);
    }
}

// decomp: SCREFF2.PRG func_800F986C (src/GIM/SCREFF2.PRG/0.c:22)
void Effects_ScreenDistortion_DrawFeedbackTiles(void)
{
    int texture_page_bits;
    int var_s0;
    int x;
    int tile_strip_x;
    int y;
    u_long* ordering_table;
    u_long* packet_words;

    ordering_table = (u_long *)(Render_frameScratch.orderingTable);

    for (y = 192; y >= 0; y -= 32) {
        for (tile_strip_x = 0x100; tile_strip_x >= 0; tile_strip_x -= 64) {
            for (x = 0x40; x >= 0; x -= 0x20) {
                int texture_page_mode;
                if (x == 0x40) {
                    packet_words = Render_QueueSprite(
                        0x160, (tile_strip_x + 2) | ((y + 30) << 16), 0x40000 | 0x40, ordering_table);
                    var_s0 = (y + 0x1C) << 8;
                } else {
                    packet_words = Render_QueueSprite(
                        0x160, (tile_strip_x + x + 0x1E) | ((y + 2) << 16), 0x1C0004, ordering_table);
                    var_s0 = (x + 0x1C) | (y << 8);
                }

                texture_page_mode = (((tile_strip_x + 0x2C0) & 0x3FF) >> 6) | 0x170;

                packet_words[1] = texture_page_mode | 0xE1000200;
                packet_words[4] = var_s0;
            }
        }
    }

    for (y = 192; y >= 0; y -= 32) {
        for (x = 0x120; x >= 0; x -= 32) {
            for (var_s0 = 0; var_s0 < 3; ++var_s0) {
                switch (var_s0) {
                case 0:
                    packet_words = Render_QueueSprite(
                        0x160, (x + 2) | ((y + 2) << 16), 0x1C001C, ordering_table);
                    break;
                case 1:
                    packet_words = Render_QueueSprite(
                        0x160, ((x - 2) & 0xFFFF) | ((y - 2) << 16), 0x200020, ordering_table);
                    break;
                case 2:
                    packet_words =
                        Render_QueueSprite(0x60, x | (y << 0x10), 0x200020, ordering_table);
                    break;
                }

                texture_page_bits = (((x & 0x1C0) + 0x2C0) & 0x3FF) >> 6;

                if (var_s0 == 0) {
                    texture_page_bits |= 0x170;
                    packet_words[1] = texture_page_bits | 0xE1000200;
                } else {
                    texture_page_bits |= 0x110;
                    packet_words[1] = texture_page_bits | 0xE1000200;
                }

                packet_words[4] = (x & 0x20) | (y << 8);
            }
        }
    }

    var_s0 = Effects_screenEffectOverlay->feedbackLayer - 3;
    packet_words = (u_long *)(Render_frameScratch.primitiveCursor);
    packet_words[0] = ((ordering_table[var_s0] & 0xFFFFFF) | 0x07000000);
    packet_words[1] = 0xE30402C0;
    packet_words[2] = 0xE4077FFF;
    packet_words[3] = 0xE50802C0;
    packet_words[4] = 0x60000000;
    packet_words[5] = 0;
    packet_words[6] = 0xE00140;
    packet_words[7] = 0xE6000001;
    ordering_table[var_s0] = LegacyGpu_Handle(packet_words);
    packet_words += 8;

    x = Render_frameBuf == 0 ? 0x140 : 0;

    packet_words[0] = ((*ordering_table & 0xFFFFFF) | 0x04000000);
    packet_words[1] = x | 0xE3000000;
    packet_words[2] = (x + 0x13F) | 0xE4037C00;
    packet_words[3] = x | 0xE5000000;
    packet_words[4] = 0xE6000000;

    *ordering_table = LegacyGpu_Handle(packet_words);
    Render_frameScratch.primitiveCursor = packet_words + 5;
}

// decomp: SCREFF2.PRG func_800F9BC0 (src/GIM/SCREFF2.PRG/0.c:114)
void Effects_ScreenDistortion_SetRadialWarp(short initial_scale_q12, short radial_growth_q12)
{
    Effects_screenEffectOverlay->radialWarpArg0 = initial_scale_q12;
    Effects_screenEffectOverlay->radialWarpArg1 = radial_growth_q12;
}

// decomp: SCREFF2.PRG func_800F9DE8 (src/GIM/SCREFF2.PRG/5E8.c:8)
void Effects_ScreenDistortion_SetVignetteEnabled(int enabled) { Effects_screenEffectOverlay->vignetteEnabled = enabled; }

// decomp: SCREFF2.PRG func_800F9DF8 (src/GIM/SCREFF2.PRG/5E8.c:10)
int Effects_ScreenDistortion_EllipsePoint(int angular_step, int radius)
{
    int radial_modulation_q6;

    radial_modulation_q6 = 8 - ((angular_step + 8) & 0xF);
    if (radial_modulation_q6 < 0) {
        radial_modulation_q6 = -radial_modulation_q6;
    }
    radial_modulation_q6 += 0x40;
    return ((((((Core_Gte_Cos(angular_step << 6) * radius * 0xB) >> 0xF) * radial_modulation_q6) >> 6) + 0xA0) & 0xFFFF)
         | ((((((Core_Gte_Sin(angular_step << 6) * radius) >> 0xC) * radial_modulation_q6) >> 6) + 0x70) << 0x10);
}

// decomp: SCREFF2.PRG func_800F9EBC (src/GIM/SCREFF2.PRG/5E8.c:23)
int* Effects_ScreenDistortion_AppendVignetteCorner(int packed_corner_xy, int first_angular_step, int* packet_words, u_long* ordering_head)
{
    int angular_step;

    for (angular_step = first_angular_step; angular_step < first_angular_step + 4; ++angular_step) {
        packet_words[0] = (*ordering_head & 0xFFFFFF) | 0x06000000;
        packet_words[1] = 0x32FFFFFF;
        packet_words[2] = packed_corner_xy;
        packet_words[3] = 0xA0A0A0;
        packet_words[4] = Effects_ScreenDistortion_EllipsePoint(angular_step + 1, 0x80);
        packet_words[5] = 0xA0A0A0;
        packet_words[6] = Effects_ScreenDistortion_EllipsePoint(angular_step, 0x80);
        *ordering_head = LegacyGpu_Handle(packet_words);
        packet_words += 7;
    }
    return packet_words;
}

// decomp: SCREFF2.PRG func_800F9FB8 (src/GIM/SCREFF2.PRG/5E8.c:41)
void Effects_ScreenDistortion_DrawVignette(void)
{
    int* packet_words;
    int i;
    u_long* overlay_ordering_table;

    overlay_ordering_table = (u_long *)(Render_frameScratch.overlayOrderingTable);
    packet_words = Effects_ScreenDistortion_AppendVignetteCorner(0xE00140, 6, (int *)(Render_frameScratch.primitiveCursor), overlay_ordering_table);
    packet_words = Effects_ScreenDistortion_AppendVignetteCorner(0xE00000, 0x16, packet_words, overlay_ordering_table);
    packet_words = Effects_ScreenDistortion_AppendVignetteCorner(0, 0x26, packet_words, overlay_ordering_table);
    packet_words = Effects_ScreenDistortion_AppendVignetteCorner(0x140, 0x36, packet_words, overlay_ordering_table);

    for (i = 0; i < 0x40; ++i) {
        packet_words[0] = (*overlay_ordering_table & 0xFFFFFF) | 0x09000000;
        packet_words[1] = 0xE1000220;
        packet_words[2] = 0x3AA0A0A0;
        packet_words[3] = Effects_ScreenDistortion_EllipsePoint(i + 1, 0x80);
        packet_words[4] = 0xA0A0A0;
        packet_words[5] = Effects_ScreenDistortion_EllipsePoint(i, 0x80);
        packet_words[6] = 0;
        packet_words[7] = Effects_ScreenDistortion_EllipsePoint(i + 1, 0x60);
        packet_words[8] = 0;
        packet_words[9] = Effects_ScreenDistortion_EllipsePoint(i, 0x60);
        *overlay_ordering_table = LegacyGpu_Handle(packet_words);
        packet_words += 10;
    }
    Render_frameScratch.primitiveCursor = packet_words;
}

}  // extern "C"
