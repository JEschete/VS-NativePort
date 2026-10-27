// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Appearance.h"
#include "Field/Background.h"
#include "Field/RoomRendering.h"
#include "Render/Palette.h"

// decomp: _clutState (_clutState@SLUS_010.40)
Render_PaletteTransitionState Render_paletteTransitionState;

// decomp: _clutBuffer (_clutBuffer@SLUS_010.40)
u_short Render_paletteColors[14][256];

// decomp: _animOffset16 (_animOffset16@SLUS_010.40)
signed char Render_paletteDeltaSequenceSixteen[][16] = {
    { 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 255 },
    { 254, 254, 254, 254, 254, 254, 254, 254, 255, 254, 254, 254, 254, 254, 254, 255 },
    { 254, 254, 254, 254, 254, 255, 254, 254, 254, 254, 255, 254, 254, 254, 254, 255 },
    { 254, 254, 254, 254, 255, 254, 254, 254, 255, 254, 254, 254, 255, 254, 254, 255 },
    { 254, 254, 254, 255, 254, 254, 255, 254, 254, 255, 254, 254, 255, 254, 254, 255 },
    { 254, 254, 255, 254, 254, 255, 254, 254, 255, 254, 255, 254, 254, 255, 254, 255 },
    { 254, 254, 255, 254, 255, 254, 255, 254, 254, 255, 254, 255, 254, 255, 254, 255 },
    { 254, 254, 255, 254, 255, 254, 255, 254, 255, 254, 255, 254, 255, 254, 255, 255 },
    { 254, 255, 254, 255, 254, 255, 254, 255, 255, 254, 255, 254, 255, 254, 255, 255 },
    { 254, 255, 254, 255, 255, 254, 255, 254, 255, 255, 254, 255, 255, 254, 255, 255 },
    { 254, 255, 255, 254, 255, 255, 254, 255, 255, 254, 255, 255, 254, 255, 255, 255 },
    { 254, 255, 255, 254, 255, 255, 255, 254, 255, 255, 255, 254, 255, 255, 255, 255 },
    { 254, 255, 255, 255, 255, 254, 255, 255, 255, 255, 254, 255, 255, 255, 255, 255 },
    { 254, 255, 255, 255, 255, 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 0 },
    { 255, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0 },
    { 255, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 0 },
    { 255, 255, 255, 0, 255, 255, 0, 255, 255, 0, 255, 255, 0, 255, 255, 0 },
    { 255, 255, 0, 255, 255, 0, 255, 255, 0, 255, 0, 255, 255, 0, 255, 0 },
    { 255, 255, 0, 255, 0, 255, 0, 255, 255, 0, 255, 0, 255, 0, 255, 0 },
    { 255, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 0 },
    { 255, 0, 255, 0, 255, 0, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0 },
    { 255, 0, 255, 0, 0, 255, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0 },
    { 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0 },
    { 255, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 0 },
    { 255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1 },
    { 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
    { 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1 },
    { 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1 },
    { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
    { 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1 },
    { 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1 },
    { 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2 },
    { 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2 },
    { 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2 },
    { 1, 1, 2, 1, 1, 2, 1, 2, 1, 1, 2, 1, 1, 2, 1, 2 },
    { 1, 1, 2, 1, 2, 1, 2, 1, 1, 2, 1, 2, 1, 2, 1, 2 },
    { 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 },
    { 1, 2, 1, 2, 1, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2, 2 },
    { 1, 2, 1, 2, 2, 1, 2, 2, 1, 2, 1, 2, 2, 1, 2, 2 },
    { 1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2, 2 },
    { 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2 },
    { 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2 },
    { 1, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2 },
    { 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 }
};

// decomp: _animOffset2 (_animOffset2@SLUS_010.40)
signed char Render_paletteDeltaSequenceTwo[][2] = { { 240, 241 }, { 240, 242 }, { 241, 242 }, { 241, 243 },
    { 242, 243 }, { 242, 244 }, { 243, 244 }, { 243, 245 }, { 244, 245 }, { 244, 246 },
    { 245, 246 }, { 245, 247 }, { 246, 247 }, { 246, 248 }, { 247, 248 }, { 247, 249 },
    { 248, 249 }, { 248, 250 }, { 249, 250 }, { 249, 251 }, { 250, 251 }, { 250, 252 },
    { 251, 252 }, { 251, 253 }, { 252, 253 }, { 252, 254 }, { 253, 254 }, { 253, 255 },
    { 254, 255 }, { 254, 0 }, { 255, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 },
    { 2, 2 }, { 2, 3 }, { 3, 3 }, { 3, 4 }, { 4, 4 }, { 4, 5 }, { 5, 5 }, { 5, 6 },
    { 6, 6 }, { 6, 7 }, { 7, 7 }, { 7, 8 }, { 8, 8 }, { 8, 9 }, { 9, 9 }, { 9, 10 },
    { 10, 10 }, { 10, 11 }, { 11, 11 }, { 11, 12 }, { 12, 12 }, { 12, 13 }, { 13, 13 },
    { 13, 14 }, { 14, 14 }, { 14, 15 }, { 15, 15 }, { 15, 16 } };

// decomp: _animOffset4 (_animOffset4@SLUS_010.40)
signed char Render_paletteDeltaSequenceFour[][4] = { { 248, 248, 248, 249 }, { 248, 248, 249, 249 },
    { 248, 249, 249, 249 }, { 248, 249, 249, 250 }, { 249, 249, 249, 250 },
    { 249, 249, 250, 250 }, { 249, 250, 250, 250 }, { 249, 250, 250, 251 },
    { 250, 250, 250, 251 }, { 250, 250, 251, 251 }, { 250, 251, 251, 251 },
    { 250, 251, 251, 252 }, { 251, 251, 251, 252 }, { 251, 251, 252, 252 },
    { 251, 252, 252, 252 }, { 251, 252, 252, 253 }, { 252, 252, 252, 253 },
    { 252, 252, 253, 253 }, { 252, 253, 253, 253 }, { 252, 253, 253, 254 },
    { 253, 253, 253, 254 }, { 253, 253, 254, 254 }, { 253, 254, 254, 254 },
    { 253, 254, 254, 255 }, { 254, 254, 254, 255 }, { 254, 254, 255, 255 },
    { 254, 255, 255, 255 }, { 254, 255, 255, 0 }, { 255, 255, 255, 0 },
    { 255, 255, 0, 0 }, { 255, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 1 }, { 0, 1, 0, 1 },
    { 0, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 2 }, { 1, 2, 1, 2 }, { 1, 2, 2, 2 },
    { 2, 2, 2, 2 }, { 2, 2, 2, 3 }, { 2, 3, 2, 3 }, { 2, 3, 3, 3 }, { 3, 3, 3, 3 },
    { 3, 3, 3, 4 }, { 3, 4, 3, 4 }, { 3, 4, 4, 4 }, { 4, 4, 4, 4 }, { 4, 4, 4, 5 },
    { 4, 5, 4, 5 }, { 4, 5, 5, 5 }, { 5, 5, 5, 5 }, { 5, 5, 5, 6 }, { 5, 6, 5, 6 },
    { 5, 6, 6, 6 }, { 6, 6, 6, 6 }, { 6, 6, 6, 7 }, { 6, 7, 6, 7 }, { 6, 7, 7, 7 },
    { 7, 7, 7, 7 }, { 7, 7, 7, 8 }, { 7, 8, 7, 8 }, { 7, 8, 8, 8 } };

// decomp: _animOffset8 (_animOffset8@SLUS_010.40)
signed char Render_paletteDeltaSequenceEight[][8] = { { 252, 252, 252, 252, 252, 252, 252, 253 },
    { 252, 252, 252, 252, 253, 252, 252, 253 },
    { 252, 252, 253, 252, 252, 253, 252, 253 },
    { 252, 252, 253, 252, 253, 252, 253, 253 },
    { 252, 253, 252, 253, 253, 252, 253, 253 },
    { 252, 253, 253, 252, 253, 253, 253, 253 },
    { 252, 253, 253, 253, 253, 253, 253, 253 },
    { 252, 253, 253, 253, 253, 253, 253, 254 },
    { 253, 253, 253, 253, 253, 253, 253, 254 },
    { 253, 253, 253, 253, 254, 253, 253, 254 },
    { 253, 253, 254, 253, 253, 254, 253, 254 },
    { 253, 253, 254, 253, 254, 253, 254, 254 },
    { 253, 254, 253, 254, 254, 253, 254, 254 },
    { 253, 254, 254, 253, 254, 254, 254, 254 },
    { 253, 254, 254, 254, 254, 254, 254, 254 },
    { 253, 254, 254, 254, 254, 254, 254, 255 },
    { 254, 254, 254, 254, 254, 254, 254, 255 },
    { 254, 254, 254, 254, 255, 254, 254, 255 },
    { 254, 254, 255, 254, 254, 255, 254, 255 },
    { 254, 254, 255, 254, 255, 254, 255, 255 },
    { 254, 255, 254, 255, 255, 254, 255, 255 },
    { 254, 255, 255, 254, 255, 255, 255, 255 },
    { 254, 255, 255, 255, 255, 255, 255, 255 }, { 254, 255, 255, 255, 255, 255, 255, 0 },
    { 255, 255, 255, 255, 255, 255, 255, 0 }, { 255, 255, 255, 255, 0, 255, 255, 0 },
    { 255, 255, 0, 255, 255, 0, 255, 0 }, { 255, 255, 0, 255, 0, 255, 0, 0 },
    { 255, 0, 255, 0, 0, 255, 0, 0 }, { 255, 0, 0, 255, 0, 0, 0, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 1, 0, 0, 0, 1 }, { 0, 0, 1, 0, 0, 1, 0, 1 }, { 0, 1, 0, 1, 0, 1, 0, 1 },
    { 0, 1, 0, 1, 1, 0, 1, 1 }, { 0, 1, 1, 1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 2 }, { 1, 1, 1, 2, 1, 1, 1, 2 },
    { 1, 1, 2, 1, 1, 2, 1, 2 }, { 1, 2, 1, 2, 1, 2, 1, 2 }, { 1, 2, 1, 2, 2, 1, 2, 2 },
    { 1, 2, 2, 2, 1, 2, 2, 2 }, { 1, 2, 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 2, 2, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 3 }, { 2, 2, 2, 3, 2, 2, 2, 3 }, { 2, 2, 3, 2, 2, 3, 2, 3 },
    { 2, 3, 2, 3, 2, 3, 2, 3 }, { 2, 3, 2, 3, 3, 2, 3, 3 }, { 2, 3, 3, 3, 2, 3, 3, 3 },
    { 2, 3, 3, 3, 3, 3, 3, 3 }, { 3, 3, 3, 3, 3, 3, 3, 3 }, { 3, 3, 3, 3, 3, 3, 3, 4 },
    { 3, 3, 3, 4, 3, 3, 3, 4 }, { 3, 3, 4, 3, 3, 4, 3, 4 }, { 3, 4, 3, 4, 3, 4, 3, 4 },
    { 3, 4, 3, 4, 4, 3, 4, 4 }, { 3, 4, 4, 4, 3, 4, 4, 4 }, { 3, 4, 4, 4, 4, 4, 4, 4 } };

// decomp: BATTLE.PRG func_8007B10C (src/BATTLE/BATTLE.PRG/146C.c:8241)
void Render_TransformRoomPalettes(int mode, int duration, int red_offset, short green_offset, short blue_offset)
{
    Render_TransformClutSlot(mode, duration, 0, red_offset, green_offset, blue_offset);
    Render_TransformClutSlot(mode, duration, 1, red_offset, green_offset, blue_offset);
    Render_TransformClutSlot(mode, duration, 2, red_offset, green_offset, blue_offset);
}

// decomp: BATTLE.PRG func_8007B1B8 (src/BATTLE/BATTLE.PRG/146C.c:8248)
void Render_TransformSceneColors(int mode, int duration, short red_offset, short green_offset, short blue_offset)
{
    Render_TransformClutSlot(mode, duration, 0, red_offset, green_offset, blue_offset);
    Render_TransformClutSlot(mode, duration, 1, red_offset, green_offset, blue_offset);
    Render_TransformClutSlot(mode, duration, 2, red_offset, green_offset, blue_offset);
    Render_RequestModelTintTransform(mode, duration, 0, red_offset, green_offset, blue_offset);
    Render_TransformClutSlot(mode, duration, 0xD, red_offset, green_offset, blue_offset);
}

// decomp: BATTLE.PRG func_8007B29C (src/BATTLE/BATTLE.PRG/146C.c:8257)
void Render_TransformActorColors(int mode, int duration, int actor_index, short red_offset, short green_offset, short blue_offset)
{
    if (actor_index < 9) {
        Render_TransformClutSlot(mode, duration, actor_index + 4, red_offset, green_offset, blue_offset);
        Render_RequestModelTintTransform(mode, duration, actor_index + 1, red_offset, green_offset, blue_offset);
    }
}

// decomp: BATTLE.PRG func_8007B344 (src/BATTLE/BATTLE.PRG/146C.c:8265)
void Render_TransformBackgroundColors(int mode, int duration, short red_offset, short green_offset, short blue_offset)
{
    Render_RequestBackgroundGradientTransform(mode, duration, red_offset, green_offset, blue_offset);
}

// decomp: BATTLE.PRG func_8007B378 (src/BATTLE/BATTLE.PRG/146C.c:8270)
void Render_LoadRoomPaletteTransition(int duration, u_short* palette_colors, int arg2, int clut_offset)
{
    ((void (*)())Render_StartClutTransition)(duration, 0, palette_colors, clut_offset);
    // BUG: Missing last parameter to vs_main_loadClutTransition
    ((void (*)())Render_StartClutTransition)(duration, 1, palette_colors + 0x100);
    ((void (*)())Render_StartClutTransition)(duration, 2, palette_colors + 0x200);
}

// decomp: BATTLE.PRG func_8007B3D0 (src/BATTLE/BATTLE.PRG/146C.c:8280)
void Render_StartSharedPaletteTransition(int duration, int shared_slot, u_short* palette_colors, int clut_offset)
{
    ((void (*)())Render_StartClutTransition)(duration, shared_slot + 4, palette_colors, clut_offset);
}

// decomp: BATTLE.PRG func_8007B3F0 (src/BATTLE/BATTLE.PRG/146C.c:8285)
void Render_ApplyBackgroundGradientColors(int duration, Render_ColorRgb8* colors) { Render_SetBackgroundGradientColors(duration, colors); }

// decomp: BATTLE.PRG func_800AC0D4 (src/BATTLE/BATTLE.PRG/40564.c:2003)
int Render_StepRgbTowardTarget(u_char* current_channels, u_char* target_channels, int signed_step)
{
    int i;
    int changed = 0;

    for (i = 0; i < 3; ++i) {
        if (signed_step < 0) {
            if (*target_channels < *current_channels) {
                changed = 1;
                *current_channels += signed_step;
                if (*current_channels < *target_channels) {
                    *current_channels = *target_channels;
                }
            }
        } else {
            if (*current_channels < *target_channels) {
                changed = 1;
                *current_channels += signed_step;
                if (*target_channels < *current_channels) {
                    *current_channels = *target_channels;
                }
            }
        }
        ++current_channels;
        ++target_channels;
    }

    return changed;
}

// decomp: BATTLE.PRG func_800AC168 (src/BATTLE/BATTLE.PRG/40564.c:2033)
int Render_ApplyPaletteTransition(u_short* colors, int count, int amount, int mode, u_short* reference)
{
    int i;
    int changed = 0;
    int baseR;
    int baseG;
    int baseB;
    int delta;

    for (i = 0; i < count; ++i, ++colors, ++reference) {
        int b = *colors;
        int r = b & 31;
        int g = (b >> 5) & 31;

        b = (b >> 10) & 31;

        switch (mode) {
        case 0:
            delta = amount * 2;
            r -= delta;

            if (r < 0) {
                r = 0;
            }

            g -= delta;

            if (g < 0) {
                g = 0;
            }

            r |= g << 5;

            if (r != 0) {
                changed = 1;
            }

            r |= b << 10;
            *colors = r | ~0x7FFF;
            break;

        case 1:
            delta = amount * 4;
        fade:
            b -= delta;

            if (b < 0) {
                b = 0;
            }

            r -= delta;

            if (r < 0) {
                r = 0;
            }

            g -= delta;

            if (g < 0) {
                g = 0;
            }

            r |= g << 5;
            r |= b << 10;

            if (r != 0) {
                changed = 1;
            }

            *colors = r | ~0x7FFF;
            break;

        case 2:
            *colors = 0xFFFF;
            break;

        case 3:
            r += amount;

            if (r >= 32) {
                r = 31;
            }

            g += amount * 2;

            if (g >= 32) {
                g = 31;
            }

            b += amount * 2;

            if (b >= 32) {
                b = 31;
            }

            r |= g << 5;
            r |= b << 10;
            *colors = r | ~0x7FFF;
            break;

        case 4:
            baseB = *reference;
            r -= amount * 2;
            baseR = baseB & 31;
            baseG = (baseB >> 5) & 31;
            baseB = (baseB >> 10) & 31;

            if (r < baseR) {
                r = baseR;
            }

            g -= amount * 2;

            if (g < baseG) {
                g = baseG;
            }

            b -= amount * 2;

            if (b < baseB) {
                b = baseB;
            }

            r |= g << 5;
            r |= b << 10;

            if (r != (*reference & 0x7FFF)) {
                changed = 1;
            }

            *colors = (*reference & 0x8000) | r;
            break;

        case 5:
            delta = amount;
            goto fade;

        case 6:
            *colors = 0x8000;
            break;
        }
    }

    return changed;
}

// decomp: SLUS_010.40 _loadClutTransition (src/SLUS_010.40/main.c:10519)
void Render_BuildClutTransition(int duration, int slot, u_short* sourceClut)
{
    int i;

    for (i = 0; i < 256; ++i) {
        u_short r = sourceClut[i] & 0x1F;
        u_short g = (sourceClut[i] & 0x3E0) >> 5;
        u_short b = (sourceClut[i] & 0x7C00) >> 10;

        struct Render_PaletteEntryFade* colors = Render_paletteTransitionState.slots[slot].entries;

        if (duration != 0) {
            colors[i].redStepRow = (r - colors[i].red) + 31;
            colors[i].greenStepRow = (g - colors[i].green) + 31;
            colors[i].blueStepRow = (b - colors[i].blue) + 31;
        } else {
            colors[i].red = r;
            colors[i].green = g;
            colors[i].blue = b;
            Render_paletteColors[slot][i] = r + (g << 5) + (b << 10) + (colors[i].maskBit << 15);
        }
    }

    if (duration != 0) {
        struct Render_PaletteTransitionTimer* state = &Render_paletteTransitionState.slots[slot].transition;
        state->active = 1;
        state->frame = 0;
        state->tick = 0;
        state->speed = duration;
    } else {
        Render_paletteTransitionState.slots[slot].transition.active = 0;
    }
}

// decomp: SLUS_010.40 vs_main_loadClutTransition (src/SLUS_010.40/main.c:10553)
void Render_StartClutTransition(
    int duration, int slot, u_short* sourceClut, int clutOffset)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_BuildClutTransition(duration, slot, sourceClut + (clutOffset * 16));
        Render_paletteTransitionState.uploadPending = 1;
    }
}

// decomp: SLUS_010.40 _loadClutTransitionWithBase (src/SLUS_010.40/main.c:10562)
void Render_StartClutTransitionWithBase(
    int duration, int slot, u_short* sourceClut, int clutOffset)
{
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        ((void (*)())Render_StartClutTransition)(duration, slot, sourceClut, clutOffset);

        for (i = 0; i < 256; ++i) {
            Render_paletteTransitionState.slots[slot].baseColors[i] = sourceClut[i];
        }
    }
}

// decomp: SLUS_010.40 _saveClutBase (src/SLUS_010.40/main.c:10579)
void Render_SaveClutBase(int slot)
{
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        for (i = 0; i < 256; ++i) {
            Render_paletteTransitionState.slots[slot].baseColors[i] = Render_paletteColors[slot][i];
        }
    }
}

// decomp: SLUS_010.40 _weightedLuminance (src/SLUS_010.40/main.c:10590)
int Render_WeightedLuminance(short g, short b, short r)
{
    return r * 2 + (g >> 5) * 3 + (b >> 10);
}

// decomp: SLUS_010.40 _transformClutSlot (src/SLUS_010.40/main.c:10595)
void Render_ApplyClutSlotTransform(
    int mode, int duration, int slot, int rOffset, short gOffset, short bOffset)
{
    int i;
    int gOffsetU = (u_short)gOffset;
    int bOffsetU = (u_short)bOffset;
    struct Render_PaletteEntryFade* colors = Render_paletteTransitionState.slots[slot].entries;

    for (i = 0; i < 256; ++i) {

        int r;
        int g;
        int b;
        short baseEntry = Render_paletteTransitionState.slots[slot].baseColors[i];

        if ((colors[i].red + colors[i].green + colors[i].blue + colors[i].maskBit) != 0) {
            switch (mode) {
            case 1: {
                int rTmp = colors[i].red;
                int gTmp = colors[i].green;
                int bTmp = colors[i].blue;
                r = rOffset + rTmp / 2;
                g = gOffsetU + gTmp / 2;
                b = bOffsetU + bTmp / 2;
                break;
            }
            case 2:
                r = rOffset + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 6;
                g = gOffsetU + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 6;
                b = bOffsetU + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 6;
                break;
            case 3:
                r = rOffset + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 12;
                g = gOffsetU + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 12;
                b = bOffsetU + ((colors[i].red * 2) + (colors[i].green * 3) + colors[i].blue) / 12;
                break;
            case 4:
            case 9:
                r = rOffset + (baseEntry & 0x1F);
                g = gOffsetU + ((baseEntry & 0x3E0) >> 5);
                b = bOffsetU + ((baseEntry & 0x7C00) >> 10);
                break;
            case 5: {
                int tmp = 0x1F;
                r = rOffset + ((baseEntry & tmp) / 2);
                g = gOffsetU + ((baseEntry & 0x3E0) >> 5) / 2;
                b = bOffsetU + ((baseEntry & 0x7C00) >> 10) / 2;
                break;
            }
            case 6:
                r = rOffset
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 6;
                g = gOffsetU
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 6;
                b = bOffsetU
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 6;
                break;
            case 7:
                r = rOffset
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 12;
                g = gOffsetU
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 12;
                b = bOffsetU
                  + Render_WeightedLuminance(
                        baseEntry & 0x3E0, baseEntry & 0x7C00, baseEntry & 0x1F)
                        / 12;
                break;
            case 8:
            case 15:
                r = baseEntry & 0x1F;
                g = (baseEntry & 0x3E0) >> 5;
                b = (baseEntry & 0x7C00) >> 10;
                break;
            case 11:
                Render_paletteTransitionState.slots[slot].transition.mode = 0;
                return;
            case 12: {
                int tmp = 0x1F;
                r = rOffset - (colors[i].red - tmp);
                g = gOffsetU - (colors[i].green - tmp);
                b = bOffsetU - (colors[i].blue - tmp);
                break;
            }
            case 13: {
                int tmp = (baseEntry & 0x1F) * 3;
                if (tmp < 0) {
                    tmp += 3;
                }
                r = rOffset + (tmp >> 2);
                tmp = ((baseEntry & 0x3E0) >> 5) * 3;
                if (tmp < 0) {
                    tmp += 3;
                }
                g = gOffsetU + (tmp >> 2);
                tmp = ((baseEntry & 0x7C00) >> 10) * 3;
                if (tmp < 0) {
                    tmp += 3;
                }
                b = bOffsetU + (tmp >> 2);
                break;
            }
            default:
                r = colors[i].red + rOffset;
                g = colors[i].green + gOffsetU;
                b = colors[i].blue + bOffsetU;
                break;
            }

            if ((short)r > 31) {
                r = 31;
            }
            if ((short)g > 31) {
                g = 31;
            }
            if ((short)b > 31) {
                b = 31;
            }
            if ((r << 16) <= 0) {
                r = 0;
            }
            if ((g << 16) <= 0) {
                g = 0;
            }
            if ((b << 16) <= 0) {
                b = 0;
            }
            if (((b | (r | g)) << 16) == 0) {
                b = 1;
            }
            if (duration != 0) {
                colors[i].redStepRow = ((r - colors[i].red) + 31);
                colors[i].greenStepRow = ((g - colors[i].green) + 31);
                colors[i].blueStepRow = ((b - colors[i].blue) + 31);
            } else {
                colors[i].blue = b;
                colors[i].red = r;
                colors[i].green = g;
                Render_paletteColors[slot][i] = r + (g << 5) + (b << 10) + (colors[i].maskBit << 15);
            }
        } else {
            colors[i].blueStepRow = 31;
            colors[i].greenStepRow = 31;
            colors[i].redStepRow = 31;
        }
    }
    if (duration != 0) {
        struct Render_PaletteTransitionTimer* p = &Render_paletteTransitionState.slots[slot].transition;
        p->active = 1;
        p->frame = 0;
        p->tick = 0;
        p->speed = duration;
        p->mode = mode;
        if (mode == 9) {
            p->pulseRed = rOffset;
            p->pulseGreen = gOffsetU;
            p->pulseBlue = bOffsetU;
        }
    } else {
        Render_paletteTransitionState.slots[slot].transition.active = 0;
        Render_paletteTransitionState.uploadPending = 1;
    }
}

// decomp: SLUS_010.40 vs_main_transformClutSlot (src/SLUS_010.40/main.c:10768)
void Render_TransformClutSlot(
    int mode, int duration, int slot, short rOffset, int gOffset, int bOffset)
{
    if (Render_paletteTransitionState.enabled != 0) {
        do {
            Render_ApplyClutSlotTransform(mode, duration, slot, rOffset, gOffset, bOffset);
        } while (0);
    }
}

// decomp: SLUS_010.40 _shift_left (src/SLUS_010.40/main.c:10778)
int Render_ToFixed16(int value) { return value << 0x10; }

// decomp: SLUS_010.40 func_800472D0 (src/SLUS_010.40/main.c:10780)
void Render_SetBackgroundGradientColors(int duration, Render_ColorRgb8* colors)
{
    int packed_colors[5];
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        int (*color_components_fixed)[6] = Render_paletteTransitionState.backgroundColorsFixed;
        for (i = 0; i < 5; ++i) {
            int r;
            int g;
            int b;
            r = Render_ToFixed16(colors[i].red);
            g = Render_ToFixed16(colors[i].green);
            b = Render_ToFixed16(colors[i].blue);

            if (duration != 0) {
                color_components_fixed[i][3] = ((int)(r - (color_components_fixed[i][0] & 0xFFFF0000)) / (duration * 2));
                color_components_fixed[i][4] = ((int)(g - (color_components_fixed[i][1] & 0xFFFF0000)) / (duration * 2));
                color_components_fixed[i][5] = ((int)(b - (color_components_fixed[i][2] & 0xFFFF0000)) / (duration * 2));
            } else {
                color_components_fixed[i][0] = r;
                color_components_fixed[i][1] = g;
                color_components_fixed[i][2] = b;
            }
        }
        if (duration != 0) {
            Render_paletteTransitionState.backgroundFadeActive = 1;
            Render_paletteTransitionState.backgroundFadeFrame = 0;
            Render_paletteTransitionState.backgroundFadeTick = 0;
            Render_paletteTransitionState.backgroundFadeSpeed = duration;
            return;
        }
        Render_paletteTransitionState.backgroundFadeActive = 0;

        for (i = 0; i < 5; ++i) {
            packed_colors[i] = ((color_components_fixed[i][0] / 65536) + ((color_components_fixed[i][1] / 65536) << 8))
                    + (((color_components_fixed[i][2] / 65536)) << 0x10);
        }
        Field_SetRoomBackgroundColors(packed_colors);
    }
}

// decomp: SLUS_010.40 func_80047464 (src/SLUS_010.40/main.c:10822)
void Render_SetBackgroundGradientBase(int duration, Render_ColorRgb8* colors)
{
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        Render_SetBackgroundGradientColors(duration, colors);
        for (i = 0; i < 5; ++i) {
            Render_paletteTransitionState.backgroundBaseColors[i] = colors[i];
        }
    }
}

// decomp: SLUS_010.40 func_800474DC (src/SLUS_010.40/main.c:10834)
void Render_TransformBackgroundGradient(int mode, int duration, int red_offset, int green_offset, int blue_offset)
{
    int packed_colors[5];
    int luminance_or_channel_fixed;
    int blue_target_or_green_fixed;
    int green_target_fixed;
    int red_target_fixed;
    int i;
    int (*p)[6] = Render_paletteTransitionState.backgroundColorsFixed;

    for (i = 0; i < 5; ++i) {
        switch (mode) {
        case 1:
            red_target_fixed = (p[i][0] / 2) + (red_offset << 0x10);
            green_target_fixed = p[i][1] / 2 + (green_offset << 0x10);
            blue_target_or_green_fixed = (p[i][2] / 2) + (blue_offset << 0x10);
            break;
        case 2:
            red_target_fixed = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (red_offset << 0x10);
            green_target_fixed = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (green_offset << 0x10);
            blue_target_or_green_fixed = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (blue_offset << 0x10);
            break;
        case 3:
            luminance_or_channel_fixed = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 12;
            red_target_fixed = luminance_or_channel_fixed + (red_offset << 0x10);
            green_target_fixed = luminance_or_channel_fixed + (green_offset << 0x10);
            blue_target_or_green_fixed = luminance_or_channel_fixed + (blue_offset << 0x10);
            break;
        case 4:
        case 9:
            red_target_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].red + red_offset) << 0x10;
            green_target_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].green + green_offset) << 0x10;
            blue_target_or_green_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].blue + blue_offset) << 0x10;
            break;
        case 5:
            red_target_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].red << 0xF) + (red_offset << 0x10);
            green_target_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].green << 0xF) + (green_offset << 0x10);
            blue_target_or_green_fixed = (Render_paletteTransitionState.backgroundBaseColors[i].blue << 0xF) + (blue_offset << 0x10);
            break;
        case 6: {
            int base_green_work = Render_paletteTransitionState.backgroundBaseColors[i].green;
            int base_blue_work = Render_paletteTransitionState.backgroundBaseColors[i].blue;
            int base_luminance_work = Render_paletteTransitionState.backgroundBaseColors[i].red;
            blue_target_or_green_fixed = base_green_work << 0x10;
            base_blue_work <<= 0x10;
            base_luminance_work <<= 0x11;
            base_green_work <<= 0x11;
            base_luminance_work += base_green_work + blue_target_or_green_fixed;
            base_luminance_work += base_blue_work;
            red_target_fixed = base_luminance_work / 6 + (red_offset << 0x10);
            green_target_fixed = base_luminance_work / 6 + (green_offset << 0x10);
            blue_target_or_green_fixed = base_luminance_work / 6 + (blue_offset << 0x10);
        } break;
        case 7: {
            int base_green_work = Render_paletteTransitionState.backgroundBaseColors[i].green;
            int base_blue_work = Render_paletteTransitionState.backgroundBaseColors[i].blue;
            int base_luminance_work = Render_paletteTransitionState.backgroundBaseColors[i].red;
            blue_target_or_green_fixed = base_green_work << 0x10;
            base_blue_work <<= 0x10;
            base_luminance_work <<= 0x11;
            base_green_work <<= 0x11;
            base_luminance_work += base_green_work + blue_target_or_green_fixed;
            base_luminance_work += base_blue_work;
            red_target_fixed = base_luminance_work / 12 + (red_offset << 0x10);
            green_target_fixed = base_luminance_work / 12 + (green_offset << 0x10);
            blue_target_or_green_fixed = base_luminance_work / 12 + (blue_offset << 0x10);
        } break;
        case 8:
            red_target_fixed = Render_paletteTransitionState.backgroundBaseColors[i].red << 0x10;
            green_target_fixed = Render_paletteTransitionState.backgroundBaseColors[i].green << 0x10;
            blue_target_or_green_fixed = Render_paletteTransitionState.backgroundBaseColors[i].blue << 0x10;
            break;
        case 11:
            Render_paletteTransitionState.backgroundFadeMode = 0;
            return;
        case 0:
        default:
            red_target_fixed = p[i][0] + (red_offset << 0x10);
            green_target_fixed = p[i][1] + (green_offset << 0x10);
            blue_target_or_green_fixed = p[i][2] + (blue_offset << 0x10);
            break;
        }

        red_target_fixed &= 0xFFFF0000;
        green_target_fixed &= 0xFFFF0000;
        blue_target_or_green_fixed &= 0xFFFF0000;

        if (red_target_fixed > 0xFF0000) {
            red_target_fixed = 0xFF0000;
        }
        if (green_target_fixed > 0xFF0000) {
            green_target_fixed = 0xFF0000;
        }
        if (blue_target_or_green_fixed > 0xFF0000) {
            blue_target_or_green_fixed = 0xFF0000;
        }
        if (red_target_fixed < 0) {
            red_target_fixed = 0;
        }
        if (green_target_fixed < 0) {
            green_target_fixed = 0;
        }
        if (blue_target_or_green_fixed < 0) {
            blue_target_or_green_fixed = 0;
        }
        if (duration != 0) {
            p[i][3] = (int)(red_target_fixed - (p[i][0] & 0xFFFF0000)) / (duration * 2);
            p[i][4] = (int)(green_target_fixed - (p[i][1] & 0xFFFF0000)) / (duration * 2);
            p[i][5] = (int)(blue_target_or_green_fixed - (p[i][2] & 0xFFFF0000)) / (duration * 2);
        } else {
            p[i][0] = red_target_fixed;
            p[i][1] = green_target_fixed;
            p[i][2] = blue_target_or_green_fixed;
        }
    }

    if (duration != 0) {
        Render_paletteTransitionState.backgroundFadeActive = 1;
        Render_paletteTransitionState.backgroundFadeFrame = 0;
        Render_paletteTransitionState.backgroundFadeTick = 0;
        Render_paletteTransitionState.backgroundFadeSpeed = duration;
        Render_paletteTransitionState.backgroundFadeMode = mode;
        if (mode == 9) {
            Render_paletteTransitionState.backgroundPulseRed = red_offset;
            Render_paletteTransitionState.backgroundPulseGreen = green_offset;
            Render_paletteTransitionState.backgroundPulseBlue = blue_offset;
        }
    } else {
        Render_paletteTransitionState.backgroundFadeActive = 0;
        for (i = 0; i < 5; ++i) {
            int red_byte, rounded_green_fixed, packed_red_green;
            luminance_or_channel_fixed = p[i][0];

            if (luminance_or_channel_fixed < 0) {
                luminance_or_channel_fixed += 0xFFFF;
            }

            red_byte = (luminance_or_channel_fixed >> 0x10);
            rounded_green_fixed = p[i][1];

            if (rounded_green_fixed < 0) {
                rounded_green_fixed += 0xFFFF;
            }

            packed_red_green = red_byte + ((rounded_green_fixed >> 0x10) << 8);
            luminance_or_channel_fixed = p[i][2];

            if (luminance_or_channel_fixed < 0) {
                luminance_or_channel_fixed += 0xFFFF;
            }
            packed_colors[i] = packed_red_green + ((luminance_or_channel_fixed >> 0x10) << 0x10);
        }
        Field_SetRoomBackgroundColors(packed_colors);
    }
}

// decomp: SLUS_010.40 func_800478E0 (src/SLUS_010.40/main.c:10990)
void Render_RequestBackgroundGradientTransform(int mode, int duration, int red_offset, int green_offset, int blue_offset)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_TransformBackgroundGradient(mode, duration, red_offset, green_offset, blue_offset);
    }
}

// decomp: SLUS_010.40 func_80047910 (src/SLUS_010.40/main.c:10997)
void Render_SetModelTint(int duration, int tint_slot, Render_ColorRgb8* color)
{
    Render_ColorCode tint_code;
    Render_ModelColorFade* fade_state;
    int red_fixed;
    int green_fixed;
    int blue_fixed;
    int v0;
    int* color_components_fixed;

    if (Render_paletteTransitionState.enabled == 0) {
        return;
    }

    fade_state = &Render_paletteTransitionState.modelColorFades[tint_slot];
    color_components_fixed = Render_paletteTransitionState.modelColorFades[tint_slot].colorFixed;
    red_fixed = Render_ToFixed16(color->red);
    green_fixed = Render_ToFixed16(color->green);
    blue_fixed = Render_ToFixed16(color->blue);

    if (duration != 0) {
        color_components_fixed[3] = ((int)(red_fixed - (color_components_fixed[0] & 0xFFFF0000)) / (duration * 2));
        color_components_fixed[4] = ((int)(green_fixed - (color_components_fixed[1] & 0xFFFF0000)) / (duration * 2));
        color_components_fixed[5] = ((int)(blue_fixed - (color_components_fixed[2] & 0xFFFF0000)) / (duration * 2));
    } else {
        color_components_fixed[0] = red_fixed;
        color_components_fixed[1] = green_fixed;
        color_components_fixed[2] = blue_fixed;
    }
    if (duration != 0) {
        fade_state->progress[0] = 1;
        fade_state->progress[1] = 0;
        fade_state->progress[2] = 0;
        fade_state->progress[3] = duration;
        return;
    }

    fade_state->progress[0] = 0;
    tint_code.red = (color_components_fixed[0] / 65536) - 64;
    tint_code.green = (color_components_fixed[1] / 65536) - 64;
    tint_code.blue = (color_components_fixed[2] / 65536) - 64;
    tint_code.code = 0;

    if (tint_slot != 0) {
        Actors_SetActorFadeParameters(tint_slot - 1, &tint_code);
    } else {
        Field_SetRoomTint(&tint_code, fade_state->blendParams[3] >> 2);
    }
}

// decomp: SLUS_010.40 func_80047AB4 (src/SLUS_010.40/main.c:11047)
void Render_SetModelTintBase(int duration, int tint_slot, Render_ColorRgb8* color)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_SetModelTint(duration, tint_slot, color);
        Render_paletteTransitionState.modelColorFades[tint_slot].baseColor = *color;
    }
}

// decomp: SLUS_010.40 func_80047B30 (src/SLUS_010.40/main.c:11055)
void Render_TransformModelTint(int mode, int duration, int tint_slot, int red_offset, int green_offset, int blue_offset)
{
    Render_ColorCode tint_code;
    short blend_target;
    int red_target_fixed;
    int blue_target_or_green_fixed;
    int green_target_fixed;
    int channel_value;
    int luminance_or_rounding_fixed;
    Render_ModelColorFade* fade_state = &Render_paletteTransitionState.modelColorFades[tint_slot];
    int* color_components_fixed = fade_state->colorFixed;

    blend_target = 0;
    switch (mode) {
    case 1:
        red_target_fixed = (color_components_fixed[0] / 2) + (red_offset << 16);
        green_target_fixed = (color_components_fixed[1] / 2) + (green_offset << 16);
        blue_target_or_green_fixed = (color_components_fixed[2] / 2) + (blue_offset << 16);
        break;
    case 2:
        blend_target = 0x4000;
        red_target_fixed = ((color_components_fixed[0] * 2) + (color_components_fixed[1] * 3) + color_components_fixed[2]) / 6 + (red_offset << 0x10);
        green_target_fixed = ((color_components_fixed[0] * 2) + (color_components_fixed[1] * 3) + color_components_fixed[2]) / 6 + (green_offset << 0x10);
        blue_target_or_green_fixed = ((color_components_fixed[0] * 2) + (color_components_fixed[1] * 3) + color_components_fixed[2]) / 6 + (blue_offset << 0x10);
        break;
    case 3:
        blend_target = 0x4000;
        luminance_or_rounding_fixed = ((color_components_fixed[0] * 2) + (color_components_fixed[1] * 3) + color_components_fixed[2]) / 12;
        red_target_fixed = luminance_or_rounding_fixed + (red_offset << 0x10);
        green_target_fixed = luminance_or_rounding_fixed + (green_offset << 0x10);
        blue_target_or_green_fixed = luminance_or_rounding_fixed + (blue_offset << 0x10);
        break;
    case 4:
    case 9:
        red_target_fixed = (fade_state->baseColor.red + red_offset) << 0x10;
        green_target_fixed = (fade_state->baseColor.green + green_offset) << 0x10;
        blue_target_or_green_fixed = (fade_state->baseColor.blue + blue_offset) << 0x10;
        break;
    case 5:
        blend_target = 0x4000;
        red_target_fixed = (fade_state->baseColor.red << 0xF) + (red_offset << 0x10);
        green_target_fixed = (fade_state->baseColor.green << 0xF) + (green_offset << 0x10);
        blue_target_or_green_fixed = (fade_state->baseColor.blue << 0xF) + (blue_offset << 0x10);
        break;
    case 6:
        blend_target = 0x4000;
        {
            int base_green_work = fade_state->baseColor.green;
            int base_blue_work = fade_state->baseColor.blue;
            int base_luminance_work = fade_state->baseColor.red;
            blue_target_or_green_fixed = base_green_work << 0x10;
            base_blue_work <<= 0x10;
            base_luminance_work <<= 0x11;
            base_green_work <<= 0x11;
            base_luminance_work += base_green_work + blue_target_or_green_fixed;
            base_luminance_work += base_blue_work;
            red_target_fixed = base_luminance_work / 6 + (red_offset << 0x10);
            green_target_fixed = base_luminance_work / 6 + (green_offset << 0x10);
            blue_target_or_green_fixed = base_luminance_work / 6 + (blue_offset << 0x10);
        }
        break;
    case 7:
        blend_target = 0x4000;
        {
            int base_green_work = fade_state->baseColor.green;
            int base_blue_work = fade_state->baseColor.blue;
            int base_luminance_work = fade_state->baseColor.red;
            blue_target_or_green_fixed = base_green_work << 0x10;
            base_blue_work <<= 0x10;
            base_luminance_work <<= 0x11;
            base_green_work <<= 0x11;
            base_luminance_work += base_green_work + blue_target_or_green_fixed;
            base_luminance_work += base_blue_work;
            red_target_fixed = base_luminance_work / 12 + (red_offset << 0x10);
            green_target_fixed = base_luminance_work / 12 + (green_offset << 0x10);
            blue_target_or_green_fixed = base_luminance_work / 12 + (blue_offset << 0x10);
        }
        break;
    case 8:
    case 15:
        red_target_fixed = fade_state->baseColor.red << 0x10;
        green_target_fixed = fade_state->baseColor.green << 0x10;
        blue_target_or_green_fixed = fade_state->baseColor.blue << 0x10;
        break;
    case 11:
        fade_state->progress[4] = 0;
        return;
    default:
        red_target_fixed = color_components_fixed[0] + (red_offset << 0x10);
        green_target_fixed = color_components_fixed[1] + (green_offset << 0x10);
        blue_target_or_green_fixed = color_components_fixed[2] + (blue_offset << 0x10);
        break;
    }
    red_target_fixed = red_target_fixed & 0xFFFF0000;
    green_target_fixed = green_target_fixed & 0xFFFF0000;
    blue_target_or_green_fixed = blue_target_or_green_fixed & 0xFFFF0000;

    if (red_target_fixed > 0x800000) {
        red_target_fixed = 0x800000;
    }
    if (green_target_fixed > 0x800000) {
        green_target_fixed = 0x800000;
    }
    if (blue_target_or_green_fixed > 0x800000) {
        blue_target_or_green_fixed = 0x800000;
    }
    if (red_target_fixed < 0) {
        red_target_fixed = 0;
    }
    if (green_target_fixed < 0) {
        green_target_fixed = 0;
    }
    if (blue_target_or_green_fixed < 0) {
        blue_target_or_green_fixed = 0;
    }
    if (duration != 0) {
        color_components_fixed[3] = ((int)(red_target_fixed - (color_components_fixed[0] & 0xFFFF0000)) / (duration * 2));
        color_components_fixed[4] = ((int)(green_target_fixed - (color_components_fixed[1] & 0xFFFF0000)) / (duration * 2));
        color_components_fixed[5] = ((int)(blue_target_or_green_fixed - (color_components_fixed[2] & 0xFFFF0000)) / (duration * 2));
        fade_state->blendParams[4] = ((blend_target - fade_state->blendParams[3]) / (duration * 2));
    } else {
        color_components_fixed[0] = red_target_fixed;
        color_components_fixed[1] = green_target_fixed;
        color_components_fixed[2] = blue_target_or_green_fixed;
        fade_state->blendParams[3] = blend_target;
    }
    channel_value = 1;
    if (duration != 0) {
        fade_state->progress[0] = 1;
        fade_state->progress[1] = 0;
        fade_state->progress[2] = 0;
        fade_state->progress[3] = duration;
        fade_state->progress[4] = mode;
        if (mode == 9) {
            fade_state->blendParams[0] = red_offset;
            fade_state->blendParams[1] = green_offset;
            fade_state->blendParams[2] = blue_offset;
        }
    } else {
        fade_state->progress[0] = 0;
        channel_value = color_components_fixed[0] >> 0x10;
        if (color_components_fixed[0] < 0) {
            luminance_or_rounding_fixed = color_components_fixed[0] + 0xFFFF;
            channel_value = luminance_or_rounding_fixed >> 0x10;
        }
        tint_code.red = channel_value - 0x40;
        channel_value = color_components_fixed[1] >> 0x10;
        if (color_components_fixed[1] < 0) {
            luminance_or_rounding_fixed = color_components_fixed[1] + 0xFFFF;
            channel_value = luminance_or_rounding_fixed >> 0x10;
        }
        tint_code.green = channel_value - 0x40;
        channel_value = color_components_fixed[2] >> 0x10;
        if (color_components_fixed[2] < 0) {
            luminance_or_rounding_fixed = color_components_fixed[2] + 0xFFFF;
            channel_value = luminance_or_rounding_fixed >> 0x10;
        }
        tint_code.blue = channel_value - 0x40;
        tint_code.code = 0;
        if (tint_slot != 0) {
            Actors_SetActorFadeParameters(tint_slot - 1, &tint_code);
        } else {
            Field_SetRoomTint(&tint_code, ((u_short)fade_state->blendParams[3] << 0x10) >> 0x12);
        }
    }
}

// decomp: SLUS_010.40 func_80047FC0 (src/SLUS_010.40/main.c:11222)
void Render_RequestModelTintTransform(int mode, int duration, int tint_slot, int red_offset, int green_offset, int blue_offset)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_TransformModelTint(mode, duration, tint_slot, red_offset, green_offset, blue_offset);
    }
}

// decomp: SLUS_010.40 func_80047FFC (src/SLUS_010.40/main.c:11229)
void Render_UpdateBackgroundGradientTransition(void)
{
    int packed_colors[5];
    int i;
    int rounded_green_fixed;
    int packed_red_green;
    int red_byte;
    int rounded_red_or_blue_fixed;
    int (*color_components_fixed)[6];

    if (Render_paletteTransitionState.backgroundFadeActive == 0) {
        return;
    }

    color_components_fixed = Render_paletteTransitionState.backgroundColorsFixed;

    for (i = 0; i < 5; ++i) {
        color_components_fixed[i][0] += color_components_fixed[i][3];
        color_components_fixed[i][1] += color_components_fixed[i][4];
        color_components_fixed[i][2] += color_components_fixed[i][5];
    }

    for (i = 0; i < 5; ++i) {
        rounded_red_or_blue_fixed = color_components_fixed[i][0];
        if (rounded_red_or_blue_fixed < 0) {
            rounded_red_or_blue_fixed += 0xFFFF;
        }
        rounded_green_fixed = color_components_fixed[i][1];
        red_byte = rounded_red_or_blue_fixed >> 16;
        if (rounded_green_fixed < 0) {
            rounded_green_fixed += 0xFFFF;
        }
        rounded_red_or_blue_fixed = color_components_fixed[i][2];
        packed_red_green = red_byte + ((rounded_green_fixed >> 16) << 8);
        if (rounded_red_or_blue_fixed < 0) {
            rounded_red_or_blue_fixed += 0xFFFF;
        }
        packed_colors[i] = packed_red_green + ((rounded_red_or_blue_fixed >> 16) << 16);
    }
    Field_SetRoomBackgroundColors(packed_colors);
    Render_paletteTransitionState.backgroundFadeFrame += 1;
    if (Render_paletteTransitionState.backgroundFadeFrame >= (Render_paletteTransitionState.backgroundFadeSpeed * 2)) {
        if (Render_paletteTransitionState.backgroundFadeMode == 9) {
            Render_TransformBackgroundGradient(4, Render_paletteTransitionState.backgroundFadeSpeed, Render_paletteTransitionState.backgroundPulseRed / 2,
                Render_paletteTransitionState.backgroundPulseGreen / 2, Render_paletteTransitionState.backgroundPulseBlue / 2);
            Render_paletteTransitionState.backgroundFadeMode = 10;
            return;
        }
        if (Render_paletteTransitionState.backgroundFadeMode == 10) {
            Render_TransformBackgroundGradient(9, Render_paletteTransitionState.backgroundFadeSpeed, Render_paletteTransitionState.backgroundPulseRed,
                Render_paletteTransitionState.backgroundPulseGreen, Render_paletteTransitionState.backgroundPulseBlue);
            Render_paletteTransitionState.backgroundFadeMode = 9;
            return;
        }
        Render_paletteTransitionState.backgroundFadeActive = 0;
    }
}

// decomp: SLUS_010.40 func_800481C0 (src/SLUS_010.40/main.c:11287)
void Render_UpdateModelColorTransitions(void)
{
    Render_ColorCode tint_code;
    int i;
    int rounded_channel_fixed;
    Render_ModelColorFade* p = Render_paletteTransitionState.modelColorFades;

    for (i = 0; i < 17; ++i) {
        int* color_components_fixed = p[i].colorFixed;
        if (p[i].progress[0] != 0) {
            color_components_fixed[0] += color_components_fixed[3];
            color_components_fixed[1] += color_components_fixed[4];
            color_components_fixed[2] += color_components_fixed[5];

            rounded_channel_fixed = color_components_fixed[0];
            if (rounded_channel_fixed < 0) {
                rounded_channel_fixed += 0xFFFF;
            }
            tint_code.red = (rounded_channel_fixed >> 16) - 0x40;
            rounded_channel_fixed = color_components_fixed[1];
            if (rounded_channel_fixed < 0) {
                rounded_channel_fixed += 0xFFFF;
            }
            tint_code.green = (rounded_channel_fixed >> 16) - 0x40;
            rounded_channel_fixed = color_components_fixed[2];
            if (rounded_channel_fixed < 0) {
                rounded_channel_fixed += 0xFFFF;
            }
            tint_code.blue = (rounded_channel_fixed >> 16) - 0x40;
            tint_code.code = 0;

            if (i != 0) {
                Actors_SetActorFadeParameters(i - 1, &tint_code);
            } else {
                p[0].blendParams[3] += p[0].blendParams[4];
                Field_SetRoomTint(&tint_code, p[0].blendParams[3] >> 2);
            }
            ++p[i].progress[1];
            if (p[i].progress[1] >= (p[i].progress[3] * 2)) {
                if (p[i].progress[4] == 9) {
                    Render_RequestModelTintTransform(4, p[i].progress[3], i, p[i].blendParams[0] / 2, p[i].blendParams[1] / 2,
                        p[i].blendParams[2] / 2);
                    p[i].progress[4] = 10;
                } else if (p[i].progress[4] == 10) {
                    Render_TransformModelTint(
                        9, p[i].progress[3], i, p[i].blendParams[0], p[i].blendParams[1], p[i].blendParams[2]);
                    p[i].progress[4] = 9;
                } else if (p[i].progress[4] == 14) {
                    Render_TransformModelTint(15, p[i].progress[3], i, 0, 0, 0);
                    p[i].progress[4] = 15;
                } else {
                    p[i].progress[0] = 0;
                }
            }
        }
    }
}

// decomp: SLUS_010.40 func_800483FC (src/SLUS_010.40/main.c:11345)
void Render_UpdateClutTransitions(void)
{
    int i;
    int j;
    u_int frame_divisor;
    u_short g;
    u_short r;
    u_short b;
    struct Render_PaletteEntryFade* index;
    int c;

    for (i = 0; i < 14; ++i) {
        if (Render_paletteTransitionState.slots[i].transition.active == 0) {
            continue;
        }
        if (++Render_paletteTransitionState.slots[i].transition.tick
            >= (Render_paletteTransitionState.slots[i].transition.speed / 8)) {

            Render_paletteTransitionState.slots[i].transition.tick = 0;

            if (Render_paletteTransitionState.slots[i].transition.speed >= 8) {
                frame_divisor = 1;
                c = Render_paletteTransitionState.slots[i].transition.frame;
                index = Render_paletteTransitionState.slots[i].entries;
                for (j = 0; j < 256; ++j) {
                    if ((index[j].red + index[j].green + index[j].blue) == 0) {
                        continue;
                    }
                    r = index[j].red + Render_paletteDeltaSequenceSixteen[index[j].redStepRow][c];
                    g = index[j].green + Render_paletteDeltaSequenceSixteen[index[j].greenStepRow][c];
                    b = index[j].blue + Render_paletteDeltaSequenceSixteen[index[j].blueStepRow][c];
                    if (((b | (r | g)) << 16) == 0) {
                        b = 1;
                    }
                    index[j].green = g;
                    index[j].blue = b;
                    index[j].red = r;
                    Render_paletteColors[i][j] = r + (g << 5) + (b << 10) + (index[j].maskBit << 15);
                }
            } else if (Render_paletteTransitionState.slots[i].transition.speed >= 4) {
                frame_divisor = 2;
                c = Render_paletteTransitionState.slots[i].transition.frame;
                index = Render_paletteTransitionState.slots[i].entries;
                for (j = 0; j < 256; ++j) {
                    if ((index[j].red + index[j].green + index[j].blue) == 0) {
                        continue;
                    }
                    r = index[j].red + Render_paletteDeltaSequenceEight[index[j].redStepRow][c];
                    g = index[j].green + Render_paletteDeltaSequenceEight[index[j].greenStepRow][c];
                    b = index[j].blue + Render_paletteDeltaSequenceEight[index[j].blueStepRow][c];
                    if (((b | (r | g)) << 16) == 0) {
                        b = 1;
                    }
                    index[j].red = r;
                    index[j].green = g;
                    index[j].blue = b;
                    Render_paletteColors[i][j] = r + (g << 5) + (b << 10) + (index[j].maskBit << 15);
                }
            } else if (Render_paletteTransitionState.slots[i].transition.speed >= 2) {
                frame_divisor = 4;
                c = Render_paletteTransitionState.slots[i].transition.frame;
                index = Render_paletteTransitionState.slots[i].entries;
                for (j = 0; j < 256; ++j) {
                    if ((index[j].red + index[j].green + index[j].blue) == 0) {
                        continue;
                    }
                    r = index[j].red + Render_paletteDeltaSequenceFour[index[j].redStepRow][c];
                    g = index[j].green + Render_paletteDeltaSequenceFour[index[j].greenStepRow][c];
                    b = index[j].blue + Render_paletteDeltaSequenceFour[index[j].blueStepRow][c];
                    if (((b | (r | g)) << 16) == 0) {
                        b = 1;
                    }
                    index[j].red = r;
                    index[j].green = g;
                    index[j].blue = b;
                    Render_paletteColors[i][j] = r + (g << 5) + (b << 10) + (index[j].maskBit << 15);
                }
            } else {
                frame_divisor = 8;
                c = Render_paletteTransitionState.slots[i].transition.frame;
                index = Render_paletteTransitionState.slots[i].entries;
                for (j = 0; j < 256; ++j) {
                    if ((index[j].red + index[j].green + index[j].blue) == 0) {
                        continue;
                    }
                    r = index[j].red + Render_paletteDeltaSequenceTwo[index[j].redStepRow][c];
                    g = index[j].green + Render_paletteDeltaSequenceTwo[index[j].greenStepRow][c];
                    b = index[j].blue + Render_paletteDeltaSequenceTwo[index[j].blueStepRow][c];
                    if (((b | (r | g)) << 16) == 0) {
                        b = 1;
                    }
                    index[j].red = r;
                    index[j].green = g;
                    index[j].blue = b;
                    Render_paletteColors[i][j] = r + (g << 5) + (b << 10) + (index[j].maskBit << 15);
                }
            }
            if (++Render_paletteTransitionState.slots[i].transition.frame >= (16 / frame_divisor)) {
                if (Render_paletteTransitionState.slots[i].transition.mode == 9) {
                    Render_ApplyClutSlotTransform(4, Render_paletteTransitionState.slots[i].transition.speed, i,
                        Render_paletteTransitionState.slots[i].transition.pulseRed / 2,
                        Render_paletteTransitionState.slots[i].transition.pulseGreen / 2,
                        Render_paletteTransitionState.slots[i].transition.pulseBlue / 2);
                    Render_paletteTransitionState.slots[i].transition.mode = 10;
                } else if (Render_paletteTransitionState.slots[i].transition.mode == 10) {
                    Render_ApplyClutSlotTransform(9, Render_paletteTransitionState.slots[i].transition.speed, i,
                        Render_paletteTransitionState.slots[i].transition.pulseRed,
                        Render_paletteTransitionState.slots[i].transition.pulseGreen,
                        Render_paletteTransitionState.slots[i].transition.pulseBlue);
                    Render_paletteTransitionState.slots[i].transition.mode = 9;
                } else if (Render_paletteTransitionState.slots[i].transition.mode == 14) {
                    Render_ApplyClutSlotTransform(
                        15, Render_paletteTransitionState.slots[i].transition.speed, i, 0, 0, 0);
                    Render_paletteTransitionState.slots[i].transition.mode = 15;
                } else {
                    Render_paletteTransitionState.slots[i].transition.active = 0;
                }
            }
        }
        Render_paletteTransitionState.uploadPending = 1;
    }
}

// decomp: SLUS_010.40 func_80048A3C (src/SLUS_010.40/main.c:11468)
void Render_StopClutTransition(int slot) { Render_paletteTransitionState.slots[slot].transition.active = 0; }

// decomp: SLUS_010.40 vs_main_loadClut (src/SLUS_010.40/main.c:11470)
void Render_LoadClutColors(u_short const* img, u_int slot, u_int colorOffset, u_int count)
{
    RECT rect;
    u_short px;
    u_int i;
    struct Render_PaletteEntryFade* dst;

    if (slot >= 14) {
        setRECT(&rect, colorOffset + 768, slot + 224, count, 1);
        LoadImage(&rect, (u_long*)img);
        return;
    }

    if (Render_paletteTransitionState.slots[slot].locked == 0) {
        Render_paletteTransitionState.slots[slot].transition.active = 0;
        for (i = 0; i < count; ++i) {
            px = img[i];
            dst = &Render_paletteTransitionState.slots[slot].entries[colorOffset + i];
            Render_paletteColors[slot][colorOffset + i] = px;
            Render_paletteTransitionState.slots[slot].baseColors[colorOffset + i] = px;
            dst->red = (px & 0x1F);
            dst->green = ((px & 0x3E0) >> 5);
            dst->blue = ((px & 0x7C00) >> 10);
            dst->maskBit = ((px & 0x8000) >> 15);
        }
        Render_paletteTransitionState.uploadPending = 1;
    }
}

// decomp: SLUS_010.40 func_80048B8C (src/SLUS_010.40/main.c:11499)
void Render_TransformClutBase(int mode, u_short* srcClut, int slot, short rOffset, short gOffset,
    short bOffset, int update_current_colors)
{
    int i;
    int source_red;
    int source_green;
    int source_blue;
    short r;
    short g;
    short b;
    short value;
    u_short stp;

    for (i = 0; i < 256; ++i) {
        value = srcClut[i];
        if (value != 0) {
            switch (mode) {
            case 5:
                r = rOffset + (value & 0x1F) / 2;
                g = gOffset + ((value & 0x3E0) >> 5) / 2;
                b = bOffset + ((value & 0x7C00) >> 10) / 2;
                break;

            case 6:
                source_red = value & 0x1F;
                source_green = (value & 0x3E0) >> 5;
                source_blue = (value & 0x7C00) >> 10;
                r = rOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 6;
                g = gOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 6;
                b = bOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 6;
                break;

            case 7:
                source_red = value & 0x1F;
                source_green = (value & 0x3E0) >> 5;
                source_blue = (value & 0x7C00) >> 10;
                r = rOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 12;
                g = gOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 12;
                b = bOffset + ((source_red * 2) + (source_green * 3) + source_blue) / 12;
                break;

            case 0:
            default:
                r = rOffset + (value & 0x1F);
                g = gOffset + ((value & 0x3E0) >> 5);
                b = bOffset + ((value & 0x7C00) >> 10);
                break;
            }

        } else {
            r = value & 0x1F;
            g = (value & 0x3E0) >> 5;
            b = (value & 0x7C00) >> 10;
        }

        stp = value;
        stp >>= 15;

        if (r >= 32) {
            r = 31;
        }
        if (g >= 32) {
            g = 31;
        }
        if (b >= 32) {
            b = 31;
        }
        if ((r << 16) < 1) {
            r = 0;
        }
        if ((g << 16) <= 0) {
            g = 0;
        }
        if ((b << 16) <= 0) {
            b = 0;
        }

        if ((((r | g | b) << 16) == 0) && (value & 0x7FFF)) {
            b = 1;
        }

        if (update_current_colors != 0) {
            Render_paletteColors[slot][i] = r + (g << 5) + (b << 10) + (stp << 15);

            if (Render_paletteTransitionState.slots[slot].transition.active == 0) {
                struct Render_PaletteEntryFade* colors = &Render_paletteTransitionState.slots[slot].entries[i];
                colors->red = r;
                colors->green = g;
                colors->blue = b;
                colors->maskBit = stp;
            }
        }

        Render_paletteTransitionState.slots[slot].baseColors[i] = r + (g << 5) + (b << 10) + (stp << 15);
    }

    Render_paletteTransitionState.uploadPending = 1;
}

// decomp: SLUS_010.40 func_80048E68 (src/SLUS_010.40/main.c:11598)
void Render_OffsetClutBase(u_short* srcClut, int slot, u_short rOffset, u_short gOffset,
    u_short bOffset, int update_current_colors)
{
    Render_TransformClutBase(8, srcClut, slot, rOffset, gOffset, bOffset, update_current_colors);
}

// decomp: SLUS_010.40 func_80048EC4 (src/SLUS_010.40/main.c:11604)
void Render_CaptureRoomBackgroundColors(void)
{
    int i;
    int (*bgColors)[6] = Render_paletteTransitionState.backgroundColorsFixed;
    int* packed_room_colors = Field_RoomBackgroundColors();

    for (i = 0; i < 5; ++i) {
        Render_paletteTransitionState.backgroundBaseColors[i].red = packed_room_colors[i];
        Render_paletteTransitionState.backgroundBaseColors[i].green = (packed_room_colors[i] & 0xFF00) / 256;
        Render_paletteTransitionState.backgroundBaseColors[i].blue = (packed_room_colors[i] & 0xFF0000) / 65536;

        bgColors[i][0] = (packed_room_colors[i] & 0xFF) << 16;
        bgColors[i][1] = (packed_room_colors[i] & 0xFF00) << 8;
        bgColors[i][2] = packed_room_colors[i] & 0xFF0000;
    }
}

// decomp: SLUS_010.40 vs_main_commitClut (src/SLUS_010.40/main.c:11621)
void Render_CommitClutBuffer(void)
{
    RECT rect;

    if (Render_paletteTransitionState.uploadPending != 0) {
        setRECT(&rect, 768, 224, 256, 14);
        LoadImage(&rect, (u_long*)Render_paletteColors[0]);
        Render_paletteTransitionState.uploadPending = 0;
    }
}

// decomp: SLUS_010.40 vs_main_setClutState (src/SLUS_010.40/main.c:11632)
void Render_SetClutAnimationEnabled(int active) { Render_paletteTransitionState.enabled = active; }

// decomp: SLUS_010.40 vs_main_clutInit (src/SLUS_010.40/main.c:11634)
void Render_InitializeClutAnimation(void)
{
    int i;

    Render_paletteTransitionState.uploadPending = 0;
    Render_paletteTransitionState.enabled = 1;

    for (i = 0; i < 14; ++i) {
        Render_paletteTransitionState.slots[i].locked = 0;
        Render_paletteTransitionState.slots[i].transition.active = 0;
        Render_paletteTransitionState.slots[i].transition.mode = 0;
    }

    Render_paletteTransitionState.backgroundFadeActive = 0;
    Render_paletteTransitionState.backgroundFadeMode = 0;

    for (i = 0; i < 17; ++i) {
        Render_paletteTransitionState.modelColorFades[i].progress[0] = 0;
        Render_paletteTransitionState.modelColorFades[i].progress[4] = 0;
        Render_paletteTransitionState.modelColorFades[i].blendParams[3] = 0;
        *((int*)&Render_paletteTransitionState.modelColorFades[i].baseColor) = 0x404040;
        Render_paletteTransitionState.modelColorFades[i].colorFixed[2] = 0x400000;
        Render_paletteTransitionState.modelColorFades[i].colorFixed[1] = 0x400000;
        Render_paletteTransitionState.modelColorFades[i].colorFixed[0] = 0x400000;
    }
}

