// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_ActionDefinition;
struct Battle_HitResult;
struct Battle_HitTargetWord;

// decomp: func_8008631C_t2 (src/BATTLE/BATTLE.PRG/146C.c:37)
struct Battle_HitTargetWord {
    u_char targetActor;
    signed char targetLimb;
    u_char hitThreshold;
    u_char impactKind;
    short hp;
    short mp;
};
static_assert(sizeof(Battle_HitTargetWord) == 0x8, "Battle_HitTargetWord must keep its PS1 layout");

extern u_char Battle_limbInjuryStatusIds[20];
extern void (*Battle_actionEffectMediators[70])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern u_int Battle_hitPrerequisiteResult;
extern u_int Battle_previousHitResult;
