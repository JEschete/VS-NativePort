// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_PALETTE_H_TYPES
#define VS_RENDER_PALETTE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_ColorCode Render_ColorCode;
typedef struct Render_ColorRgb8 Render_ColorRgb8;
typedef struct Render_ModelColorFade Render_ModelColorFade;
typedef struct Render_PaletteSlot Render_PaletteSlot;
typedef struct Render_PaletteTransitionState Render_PaletteTransitionState;

// decomp: D_8005DC6C_t (src/SLUS_010.40/main.h:382)
typedef struct Render_ColorRgb8 {
    char red;
    char green;
    char blue;
    char pad3;
} Render_ColorRgb8;

// decomp: D_8005DC80_t (src/SLUS_010.40/main.c:32)
typedef struct Render_ModelColorFade {
    char progress[6];
    short blendParams[5];
    int colorFixed[6];
    Render_ColorRgb8 baseColor;
} Render_ModelColorFade;

// decomp: D_800F4538_unk58_2 (src/BATTLE/BATTLE.PRG/3A1A0.h:38)
typedef struct Render_ColorCode {
    u_char red;
    u_char green;
    u_char blue;
    u_char code;
} Render_ColorCode;

// decomp: _clutSlot (src/SLUS_010.40/main.c:57)
typedef struct Render_PaletteSlot {
    char reserved0;
    char locked;
    struct Render_PaletteTransitionTimer {
        u_char active;
        u_char frame;
        u_char tick;
        u_char speed;
        u_char mode;
        signed char pulseRed;
        signed char pulseGreen;
        signed char pulseBlue;
    } transition;
    struct Render_PaletteEntryFade {
        char red;
        char green;
        char blue;
        char maskBit;
        u_char redStepRow;
        u_char greenStepRow;
        u_char blueStepRow;
    } entries[256];
    u_short baseColors[256];
} Render_PaletteSlot;

// decomp: _clutState_t (src/SLUS_010.40/main.c:75)
typedef struct Render_PaletteTransitionState {
    u_short uploadPending;
    u_short enabled;
    Render_PaletteSlot slots[14];
    u_char backgroundFadeActive;
    u_char backgroundFadeFrame;
    u_char backgroundFadeTick;
    u_char backgroundFadeSpeed;
    u_char backgroundFadeMode;
    u_char pad7E95;
    short backgroundPulseRed;
    short backgroundPulseGreen;
    short backgroundPulseBlue;
    int backgroundColorsFixed[5][6];
    Render_ColorRgb8 backgroundBaseColors[5];
    Render_ModelColorFade modelColorFades[17];
} Render_PaletteTransitionState;

#endif // VS_RENDER_PALETTE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_PALETTE_H_DECLS)
#define VS_RENDER_PALETTE_H_DECLS

extern Render_PaletteTransitionState Render_paletteTransitionState;
extern u_short Render_paletteColors[14][256];
extern signed char Render_paletteDeltaSequenceSixteen[][16];
extern signed char Render_paletteDeltaSequenceTwo[][2];
extern signed char Render_paletteDeltaSequenceFour[][4];
extern signed char Render_paletteDeltaSequenceEight[][8];
void Render_TransformRoomPalettes(int arg0, int arg1, int arg2, short arg3, short arg4);
void Render_TransformSceneColors(int arg0, int arg1, short arg2, short arg3, short arg4);
void Render_TransformActorColors(int arg0, int arg1, int arg2, short arg3, short arg4, short arg5);
void Render_TransformBackgroundColors(int arg0, int arg1, short arg2, short arg3, short arg4);
void Render_LoadRoomPaletteTransition(int arg0, u_short* arg1, int arg2, int arg3);
void Render_StartSharedPaletteTransition(int arg0, int arg1, u_short* arg2, int arg3);
void Render_ApplyBackgroundGradientColors(int arg0, Render_ColorRgb8* arg1);
int Render_StepRgbTowardTarget(u_char* arg0, u_char* arg1, int arg2);
int Render_ApplyPaletteTransition(u_short* colors, int count, int amount, int mode, u_short* reference);
void Render_BuildClutTransition(int duration, int slot, u_short* sourceClut);
void Render_StartClutTransition(
    int duration, int slot, u_short* sourceClut, int clutOffset);
void Render_StartClutTransitionWithBase(
    int duration, int slot, u_short* sourceClut, int clutOffset);
void Render_SaveClutBase(int slot);
int Render_WeightedLuminance(short g, short b, short r);
void Render_ApplyClutSlotTransform(
    int mode, int duration, int slot, int rOffset, short gOffset, short bOffset);
void Render_TransformClutSlot(
    int mode, int duration, int slot, short rOffset, int gOffset, int bOffset);
int Render_ToFixed16(int arg0);
void Render_SetBackgroundGradientColors(int arg0, Render_ColorRgb8* arg1);
void Render_SetBackgroundGradientBase(int arg0, Render_ColorRgb8* arg1);
void Render_TransformBackgroundGradient(int arg0, int arg1, int arg2, int arg3, int arg4);
void Render_RequestBackgroundGradientTransform(int arg0, int arg1, int arg2, int arg3, int arg4);
void Render_SetModelTint(int arg0, int arg1, Render_ColorRgb8* arg2);
void Render_SetModelTintBase(int arg0, int arg1, Render_ColorRgb8* arg2);
void Render_TransformModelTint(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5);
void Render_RequestModelTintTransform(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5);
void Render_UpdateBackgroundGradientTransition(void);
void Render_UpdateModelColorTransitions(void);
void Render_UpdateClutTransitions(void);
void Render_StopClutTransition(int arg0);
void Render_LoadClutColors(u_short const* img, u_int slot, u_int colorOffset, u_int count);
void Render_TransformClutBase(int mode, u_short* srcClut, int slot, short rOffset, short gOffset,
    short bOffset, int arg6);
void Render_OffsetClutBase(u_short* srcClut, int slot, u_short rOffset, u_short gOffset,
    u_short bOffset, int arg5);
void Render_CaptureRoomBackgroundColors(void);
void Render_CommitClutBuffer(void);
void Render_SetClutAnimationEnabled(int active);
void Render_InitializeClutAnimation(void);

#endif // VS_RENDER_PALETTE_H_DECLS
