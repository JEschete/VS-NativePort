// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_SCREENEFFECTS_H_TYPES
#define VS_EFFECTS_SCREENEFFECTS_H_TYPES
#include "Legacy/Psx.h"

struct Content_DiscQueueSlot;
struct Effects_ScreenEffectOverlay;

// decomp: D_800EB9B4_t (src/GIM/SCREFF2.PRG/0.h:12)
struct Effects_ScreenEffectOverlay {
    u_char loading;
    u_char opaque1;
    u_short feedbackLayer;
    short radialWarpArg0;
    short radialWarpArg1;
    int vignetteEnabled;
    Content_DiscQueueSlot* slot;
};

#endif // VS_EFFECTS_SCREENEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_SCREENEFFECTS_H_DECLS)
#define VS_EFFECTS_SCREENEFFECTS_H_DECLS

extern Effects_ScreenEffectOverlay* Effects_screenEffectOverlay;

#endif // VS_EFFECTS_SCREENEFFECTS_H_DECLS
