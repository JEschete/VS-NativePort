// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Field/PlatformPlacementLive.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800F58C4 (D_800F58C4@BATTLE/BATTLE.PRG)
int Battle_Ai_savedMovementRandomState;

// decomp: D_800F58D8 (D_800F58D8@BATTLE/BATTLE.PRG)
unsigned short (*Battle_Ai_patrolRoutes)[8];

// decomp: src/BATTLE/BATTLE.PRG/771A8.c:794
#define OBSTRUCTION_CACHE ((Battle_Ai_ObstructionCache*)((uintptr_t)&Battle_Ai_FrameState()->directionCache))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:795
#define OBSTRUCTION_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:796
#define OBSTRUCTION_STATES ((Battle_Ai_ObstructionView**)((uintptr_t)&Battle_Ai_FrameState()->actors))
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
    i = Field_CollectMovingPlatformPositionsEntry((Field_PlatformContactPoint *)((void*)points));
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
#define MOVEMENT_CHECK_CACHE ((Battle_Ai_DirectionCacheBits*)((uintptr_t)&Battle_Ai_FrameState()->directionCache))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:902
#define MOVEMENT_CHECK_DIRECTIONS ((Battle_Ai_DirectionDelta*)((uintptr_t)&Battle_Ai_FrameState()->directions))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:903
#define MOVEMENT_CHECK_TILES (*(unsigned short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))
// decomp: BATTLE.PRG func_800E0678 (src/BATTLE/BATTLE.PRG/771A8.c:904)
int Battle_Ai_CanFlyInDirection(Battle_Ai_DirectionCacheView* state, int direction)
{
    int x, z;
    if ((MOVEMENT_CHECK_CACHE->tested >> direction) & 1) {
        return (MOVEMENT_CHECK_CACHE->valid & (1 << direction)) > 0;
    }
    if (Battle_Ai_IsFlightDirectionObstructed((Battle_Ai_ObstructionView *)((void*)state), direction, 1)) {
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
        && VS::Field::GroundHeightAtTile(VS::Battle::Ai::CurrentTerrain(), x, z) + state->flightHeight < Battle_Ai_roomAiState->flightCeiling) {
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

// decomp: BATTLE.PRG func_800E0918 (src/BATTLE/BATTLE.PRG/771A8.c:985)
int Battle_Ai_UpdateFlightGroundTransition(Battle_Ai_MovementCheckView* state, int action, int mode)
{
    void (*callback)(Battle_Ai_MovementCheckView*, int);
    unsigned int distance;
    if (mode) {
        callback = (void (*)(Battle_Ai_MovementCheckView*, int))VS::Battle::Ai::MoveLiveActorTowardDestinationTile;
    } else {
        callback = (void (*)(Battle_Ai_MovementCheckView*, int))VS::Battle::Ai::BeginLiveAiGroundMovement;
    }
    if (state->movementFlags & 6) {
        distance = VS::Core::Math::SquaredTileDistance(VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->destination)), VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->tile)));
        if (mode && distance < 16) {
            if (!(state->model->opaque8 & 0x200000)) {
                callback(state, action);
                return 1;
            }
        } else if (Battle_Ai_roomAiState->flightCeiling) {
            if (VS::Field::GroundHeightAtPackedTile(VS::Battle::Ai::CurrentTerrain(), VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->tile))) + state->flightHeight < Battle_Ai_roomAiState->flightCeiling) {
                if (state->model->opaque8 & 0x200000) {
                    if (VS::Battle::Ai::LiveDestinationOccupancy((Battle_Ai_MoveTargetView *)((void*)state), state->tile) > 0) {
                        VS::Battle::Ai::ReleaseLiveActorAiReservations((Battle_Ai_ActorAiState *)((void*)state));
                        VS::Battle::Ai::BeginLiveAiTerrainMovement(state);
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
#define MOVEMENT_STEP_TILES (*(unsigned short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))
// decomp: src/BATTLE/BATTLE.PRG/771A8.c:1063
#define MOVEMENT_STEP_RESULT (*(int*)((uintptr_t)&Battle_Ai_FrameState()->movementResult))
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

    if (Battle_Ai_UpdateFlightGroundTransition((Battle_Ai_MovementCheckView *)((void*)state), action, 1)) {
        return;
    }
    if (!(state->model->opaque8 & 0x200000)) {
        VS::Battle::Ai::BeginLiveAiAlternateTerrainMovement((Battle_Ai_ActorAiState *)((void*)state));
        return;
    }
    state->movementDisabled = 1;
    if (!VS::Battle::Ai::SelectLiveActorVerticalMovementStep((Battle_Ai_ActorAiState *)((void*)state), &step, &mode)) {
        goto recover;
    }
    x = state->destination.tile.tileX * 128 + 64;
    z = state->destination.tile.tileZ * 128 + 64;
    angle = VS::Core::Math::Arctangent2(state->position.tile.positionX - x, state->z - z);
    angle &= 0xFFF;
    if (state->opaque0 && VS::Core::Math::SquaredTileDistance(VS::Core::Math::UnpackTileCoordinate(state->tile.word), VS::Core::Math::UnpackTileCoordinate(state->destination.word)) < 2
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
        rise = VS::Field::GroundHeightAtPackedTile(VS::Battle::Ai::CurrentTerrain(), VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->tile.word)));
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
            centerAngle = VS::Core::Math::Arctangent2(state->position.tile.positionX - angle, state->z - centerZ);
            state->moveY = selectedHeight << 12;
            heading = (-centerAngle + ONE * 3 / 4) & 0xFFF;
            offsetX = state->position.tile.positionX - angle;
            if (ABS(offsetX) < step) {
                state->moveX = angle << 12;
            } else {
                state->moveX = (state->position.tile.positionX << 12) + step * VS::Core::Math::Cosine(heading);
            }
            offsetZ = state->z - centerZ;
            state->moveZ = ABS(offsetZ) < step ? centerZ << 12
                                               : (state->z << 12) + step * VS::Core::Math::Sine(heading);
            VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), 8);
            return;
        }
        state->moveY = rise;
        state->moveZ = 0;
        state->moveX = 0;
        if (state->moveY > 0 && Battle_Ai_IsFlightDirectionObstructed((Battle_Ai_ObstructionView *)((void*)state), 0, 0)) {
            state->moveY = 0;
        }
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
        return;
    }
    if ((state->tile.word & 0xFF00FF) == (state->destination.word & 0xFF00FF)) {
        int floorY;
        int tileX;
        int deltaX;
        int tileZ;
        int deltaZ;
        if (!state->opaque0 || Battle_Ai_IsFlightDirectionObstructed((Battle_Ai_ObstructionView *)((void*)state), 0, 0)) {
        recover:
            VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)state));
            return;
        }
        floorY = state->flightHeight;
        actor = (Battle_Ai_StepActorView*)Actors_actorRecords[state->actorId];
        if (!(MOVEMENT_STEP_TILES[state->destination.tile.tileZ][state->destination.tile.tileX]
                & 0x20)) {
            floorY += VS::Field::GroundHeightAtPackedTile(VS::Battle::Ai::CurrentTerrain(), VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->destination.word)));
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
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
        return;
    }
    VS::Battle::Ai::ResolveLiveDirectionalMovement((Battle_Ai_ActorAiState *)((void*)state), angle, 2);
    switch (MOVEMENT_STEP_RESULT) {
    case 5:
        angle = state->heading * (ONE / 8);
        if (VS::Battle::Ai::LowestReachableGroundHeight(VS::Battle::Ai::ActorFlightNeighborhood(*(Battle_Ai_ActorAiState *)((void*)state)), angle) + state->flightHeight < state->position.tile.positionY) {
        fall:
            state->moveY = -ONE;
            state->moveZ = 0;
            state->moveX = 0;
        } else {
            angle = -angle;
            angle += ONE * 3 / 4;
            angle &= 0xFFF;
            state->moveY = 0;
            state->moveX = VS::Core::Math::Cosine(angle);
            state->moveZ = VS::Core::Math::Sine(angle);
        }
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
        return;
    case 7:
        height = VS::Field::GroundHeightAtPackedTile(VS::Battle::Ai::CurrentTerrain(), VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->destination.word))) + state->flightHeight;
        px = state->position.word & 0xFFFF;
        py = (int)state->position.word >> 16;
        pz = state->z;
        ground = VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(VS::Battle::Ai::CurrentTerrain(), px, pz, x, z) + state->groundClearance;
        if (Battle_Ai_roomAiState->flightCeiling) {
            ground = ground < Battle_Ai_roomAiState->flightCeiling ? Battle_Ai_roomAiState->flightCeiling : ground;
        }
        top = VS::Battle::Ai::LowestReachableGroundHeight(VS::Battle::Ai::ActorFlightNeighborhood(*(Battle_Ai_ActorAiState *)((void*)state)), angle) + state->flightHeight;
        ground = top > ground ? ground : top;
        distance = VS::Core::Math::SquareRootInteger(VS::Core::Math::SquaredVectorLength(x - px, 0, z - pz));
        value = height + ground;
        denom = VS::Core::Math::SquareRootInteger(((value << 12) / value) << 12) + ONE;
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
            VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
            state->stallCounter = 0;
            return;
        }
        if ((unsigned int)step >= distance) {
        fallback:
            Battle_Ai_UpdateFlightDirectionalMovement((Battle_Ai_ActorAiState *)((void*)state), angle);
            return;
        }
        height =
            (unsigned int)(offset * (step - value) * (step - value)) / square + ground;
        angle = -angle;
        angle += ONE * 3 / 4;
        angle &= 0xFFF;
        x = (px << 12) + step * VS::Core::Math::Cosine(angle);
        z = (pz << 12) + step * VS::Core::Math::Sine(angle);
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
            VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), mode);
        } else {
            state->moveX = x;
            state->moveY = height << 12;
            state->moveZ = z;
            VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), 8);
        }
        return;
    }
}
#undef MOVEMENT_STEP_RESULT
#undef MOVEMENT_STEP_TILES

// decomp: BATTLE.PRG func_800E1238 (src/BATTLE/BATTLE.PRG/771A8.c:1278)
void Battle_Ai_UpdateFlightDirectionalMovement(Battle_Ai_ActorAiState* ai_state, int movement_angle)
{
    int vertical_step;
    int movement_command;
    int angle;
    int c;
    int s;

    if (Battle_Ai_UpdateFlightGroundTransition((Battle_Ai_MovementCheckView *)((void*)ai_state), movement_angle, 0) != 0) {
        return;
    }

    if (VS::Battle::Ai::SelectLiveActorVerticalMovementStep(ai_state, &vertical_step, &movement_command) == 0) {
        VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)ai_state));
        return;
    }

    ai_state->movementDisabled = 1;
    VS::Battle::Ai::ResolveLiveDirectionalMovement(ai_state, movement_angle, 3);

    if (*(int*)((uintptr_t)&Battle_Ai_FrameState()->movementResult) != 5 && *(int*)((uintptr_t)&Battle_Ai_FrameState()->movementResult) != 7) {
        VS::Battle::Ai::ReleaseLiveActorAiReservations(ai_state);
        VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)ai_state));
        return;
    }

    if (*(int*)((uintptr_t)&Battle_Ai_FrameState()->movementResult) == 5) {
        movement_angle = ai_state->heading * (ONE / 8);
    }

    angle = (-movement_angle + ONE * 3 / 4) & 0xFFF;
    c = VS::Core::Math::Cosine(angle);
    s = VS::Core::Math::Sine(angle);

    if (VS::Battle::Ai::LowestReachableGroundHeight(VS::Battle::Ai::ActorFlightNeighborhood(*ai_state), movement_angle) + ai_state->flightHeight < ai_state->position.vy) {
        ai_state->moveY = -ONE;
        ai_state->moveZ = 0;
        ai_state->moveX = 0;
    } else {
        ai_state->moveY = 0;
        ai_state->moveX = c;
        ai_state->moveZ = s;
    }

    if (!(ai_state->model->opaqueABit5 << 21)) {
        VS::Battle::Ai::BeginLiveAiAlternateTerrainMovement(ai_state);
    } else {
        ai_state->stallCounter = 0;
        VS::Battle::Ai::IssueLiveActorCommand(ai_state, movement_command);
    }
}

// decomp: BATTLE.PRG func_800E24EC (src/BATTLE/BATTLE.PRG/771A8.c:1973)
int Battle_Ai_UpdateTargetApproachMovement(Battle_Ai_TargetApproachView* s, int option)
{
    int result = 1, target = s->targetId, mode, value, angle, distance, one, radius;
    unsigned int oldPosition;
    Battle_Ai_ApproachTargetRecordView* model;
    if (target == s->actorId) {
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)s), 0);
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
            s->opaque1 = VS::Battle::Ai::IsLiveAiTargetBeyondNearRadius(s, target, s->range);
            if (s->movementFlags & 8) {
                oldPosition = s->destination.word;
                angle = VS::Core::Math::Arctangent2(s->x - (s->destination.bytes[0] << 7) - 64,
                            s->z - (s->destination.bytes[2] << 7) - 64)
                      & 4095;
                distance = VS::Core::Math::SquaredVectorLength(s->x - (s->destination.bytes[0] << 7) - 64, 0,
                    s->z - (s->destination.bytes[2] << 7) - 64);
                s->savedDestination.word = oldPosition;
                distance = VS::Core::Math::SquareRootInteger(distance);
                radius = s->range - 96;
                VS::Battle::Ai::ProjectLiveClearRadialDestination((Battle_Ai_ActorAiState *)((void*)s), s->tile, angle,
                    distance - (radius >= 0 ? radius : -radius));
                if ((s->destination.word & 0xFF00FF) == (s->tile.word & 0xFF00FF)) {
                    s->opaqueD = one;
                    s->destination.word = oldPosition;
                }
            }
            s->movementTarget = s->targetId;
            VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 0, 0);
            break;
        case 1:
        recover:
            s->movementTarget = target;
            VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)s));
            break;
        case 2:
            s->opaque3 = one;
            *(unsigned char*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
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
            angle = VS::Core::Math::Arctangent2(s->moveX - s->x, s->moveZ - s->z);
            VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 1, angle);
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
                ((void (*)(Battle_Ai_ActionExecutionView* ai_state, int awareness_marker, int duration_frames))Battle_Ai_SetTemporaryAwarenessIndicator)((Battle_Ai_ActionExecutionView *)((void*)s), 7, 40);
                if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 128, 1, 1)) {
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
                ((void (*)(Battle_Ai_ActionExecutionView* ai_state, int awareness_marker, int duration_frames))Battle_Ai_SetTemporaryAwarenessIndicator)((Battle_Ai_ActionExecutionView *)((void*)s), 7, 40);
                if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 128, 1, 1)) {
                cancel:
                    s->opaqueE4 = 0;
                    result = 0;
                }
            } else if (s->timeout) {
                s->opaque1 = 1;
                s->opaqueC = 1;
                VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 0, 0);
            } else {
                goto cancel;
            }
        } else if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 0, 3, 1)) {
            result = 0;
        }
    }
    goto done;

done:
    VS::Battle::Ai::RefreshLiveAiTargetTile(s);
    return result;
}

// decomp: BATTLE.PRG func_800E2B2C (src/BATTLE/BATTLE.PRG/771A8.c:2185)
int Battle_Ai_UpdateTargetFacing(Battle_Ai_MovementDecisionView* context, int targetId)
{
    Actors_ActorRecord* actor = context->model;
    int angle;
    if (targetId == context->actorId) {
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)context), 0);
        return 1;
    }
    if (context->facingDelay == 0) {
        return 0;
    }
    if (context->reactionPending != 0) {
        if (VS::Battle::Ai::UpdateLiveHesitationTurn(context, 128, 3, 6)) {
            return 1;
        }
        context->facingPending = 0;
        goto failed;
    }
    angle = VS::Core::Math::Arctangent2(context->x - context->moveX, context->z - context->moveZ);
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
            if (VS::Battle::Ai::UpdateLiveHesitationTurn(context, 128, 1, 6)) {
                return 1;
            }
            context->facingPending = 0;
            goto failed;
        }
        context->facingPending = 1;
        context->angle = angle & 0xFFF;
        context->previousFacingTile = context->destination;
    }
    VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)context), 4);
    return 1;
failed:
    return 0;
}

}  // extern "C"
