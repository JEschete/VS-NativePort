// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Render_FramebufferEffectTranslation;

// decomp: D_800F1A68_t (src/BATTLE/BATTLE.PRG/146C.h:726)
struct Render_FramebufferEffectTranslation {
    int x;
    int y;
    int reserved8;
    int reservedC;
};
static_assert(sizeof(Render_FramebufferEffectTranslation) == 0x10, "Render_FramebufferEffectTranslation must keep its PS1 layout");
