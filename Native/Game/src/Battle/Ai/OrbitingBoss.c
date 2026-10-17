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
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Vector.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_800F17DC (D_800F17DC@BATTLE/BATTLE.PRG)
short Battle_Ai_radialRetreatPath[36][2]; /* contents: module image (ModuleData.c) */

// decomp: D_800F5918 (D_800F5918@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitAngularVelocity;

// decomp: D_800F591C (D_800F591C@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitHeightOffset;

// decomp: D_800F5920 (D_800F5920@BATTLE/BATTLE.PRG)
Battle_Ai_ArenaOrbitState* Battle_Ai_arenaOrbit;

// decomp: BATTLE.PRG func_800E7660 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_AllocateOrbitingBossState(Battle_Ai_SettingsView*)
{
    VS_UNPORTED("Battle::Ai::AllocateOrbitingBossState");
}

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
                ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((void*)state);
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
    Battle_Ai_IssueActorCommand((void*)state, 8);
    motion->distanceCached = 0;
    motion->pathStep++;
    return 0;
}

// decomp: BATTLE.PRG func_800E78F4 (src/BATTLE/BATTLE.PRG/7EE98.c:553)
int Battle_Ai_IsBossActionReady(Battle_Ai_ActorAiState* arg0)
{
    int r = 0;
    int a;
    int b;

    if (arg0->candidateCount != 0 && Battle_Ai_IsActionCooldownReady(arg0, arg0->candidateOrder[0])) {
        a = arg0->candidateOrder[0]->threshold;
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
    if (Battle_Ai_IsBossActionReady((void*)state) && Battle_Ai_IsEngagementReady((void*)state)) {
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
    Battle_Ai_IssueActorCommand((void*)state, 8);
}

// decomp: BATTLE.PRG func_800E7F8C (src/BATTLE/BATTLE.PRG/7EE98.c:753)
int Battle_Ai_UpdateOrbitingBossBehavior(Battle_Ai_ActorAiState* arg0)
{
    Battle_Ai_ArenaOrbitState* temp_s0;
    char state;

    if (Battle_Ai_IsBossActionReady(arg0) != 0) {
        Battle_Ai_TryEngageActor(arg0);
    }

    state = State_ScriptFlag(0xA6);
    temp_s0 = Battle_Ai_arenaOrbit;

    if (temp_s0->cooldown > 0) {
        --temp_s0->cooldown;
    }

    switch (state) {
    case 0:
        if (temp_s0->planSaved != 0) {
            temp_s0->planSaved = 0;
            arg0->candidateCount = temp_s0->savedPlanCount;
        }

        temp_s0->isOrbiting = 0;
        Battle_Ai_UpdateBossOrbitTracking((void*)arg0);

        if ((temp_s0->requested != 0) && (temp_s0->cooldown == 0)
            && (arg0->stats->limbs[4].hp >= 2)) {
            temp_s0->cooldown = 900;
            temp_s0->requested = 0;
            temp_s0->savedPlanCount = arg0->candidateCount;
            state = 1;
            arg0->candidateCount = 0;
            temp_s0->planSaved = 1;
        }
        break;

    case 4:
        temp_s0->started = 0;
        Battle_Ai_DisengageActor(arg0);

        if (Battle_Ai_UpdateBossRadialRetreat((void*)arg0) != 0) {
            if (temp_s0->isOrbiting != 0) {
                Battle_Ai_EngagementData* temp_a1 = arg0->actor;

                if (temp_a1->opaque3C->limbs[4].hp >= 2) {
                    temp_a1->opaqueC = 4;
                    temp_a1->opaqueE = 0;
                    setVector(&temp_a1->opaque10, Actors_actorRecords[0]->instance.position.vx,
                        Actors_actorRecords[0]->instance.position.vy
                            - (Actors_actorRecords[0]->collisionHeight >> 1),
                        Actors_actorRecords[0]->instance.position.vz);
                    temp_a1->opaque9 = 24;
                    temp_a1->opaque8 = 0x80;
                }

                temp_s0->savedHeight = arg0->position.vy;
                arg0->candidateCount = temp_s0->savedPlanCount;
                temp_s0->planSaved = 0;
            } else {
                state = 6;
            }

            Battle_Ai_orbitHeightOffset = 0;
            temp_s0->requested = 0;
            Battle_Ai_orbitAngularVelocity = 0;
        }
        break;
    }

    State_SetScriptFlag(0xA6, state);
    return 1;
}

