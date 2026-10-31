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
_Static_assert(sizeof(Render_ColorRgb8) == 0x4, "Render_ColorRgb8 must keep its PS1 layout");

// decomp: D_8005DC80_t (src/SLUS_010.40/main.c:32)
typedef struct Render_ModelColorFade {
    char progress[6];
    short blendParams[5];
    int colorFixed[6];
    Render_ColorRgb8 baseColor;
} Render_ModelColorFade;
_Static_assert(sizeof(Render_ModelColorFade) == 0x2C, "Render_ModelColorFade must keep its PS1 layout");

// decomp: D_800F4538_unk58_2 (src/BATTLE/BATTLE.PRG/3A1A0.h:38)
typedef struct Render_ColorCode {
    u_char red;
    u_char green;
    u_char blue;
    u_char code;
} Render_ColorCode;
_Static_assert(sizeof(Render_ColorCode) == 0x4, "Render_ColorCode must keep its PS1 layout");

// decomp: _clutSlot (src/SLUS_010.40/main.c:57)
struct Render_PaletteTransitionTimer {
    u_char active;
    u_char frame;
    u_char tick;
    u_char speed;
    u_char mode;
    signed char pulseRed;
    signed char pulseGreen;
    signed char pulseBlue;
};
struct Render_PaletteEntryFade {
    char red;
    char green;
    char blue;
    char maskBit;
    u_char redStepRow;
    u_char greenStepRow;
    u_char blueStepRow;
};
typedef struct Render_PaletteSlot {
    char reserved0;
    char locked;
    struct Render_PaletteTransitionTimer transition;
    struct Render_PaletteEntryFade entries[256];
    u_short baseColors[256];
} Render_PaletteSlot;
_Static_assert(sizeof(Render_PaletteSlot) == 0x90A, "Render_PaletteSlot must keep its PS1 layout");

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
_Static_assert(sizeof(Render_PaletteTransitionState) == 0x8214, "Render_PaletteTransitionState must keep its PS1 layout");

#endif // VS_RENDER_PALETTE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_PALETTE_H_DECLS)
#define VS_RENDER_PALETTE_H_DECLS

extern Render_PaletteTransitionState Render_paletteTransitionState;
extern u_short Render_paletteColors[14][256];
extern signed char Render_paletteDeltaSequenceSixteen[][16];
extern signed char Render_paletteDeltaSequenceTwo[][2];
extern signed char Render_paletteDeltaSequenceFour[][4];
extern signed char Render_paletteDeltaSequenceEight[][8];
void Render_TransformRoomPalettes(int mode, int duration, int red_offset, short green_offset, short blue_offset);
void Render_TransformSceneColors(int mode, int duration, short red_offset, short green_offset, short blue_offset);
void Render_TransformActorColors(int mode, int duration, int actor_index, short red_offset, short green_offset, short blue_offset);
void Render_TransformBackgroundColors(int mode, int duration, short red_offset, short green_offset, short blue_offset);
void Render_LoadRoomPaletteTransition(int duration, u_short* palette_colors, int arg2, int clut_offset);
void Render_StartSharedPaletteTransition(int duration, int shared_slot, u_short* palette_colors, int clut_offset);
void Render_ApplyBackgroundGradientColors(int duration, Render_ColorRgb8* colors);
int Render_StepRgbTowardTarget(u_char* current_channels, u_char* target_channels, int signed_step);
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
int Render_ToFixed16(int value);
void Render_SetBackgroundGradientColors(int duration, Render_ColorRgb8* colors);
void Render_SetBackgroundGradientBase(int duration, Render_ColorRgb8* colors);
void Render_TransformBackgroundGradient(int mode, int duration, int red_offset, int green_offset, int blue_offset);
void Render_RequestBackgroundGradientTransform(int mode, int duration, int red_offset, int green_offset, int blue_offset);
void Render_SetModelTint(int duration, int tint_slot, Render_ColorRgb8* color);
void Render_SetModelTintBase(int duration, int tint_slot, Render_ColorRgb8* color);
void Render_TransformModelTint(int mode, int duration, int tint_slot, int red_offset, int green_offset, int blue_offset);
void Render_RequestModelTintTransform(int mode, int duration, int tint_slot, int red_offset, int green_offset, int blue_offset);
void Render_UpdateBackgroundGradientTransition(void);
void Render_UpdateModelColorTransitions(void);
void Render_UpdateClutTransitions(void);
void Render_StopClutTransition(int slot);
void Render_LoadClutColors(u_short const* img, u_int slot, u_int colorOffset, u_int count);
void Render_TransformClutBase(int mode, u_short* srcClut, int slot, short rOffset, short gOffset,
    short bOffset, int update_current_colors);
void Render_OffsetClutBase(u_short* srcClut, int slot, u_short rOffset, u_short gOffset,
    u_short bOffset, int update_current_colors);
void Render_CaptureRoomBackgroundColors(void);
void Render_CommitClutBuffer(void);
void Render_SetClutAnimationEnabled(int active);
void Render_InitializeClutAnimation(void);

#endif // VS_RENDER_PALETTE_H_DECLS
