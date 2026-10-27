// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES
#define VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_FramebufferEffectTranslation Render_FramebufferEffectTranslation;
typedef struct Render_LegacyFramebufferEffectPackets Render_LegacyFramebufferEffectPackets;

// decomp: D_800F1A2C (D_800F1A2C@BATTLE/BATTLE.PRG): a view into another global
#define Render_framebufferEffectRoll (*(short*)((u_char*)&Render_framebufferEffectRotation + 4))

// decomp: D_800F1A68_t (src/BATTLE/BATTLE.PRG/146C.h:726)
typedef struct Render_FramebufferEffectTranslation {
    int x;
    int y;
    int reserved8;
    int reservedC;
} Render_FramebufferEffectTranslation;
_Static_assert(sizeof(Render_FramebufferEffectTranslation) == 0x10, "Render_FramebufferEffectTranslation must keep its PS1 layout");

// decomp: D_800F1ABC_t (src/BATTLE/BATTLE.PRG/146C.c:301)
typedef struct Render_LegacyFramebufferEffectPackets {
    POLY_FT4 rotatedStrips[10];
    SPRT flatStrips[10];
    DR_MODE drawModes[10];
} Render_LegacyFramebufferEffectPackets;
_Static_assert(sizeof(Render_LegacyFramebufferEffectPackets) == 0x2D0, "Render_LegacyFramebufferEffectPackets must keep its PS1 layout");

#endif // VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_FRAMEBUFFEREFFECTS_H_DECLS)
#define VS_RENDER_FRAMEBUFFEREFFECTS_H_DECLS

extern int Render_framebufferEffectParameter;
extern SVECTOR Render_framebufferEffectRotation;
extern VECTOR Render_framebufferEffectScale;
extern int Render_framebufferEffectUsesPreviousFrame;
extern u_int Render_framebufferEffectDepth;
extern Render_FramebufferEffectTranslation Render_framebufferEffectOffset;
extern CVECTOR Render_framebufferEffectTint;
extern char Render_framebufferEffectResetBytes[4];
extern Render_LegacyFramebufferEffectPackets* Render_legacyFramebufferEffectPackets;
extern int Render_framebufferEffectState;
extern int Render_framebufferEffectBlendMode;
void Render_DrawScreenTransition(void* ot);
void Render_ResetFramebufferEffect(void);
void Render_InitializeFramebufferEffect(void);
void Render_BuildFramebufferEffectPrimitives(void);
void Render_DrawFramebufferEffect(void* ordering_table);
void Render_SetFramebufferEffectEnabled(int enabled);
void Render_SetFramebufferEffectRotation(int roll_angle);
void Render_SetFramebufferEffectScale(Render_FramebufferEffectTranslation* scale);
void Render_SetFramebufferEffectTint(CVECTOR* tint);
void Render_SetFramebufferEffectOffset(Render_FramebufferEffectTranslation* offset);
void Render_SetFramebufferEffectBlendMode(int blend_mode);
void Render_SetFramebufferEffectParameter(int arg0);
void Render_SetFramebufferEffectDepth(u_int ordering_depth);
void Render_SetFramebufferPreviousFrame(int use_previous_frame);
int Render_FramebufferEffectState(void);
short Render_FramebufferEffectRotation(void);
void Render_FramebufferEffectScale(int* output);
void Render_FramebufferEffectTint(CVECTOR* output);
void Render_FramebufferEffectOffset(Render_FramebufferEffectTranslation* output);
int Render_FramebufferEffectBlendMode(void);
int Render_FramebufferEffectParameter(void);
int Render_FramebufferEffectDepth(void);
int Render_UsesFramebufferPreviousFrame(void);

#endif // VS_RENDER_FRAMEBUFFEREFFECTS_H_DECLS
