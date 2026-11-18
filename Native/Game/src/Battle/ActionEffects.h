// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_SoundEventTargets;

#include "Effects/Runtime.h"

// decomp: soundEventTargets (src/BATTLE/BATTLE.PRG/5BF94.c:637)
struct Battle_SoundEventTargets {
    u_short reserved0;
    u_char reserved2, reserved3;
    char reserved4[12];
    Effects_ActionEffectTarget reserved10[0];
};
