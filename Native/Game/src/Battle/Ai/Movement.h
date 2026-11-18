// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_Ai_ArenaOrbitState;

// layout: ledger (upstream merge) for D_800F5920_t
struct Battle_Ai_ArenaOrbitState {
    /* 0x00 */ u_short heading;
    /* 0x02 */ u_char phase;
    /* 0x03 */ u_char distanceCached;
    /* 0x04 */ int distanceSquared;
    /* 0x08 */ char reserved8[2];
    /* 0x0A */ u_char isOrbiting;
    /* 0x0B */ u_char pathStep;
    /* 0x0C */ short cooldown;
    /* 0x0E */ short requested;
    /* 0x10 */ short orbitAngle;
    /* 0x12 */ short savedHeight;
    /* 0x14 */ u_char healthPhase;
    /* 0x15 */ char reserved15;
    /* 0x16 */ u_char planSaved;
    /* 0x17 */ u_char savedPlanCount;
    /* 0x18 */ short startX;
    /* 0x1A */ short startZ;
    /* 0x1C */ u_char started;
};
static_assert(sizeof(Battle_Ai_ArenaOrbitState) == 0x20, "Battle_Ai_ArenaOrbitState must keep its PS1 layout");

extern int Battle_Ai_savedMovementRandomState;
extern unsigned short (*Battle_Ai_patrolRoutes)[8];
