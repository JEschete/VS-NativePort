// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_NAVIGATION_H_TYPES
#define VS_BATTLE_AI_NAVIGATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_Ai_DirectionDelta Battle_Ai_DirectionDelta;
typedef struct Battle_Ai_MovementPathView Battle_Ai_MovementPathView;
typedef struct Battle_Ai_RouteGateEnd Battle_Ai_RouteGateEnd;
typedef union Battle_Ai_TilePosition Battle_Ai_TilePosition;

// decomp: D_800F16EC_t (src/BATTLE/BATTLE.PRG/6F014.c:70)
typedef struct Battle_Ai_DirectionDelta {
    signed char dx : 2;
    signed char reserved2Bits : 2;
    signed char dz : 2;
    signed char reserved6Bits : 2;
} Battle_Ai_DirectionDelta;
_Static_assert(sizeof(Battle_Ai_DirectionDelta) == 0x1, "Battle_Ai_DirectionDelta must keep its PS1 layout");

// decomp: D_800F5910_t (src/BATTLE/BATTLE.PRG/6F014.c:43)
typedef struct Battle_Ai_RouteGateEnd {
    u_short tileX : 5;
    u_short tileZ : 5;
    u_short reservedBits10 : 6;
} Battle_Ai_RouteGateEnd;
_Static_assert(sizeof(Battle_Ai_RouteGateEnd) == 0x2, "Battle_Ai_RouteGateEnd must keep its PS1 layout");

// decomp: movementPathTest (src/BATTLE/BATTLE.PRG/771A8.c:2383)
typedef int (*Battle_Ai_PathTest)(Battle_Ai_MovementPathView*, int);

// decomp: routeEdgePair (src/BATTLE/BATTLE.PRG/771A8.c:566)
typedef u_short Battle_Ai_RouteEdgePair[2];

// decomp: vs_battle_movementPosition (src/BATTLE/BATTLE.PRG/6FC00.c:418)
typedef union Battle_Ai_TilePosition {
    unsigned int word;
    struct {
        unsigned int tileX : 8, tag : 8, tileZ : 8, searchStatus : 8;
    } tile;
} Battle_Ai_TilePosition;

#endif // VS_BATTLE_AI_NAVIGATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_NAVIGATION_H_DECLS)
#define VS_BATTLE_AI_NAVIGATION_H_DECLS
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"

extern Battle_Ai_DirectionDelta Battle_Ai_directionDeltas[8];
extern u_char Battle_Ai_routeStepDirections[272];
extern u_short (*Battle_Ai_tileFlagMap)[32];
extern unsigned short* Battle_Ai_roomWaypoints;
extern u_short (*Battle_Ai_specialTerrainHeights)[32];
extern Battle_Ai_RouteGateEnd* Battle_Ai_routeGates;
unsigned int Battle_Ai_FindAdjacentMovementDestination(
    Battle_Ai_MoveTargetView* state, int first, int second, unsigned int direction);
unsigned int Battle_Ai_WaypointValue(unsigned short* row, int index);
unsigned short* Battle_Ai_WaypointAddress(unsigned short* row, int index);
void Battle_Ai_FindNearestActiveWaypoint(Battle_Ai_WaypointSearchView* state, Battle_Ai_WaypointTile* destination);
void Battle_Ai_ResolveRouteGateExit(Battle_Ai_RouteGateTile* entry_tile, Battle_Ai_RouteGateTile* exit_tile_out);
void Battle_Ai_FindNearestRouteGateEntry(int platform_index, int arg1, int arg2, Battle_Ai_RouteGateTile* entry_tile_out);
int Battle_Ai_FindRouteGateAtTile(intptr_t arg0, u_int packed_tile, int match_entry_end);
u_int Battle_Ai_RouteEndpointPair(Battle_Ai_RouteEdgePair* pair);
u_short Battle_Ai_RouteEndpointValue(Battle_Ai_RouteEdgePair* pair, int index);
u_int Battle_Ai_NavigationTileFlags(int z, int x);
int Battle_Ai_SetNavigationAreaFromTile(Battle_Ai_AreaRouteView* state, Battle_Ai_TilePosition position);
int Battle_Ai_ResolveNavigationAreaRoute(Battle_Ai_AreaRouteView* state, int index, int area);
void Battle_Ai_UpdateNavigationAreaMovement(Battle_Ai_TerrainMoveView* state);
int Battle_Ai_FollowObstacleDetour(Battle_Ai_MovementPathView* state, Battle_Ai_PathTest test, int mask);
void Battle_Ai_FloodFillAiTerrainHeights(int tile_x, int tile_z, int height_value);
int Battle_Ai_FindAdjacentAiGroundHeight(int tile_x, int tile_z);
void Battle_Ai_BuildAiSpecialTerrainHeights(void);
void Battle_Ai_InitializeRoomNavigation(void);

#endif // VS_BATTLE_AI_NAVIGATION_H_DECLS
