// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/RoomState.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/TerrainHeight.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800F16EC (D_800F16EC@BATTLE/BATTLE.PRG)
Battle_Ai_DirectionDelta Battle_Ai_directionDeltas[8]; /* contents: module image (ModuleData.c) */

// decomp: D_800F175C (D_800F175C@BATTLE/BATTLE.PRG)
u_char Battle_Ai_routeStepDirections[272]; /* contents: module image (ModuleData.c) */

// decomp: D_800F58B8 (D_800F58B8@BATTLE/BATTLE.PRG)
u_short (*Battle_Ai_tileFlagMap)[32];

// decomp: D_800F58C8 (D_800F58C8@BATTLE/BATTLE.PRG)
unsigned short* Battle_Ai_roomWaypoints;

// decomp: D_800F58D0 (D_800F58D0@BATTLE/BATTLE.PRG)
u_short (*Battle_Ai_specialTerrainHeights)[32];

// decomp: D_800F5910 (D_800F5910@BATTLE/BATTLE.PRG)
Battle_Ai_RouteGateEnd* Battle_Ai_routeGates;

// decomp: BATTLE.PRG func_800D8400 (src/BATTLE/BATTLE.PRG/6FC00.c:430)
unsigned int Battle_Ai_FindAdjacentMovementDestination(
    Battle_Ai_MoveTargetView* state, int first, int second, unsigned int direction)
{
    Battle_Ai_TilePosition dest, source;
    int count, i, result;
    unsigned int directions, quadrant;
    int nearest;
    if (state->movementDisabled) {
        return 0;
    }
    dest.tile.searchStatus = 1;
    source = state->tile;
    if (first == second) {
        directions = direction;
        count = 1;
    } else {
        quadrant = (direction & 0xFFF) >> 10;
        if ((direction & 0x3FF) > 512) {
            nearest = ((quadrant + 1) & 3) * 2;
        } else {
            nearest = quadrant * 2;
        }
        direction = nearest;
        if (direction != first) {
            directions = (first << 16) | direction;
        } else {
            directions = (second << 16) | direction;
        }
        count = 2;
    }
    for (i = 0; i < count; i++) {
        direction = (directions >> (i * 16)) & 0xFFFF;
        dest.tile.tileX = source.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dx;
        dest.tile.tileZ = source.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dz;
        result = Battle_Ai_CanTraverseTileEdge(state, source.word, dest.word, direction);
        if (result > 0) {
            result = Battle_Ai_DestinationOccupancy(state, dest.word);
            if (result < 0) {
                goto blocked;
            }
            if (result) {
                goto done;
            }
            return 0;
        }
        if (result < 0) {
            goto blocked;
        }
        dest.word = Battle_Ai_FindGapLandingTile(state, dest.word, direction, source.word);
        if (dest.tile.searchStatus == 1) {
            goto done;
        }
        if (dest.tile.searchStatus) {
            goto blocked;
        }
    }
    return 0;
blocked:
    dest.tile.searchStatus = 2;
done:
    return dest.word & 0xFFFF00FF;
}

// decomp: BATTLE.PRG func_800DC638 (src/BATTLE/BATTLE.PRG/73C84.s:0) [asm-file: not yet ported]
void Battle_Ai_UpdateRouteGateAvailability(void)
{
    VS_UNPORTED("Battle::Ai::UpdateRouteGateAvailability");
}

// decomp: BATTLE.PRG nearestWaypointValue (src/BATTLE/BATTLE.PRG/74088.c:946)
unsigned int Battle_Ai_WaypointValue(unsigned short* row, int index)
{
    unsigned short* p = row;
    p += index;
    return *p;
}

// decomp: BATTLE.PRG nearestWaypointAddress (src/BATTLE/BATTLE.PRG/74088.c:952)
unsigned short* Battle_Ai_WaypointAddress(unsigned short* row, int index)
{
    row += index;
    return row;
}

// decomp: BATTLE.PRG func_800DD7CC (src/BATTLE/BATTLE.PRG/74088.c:957)
void Battle_Ai_FindNearestActiveWaypoint(Battle_Ai_WaypointSearchView* state, Battle_Ai_WaypointTile* destination)
{
    int x;
    int z;
    u_short (*savedMap)[32];
    int dx;
    int dz;
    int index;
    unsigned short entry;
    unsigned int packed;
    unsigned int distance;
    unsigned int closest;

    closest = -1U;
    {
        index = 0;
        savedMap = Battle_Ai_FrameState()->heightMap;
    }
    Battle_Ai_FrameState()->heightMap = Battle_Ai_specialTerrainHeights;
    do {
        entry = *Battle_Ai_WaypointAddress(Battle_Ai_roomWaypoints, index);
        packed = entry & 0xFFFF;
        if ((packed >> 0xF) != 0) {
            x = ((entry & 0x1F) << 7) + 0x40;
            z = ((packed * 4) & 0xF80) + 0x40;
            dx = state->x - x;
            dz = state->z - z;
            distance = Core_Math_SquaredVectorLength(dx, state->y - Field_GroundHeightAtWorldPosition(x, z), dz);
            if (distance < closest) {
                destination->x =
                    (signed char)(Battle_Ai_WaypointValue(Battle_Ai_roomWaypoints, index) & 0x1F);
                closest = distance;
                destination->z =
                    (signed char)(((unsigned short)Battle_Ai_WaypointValue(Battle_Ai_roomWaypoints, index)
                                      >> 5)
                                  & 0x1F);
            }
        }
        index += 1;
    } while (index < 8);
    Battle_Ai_FrameState()->heightMap = savedMap;
}

// decomp: BATTLE.PRG func_800DF9A8 (src/BATTLE/BATTLE.PRG/771A8.c:459)
void Battle_Ai_ResolveRouteGateExit(Battle_Ai_RouteGateTile* arg0, Battle_Ai_RouteGateTile* arg1)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (Battle_Ai_routeGates[i * 2].tileX == arg0->opaque0
            && Battle_Ai_routeGates[i * 2].tileZ == arg0->opaque2) {
            if (i & 1) {
                i--;
            } else {
                i++;
            }
            arg1->opaque0 = Battle_Ai_routeGates[i * 2 + 1].tileX;
            arg1->opaque2 = Battle_Ai_routeGates[i * 2 + 1].tileZ;
            arg1->opaque1 = i >> 1;
            return;
        }
    }
    *(int*)arg1 = *(int*)arg0;
}

// decomp: BATTLE.PRG func_800DFA54 (src/BATTLE/BATTLE.PRG/771A8.c:480)
void Battle_Ai_FindNearestRouteGateEntry(int arg0, int arg1, int arg2, Battle_Ai_RouteGateTile* arg3)
{
    int i;

    arg1 >>= 19;
    arg2 >>= 19;
    for (i = arg0 * 2; i < arg0 * 2 + 2; i++) {
        if (arg1 == Battle_Ai_routeGates[i * 2 + 1].tileX
            && arg2 == Battle_Ai_routeGates[i * 2 + 1].tileZ) {
            goto found;
        }
    }
    i = arg0 * 2;
found:
    arg3->opaque0 = Battle_Ai_routeGates[i * 2].tileX;
    arg3->opaque2 = Battle_Ai_routeGates[i * 2].tileZ;
}

// decomp: BATTLE.PRG func_800DFAF8 (src/BATTLE/BATTLE.PRG/771A8.c:498)
int Battle_Ai_FindRouteGateAtTile(intptr_t arg0, u_int arg1, int arg2)
{
    Field_PlatformContactPoint sp10[4];
    Field_PlatformContactPoint* m;
    int i;
    int j;

    Field_CollectMovingPlatformPositions(sp10);
    for (i = 0; i < 8; ++i) {
        Battle_Ai_RouteGateEnd* e = &Battle_Ai_routeGates[i * 2];
        j = i >> 1;
        if (arg2 == 0) {
            ++e;
        }
        if ((u_char)arg1 == e->tileX && (u_char)(arg1 >> 16) == e->tileZ) {
            m = &sp10[j];
            if (Battle_Ai_routeGates[i * 2 + 1].tileX == m->x >> 7
                && Battle_Ai_routeGates[i * 2 + 1].tileZ == m->z >> 7) {
                return i;
            }
        }
    }
    return -1;
}

// decomp: BATTLE.PRG routeWord (src/BATTLE/BATTLE.PRG/771A8.c:572)
u_int Battle_Ai_RouteEndpointPair(Battle_Ai_RouteEdgePair* pair) { return *(u_int*)pair; }

// decomp: BATTLE.PRG routeHalf (src/BATTLE/BATTLE.PRG/771A8.c:574)
u_short Battle_Ai_RouteEndpointValue(Battle_Ai_RouteEdgePair* pair, int index) { return (*pair)[index]; }

// decomp: BATTLE.PRG routeTile (src/BATTLE/BATTLE.PRG/771A8.c:576)
u_int Battle_Ai_NavigationTileFlags(int z, int x)
{
    return (*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[z][x];
}

// decomp: BATTLE.PRG routeSetArea (src/BATTLE/BATTLE.PRG/771A8.c:581)
int Battle_Ai_SetNavigationAreaFromTile(Battle_Ai_AreaRouteView* state, Battle_Ai_TilePosition position)
{
    state->area = Battle_Ai_NavigationTileFlags(position.tile.tileZ, position.tile.tileX) & 15;
    return 6;
}

// decomp: BATTLE.PRG func_800DFBCC (src/BATTLE/BATTLE.PRG/771A8.c:587)
int Battle_Ai_ResolveNavigationAreaRoute(Battle_Ai_AreaRouteView* state, int index, int area)
{
    Battle_Ai_RouteEdgePair routes;
    int distances[2];
    Battle_Ai_TilePosition position;
    Battle_Ai_TilePosition candidate;
    int count;
    int i;
    int result;
    int amount;
    int x;
    int z;
    u_short route;
    u_char* direction;
    u_char* table;

    /* Both packed words are copied through u_int views. */
    position.word = *(u_int*)&state->tile;
    *(u_int*)routes = (*(u_int**)((uintptr_t)&Battle_Ai_FrameState()->roomData))[area * 16 + index];
    if (!(Battle_Ai_RouteEndpointPair(&routes) & 0x1BE0)) {
        return 6;
    }
    state->area = area;
    count = 1;
    if (Battle_Ai_RouteEndpointPair(&routes) & 0x1BE00000) {
        count = 2;
    }

    for (i = 0; i < count; i++) {
        if (!i) {
            route = Battle_Ai_RouteEndpointValue(&routes, 0);
        } else {
            route = Battle_Ai_RouteEndpointValue(&routes, 1);
        }
        if (((route >> 5) & 31) != position.tile.tileX || (route & 31) != position.tile.tileZ) {
            continue;
        }
        candidate = position;
        if (route & 0x400) {
            if (route & 0x8000) {
                continue;
            }
            result = Battle_Ai_FindRouteGateAtTile((intptr_t)state, position.word, 1);
            if (result >= 0 && ((int (*)(int))Field_MovingPlatformDwellTicks)(i = result >> 1) >= 0) {
                candidate.tile.tag = i + 2;
                if (!((((Battle_Ai_RouteGateView*)Battle_Ai_roomAiState)->closedGates >> candidate.tile.tag)
                        & 1)) {
                    state->gate = candidate.tile.tag;
                    if (state->actorId) {
                        Battle_Ai_SetAiContactOccupancy(candidate.tile.tag);
                    }
                    Battle_Ai_ResolveRouteGateExit((Battle_Ai_RouteGateTile*)&candidate, &state->exit);
                    goto accept;
                }
            }
            candidate.tile.tileX = (route >> 5) & 31;
            candidate.tile.tileZ = route & 31;
            goto center;
        }
        if (route & 0x6000) {
            u_char* step;

            state->area = Battle_Ai_NavigationTileFlags(candidate.tile.tileZ, candidate.tile.tileX) & 15;
            state->opaque5 = 0;
            amount = ((route >> 13) & 3) + 1;
            step = &Battle_Ai_routeStepDirections[(route >> 11) & 3];
            candidate.tile.tileX += amount * ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[*step].dx;
            candidate.tile.tileZ += amount * ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[*step].dz;
            result = Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, candidate.word);
            if (result < 0) {
            center:
                if (state->movementTarget != 16
                    && ((Battle_Ai_RouteActorView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[state->movementTarget]) {
                    state->moveX =
                        ((Battle_Ai_RouteActorView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[state->movementTarget]->position[0];
                    state->moveZ =
                        ((Battle_Ai_RouteActorView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[state->movementTarget]->position[2];
                } else {
                    state->moveX = candidate.tile.tileX * 128 + 64;
                    state->moveZ = candidate.tile.tileZ * 128 + 64;
                }
                state->angle =
                    Core_Gte_Atan2(state->x - state->moveX, state->z - state->moveZ) & 0xFFF;
                state->cooldown = state->period;
                return 4;
            }
            if (result <= 0) {
                continue;
            }
        stop:
            candidate.tile.tag = 0;
        accept:
            state->nextTile = candidate;
            return 14;
        }
        table = Battle_Ai_routeStepDirections;
        direction = &table[(route >> 11) & 3];
        candidate.tile.tileX += ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[*direction].dx;
        candidate.tile.tileZ += ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[*direction].dz;
        if (Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, candidate.word) <= 0) {
            goto center;
        }
        if ((*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->heightMap))[candidate.tile.tileZ][candidate.tile.tileX] & 0x1C00) {
            goto stop;
        }
        if (((*(u_char**)((uintptr_t)&Battle_Ai_FrameState()->terrainRegions))[position.tile.tileZ * *(u_char*)((uintptr_t)&Battle_Ai_FrameState()->roomBounds) + position.tile.tileX]
                >> *direction)
            & 1) {
            goto stop;
        }
        state->heading = *direction;
        return 5;
    }

    if ((Battle_Ai_RouteEndpointPair(&routes) & 0x8400) == 0x8400) {
        if (count == 1) {
        areaOnly:
            return Battle_Ai_SetNavigationAreaFromTile(state, position);
        }
        if ((Battle_Ai_RouteEndpointPair(&routes) & 0x84000000) == 0x84000000) {
            goto areaOnly;
        }
        goto second;
    }
    if ((Battle_Ai_RouteEndpointPair(&routes) & 0x84000000) == 0x84000000) {
        goto first;
    }

    for (i = 0; i < count; i++) {
        if (!i) {
            route = Battle_Ai_RouteEndpointValue(&routes, 0);
        } else {
            route = Battle_Ai_RouteEndpointValue(&routes, 1);
        }
        if (!((*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->heightMap))[route & 31][(route >> 5) & 31] & 0x1C00)) {
            continue;
        }
        if (i) {
            route = Battle_Ai_RouteEndpointValue(&routes, 0);
        } else {
            route = Battle_Ai_RouteEndpointValue(&routes, 1);
        }
        if (((*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->heightMap))[route & 31][(route >> 5) & 31] & 0x1C00)
            || count == 1) {
            state->area = (*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[position.tile.tileZ][position.tile.tileX] % 16;
            return 6;
        }
        state->destination.word = ((route & 31) << 16) | ((route >> 5) & 31);
        return 6;
    }

    x = position.tile.tileX;
    if (count == 1) {
        if (((state->destination.word = ((Battle_Ai_RouteEndpointValue(&routes, 0) & 31) << 16)
                                     | ((Battle_Ai_RouteEndpointPair(&routes) >> 5) & 31))
                & 0xFF00FF)
            == (state->tile.word & 0xFF00FF)) {
            goto center;
        }
        return 6;
    }
    z = position.tile.tileZ;
    distances[0] =
        (x - ((Battle_Ai_RouteEndpointPair(&routes) >> 5) & 31)) * (x - ((Battle_Ai_RouteEndpointPair(&routes) >> 5) & 31))
        + (z - (Battle_Ai_RouteEndpointValue(&routes, 0) & 31)) * (z - (Battle_Ai_RouteEndpointValue(&routes, 0) & 31));
    x -= (Battle_Ai_RouteEndpointPair(&routes) >> 21) & 31;
    z -= (Battle_Ai_RouteEndpointPair(&routes) >> 16) & 31;
    distances[1] = x * x + z * z;
    if (distances[0] < distances[1]) {
    first:
        state->destination.word =
            ((Battle_Ai_RouteEndpointValue(&routes, 0) & 31) << 16) | ((Battle_Ai_RouteEndpointPair(&routes) >> 5) & 31);
    } else {
    second:
        state->destination.word =
            (Battle_Ai_RouteEndpointPair(&routes) & 0x1F0000) | ((Battle_Ai_RouteEndpointPair(&routes) >> 21) & 31);
    }
    if ((state->destination.word & 0xFF00FF) == (state->tile.word & 0xFF00FF)) {
        goto center;
    }
    return 6;
}

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1512
#define TERRAIN_TILES (*(unsigned short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))
// decomp: BATTLE.PRG func_800E19FC (src/BATTLE/BATTLE.PRG/771A8.c:1540)
void Battle_Ai_UpdateNavigationAreaMovement(Battle_Ai_TerrainMoveView* state)
{
    int current, next, mode;
    int active = 1;
    state->opaque4 = 0;
    state->opaque5 = active;
    if (state->opaque1A) {
        Battle_Ai_MoveWithHeightFilteredDirections(state);
        return;
    }
    current = TERRAIN_TILES[state->tile.tile.tileZ][state->tile.tile.tileX] & 15;
    next = TERRAIN_TILES[state->destination.tile.tileZ][state->destination.tile.tileX] & 15;
    if (current != next) {
        if (state->settingsBit14) {
            mode = Battle_Ai_ResolveNavigationAreaRoute((struct Battle_Ai_AreaRouteView*)state, current, next);
            if (mode == 6) {
                state->moveX = (state->destination.tile.tileX << 7) + 64;
                state->moveZ = (state->destination.tile.tileZ << 7) + 64;
                if ((TERRAIN_TILES[state->destination.tile.tileZ][state->destination.tile.tileX] & 15)
                    == (TERRAIN_TILES[state->tile.tile.tileZ][state->tile.tile.tileX] & 15)) {
                    goto move;
                }
            } else {
                Battle_Ai_RefreshAiMovementCooldown(state);
                if (mode == 5) {
                    Battle_Ai_BeginAiGroundMovement(state, state->angle << 9);
                } else {
                    Battle_Ai_IssueActorCommand((void*)state, mode);
                }
                return;
            }
        }
        state->isStuck = active;
        *(unsigned char*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
    }
move:
    Battle_Ai_ResolveDirectionalMovement((void*)state,
        Core_Gte_Atan2(state->position[0] - state->moveX, state->position[2] - state->moveZ)
            & 0xFFF,
        0);
}
#undef TERRAIN_TILES

// decomp: BATTLE.PRG func_800E2F5C (src/BATTLE/BATTLE.PRG/771A8.c:2388)
int Battle_Ai_FollowObstacleDetour(Battle_Ai_MovementPathView* state, Battle_Ai_PathTest test, int mask)
{
    Battle_Ai_DetourWord path;
    u_int direction;
    int heading;
    int blocker;
    int i;
    int cell;
    int turn;
    int x;
    int z;
    int boundX;
    int boundZ;
    int center;
    int hit;
    Battle_Ai_TilePosition ahead;
    Battle_Ai_TilePosition point;

    path = state->path;
    state->opaque5 = 0;
    state->opaque1 = state->opaque199;
    if (path.bits.steps == 0) {
        goto done;
    }

    heading = path.bits.heading;
    direction = (path.bits.clockwise ? heading - 2 : heading + 2) & 7;
    hit = Battle_Ai_DirectionalBlockingActor(state, direction);
    blocker = hit - 1;
    if (hit) {
        if (!(((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->movementFlags & 0x30)
            && !((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->waitTimer) {
            ((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->waitTimer = Battle_Ai_NextActorRandomMasked(4) + 4;
        }
    }

    if (!state->opaque4 && !path.bits.changed && !test(state, path.bits.heading)) {
        ahead.tile.tileX = state->tile.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[path.bits.heading].dx;
        ahead.tile.tileZ = state->tile.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[path.bits.heading].dz;
        ahead.tile.tag = 0;
        if (!Battle_Ai_CanTraverseTileEdge(
                (Battle_Ai_MoveTargetView*)state, state->tile.word, ahead.word, path.bits.heading)
            || Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, ahead.word) <= 0) {
            path.bits.changed = 1;
            path.bits.clockwise = !path.bits.clockwise;
            path.bits.visited = 0;
            path.bits.heading += 4;
        }
    }

    boundX = path.bits.boundX << 7;
    boundZ = path.bits.boundZ << 7;
    if (!state->opaque4) {
        x = state->tile.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dx;
        z = state->tile.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dz;
        if (((*(unsigned short (**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[z][x] >> mask) & 1
            || x > Battle_Ai_roomAiState->maxTileX || z > Battle_Ai_roomAiState->maxTileZ || x < Battle_Ai_roomAiState->minTileX
            || z < Battle_Ai_roomAiState->minTileZ) {
            state->opaqueE8 = 0;
            state->opaqueE7 = 0;
            state->opaqueE4 = 0;
            goto stop;
        }
        if (boundZ > 0) {
            center = (state->destination.tile.tileZ << 7) + 64;
            if ((center < boundZ && state->z < boundZ)
                || (boundZ < center && boundZ < state->z)) {
                goto done;
            }
        }
        if (boundX > 0) {
            center = (state->destination.tile.tileX << 7) + 64;
            if ((center < boundX && state->x < boundX)
                || (boundX < center && boundX < state->x)) {
                goto done;
            }
        }
    } else {
        if (boundZ > 0) {
            center = (state->destination.tile.tileZ << 7) + 64;
            if ((boundZ < center && state->z < boundZ)
                || (center < boundZ && boundZ < state->z)) {
                goto done;
            }
        }
        if (boundX > 0) {
            center = (state->destination.tile.tileX << 7) + 64;
            if ((boundX < center && state->x < boundX)
                || (center < boundX && boundX < state->x)) {
                goto done;
            }
        }
    }

    cell = Battle_Ai_CardinalPassabilityMask(state, test);
    if (cell == path.bits.cell) {
        goto keep;
    }
    path.bits.cell = cell;

    turn = -1;
    if (path.bits.clockwise) {
        turn = 1;
    }
    if (test(state, (path.bits.heading - turn * 2 - turn) & 7)
        && test(state, (path.bits.heading - turn * 2) & 7)) {
        if (!(state->movementFlags & 0x30) && !state->waitTimer) {
            state->waitTimer = Battle_Ai_NextActorRandomMasked(4) + 4;
        }
        goto stop;
    }

    path.bits.visited |= 1 << (path.bits.heading >> 1);
    if (path.bits.visited == 15) {
        goto stop;
    }

    direction = (path.bits.heading - turn * 2) & 7;
    for (i = 0; i < 4; i++) {
        if (test(state, direction)) {
            if (path.bits.heading != direction) {
                path.bits.changed = 1;
            }
            path.bits.heading = direction;
            goto keep;
        }
        point.tile.tileX = state->tile.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dx;
        point.tile.tileZ = state->tile.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[direction].dz;
        point.tile.tag = 0;
        /* Passed through its address: the status byte is left over from the previous
         * probe. */
        if (Battle_Ai_CanTraverseTileEdge(
                (Battle_Ai_MoveTargetView*)state, state->tile.word, *(u_int*)&point, direction)
            && Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, *(u_int*)&point) > 0) {
            state->nextTile = point;
            if (path.bits.heading != direction) {
                path.bits.changed = 1;
            }
            path.bits.heading = direction;
            goto detour;
        }
        direction = (direction + turn * 2) & 7;
    }
    state->path = path;
    return -1;

detour:
    state->path = path;
    return 14;

keep:
    state->path = path;
    return 5;

stop:
    path.bits.stop = 1;
done:
    state->path = path;
    return -1;
}

// decomp: BATTLE.PRG func_800E5154 (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
int Battle_Ai_ActorNavigationOverrideStatus(void)
{
    VS_UNPORTED("Battle::Ai::ActorNavigationOverrideStatus");
    return 0;
}

// decomp: BATTLE.PRG func_800E5158 (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
int Battle_Ai_HasActorNavigationState()
{
    VS_UNPORTED("Battle::Ai::HasActorNavigationState");
    return 0;
}

// decomp: BATTLE.PRG func_800E51C8 (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
int Battle_Ai_CopyActorNavigationOverrideState()
{
    VS_UNPORTED("Battle::Ai::CopyActorNavigationOverrideState");
    return 0;
}

// decomp: BATTLE.PRG func_800E527C (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
void Battle_Ai_SetActorNavigationOverride(int, int, int)
{
    VS_UNPORTED("Battle::Ai::SetActorNavigationOverride");
}

// decomp: BATTLE.PRG func_800E5308 (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
void Battle_Ai_UpdateActorNavigationOverrides(void)
{
    VS_UNPORTED("Battle::Ai::UpdateActorNavigationOverrides");
}

// decomp: BATTLE.PRG func_800E5EC0 (src/BATTLE/BATTLE.PRG/7CD68.c:760)
void Battle_Ai_FloodFillAiTerrainHeights(int arg0, int arg1, int arg2)
{
    Battle_Ai_DirectionDelta* step;
    int i;
    int value = arg2 & 0x3FF;

    Battle_Ai_specialTerrainHeights[arg1][arg0] = (Battle_Ai_specialTerrainHeights[arg1][arg0] & 0xFC00) | value | 0x2000;

    for (i = 0, step = Battle_Ai_directionDeltas; i < 4; ++i, step += 2) {
        u_int x = arg0 + step->dx;
        u_int z = arg1 + step->dz;

        if (x < 32 && z < 32 && (Battle_Ai_tileFlagMap[z][x] & 0x20)
            && !(Battle_Ai_specialTerrainHeights[z][x] & 0x2000)) {
            Battle_Ai_FloodFillAiTerrainHeights(x, z, arg2);
        }
    }
}

// decomp: BATTLE.PRG func_800E5FDC (src/BATTLE/BATTLE.PRG/7CD68.c:779)
int Battle_Ai_FindAdjacentAiGroundHeight(int arg0, int arg1)
{
    int i;
    u_int x;
    u_int z;

    for (i = 0; i < 4; i++) {
        x = arg0 + Battle_Ai_directionDeltas[i * 2].dx;
        z = arg1 + Battle_Ai_directionDeltas[i * 2].dz;
        if (x < 32 && z < 32 && !(Battle_Ai_tileFlagMap[z][x] & 0x70)) {
            return Field_FloorHeightAt(x << 7 | 0x40, z << 7 | 0x40) >> 6;
        }
    }
    return -1;
}

// decomp: BATTLE.PRG func_800E6098 (src/BATTLE/BATTLE.PRG/7CD68.c:795)
void Battle_Ai_BuildAiSpecialTerrainHeights(void)
{
    int x;
    int z;

    for (z = 0; z < 32; ++z) {
        for (x = 0; x < 32; ++x) {
            if ((Battle_Ai_tileFlagMap[z][x] & 0x20) && !(Battle_Ai_specialTerrainHeights[z][x] & 0x2000)) {
                int r = Battle_Ai_FindAdjacentAiGroundHeight(x, z);
                if (r >= 0) {
                    Battle_Ai_FloodFillAiTerrainHeights(x, z, r);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800E6158 (src/BATTLE/BATTLE.PRG/7CD68.c:814)
void Battle_Ai_InitializeRoomNavigation(void) { Battle_Ai_InitializeRoomAiMap(); }

// decomp: BATTLE.PRG func_800E6898 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_AddAiTileOccupant(void*)
{
    VS_UNPORTED("Battle::Ai::AddAiTileOccupant");
}

// decomp: BATTLE.PRG func_800E68A0 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_RemoveAiTileOccupant(Field_GimmickInstance*)
{
    VS_UNPORTED("Battle::Ai::RemoveAiTileOccupant");
}

// decomp: BATTLE.PRG func_800E68EC (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_MarkAiTileDynamicObstacle(int, int)
{
    VS_UNPORTED("Battle::Ai::MarkAiTileDynamicObstacle");
}

// decomp: BATTLE.PRG func_800E6A6C (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Battle_Ai_MinimumAiGroundHeightAlongSegment(int, int, int, int)
{
    VS_UNPORTED("Battle::Ai::MinimumAiGroundHeightAlongSegment");
    return 0;
}

// decomp: BATTLE.PRG func_800E6B4C (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Battle_Ai_BuildAiTerrainHeightsWithWorkspace()
{
    VS_UNPORTED("Battle::Ai::BuildAiTerrainHeightsWithWorkspace");
    return 0;
}

// decomp: BATTLE.PRG func_800E6EB0 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_ReleaseActorAiContactOccupancy(int)
{
    VS_UNPORTED("Battle::Ai::ReleaseActorAiContactOccupancy");
}

// decomp: BATTLE.PRG func_800E75EC (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Battle_Ai_RoomNavigationCeilingHeight(void)
{
    VS_UNPORTED("Battle::Ai::RoomNavigationCeilingHeight");
    return 0;
}

