// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_ASHLEY_H_TYPES
#define VS_STATE_ASHLEY_H_TYPES
#include "Legacy/Psx.h"

struct State_AshleyLimbSnapshot;
struct State_AshleySnapshot;
struct State_AshleyStatsSnapshot;

// decomp: D_80060068_t2 (src/SLUS_010.40/main.h:209)
struct State_AshleyLimbSnapshot {
    u_short hp;
    u_short maxHp;
    u_short types[4];
    u_short affinities[8];
};
static_assert(sizeof(State_AshleyLimbSnapshot) == 0x1C, "State_AshleyLimbSnapshot must keep its PS1 layout");

// decomp: D_80060068_t3 (src/SLUS_010.40/main.h:231)
struct State_AshleyStatsSnapshot {
    int isPending;
    short currentHp;
    short maxHp;
    short currentMp;
    short maxMp;
    short strength;
    short totalStrength;
    short intelligence;
    short totalIntelligence;
    short agility;
    short totalAgility;
    short risk;
    u_short actorFlags;
    int statuses;
    char statusTimers[8];
    State_AshleyLimbSnapshot limbs[6];
    int weaponDrawn;
    int cameraPitch;
    int cameraDistance;
};
static_assert(sizeof(State_AshleyStatsSnapshot) == 0xDC, "State_AshleyStatsSnapshot must keep its PS1 layout");

// decomp: D_80060068_t (src/SLUS_010.40/main.h:236)
struct State_AshleySnapshot {
    State_AshleyStatsSnapshot stats;
    int reservedDC[9];
};
static_assert(sizeof(State_AshleySnapshot) == 0x100, "State_AshleySnapshot must keep its PS1 layout");

#endif // VS_STATE_ASHLEY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_ASHLEY_H_DECLS)
#define VS_STATE_ASHLEY_H_DECLS

extern State_AshleySnapshot State_ashleySnapshot;

#endif // VS_STATE_ASHLEY_H_DECLS
