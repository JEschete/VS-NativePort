// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/AnimationBindings.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/MotionRequests.h"
#include "Actors/Movement.h"
#include "Actors/SequencePlayback.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Audio/ActorSound.h"
#include "Audio/Sfx.h"
#include "Battle/ActionAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/WordCopy.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800E8960 (D_800E8960@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceIdsByKind[592]; /* contents: module image (ModuleData.c) */

// decomp: D_800E8F2C (D_800E8F2C@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceBankIndex; /* contents: module image (ModuleData.c) */

// decomp: D_800E8FC0 (D_800E8FC0@BATTLE/BATTLE.PRG)
char Actors_reservedControlByte; /* contents: module image (ModuleData.c) */

// decomp: D_800E9090 (D_800E9090@BATTLE/BATTLE.PRG)
u_char Actors_drawnWeaponAnimationBases[3232]; /* contents: module image (ModuleData.c) */

// decomp: D_800E909C (D_800E909C@BATTLE/BATTLE.PRG)
u_char Actors_sheathedWeaponAnimationBases[3220]; /* contents: module image (ModuleData.c) */

// decomp: D_800E916C (D_800E916C@BATTLE/BATTLE.PRG)
Actors_TextureAnimationKey Actors_secondarySlotSequences[188][8]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9298 (D_800E9298@BATTLE/BATTLE.PRG)
u_char Actors_animationEventLengths[2712]; /* contents: module image (ModuleData.c) */

// decomp: D_800E92DC (D_800E92DC@BATTLE/BATTLE.PRG)
u_char Actors_drawnWeaponSoundIds[2644]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9A28 (D_800E9A28@BATTLE/BATTLE.PRG)
u_short Actors_animationEventPoints[97][4]; /* contents: module image (ModuleData.c) */

// decomp: D_800F244F (D_800F244F@BATTLE/BATTLE.PRG)
u_char Actors_sequenceTexturePagesOneBased[8425];

// decomp: D_800F2450 (D_800F2450@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceBankTexturePages[8424];

// decomp: D_800F4528 (D_800F4528@BATTLE/BATTLE.PRG)
void* Actors_sharedSequenceData[4];

// decomp: D_800F4628 (D_800F4628@BATTLE/BATTLE.PRG)
short Actors_sharedSequenceAnimationIds[4][16];

// decomp: D_800F4768 (D_800F4768@BATTLE/BATTLE.PRG)
void* Actors_sequenceWorkBuffer;

// decomp: D_800F4810 (D_800F4810@BATTLE/BATTLE.PRG)
char* Actors_sharedSequenceAnimationData[4][16];

// decomp: D_800F4930 (D_800F4930@BATTLE/BATTLE.PRG)
void* Actors_primarySequenceBuffer;

// decomp: D_800F4938 (D_800F4938@BATTLE/BATTLE.PRG)
short Actors_sharedSequenceEntryParams[4][16];

// decomp: D_800F49DC (D_800F49DC@BATTLE/BATTLE.PRG)
char Actors_lastResolvedSequenceIndex;

// decomp: BATTLE.PRG func_800770FC (src/BATTLE/BATTLE.PRG/146C.c:6444)
void Actors_QueueActorBaseSequenceLoad(Battle_Actor* arg0 __attribute__((unused)), int actor_id)
{
    Content_ActorAssetRequest asset_request;

    asset_request.assetKind = 7;
    asset_request.targetSlot = actor_id;
    asset_request.modelId = 0;
    asset_request.subKind = 0;
    Content_QueueActorAssetRequest(&asset_request);
}

// decomp: BATTLE.PRG func_800771E0 (src/BATTLE/BATTLE.PRG/146C.c:6480)
void Actors_QueueActorWeaponSequenceLoad(char* actor_bytes, int actor_id, int weapon_category, int spawn_flags)
{
    Content_ActorAssetRequest asset_request;

    if (((weapon_category != 0) || (actor_id >= 2)) && ((actor_id < 2) || ((spawn_flags & 3) == 1))) {
        asset_request.assetKind = 7;
        asset_request.targetSlot = actor_id;
        asset_request.modelId = ((Battle_Actor*)actor_bytes)->equippedWeaponCategory;
        asset_request.subKind = 0;
        Content_QueueActorAssetRequest(&asset_request);
    }
}

// decomp: BATTLE.PRG func_8007C348 (src/BATTLE/BATTLE.PRG/146C.c:8738)
void Actors_InterruptActorAction(int actor_id, int animate) { Actors_BeginActorDeath(actor_id, NULL, animate); }

// decomp: BATTLE.PRG func_8007CB84 (src/BATTLE/BATTLE.PRG/146C.c:8997)
void Actors_QueueSharedSequenceLoad(int index, int wepId)
{
    Content_ActorAssetRequest objData;

    objData.assetKind = 7;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.subKind = 1;
    Content_QueueActorAssetRequest(&objData);
}

// decomp: BATTLE.PRG func_8007CBBC (src/BATTLE/BATTLE.PRG/146C.c:9008)
int Actors_UnloadSharedSequence(int bankId)
{
    int slot;
    int result;
    if (bankId < 1 || bankId > 3) {
        Core_Errorf("actors.sequence", "invalid shared bank retirement=%d", bankId);
        return -1;
    }
    for (slot = 0; slot < 17; ++slot) {
        Actors_ActorRecord* actor = Actors_actorRecords[slot];
        u_short* entry;
        if (actor == NULL || actor->sharedSequences[bankId - 1] == NULL ||
            actor->animationHeader != (intptr_t)actor->sharedSequences[bankId - 1] ||
            !actor->poseOverrideActive) {
            continue;
        }
        if (Actors_FindBoundAnimationHeader(actor, &entry) != actor->animationHeader ||
            entry == NULL || entry[2] == 0) {
            Core_Errorf("actors.sequence", "bank=%d cannot unwind actor=%d animation=%d",
                bankId, slot, actor->animationId);
            return -1;
        }
    }
    for (slot = 0; slot < 17; ++slot) {
        Actors_ActorRecord* actor = Actors_actorRecords[slot];
        if (actor == NULL || actor->sharedSequences[bankId - 1] == NULL ||
            actor->animationHeader != (intptr_t)actor->sharedSequences[bankId - 1]) {
            continue;
        }
        if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {
            return -1;
        }
        actor->poseOverrideActive = 0;
        actor->animationHeader = 0;
        actor->animationSet = 0xFF;
        actor->pose.eventScript = NULL;
        actor->pose.nextEventFrame = 0xFFFF;
        actor->blendPose.eventScript = NULL;
        actor->blendPose.nextEventFrame = 0xFFFF;
    }
    result = Content_ReleaseActorSequence(bankId, 1);
    if (result < 0) {
        Core_Errorf("actors.sequence", "shared bank=%d retirement failed status=%d", bankId, result);
    }
    return result;
}

// decomp: BATTLE.PRG func_8007CBDC (src/BATTLE/BATTLE.PRG/146C.c:9010)
void Actors_QueueSharedSequenceWithTextures(int index, int wepId, int texture_slot)
{
    Content_ActorAssetRequest objData;

    objData.assetKind = 7;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.subKind = 2;
    objData.textureSlot = texture_slot;
    Content_QueueActorAssetRequest(&objData);
}

// decomp: BATTLE.PRG vs_battle_loadEtm (src/BATTLE/BATTLE.PRG/146C.c:9022)
void Actors_QueueSequenceEffectTextures(int index, int wepId, int texture_slot)
{
    Content_ActorAssetRequest objData;

    objData.assetKind = 9;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.textureSlot = texture_slot;
    Content_QueueActorAssetRequest(&objData);
}

// decomp: BATTLE.PRG func_8007CC4C (src/BATTLE/BATTLE.PRG/146C.c:9033)
void Actors_ReleaseSequenceEffectTextures(int bank_id) { Content_ReleaseSequenceEffectTexture(bank_id); }

// decomp: BATTLE.PRG func_8007CC6C (src/BATTLE/BATTLE.PRG/146C.c:9035)
void Actors_SetSharedSequenceBank(int bank_id) { Actors_SelectSharedSequenceBank(bank_id); }

// decomp: BATTLE.PRG func_8007CC8C (src/BATTLE/BATTLE.PRG/146C.c:9037)
void Actors_SharedSequenceBank(void) { Actors_SharedSequenceBankId(); }

// decomp: BATTLE.PRG func_8007CEA0 (src/BATTLE/BATTLE.PRG/146C.c:9115)
void Actors_ReturnActorToIdle(int actor_id) { Actors_RestoreActorIdlePose(actor_id); }

// decomp: BATTLE.PRG func_8007CEC0 (src/BATTLE/BATTLE.PRG/146C.c:9117)
void Actors_ReturnAllActorsToIdle(void)
{
    int i;
    for (i = 0; i < 16; ++i) {
        if (Battle_actors[i] != NULL) {
            Actors_ReturnActorToIdle(i);
        }
    }
}

// decomp: BATTLE.PRG func_8009CE9C (src/BATTLE/BATTLE.PRG/30DB0.c:1894)
int Actors_SelectSharedSequenceBank(u_int bank_id)
{
    int i;

    if (bank_id - 1 >= 3) {
        return -1;
    }

    for (i = 0; i < 17; ++i) {
        Actors_ActorRecord* actor = Actors_actorRecords[i];
        if ((actor != NULL) && actor->textureRowOffsets[Actors_sharedSequenceBankIndex] != 0xFF
            && (actor->textureRowOffsets[bank_id - 1] == 0xFF)) {
            if (actor->textureAnimationSlots[0].currentFrame >= 2) {
                Actors_UploadActorTextureFrame(actor, 0, 0);
            }
            if (actor->textureAnimationSlots[1].currentFrame >= 2) {
                Actors_UploadActorTextureFrame(actor, 1, 0);
            }
        }
    }
    Actors_sharedSequenceBankIndex = bank_id - 1;
    return 0;
}

// decomp: BATTLE.PRG func_8009CFA0 (src/BATTLE/BATTLE.PRG/30DB0.c:1918)
int Actors_SharedSequenceBankId(void) { return Actors_sharedSequenceBankIndex + 1; }

// decomp: BATTLE.PRG func_8009D854 (src/BATTLE/BATTLE.PRG/30DB0.c:2188)
void Actors_RequestActorAnimationAdvance(int actor_id)
{
    if (Actors_actorRecords[actor_id] != NULL) {
        Actors_actorRecords[actor_id]->advanceRequested = 1;
    }
}

// decomp: BATTLE.PRG func_8009D934 (src/BATTLE/BATTLE.PRG/30DB0.c:2219)
void Actors_SetActorWeaponDrawn(int actor_id, int weapon_drawn, int transition_mode)
{
    int blend_frames;
    int draw_sound_base;

    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return;
    }

    if (transition_mode == 2) {
        int i;
        actor->instance.weaponDrawn = weapon_drawn;

        for (i = 0; i < 2; ++i) {
            Actors_WeaponModel* equipment_model = Actors_weaponModels[actor_id * 2 + i];
            if (equipment_model != NULL) {
                if (weapon_drawn != 0) {
                    equipment_model->reserved11 = 0x40;
                    equipment_model->reserved12 = 0x40;
                    equipment_model->reserved08Bit4 = 1;
                } else {
                    equipment_model->reserved11 = 0;
                    equipment_model->reserved12 = 0;
                    equipment_model->reserved08Bit4 = 0;
                }
            }
        }
        return;
    }

    blend_frames = 5;

    if (transition_mode != 0) {
        blend_frames = 16;
        draw_sound_base = Actors_drawnWeaponAnimationBases[actor->sequenceModelId];
    } else {
        draw_sound_base = Actors_sheathedWeaponAnimationBases[actor->sequenceModelId];
    }

    if (((int (*)())Event_EventPlaybackState)() == 4) {
        actor->instance.weaponDrawn = weapon_drawn;
        Actors_PlayActorAnimation(actor_id, 1, 0, 16);
        return;
    }

    if (weapon_drawn != actor->instance.weaponDrawn) {
        if (weapon_drawn == 0) {

            if (actor->modelId == 0) {
                Audio_PlayActorSoundPair(actor, draw_sound_base + 1, 0);
            }

            Actors_PlayActorAnimation(actor_id, (transition_mode * 2) + 10, 0, blend_frames);

            actor->weaponPoseActive = 1;
            actor->instance.weaponDrawn = 0;

            Actors_QueueActorAnimation(actor_id, 1, 16, 0);
        } else {

            if (actor->modelId == 0) {
                Audio_PlayActorSoundPair(actor, (int)draw_sound_base, 0);
            }

            actor->instance.weaponDrawn = weapon_drawn;

            Actors_PlayActorAnimation(actor_id, (transition_mode * 2) + 9, 0, blend_frames);

            actor->weaponPoseActive = 1;

            Actors_QueueActorAnimation(actor_id, 1, 16, 0);
        }
    }
}

// decomp: BATTLE.PRG func_8009DBDC (src/BATTLE/BATTLE.PRG/30DB0.c:2296)
void Actors_PlayActorWeaponDrawSound(int actor_id, int sound_offset)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    if (actor != NULL) {
        Audio_PlayActorSoundPair(actor, Actors_drawnWeaponAnimationBases[actor->sequenceModelId] + sound_offset, 0);
    }
}

// decomp: BATTLE.PRG func_8009E070 (src/BATTLE/BATTLE.PRG/30DB0.c:2451)
void Actors_PlayActorActionPose(int actor_id, SVECTOR* facing_target, int pose_kind)
{
    int animation_id;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    switch (pose_kind) {
    case 1:
    case 2:
    case 3:
    case 4:
        animation_id = pose_kind + 0x5F;
        break;
    case 5:
        animation_id = 0x1E;
        break;
    case 6:
        animation_id = 0x1C;
        if (actor->actionPoseActive << 3) {
            animation_id = 1;
        }

        if ((actor->instance.movementMode == 3) || (actor->instance.locomotionState == 2)) {
            return;
        }
        break;
    case 7:
        animation_id = 0x1D;
        break;
    default:
        return;
    }

    actor->instance.facingDelta = Actors_ActorFacingDeltaToPoint(actor, facing_target);
    actor->instance.turnFrames = 0xA;
    actor->runFrames = 0;
    actor->weaponPoseActive = 1;
    Actors_PlayActorAnimation(actor_id, animation_id, 0, 6);
    actor->queuedAnimation = 0;
}

// decomp: BATTLE.PRG func_8009E480 (src/BATTLE/BATTLE.PRG/30DB0.c:2619)
int Actors_IsAshleyTransitionPose(void)
{
    if (Actors_actorRecords[0]->animationId == 0x63) {
        return 1;
    }
    return Actors_actorRecords[0]->animationId == 0xC7;
}

// decomp: BATTLE.PRG func_8009E4B0 (src/BATTLE/BATTLE.PRG/30DB0.c:2627)
int Actors_IsActorActionAnimationBusy(int actor_id)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        if (Field_gimmickInstances[actor_id] == NULL) {
            return -1;
        }
        return 0;
    }

    if ((actor->instance.actionAnimationState != 0) && (actor->instance.actionAnimationState != 5)
        && (actor->instance.actionAnimationState != 11)) {
        return 1;
    }

    if (!actor->pose.holdingLastFrame) {
        int base_animation_id = actor->animationId;
        if (base_animation_id >= 100) {
            base_animation_id -= 100;

            if ((base_animation_id - 9) < 4U) {
                return 1;
            }

            if (base_animation_id == 0x1C) {
                return 1;
            }

            if ((base_animation_id - 0x62) < 4U) {
                return 0;
            }
            if (base_animation_id >= 0x40) {
                return 1;
            }
            actor = Actors_actorRecords[actor_id];
            return 0;
        }

        if (base_animation_id == 0x1C) {
            return 1;
        }
        if (base_animation_id == 0x3B) {
            return 1;
        }
        if ((base_animation_id - 0x40) < 7U) {
            return 1;
        }
        if ((base_animation_id - 0x55) < 7U) {
            return 1;
        }
        if (base_animation_id == 0x5E) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009E5C4 (src/BATTLE/BATTLE.PRG/30DB0.c:2685)
void Actors_RestoreActorIdlePose(int actor_id)
{
    if (Actors_actorRecords[actor_id] == NULL
        || ((Actors_actorRecords[actor_id]->animationId != 0x63)
            && (Actors_actorRecords[actor_id]->animationId != 0xC7))) {
        Actors_RestoreActorBasePose(actor_id, 1);
    }
}

// decomp: BATTLE.PRG func_8009E614 (src/BATTLE/BATTLE.PRG/30DB0.c:2693)
void Actors_RestoreActorNeutralPose(int actor_id) { Actors_RestoreActorBasePose(actor_id, 6); }

// decomp: BATTLE.PRG func_8009E700 (src/BATTLE/BATTLE.PRG/30DB0.c:2735)
void Actors_RestoreActorBasePose(int actorId, int animation)
{
    Actors_ActorRecord* actor;
    Field_GimmickInstance* object;
    int facing;
    int direction;
    int distance;
    int tileDx;
    int tileDz;
    int currentAnimation;
    int reactionAnimation;
    int baseAnimation;

    actor = Actors_actorRecords[actorId];
    if (actor == NULL) {
        object = Field_gimmickInstances[actorId];
        if (object != NULL && (object->mode == 0xF9 || object->mode == 0xFF)
            && object->objectInfo.supportIndex == 0) {
            object->mode = 0;
        }
    } else {
        currentAnimation = actor->animationId;
        if ((currentAnimation != 0xC6) && (currentAnimation != 0xC8)
            && (actor->instance.slot != 0 || actor->instance.locomotionState != 1 || actor->instance.movementPending)) {
            if (actor->instance.movementMode < 2 && actor->instance.poseState != 2
                && actor->instance.poseState != 5) {
                if (actor->instance.reactionState == 3) {
                    Actors_CancelActorTeleportTransition(actor);
                } else if (actor->instance.reactionState != 0) {
                    goto resetState;
                }
                {
                    if (actor->instance.locomotionState == 2) {
                        if (actor->instance.turnFrames != 0 || actor->instance.currentAnimation != 0) {
                            actor->instance.currentAnimation = 0;
                            actor->instance.turnFrames = 0;
                            actor->instance.position.vx += actor->instance.rootMotion.vx;
                            actor->instance.position.vy += actor->instance.rootMotion.vy;
                            actor->instance.position.vz += actor->instance.rootMotion.vz;
                            actor->instance.facing += actor->instance.facingDelta;
                        }
                    } else if (actor->instance.actionAnimationState == 0) {
                        baseAnimation = actor->animationId;
                        if (baseAnimation > 100) {
                            baseAnimation -= 0x64;
                        }
                        if (baseAnimation != 0x62) {
                            distance = 1;
                            if (actor->instance.slot == 0) {
                                reactionAnimation = actor->animationId;
                                if ((u_int)(reactionAnimation - 0x4F) >= 4U) {
                                    if (((u_int)(reactionAnimation - 0x47) >= 4U)
                                        && (reactionAnimation != 0x4C)) {

                                    } else {
                                        goto moveFromReaction;
                                    }
                                } else {
                                    distance = 2;
                                moveFromReaction:
                                    facing = actor->instance.facing + ONE / 8;
                                    facing &= 0xFFF;
                                    facing /= ONE / 4;
                                    direction = facing;
                                    tileDx = 0;
                                    tileDz = 0;
                                    if (direction < 2) {
                                        distance = -distance;
                                    }
                                    if (direction & 1) {
                                        tileDx = distance;
                                    } else {
                                        tileDz = distance;
                                    }
                                    ((void (*)(int, int, int, int))Battle_Ai_ReleaseAiDestinationTile)(actor->instance.currentTileX + tileDx,
                                        actor->instance.currentTileZ + tileDz, tileDx,
                                        tileDz);
                                }
                            }
                            if (actor->actionPoseActive) {
                                Actors_PlayActorAnimation(actor->instance.slot, 0x4B, 0, 0xC);
                            } else {
                                Actors_PlayActorAnimation(actorId, animation, 0, 0x10);
                                actor->runFrames = 0;
                            }
                        }
                    }
                }
            }
        resetState:
            if (actor->instance.locomotionState != 2) {
                actor->instance.locomotionState = 0;
            }
            actor->scaleFrames = 0;
            actor->instance.turnFrames = 0;
            actor->instance.currentAnimation = 0;
            actor->instance.movementPending = 0;
            Actors_ResetActorRunMomentum(actorId);
        }
    }
}

// decomp: BATTLE.PRG func_8009F8DC (src/BATTLE/BATTLE.PRG/30DB0.c:3373)
int Actors_AnimationBattleWindowFrame(int actor_id)
{
    u_char* saved_event_script;
    int battle_window_frame;
    int saved_event_frame;
    Actors_AnimationPose* pose = &Actors_actorRecords[actor_id]->pose;

    if (pose->eventScript == NULL) {
        return 0;
    }
    saved_event_frame = pose->nextEventFrame;
    saved_event_script = pose->eventScript;
    battle_window_frame = Actors_RunAnimationEvents(Actors_actorRecords[actor_id], pose, 3);
    pose->nextEventFrame = saved_event_frame;
    pose->eventScript = saved_event_script;
    return battle_window_frame;
}

// decomp: BATTLE.PRG func_8009F990 (src/BATTLE/BATTLE.PRG/30DB0.c:3401)
void Actors_SetActorSoundOverrides(int actor_id, Actors_ModelEffectSlot* sound_override)
{
    int i;

    if (actor_id == -1) {
        for (i = 0; i < 17; ++i) {
            Actors_ApplyActorSoundOverrides(i, sound_override);
        }
    } else {
        Actors_ApplyActorSoundOverrides(actor_id, sound_override);
    }
}

// decomp: BATTLE.PRG func_8009F9F4 (src/BATTLE/BATTLE.PRG/30DB0.c:3414)
void Actors_ApplyActorSoundOverrides(int actor_id, Actors_ModelEffectSlot* sound_override)
{
    int override_kind;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return;
    }

    override_kind = sound_override->kind;

    if (override_kind == 0) {
        actor->soundEffect.level = 0;
        actor->soundEffect.flags = 0;
    } else if (override_kind == 1) {
        if (sound_override->parameter == 0) {
            actor->soundEffect = *sound_override;
        } else {
            int pan;
            int volume;
            int previous_kind;

            actor->soundEffectTarget = *sound_override;
            actor->soundEffect.parameter = sound_override->parameter;
            previous_kind = actor->soundEffect.kind;

            if (previous_kind == 0) {
                ((void (*)(int, int*, int*, int))Audio_ActorSoundPanAndVolume)(actor_id, &pan, &volume, actor_id);
                actor->soundEffect.level = pan;
                actor->soundEffect.flags = volume;
            } else if (previous_kind == override_kind) {
                ((void (*)(int, int*, int*, int))Audio_ActorSoundPanAndVolume)(actor_id, &pan, &volume, actor_id);
                if (actor->soundEffect.level == 0) {
                    actor->soundEffect.level = pan;
                }
                if (actor->soundEffect.flags == 0) {
                    actor->soundEffect.flags = volume;
                }
            } else if (previous_kind == 2) {
                actor->soundEffect.level = 0x80;
                actor->soundEffect.flags = override_kind;
            }
            if (sound_override->level == 0) {
                actor->soundEffect.level = 0;
            }
            if (sound_override->flags == 0) {
                actor->soundEffect.flags = 0;
            }
        }
    }
    actor->soundEffect.kind = sound_override->kind;
}

// decomp: BATTLE.PRG func_8009FBD8 (src/BATTLE/BATTLE.PRG/30DB0.c:3480)
int Actors_SetActorTravelAnimation(int actor_id, int animation_id)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return -1;
    }

    if (animation_id == -1) {
        actor->travelAnimation = 0xD;
    } else {
        actor->travelAnimation = animation_id;
    }

    return 0;
}

// decomp: BATTLE.PRG func_8009FC20 (src/BATTLE/BATTLE.PRG/30DB0.c:3498)
int Actors_ActorTravelAnimation(int actor_id, int* animation_out)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return -1;
    }

    *animation_out = actor->travelAnimation;

    return 0;
}

// decomp: BATTLE.PRG func_800A01C8 (src/BATTLE/BATTLE.PRG/30DB0.c:3710)
int Actors_QueueActorAnimation(int actor_id, int animation_id, int blend_frames, int start_frame)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return -1;
    }

    actor->queuedAnimation = animation_id;
    actor->queuedParameterA = blend_frames;
    actor->queuedParameterB = start_frame;

    return 0;
}

// decomp: BATTLE.PRG func_800A0204 (src/BATTLE/BATTLE.PRG/30DB0.c:3725)
int Actors_PlayActorAnimation(int actorId, int animation, int arg2, int blendFrames)
{
    u_short* frames;
    Actors_ActorRecord* actor;
    int keepFlag15;
    int anim;
    intptr_t found;

    if (animation == 0) {
        return 0;
    }
    actor = Actors_actorRecords[actorId];
    if (actor == NULL) {
        return -1;
    }
    if (actor->animationLocked) {
        return 0;
    }
    if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {
        return -1;
    }
    actor->animationEventFlag = 0;
    actor->poseOverrideActive = 0;
    keepFlag15 = 0;
    if (animation == 25 || animation == 26) {
        keepFlag15 = 1;
    }
    if (!keepFlag15) {
        actor->isRunning = 0;
    }
    actor->textureAnimationFrames[0].overlayIndex = 0;
    actor->textureAnimationFrames[1].overlayIndex = 0;
    Actors_SetActorTextureAnimation(actorId, 1, 1, 1);
    Actors_ClearActorBonePoseLocks(actor);
    if ((actor->isRunning) && !keepFlag15) {
        actor->isRunning = 0;
    }
    anim = animation;
    actor->fastAnimation = 0;

    if (anim < 100) {
        if (actor->instance.motionLocked) {
            if (anim == 1 || anim == 6) {
                anim = 8;
            }
        } else if (actor->instance.movementMode == 3) {
            if (anim == 1) {
                anim = 48;
                if (actor->instance.isJumping) {
                    anim = 35;
                }
            }
        } else if (actor->instance.isJumping) {
            if (anim == 1) {
                anim = 31;
            } else if (anim == 13) {
                anim = 32;
            }
        }
        if (actor->instance.weaponDrawn) {
            anim += 100;
        } else if (actor->actionPoseActive) {
            if (anim == 1) {
                anim = 75;
            } else if (anim == 13) {
                anim = 84;
                if (actor->animationId == anim) {
                    return 0;
                }
            } else if (anim == 84) {
                if (actor->animationId == anim) {
                    return 0;
                }
            }
        }
    }

    actor->weaponPoseActive = 0;
    if (!actor->instance.weaponDrawn && (anim == 36 || anim == 39)) {
        actor->instance.locomotionState = 1;
    setHeading:
        actor->moveFacing = (actor->instance.facing + actor->instance.facingDelta) & 0xFFF;
    } else if (!actor->instance.weaponDrawn && (anim == 41 || anim == 59)) {
        actor->instance.locomotionState = 2;
        goto setHeading;
    } else if (actor->instance.rootMotion.vx == 0 && actor->instance.rootMotion.vy == 0
               && actor->instance.rootMotion.vz == 0) {
        actor->instance.locomotionState = 0;
        actor->instance.movementPending = 0;
    }

retry:
    found = Actors_FindActorAnimationHeader(actor, anim, &frames);
    if (found == 0 || frames == NULL) {
        actor->isRunning = 0;
        if (anim == 0) {
            // Returns without a value (v0 holds the updated flags word).
            return;
        }
        anim = 0;
        goto retry;
    }

    actor->animationId = anim;
    if (((int (*)())Event_EventPlaybackState)() == 4 || actor->animationHeader != found || Actors_lastResolvedSequenceIndex != actor->animationSet) {
        actor->animationSet = Actors_lastResolvedSequenceIndex;
        actor->animationHeader = found;
        actor->animationLength = *frames;
        ((void (*)(void*, void*))Actors_InitializeActorAnimationPose)(actor, &actor->pose);
        actor->queuedAnimation = 0;
        Actors_BeginActorShadowBlend(actor, blendFrames);
        if (actor->collisionRadius >= 0x80) {
            blendFrames *= 2;
        }
        if (blendFrames >= 2) {
            actor->blendFrames = blendFrames;
            actor->blendFrame = 1;
        } else {
            actor->blendFrames = 0;
            actor->blendFrame = 0;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A0618 (src/BATTLE/BATTLE.PRG/30DB0.c:3850)
int Actors_PlayActorAnimationAtFrame(int actor_id, int animation_id, int blend_frames, int start_frame)
{
    Actors_ActorRecord* actor;

    int play_result = Actors_PlayActorAnimation(actor_id, animation_id, 0, blend_frames);

    if (play_result < 0) {
        return play_result;
    }

    if (start_frame == 0) {
        return 0;
    }

    start_frame -= State_tickSpeed / 2;
    actor = Actors_actorRecords[actor_id];

    if (start_frame > 0) {
        actor->framesPerStep = start_frame;
        Actors_AdvanceSkeletalAnimation(actor, &actor->pose, 1);
    }

    actor->framesPerStep = 1;

    return 0;
}

// decomp: BATTLE.PRG func_800A0870 (src/BATTLE/BATTLE.PRG/30DB0.c:3948)
void Actors_SetActorTransitionMode(int control_byte) { Actors_reservedControlByte = control_byte; }

// decomp: BATTLE.PRG func_800A09D8 (src/BATTLE/BATTLE.PRG/30DB0.c:3987)
void Actors_SetActorAnimationLocked(int actor_id, int locked)
{
    if (Actors_actorRecords[actor_id] != NULL) {
        Actors_actorRecords[actor_id]->animationLocked = locked;
    }
}

// decomp: BATTLE.PRG func_800A249C (src/BATTLE/BATTLE.PRG/38C1C.c:578)
void Actors_PlayActorTurnAnimation(int actor_id, int target_facing)
{
    int turn_animation_id;
    int facing_delta;
    int current_animation_id;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor->instance.motionLocked) {
        return;
    }

    facing_delta = target_facing - actor->instance.facing;

    if (facing_delta > 0x800) {
        facing_delta -= 0x1000;
    }

    if (facing_delta < -0x800) {
        facing_delta += 0x1000;
    }

    if (facing_delta == 0) {
        return;
    }

    actor->turnState.mode = 6;

    current_animation_id = actor->animationId;

    if ((current_animation_id == 0x76) || (actor->animationId == 0x75) || actor->instance.supportLayer) {
        return;
    }

    turn_animation_id = 0x1F;

    if (!(actor->instance.isJumping)) {

        turn_animation_id = 0x11;

        if (facing_delta < 0) {
            turn_animation_id = 0x12;
        }
    }

    ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(actor_id, turn_animation_id, 0, 0xA);
}

// decomp: BATTLE.PRG func_800A6AA0 (src/BATTLE/BATTLE.PRG/3A1A0.c:2032)
int Actors_UpdateActorLocomotionAnimation(Actors_ActorRecord* actor, int speed, int direction)
{
    u_short* entry;
    int next;
    int frames;
    int quadrant;
    int animation;
    u_int state;
    u_int walkState;
    int step;

    animation = actor->animationId;
    if (animation >= 100) {
        animation -= 100;
    }
    quadrant = 0;
    if (actor->turnState.mode == 1) {
        quadrant = (short)actor->turnState.facing - (short)actor->turnState.direction + ONE / 8;
        quadrant &= 0xFFF;
        quadrant /= ONE / 4;
    }
    if (actor->actionPoseActive) {
        next = 0x54;
        if (speed > 0) {
            frames = 4;
        } else {
            next = 0x4B;
            frames = 10;
        }
        if (animation == 0x5F) {
            return;
        }
        Actors_PlayActorAnimation(actor->instance.slot, next, 0, frames);
        return;
    }
    if (actor->instance.isJumping) {
        if (speed >= actor->runSpeed) {
            next = Actors_kFastQuadrantMoveAnimations[quadrant];
            frames = 20;
        } else if (speed > 0) {
            next = Actors_kSlowQuadrantMoveAnimations[quadrant];
            frames = 20;
        } else {
            next = 1;
            frames = 10;
        }
        Actors_PlayActorAnimation(actor->instance.slot, next, 0, frames);
        return;
    }
    if (actor->isRunning) {
        if ((u_int)(animation - 0x19) < 2) {
            return;
        }
        next = 0x1A;
        if ((u_int)(animation - 0x12) >= 2) {
            if (animation == 0x14 || animation == 0x11) {
                next = 0x19;
            } else {
                actor->isRunning = 0;
                return;
            }
        }

        Actors_FindActorAnimationHeader(actor, next, &entry);
        if (entry == NULL) {
            next = 6;
        }
        Actors_PlayActorAnimation(actor->instance.slot, next, 0, 8);
    } else if (speed >= actor->runSpeed - 1) {
        if ((u_int)(animation - 0x13) < 2) {
            return;
        }
        if ((u_int)(animation - 0xF) < 4 || (u_int)(animation - 0x2E) < 11
            || (u_int)(animation - 9) < 4 || animation == 1 || animation == 6) {
            state = actor->locomotionAnimationState;
            actor->locomotionAnimationState ^= 1;
            Actors_PlayActorAnimation(actor->instance.slot, state + 0x13, 0, 6);
            return;
        }
        next = 0x13;
        if (animation == 0xD || animation == 0xF) {
            next = 0x14;
        }
        Actors_QueueActorAnimation(actor->instance.slot, next, 4, 0);
        return;
    } else if (speed > 0) {
        if ((u_int)(animation - 0x2E) >= 11 && (u_int)(animation - 9) >= 4) {
            if ((u_int)(animation - 0xD) < 2) {
                if (quadrant != 2) {
                    return;
                }
                goto walk_toggle;
            } else if ((u_int)(animation - 0xF) < 2) {
                if (quadrant == 2) {
                    return;
                }
                goto walk_toggle;
            } else {
                if (animation == 1 || animation == 6 || animation == 0x11
                    || animation == 0x12) {
                    goto walk_toggle;
                }
                goto walk_start;
            }
        } else {
        walk_toggle:
            walkState = actor->locomotionAnimationState;
            step = walkState;
            next = step + 0xD;
            actor->locomotionAnimationState ^= 1;
            if (quadrant == 2) {
                next = step + 0xF;
            }
            Actors_PlayActorAnimation(actor->instance.slot, next, 0, 6);
            return;
        walk_start:
            next = 0xD;
            if (animation == 0x13 || animation == 0xF || animation == 0xD) {
                next += 1;
            }
            if (quadrant == 2) {
                next += 2;
            }
            Actors_QueueActorAnimation(actor->instance.slot, next, 4, 0);
            return;
        }
    } else {
        if ((u_int)(animation - 0xD) < 4) {
            Actors_PlayActorAnimation(actor->instance.slot, 6, 0, 10);
            Actors_QueueActorAnimation(actor->instance.slot, 1, 10, 0);
            return;
        }
        if (animation != 1 && actor->turnState.mode != 6) {
            Actors_PlayActorAnimation(actor->instance.slot, 1, 0, 10);
            actor->locomotionCounter = 0;
        }
        actor->locomotionCounter += State_tickSpeed / 2;
        if (actor->locomotionCounter >= 250) {
            actor->locomotionCounter = 250;
        }
    }
}

// decomp: BATTLE.PRG func_800AC440 (src/BATTLE/BATTLE.PRG/40564.c:2199)
void Actors_SetActorBonePoseLock(Actors_ActorRecord* actor, int bone_index, int lock_pose)
{
    int bone_length;
    int i;
    Actors_ShpBone* bone = &actor->instance.model->bones[bone_index];

    for (i = 0; i < 2; ++i) {
        if (lock_pose == 0) {
            if (actor->lockedBones[i] == bone_index) {
                actor->lockedBones[i] = 0;
                bone->poseLockSlot = 0;
                return;
            }
        } else if (actor->lockedBones[i] == 0) {
            ((void (*)(int, int, void*, int*))Actors_ActorAnchorMatrix)(actor->instance.slot, bone_index, &actor->lockedBoneMatrices[i], &bone_length);
            bone->poseLockSlot = i + 1;
            actor->lockedBones[i] = bone_index;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_800AC500 (src/BATTLE/BATTLE.PRG/40564.c:2221)
void Actors_ClearActorBonePoseLocks(Actors_ActorRecord* actor)
{
    int i;

    actor->lockedBones[0] = 0;
    actor->lockedBones[1] = 0;

    for (i = 0; i < actor->instance.boneCount; ++i) {
        actor->instance.model->bones[i].poseLockSlot = 0;
    }
}

// decomp: BATTLE.PRG func_800ACFA0 (src/BATTLE/BATTLE.PRG/40564.c:2561)
void Actors_DecodeInitialPoseVector(short* pose_key, u_char* encoded_vector, int base_animation_index)
{
    int component_value;
    component_value = encoded_vector[0];
    component_value <<= 8;
    component_value |= encoded_vector[1];
    component_value *= 2;
    pose_key[0] = component_value;

    component_value = encoded_vector[2];
    component_value <<= 8;
    component_value |= encoded_vector[3];
    component_value *= 2;
    pose_key[1] = component_value;

    component_value = encoded_vector[4];
    component_value <<= 8;
    component_value |= encoded_vector[5];
    component_value *= 2;
    pose_key[2] = component_value;

    pose_key[3] = 6;

    if (base_animation_index != 0xFF) {
        pose_key[3] = 0;
    }

    pose_key[339] = 0;
}

// decomp: BATTLE.PRG func_800AD008 (src/BATTLE/BATTLE.PRG/40564.c:2623)
void Actors_InitializeActorAnimationPose(Actors_ActorRecord* actor, Actors_AnimationPose* pose)
{
    u_short* header;
    Actors_SeqAnimationBank* anim;
    u_char* data;
    SVECTOR* key;
    int frame;
    int i;

    if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {
        return;
    }
    actor->poseOverrideActive = 0;
    pose->frame = 0;
    pose->eventWait = 0;
    *(int*)&pose->scaleKeys[41].vx = 0;
    *(int*)&pose->scaleKeys[41].vz = 0;
    pose->trackStates[41].pad = 0;

    anim = (Actors_SeqAnimationBank*)Actors_FindActorAnimationHeader(actor, actor->animationId, &header);
    if (anim == NULL) {
        return;
    }
    if (header[2] == 0) {
        pose->nextEventFrame = 0xFFFF;
    } else {
        data = (u_char*)VS_ONDISC(anim, animationData, u_char *);
        data += header[2];
        i = data[0];
        if (i == 0xFF) {
            i = data[1] + 0xFF;
            data += 2;
        } else {
            data += 1;
        }
        pose->nextEventFrame = i;
        pose->eventScript = data;
    }
    pose->holdingLastFrame = 0;
    pose->animationId = actor->animationId;
    if (header[4] != 0) {
        key = &pose->scaleKeys[41];
        data = (u_char*)VS_ONDISC(anim, animationData, u_char *);
        data += header[4];
        Actors_DecodeInitialPoseVector(key, data, 0xFF);
    }
    frame = ((u_char*)header)[2];
    if (frame != 0xFF) {
        header = (u_short*)((u_char*)anim + ((actor->instance.boneCount * 4 + 10) * frame + 8));
    }
    key = &pose->boneKeys[41];
    data = (u_char*)VS_ONDISC(anim, animationData, u_char *);
    data += header[3];
    Actors_DecodeInitialPoseVector(key, data, frame);
    for (i = 0; i < actor->instance.boneCount; ++i) {
        key = &pose->boneKeys[i];
        data = (u_char*)VS_ONDISC(anim, animationData, u_char *);
        data += header[5 + i];
        Actors_DecodeInitialPoseVector(key, data, frame);
    }
    if (((u_char*)header)[3] & 1) {
        for (i = 0; i < actor->instance.boneCount; ++i) {
            data = (u_char*)VS_ONDISC(anim, animationData, u_char *);
            data += header[actor->instance.boneCount + i + 5];
            if (data[0] == 0xFE && data[1] == 1) {
                data += 2;
                key = &pose->scaleKeys[i];
                Actors_DecodeInitialPoseVector(key, data, frame);
                pose->scaleKeys[i].pad = 0;
                pose->scaleKeys[i].vx >>= 1;
                pose->scaleKeys[i].vy >>= 1;
                pose->scaleKeys[i].vz >>= 1;
                if (frame == 0xFF) {
                    pose->scaleKeys[i].pad = 8;
                }
            } else {
                pose->scaleKeys[i].vx = data[0];
                pose->scaleKeys[i].vy = data[1];
                pose->scaleKeys[i].vz = data[2];
                pose->scaleKeys[i].pad = 0;
                if (frame == 0xFF) {
                    pose->scaleKeys[i].pad = 3;
                }
            }
            pose->trackStates[i].pad = 0;
        }
    } else {
        for (i = 0; i < actor->instance.boneCount; ++i) {
            *(int*)&pose->scaleKeys[i].vx = 0x400040;
            *(int*)&pose->scaleKeys[i].vz = 0x40;
            pose->scaleKeys[i].pad = 0;
            pose->trackStates[i].pad = 0;
        }
    }
}

// decomp: BATTLE.PRG func_800AD494 (src/BATTLE/BATTLE.PRG/40564.c:2719)
intptr_t Actors_FindActorAnimationHeader(void* actor_context, int animation_id, u_short** header_out)
{
    Actors_ActorRecord* actor = actor_context;
    Actors_SeqAnimationBank* p;
    int idx;

    *header_out = NULL;

    if (animation_id == 0) {
        p = actor->ownedSequence;
        idx = 0;
    } else if (animation_id == 0xFF) {
        p = actor->sharedSequences[3];
        if ((actor->instance.slot == 0) || (actor->modelId == 2)) {
            idx = (*(u_char (*))&Battle_specialActionPitch);
        } else {
            if (actor->sequenceModelId == 0) {
                return 0;
            }
            idx = actor->sequenceModelId - 1;
            idx = idx * 3 + (*(u_char (*))&Battle_specialActionPitch);
        }
        if (idx >= actor->battleAnimationCount) {
            return 0;
        }
    } else if (animation_id == 0xFE) {
        p = actor->sharedSequences[3];
        if (actor->sequenceModelId == 0) {
            return 0;
        }
        idx = actor->sequenceModelId - 1;
        idx = idx * 3 + 31;
        if (idx >= actor->battleAnimationCount) {
            return 0;
        }
    } else if (animation_id >= 0xDC) {
        p = actor->sharedSequences[Actors_sharedSequenceBankIndex];
        idx = animation_id - 0xDC;
    } else if (animation_id >= 0xCA) {
        p = Actors_sequenceWorkBuffer;
        idx = animation_id - 0xCA;
    } else if (animation_id >= 100) {
        p = actor->battleSequence;
        idx = animation_id - 100;
        if (idx >= actor->commonAnimationCount) {
            return 0;
        }
    } else {
        p = actor->commonSequence;
        idx = animation_id;
        if (idx >= actor->sequenceFlags) {
            return 0;
        }
    }

    if (p == NULL) {
        return 0;
    }

    idx = VS_ONDISC(p, animationMap, u_char *)[idx];
    Actors_lastResolvedSequenceIndex = idx;

    if (idx == 0xFF) {
        *header_out = NULL;
    } else {
        *header_out = &p->headers[(((actor->instance.boneCount * 4) + 10) * idx) >> 1];
    }
    return (intptr_t)p;
}

// decomp: BATTLE.PRG func_800AD714 (src/BATTLE/BATTLE.PRG/44F14.c:168)
int Actors_RunAnimationEvents(Actors_ActorRecord* actor, Actors_AnimationPose* state, int mode)
{
    int op;
    u_short* point;
    int arg;
    int sfx[2];
    int drawOp;
    short delta;
    short packed;
    int value;
    u_char* script;
    Actors_WeaponModel* weapon;
    Actors_ShadowBlend* motion = &actor->shadowBlend;
    int id = actor->instance.slot;

    script = state->eventScript;
loop: {
    op = *script;
    switch (mode) {
    case 0:
        break;
    case 1:
        if ((u_char)op == 0x2C || (u_char)op == 0x19 || (u_int)(op - 0x1D) < 0x12
            || (u_int)(op - 0x10) < 3 || (u_int)(op - 0x33) < 4 || (u_int)(op - 0x38) < 2
            || (u_int)(op - 0x20) < 8) {
            break;
        }
        goto next;
    case 2:
        if ((u_int)(op - 0x24) >= 4) {
            goto next;
        }
        break;
    case 3:
        if (op != 2) {
            goto next;
        }
        return state->nextEventFrame;
    }
    switch ((u_char)op) {
    case 14:
        Effects_StartWeaponTrail(id, script[1]);
        break;
    case 15:
        Effects_FadeWeaponTrails(id, script[1]);
        break;
    case 29:
        actor->paletteFadeActive = 1;
        actor->paletteFadeState[0] = 0;
        actor->paletteFadeReverse = 0;
        actor->paletteFadeState[1] = 0x20;
        break;
    case 44:
        ((void (*)(int index, u_char* arg))Actors_UploadActorTextureOverlay)(id, script);
        break;
    case 28:
        Field_ApplyGimmickInteractionCommand(actor, script[1]);
        break;
    case 23:
        arg = actor->animationId - 1;
    playAnimation:
        if (arg >= 100) {
            arg -= 100;
        }
        Actors_PlayActorAnimation(id, arg, 0, 0);
        goto reload;
    case 24:
        arg = actor->animationId + 1;
        goto playAnimation;
    case 19:
        arg = script[1];
        sfx[0] = 13;
        goto setAnimation;
    case 55:
        if (state->eventWait == 0) {
            state->eventWait = script[1];
        } else if (--state->eventWait == 0) {
            break;
        }
        state->frame--;
        return 0;
    case 20:
        arg = script[1];
        sfx[0] = 1;
    setAnimation:
        Actors_PlayActorAnimation(id, sfx[0], 0, arg);
        goto reload;
    case 26:
    case 27:
        arg = 0;
        drawOp = (u_char)op;
        sfx[0] = script[1];
        for (; arg < 20; ++arg) {
            weapon = Actors_weaponModels[arg];
            if (weapon != NULL && weapon->boneCount != 0 && weapon->attachmentBone != 0
                && weapon->actorId == id) {
                if (drawOp == 26) {
                    if (((short (*)(void))Event_EventPlaybackState)() != 4 && !actor->instance.weaponDrawn) {
                        goto next;
                    }
                    weapon->reserved08Bit4 = 1;
                    weapon->reserved11 = 0;
                    weapon->reserved12 = 0x40;
                } else {
                    weapon->reserved11 = 0x40;
                    weapon->reserved12 = 0;
                }
                weapon->reserved13 = sfx[0];
            }
        }
        break;
    case 21:
        Actors_PlayActorAnimation(id, script[1], 0, 0);
        actor->animationEventFlag = 0;
        goto reload;
    case 22:
        arg = script[1];
        sfx[0] = script[2];
        Actors_PlayActorAnimation(id, arg, 0, sfx[0]);
        actor->animationEventFlag = 0;
        goto reload;
    case 4:
        arg = script[1];
        if (arg == 0x58) {
            if (actor->sequenceModelId == 0 || !actor->instance.weaponDrawn) {
                arg = Actors_drawnWeaponSoundIds[0];
            } else {
                arg = Actors_drawnWeaponSoundIds[actor->sequenceModelId];
            }
        }
        if (actor->instance.position.vy < 3000) {
            Audio_PlayActorSoundPair(actor, arg, 0);
        }
        break;
    case 5: {
        Actors_ActorRecord* target;

        arg = script[1];
        target = Actors_actorRecords[id];
        sfx[1] = script[2];
        if (target->soundEffect.kind == 2) {
            sfx[0] = 0x80;
        } else {
            if ((*(int*)((char*)target + 8) & 0x3F9F00) != 0x8000) {
                Actors_ProjectActorTaggedBone(id, 0xFF, &target->soundPosition, 0);
            }
            sfx[0] =
                Audio_SfxPanAndVolume(*(int*)&target->soundPosition, target->soundPosition.vz) >> 16;
            if (target->soundEffect.level != 0) {
                sfx[0] = target->soundEffect.level;
            }
        }
        if (sfx[1] != 0) {
            Audio_PlaySfx(0x7E, arg, sfx[0], sfx[1]);
        }
        break;
    }
    case 7: {
        Actors_ActorRecord* target;

        arg = script[1];
        target = Actors_actorRecords[id];
        if (target->soundEffect.kind == 2) {
            sfx[0] = 0x80;
            sfx[1] = 0;
        } else {
            if ((*(int*)((char*)target + 8) & 0x3F9F00) != 0x8000) {
                Actors_ProjectActorTaggedBone(id, 0xFF, &target->soundPosition, 0);
            }
            value = Audio_SfxPanAndVolume(*(int*)&target->soundPosition, target->soundPosition.vz);
            sfx[0] = value >> 16;
            if (target->soundEffect.level != 0) {
                sfx[0] = target->soundEffect.level;
            }
            sfx[1] = value & 0xFFFF;
            if (target->soundEffect.flags != 0) {
                sfx[1] = target->soundEffect.flags;
            }
        }
        if (sfx[1] != 0) {
            Audio_PlaySfx(0xFF000, arg, sfx[0], sfx[1]);
        }
        break;
    }
    case 8: {
        Actors_ActorRecord* target;

        arg = script[1];
        target = Actors_actorRecords[id];
        sfx[1] = script[2];
        if (target->soundEffect.kind == 2) {
            sfx[0] = 0x80;
        } else {
            if ((*(int*)((char*)target + 8) & 0x3F9F00) != 0x8000) {
                Actors_ProjectActorTaggedBone(id, 0xFF, &target->soundPosition, 0);
            }
            sfx[0] =
                Audio_SfxPanAndVolume(*(int*)&target->soundPosition, target->soundPosition.vz) >> 16;
            if (target->soundEffect.level != 0) {
                sfx[0] = target->soundEffect.level;
            }
        }
        if (sfx[1] != 0) {
            Audio_PlaySfx(0xFF000, arg, sfx[0], sfx[1]);
        }
        break;
    }
    case 6:
        Audio_StopSfx(0x7E, script[1]);
        break;
    case 9:
        Audio_StopSfx(0xFF000, script[1]);
        break;
    case 11:
        Audio_PlayActorSurfaceContactSound(id, actor, op);
        break;
    case 1:
        ((void (*)(void*, void*))Actors_InitializeActorAnimationPose)(actor, state);
        actor->animationEventFlag = 1;
    reload:
        arg = state->nextEventFrame;
        script = state->eventScript;
        state->frame = 0;
        goto check;
    case 12:
        if (!(actor->animationEventFlag)) {
            Effects_StartEffectTrack(script[1]);
        }
        break;
    case 13:
        if (Effects_effectBusyFlags == 1 && !(actor->advanceRequested)) {
            ((void (*)(void*, void*))Actors_InitializeActorAnimationPose)(actor, state);
            actor->animationEventFlag = 1;
            goto reload;
        }
        actor->advanceRequested = 0;
        break;
    case 2:
        if (Effects_QueueActionImpactEffects(id) == 0) {
            sfx[0] = 1;
            goto setAnimation;
        }
        break;
    case 36:
    case 37:
    case 38:
        delta = script[1];
        packed = delta << 8;
        packed |= script[2];
        arg = packed & 0x1FF;
        packed >>= 9;
        delta = packed;
        if (mode == 2) {
            delta = -packed;
        }
        ((short*)((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *) + arg))[op - 36] += delta;
        actor->poseOverrideActive = 1;
        break;
    case 39:
        delta = script[1];
        packed = delta << 8;
        packed |= script[2];
        arg = packed & 0x1FF;
        packed >>= 9;
        if (mode == 2) {
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vx -= packed;
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vy -= (signed char)script[3];
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vz -= (signed char)script[4];
        } else {
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vx += packed;
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vy += (signed char)script[3];
            ((SVECTOR*)VS_ONDISC(actor->instance.model, vertexSection, void *))[arg].vz += (signed char)script[4];
        }
        actor->poseOverrideActive = 1;
        break;
    case 30:
    case 31:
        arg = op - 30;
        actor->textureAnimationFrames[arg].overlayIndex = script[1];
        actor->textureAnimationFrames[arg].sourceX = script[2];
        actor->textureAnimationFrames[arg].sourceY = script[3];
        actor->textureAnimationFrames[arg].columns = script[4];
        break;
    case 32:
        Actors_SetActorTextureAnimation(id, 1, 1, 1);
        break;
    case 33:
        Actors_SetActorTextureAnimation(id, 1, 1, 0);
        break;
    case 34:
        Actors_SetActorTextureAnimation(id, 1, 0, 1);
        break;
    case 35:
        Actors_SetActorTextureAnimation(id, 1, 0, 0);
        break;
    case 40:
        ((void (*)())Actors_IgnoreAnimationEvent)(script);
        break;
    case 41:
        ((void (*)())Actors_IgnoreAnimationEvent)(script);
        script += 5;
        break;
    case 10:
        Audio_PlayActorModelSound(&actor->instance, script[1]);
        break;
    case 25:
        actor->weaponPoseActive = 1;
        break;
    case 51: {
        int lo;
        delta = script[1];
        lo = script[2];
        motion->radiusExpandDelta = ((delta << 8) | lo) - motion->radiusExpandRate;
    }
    translateX:
        arg = script[3];
    applyTranslation:
        if (arg == 0) {
            motion->radiusExpandRate += motion->radiusExpandDelta;
            motion->radiusShrinkRate += motion->radiusShrinkDelta;
        }
        motion->radiusFrames = arg;
        break;
    case 52: {
        int lo;
        delta = script[1];
        lo = script[2];
        motion->radiusShrinkDelta = ((delta << 8) | lo) - motion->radiusShrinkRate;
    }
        goto translateX;
    case 53: {
        int lo;
        delta = script[1];
        lo = script[2];
        motion->radiusExpandDelta = ((delta << 8) | lo) - motion->radiusExpandRate;
    }
        {
            int lo;
            delta = script[3];
            lo = script[4];
            motion->radiusShrinkDelta = ((delta << 8) | lo) - motion->radiusShrinkRate;
        }
        arg = script[5];
        goto applyTranslation;
    case 54: {
        int lo;
        delta = script[1];
        lo = script[2];
        motion->levelDelta = ((delta << 8) | lo) - motion->level;
    }
        if (motion->levelDelta < -ONE / 2) {
            motion->levelDelta += ONE;
        }
        if (motion->levelDelta > ONE / 2) {
            motion->levelDelta -= ONE;
        }
        arg = script[3];
        if (arg == 0) {
            motion->level = (motion->level + motion->levelDelta) & 0xFFF;
        }
        motion->levelFrames = arg;
        break;
    case 56:
    case 57:
        Actors_SetActorBonePoseLock(actor, script[1], op - 56);
        break;
    case 58:
        actor->instance.actionAnimationState = 2;
        if (actor->companionSlot >= 2) {
            Actors_actorRecords[actor->companionSlot]->instance.actionAnimationState = 2;
        }
        break;
    case 59:
        actor->instance.rootMotion.vy = 3;
        actor->instance.movementMode = 3;
        actor->instance.locomotionState = 0;
        actor->instance.movementPending = 0;
        break;
    case 61:
        actor->fastAnimation = 0;
        break;
    case 62:
        actor->fastAnimation = 1;
        break;
    case 60:
        arg = script[1];
        point = Actors_animationEventPoints[0];
        point += arg * 4;
        motion->radiusExpandDelta = point[0] - motion->radiusExpandRate;
        motion->radiusShrinkDelta = point[1] - motion->radiusShrinkRate;
        if (motion->radiusFrames == 0) {
            motion->radiusExpandRate += motion->radiusExpandDelta;
            motion->radiusShrinkRate += motion->radiusShrinkDelta;
        }
        delta = point[2];
        motion->levelDelta = delta - motion->level;
        if (motion->levelDelta < -ONE / 2) {
            motion->levelDelta += ONE;
        }
        if (motion->levelDelta > ONE / 2) {
            motion->levelDelta -= ONE;
        }
        if (motion->levelFrames == 0) {
            motion->level = (motion->level + motion->levelDelta) & 0xFFF;
        }
        break;
    case 63:
        if (!(actor->animationEventFlag)) {
            Effects_SpawnSourceActorEffect();
        }
        break;
    case 64:
        actor->weaponPoseActive = 0;
        break;
    case 0:
    default:
        state->nextEventFrame = 0xFFFF;
        return 0;
    }
next:
    script += Actors_animationEventLengths[op];
    arg = *script;
    if (arg == 0xFF) {
        arg = script[1] + 0xFF;
        script += 2;
    } else {
        ++script;
    }
    state->nextEventFrame = arg;
    state->eventScript = script;
check:
    if (!((mode >= 2 ? (u_short)actor->animationLength : (short)state->frame) < arg)) {
        goto loop;
    }
}
    return 0;
}

// decomp: BATTLE.PRG func_800AE46C (src/BATTLE/BATTLE.PRG/44F14.c:606)
void Actors_IgnoreAnimationEvent(void) { }

// decomp: BATTLE.PRG func_800AE47C (src/BATTLE/BATTLE.PRG/44F14.c:610)
int Actors_RestartActorAnimationEvents(Actors_ActorRecord* actor)
{
    u_short* animation_header;
    int next_event_frame;
    void* event_script;

    Actors_SeqAnimationBank* animation_bank = (Actors_SeqAnimationBank*)Actors_FindBoundAnimationHeader(actor, &animation_header);
    u_char* event_data;
    if (animation_bank == NULL || animation_header == NULL || (animation_header[2] == 0 && actor->poseOverrideActive)) {
        Core_Errorf("actors.sequence", "cannot unwind actor=%u animation=%u: unavailable events",
            (unsigned)actor->instance.slot, (unsigned)actor->animationId);
        return -1;
    }
    if (animation_header[2] == 0) {
        return 0;
    }
    event_data = VS_ONDISC(animation_bank, animationData, u_char*);
    event_data += animation_header[2];

    next_event_frame = event_data[0];

    if (next_event_frame == 0xFF) {
        event_script = event_data + 2;
        next_event_frame = event_data[1] + 0xFF;
    } else {
        event_script = event_data + 1;
    }

    actor->blendPose.nextEventFrame = next_event_frame;
    actor->blendPose.eventScript = event_script;

    /* The decoder reads state->unk544 itself; this caller passes an ignored fourth
     * argument. */
    ((void (*)())Actors_RunAnimationEvents)(actor, &actor->blendPose, 2, event_script);
    return 0;
}

// decomp: BATTLE.PRG func_800AE980 (src/BATTLE/BATTLE.PRG/44F14.c:781)
void Actors_BlendActorPoseHalfway(Actors_AnimationPose* dst, Actors_AnimationPose* src, int count)
{
    int i;

    dst->boneKeys[41].vx = (dst->boneKeys[41].vx + src->boneKeys[41].vx) >> 1;
    dst->boneKeys[41].vy = (dst->boneKeys[41].vy + src->boneKeys[41].vy) >> 1;
    dst->boneKeys[41].vz = (dst->boneKeys[41].vz + src->boneKeys[41].vz) >> 1;

    for (i = 0; i < count; ++i) {
        int x = src->boneKeys[i].vx - dst->boneKeys[i].vx;
        int y;
        int z;

        if (x >= ONE) {
            x -= ONE * 2;
        } else if (x < -ONE) {
            x += ONE * 2;
        }

        dst->boneKeys[i].vx += x >> 1;
        y = src->boneKeys[i].vy - dst->boneKeys[i].vy;

        if (y >= ONE) {
            y -= ONE * 2;
        } else if (y < -ONE) {
            y += ONE * 2;
        }

        dst->boneKeys[i].vy += y >> 1;
        z = src->boneKeys[i].vz - dst->boneKeys[i].vz;

        if (z >= ONE) {
            z -= ONE * 2;
        } else if (z < -ONE) {
            z += ONE * 2;
        }

        dst->boneKeys[i].vz += z >> 1;

        dst->scaleKeys[i].vx = (dst->scaleKeys[i].vx + src->scaleKeys[i].vx) >> 1;
        dst->scaleKeys[i].vy = (dst->scaleKeys[i].vy + src->scaleKeys[i].vy) >> 1;
        dst->scaleKeys[i].vz = (dst->scaleKeys[i].vz + src->scaleKeys[i].vz) >> 1;
    }
}

// decomp: BATTLE.PRG func_800AF6E8 (src/BATTLE/BATTLE.PRG/44F14.c:1146)
void Actors_UpdateActorAnimationBlend(Actors_ActorRecord* actor)
{
    Actors_AdvanceSkeletalAnimation(actor, &actor->pose, 0);

    if (actor->blendFrame != 0) {
        int frames_per_step;

        if (actor->blendFrame == 1) {
            Actors_AlignPoseRotationBranches(actor->blendPose.boneKeys, actor->pose.boneKeys, actor->instance.boneCount);
        }

        frames_per_step = actor->framesPerStep;

        if (frames_per_step != 1) {
            int next = actor->blendFrame + 0xFF;
            actor->blendFrame = frames_per_step + next;

            if (actor->blendFrame >= actor->blendFrames) {
                actor->blendFrame = 0;
                actor->blendFrames = 0;
                goto tail;
            }
        }

        Actors_BlendActorPose(
            &actor->blendPose, &actor->pose, actor->instance.boneCount, actor->blendFrame, actor->blendFrames);

        actor->blendPose.holdingLastFrame = actor->pose.holdingLastFrame;
        actor->blendPose.reachedEnd = actor->pose.reachedEnd;

        Actors_UpdateMotionAndLookAt(actor, 0);

        ++actor->blendFrame;

        if (actor->blendFrame >= actor->blendFrames) {
            actor->blendFrame = 0;
            actor->blendFrames = 0;
        }
        return;
    }

tail:
    Core_Memory_CopyAlignedWords(&actor->blendPose, &actor->pose, sizeof(actor->blendPose));
    Actors_UpdateMotionAndLookAt(actor, 0);
}

// decomp: BATTLE.PRG func_800AF844 (src/BATTLE/BATTLE.PRG/44F14.c:1192)
void Actors_AlignPoseRotationBranches(SVECTOR* dst, SVECTOR* src, int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        int x = src[i].vx - dst[i].vx;
        int z;

        if (x >= ONE) {
            x -= ONE * 2;
        } else if (x < -ONE) {
            x += ONE * 2;
        }

        x = ABS(x);
        z = src[i].vz - dst[i].vz;

        if (z >= ONE) {
            z -= ONE * 2;
        } else if (z < -ONE) {
            z += ONE * 2;
        }

        z = ABS(z);

        if (x + z >= ONE) {
            dst[i].vx = (dst[i].vx + ONE) % (ONE * 2);
            dst[i].vz = (dst[i].vz + ONE) % (ONE * 2);
            dst[i].vy = (ONE - dst[i].vy) % (ONE * 2);
        }
    }
}

// decomp: BATTLE.PRG func_800B002C (src/BATTLE/BATTLE.PRG/44F14.c:1486)
void Actors_UpdateMotionAndLookAt(Actors_ActorRecord* actor, int skip_motion_update)
{
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();
    int dx;
    int dz;
    int yaw;
    int pitch;
    int limit;
    int trackAnim;
    Actors_ActorRecord* other;

    if (*(int*)&actor->instance.scaleX == 0x10001000 && actor->instance.scaleZ == ONE) {
        scratch->rootOffset.vx = actor->blendPose.boneKeys[41].vx >> 1;
        scratch->rootOffset.vy = actor->blendPose.boneKeys[41].vy >> 1;
        scratch->rootOffset.vz = actor->blendPose.boneKeys[41].vz >> 1;
    } else {
        scratch->rootOffset.vx = actor->blendPose.boneKeys[41].vx * actor->instance.scaleY / 8192;
        scratch->rootOffset.vy = actor->blendPose.boneKeys[41].vy * actor->instance.scaleZ / 8192;
        scratch->rootOffset.vz = actor->blendPose.boneKeys[41].vz * actor->instance.scaleX / 8192;
    }
    scratch->rootOffset.vx = scratch->rootOffset.vx * actor->scale / ONE;
    scratch->rootOffset.vy = scratch->rootOffset.vy * actor->scale / ONE;
    scratch->rootOffset.vz = scratch->rootOffset.vz * actor->scale / ONE;

    if (skip_motion_update == 0) {
        if (actor->scaleFrames != 0) {
            dx = actor->instance.scaleDelta.vx / actor->scaleFrames;
            actor->instance.scaleDelta.vx -= dx;
            actor->instance.scaleX += dx;
            dx = actor->instance.scaleDelta.vy / actor->scaleFrames;
            actor->instance.scaleDelta.vy -= dx;
            actor->instance.scaleY += dx;
            dx = actor->instance.scaleDelta.vz / actor->scaleFrames;
            actor->instance.scaleDelta.vz -= dx;
            actor->instance.scaleZ += dx;
            --actor->scaleFrames;
        }
        if (actor->instance.turnFrames != 0) {
            dx = actor->instance.rotationStepX / actor->instance.turnFrames;
            actor->instance.rotationStepX -= dx;
            actor->instance.rotationX = (actor->instance.rotationX + dx) & 0xFFF;
            dx = actor->instance.facingDelta / actor->instance.turnFrames;
            actor->instance.facingDelta -= dx;
            actor->instance.facing = (actor->instance.facing + dx) & 0xFFF;
            dx = actor->instance.rotationStepZ / actor->instance.turnFrames;
            actor->instance.rotationStepZ -= dx;
            actor->instance.rotationZ = (actor->instance.rotationZ + dx) & 0xFFF;
            --actor->instance.turnFrames;
        }
        if (actor->previewPose) {
            actor->blendPose.boneKeys[0].vx = ONE / 2;
            actor->pose.boneKeys[0].vx = ONE / 2;
        }
        if (actor->instance.currentAnimation != 0) {
            dx = actor->instance.rootMotion.vx / actor->instance.currentAnimation;
            actor->instance.rootMotion.vx -= dx;
            actor->instance.position.vx += dx;
            dx = actor->instance.rootMotion.vy / actor->instance.currentAnimation;
            actor->instance.rootMotion.vy -= dx;
            actor->instance.position.vy += dx;
            dx = actor->instance.rootMotion.vz / actor->instance.currentAnimation;
            actor->instance.rootMotion.vz -= dx;
            actor->instance.position.vz += dx;
            if (--actor->instance.currentAnimation == 0) {
                if (actor->instance.locomotionState == 1) {
                    actor->instance.locomotionState = 0;
                    actor->instance.movementPending = 0;
                    actor->animationSet = 0xFF;
                }
                if (actor->instance.locomotionState != 2) {
                    actor->instance.currentTileX = actor->instance.position.vx / 128;
                    actor->instance.currentTileZ = actor->instance.position.vz / 128;
                    Field_GimmickSupportHeight(actor->instance.currentTileX, actor->instance.currentTileZ);
                    actor->supportGimmick = Actors_supportingGimmickIndex;
                }
            }
        }
    }
    Actors_TransformActorRootOffset(actor);
    if (!(*(int*)((char*)actor + 8) & 0x3F0000)) {
        Actors_SaveActorGroundedTransform(actor);
    }

    if (actor->headTrackTarget != -2) {
        trackAnim = 0;
        if (((short (*)(void))Event_EventPlaybackState)() != 4) {
            if (actor->instance.wasDrawn) {
                return;
            }
            dx = actor->animationId;
            if (dx >= 100) {
                dx -= 100;
            }
            if (actor->instance.isJumping) {
                if ((u_int)(dx - 31) < 8) {
                    trackAnim = 1;
                }
            } else if (actor->instance.motionLocked) {
                if ((u_int)(dx - 8) < 27) {
                    trackAnim = 1;
                }
            } else if (dx == 0) {
                goto track;
            } else if (dx == 1 || dx == 6 || dx == 13 || dx == 14 || dx == 17
                       || dx == 18) {
                goto track;
            }
            if (trackAnim == 0) {
            reset:
                yaw = 0;
                pitch = yaw;
                goto apply;
            }
        }
    track:
        if (actor->headTrackTarget == -4) {
            pitch = actor->headTracking.vx;
            yaw = actor->headTracking.vy;
        } else if (actor->headTrackTarget == -3) {
            pitch = actor->headTracking.vx - actor->instance.rotationX;
            yaw = actor->headTracking.vy - actor->instance.facing;
        } else {
            if (actor->headTrackTarget == -1) {
                dx = actor->headTracking.vx - actor->instance.position.vx;
                dz = actor->headTracking.vz - actor->instance.position.vz;
            } else {
                other = Actors_actorRecords[actor->headTrackTarget];
                dx = other->instance.position.vx - actor->instance.position.vx;
                dz = other->instance.position.vz - actor->instance.position.vz;
            }
            yaw = ONE * 3 / 4 - Core_Gte_Atan2(dz, dx);
            yaw &= 0xFFF;
            yaw -= actor->instance.facing;
            dx *= dx;
            if (dx < 0) {
                dx = -dx;
            }
            dz *= dz;
            if (dz < 0) {
                dz = -dz;
            }
            dx = Core_Math_SquareRootInteger(dx + dz);
            if (actor->headTrackTarget == -1) {
                dz = actor->headTracking.vy - actor->instance.position.vy;
            } else {
                dz = Actors_actorRecords[actor->headTrackTarget]->instance.position.vy
                   - actor->instance.position.vy;
            }
            pitch = Core_Gte_Atan2(dx, dz);
            pitch -= ONE / 4;
            pitch &= 0xFFF;
        }
        if (yaw >= ONE / 2) {
            yaw -= ONE;
        }
        if (yaw < -ONE / 2) {
            yaw += ONE;
        }
        if (yaw > 800) {
            yaw = 800;
        }
        if (yaw < -800) {
            yaw = -800;
        }
        if (pitch >= ONE / 2) {
            pitch -= ONE;
        }
        if (pitch < -ONE / 2) {
            pitch += ONE;
        }
        if (pitch > 800) {
            pitch = 800;
        }
        if (pitch < -800) {
            pitch = -800;
        }
    apply:
        limit = actor->headTracking.pad;
        yaw -= (short)actor->instance.lookYaw;
        if (yaw > limit) {
            yaw = limit;
        }
        if (yaw < -limit) {
            yaw = -limit;
        }
        pitch -= (short)actor->instance.lookPitch;
        if (pitch > limit) {
            pitch = limit;
        }
        if (pitch < -limit) {
            pitch = -limit;
        }
        scratch->angleX = actor->instance.lookYaw + yaw;
        scratch->angleY = actor->instance.lookPitch + pitch;
        actor->instance.lookYaw = scratch->angleX;
        actor->instance.lookPitch = scratch->angleY;
    } else if (*(int*)&actor->instance.lookYaw != 0) {
        goto reset;
    } else {
        scratch->angleX = 0;
        scratch->angleY = 0;
    }
}

// decomp: BATTLE.PRG func_800B26B8 (src/BATTLE/BATTLE.PRG/49268.c:246)
void Actors_ApplyLockedBoneTransform(Actors_ActorRecord* actor, MATRIX* parent, int bone, int saved)
{
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();

    scratch->matrix = *(actor->lockedBoneMatrices + saved - 1);
    Core_Math_PremultiplyRotationMatrix(parent, &scratch->matrix);
    Core_Gte_SetRotMatrix(parent);
    Core_Gte_SetTransMatrix(parent);
    --saved;
    setVector(&scratch->position, actor->lockedBoneMatrices[saved].t[0], actor->lockedBoneMatrices[saved].t[1],
        actor->lockedBoneMatrices[saved].t[2]);
    Core_Gte_LoadV0(&scratch->position);
    Core_Gte_Rtv0Tr();
    Core_Gte_StoreLvnl(scratch->matrix.t);
    Actors_skeletonPose.boneMatrices[bone] = scratch->matrix;
    actor->boneMatrices[bone] = scratch->matrix;
}

