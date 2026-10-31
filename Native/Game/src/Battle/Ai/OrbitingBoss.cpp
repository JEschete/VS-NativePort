// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Core/Math/ScalarMath.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_800F17DC (D_800F17DC@BATTLE/BATTLE.PRG)
short Battle_Ai_radialRetreatPath[36][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F5918 (D_800F5918@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitAngularVelocity;

// decomp: D_800F591C (D_800F591C@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitHeightOffset;

// decomp: D_800F5920 (D_800F5920@BATTLE/BATTLE.PRG)
Battle_Ai_ArenaOrbitState* Battle_Ai_arenaOrbit;

// decomp: BATTLE.PRG func_800E7698 (src/BATTLE/BATTLE.PRG/7EE98.c:506)
int Battle_Ai_UpdateBossRadialRetreat(Battle_Ai_RadialRetreatView* state)
{
    Battle_Ai_ModelActiveView* context = state->model;
    Battle_Ai_RadialRetreatOrbitView* motion = (Battle_Ai_RadialRetreatOrbitView*)Battle_Ai_arenaOrbit;
    int angle;
    state->candidateCount = 0;
    context->active = motion->pathStep < 14;
    Core_Math_SquaredVectorLength(0x7C0 - state->x, 0, 0x7C0 - state->z);
    switch (motion->phase) {
    case 0:
        motion->isOrbiting = 1;
        angle = Core_Gte_Atan2(state->x - 0x7C0, state->z - 0x7C0) & 0xFFF;
        angle = -angle;
        angle += 0xC00;
        motion->orbitAngle = angle & 0xFFF;
        motion->pathStep = 0;
        motion->phase++;
        motion->startX = state->x;
        motion->startZ = state->z;
        break;
    case 1:
        if (!Battle_Ai_radialRetreatPath[motion->pathStep][0] && !Battle_Ai_radialRetreatPath[motion->pathStep][1]) {
            if (((Battle_Ai_RetreatPlayerRecordView*)Actors_actorRecords[0])->opaque8 & 0x70000) {
                ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((Battle_Ai_ActorAiState *)((void*)state));
                return 0;
            }
            motion->phase = 0;
            angle = Core_Gte_Atan2(0x7C0 - state->x, 0x7C0 - state->z) & 0xFFF;
            angle = -angle;
            angle += 0xC00;
            motion->heading = angle & 0xFFF;
            context->active = 0;
            return 1;
        }
        break;
    }
    angle = Core_Gte_Cos(motion->orbitAngle);
    state->moveX = (state->x << 12) + Battle_Ai_radialRetreatPath[motion->pathStep][0] * angle;
    angle = Core_Gte_Sin(motion->orbitAngle);
    state->moveZ = (state->z << 12) + Battle_Ai_radialRetreatPath[motion->pathStep][0] * angle;
    state->moveY = (state->y + Battle_Ai_radialRetreatPath[motion->pathStep][1]) << 12;
    Battle_Ai_IssueActorCommand((Battle_Ai_ActorAiState *)((void*)state), 8);
    motion->distanceCached = 0;
    motion->pathStep++;
    return 0;
}

// decomp: BATTLE.PRG func_800E78F4 (src/BATTLE/BATTLE.PRG/7EE98.c:553)
int Battle_Ai_IsBossActionReady(Battle_Ai_ActorAiState* ai_state)
{
    int r = 0;
    int a;
    int b;

    if (ai_state->candidateCount != 0 && Battle_Ai_IsActionCooldownReady(ai_state, ai_state->candidateOrder[0])) {
        a = ai_state->candidateOrder[0]->threshold;
        b = Battle_Ai_roomAiState->readinessLevel;
        r = a < b;
    }
    return r;
}

// decomp: BATTLE.PRG func_800E7960 (src/BATTLE/BATTLE.PRG/7EE98.c:599)
void Battle_Ai_UpdateBossOrbitTracking(Battle_Ai_OrbitTrackingView* state)
{
    Battle_Ai_OrbitTrackingOrbitView* motion = (Battle_Ai_OrbitTrackingOrbitView*)Battle_Ai_arenaOrbit;
    int center[3], playerAngle, currentAngle, tracking;
    int radius, heading, sign, cosCurrent, sinTarget, sinCurrent, cosTarget, aligned,
        delta, ratio, angle, adjust, denominator, scale, magnitude;
    int oppositeAngle;
    int initialAngle, currentHeading, targetHeading, facing, trig, correction, difference;
    if (!motion->started) {
        initialAngle = Core_Gte_Atan2(0x7C0 - state->x, 0x7C0 - state->z) & 0xFFF;
        initialAngle = -initialAngle;
        initialAngle += 0xC00;
        motion->heading = initialAngle & 0xFFF;
        motion->savedHeight = state->y;
        motion->started = 1;
    }
    if (!motion->distanceCached) {
        motion->distanceSquared = Core_Math_SquaredVectorLength(0x7C0 - state->x, 0, 0x7C0 - state->z);
        motion->distanceCached++;
    }
    state->model->active = 0;
    radius = Core_Math_SquareRootInteger(motion->distanceSquared);
    if (radius < 1024) {
        radius += 32;
    }
    if (radius > 1024) {
        radius -= 32;
    }
    difference = radius - 1024;
    if (difference < 0) {
        difference = -difference;
    }
    if (difference < 32) {
        radius = 1024;
    }
    motion->distanceSquared = radius * radius;
    center[0] = 0x7C0000;
    difference = motion->savedHeight - 160;
    if (difference < 0) {
        difference = -difference;
    }
    if (difference < 16) {
        motion->savedHeight = 160;
    } else if (motion->savedHeight < 160) {
        motion->savedHeight += 12;
    } else {
        motion->savedHeight -= 12;
    }
    center[1] = motion->savedHeight << 12;
    center[2] = 0x7C0000;
    playerAngle = Core_Gte_Atan2(0x7C0 - ((Battle_Ai_OrbitPlayerRecordView*)Actors_actorRecords[0])->x,
        0x7C0 - ((Battle_Ai_OrbitPlayerRecordView*)Actors_actorRecords[0])->z);
    currentAngle = Core_Gte_Atan2(0x7C0 - state->x, 0x7C0 - state->z);
    if (Battle_Ai_IsBossActionReady((Battle_Ai_ActorAiState *)((void*)state)) && Battle_Ai_IsEngagementReady((Battle_Ai_ActorAiState *)((void*)state))) {
        heading = playerAngle;
        tracking = 1;
    } else {
        tracking = 0;
        heading = playerAngle + 0x800;
    }
    sign = -1;
    if (Battle_Ai_orbitAngularVelocity >= 0) {
        sign = 1;
    }
    cosCurrent = Core_Gte_Cos(-currentAngle + 0xC00);
    sinTarget = Core_Gte_Sin(-heading + 0xC00);
    sinCurrent = Core_Gte_Sin(-currentAngle + 0xC00);
    cosTarget = Core_Gte_Cos(-heading + 0xC00);
    aligned = ((cosCurrent * sinTarget - sinCurrent * cosTarget) * sign) >= 0;
    if (aligned) {
        if (ABS(Battle_Ai_orbitAngularVelocity) > 0x280000) {
            delta = Battle_Ai_orbitAngularVelocity / 8 + (sign << 16);
        } else {
            delta = sign << 16;
        }
    } else {
        delta = Battle_Ai_orbitAngularVelocity / 4 + (sign << 16);
    }
    if (tracking || Core_Gte_Cos(currentAngle - heading) < Core_Gte_Cos(222)) {
        if (!aligned) {
            delta = -delta;
        }
    }
    Battle_Ai_orbitAngularVelocity += delta;
    sign = -1;
    if (Battle_Ai_orbitAngularVelocity >= 0) {
        sign = 1;
    }
    if (ABS(Battle_Ai_orbitAngularVelocity) > 0x820000) {
        Battle_Ai_orbitAngularVelocity = sign * 0x820000;
    }
    ratio = (state->stats->currentHp << 16) / state->stats->maxHp;
    if (!motion->cooldown) {
        if ((!motion->healthPhase && ratio < 0xC000)
            || (motion->healthPhase == 1 && ratio < 0x4000)) {
            motion->healthPhase++;
            goto trigger;
        } else if (ABS(Battle_Ai_orbitAngularVelocity) < 0x40000 && tracking && Battle_Ai_orbitHeightOffset < 30
                   && !(Core_Random_Rand() & 511)) {
        trigger:
            motion->requested = 1;
        }
    }
    motion->heading += Battle_Ai_orbitAngularVelocity >> 16;
    facing = motion->heading;
    state->movementTarget = 0;
    trig = Core_Gte_Cos(facing);
    state->moveX = center[0] + radius * trig;
    trig = Core_Gte_Sin(facing);
    state->moveZ = center[2] + radius * trig;
    if (tracking) {
        if (aligned) {
            denominator = currentAngle - playerAngle;
            scale = -80 - Battle_Ai_orbitHeightOffset;
        } else {
            oppositeAngle = currentAngle - 0x800;
            denominator = playerAngle - oppositeAngle;
            scale = 300 - Battle_Ai_orbitHeightOffset;
        }
    } else if (aligned) {
        oppositeAngle = currentAngle - 0x800;
        denominator = playerAngle - oppositeAngle;
        scale = 300 - Battle_Ai_orbitHeightOffset;
    } else {
        denominator = ABS(Battle_Ai_orbitAngularVelocity) >> 16;
        scale = 10;
    }
    denominator &= 0xFFF;
    if (denominator > 0x800) {
        denominator = 0x1000 - denominator;
    }
    if (!denominator) {
        denominator = 1;
        scale = 0;
    }
    magnitude = ABS(Battle_Ai_orbitAngularVelocity);
    correction = magnitude * scale / denominator;
    if (ABS(correction) > magnitude * 4) {
        correction = (correction >> 31) * magnitude * 4;
    }
    Battle_Ai_orbitHeightOffset += correction >> 16;
    if (Battle_Ai_orbitHeightOffset < -100) {
        Battle_Ai_orbitHeightOffset = -100;
    }
    if (Battle_Ai_orbitHeightOffset > 500) {
        Battle_Ai_orbitHeightOffset = 500;
    }
    state->moveY = center[1] - (Battle_Ai_orbitHeightOffset << 12);
    if (state->moveY >= 0xFF000) {
        state->moveY = 0xFF000;
    }
    Battle_Ai_IssueActorCommand((Battle_Ai_ActorAiState *)((void*)state), 8);
}

// decomp: BATTLE.PRG func_800E7F8C (src/BATTLE/BATTLE.PRG/7EE98.c:753)
int Battle_Ai_UpdateOrbitingBossBehavior(Battle_Ai_ActorAiState* ai_state)
{
    Battle_Ai_ArenaOrbitState* orbit_state;
    char state;

    if (Battle_Ai_IsBossActionReady(ai_state) != 0) {
        Battle_Ai_TryEngageActor(ai_state);
    }

    state = State_ScriptFlag(0xA6);
    orbit_state = Battle_Ai_arenaOrbit;

    if (orbit_state->cooldown > 0) {
        --orbit_state->cooldown;
    }

    switch (state) {
    case 0:
        if (orbit_state->planSaved != 0) {
            orbit_state->planSaved = 0;
            ai_state->candidateCount = orbit_state->savedPlanCount;
        }

        orbit_state->isOrbiting = 0;
        Battle_Ai_UpdateBossOrbitTracking((Battle_Ai_OrbitTrackingView *)((void*)ai_state));

        if ((orbit_state->requested != 0) && (orbit_state->cooldown == 0)
            && (ai_state->stats->limbs[4].hp >= 2)) {
            orbit_state->cooldown = 900;
            orbit_state->requested = 0;
            orbit_state->savedPlanCount = ai_state->candidateCount;
            state = 1;
            ai_state->candidateCount = 0;
            orbit_state->planSaved = 1;
        }
        break;

    case 4:
        orbit_state->started = 0;
        Battle_Ai_DisengageActor(ai_state);

        if (Battle_Ai_UpdateBossRadialRetreat((Battle_Ai_RadialRetreatView *)((void*)ai_state)) != 0) {
            if (orbit_state->isOrbiting != 0) {
                Battle_Ai_EngagementData* command_actor = ai_state->actor;

                if (command_actor->opaque3C->limbs[4].hp >= 2) {
                    command_actor->opaqueC = 4;
                    command_actor->opaqueE = 0;
                    setVector(&command_actor->opaque10, Actors_actorRecords[0]->instance.position.vx,
                        Actors_actorRecords[0]->instance.position.vy
                            - (Actors_actorRecords[0]->collisionHeight >> 1),
                        Actors_actorRecords[0]->instance.position.vz);
                    command_actor->opaque9 = 24;
                    command_actor->opaque8 = 0x80;
                }

                orbit_state->savedHeight = ai_state->position.vy;
                ai_state->candidateCount = orbit_state->savedPlanCount;
                orbit_state->planSaved = 0;
            } else {
                state = 6;
            }

            Battle_Ai_orbitHeightOffset = 0;
            orbit_state->requested = 0;
            Battle_Ai_orbitAngularVelocity = 0;
        }
        break;
    }

    State_SetScriptFlag(0xA6, state);
    return 1;
}

}  // extern "C"
