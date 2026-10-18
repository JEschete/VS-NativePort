// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800F58C4 (D_800F58C4@BATTLE/BATTLE.PRG)
int Battle_Ai_savedMovementRandomState;

// decomp: D_800F58D8 (D_800F58D8@BATTLE/BATTLE.PRG)
unsigned short (*Battle_Ai_patrolRoutes)[8];

// decomp: BATTLE.PRG historyRepeats (src/BATTLE/BATTLE.PRG/6FC00.c:960)
int Battle_Ai_HasRepeatingMovementHistory(Battle_Ai_PerceptionUpdateView* state)
{
    return state->historyX.words[0] == state->historyX.words[1]
        && state->historyZ.words[0] == state->historyZ.words[1];
}

// decomp: BATTLE.PRG historyStalled (src/BATTLE/BATTLE.PRG/6FC00.c:966)
int Battle_Ai_IsMovementHistoryStalled(Battle_Ai_PerceptionUpdateView* state)
{
    return state->historyMask == 0xFF ? Battle_Ai_HasRepeatingMovementHistory(state) : 0;
}

// decomp: BATTLE.PRG func_800D9538 (src/BATTLE/BATTLE.PRG/6FC00.c:1178)
void Battle_Ai_SaveMovementRandomState(void) { Battle_Ai_savedMovementRandomState = *(int*)((uintptr_t)Core_ScratchBase() + 0x3F8); }

// decomp: BATTLE.PRG D_800D9B6C (src/BATTLE/BATTLE.PRG/70D4C.s:458) [asm-entry: not yet ported]
int Battle_Ai_CanWalkInDirection(Battle_Ai_WanderView*, int)
{
    VS_UNPORTED("Battle::Ai::CanWalkInDirection");
    return 0;
}

// decomp: BATTLE.PRG func_800DB0BC (src/BATTLE/BATTLE.PRG/71618.c:1212)
int Battle_Ai_TryRecoverBlockedMovement(Battle_Ai_BlockedRecoveryView* state)
{
    u_char directions[4];
    u_int probe;
    u_int angle;
    u_int quadrant;
    int first;
    int second;
    int i;

    if (Battle_Ai_arenaOrbit == NULL) {
        if ((state->tile.word & 0xFF00FF) == (state->previousTile.word & 0xFF00FF)) {
            state->isStuck = 1;
            if (state->detourStep == 0) {
                angle = Core_Gte_Atan2(state->x - state->destination.tile.tileX * 128 - 64,
                    state->z - state->destination.tile.tileZ * 128 - 64);
                angle &= 0xFFF;
                first = (angle >> 9) & 7;
                second = ((angle >> 9) + 2) & 7;
                quadrant = angle >> 10;
                if (((angle & 0x3FF) > 512 ? ((quadrant + 1) & 3) * 2 : quadrant * 2)
                    == first) {
                    directions[0] = first;
                    directions[1] = second;
                } else {
                    directions[0] = second;
                    directions[1] = first;
                }
                if (Battle_Ai_NextActorRandomMasked(2)) {
                    directions[2] = (first + 4) & 7;
                    directions[3] = (second + 4) & 7;
                } else {
                    directions[2] = (second + 4) & 7;
                    directions[3] = (first + 4) & 7;
                }
                *(u_int*)state->detourDirections = *(u_int*)directions;
                probe = 0;
                for (i = 0; i < 4; ++i) {
                    ((u_char*)&probe)[0] = state->tile.tile.tileX
                                         + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[directions[i]].dx;
                    ((u_char*)&probe)[2] = state->tile.tile.tileZ
                                         + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[directions[i]].dz;
                    if (Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, probe) > 0) {
                        state->detourStep = i;
                        break;
                    }
                }
                state->detourStep++;
            }
            if (state->detourStep >= 5) {
                Battle_Ai_ForgetAiActionTarget((void*)state);
                state->opaque1C = 0;
                state->detourStep = 0;
                return 0;
            }
            state->destination.tile.tileX =
                state->tile.tile.tileX
                + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[state->detourDirections[state->detourStep
                                                                      - 1]]
                      .dx;
            state->destination.tile.tileZ =
                state->tile.tile.tileZ
                + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[state->detourDirections[state->detourStep
                                                                      - 1]]
                      .dz;
            state->movementTarget = state->opaqueFB;
            if (Battle_Ai_DestinationOccupancy((Battle_Ai_MoveTargetView*)state, state->destination.word) > 0) {
                Battle_Ai_DispatchAiLocomotionMode(state, 0, 0);
            } else {
                state->detourStep++;
            }
            return 1;
        }
        state->detourStep = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800DB370 (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
int Battle_Ai_ApproachActor(Battle_Ai_ActorAiState*, int, int, int)
{
    VS_UNPORTED("Battle::Ai::ApproachActor");
    return 0;
}

// decomp: BATTLE.PRG func_800DB4AC (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
int Battle_Ai_SetMovementBehavior()
{
    VS_UNPORTED("Battle::Ai::SetMovementBehavior");
    return 0;
}

// decomp: BATTLE.PRG func_800DB5B0 (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
void Battle_Ai_WanderAroundSavedCenter(void*)
{
    VS_UNPORTED("Battle::Ai::WanderAroundSavedCenter");
}

// decomp: BATTLE.PRG func_800DB5B8 (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
void Battle_Ai_WanderAroundTile(Battle_Ai_CandidateMovementView*, unsigned int, int)
{
    VS_UNPORTED("Battle::Ai::WanderAroundTile");
}

// decomp: BATTLE.PRG func_800DB5DC (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
void Battle_Ai_MoveToTile(Battle_Ai_CandidateMovementView*, unsigned int, int)
{
    VS_UNPORTED("Battle::Ai::MoveToTile");
}

// decomp: BATTLE.PRG func_800DB5FC (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
void Battle_Ai_PatrolWaypoints(void*)
{
    VS_UNPORTED("Battle::Ai::PatrolWaypoints");
}

// decomp: BATTLE.PRG func_800DB61C (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
int Battle_Ai_FleeVisibleEnemies(Battle_Ai_CandidateMovementView*, int)
{
    VS_UNPORTED("Battle::Ai::FleeVisibleEnemies");
    return 0;
}

// decomp: BATTLE.PRG func_800DB74C (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
int Battle_Ai_TrackActorPosition(Battle_Ai_ActorAiState*, int)
{
    VS_UNPORTED("Battle::Ai::TrackActorPosition");
    return 0;
}

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:794
#define OBSTRUCTION_CACHE ((Battle_Ai_ObstructionCache*)((uintptr_t)Core_ScratchBase() + 0x3D0))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:795
#define OBSTRUCTION_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:796
#define OBSTRUCTION_STATES ((Battle_Ai_ObstructionView**)((uintptr_t)Core_ScratchBase() + 0x37C))
// decomp: BATTLE.PRG func_800E02B4 (src/BATTLE/BATTLE.PRG/771A8.c:798)
int Battle_Ai_IsFlightDirectionObstructed(Battle_Ai_ObstructionView* state, int direction, int ahead)
{
    Battle_Ai_ObstructionVector points[4];
    int x, y, z, i, dx, dy, dz;
    unsigned int radius, height;
    Battle_Actor* actor;
    Battle_Ai_ObstructionView* other;
    Battle_Ai_ObstructionModel* model;
    if ((OBSTRUCTION_CACHE->tested >> direction) & 1) {
        return OBSTRUCTION_CACHE->result[direction];
    }
    x = state->position.x + ahead * (state->maxRange * OBSTRUCTION_DIRECTIONS[direction].dx);
    y = state->position.y;
    z = state->position.z + ahead * (state->maxRange * OBSTRUCTION_DIRECTIONS[direction].dz);
    i = Field_CollectMovingPlatformPositions((void*)points);
    for (i = i - 1; i != -1; i--) {
        dx = points[i].x - x;
        dy = points[i].y - y;
        dz = points[i].z - z;
        if (ahead) {
            if (OBSTRUCTION_DIRECTIONS[direction].dx > 0 && dx < 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dx < 0 && dx > 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dz > 0 && dz < 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dz < 0 && dz > 0) {
                continue;
            }
        }
        radius = state->radius + 96;
        height = (state->height + 32) >> 1;
        if ((unsigned int)ABS(dx) <= radius && (unsigned int)ABS(dz) <= radius
            && (unsigned int)ABS(dy) <= height) {
            OBSTRUCTION_CACHE->tested |= 1 << direction;
            OBSTRUCTION_CACHE->result[direction] = 1;
            return 1;
        }
    }
    for (actor = Battle_actors[0]; actor; actor = actor->next) {
        other = OBSTRUCTION_STATES[actor->id];
        model = other->model;
        if (other->actorId == state->actorId || !other->actorId) {
            continue;
        }
        dx = model->position.x - x;
        dy = model->position.y - y;
        dz = model->position.z - z;
        if (ahead) {
            if (OBSTRUCTION_DIRECTIONS[direction].dx > 0 && dx < 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dx < 0 && dx > 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dz > 0 && dz < 0) {
                continue;
            }
            if (OBSTRUCTION_DIRECTIONS[direction].dz < 0 && dz > 0) {
                continue;
            }
        }
        radius = other->radius + state->radius + other->maxRange;
        if (!other->actorId) {
            height = -1;
        } else {
            height = (state->height + other->height) >> 1;
        }
        if ((unsigned int)ABS(dx) <= radius && (unsigned int)ABS(dz) <= radius
            && (unsigned int)ABS(dy) <= height) {
            OBSTRUCTION_CACHE->tested |= 1 << direction;
            OBSTRUCTION_CACHE->result[direction] = actor->id + 1;
            return actor->id + 1;
        }
    }
    OBSTRUCTION_CACHE->tested |= 1 << direction;
    OBSTRUCTION_CACHE->result[direction] = 0;
    return 0;
}
#undef OBSTRUCTION_STATES
#undef OBSTRUCTION_DIRECTIONS
#undef OBSTRUCTION_CACHE

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:901
#define MOVEMENT_CHECK_CACHE ((Battle_Ai_DirectionCacheBits*)((uintptr_t)Core_ScratchBase() + 0x3D0))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:902
#define MOVEMENT_CHECK_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:903
#define MOVEMENT_CHECK_TILES (*(unsigned short(**)[32])((uintptr_t)Core_ScratchBase() + 0x3C0))
// decomp: BATTLE.PRG func_800E0678 (src/BATTLE/BATTLE.PRG/771A8.c:904)
int Battle_Ai_CanFlyInDirection(Battle_Ai_DirectionCacheView* state, int direction)
{
    int x, z;
    if ((MOVEMENT_CHECK_CACHE->tested >> direction) & 1) {
        return (MOVEMENT_CHECK_CACHE->valid & (1 << direction)) > 0;
    }
    if (Battle_Ai_IsFlightDirectionObstructed((void*)state, direction, 1)) {
        goto invalid;
    }
    x = state->tile.tile.tileX + MOVEMENT_CHECK_DIRECTIONS[direction].dx;
    z = state->tile.tile.tileZ + MOVEMENT_CHECK_DIRECTIONS[direction].dz;
    if (x > Battle_Ai_roomAiState->maxTileX || x < Battle_Ai_roomAiState->minTileX || z > Battle_Ai_roomAiState->maxTileZ
        || z < Battle_Ai_roomAiState->minTileZ) {
        goto invalid;
    }
    if (MOVEMENT_CHECK_TILES[z][x] & 0x40) {
        goto invalid;
    }
    if (state->previousTile.tile.tileX == x && state->previousTile.tile.tileZ == z) {
        goto invalid;
    }
    if (Battle_Ai_roomAiState->flightCeiling
        && Field_GroundHeightAtTile(x, z) + state->flightHeight < Battle_Ai_roomAiState->flightCeiling) {
        return 0;
    }
    MOVEMENT_CHECK_CACHE->tested |= 1 << direction;
    MOVEMENT_CHECK_CACHE->valid |= 1 << direction;
    return 1;
invalid:
    MOVEMENT_CHECK_CACHE->tested |= 1 << direction;
    MOVEMENT_CHECK_CACHE->valid &= ~(1 << direction);
    return 0;
}
#undef MOVEMENT_CHECK_TILES
#undef MOVEMENT_CHECK_DIRECTIONS
#undef MOVEMENT_CHECK_CACHE

// decomp: BATTLE.PRG func_800E0850 (src/BATTLE/BATTLE.PRG/771A8.c:942)
int Battle_Ai_LowestReachableGroundHeight(Battle_Ai_ActorAiState* arg0, u_int arg1)
{
    int min = 0;
    int count = 1;

    if (arg1 & 0x3FF) {
        count = 3;
    }
    arg1 = (arg1 >> 9) & 6;
    while (--count != -1) {
        if (Battle_Ai_CanFlyInDirection((void*)arg0, arg1) != 0) {
            int h = Field_GroundHeightAtTile(arg0->tile.word + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[arg1].dx,
                arg0->tile.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[arg1].dz);
            if (h < min) {
                min = h;
            }
        }
        arg1 = (arg1 + 1) & 7;
    }
    return min;
}

// decomp: BATTLE.PRG func_800E0918 (src/BATTLE/BATTLE.PRG/771A8.c:985)
int Battle_Ai_UpdateFlightGroundTransition(Battle_Ai_MovementCheckView* state, int action, int mode)
{
    void (*callback)(Battle_Ai_MovementCheckView*, int);
    unsigned int distance;
    if (mode) {
        callback = (void (*)(Battle_Ai_MovementCheckView*, int))Battle_Ai_MoveActorTowardDestinationTile;
    } else {
        callback = (void (*)(Battle_Ai_MovementCheckView*, int))Battle_Ai_BeginAiGroundMovement;
    }
    if (state->movementFlags & 6) {
        distance = Core_Math_SquaredTileDistance(state->destination, state->tile);
        if (mode && distance < 16) {
            if (!(state->model->opaque8 & 0x200000)) {
                callback(state, action);
                return 1;
            }
        } else if (Battle_Ai_roomAiState->flightCeiling) {
            if (Field_GroundHeightAtPackedTile(state->tile) + state->flightHeight < Battle_Ai_roomAiState->flightCeiling) {
                if (state->model->opaque8 & 0x200000) {
                    if (Battle_Ai_DestinationOccupancy((void*)state, state->tile) > 0) {
                        Battle_Ai_ReleaseActorAiReservations((void*)state);
                        Battle_Ai_BeginAiTerrainMovement(state);
                        return 1;
                    }
                } else {
                    callback(state, action);
                    return 1;
                }
            }
        }
    }
    return 0;
}

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1062
#define MOVEMENT_STEP_TILES (*(unsigned short(**)[32])((uintptr_t)Core_ScratchBase() + 0x3C0))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1063
#define MOVEMENT_STEP_RESULT (*(int*)((uintptr_t)Core_ScratchBase() + 0x3C8))
// decomp: BATTLE.PRG func_800E0A68 (src/BATTLE/BATTLE.PRG/771A8.c:1064)
void Battle_Ai_UpdateFlightMovement(Battle_Ai_MovementStepView* state, int action)
{
    int step;
    int mode;
    int angle;
    int x;
    int z;
    int ground;
    int height;
    int dy;
    int px;
    int py;
    int pz;
    int top;
    unsigned int distance;
    unsigned int denom;
    unsigned int square;
    int value;
    int diff;
    int offset;
    Battle_Ai_StepActorView* actor;

    if (Battle_Ai_UpdateFlightGroundTransition((void*)state, action, 1)) {
        return;
    }
    if (!(state->model->opaque8 & 0x200000)) {
        Battle_Ai_BeginAiAlternateTerrainMovement((void*)state);
        return;
    }
    state->movementDisabled = 1;
    if (!Battle_Ai_SelectActorVerticalMovementStep((void*)state, &step, &mode)) {
        goto recover;
    }
    x = state->destination.tile.tileX * 128 + 64;
    z = state->destination.tile.tileZ * 128 + 64;
    angle = Core_Gte_Atan2(state->position.tile.positionX - x, state->z - z);
    angle &= 0xFFF;
    if (state->opaque0 && Core_Math_SquaredTileDistance(state->tile.word, state->destination.word) < 2
        && (MOVEMENT_STEP_TILES[state->destination.tile.tileZ][state->destination.tile.tileX] & 0x40)) {
        int centerZ;
        int centerAngle;
        int selectedHeight;
        int heading;
        int rise;
        int offsetX;
        int offsetZ;
        int actorY;
        actorY = ((Battle_Ai_StepActorView*)Actors_actorRecords[state->actorId])->y;
        rise = Field_GroundHeightAtPackedTile(state->tile.word);
        if (actorY >= rise) {
            selectedHeight = rise + state->flightHeight;
        } else if (Battle_Ai_roomAiState->flightCeiling && Battle_Ai_roomAiState->flightCeiling >= actorY) {
            selectedHeight = Battle_Ai_roomAiState->flightCeiling;
        } else {
            selectedHeight = (short)actorY;
        }
        rise = selectedHeight - state->position.tile.positionY;
        if (ABS(rise) < step) {
            angle = state->tile.tile.tileX * 128 + 64;
            centerZ = state->tile.tile.tileZ * 128 + 64;
            centerAngle = Core_Gte_Atan2(state->position.tile.positionX - angle, state->z - centerZ);
            state->moveY = selectedHeight << 12;
            heading = (-centerAngle + ONE * 3 / 4) & 0xFFF;
            offsetX = state->position.tile.positionX - angle;
            if (ABS(offsetX) < step) {
                state->moveX = angle << 12;
            } else {
                state->moveX = (state->position.tile.positionX << 12) + step * Core_Gte_Cos(heading);
            }
            offsetZ = state->z - centerZ;
            state->moveZ = ABS(offsetZ) < step ? centerZ << 12
                                               : (state->z << 12) + step * Core_Gte_Sin(heading);
            Battle_Ai_IssueActorCommand((void*)state, 8);
            return;
        }
        state->moveY = rise;
        state->moveZ = 0;
        state->moveX = 0;
        if (state->moveY > 0 && Battle_Ai_IsFlightDirectionObstructed((void*)state, 0, 0)) {
            state->moveY = 0;
        }
        Battle_Ai_IssueActorCommand((void*)state, mode);
        return;
    }
    if ((state->tile.word & 0xFF00FF) == (state->destination.word & 0xFF00FF)) {
        int floorY;
        int tileX;
        int deltaX;
        int tileZ;
        int deltaZ;
        if (!state->opaque0 || Battle_Ai_IsFlightDirectionObstructed((void*)state, 0, 0)) {
        recover:
            Battle_Ai_UpdateMovementRecovery((void*)state);
            return;
        }
        floorY = state->flightHeight;
        actor = (Battle_Ai_StepActorView*)Actors_actorRecords[state->actorId];
        if (!(MOVEMENT_STEP_TILES[state->destination.tile.tileZ][state->destination.tile.tileX]
                & 0x20)) {
            floorY += Field_GroundHeightAtPackedTile(state->destination.word);
        }
        tileX = state->destination.tile.tileX * 128;
        deltaX = state->position.tile.positionX - 64;
        state->moveX = tileX - deltaX;
        tileZ = state->destination.tile.tileZ * 128;
        deltaZ = state->z - 64;
        state->moveZ = tileZ - deltaZ;
        state->moveY = floorY - state->position.tile.positionY;
        if ((state->destination.word & 0xFF00FF) == (actor->tile.word & 0xFF00FF)) {
            state->moveY = actor->y - (actor->height >> 1) - state->position.tile.positionY;
        }
        if (state->moveY > 0 && floorY < state->position.tile.positionY + step) {
            state->moveY = 0;
        }
        if (ABS(state->moveX) < step) {
            state->moveX = 0;
        }
        if (ABS(state->moveZ) < step) {
            state->moveZ = 0;
        }
        Battle_Ai_IssueActorCommand((void*)state, mode);
        return;
    }
    Battle_Ai_ResolveDirectionalMovement((void*)state, angle, 2);
    switch (MOVEMENT_STEP_RESULT) {
    case 5:
        angle = state->heading * (ONE / 8);
        if (Battle_Ai_LowestReachableGroundHeight((void*)state, angle) + state->flightHeight < state->position.tile.positionY) {
        fall:
            state->moveY = -ONE;
            state->moveZ = 0;
            state->moveX = 0;
        } else {
            angle = -angle;
            angle += ONE * 3 / 4;
            angle &= 0xFFF;
            state->moveY = 0;
            state->moveX = Core_Gte_Cos(angle);
            state->moveZ = Core_Gte_Sin(angle);
        }
        Battle_Ai_IssueActorCommand((void*)state, mode);
        return;
    case 7:
        height = Field_GroundHeightAtPackedTile(state->destination.word) + state->flightHeight;
        px = state->position.word & 0xFFFF;
        py = (int)state->position.word >> 16;
        pz = state->z;
        ground = Battle_Ai_MinimumAiGroundHeightAlongSegment(px, pz, x, z) + state->groundClearance;
        if (Battle_Ai_roomAiState->flightCeiling) {
            ground = ground < Battle_Ai_roomAiState->flightCeiling ? Battle_Ai_roomAiState->flightCeiling : ground;
        }
        top = Battle_Ai_LowestReachableGroundHeight((void*)state, angle) + state->flightHeight;
        ground = top > ground ? ground : top;
        distance = Core_Math_SquareRootInteger(Core_Math_SquaredVectorLength(x - px, 0, z - pz));
        value = height + ground;
        denom = Core_Math_SquareRootInteger(((value << 12) / value) << 12) + ONE;
        if (!denom) {
            goto fallback;
        }
        value = (distance << 12) / denom;
        square = value * value;
        diff = py - ground;
        offset = ABS(diff);
        if (py + step < ground) {
            if (step < height - py) {
                state->moveY = height - py;
            } else {
                state->moveY = 0;
            }
            state->moveX = x - px;
            state->moveZ = z - pz;
            Battle_Ai_IssueActorCommand((void*)state, mode);
            state->stallCounter = 0;
            return;
        }
        if ((unsigned int)step >= distance) {
        fallback:
            Battle_Ai_UpdateFlightDirectionalMovement((void*)state, angle);
            return;
        }
        height =
            (unsigned int)(offset * (step - value) * (step - value)) / square + ground;
        angle = -angle;
        angle += ONE * 3 / 4;
        angle &= 0xFFF;
        x = (px << 12) + step * Core_Gte_Cos(angle);
        z = (pz << 12) + step * Core_Gte_Sin(angle);
        if (height < ground) {
            height = ground;
        } else if (top < height) {
            if (ground < py) {
                goto fall;
            }
            height = top;
        }
        dy = height - py;
        state->stallCounter = 0;
        if (step < ABS(dy)) {
            state->moveX = (x >> 12) - px;
            state->moveZ = (z >> 12) - pz;
            state->moveY = dy;
            Battle_Ai_IssueActorCommand((void*)state, mode);
        } else {
            state->moveX = x;
            state->moveY = height << 12;
            state->moveZ = z;
            Battle_Ai_IssueActorCommand((void*)state, 8);
        }
        return;
    }
}
#undef MOVEMENT_STEP_RESULT
#undef MOVEMENT_STEP_TILES

// decomp: BATTLE.PRG func_800E1238 (src/BATTLE/BATTLE.PRG/771A8.c:1278)
void Battle_Ai_UpdateFlightDirectionalMovement(Battle_Ai_ActorAiState* arg0, int arg1)
{
    int sp10;
    int sp14;
    int angle;
    int c;
    int s;

    if (Battle_Ai_UpdateFlightGroundTransition((void*)arg0, arg1, 0) != 0) {
        return;
    }

    if (Battle_Ai_SelectActorVerticalMovementStep(arg0, &sp10, &sp14) == 0) {
        Battle_Ai_UpdateMovementRecovery((void*)arg0);
        return;
    }

    arg0->movementDisabled = 1;
    Battle_Ai_ResolveDirectionalMovement(arg0, arg1, 3);

    if (*(int*)((uintptr_t)Core_ScratchBase() + 0x3C8) != 5 && *(int*)((uintptr_t)Core_ScratchBase() + 0x3C8) != 7) {
        Battle_Ai_ReleaseActorAiReservations(arg0);
        Battle_Ai_UpdateMovementRecovery((void*)arg0);
        return;
    }

    if (*(int*)((uintptr_t)Core_ScratchBase() + 0x3C8) == 5) {
        arg1 = arg0->heading * (ONE / 8);
    }

    angle = (-arg1 + ONE * 3 / 4) & 0xFFF;
    c = Core_Gte_Cos(angle);
    s = Core_Gte_Sin(angle);

    if (Battle_Ai_LowestReachableGroundHeight(arg0, arg1) + arg0->flightHeight < arg0->position.vy) {
        arg0->moveY = -ONE;
        arg0->moveZ = 0;
        arg0->moveX = 0;
    } else {
        arg0->moveY = 0;
        arg0->moveX = c;
        arg0->moveZ = s;
    }

    if (!(arg0->model->opaqueABit5 << 21)) {
        Battle_Ai_BeginAiAlternateTerrainMovement(arg0);
    } else {
        arg0->stallCounter = 0;
        Battle_Ai_IssueActorCommand(arg0, sp14);
    }
}

// decomp: BATTLE.PRG func_800E1388 (src/BATTLE/BATTLE.PRG/771A8.c:1330)
void Battle_Ai_ProjectClearRadialDestination(Battle_Ai_MoveTargetView* state, Battle_Ai_TilePosition source, int angle,
    unsigned int distance)
{
    Battle_Ai_TilePosition dest;
    int i, x, z, heading;
    unsigned int step;
    angle = -angle;
    angle += 0xC00;
    heading = angle & 0xFFF;
    step = distance >> 3;

    for (i = 0; i < 8; i++, distance -= step) {
        x = ((int)(distance * Core_Gte_Cos(heading)) >> 12) + 64;
        dest.tile.tileX = (int)(source.tile.tileX * 128 + x) >> 7;
        z = ((int)(distance * Core_Gte_Sin(heading)) >> 12) + 64;
        dest.tile.tileZ = (int)(source.tile.tileZ * 128 + z) >> 7;
        if (Battle_Ai_DestinationOccupancy(state, dest.word) > 0) {
            break;
        }
    }
    if (dest.tile.tileX < Battle_Ai_roomAiState->minTileX) {
        dest.tile.tileX = Battle_Ai_roomAiState->minTileX;
    } else if (dest.tile.tileX > Battle_Ai_roomAiState->maxTileX) {
        dest.tile.tileX = Battle_Ai_roomAiState->maxTileX;
    }
    if (dest.tile.tileZ < Battle_Ai_roomAiState->minTileZ) {
        dest.tile.tileZ = Battle_Ai_roomAiState->minTileZ;
    } else if (dest.tile.tileZ > Battle_Ai_roomAiState->maxTileZ) {
        dest.tile.tileZ = Battle_Ai_roomAiState->maxTileZ;
    }
    state->destination = dest;
}

// decomp: BATTLE.PRG func_800E153C (src/BATTLE/BATTLE.PRG/771A8.c:1389)
void Battle_Ai_MoveToRadialDestination(Battle_Ai_MovementDestinationView* state, int unused)
{
    int distance = state->moveDistance;
    int angle, i;
    Battle_Ai_TilePosition dest;
    if ((state->tile.word & 0xFF00FF) == (state->destination.word & 0xFF00FF)) {
        goto fallback;
    }
    angle = Core_Gte_Atan2(state->x - state->destination.tile.tileX * 128 - 64,
        state->z - state->destination.tile.tileZ * 128 - 64);
    if (Core_Math_SquaredVectorLength(state->destination.tile.tileX * 128 - state->x + 64, 0,
            state->destination.tile.tileZ * 128 - state->z + 64)
        > (unsigned int)(distance * distance)) {
        Battle_Ai_ProjectClearRadialDestination((void*)state, state->tile, angle, distance);
    }
    if ((state->movementFlags & 6)
        && (state->opaqueD
            || Core_Math_SquaredTileDistance(state->destination.word, state->tile.word) < 16)) {
        if ((int)(state->savedDestination.word << 8) >= 0) {
            state->destination = state->savedDestination;
        }
        Battle_Ai_MoveActorTowardDestinationTile(state, angle);
        return;
    }
    if (++state->moveAttempts < 6) {
        goto fallback;
    }
    dest = state->destination;
    if (Battle_Ai_DestinationOccupancy((void*)state, dest.word) > 0) {
        goto begin;
    }
    for (i = 0; i < 4; i++) {
        dest.tile.tileX = state->destination.tile.tileX + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[i * 2].dx;
        dest.tile.tileZ = state->destination.tile.tileZ + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[i * 2].dz;
        if (Battle_Ai_DestinationOccupancy((void*)state, dest.word) > 0) {
            state->destination = dest;
            goto begin;
        }
    }
    goto fallback;
begin:
    state->moveAttempts = 0;
    state->nextTile = state->destination;
    Battle_Ai_IssueActorCommand((void*)state, 13);
    return;
fallback:
    Battle_Ai_UpdateMovementRecovery((void*)state);
}

// decomp: BATTLE.PRG func_800E1764 (src/BATTLE/BATTLE.PRG/771A8.c:1437)
void Battle_Ai_MoveAwayFromReference(Battle_Ai_MovementDestinationView* state, int angle)
{
    unsigned int previous[1];
    unsigned int distance;
    int opposite = (angle + 0x800) & 0xFFF;
    Battle_Ai_ProjectClearRadialDestination((void*)state, state->tile, angle, state->moveDistance);
    previous[0] = state->destination.word;
    Battle_Ai_ProjectClearRadialDestination((void*)state, state->tile, opposite, state->moveDistance);
    distance = Core_Math_SquaredVectorLength(state->destination.tile.tileX * 128 - state->moveX + 64, 0,
        state->destination.tile.tileZ * 128 - state->moveZ + 64);
    if (distance
        < Core_Math_SquaredVectorLength(((unsigned char*)&previous)[0] * 128 - state->moveX + 64, 0,
            ((unsigned char*)&previous)[2] * 128 - state->moveZ + 64)) {
        state->destination.word = previous[0];
    }
    Battle_Ai_MoveToRadialDestination(state, angle);
}

// decomp: BATTLE.PRG func_800E1850 (src/BATTLE/BATTLE.PRG/771A8.c:1455)
u_char Battle_Ai_FilterDirectionsByGroundHeight(u_char arg0, u_int arg1, int arg2)
{
    int x = arg1 & 0xFF;
    int z = (arg1 >> 16) & 0xFF;
    int i;

    for (i = 0; i < 8; ++i) {
        if ((arg0 >> i) & 1) {
            if (Field_GroundHeightAtTile(x + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[i].dx,
                    z + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[i].dz)
                > arg2) {
                arg0 &= ~(1 << i);
            }
        }
    }
    return arg0;
}

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1509
#define TERRAIN_TILE_MASK (*(unsigned char**)((uintptr_t)Core_ScratchBase() + 0x3C4))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1510
#define TERRAIN_MAP_WIDTH (*(unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3DC))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1511
#define TERRAIN_MOVE_RESULT (*(int*)((uintptr_t)Core_ScratchBase() + 0x3C8))
// decomp: BATTLE.PRG func_800E1908 (src/BATTLE/BATTLE.PRG/771A8.c:1517)
void Battle_Ai_MoveWithHeightFilteredDirections(Battle_Ai_TerrainMoveView* state)
{
    unsigned char* tile =
        &TERRAIN_TILE_MASK[state->tile.tile.tileZ * TERRAIN_MAP_WIDTH + state->tile.tile.tileX];
    int old = *tile;
    Battle_Ai_TerrainMoveActorView* actor = state->actor;
    int mode;
    *tile = Battle_Ai_FilterDirectionsByGroundHeight(old, state->tile.word, state->position[1]);
    Battle_Ai_ResolveDirectionalMovement((void*)state,
        Core_Gte_Atan2(state->position[0] - state->moveX, state->position[2] - state->moveZ)
            & 0xFFF,
        0);
    *tile = old;
    mode = TERRAIN_MOVE_RESULT;
    if ((0xA0 >> mode) & 1) {
        actor->commandOpcode = 9;
        if (mode == 5) {
            actor->commandHeading = state->angle << 9;
        } else {
            actor->commandHeading = state->angle;
        }
    }
}
#undef TERRAIN_MOVE_RESULT
#undef TERRAIN_MAP_WIDTH
#undef TERRAIN_TILE_MASK

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1627
#define WANDER_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))
// decomp: BATTLE.PRG wanderDirection (src/BATTLE/BATTLE.PRG/771A8.c:1633)
Battle_Ai_DirectionDelta* Battle_Ai_WanderDirectionOffset(int index)
{
    return WANDER_DIRECTIONS + index;
}
#undef WANDER_DIRECTIONS

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1627
#define WANDER_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1628
#define WANDER_TILES (*(unsigned short(**)[32])((uintptr_t)Core_ScratchBase() + 0x3C0))
// decomp: BATTLE.PRG func_800E1BB8 (src/BATTLE/BATTLE.PRG/771A8.c:1638)
int Battle_Ai_ChooseNextWanderTile(Battle_Ai_WanderView* state, unsigned int tile, unsigned int maxCount, int arg3)
{
    Battle_Ai_TilePosition candidate;
    int count = 0;
    int choices[4];
    int i;
    unsigned int chosen;

    if (!maxCount) {
        return -1;
    }
    if (!state->count) {
        state->goal.word = tile;
    }
    candidate = state->goal;
    for (i = 0; i < 4; i++) {
        if (!(state->movementFlags & 8)) {
            Battle_Ai_DirectionDelta offset;
            if (*(int*)((uintptr_t)Core_ScratchBase() + 0x3F4)) {
                candidate.tile.tag =
                    (*(unsigned char**)((uintptr_t)Core_ScratchBase() + 0x3C4))[state->goal.tile.tileZ
                                                       * (*(unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3DC))
                                                   + state->goal.tile.tileX]
                        >> i * 2
                    & 1;
            } else {
                candidate.tile.tag = Battle_Ai_IsAdjacentTileStepBlocked(state->goal.word, i * 2);
            }
            offset = *Battle_Ai_WanderDirectionOffset(i * 2);
            candidate.tile.tileX = state->goal.tile.tileX + offset.dx;
            candidate.tile.tileZ = state->goal.tile.tileZ + offset.dz;
            if (candidate.tile.tag) {
                if (!Battle_Ai_CanTraverseTileEdge(
                        (Battle_Ai_MoveTargetView*)state, state->goal.word, candidate.word, i * 2)) {
                    continue;
                }
                candidate.tile.tag = 0;
            }
            if (!state->isStuck
                && (WANDER_TILES[candidate.tile.tileZ][candidate.tile.tileX] & 15)
                       != (WANDER_TILES[state->goal.tile.tileZ][state->goal.tile.tileX] & 15)) {
                continue;
            }
        }
        if (!(WANDER_TILES[candidate.tile.tileZ][candidate.tile.tileX] & 32)) {
            choices[count++] = i * 2;
        }
    }
    if (!count) {
        return -1;
    }
    chosen = (unsigned int)Battle_Ai_NextActorRandomMasked(4096) % count;
    if (state->wanderHeading > 0 && ((state->wanderHeading + 4) & 7) == choices[chosen]) {
        chosen = (unsigned int)Battle_Ai_NextActorRandomMasked(4096) % count;
    }
    state->goal.tile.tileX += WANDER_DIRECTIONS[choices[chosen]].dx;
    state->goal.tile.tileZ += WANDER_DIRECTIONS[choices[chosen]].dz;
    state->wanderHeading = choices[chosen];
    state->count++;
    return maxCount < state->count;
}
#undef WANDER_TILES
#undef WANDER_DIRECTIONS

// decomp: BATTLE.PRG wanderRandomHeading (src/BATTLE/BATTLE.PRG/771A8.c:1700)
int Battle_Ai_RandomWanderHeading(int direction)
{
    int offset = Battle_Ai_NextActorRandomMasked(512);
    int angle = direction << 9;
    offset += 0xFF00;
    return angle + offset;
}

// decomp: BATTLE.PRG wanderStartWalk (src/BATTLE/BATTLE.PRG/771A8.c:1708)
void Battle_Ai_StartWanderWalk(Battle_Ai_WanderView* s)
{
    s->wanderHeading = -1;
    s->count = 0;
    s->wanderCooldown = 60;
    s->wanderPhase++;
    Battle_Ai_UpdateMovementRecovery((Battle_Ai_MovementRecoveryView*)s);
}

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1627
#define WANDER_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1628
#define WANDER_TILES (*(unsigned short(**)[32])((uintptr_t)Core_ScratchBase() + 0x3C0))
// decomp: BATTLE.PRG func_800E1EAC (src/BATTLE/BATTLE.PRG/771A8.c:1717)
void Battle_Ai_UpdateWanderMovement(Battle_Ai_WanderView* state, unsigned int tile, unsigned int range)
{
    unsigned short rest, move;
    int rate, i, heading, x, z, value, result;
    int (*check)(Battle_Ai_WanderView*, int);

    state->wanderTimer++;
    rate = state->wanderRate;
    rest = state->restPeriod;
    move = state->movePeriod;
    if (state->model->opaque8 & 0x200000) {
        check = (int (*)(Battle_Ai_WanderView*, int))Battle_Ai_CanFlyInDirection;
    } else {
        check = Battle_Ai_CanWalkInDirection;
    }
    if (state->behaviorMode == 1) {
        if (state->wanderTimer < 31) {
            return;
        }
        if ((unsigned int)Battle_Ai_NextActorRandomMasked(128) < 44) {
            state->behaviorMode = 2;
        } else {
            state->wanderTimer = 0;
            return;
        }
    }
    if (state->wanderTimer == move) {
        if ((unsigned int)Battle_Ai_NextActorRandomMasked(128) < 44) {
            state->wanderTimer = 0;
        }
    } else if (state->wanderTimer >= rest + move) {
        state->wanderTimer = (unsigned int)Battle_Ai_NextActorRandomMasked(128) < 44 ? move : 0;
    }
    state->isStuck = 1;
    *(unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3D0) = 0;
    value = state->wanderTimer < move;
    if (!value) {
        goto recover;
    }
    if (range >= 63) {
        if (!state->wanderPhase || !Battle_Ai_NextActorRandomMasked(1 << rate) || !state->wanderCooldown) {
            value = (unsigned int)Battle_Ai_NextActorRandomMasked(4096) % range + 128;
            state->wanderPhase = 1;
            state->wanderCooldown = 60;
            state->wanderRangeSquared = value * value;
            heading = Battle_Ai_NextActorRandomMasked(4) * 2;
            for (i = 0; i < 4; i++) {
                int direction = (heading + i * 2) & 7;
                if (check(state, direction)) {
                    state->wanderHeading = Battle_Ai_RandomWanderHeading(direction);
                    break;
                }
            }
            if (i == 4) {
                state->wanderHeading = Battle_Ai_NextActorRandomMasked(4096);
            }
        }
        if (!state->opaque8) {
            state->wanderCooldown = 60;
        } else if (state->wanderCooldown) {
            state->wanderCooldown--;
        }
        if (state->movementFlags & 8) {
            Battle_Ai_ProjectClearRadialDestination(
                (Battle_Ai_MoveTargetView*)state, state->tile, state->wanderHeading, state->moveDistance);
        }
        {
            int boundsHeading = state->wanderHeading >> 9;
            int boundsIndex;
            for (boundsIndex = 0; boundsIndex < 2;
                boundsHeading = (boundsHeading + 2) & 7, boundsIndex++) {
                x = state->tile.tile.tileX + WANDER_DIRECTIONS[boundsHeading].dx;
                z = state->tile.tile.tileZ + WANDER_DIRECTIONS[boundsHeading].dz;
                if (Battle_Ai_roomAiState->maxTileZ < z || z < Battle_Ai_roomAiState->minTileZ || Battle_Ai_roomAiState->maxTileX < x
                    || x < Battle_Ai_roomAiState->minTileX || (WANDER_TILES[z][x] & 0x800)) {
                    state->wanderCooldown = 0;
                    break;
                }
            }
        }
        Battle_Ai_DispatchAiLocomotionMode(state, 1, state->wanderHeading);
        return;
    }
    state->opaque3 = 1;
    if (!Battle_Ai_NextActorRandomMasked(1 << rate)) {
        state->wanderPhase = 0;
    }
    switch (state->wanderPhase) {
    case 0:
        Battle_Ai_StartWanderWalk(state);
        return;
    case 1:
        result = Battle_Ai_ChooseNextWanderTile(state, tile, range, 3);
        if (result < 0) {
            state->wanderPhase = 0;
        } else if (result > 0) {
            state->wanderPhase++;
            state->destination = state->goal;
        }
        break;
    case 2:
        state->destination = state->goal;
        if (state->movementFlags & 8) {
            heading = Core_Gte_Atan2(state->x - state->destination.tile.tileX * 128 - 64,
                          state->z - state->destination.tile.tileZ * 128 - 64)
                    & 4095;
            state->savedDestination = state->destination;
            Battle_Ai_ProjectClearRadialDestination((Battle_Ai_MoveTargetView*)state, state->tile, heading, state->moveDistance);
        }
        Battle_Ai_DispatchAiLocomotionMode(state, 0, 0);
        if (!state->opaque8) {
            state->wanderCooldown = 60;
        } else if (state->wanderCooldown) {
            state->wanderCooldown--;
        }
        if ((state->destination.word & 0xFF00FF) == (state->tile.word & 0xFF00FF)
            || !state->wanderCooldown) {
            state->wanderPhase = 0;
        }
        return;
    default:
        state->wanderPhase = 0;
    }
recover:
    Battle_Ai_UpdateMovementRecovery((Battle_Ai_MovementRecoveryView*)state);
}
#undef WANDER_TILES
#undef WANDER_DIRECTIONS

// decomp: BATTLE.PRG func_800E23AC (src/BATTLE/BATTLE.PRG/771A8.c:1863)
void Battle_Ai_UpdateWaypointPatrol(Battle_Ai_PatrolView* state)
{
    unsigned short* row;
    unsigned int entry;
    int step;
    row = Battle_Ai_patrolRoutes[state->row];
    entry = Battle_Ai_WaypointValue(row, state->index);
    if (state->tile.tile.tileX != (entry & 31) || state->tile.tile.tileZ != ((entry >> 5) & 31)) {
        if (state->opaque104) {
            state->opaque104--;
        }
        if (Battle_Ai_NextActorRandomMasked(1 << state->rate) || state->opaque104) {
            goto move;
        }
        state->direction = -state->direction;
    }
    step = state->direction + 8;
    do {
        state->index = (state->index + step) & 7;
    } while (!(Battle_Ai_WaypointValue(row, state->index) >> 15));
    state->opaque104 = 10;
move:
    state->destination.tile.tileX = Battle_Ai_WaypointValue(row, state->index) & 31;
    state->destination.tile.tileZ = (Battle_Ai_WaypointValue(row, state->index) >> 5) & 31;
    Battle_Ai_DispatchAiLocomotionMode(state, 0, 0);
}

// decomp: BATTLE.PRG func_800E24EC (src/BATTLE/BATTLE.PRG/771A8.c:1973)
int Battle_Ai_UpdateTargetApproachMovement(Battle_Ai_TargetApproachView* s, int option)
{
    int result = 1, target = s->targetId, mode, value, angle, distance, one, radius;
    unsigned int oldPosition;
    Battle_Ai_ApproachTargetRecordView* model;
    if (target == s->actorId) {
        Battle_Ai_IssueActorCommand((void*)s, 0);
        goto done;
    }
    model = (Battle_Ai_ApproachTargetRecordView*)Actors_actorRecords[target];
    if (s->model->opaque8 & 0x200000) {
        option = 1;
    }
    if ((!(model->opaque8 & 0x1F0000)
                ? ((model->x >> 6) != s->targetTileX || (model->z >> 6) != s->targetTileZ)
                : (s->destination.bytes[0] != (s->targetTileX >> 1)
                      || s->destination.bytes[2] != (s->targetTileZ >> 1)))
        || s->range != s->previousRange || target != s->previousTargetId
        || option != s->previousOption || s->opaqueF8++ > 180) {
        s->opaqueF8 = 0;
        s->opaqueE8 = 0;
        s->opaqueE7 = 0;
    }
    s->previousOption = option;
    s->previousTargetId = target;
    s->previousRange = s->range;
    if (s->relations[target].active) {
        s->opaqueE4 = 0;
        if (s->relations[target].opaque0Bit2) {
            mode = s->rangeMode;
            switch (mode) {
            case 0:
                if ((unsigned int)((s->range - 96) * (s->range - 96))
                    < s->distancesSquared[target]) {
                    s->rangeMode = 0;
                } else {
                    s->rangeMode = 1;
                }
                break;
            case 1:
                value = s->range;
                if ((unsigned int)(value * value) < s->distancesSquared[target]) {
                out_of_range:
                    s->rangeMode = 0;
                } else {
                    if ((unsigned int)((value - 128) * (value - 128))
                        >= s->distancesSquared[target]) {
                        s->rangeMode = 2;
                    } else {
                        s->rangeMode = mode;
                    }
                }
                break;
            case 2:
                if ((unsigned int)((s->range - 32) * (s->range - 32))
                    < s->distancesSquared[target]) {
                    s->rangeMode = 1;
                } else {
                    s->rangeMode = mode;
                }
                break;
            }
        } else {
            goto out_of_range;
        }
        if ((s->rangeMode != 2 && s->opaqueE8) || (s->rangeMode != 0 && s->opaqueE7)) {
            goto recover;
        }
        if (option == 0 && s->rangeMode == 2) {
            s->rangeMode = 1;
        }
        one = 1;
        switch (s->rangeMode) {
        case 0:
            if (option == one) {
                s->opaque0 = one;
            }
            s->opaqueC = one;
            s->opaque1 = Battle_Ai_IsAiTargetBeyondNearRadius(s, target, s->range);
            if (s->movementFlags & 8) {
                oldPosition = s->destination.word;
                angle = Core_Gte_Atan2(s->x - (s->destination.bytes[0] << 7) - 64,
                            s->z - (s->destination.bytes[2] << 7) - 64)
                      & 4095;
                distance = Core_Math_SquaredVectorLength(s->x - (s->destination.bytes[0] << 7) - 64, 0,
                    s->z - (s->destination.bytes[2] << 7) - 64);
                s->savedDestination.word = oldPosition;
                distance = Core_Math_SquareRootInteger(distance);
                radius = s->range - 96;
                Battle_Ai_ProjectClearRadialDestination((void*)s, s->tile, angle,
                    distance - (radius >= 0 ? radius : -radius));
                if ((s->destination.word & 0xFF00FF) == (s->tile.word & 0xFF00FF)) {
                    s->opaqueD = one;
                    s->destination.word = oldPosition;
                }
            }
            s->movementTarget = s->targetId;
            Battle_Ai_DispatchAiLocomotionMode(s, 0, 0);
            break;
        case 1:
        recover:
            s->movementTarget = target;
            Battle_Ai_UpdateMovementRecovery((void*)s);
            break;
        case 2:
            s->opaque3 = one;
            *(unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3D0) = 0;
            s->opaque4 = one;
            s->moveX = model->x;
            s->moveZ = model->z;
            if (s->settingsByte0Bit7) {
                s->opaque1 = one;
                s->opaque9 = one;
                s->targetX = s->moveX;
                s->targetZ = s->moveZ;
            } else {
                s->movementTarget = target;
            }
            angle = Core_Gte_Atan2(s->moveX - s->x, s->moveZ - s->z);
            Battle_Ai_DispatchAiLocomotionMode(s, 1, angle);
            if (s->actor->id >= 9) {
                s->opaqueE8 = 1;
            }
            break;
        }
    } else {
        if (!s->opaqueE4 && s->relations[target].opaque0Bit1) {
            if (s->settingsBit13) {
                s->opaqueE4 = 1;
                s->timeout = 255;
                s->cachedDestination.bytes[0] = s->targetTileX >> 1;
                s->cachedDestination.bytes[2] = s->targetTileZ >> 1;
            } else {
                if (s->relations[target].visible) {
                    result = 0;
                    goto done;
                }
                ((void (*)(Battle_Ai_ActionExecutionView* arg0, int arg1, int arg2))Battle_Ai_SetTemporaryAwarenessIndicator)((void*)s, 7, 40);
                if (!Battle_Ai_UpdateHesitationTurn(s, 128, 1, 1)) {
                    result = 0;
                }
                goto done;
            }
        }
        if (s->opaqueE8) {
            goto recover;
        }
        if (s->opaqueE4) {
            s->destination.word = s->cachedDestination.word;
            if (s->timeout) {
                s->timeout--;
            }
            if ((s->destination.word & 0xFF00FF) == (s->tile.word & 0xFF00FF)) {
                if (s->relations[target].visible) {
                    result = 0;
                    goto done;
                }
                ((void (*)(Battle_Ai_ActionExecutionView* arg0, int arg1, int arg2))Battle_Ai_SetTemporaryAwarenessIndicator)((void*)s, 7, 40);
                if (!Battle_Ai_UpdateHesitationTurn(s, 128, 1, 1)) {
                cancel:
                    s->opaqueE4 = 0;
                    result = 0;
                }
            } else if (s->timeout) {
                s->opaque1 = 1;
                s->opaqueC = 1;
                Battle_Ai_DispatchAiLocomotionMode(s, 0, 0);
            } else {
                goto cancel;
            }
        } else if (!Battle_Ai_UpdateHesitationTurn(s, 0, 3, 1)) {
            result = 0;
        }
    }
    goto done;

done:
    Battle_Ai_RefreshAiTargetTile(s);
    return result;
}

// decomp: BATTLE.PRG func_800E2B2C (src/BATTLE/BATTLE.PRG/771A8.c:2185)
int Battle_Ai_UpdateTargetFacing(Battle_Ai_MovementDecisionView* context, int targetId)
{
    Actors_ActorRecord* actor = context->model;
    int angle;
    if (targetId == context->actorId) {
        Battle_Ai_IssueActorCommand((void*)context, 0);
        return 1;
    }
    if (context->facingDelay == 0) {
        return 0;
    }
    if (context->reactionPending != 0) {
        if (Battle_Ai_UpdateHesitationTurn(context, 128, 3, 6)) {
            return 1;
        }
        context->facingPending = 0;
        goto failed;
    }
    angle = Core_Gte_Atan2(context->x - context->moveX, context->z - context->moveZ);
    if ((context->destination & 0xFF00FF) != (context->previousFacingTile & 0xFF00FF)
        && ((context->targetFlags[targetId] >> 5) & 1)) {
        if (context->facingPending == 0) {
            context->facingPending = 1;
            context->angle = angle;
            context->previousFacingTile = context->destination;
        }
    }
    if (context->facingPending == 0) {
        return 0;
    }
    context->opaque13A = context->facingDelay;
    if (((actor->instance.facing + (short)actor->instance.lookYaw) & 0xFFF)
        == (context->angle & 0xFFF)) {
        if (!((context->targetFlags[targetId] >> 5) & 1)) {
            if (Battle_Ai_UpdateHesitationTurn(context, 128, 1, 6)) {
                return 1;
            }
            context->facingPending = 0;
            goto failed;
        }
        context->facingPending = 1;
        context->angle = angle & 0xFFF;
        context->previousFacingTile = context->destination;
    }
    Battle_Ai_IssueActorCommand((void*)context, 4);
    return 1;
failed:
    return 0;
}

// decomp: BATTLE.PRG func_800E2CCC (src/BATTLE/BATTLE.PRG/771A8.c:2272)
void Battle_Ai_UpdateMovementRecovery(Battle_Ai_MovementRecoveryView* state)
{
    Battle_Ai_RecoveryModelView* context = state->model;
    int direction, i, bestHeight, bestDirection, height;
    unsigned int x, z;
    unsigned int angle, quadrant;
    int nx, nz;
    Battle_Ai_TilePosition source;
    unsigned char* bounds;
    if ((context->opaque8 & 0x200000) && state->stallCounter++ > 120 && (state->movementFlags & 6)
        && (state->movementFlags & 0x30)
        && Battle_Ai_DestinationOccupancy((void*)state, state->tile.word) > 0) {
        Battle_Ai_ReleaseActorAiReservations((void*)state);
        Battle_Ai_BeginAiTerrainMovement(state);
        return;
    }
    if (state->movementTarget != 16) {
        Battle_Ai_ActorPositionView* actor =
            (Battle_Ai_ActorPositionView*)Actors_actorRecords[state->movementTarget];
        direction = Core_Gte_Atan2(state->x - actor->x, state->z - actor->z) & 0xFFF;
        state->angle = direction;
        state->opaque13A = state->duration;
        Battle_Ai_IssueActorCommand((void*)state, 4);
    } else if (state->opaque9) {
        direction = Core_Gte_Atan2(state->x - state->targetX, state->z - state->targetZ) & 0xFFF;
        state->heading = direction;
        state->opaque13A = state->duration;
        Battle_Ai_IssueActorCommand((void*)state, 3);
    } else {
        bestHeight = (-2147483647 - 1);
        bestDirection = -1;
        source = state->tile;
        x = source.word & 255;
        z = (source.word >> 16) & 255;
        angle = context->angle26 + context->angle14;
        quadrant = (angle & 0xFFF) >> 10;
        if ((angle & 0x3FF) > 512) {
            direction = ((quadrant + 1) & 3) * 2;
        } else {
            direction = quadrant * 2;
        }
        i = 0;
        bounds = (unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3DC);
        for (; i < 4; i++) {
            direction = (direction + i * 2) & 7;
            nx = (unsigned char)(x + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[direction].dx);
            nz = (unsigned char)(z + ((Battle_Ai_DirectionDelta*)((uintptr_t)Core_ScratchBase() + 0x3EC))[direction].dz);
            if (nx < bounds[0] && nz < bounds[1]) {
                height = (Field_GroundHeightFlagsAtTile(nx, nz) << 17) >> 17;
                if (height > bestHeight) {
                    bestDirection = direction;
                    bestHeight = height;
                }
            }
        }
        if (bestDirection < 0) {
            Battle_Ai_IssueActorCommand((void*)state, 0);
        } else {
            state->angle = bestDirection << 9;
            state->opaque13A = state->duration;
            Battle_Ai_IssueActorCommand((void*)state, 4);
        }
    }
}

