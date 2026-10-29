// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/MotionRequests.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Audio/ActorSound.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FlightTerrain.h"
#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Recovery.h"
#include "Battle/StatusEffects.h"
#include "Field/Actors.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/PlatformPlacement.h"
#include "Field/Platforms.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_800F49E0 (D_800F49E0@BATTLE/BATTLE.PRG)
u_int Actors_stateFlagsScratch;

// decomp: BATTLE.PRG func_8007BF6C (src/BATTLE/BATTLE.PRG/146C.c:8623)
void Actors_EnableActorModel(int actor_id)
{
    if (Battle_actors[actor_id] != NULL) {
        Actors_SetActorDisplayFlags(actor_id, Actors_ActorStateFlags(actor_id) | 1);
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
void Actors_DisableActorModel(int actor_id)
{
    if (Battle_actors[actor_id] != NULL) {
        Actors_SetActorDisplayFlags(actor_id, Actors_ActorStateFlags(actor_id) & ~1);
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
void Actors_SetActorHostile(int actor_id, int hostile)
{
    if (Battle_actors[actor_id] != 0) {
        if (hostile != 0) {
            Battle_actors[actor_id]->kind = 4;
            return;
        }
        Battle_actors[actor_id]->kind = 8;
    }
}

// decomp: BATTLE.PRG func_8007C218 (src/BATTLE/BATTLE.PRG/146C.c:8697)
int Actors_ActorHpPercentage(int actor_id)
{
    int hp_percentage;
    Battle_ActorStats* stats = Battle_actors[actor_id]->stats;

    if (stats != NULL) {
        if (stats->currentHp > 0) {
            hp_percentage = (stats->currentHp * 0x64) / stats->maxHp;
            if (hp_percentage == 0) {
                hp_percentage = 1;
            }
            return hp_percentage;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007C28C (src/BATTLE/BATTLE.PRG/146C.c:8714)
void Actors_RestoreActorVitals(int actor_id)
{
    int i;
    Battle_ActorStats* stats;

    if (Battle_actors[actor_id] != NULL) {
        stats = Battle_actors[actor_id]->stats;

        if (stats != NULL) {
            stats->risk = 0;
            stats->actionRecoveryTimer = 0;
            stats->currentHp = stats->maxHp;
            stats->currentMp = stats->maxMp;
            for (i = 0; i < 6; ++i) {
                stats->limbs[i].hp = stats->limbs[i].maxHp;
            }
            Battle_RemoveActorStatuses(0x7FFFFFFF, stats);
            Battle_recoveryResetWord = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9127)
int Actors_FindActorByModelId(int model_id)
{
    u_int i;

    for (i = 0; i < 16; ++i) {
        if ((Battle_actors[i] != 0) && (Battle_actors[i]->modelId == model_id)) {
            return i;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_8007CF64 (src/BATTLE/BATTLE.PRG/146C.c:9139)
Battle_Actor* Actors_ActorAtSlot(int actorId) { return Battle_actors[actorId]; }

// decomp: BATTLE.PRG func_8007CF80 (src/BATTLE/BATTLE.PRG/146C.c:9141)
int Actors_CountActorsByDisposition(int actor_kind)
{
    u_int i;
    u_int matching_actor_count;

    matching_actor_count = 0;
    for (i = 0; i < 16; ++i) {
        if ((Battle_actors[i] != 0) && (Battle_actors[i]->kind == actor_kind)) {
            ++matching_actor_count;
        }
    }
    return matching_actor_count;
}

// decomp: BATTLE.PRG func_8007CFCC (src/BATTLE/BATTLE.PRG/146C.c:9155)
char Actors_IsActorRetainedAfterDefeat(int actor_id)
{
    if (Battle_actors[actor_id] != 0) {
        return Battle_actors[actor_id]->retainAfterDefeat;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007D08C (src/BATTLE/BATTLE.PRG/146C.c:9179)
int Actors_SetActorLogicEnabled(int actor_id, int enabled)
{
    if (enabled != 0) {
        if (Battle_actors[actor_id] != NULL) {
            if (Battle_actors[actor_id]->commandState == 0x20) {
                Battle_actors[actor_id]->commandState = 0;
                Battle_actors[actor_id]->kind &= 0x7FFF;
            }
        }
    } else {
        if (Battle_actors[actor_id] != NULL) {
            Battle_actors[actor_id]->commandState = 0x20;
            Battle_actors[actor_id]->kind |= 0x8000;
            Battle_SetActorMarker(actor_id, 0);
            Battle_SetActorMarker(actor_id, 0x40);
        }
    }
    return enabled;
}

// decomp: BATTLE.PRG func_8007D15C (src/BATTLE/BATTLE.PRG/146C.c:9199)
int Actors_SetOtherActorLogicEnabled(int enabled)
{
    int i;

    for (i = 1; i < 16; ++i) {
        Actors_SetActorLogicEnabled(i, enabled);
    }
    return enabled;
}

// decomp: BATTLE.PRG func_8007D1A8 (src/BATTLE/BATTLE.PRG/146C.c:9209)
void Actors_SetActorDetached(int actor_id, int detached)
{
    int list_changed;
    Battle_Actor* actor = Battle_actors[actor_id];
    if (actor != NULL) {
        if (detached != 0) {
            list_changed = Actors_UnlinkActorFromRoomList(actor);
            Actors_SetActorAnimationLocked(actor_id, detached);
            if (list_changed != 0) {
                Battle_Ai_ClearActorAiLinksAndPlan(actor_id);
                Battle_Ai_RebuildActorActionPlans(NULL);
            }
        } else {
            list_changed = Actors_LinkActorAfterAshley(actor);
            Actors_SetActorAnimationLocked(actor_id, 0);
            if (list_changed != 0) {
                Battle_Ai_ResolveAiLinkedActors();
                Battle_Ai_RebuildActorActionPlans(NULL);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8009D458 (src/BATTLE/BATTLE.PRG/30DB0.c:2069)
void Actors_FinishActorPlacement(void) { }

// decomp: BATTLE.PRG func_8009D468 (src/BATTLE/BATTLE.PRG/30DB0.c:2073)
void Actors_ResetActorMotionState(int actor_id, int arg1)
{
    Actors_ModelEffectSlot sound_override;
    int initial_animation_id;
    Actors_ShadowBlend* shadow_blend;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor != NULL) {
        actor->instance.scaleX = 0x1000;
        actor->instance.scaleY = 0x1000;
        actor->instance.scaleZ = 0x1000;
        actor->headTrackTarget = -2;
        actor->instance.jumpCountdown = 0;
        actor->scaleFrames = 0;
        actor->instance.turnFrames = 0;
        *(int*)&actor->instance.rotationX = 0;
        *(int*)&actor->instance.rotationZ = 0;
        actor->instance.currentAnimation = 0;
        actor->instance.lookPitch = 0;
        actor->instance.lookYaw = 0;
        actor->instance.movementMode = 0;
        actor->routePhase = 0;
        actor->instance.reactionState = 0;
        actor->instance.poseState = 0;
        Actors_ResetActorHeadTrackingSpeed(actor);
        if (Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] != 0) {
            Actors_UploadActorTextureFrame(actor, 0, 0);
            Actors_UploadActorTextureFrame(actor, 1, 0);
        }
        actor->paletteFadeState[0] = 0;
        actor->textureAnimationSlots[0].timeline = 0;
        actor->paletteFadeActive = 0;
        actor->textureAnimationSlots[1].timeline = 0;
        actor->paletteFadeReverse = 0;
        Actors_SetActorTint(actor_id, NULL);
        if (actor->companionSlot == 1) {
            actor->companionSlot = 0;
        }
        actor->instance.soundEnabled = 1;
        sound_override.kind = 0;
        Actors_SetActorSoundOverrides(actor_id, &sound_override);
        actor->collisionState = 4;
        actor->opaque17FF = 0;
        actor->instance.palettePulse.kind = 0;
        actor->opaque1860 = 0;
        actor->instance.supportLayer = 0;
        actor->instance.placementLayer = 0;
        actor->instance.locomotionState = 0;
        actor->instance.movementPending = 0;
        actor->actionPoseActive = 0;
        actor->instance.actionAnimationState = 0;
        initial_animation_id = 0x62;
        if (actor_id == 0) {
            initial_animation_id = 1;
        }
        Actors_PlayActorAnimation(actor_id, initial_animation_id, 0, 0);
        actor->queuedAnimation = 0;
        actor->ledgeHeight = 0;
        actor->platformOffsetX = 0;
        actor->platformOffsetZ = 0;
        actor->runFrames = 0;
        actor->animationLocked = 0;
        actor->instance.isSkipped = 1;
        actor->instance.isVisible = 1;
        actor->instance.isFrozen = 1;
        actor->isRunning = 0;
        shadow_blend = &actor->shadowBlend;
        memset(shadow_blend, 0, sizeof *shadow_blend);
        shadow_blend->radiusExpandRate = actor->shadowRadiusExpandRate;
        shadow_blend->radiusShrinkRate = actor->shadowRadiusShrinkRate;
        memset(&actor->turnState, 0, sizeof actor->turnState);
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
void Actors_SetActorCompanion(int actor_id, int companion_slot, Render_ColorCode* companion_state, Render_ColorCode* companion_color)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor != NULL) {
        actor->companionSlot = companion_slot;
        if (companion_slot != 0) {
            *(int*)&actor->companionState = *(int*)companion_state;
            actor->companionColor = *companion_color;
        }
    }
}

// decomp: BATTLE.PRG func_8009FF08 (src/BATTLE/BATTLE.PRG/30DB0.c:3613)
void Actors_AttachActorToParentBone(int actor_id, int parent_actor_id, int parent_bone_index, int arg3)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (parent_actor_id != -1) {
        if (actor->instance.attachParent != 0xFF) {
            if (Actors_actorRecords[actor->instance.attachParent] != NULL) {
                --Actors_actorRecords[actor->instance.attachParent]->attachedChildCount;
            }
        }
        actor->instance.attachParent = parent_actor_id;
        actor->instance.attachBone = parent_bone_index;
    }
    *(int*)&actor->instance.position.vx = 0;
    *(int*)&actor->instance.position.vz = 0;
    actor->instance.currentAnimation = 0;
    *(int*)&actor->instance.rotationX = 0;
    *(int*)&actor->instance.rotationZ = 0;
    actor->instance.turnFrames = 0;
    actor->instance.isAttached = 1;
}

// decomp: BATTLE.PRG func_8009FFA8 (src/BATTLE/BATTLE.PRG/30DB0.c:3635)
int Actors_ReleaseActorAttachmentLock(int actor_id, int arg1)
{
    int _[2] __attribute__((unused));

    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    int parent_actor_id = actor->instance.attachParent;

    if (parent_actor_id == 0xFF) {
        return -2;
    }

    if ((parent_actor_id > 16) || (Actors_actorRecords[parent_actor_id]->instance.boneCount == 0)) {
        return -1;
    }

    actor->instance.isAttached = 0;
    // BUG: no return value, in practice it isn't read by the only caller.
}

// decomp: BATTLE.PRG func_800A087C (src/BATTLE/BATTLE.PRG/30DB0.c:3950)
void Actors_SetActorDisplayFlags(int actorId, int display_flags)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];

    if (actor == NULL) {
        Field_GimmickInstance* gimmick = Field_gimmickInstances[actorId];
        if (gimmick != NULL) {
            gimmick->isSkipped = (display_flags & 1) ^ 1;
            gimmick->isVisible = display_flags >> 1;
        }
    } else {
        Actors_WeaponModel* model;
        int equipment_display_bit;

        actor->instance.isSkipped = (display_flags & 1) ^ 1;
        actor->instance.isVisible = display_flags >> 1;
        actor->instance.isFrozen = display_flags >> 2;
        actor->instance.soundEnabled = display_flags >> 6;

        model = Actors_weaponModels[actorId * 2];
        equipment_display_bit = (display_flags >> 11) & 1;
        if (model != NULL) {
            model->reserved08Bit4 = equipment_display_bit;
        }

        model = Actors_weaponModels[actorId * 2 + 1];
        equipment_display_bit = (display_flags >> 12) & 1;
        if (model != NULL) {
            model->reserved08Bit4 = equipment_display_bit;
        }

        if (actor->companionSlot >= 2) {
            Actors_SetActorDisplayFlags(actor->companionSlot, display_flags);
        }
    }
}

// decomp: BATTLE.PRG func_800A0BE0 (src/BATTLE/BATTLE.PRG/30DB0.c:4065)
u_int Actors_ActorStateFlags(int actorId)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];
    int animation_lock_mask;

    Actors_stateFlagsScratch = 0;

    if (actor == NULL) {
        Field_GimmickInstance* gimmick = Field_gimmickInstances[actorId];
        if (gimmick != NULL) {
            if (!(gimmick->isSkipped)) {
                Actors_stateFlagsScratch = 1;
            }
            if (gimmick->isVisible) {
                Actors_stateFlagsScratch |= 2;
            }
        }
    } else {
        Actors_stateFlagsScratch = 0x8000;
        if (!(actor->instance.isSkipped)) {
            Actors_stateFlagsScratch = 0x8001;
        }
        if (actor->instance.isVisible) {
            Actors_stateFlagsScratch |= 2;
        }
        if (actor->instance.isFrozen) {
            Actors_stateFlagsScratch |= 4;
        }
        if (actor->routePhase) {
            Actors_stateFlagsScratch = Actors_stateFlagsScratch | 0x18;
        } else {
            if (actor->instance.currentAnimation != 0) {
                Actors_stateFlagsScratch |= 8;
            }
            if (actor->instance.turnFrames != 0) {
                Actors_stateFlagsScratch = Actors_stateFlagsScratch | 0x10;
            }
        }
        if (actor->scaleFrames != 0) {
            Actors_stateFlagsScratch |= 0x20;
        }
        if (actor->instance.soundEnabled) {
            Actors_stateFlagsScratch |= 0x40;
        }
        if (actor->pose.holdingLastFrame) {
            Actors_stateFlagsScratch |= 0x08000080;
        }
        if (actor->pose.reachedEnd) {
            Actors_stateFlagsScratch |= 0x100;
        }
        if (actor->attachedChildCount << 4) {
            Actors_stateFlagsScratch |= 0x200;
        }
        if (actor->instance.attachParent != 0xFF) {
            Actors_stateFlagsScratch |= 0x400;
        }
        if ((Actors_weaponModels[actor->instance.slot * 2] != NULL)
            && (Actors_weaponModels[actor->instance.slot * 2]->reserved08Bit4)) {
            Actors_stateFlagsScratch |= 0x800;
        }
        if (((Actors_weaponModels + 1)[actor->instance.slot * 2] != NULL)
            && ((Actors_weaponModels + 1)[actor->instance.slot * 2]->reserved08Bit4)) {
            Actors_stateFlagsScratch |= 0x1000;
        }
        animation_lock_mask = 0x1000000;
        if (*((int*)&actor->sequenceFlags - 2) & animation_lock_mask) {
            Actors_stateFlagsScratch |= 0x2000;
        }
        if (actor->instance.movementMode) {
            Actors_stateFlagsScratch |= 0x4000;
        }
        if (actor->instance.supportLayer) {
            Actors_stateFlagsScratch |= (actor->instance.supportLayer) << 0x10;
        }
        if (actor->actionPoseActive << 3) {
            Actors_stateFlagsScratch |= 0x80000;
        }
        if ((actor->animationId - 0x47) < 0xEU) {
            Actors_stateFlagsScratch |= 0x80000;
        }
        if (actor->instance.locomotionState) {
            Actors_stateFlagsScratch |= 0x100000;
        }
        if (actor->instance.motionLocked) {
            Actors_stateFlagsScratch |= 0x200000;
        }
        if (*((int*)&actor->sequenceFlags - 2) & 0xF0000000) {
            Actors_stateFlagsScratch |= 0x400000;
        }
        if (*((int*)&actor->sequenceFlags - 2) & 0x10000) {
            Actors_stateFlagsScratch |= 0x08000000;
        }
        if ((actor->instance.actionAnimationState) && (actor->instance.actionAnimationState < 6)) {
            Actors_stateFlagsScratch |= 0x01000000;
        }
        if ((actor->instance.reactionState) && (actor->instance.reactionState < 3U)) {
            Actors_stateFlagsScratch |= 0x02000000;
        }
        if (actor->instance.poseState) {
            Actors_stateFlagsScratch |= 0x04000000;
        }
        if (actor->bypassActorCollision << 9) {
            Actors_stateFlagsScratch |= 0x08000000;
        }
        if ((actor->instance.reactionState) >= 3U) {
            Actors_stateFlagsScratch |= 0x10000000;
        }
        if ((actor->instance.locomotionState) == 1) {
            Actors_stateFlagsScratch |= 0x20000000;
        }
        if (actor->instance.movementPending) {
            Actors_stateFlagsScratch |= 0x40000000;
        }
    }
    return Actors_stateFlagsScratch;
}

// decomp: BATTLE.PRG func_800A1108 (src/BATTLE/BATTLE.PRG/30DB0.c:4182)
int Actors_ActorPlacement(int actor_id, void* placement_out)
{
    Actors_ActorRecord* placed_object = Actors_actorRecords[actor_id];

    if (placed_object == NULL) {
        placed_object = (Actors_ActorRecord*)Field_gimmickInstances[actor_id];
        if (placed_object == NULL) {
            return -1;
        }
    }

    ((int*)placement_out)[0] = *(int*)&placed_object->instance.currentTileX;
    ((int*)placement_out)[1] = *(int*)&placed_object->instance.position.vx;
    ((int*)placement_out)[2] = *(int*)&placed_object->instance.position.vz;
    ((u_short*)placement_out)[5] = placed_object->instance.facing;
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
    Field_PlatformContactPoint platform_contact;

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
                if (!Field_CopyMovingPlatformPlacement((u_char)placement->direction, &platform_contact)) {
                    abort();
                }
                actor->instance.position.vx = platform_contact.x;
                actor->instance.position.vy = platform_contact.height;
                actor->instance.position.vz = platform_contact.z;
                actor->instance.position.pad = platform_contact.platformIndex;
                actor->platformAnchor.vx = platform_contact.x;
                actor->platformAnchor.vy = platform_contact.height;
                actor->platformAnchor.vz = platform_contact.z;

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
int Actors_FindActorAtFlightPosition(int actor_id, SVECTOR* position)
{
    int i;

    for (i = 2; i < actor_id; ++i) {
        Actors_ActorRecord* other_actor = Actors_actorRecords[i];
        if (other_actor != NULL && other_actor->instance.motionLocked
            && *(int*)&other_actor->instance.position == *(int*)position
            && other_actor->instance.position.vz == position->vz) {
            return i;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A3394 (src/BATTLE/BATTLE.PRG/3A1A0.c:365)
void Actors_SeparateActorFlightPlacement(int actor_id, SVECTOR* position)
{
    SVECTOR pos;
    int floor;
    int retried;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    pos.vx = position->vx;
    pos.vz = position->vz;
    pos.vy = position->vy - 0xC0;
    floor = Battle_Ai_RoomNavigationCeilingHeight();
    retried = 0;

    while (1) {
        if (Actors_FindActorAtFlightPosition(actor_id, &pos) != 0) {
            pos.vy -= 0xC0;
        } else if (floor != 0 && pos.vy - actor->collisionHeight < floor) {
            if (retried != 0) {
                return;
            }
            retried = 1;
            pos.vy = position->vy - 0x60;
        } else {
            position->vy = pos.vy;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_800A36E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:448)
void Actors_ActorSavedPlacement(int actor_id, int movement_class, Actors_ActorLocationQuery* placement_out)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    if (actor != NULL) {
        placement_out->location.packedTile.word = actor->savedPlacement;
        if (actor->instance.motionLocked && ((movement_class == 0x10) || (movement_class == 0x20))) {
            placement_out->location.packedTile.word = *(int*)&actor->instance.currentTileX;
        }
        placement_out->location.packedTile.target.facing = actor->instance.facing / 16;
    }
}

// decomp: BATTLE.PRG func_800A3760 (src/BATTLE/BATTLE.PRG/3A1A0.c:460)
int Actors_FindNearestActorAhead(int actor_id, int horizontal_range, int vertical_range)
{
    int i;
    int maxDist;
    int bestDist;
    int best = -1;
    Actors_ActorRecord* self = Actors_actorRecords[actor_id];

    if (self == NULL) {
        return -1;
    }

    maxDist = horizontal_range * horizontal_range;

    for (i = 0; i < 17; ++i) {
        Actors_ActorRecord* actor;
        int dx;
        int dy;
        int dz;
        int dist;
        int angle;

        if (i == actor_id) {
            continue;
        }
        actor = Actors_actorRecords[i];
        if (actor == NULL || actor->instance.isSkipped) {
            continue;
        }
        dy = actor->instance.position.vy - self->instance.position.vy;
        if (dy < -vertical_range || dy > vertical_range) {
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

