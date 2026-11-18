// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_NAVIGATION_H_TYPES
#define VS_BATTLE_AI_NAVIGATION_H_TYPES
#include "Legacy/Psx.h"

struct Battle_Ai_DirectionDelta;
struct Battle_Ai_MovementPathView;
struct Battle_Ai_RouteGateEnd;
union Battle_Ai_TilePosition;

// decomp: D_800F16EC_t (src/BATTLE/BATTLE.PRG/6F014.c:70)
struct Battle_Ai_DirectionDelta {
    signed char dx : 2;
    signed char reserved2Bits : 2;
    signed char dz : 2;
    signed char reserved6Bits : 2;
};
static_assert(sizeof(Battle_Ai_DirectionDelta) == 0x1, "Battle_Ai_DirectionDelta must keep its PS1 layout");

// decomp: D_800F5910_t (src/BATTLE/BATTLE.PRG/6F014.c:43)
struct Battle_Ai_RouteGateEnd {
    u_short tileX : 5;
    u_short tileZ : 5;
    u_short reservedBits10 : 6;
};
static_assert(sizeof(Battle_Ai_RouteGateEnd) == 0x2, "Battle_Ai_RouteGateEnd must keep its PS1 layout");

// decomp: movementPathTest (src/BATTLE/BATTLE.PRG/771A8.c:2383)
using Battle_Ai_PathTest = int (*)(Battle_Ai_MovementPathView*, int);

// decomp: routeEdgePair (src/BATTLE/BATTLE.PRG/771A8.c:566)
using Battle_Ai_RouteEdgePair = u_short[2];

// decomp: vs_battle_movementPosition (src/BATTLE/BATTLE.PRG/6FC00.c:418)
union Battle_Ai_TilePosition {
    unsigned int word;
    struct {
        unsigned int tileX : 8, tag : 8, tileZ : 8, searchStatus : 8;
    } tile;
};

#endif // VS_BATTLE_AI_NAVIGATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_NAVIGATION_H_DECLS)
#define VS_BATTLE_AI_NAVIGATION_H_DECLS

extern Battle_Ai_DirectionDelta Battle_Ai_directionDeltas[8];
extern u_char Battle_Ai_routeStepDirections[272];
extern u_short (*Battle_Ai_tileFlagMap)[32];
extern unsigned short* Battle_Ai_roomWaypoints;
extern u_short (*Battle_Ai_specialTerrainHeights)[32];
extern Battle_Ai_RouteGateEnd* Battle_Ai_routeGates;

#endif // VS_BATTLE_AI_NAVIGATION_H_DECLS
