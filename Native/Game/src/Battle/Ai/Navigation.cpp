// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"

// decomp: D_800F16EC (D_800F16EC@BATTLE/BATTLE.PRG)
Battle_Ai_DirectionDelta Battle_Ai_directionDeltas[8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F175C (D_800F175C@BATTLE/BATTLE.PRG)
u_char Battle_Ai_routeStepDirections[272]; /* contents: module image (ModuleData.cpp) */

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
        result = VS::Battle::Ai::CanTraverseLiveTileEdge(state, source.word, dest.word, direction);
        if (result > 0) {
            result = VS::Battle::Ai::LiveDestinationOccupancy(state, dest.word);
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
        dest.word = VS::Battle::Ai::FindLiveGapLandingTile(state, dest.word, direction, source.word);
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
            distance = VS::Core::Math::SquaredVectorLength(dx, state->y - VS::Field::GroundHeightAtWorldPosition(VS::Battle::Ai::CurrentTerrain(), x, z), dz);
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
            mode = VS::Battle::Ai::ResolveLiveNavigationAreaRoute((struct Battle_Ai_AreaRouteView*)state, current, next);
            if (mode == 6) {
                state->moveX = (state->destination.tile.tileX << 7) + 64;
                state->moveZ = (state->destination.tile.tileZ << 7) + 64;
                if ((TERRAIN_TILES[state->destination.tile.tileZ][state->destination.tile.tileX] & 15)
                    == (TERRAIN_TILES[state->tile.tile.tileZ][state->tile.tile.tileX] & 15)) {
                    goto move;
                }
            } else {
                VS::Battle::Ai::RefreshLiveAiMovementCooldown(state);
                if (mode == 5) {
                    VS::Battle::Ai::BeginLiveAiGroundMovement(state, state->angle << 9);
                } else {
                    VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
                }
                return;
            }
        }
        state->isStuck = active;
        *(unsigned char*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
    }
move:
    VS::Battle::Ai::ResolveLiveDirectionalMovement((Battle_Ai_ActorAiState *)((void*)state),
        VS::Core::Math::Arctangent2(state->position[0] - state->moveX, state->position[2] - state->moveZ)
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
    hit = VS::Battle::Ai::LiveDirectionalBlockingActor(state, direction);
    blocker = hit - 1;
    if (hit) {
        if (!(((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->movementFlags & 0x30)
            && !((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->waitTimer) {
            ((Battle_Ai_MovementPathView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[blocker]->waitTimer = VS::Battle::Ai::NextLiveActorRandomMasked(4) + 4;
        }
    }

    if (!state->opaque4 && !path.bits.changed && !test(state, path.bits.heading)) {
        ahead.tile.tileX = state->tile.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[path.bits.heading].dx;
        ahead.tile.tileZ = state->tile.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))[path.bits.heading].dz;
        ahead.tile.tag = 0;
        if (!VS::Battle::Ai::CanTraverseLiveTileEdge(
                (Battle_Ai_MoveTargetView*)state, state->tile.word, ahead.word, path.bits.heading)
            || VS::Battle::Ai::LiveDestinationOccupancy((Battle_Ai_MoveTargetView*)state, ahead.word) <= 0) {
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

    cell = VS::Battle::Ai::LiveCardinalPassabilityMask(state, test);
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
            state->waitTimer = VS::Battle::Ai::NextLiveActorRandomMasked(4) + 4;
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
        if (VS::Battle::Ai::CanTraverseLiveTileEdge(
                (Battle_Ai_MoveTargetView*)state, state->tile.word, *(u_int*)&point, direction)
            && VS::Battle::Ai::LiveDestinationOccupancy((Battle_Ai_MoveTargetView*)state, *(u_int*)&point) > 0) {
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

}  // extern "C"
