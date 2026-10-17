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
void Render_TransformRoomPalettes(int arg0, int arg1, int arg2, short arg3, short arg4)
{
    Render_TransformClutSlot(arg0, arg1, 0, arg2, arg3, arg4);
    Render_TransformClutSlot(arg0, arg1, 1, arg2, arg3, arg4);
    Render_TransformClutSlot(arg0, arg1, 2, arg2, arg3, arg4);
}

// decomp: BATTLE.PRG func_8007B1B8 (src/BATTLE/BATTLE.PRG/146C.c:8248)
void Render_TransformSceneColors(int arg0, int arg1, short arg2, short arg3, short arg4)
{
    Render_TransformClutSlot(arg0, arg1, 0, arg2, arg3, arg4);
    Render_TransformClutSlot(arg0, arg1, 1, arg2, arg3, arg4);
    Render_TransformClutSlot(arg0, arg1, 2, arg2, arg3, arg4);
    Render_RequestModelTintTransform(arg0, arg1, 0, arg2, arg3, arg4);
    Render_TransformClutSlot(arg0, arg1, 0xD, arg2, arg3, arg4);
}

// decomp: BATTLE.PRG func_8007B29C (src/BATTLE/BATTLE.PRG/146C.c:8257)
void Render_TransformActorColors(int arg0, int arg1, int arg2, short arg3, short arg4, short arg5)
{
    if (arg2 < 9) {
        Render_TransformClutSlot(arg0, arg1, arg2 + 4, arg3, arg4, arg5);
        Render_RequestModelTintTransform(arg0, arg1, arg2 + 1, arg3, arg4, arg5);
    }
}

// decomp: BATTLE.PRG func_8007B344 (src/BATTLE/BATTLE.PRG/146C.c:8265)
void Render_TransformBackgroundColors(int arg0, int arg1, short arg2, short arg3, short arg4)
{
    Render_RequestBackgroundGradientTransform(arg0, arg1, arg2, arg3, arg4);
}

// decomp: BATTLE.PRG func_8007B378 (src/BATTLE/BATTLE.PRG/146C.c:8270)
void Render_LoadRoomPaletteTransition(int arg0, u_short* arg1, int arg2, int arg3)
{
    ((void (*)())Render_StartClutTransition)(arg0, 0, arg1, arg3);
    // BUG: Missing last parameter to vs_main_loadClutTransition
    ((void (*)())Render_StartClutTransition)(arg0, 1, arg1 + 0x100);
    ((void (*)())Render_StartClutTransition)(arg0, 2, arg1 + 0x200);
}

// decomp: BATTLE.PRG func_8007B3D0 (src/BATTLE/BATTLE.PRG/146C.c:8280)
void Render_StartSharedPaletteTransition(int arg0, int arg1, u_short* arg2, int arg3)
{
    ((void (*)())Render_StartClutTransition)(arg0, arg1 + 4, arg2, arg3);
}

// decomp: BATTLE.PRG func_8007B3F0 (src/BATTLE/BATTLE.PRG/146C.c:8285)
void Render_ApplyBackgroundGradientColors(int arg0, Render_ColorRgb8* arg1) { Render_SetBackgroundGradientColors(arg0, arg1); }

// decomp: BATTLE.PRG func_800AC0D4 (src/BATTLE/BATTLE.PRG/40564.c:2003)
int Render_StepRgbTowardTarget(u_char* arg0, u_char* arg1, int arg2)
{
    int i;
    int var_t0 = 0;

    for (i = 0; i < 3; ++i) {
        if (arg2 < 0) {
            if (*arg1 < *arg0) {
                var_t0 = 1;
                *arg0 += arg2;
                if (*arg0 < *arg1) {
                    *arg0 = *arg1;
                }
            }
        } else {
            if (*arg0 < *arg1) {
                var_t0 = 1;
                *arg0 += arg2;
                if (*arg1 < *arg0) {
                    *arg0 = *arg1;
                }
            }
        }
        ++arg0;
        ++arg1;
    }

    return var_t0;
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
int Render_ToFixed16(int arg0) { return arg0 << 0x10; }

// decomp: SLUS_010.40 func_800472D0 (src/SLUS_010.40/main.c:10780)
void Render_SetBackgroundGradientColors(int arg0, Render_ColorRgb8* arg1)
{
    int sp10[5];
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        int (*temp_t6)[6] = Render_paletteTransitionState.backgroundColorsFixed;
        for (i = 0; i < 5; ++i) {
            int r;
            int g;
            int b;
            r = Render_ToFixed16(arg1[i].red);
            g = Render_ToFixed16(arg1[i].green);
            b = Render_ToFixed16(arg1[i].blue);

            if (arg0 != 0) {
                temp_t6[i][3] = ((int)(r - (temp_t6[i][0] & 0xFFFF0000)) / (arg0 * 2));
                temp_t6[i][4] = ((int)(g - (temp_t6[i][1] & 0xFFFF0000)) / (arg0 * 2));
                temp_t6[i][5] = ((int)(b - (temp_t6[i][2] & 0xFFFF0000)) / (arg0 * 2));
            } else {
                temp_t6[i][0] = r;
                temp_t6[i][1] = g;
                temp_t6[i][2] = b;
            }
        }
        if (arg0 != 0) {
            Render_paletteTransitionState.backgroundFadeActive = 1;
            Render_paletteTransitionState.backgroundFadeFrame = 0;
            Render_paletteTransitionState.backgroundFadeTick = 0;
            Render_paletteTransitionState.backgroundFadeSpeed = arg0;
            return;
        }
        Render_paletteTransitionState.backgroundFadeActive = 0;

        for (i = 0; i < 5; ++i) {
            sp10[i] = ((temp_t6[i][0] / 65536) + ((temp_t6[i][1] / 65536) << 8))
                    + (((temp_t6[i][2] / 65536)) << 0x10);
        }
        Field_SetRoomBackgroundColors(sp10);
    }
}

// decomp: SLUS_010.40 func_80047464 (src/SLUS_010.40/main.c:10822)
void Render_SetBackgroundGradientBase(int arg0, Render_ColorRgb8* arg1)
{
    int i;

    if (Render_paletteTransitionState.enabled != 0) {
        Render_SetBackgroundGradientColors(arg0, arg1);
        for (i = 0; i < 5; ++i) {
            Render_paletteTransitionState.backgroundBaseColors[i] = arg1[i];
        }
    }
}

// decomp: SLUS_010.40 func_800474DC (src/SLUS_010.40/main.c:10834)
void Render_TransformBackgroundGradient(int arg0, int arg1, int arg2, int arg3, int arg4)
{
    int sp10[5];
    int temp_v0;
    int var_a1;
    int var_t0;
    int var_t1;
    int i;
    int (*p)[6] = Render_paletteTransitionState.backgroundColorsFixed;

    for (i = 0; i < 5; ++i) {
        switch (arg0) {
        case 1:
            var_t1 = (p[i][0] / 2) + (arg2 << 0x10);
            var_t0 = p[i][1] / 2 + (arg3 << 0x10);
            var_a1 = (p[i][2] / 2) + (arg4 << 0x10);
            break;
        case 2:
            var_t1 = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (arg2 << 0x10);
            var_t0 = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (arg3 << 0x10);
            var_a1 = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 6 + (arg4 << 0x10);
            break;
        case 3:
            temp_v0 = ((p[i][0] * 2) + (p[i][1] * 3) + p[i][2]) / 12;
            var_t1 = temp_v0 + (arg2 << 0x10);
            var_t0 = temp_v0 + (arg3 << 0x10);
            var_a1 = temp_v0 + (arg4 << 0x10);
            break;
        case 4:
        case 9:
            var_t1 = (Render_paletteTransitionState.backgroundBaseColors[i].red + arg2) << 0x10;
            var_t0 = (Render_paletteTransitionState.backgroundBaseColors[i].green + arg3) << 0x10;
            var_a1 = (Render_paletteTransitionState.backgroundBaseColors[i].blue + arg4) << 0x10;
            break;
        case 5:
            var_t1 = (Render_paletteTransitionState.backgroundBaseColors[i].red << 0xF) + (arg2 << 0x10);
            var_t0 = (Render_paletteTransitionState.backgroundBaseColors[i].green << 0xF) + (arg3 << 0x10);
            var_a1 = (Render_paletteTransitionState.backgroundBaseColors[i].blue << 0xF) + (arg4 << 0x10);
            break;
        case 6: {
            int v1 = Render_paletteTransitionState.backgroundBaseColors[i].green;
            int a0 = Render_paletteTransitionState.backgroundBaseColors[i].blue;
            int v0 = Render_paletteTransitionState.backgroundBaseColors[i].red;
            var_a1 = v1 << 0x10;
            a0 <<= 0x10;
            v0 <<= 0x11;
            v1 <<= 0x11;
            v0 += v1 + var_a1;
            v0 += a0;
            var_t1 = v0 / 6 + (arg2 << 0x10);
            var_t0 = v0 / 6 + (arg3 << 0x10);
            var_a1 = v0 / 6 + (arg4 << 0x10);
        } break;
        case 7: {
            int v1 = Render_paletteTransitionState.backgroundBaseColors[i].green;
            int a0 = Render_paletteTransitionState.backgroundBaseColors[i].blue;
            int v0 = Render_paletteTransitionState.backgroundBaseColors[i].red;
            var_a1 = v1 << 0x10;
            a0 <<= 0x10;
            v0 <<= 0x11;
            v1 <<= 0x11;
            v0 += v1 + var_a1;
            v0 += a0;
            var_t1 = v0 / 12 + (arg2 << 0x10);
            var_t0 = v0 / 12 + (arg3 << 0x10);
            var_a1 = v0 / 12 + (arg4 << 0x10);
        } break;
        case 8:
            var_t1 = Render_paletteTransitionState.backgroundBaseColors[i].red << 0x10;
            var_t0 = Render_paletteTransitionState.backgroundBaseColors[i].green << 0x10;
            var_a1 = Render_paletteTransitionState.backgroundBaseColors[i].blue << 0x10;
            break;
        case 11:
            Render_paletteTransitionState.backgroundFadeMode = 0;
            return;
        case 0:
        default:
            var_t1 = p[i][0] + (arg2 << 0x10);
            var_t0 = p[i][1] + (arg3 << 0x10);
            var_a1 = p[i][2] + (arg4 << 0x10);
            break;
        }

        var_t1 &= 0xFFFF0000;
        var_t0 &= 0xFFFF0000;
        var_a1 &= 0xFFFF0000;

        if (var_t1 > 0xFF0000) {
            var_t1 = 0xFF0000;
        }
        if (var_t0 > 0xFF0000) {
            var_t0 = 0xFF0000;
        }
        if (var_a1 > 0xFF0000) {
            var_a1 = 0xFF0000;
        }
        if (var_t1 < 0) {
            var_t1 = 0;
        }
        if (var_t0 < 0) {
            var_t0 = 0;
        }
        if (var_a1 < 0) {
            var_a1 = 0;
        }
        if (arg1 != 0) {
            p[i][3] = (int)(var_t1 - (p[i][0] & 0xFFFF0000)) / (arg1 * 2);
            p[i][4] = (int)(var_t0 - (p[i][1] & 0xFFFF0000)) / (arg1 * 2);
            p[i][5] = (int)(var_a1 - (p[i][2] & 0xFFFF0000)) / (arg1 * 2);
        } else {
            p[i][0] = var_t1;
            p[i][1] = var_t0;
            p[i][2] = var_a1;
        }
    }

    if (arg1 != 0) {
        Render_paletteTransitionState.backgroundFadeActive = 1;
        Render_paletteTransitionState.backgroundFadeFrame = 0;
        Render_paletteTransitionState.backgroundFadeTick = 0;
        Render_paletteTransitionState.backgroundFadeSpeed = arg1;
        Render_paletteTransitionState.backgroundFadeMode = arg0;
        if (arg0 == 9) {
            Render_paletteTransitionState.backgroundPulseRed = arg2;
            Render_paletteTransitionState.backgroundPulseGreen = arg3;
            Render_paletteTransitionState.backgroundPulseBlue = arg4;
        }
    } else {
        Render_paletteTransitionState.backgroundFadeActive = 0;
        for (i = 0; i < 5; ++i) {
            int temp_a1, var_v0, new_var;
            temp_v0 = p[i][0];

            if (temp_v0 < 0) {
                temp_v0 += 0xFFFF;
            }

            temp_a1 = (temp_v0 >> 0x10);
            var_v0 = p[i][1];

            if (var_v0 < 0) {
                var_v0 += 0xFFFF;
            }

            new_var = temp_a1 + ((var_v0 >> 0x10) << 8);
            temp_v0 = p[i][2];

            if (temp_v0 < 0) {
                temp_v0 += 0xFFFF;
            }
            sp10[i] = new_var + ((temp_v0 >> 0x10) << 0x10);
        }
        Field_SetRoomBackgroundColors(sp10);
    }
}

// decomp: SLUS_010.40 func_800478E0 (src/SLUS_010.40/main.c:10990)
void Render_RequestBackgroundGradientTransform(int arg0, int arg1, int arg2, int arg3, int arg4)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_TransformBackgroundGradient(arg0, arg1, arg2, arg3, arg4);
    }
}

// decomp: SLUS_010.40 func_80047910 (src/SLUS_010.40/main.c:10997)
void Render_SetModelTint(int arg0, int arg1, Render_ColorRgb8* arg2)
{
    Render_ColorCode sp10;
    Render_ModelColorFade* temp_t0;
    int temp_a1;
    int temp_t1;
    int temp_t2;
    int v0;
    int* temp_a3;

    if (Render_paletteTransitionState.enabled == 0) {
        return;
    }

    temp_t0 = &Render_paletteTransitionState.modelColorFades[arg1];
    temp_a3 = Render_paletteTransitionState.modelColorFades[arg1].colorFixed;
    temp_a1 = Render_ToFixed16(arg2->red);
    temp_t1 = Render_ToFixed16(arg2->green);
    temp_t2 = Render_ToFixed16(arg2->blue);

    if (arg0 != 0) {
        temp_a3[3] = ((int)(temp_a1 - (temp_a3[0] & 0xFFFF0000)) / (arg0 * 2));
        temp_a3[4] = ((int)(temp_t1 - (temp_a3[1] & 0xFFFF0000)) / (arg0 * 2));
        temp_a3[5] = ((int)(temp_t2 - (temp_a3[2] & 0xFFFF0000)) / (arg0 * 2));
    } else {
        temp_a3[0] = temp_a1;
        temp_a3[1] = temp_t1;
        temp_a3[2] = temp_t2;
    }
    if (arg0 != 0) {
        temp_t0->progress[0] = 1;
        temp_t0->progress[1] = 0;
        temp_t0->progress[2] = 0;
        temp_t0->progress[3] = arg0;
        return;
    }

    temp_t0->progress[0] = 0;
    sp10.red = (temp_a3[0] / 65536) - 64;
    sp10.green = (temp_a3[1] / 65536) - 64;
    sp10.blue = (temp_a3[2] / 65536) - 64;
    sp10.code = 0;

    if (arg1 != 0) {
        Actors_SetActorFadeParameters(arg1 - 1, &sp10);
    } else {
        Field_SetRoomTint(&sp10, temp_t0->blendParams[3] >> 2);
    }
}

// decomp: SLUS_010.40 func_80047AB4 (src/SLUS_010.40/main.c:11047)
void Render_SetModelTintBase(int arg0, int arg1, Render_ColorRgb8* arg2)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_SetModelTint(arg0, arg1, arg2);
        Render_paletteTransitionState.modelColorFades[arg1].baseColor = *arg2;
    }
}

// decomp: SLUS_010.40 func_80047B30 (src/SLUS_010.40/main.c:11055)
void Render_TransformModelTint(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    Render_ColorCode sp10;
    short var_t4;
    int var_a1;
    int var_a2;
    int var_t0;
    int var_v0;
    int var_v1;
    Render_ModelColorFade* t1 = &Render_paletteTransitionState.modelColorFades[arg2];
    int* t2 = t1->colorFixed;

    var_t4 = 0;
    switch (arg0) {
    case 1:
        var_a1 = (t2[0] / 2) + (arg3 << 16);
        var_t0 = (t2[1] / 2) + (arg4 << 16);
        var_a2 = (t2[2] / 2) + (arg5 << 16);
        break;
    case 2:
        var_t4 = 0x4000;
        var_a1 = ((t2[0] * 2) + (t2[1] * 3) + t2[2]) / 6 + (arg3 << 0x10);
        var_t0 = ((t2[0] * 2) + (t2[1] * 3) + t2[2]) / 6 + (arg4 << 0x10);
        var_a2 = ((t2[0] * 2) + (t2[1] * 3) + t2[2]) / 6 + (arg5 << 0x10);
        break;
    case 3:
        var_t4 = 0x4000;
        var_v1 = ((t2[0] * 2) + (t2[1] * 3) + t2[2]) / 12;
        var_a1 = var_v1 + (arg3 << 0x10);
        var_t0 = var_v1 + (arg4 << 0x10);
        var_a2 = var_v1 + (arg5 << 0x10);
        break;
    case 4:
    case 9:
        var_a1 = (t1->baseColor.red + arg3) << 0x10;
        var_t0 = (t1->baseColor.green + arg4) << 0x10;
        var_a2 = (t1->baseColor.blue + arg5) << 0x10;
        break;
    case 5:
        var_t4 = 0x4000;
        var_a1 = (t1->baseColor.red << 0xF) + (arg3 << 0x10);
        var_t0 = (t1->baseColor.green << 0xF) + (arg4 << 0x10);
        var_a2 = (t1->baseColor.blue << 0xF) + (arg5 << 0x10);
        break;
    case 6:
        var_t4 = 0x4000;
        {
            int v1 = t1->baseColor.green;
            int a0 = t1->baseColor.blue;
            int v0 = t1->baseColor.red;
            var_a2 = v1 << 0x10;
            a0 <<= 0x10;
            v0 <<= 0x11;
            v1 <<= 0x11;
            v0 += v1 + var_a2;
            v0 += a0;
            var_a1 = v0 / 6 + (arg3 << 0x10);
            var_t0 = v0 / 6 + (arg4 << 0x10);
            var_a2 = v0 / 6 + (arg5 << 0x10);
        }
        break;
    case 7:
        var_t4 = 0x4000;
        {
            int v1 = t1->baseColor.green;
            int a0 = t1->baseColor.blue;
            int v0 = t1->baseColor.red;
            var_a2 = v1 << 0x10;
            a0 <<= 0x10;
            v0 <<= 0x11;
            v1 <<= 0x11;
            v0 += v1 + var_a2;
            v0 += a0;
            var_a1 = v0 / 12 + (arg3 << 0x10);
            var_t0 = v0 / 12 + (arg4 << 0x10);
            var_a2 = v0 / 12 + (arg5 << 0x10);
        }
        break;
    case 8:
    case 15:
        var_a1 = t1->baseColor.red << 0x10;
        var_t0 = t1->baseColor.green << 0x10;
        var_a2 = t1->baseColor.blue << 0x10;
        break;
    case 11:
        t1->progress[4] = 0;
        return;
    default:
        var_a1 = t2[0] + (arg3 << 0x10);
        var_t0 = t2[1] + (arg4 << 0x10);
        var_a2 = t2[2] + (arg5 << 0x10);
        break;
    }
    var_a1 = var_a1 & 0xFFFF0000;
    var_t0 = var_t0 & 0xFFFF0000;
    var_a2 = var_a2 & 0xFFFF0000;

    if (var_a1 > 0x800000) {
        var_a1 = 0x800000;
    }
    if (var_t0 > 0x800000) {
        var_t0 = 0x800000;
    }
    if (var_a2 > 0x800000) {
        var_a2 = 0x800000;
    }
    if (var_a1 < 0) {
        var_a1 = 0;
    }
    if (var_t0 < 0) {
        var_t0 = 0;
    }
    if (var_a2 < 0) {
        var_a2 = 0;
    }
    if (arg1 != 0) {
        t2[3] = ((int)(var_a1 - (t2[0] & 0xFFFF0000)) / (arg1 * 2));
        t2[4] = ((int)(var_t0 - (t2[1] & 0xFFFF0000)) / (arg1 * 2));
        t2[5] = ((int)(var_a2 - (t2[2] & 0xFFFF0000)) / (arg1 * 2));
        t1->blendParams[4] = ((var_t4 - t1->blendParams[3]) / (arg1 * 2));
    } else {
        t2[0] = var_a1;
        t2[1] = var_t0;
        t2[2] = var_a2;
        t1->blendParams[3] = var_t4;
    }
    var_v0 = 1;
    if (arg1 != 0) {
        t1->progress[0] = 1;
        t1->progress[1] = 0;
        t1->progress[2] = 0;
        t1->progress[3] = arg1;
        t1->progress[4] = arg0;
        if (arg0 == 9) {
            t1->blendParams[0] = arg3;
            t1->blendParams[1] = arg4;
            t1->blendParams[2] = arg5;
        }
    } else {
        t1->progress[0] = 0;
        var_v0 = t2[0] >> 0x10;
        if (t2[0] < 0) {
            var_v1 = t2[0] + 0xFFFF;
            var_v0 = var_v1 >> 0x10;
        }
        sp10.red = var_v0 - 0x40;
        var_v0 = t2[1] >> 0x10;
        if (t2[1] < 0) {
            var_v1 = t2[1] + 0xFFFF;
            var_v0 = var_v1 >> 0x10;
        }
        sp10.green = var_v0 - 0x40;
        var_v0 = t2[2] >> 0x10;
        if (t2[2] < 0) {
            var_v1 = t2[2] + 0xFFFF;
            var_v0 = var_v1 >> 0x10;
        }
        sp10.blue = var_v0 - 0x40;
        sp10.code = 0;
        if (arg2 != 0) {
            Actors_SetActorFadeParameters(arg2 - 1, &sp10);
        } else {
            Field_SetRoomTint(&sp10, ((u_short)t1->blendParams[3] << 0x10) >> 0x12);
        }
    }
}

// decomp: SLUS_010.40 func_80047FC0 (src/SLUS_010.40/main.c:11222)
void Render_RequestModelTintTransform(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    if (Render_paletteTransitionState.enabled != 0) {
        Render_TransformModelTint(arg0, arg1, arg2, arg3, arg4, arg5);
    }
}

// decomp: SLUS_010.40 func_80047FFC (src/SLUS_010.40/main.c:11229)
void Render_UpdateBackgroundGradientTransition(void)
{
    int sp18[5];
    int i;
    int var_v0;
    int new_var;
    int new_var2;
    int var_v1;
    int (*t1)[6];

    if (Render_paletteTransitionState.backgroundFadeActive == 0) {
        return;
    }

    t1 = Render_paletteTransitionState.backgroundColorsFixed;

    for (i = 0; i < 5; ++i) {
        t1[i][0] += t1[i][3];
        t1[i][1] += t1[i][4];
        t1[i][2] += t1[i][5];
    }

    for (i = 0; i < 5; ++i) {
        var_v1 = t1[i][0];
        if (var_v1 < 0) {
            var_v1 += 0xFFFF;
        }
        var_v0 = t1[i][1];
        new_var2 = var_v1 >> 16;
        if (var_v0 < 0) {
            var_v0 += 0xFFFF;
        }
        var_v1 = t1[i][2];
        new_var = new_var2 + ((var_v0 >> 16) << 8);
        if (var_v1 < 0) {
            var_v1 += 0xFFFF;
        }
        sp18[i] = new_var + ((var_v1 >> 16) << 16);
    }
    Field_SetRoomBackgroundColors(sp18);
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
    Render_ColorCode sp18;
    int i;
    int var_v0;
    Render_ModelColorFade* p = Render_paletteTransitionState.modelColorFades;

    for (i = 0; i < 17; ++i) {
        int* a2 = p[i].colorFixed;
        if (p[i].progress[0] != 0) {
            a2[0] += a2[3];
            a2[1] += a2[4];
            a2[2] += a2[5];

            var_v0 = a2[0];
            if (var_v0 < 0) {
                var_v0 += 0xFFFF;
            }
            sp18.red = (var_v0 >> 16) - 0x40;
            var_v0 = a2[1];
            if (var_v0 < 0) {
                var_v0 += 0xFFFF;
            }
            sp18.green = (var_v0 >> 16) - 0x40;
            var_v0 = a2[2];
            if (var_v0 < 0) {
                var_v0 += 0xFFFF;
            }
            sp18.blue = (var_v0 >> 16) - 0x40;
            sp18.code = 0;

            if (i != 0) {
                Actors_SetActorFadeParameters(i - 1, &sp18);
            } else {
                p[0].blendParams[3] += p[0].blendParams[4];
                Field_SetRoomTint(&sp18, p[0].blendParams[3] >> 2);
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
    u_int var_t5;
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
                var_t5 = 1;
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
                var_t5 = 2;
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
                var_t5 = 4;
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
                var_t5 = 8;
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
            if (++Render_paletteTransitionState.slots[i].transition.frame >= (16 / var_t5)) {
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
void Render_StopClutTransition(int arg0) { Render_paletteTransitionState.slots[arg0].transition.active = 0; }

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
    short bOffset, int arg6)
{
    int i;
    int new_var;
    int new_var2;
    int new_var3;
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
                new_var = value & 0x1F;
                new_var2 = (value & 0x3E0) >> 5;
                new_var3 = (value & 0x7C00) >> 10;
                r = rOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 6;
                g = gOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 6;
                b = bOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 6;
                break;

            case 7:
                new_var = value & 0x1F;
                new_var2 = (value & 0x3E0) >> 5;
                new_var3 = (value & 0x7C00) >> 10;
                r = rOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 12;
                g = gOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 12;
                b = bOffset + ((new_var * 2) + (new_var2 * 3) + new_var3) / 12;
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

        if (arg6 != 0) {
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
    u_short bOffset, int arg5)
{
    Render_TransformClutBase(8, srcClut, slot, rOffset, gOffset, bOffset, arg5);
}

// decomp: SLUS_010.40 func_80048EC4 (src/SLUS_010.40/main.c:11604)
void Render_CaptureRoomBackgroundColors(void)
{
    int i;
    int (*bgColors)[6] = Render_paletteTransitionState.backgroundColorsFixed;
    int* var_v1 = Field_RoomBackgroundColors();

    for (i = 0; i < 5; ++i) {
        Render_paletteTransitionState.backgroundBaseColors[i].red = var_v1[i];
        Render_paletteTransitionState.backgroundBaseColors[i].green = (var_v1[i] & 0xFF00) / 256;
        Render_paletteTransitionState.backgroundBaseColors[i].blue = (var_v1[i] & 0xFF0000) / 65536;

        bgColors[i][0] = (var_v1[i] & 0xFF) << 16;
        bgColors[i][1] = (var_v1[i] & 0xFF00) << 8;
        bgColors[i][2] = var_v1[i] & 0xFF0000;
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

