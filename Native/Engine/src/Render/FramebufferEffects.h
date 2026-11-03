// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES
#define VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_FramebufferEffectTranslation Render_FramebufferEffectTranslation;

// decomp: D_800F1A68_t (src/BATTLE/BATTLE.PRG/146C.h:726)
typedef struct Render_FramebufferEffectTranslation {
    int x;
    int y;
    int reserved8;
    int reservedC;
} Render_FramebufferEffectTranslation;
_Static_assert(sizeof(Render_FramebufferEffectTranslation) == 0x10, "Render_FramebufferEffectTranslation must keep its PS1 layout");

#endif // VS_RENDER_FRAMEBUFFEREFFECTS_H_TYPES
