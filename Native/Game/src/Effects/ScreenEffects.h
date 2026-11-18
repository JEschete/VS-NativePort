// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
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

extern Effects_ScreenEffectOverlay* Effects_screenEffectOverlay;
