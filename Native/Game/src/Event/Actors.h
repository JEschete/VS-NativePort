// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_ActorSelection;

// decomp: func_800BB68C_t (src/BATTLE/BATTLE.PRG/4A0A8.c:182)
struct Event_ActorSelection {
    char remainingCount;
    char cursor;
    char actorIds[22];
};
static_assert(sizeof(Event_ActorSelection) == 0x18, "Event_ActorSelection must keep its PS1 layout");

extern int Event_weaponDrawStates[121];
extern char Event_animationBankBases[472];
