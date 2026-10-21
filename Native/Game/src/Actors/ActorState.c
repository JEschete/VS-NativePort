// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Audio/ActorSound.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Recovery.h"
#include "Battle/StatusEffects.h"
#include "Field/Actors.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_800F49E0 (D_800F49E0@BATTLE/BATTLE.PRG)
u_int Actors_stateFlagsScratch;

// decomp: BATTLE.PRG func_8007BF6C (src/BATTLE/BATTLE.PRG/146C.c:8623)
void Actors_EnableActorModel(int arg0)
{
    if (Battle_actors[arg0] != NULL) {
        Actors_SetActorDisplayFlags(arg0, Actors_ActorStateFlags(arg0) | 1);
    }
}

// decomp: BATTLE.PRG func_8007BFC0 (src/BATTLE/BATTLE.PRG/146C.c:8630)
void Actors_EnableAllActorModels(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        Actors_EnableActorModel(i);
    }
}

// decomp: BATTLE.PRG func_8007BFF8 (src/BATTLE/BATTLE.PRG/146C.c:8639)
void Actors_DisableActorModel(int arg0)
{
    if (Battle_actors[arg0] != NULL) {
        Actors_SetActorDisplayFlags(arg0, Actors_ActorStateFlags(arg0) & ~1);
    }
}

// decomp: BATTLE.PRG func_8007C050 (src/BATTLE/BATTLE.PRG/146C.c:8646)
void Actors_DisableAllActorModels(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        Actors_DisableActorModel(i);
    }
}

// decomp: BATTLE.PRG func_8007C1D8 (src/BATTLE/BATTLE.PRG/146C.c:8686)
void Actors_SetActorHostile(int arg0, int arg1)
{
    if (Battle_actors[arg0] != 0) {
        if (arg1 != 0) {
            Battle_actors[arg0]->kind = 4;
            return;
        }
        Battle_actors[arg0]->kind = 8;
    }
}

// decomp: BATTLE.PRG func_8007C218 (src/BATTLE/BATTLE.PRG/146C.c:8697)
int Actors_ActorHpPercentage(int arg0)
{
    int var_v1;
    Battle_ActorStats* temp_a0 = Battle_actors[arg0]->stats;

    if (temp_a0 != NULL) {
        if (temp_a0->currentHp > 0) {
            var_v1 = (temp_a0->currentHp * 0x64) / temp_a0->maxHp;
            if (var_v1 == 0) {
                var_v1 = 1;
            }
            return var_v1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007C28C (src/BATTLE/BATTLE.PRG/146C.c:8714)
void Actors_RestoreActorVitals(int arg0)
{
    int i;
    Battle_ActorStats* temp_a1;

    if (Battle_actors[arg0] != NULL) {
        temp_a1 = Battle_actors[arg0]->stats;

        if (temp_a1 != NULL) {
            temp_a1->risk = 0;
            temp_a1->actionRecoveryTimer = 0;
            temp_a1->currentHp = temp_a1->maxHp;
            temp_a1->currentMp = temp_a1->maxMp;
            for (i = 0; i < 6; ++i) {
                temp_a1->limbs[i].hp = temp_a1->limbs[i].maxHp;
            }
            Battle_RemoveActorStatuses(0x7FFFFFFF, temp_a1);
            Battle_recoveryResetWord = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9127)
int Actors_FindActorByModelId(int arg0)
{
    u_int i;

    for (i = 0; i < 16; ++i) {
        if ((Battle_actors[i] != 0) && (Battle_actors[i]->modelId == arg0)) {
            return i;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_8007CF64 (src/BATTLE/BATTLE.PRG/146C.c:9139)
Battle_Actor* Actors_ActorAtSlot(int actorId) { return Battle_actors[actorId]; }

// decomp: BATTLE.PRG func_8007CF80 (src/BATTLE/BATTLE.PRG/146C.c:9141)
int Actors_CountActorsByDisposition(int arg0)
{
    u_int i;
    u_int var_a2;

    var_a2 = 0;
    for (i = 0; i < 16; ++i) {
        if ((Battle_actors[i] != 0) && (Battle_actors[i]->kind == arg0)) {
            ++var_a2;
        }
    }
    return var_a2;
}

// decomp: BATTLE.PRG func_8007CFCC (src/BATTLE/BATTLE.PRG/146C.c:9155)
char Actors_IsActorRetainedAfterDefeat(int arg0)
{
    if (Battle_actors[arg0] != 0) {
        return Battle_actors[arg0]->retainAfterDefeat;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007D08C (src/BATTLE/BATTLE.PRG/146C.c:9179)
int Actors_SetActorLogicEnabled(int arg0, int arg1)
{
    if (arg1 != 0) {
        if (Battle_actors[arg0] != NULL) {
            if (Battle_actors[arg0]->commandState == 0x20) {
                Battle_actors[arg0]->commandState = 0;
                Battle_actors[arg0]->kind &= 0x7FFF;
            }
        }
    } else {
        if (Battle_actors[arg0] != NULL) {
            Battle_actors[arg0]->commandState = 0x20;
            Battle_actors[arg0]->kind |= 0x8000;
            Battle_SetActorMarker(arg0, 0);
            Battle_SetActorMarker(arg0, 0x40);
        }
    }
    return arg1;
}

// decomp: BATTLE.PRG func_8007D15C (src/BATTLE/BATTLE.PRG/146C.c:9199)
int Actors_SetOtherActorLogicEnabled(int arg0)
{
    int i;

    for (i = 1; i < 16; ++i) {
        Actors_SetActorLogicEnabled(i, arg0);
    }
    return arg0;
}

// decomp: BATTLE.PRG func_8007D1A8 (src/BATTLE/BATTLE.PRG/146C.c:9209)
void Actors_SetActorDetached(int arg0, int arg1)
{
    int temp_s0;
    Battle_Actor* temp_a0 = Battle_actors[arg0];
    if (temp_a0 != NULL) {
        if (arg1 != 0) {
            temp_s0 = Actors_UnlinkActorFromRoomList(temp_a0);
            Actors_SetActorAnimationLocked(arg0, arg1);
            if (temp_s0 != 0) {
                Battle_Ai_ClearActorAiLinksAndPlan(arg0);
                Battle_Ai_RebuildActorActionPlans(NULL);
            }
        } else {
            temp_s0 = Actors_LinkActorAfterAshley(temp_a0);
            Actors_SetActorAnimationLocked(arg0, 0);
            if (temp_s0 != 0) {
                Battle_Ai_ResolveAiLinkedActors();
                Battle_Ai_RebuildActorActionPlans(NULL);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8009D458 (src/BATTLE/BATTLE.PRG/30DB0.c:2069)
void Actors_FinishActorPlacement(void) { }

// decomp: BATTLE.PRG func_8009D468 (src/BATTLE/BATTLE.PRG/30DB0.c:2073)
void Actors_ResetActorMotionState(int arg0, int arg1)
{
    Actors_ModelEffectSlot sp10;
    int var_a2;
    Actors_ShadowBlend* s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    if (temp_s1 != NULL) {
        temp_s1->instance.scaleX = 0x1000;
        temp_s1->instance.scaleY = 0x1000;
        temp_s1->instance.scaleZ = 0x1000;
        temp_s1->headTrackTarget = -2;
        temp_s1->instance.jumpCountdown = 0;
        temp_s1->scaleFrames = 0;
        temp_s1->instance.turnFrames = 0;
        *(int*)&temp_s1->instance.rotationX = 0;
        *(int*)&temp_s1->instance.rotationZ = 0;
        temp_s1->instance.currentAnimation = 0;
        temp_s1->instance.lookPitch = 0;
        temp_s1->instance.lookYaw = 0;
        temp_s1->instance.movementMode = 0;
        temp_s1->routePhase = 0;
        temp_s1->instance.reactionState = 0;
        temp_s1->instance.poseState = 0;
        Actors_ResetActorHeadTrackingSpeed(temp_s1);
        if (Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] != 0) {
            Actors_UploadActorTextureFrame(temp_s1, 0, 0);
            Actors_UploadActorTextureFrame(temp_s1, 1, 0);
        }
        temp_s1->paletteFadeState[0] = 0;
        temp_s1->textureAnimationSlots[0].timeline = 0;
        temp_s1->paletteFadeActive = 0;
        temp_s1->textureAnimationSlots[1].timeline = 0;
        temp_s1->paletteFadeReverse = 0;
        Actors_SetActorTint(arg0, NULL);
        if (temp_s1->companionSlot == 1) {
            temp_s1->companionSlot = 0;
        }
        temp_s1->instance.soundEnabled = 1;
        sp10.kind = 0;
        Actors_SetActorSoundOverrides(arg0, &sp10);
        temp_s1->collisionState = 4;
        temp_s1->opaque17FF = 0;
        temp_s1->instance.palettePulse.kind = 0;
        temp_s1->opaque1860 = 0;
        temp_s1->instance.supportLayer = 0;
        temp_s1->instance.placementLayer = 0;
        temp_s1->instance.locomotionState = 0;
        temp_s1->instance.movementPending = 0;
        temp_s1->actionPoseActive = 0;
        temp_s1->instance.actionAnimationState = 0;
        var_a2 = 0x62;
        if (arg0 == 0) {
            var_a2 = 1;
        }
        Actors_PlayActorAnimation(arg0, var_a2, 0, 0);
        temp_s1->queuedAnimation = 0;
        temp_s1->ledgeHeight = 0;
        temp_s1->platformOffsetX = 0;
        temp_s1->platformOffsetZ = 0;
        temp_s1->runFrames = 0;
        temp_s1->animationLocked = 0;
        temp_s1->instance.isSkipped = 1;
        temp_s1->instance.isVisible = 1;
        temp_s1->instance.isFrozen = 1;
        temp_s1->isRunning = 0;
        s0 = &temp_s1->shadowBlend;
        memset(s0, 0, sizeof *s0);
        s0->radiusExpandRate = temp_s1->shadowRadiusExpandRate;
        s0->radiusShrinkRate = temp_s1->shadowRadiusShrinkRate;
        memset(&temp_s1->turnState, 0, sizeof temp_s1->turnState);
    }
}

// decomp: BATTLE.PRG func_8009D6F4 (src/BATTLE/BATTLE.PRG/30DB0.c:2147)
void Actors_ResetAllActorMovement(void)
{
    int i;

    for (i = 0; i < 17; ++i) {
        Actors_ActorRecord* actor = Actors_actorRecords[i];
        if (actor == NULL) {
            continue;
        }

        if (actor->actionPoseActive) {
            actor->actionPoseActive = 0;
        }

        actor->instance.jumpCountdown = 0;
        actor->weaponPoseActive = 0;
        actor->instance.supportLayer = 0;
        actor->instance.placementLayer = 0;
        actor->instance.locomotionState = 0;
        actor->instance.movementPending = 0;
        actor->isRunning = 0;
        actor->instance.movementMode = 0;
        memset(&actor->turnState, 0, sizeof actor->turnState);
    }
}

// decomp: BATTLE.PRG func_8009F314 (src/BATTLE/BATTLE.PRG/30DB0.c:3202)
void Actors_BeginActorDeath(int actorId, void* context, int animate)
{
    u_short* animation;
    Actors_ActorRecord* actor;
    Field_GimmickInstance* object;
    int animationId;

    actor = Actors_actorRecords[actorId];
    if (actor != NULL) {
        if (actor->instance.movementMode || actor->instance.motionLocked || actor->instance.locomotionState == 2) {
            if (actor->instance.supportLayer) {
                ((void (*)(int, int))Field_SetMovingPlatformActor)(actor->instance.supportLayer - 2, -1);
                actor->instance.supportLayer = 0;
            }
            actor->instance.currentAnimation = 0;
            actor->instance.rootMotion.vx = 0;
            actor->instance.rootMotion.vz = 0;
            if (actor->instance.slot != 0 || actor->instance.movementMode != 3
                || actor->instance.rootMotion.vy <= 0) {
                actor->instance.rootMotion.vy = -6;
            }
            actor->destinationMode = 0;
            actor->instance.movementMode = 3;
        }
        if (animate != 0) {
            animationId = 0x46;
            if (actor->actionPoseActive) {
                animationId = 0x3A;
            }
            Actors_FindActorAnimationHeader(actor, 0xABU, &animation);
            if (animation != NULL) {
                if (actor->instance.movementMode == 0) {
                    animationId = 0x47;
                }
            }
            Actors_PlayActorAnimation(actorId, animationId, 0, 6);
            Actors_ResetActorHeadTrackingSpeed(actor);
            actor->instance.actionAnimationState = 1;
        } else {
            actor->instance.actionAnimationState = 2;
        }
        if (actor->companionSlot >= 2) {
            Actors_BeginActorDeath(actor->companionSlot, context, animate);
        }
    } else {
        object = Field_gimmickInstances[actorId];
        if (object != NULL) {
            object->mode = 0xF7;
            object->objectInfo.supportIndex = 0;
            Audio_PlayGimmickSoundPair(object, 0x32, 0);
        }
    }
}

// decomp: BATTLE.PRG func_8009F940 (src/BATTLE/BATTLE.PRG/30DB0.c:3388)
void Actors_SetActorCompanion(int arg0, int arg1, Render_ColorCode* arg2, Render_ColorCode* arg3)
{
    Actors_ActorRecord* temp_t0 = Actors_actorRecords[arg0];

    if (temp_t0 != NULL) {
        temp_t0->companionSlot = arg1;
        if (arg1 != 0) {
            *(int*)&temp_t0->companionState = *(int*)arg2;
            temp_t0->companionColor = *arg3;
        }
    }
}

// decomp: BATTLE.PRG func_8009FF08 (src/BATTLE/BATTLE.PRG/30DB0.c:3613)
void Actors_AttachActorToParentBone(int arg0, int arg1, int arg2, int arg3)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (arg1 != -1) {
        if (temp_a0->instance.attachParent != 0xFF) {
            if (Actors_actorRecords[temp_a0->instance.attachParent] != NULL) {
                --Actors_actorRecords[temp_a0->instance.attachParent]->attachedChildCount;
            }
        }
        temp_a0->instance.attachParent = arg1;
        temp_a0->instance.attachBone = arg2;
    }
    *(int*)&temp_a0->instance.position.vx = 0;
    *(int*)&temp_a0->instance.position.vz = 0;
    temp_a0->instance.currentAnimation = 0;
    *(int*)&temp_a0->instance.rotationX = 0;
    *(int*)&temp_a0->instance.rotationZ = 0;
    temp_a0->instance.turnFrames = 0;
    temp_a0->instance.isAttached = 1;
}

// decomp: BATTLE.PRG func_8009FFA8 (src/BATTLE/BATTLE.PRG/30DB0.c:3635)
int Actors_ReleaseActorAttachmentLock(int arg0, int arg1)
{
    int _[2] __attribute__((unused));

    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    int temp_v1 = temp_a0->instance.attachParent;

    if (temp_v1 == 0xFF) {
        return -2;
    }

    if ((temp_v1 > 16) || (Actors_actorRecords[temp_v1]->instance.boneCount == 0)) {
        return -1;
    }

    temp_a0->instance.isAttached = 0;
    // BUG: no return value, in practice it isn't read by the only caller.
}

// decomp: BATTLE.PRG func_800A087C (src/BATTLE/BATTLE.PRG/30DB0.c:3950)
void Actors_SetActorDisplayFlags(int actorId, int arg1)
{
    Actors_ActorRecord* temp_t0 = Actors_actorRecords[actorId];

    if (temp_t0 == NULL) {
        Field_GimmickInstance* temp_a0 = Field_gimmickInstances[actorId];
        if (temp_a0 != NULL) {
            temp_a0->isSkipped = (arg1 & 1) ^ 1;
            temp_a0->isVisible = arg1 >> 1;
        }
    } else {
        Actors_WeaponModel* model;
        int new_var;

        temp_t0->instance.isSkipped = (arg1 & 1) ^ 1;
        temp_t0->instance.isVisible = arg1 >> 1;
        temp_t0->instance.isFrozen = arg1 >> 2;
        temp_t0->instance.soundEnabled = arg1 >> 6;

        model = Actors_weaponModels[actorId * 2];
        new_var = (arg1 >> 11) & 1;
        if (model != NULL) {
            model->reserved08Bit4 = new_var;
        }

        model = Actors_weaponModels[actorId * 2 + 1];
        new_var = (arg1 >> 12) & 1;
        if (model != NULL) {
            model->reserved08Bit4 = new_var;
        }

        if (temp_t0->companionSlot >= 2) {
            Actors_SetActorDisplayFlags(temp_t0->companionSlot, arg1);
        }
    }
}

// decomp: BATTLE.PRG func_800A0BE0 (src/BATTLE/BATTLE.PRG/30DB0.c:4065)
u_int Actors_ActorStateFlags(int actorId)
{
    Actors_ActorRecord* temp_a1 = Actors_actorRecords[actorId];
    int a2;

    Actors_stateFlagsScratch = 0;

    if (temp_a1 == NULL) {
        Field_GimmickInstance* temp_v1 = Field_gimmickInstances[actorId];
        if (temp_v1 != NULL) {
            if (!(temp_v1->isSkipped)) {
                Actors_stateFlagsScratch = 1;
            }
            if (temp_v1->isVisible) {
                Actors_stateFlagsScratch |= 2;
            }
        }
    } else {
        Actors_stateFlagsScratch = 0x8000;
        if (!(temp_a1->instance.isSkipped)) {
            Actors_stateFlagsScratch = 0x8001;
        }
        if (temp_a1->instance.isVisible) {
            Actors_stateFlagsScratch |= 2;
        }
        if (temp_a1->instance.isFrozen) {
            Actors_stateFlagsScratch |= 4;
        }
        if (temp_a1->routePhase) {
            Actors_stateFlagsScratch = Actors_stateFlagsScratch | 0x18;
        } else {
            if (temp_a1->instance.currentAnimation != 0) {
                Actors_stateFlagsScratch |= 8;
            }
            if (temp_a1->instance.turnFrames != 0) {
                Actors_stateFlagsScratch = Actors_stateFlagsScratch | 0x10;
            }
        }
        if (temp_a1->scaleFrames != 0) {
            Actors_stateFlagsScratch |= 0x20;
        }
        if (temp_a1->instance.soundEnabled) {
            Actors_stateFlagsScratch |= 0x40;
        }
        if (temp_a1->pose.holdingLastFrame) {
            Actors_stateFlagsScratch |= 0x08000080;
        }
        if (temp_a1->pose.reachedEnd) {
            Actors_stateFlagsScratch |= 0x100;
        }
        if (temp_a1->attachedChildCount << 4) {
            Actors_stateFlagsScratch |= 0x200;
        }
        if (temp_a1->instance.attachParent != 0xFF) {
            Actors_stateFlagsScratch |= 0x400;
        }
        if ((Actors_weaponModels[temp_a1->instance.slot * 2] != NULL)
            && (Actors_weaponModels[temp_a1->instance.slot * 2]->reserved08Bit4)) {
            Actors_stateFlagsScratch |= 0x800;
        }
        if (((Actors_weaponModels + 1)[temp_a1->instance.slot * 2] != NULL)
            && ((Actors_weaponModels + 1)[temp_a1->instance.slot * 2]->reserved08Bit4)) {
            Actors_stateFlagsScratch |= 0x1000;
        }
        a2 = 0x1000000;
        if (*((int*)&temp_a1->sequenceFlags - 2) & a2) {
            Actors_stateFlagsScratch |= 0x2000;
        }
        if (temp_a1->instance.movementMode) {
            Actors_stateFlagsScratch |= 0x4000;
        }
        if (temp_a1->instance.supportLayer) {
            Actors_stateFlagsScratch |= (temp_a1->instance.supportLayer) << 0x10;
        }
        if (temp_a1->actionPoseActive << 3) {
            Actors_stateFlagsScratch |= 0x80000;
        }
        if ((temp_a1->animationId - 0x47) < 0xEU) {
            Actors_stateFlagsScratch |= 0x80000;
        }
        if (temp_a1->instance.locomotionState) {
            Actors_stateFlagsScratch |= 0x100000;
        }
        if (temp_a1->instance.motionLocked) {
            Actors_stateFlagsScratch |= 0x200000;
        }
        if (*((int*)&temp_a1->sequenceFlags - 2) & 0xF0000000) {
            Actors_stateFlagsScratch |= 0x400000;
        }
        if (*((int*)&temp_a1->sequenceFlags - 2) & 0x10000) {
            Actors_stateFlagsScratch |= 0x08000000;
        }
        if ((temp_a1->instance.actionAnimationState) && (temp_a1->instance.actionAnimationState < 6)) {
            Actors_stateFlagsScratch |= 0x01000000;
        }
        if ((temp_a1->instance.reactionState) && (temp_a1->instance.reactionState < 3U)) {
            Actors_stateFlagsScratch |= 0x02000000;
        }
        if (temp_a1->instance.poseState) {
            Actors_stateFlagsScratch |= 0x04000000;
        }
        if (temp_a1->bypassActorCollision << 9) {
            Actors_stateFlagsScratch |= 0x08000000;
        }
        if ((temp_a1->instance.reactionState) >= 3U) {
            Actors_stateFlagsScratch |= 0x10000000;
        }
        if ((temp_a1->instance.locomotionState) == 1) {
            Actors_stateFlagsScratch |= 0x20000000;
        }
        if (temp_a1->instance.movementPending) {
            Actors_stateFlagsScratch |= 0x40000000;
        }
    }
    return Actors_stateFlagsScratch;
}

// decomp: BATTLE.PRG func_800A1108 (src/BATTLE/BATTLE.PRG/30DB0.c:4182)
int Actors_ActorPlacement(int arg0, void* arg1)
{
    Actors_ActorRecord* var_v1 = Actors_actorRecords[arg0];

    if (var_v1 == NULL) {
        var_v1 = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
        if (var_v1 == NULL) {
            return -1;
        }
    }

    ((int*)arg1)[0] = *(int*)&var_v1->instance.currentTileX;
    ((int*)arg1)[1] = *(int*)&var_v1->instance.position.vx;
    ((int*)arg1)[2] = *(int*)&var_v1->instance.position.vz;
    ((u_short*)arg1)[5] = var_v1->instance.facing;
    return 0;
}

// decomp: BATTLE.PRG func_800A30A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:274)
void Actors_PlaceActorInRoom(int actorId, Field_ActorPlacement* placement, int height, int mode)
{
    Actors_ActorRecord* actor;
    SVECTOR* position;
    short z;
    int ceiling;

    int value;
    u_char tileType;
    int* anchor;

    actor = Actors_actorRecords[actorId];
    *(int*)&actor->instance.currentTileX = *(int*)placement;

    actor->instance.position.vx = ((u_char)placement->tileX << 7) + 0x40;
    z = ((u_char)placement->tileZ << 7) + 0x40;
    actor->instance.position.vz = z;
    actor->instance.position.vy = Field_ActorSupportHeight(actor->instance.position.vx, z);
    if (mode == 0x10 || mode == 0x20) {
        ceiling = Battle_Ai_RoomNavigationCeilingHeight();
        if (ceiling == 0) {
            value = -0x180;
        } else {
            value = ceiling - actor->instance.position.vy;
            if (value >= -0x7F) {
                value = -0x80;
            }
        }
        position = &actor->instance.position;
        actor->instance.position.vy = (u_short)actor->instance.position.vy + value;
        if (Actors_FindActorAtFlightPosition(actorId, position) != 0) {
            Actors_SeparateActorFlightPlacement(actorId, position);
        }
        actor->instance.motionLocked = 1;
        Actors_PlayActorAnimation(actorId, 8, 0, 0);
    } else {
        if (mode == 0x40 || mode == 0x80) {
            actor->instance.isJumping = 1;
        }
        tileType = (u_char)placement->direction;
        if (tileType == 7) {
            actor->instance.currentTileLayer = 0;
            actor->instance.position.vy = (short)height;
            actor->instance.rootMotion.vy = 0;
            actor->instance.movementMode = 3;
        } else {
            if (tileType >= 2U) {
                anchor = (int*)Field_FindMovingPlatformById((u_char)placement->direction);
                *(int*)&actor->instance.position.vx = anchor[0];
                *(int*)&actor->instance.position.vz = anchor[1];
                *(int*)&actor->platformAnchor.vx = anchor[0];
                *(int*)&actor->platformAnchor.vz = anchor[1];

                actor->platformAnchor.pad = (u_char)placement->direction;

                actor->instance.supportLayer = placement->direction;
                actor->instance.placementLayer = placement->direction;
            }
            Actors_PlayActorAnimation(actorId, 1, 0, 0);
        }
    }
    ((void (*)(Actors_ActorRecord*))Actors_SaveActorGroundedTransform)(actor);
    actor->instance.currentAnimation = 0;
    value = placement->height;
    actor->instance.facingDelta = 0;
    actor->instance.turnFrames = 0;
    actor->pendingTargetX = 0;
    actor->pendingTargetZ = 0;
    actor->previousTargetX = 0;
    actor->previousTargetZ = 0;
    value *= 0x10;
    actor->instance.facing = value;
    Actors_ResetActorRunMomentum(actorId);
    actor->contactKind = 0;
}

// decomp: BATTLE.PRG func_800A3310 (src/BATTLE/BATTLE.PRG/3A1A0.c:350)
int Actors_FindActorAtFlightPosition(int arg0, SVECTOR* arg1)
{
    int i;

    for (i = 2; i < arg0; ++i) {
        Actors_ActorRecord* temp_a2 = Actors_actorRecords[i];
        if (temp_a2 != NULL && temp_a2->instance.motionLocked
            && *(int*)&temp_a2->instance.position == *(int*)arg1
            && temp_a2->instance.position.vz == arg1->vz) {
            return i;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A3394 (src/BATTLE/BATTLE.PRG/3A1A0.c:365)
void Actors_SeparateActorFlightPlacement(int arg0, SVECTOR* arg1)
{
    SVECTOR pos;
    int floor;
    int retried;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    pos.vx = arg1->vx;
    pos.vz = arg1->vz;
    pos.vy = arg1->vy - 0xC0;
    floor = Battle_Ai_RoomNavigationCeilingHeight();
    retried = 0;

    while (1) {
        if (Actors_FindActorAtFlightPosition(arg0, &pos) != 0) {
            pos.vy -= 0xC0;
        } else if (floor != 0 && pos.vy - actor->collisionHeight < floor) {
            if (retried != 0) {
                return;
            }
            retried = 1;
            pos.vy = arg1->vy - 0x60;
        } else {
            arg1->vy = pos.vy;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_800A36E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:448)
void Actors_ActorSavedPlacement(int arg0, int arg1, Actors_ActorLocationQuery* arg2)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != NULL) {
        arg2->location.packedTile.word = temp_a0->savedPlacement;
        if (temp_a0->instance.motionLocked && ((arg1 == 0x10) || (arg1 == 0x20))) {
            arg2->location.packedTile.word = *(int*)&temp_a0->instance.currentTileX;
        }
        arg2->location.packedTile.target.facing = temp_a0->instance.facing / 16;
    }
}

// decomp: BATTLE.PRG func_800A3760 (src/BATTLE/BATTLE.PRG/3A1A0.c:460)
int Actors_FindNearestActorAhead(int arg0, int arg1, int arg2)
{
    int i;
    int maxDist;
    int bestDist;
    int best = -1;
    Actors_ActorRecord* self = Actors_actorRecords[arg0];

    if (self == NULL) {
        return -1;
    }

    maxDist = arg1 * arg1;

    for (i = 0; i < 17; ++i) {
        Actors_ActorRecord* actor;
        int dx;
        int dy;
        int dz;
        int dist;
        int angle;

        if (i == arg0) {
            continue;
        }
        actor = Actors_actorRecords[i];
        if (actor == NULL || actor->instance.isSkipped) {
            continue;
        }
        dy = actor->instance.position.vy - self->instance.position.vy;
        if (dy < -arg2 || dy > arg2) {
            continue;
        }
        dx = actor->instance.position.vx - self->instance.position.vx;
        dz = actor->instance.position.vz - self->instance.position.vz;
        dist = dx * dx;
        if (dist < 0) {
            dist = -dist;
        }
        dz = dz * dz;
        if (dz < 0) {
            dz = -dz;
        }
        dist += dz;
        if (dist > maxDist) {
            continue;
        }
        angle = Actors_ActorFacingDeltaToPoint(self, &actor->instance.position);
        if (angle < -ONE / 4 || angle > ONE / 4) {
            continue;
        }
        if (best == -1) {
            best = i;
            bestDist = dist;
        } else if (dist < bestDist) {
            best = i;
            bestDist = dist;
        }
    }
    return best;
}

