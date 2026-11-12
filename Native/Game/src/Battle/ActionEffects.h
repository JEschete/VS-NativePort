// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONEFFECTS_H_TYPES
#define VS_BATTLE_ACTIONEFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_SoundEventTargets Battle_SoundEventTargets;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#endif
#include "Effects/Runtime.h"
#ifdef VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#endif

// decomp: soundEventTargets (src/BATTLE/BATTLE.PRG/5BF94.c:637)
typedef struct Battle_SoundEventTargets {
    u_short reserved0;
    u_char reserved2, reserved3;
    char reserved4[12];
    Effects_ActionEffectTarget reserved10[0];
} Battle_SoundEventTargets;

#endif // VS_BATTLE_ACTIONEFFECTS_H_TYPES
