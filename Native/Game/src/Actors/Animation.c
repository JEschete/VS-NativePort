// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
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
#include "Core/Core/IntPair.h"
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
void Actors_QueueActorBaseSequenceLoad(Battle_Actor* arg0 __attribute__((unused)), int arg1)
{
    Content_ActorAssetRequest sp10;

    sp10.assetKind = 7;
    sp10.targetSlot = arg1;
    sp10.modelId = 0;
    sp10.subKind = 0;
    Content_QueueActorAssetRequest(&sp10);
}

// decomp: BATTLE.PRG func_800771E0 (src/BATTLE/BATTLE.PRG/146C.c:6480)
void Actors_QueueActorWeaponSequenceLoad(char* arg0, int arg1, int arg2, int arg3)
{
    Content_ActorAssetRequest sp10;

    if (((arg2 != 0) || (arg1 >= 2)) && ((arg1 < 2) || ((arg3 & 3) == 1))) {
        sp10.assetKind = 7;
        sp10.targetSlot = arg1;
        sp10.modelId = ((Battle_Actor*)arg0)->equippedWeaponCategory;
        sp10.subKind = 0;
        Content_QueueActorAssetRequest(&sp10);
    }
}

// decomp: BATTLE.PRG func_8007C348 (src/BATTLE/BATTLE.PRG/146C.c:8738)
void Actors_InterruptActorAction(int arg0, int arg1) { Actors_BeginActorDeath(arg0, NULL, arg1); }

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
        if (Actors_FindActorAnimationHeader(actor, actor->animationId, &entry) != actor->animationHeader ||
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
void Actors_QueueSharedSequenceWithTextures(int index, int wepId, int arg2)
{
    Content_ActorAssetRequest objData;

    objData.assetKind = 7;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.subKind = 2;
    objData.textureSlot = arg2;
    Content_QueueActorAssetRequest(&objData);
}

// decomp: BATTLE.PRG vs_battle_loadEtm (src/BATTLE/BATTLE.PRG/146C.c:9022)
void Actors_QueueSequenceEffectTextures(int index, int wepId, int arg2)
{
    Content_ActorAssetRequest objData;

    objData.assetKind = 9;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.textureSlot = arg2;
    Content_QueueActorAssetRequest(&objData);
}

// decomp: BATTLE.PRG func_8007CC4C (src/BATTLE/BATTLE.PRG/146C.c:9033)
void Actors_ReleaseSequenceEffectTextures(int arg0) { Content_ReleaseSequenceEffectTexture(arg0); }

// decomp: BATTLE.PRG func_8007CC6C (src/BATTLE/BATTLE.PRG/146C.c:9035)
void Actors_SetSharedSequenceBank(int arg0) { Actors_SelectSharedSequenceBank(arg0); }

// decomp: BATTLE.PRG func_8007CC8C (src/BATTLE/BATTLE.PRG/146C.c:9037)
void Actors_SharedSequenceBank(void) { Actors_SharedSequenceBankId(); }

// decomp: BATTLE.PRG func_8007CEA0 (src/BATTLE/BATTLE.PRG/146C.c:9115)
void Actors_ReturnActorToIdle(int arg0) { Actors_RestoreActorIdlePose(arg0); }

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
int Actors_SelectSharedSequenceBank(u_int arg0)
{
    int i;

    if (arg0 - 1 >= 3) {
        return -1;
    }

    for (i = 0; i < 17; ++i) {
        Actors_ActorRecord* temp_s0 = Actors_actorRecords[i];
        if ((temp_s0 != NULL) && temp_s0->textureRowOffsets[Actors_sharedSequenceBankIndex] != 0xFF
            && (temp_s0->textureRowOffsets[arg0 - 1] == 0xFF)) {
            if (temp_s0->textureAnimationSlots[0].currentFrame >= 2) {
                Actors_UploadActorTextureFrame(temp_s0, 0, 0);
            }
            if (temp_s0->textureAnimationSlots[1].currentFrame >= 2) {
                Actors_UploadActorTextureFrame(temp_s0, 1, 0);
            }
        }
    }
    Actors_sharedSequenceBankIndex = arg0 - 1;
    return 0;
}

// decomp: BATTLE.PRG func_8009CFA0 (src/BATTLE/BATTLE.PRG/30DB0.c:1918)
int Actors_SharedSequenceBankId(void) { return Actors_sharedSequenceBankIndex + 1; }

// decomp: BATTLE.PRG func_8009D854 (src/BATTLE/BATTLE.PRG/30DB0.c:2188)
void Actors_RequestActorAnimationAdvance(int arg0)
{
    if (Actors_actorRecords[arg0] != NULL) {
        Actors_actorRecords[arg0]->advanceRequested = 1;
    }
}

// decomp: BATTLE.PRG func_8009D934 (src/BATTLE/BATTLE.PRG/30DB0.c:2219)
void Actors_SetActorWeaponDrawn(int arg0, int arg1, int arg2)
{
    int var_s2;
    int temp_s1;

    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0 == NULL) {
        return;
    }

    if (arg2 == 2) {
        int i;
        temp_s0->instance.weaponDrawn = arg1;

        for (i = 0; i < 2; ++i) {
            Actors_WeaponModel* temp_v1 = Actors_weaponModels[arg0 * 2 + i];
            if (temp_v1 != NULL) {
                if (arg1 != 0) {
                    temp_v1->reserved11 = 0x40;
                    temp_v1->reserved12 = 0x40;
                    temp_v1->reserved08Bit4 = 1;
                } else {
                    temp_v1->reserved11 = 0;
                    temp_v1->reserved12 = 0;
                    temp_v1->reserved08Bit4 = 0;
                }
            }
        }
        return;
    }

    var_s2 = 5;

    if (arg2 != 0) {
        var_s2 = 16;
        temp_s1 = Actors_drawnWeaponAnimationBases[temp_s0->sequenceModelId];
    } else {
        temp_s1 = Actors_sheathedWeaponAnimationBases[temp_s0->sequenceModelId];
    }

    if (((int (*)())Event_EventPlaybackState)() == 4) {
        temp_s0->instance.weaponDrawn = arg1;
        Actors_PlayActorAnimation(arg0, 1, 0, 16);
        return;
    }

    if (arg1 != temp_s0->instance.weaponDrawn) {
        if (arg1 == 0) {

            if (temp_s0->modelId == 0) {
                Audio_PlayActorSoundPair(temp_s0, temp_s1 + 1, 0);
            }

            Actors_PlayActorAnimation(arg0, (arg2 * 2) + 10, 0, var_s2);

            temp_s0->weaponPoseActive = 1;
            temp_s0->instance.weaponDrawn = 0;

            Actors_QueueActorAnimation(arg0, 1, 16, 0);
        } else {

            if (temp_s0->modelId == 0) {
                Audio_PlayActorSoundPair(temp_s0, (int)temp_s1, 0);
            }

            temp_s0->instance.weaponDrawn = arg1;

            Actors_PlayActorAnimation(arg0, (arg2 * 2) + 9, 0, var_s2);

            temp_s0->weaponPoseActive = 1;

            Actors_QueueActorAnimation(arg0, 1, 16, 0);
        }
    }
}

// decomp: BATTLE.PRG func_8009DBDC (src/BATTLE/BATTLE.PRG/30DB0.c:2296)
void Actors_PlayActorWeaponDrawSound(int arg0, int arg1)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != NULL) {
        Audio_PlayActorSoundPair(temp_a0, Actors_drawnWeaponAnimationBases[temp_a0->sequenceModelId] + arg1, 0);
    }
}

// decomp: BATTLE.PRG func_8009E070 (src/BATTLE/BATTLE.PRG/30DB0.c:2451)
void Actors_PlayActorActionPose(int arg0, SVECTOR* arg1, int arg2)
{
    int var_s1;
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    switch (arg2) {
    case 1:
    case 2:
    case 3:
    case 4:
        var_s1 = arg2 + 0x5F;
        break;
    case 5:
        var_s1 = 0x1E;
        break;
    case 6:
        var_s1 = 0x1C;
        if (temp_s0->actionPoseActive << 3) {
            var_s1 = 1;
        }

        if ((temp_s0->instance.movementMode == 3) || (temp_s0->instance.locomotionState == 2)) {
            return;
        }
        break;
    case 7:
        var_s1 = 0x1D;
        break;
    default:
        return;
    }

    temp_s0->instance.facingDelta = Actors_ActorFacingDeltaToPoint(temp_s0, arg1);
    temp_s0->instance.turnFrames = 0xA;
    temp_s0->runFrames = 0;
    temp_s0->weaponPoseActive = 1;
    Actors_PlayActorAnimation(arg0, var_s1, 0, 6);
    temp_s0->queuedAnimation = 0;
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
int Actors_IsActorActionAnimationBusy(int arg0)
{
    Actors_ActorRecord* temp_a1 = Actors_actorRecords[arg0];

    if (temp_a1 == NULL) {
        if (Field_gimmickInstances[arg0] == NULL) {
            return -1;
        }
        return 0;
    }

    if ((temp_a1->instance.actionAnimationState != 0) && (temp_a1->instance.actionAnimationState != 5)
        && (temp_a1->instance.actionAnimationState != 11)) {
        return 1;
    }

    if (!temp_a1->pose.holdingLastFrame) {
        int temp_a0 = temp_a1->animationId;
        if (temp_a0 >= 100) {
            temp_a0 -= 100;

            if ((temp_a0 - 9) < 4U) {
                return 1;
            }

            if (temp_a0 == 0x1C) {
                return 1;
            }

            if ((temp_a0 - 0x62) < 4U) {
                return 0;
            }
            if (temp_a0 >= 0x40) {
                return 1;
            }
            temp_a1 = Actors_actorRecords[arg0];
            return 0;
        }

        if (temp_a0 == 0x1C) {
            return 1;
        }
        if (temp_a0 == 0x3B) {
            return 1;
        }
        if ((temp_a0 - 0x40) < 7U) {
            return 1;
        }
        if ((temp_a0 - 0x55) < 7U) {
            return 1;
        }
        if (temp_a0 == 0x5E) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009E5C4 (src/BATTLE/BATTLE.PRG/30DB0.c:2685)
void Actors_RestoreActorIdlePose(int arg0)
{
    if (Actors_actorRecords[arg0] == NULL
        || ((Actors_actorRecords[arg0]->animationId != 0x63)
            && (Actors_actorRecords[arg0]->animationId != 0xC7))) {
        Actors_RestoreActorBasePose(arg0, 1);
    }
}

// decomp: BATTLE.PRG func_8009E614 (src/BATTLE/BATTLE.PRG/30DB0.c:2693)
void Actors_RestoreActorNeutralPose(int arg0) { Actors_RestoreActorBasePose(arg0, 6); }

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
int Actors_AnimationBattleWindowFrame(int arg0)
{
    u_char* temp_s2;
    int temp_v0;
    int temp_s1;
    Actors_AnimationPose* s0 = &Actors_actorRecords[arg0]->pose;

    if (s0->eventScript == NULL) {
        return 0;
    }
    temp_s1 = s0->nextEventFrame;
    temp_s2 = s0->eventScript;
    temp_v0 = Actors_RunAnimationEvents(Actors_actorRecords[arg0], s0, 3);
    s0->nextEventFrame = temp_s1;
    s0->eventScript = temp_s2;
    return temp_v0;
}

// decomp: BATTLE.PRG func_8009F990 (src/BATTLE/BATTLE.PRG/30DB0.c:3401)
void Actors_SetActorSoundOverrides(int arg0, Actors_ModelEffectSlot* arg1)
{
    int i;

    if (arg0 == -1) {
        for (i = 0; i < 17; ++i) {
            Actors_ApplyActorSoundOverrides(i, arg1);
        }
    } else {
        Actors_ApplyActorSoundOverrides(arg0, arg1);
    }
}

// decomp: BATTLE.PRG func_8009F9F4 (src/BATTLE/BATTLE.PRG/30DB0.c:3414)
void Actors_ApplyActorSoundOverrides(int arg0, Actors_ModelEffectSlot* arg1)
{
    int temp_a2;
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0 == NULL) {
        return;
    }

    temp_a2 = arg1->kind;

    if (temp_a2 == 0) {
        temp_s0->soundEffect.level = 0;
        temp_s0->soundEffect.flags = 0;
    } else if (temp_a2 == 1) {
        if (arg1->parameter == 0) {
            temp_s0->soundEffect = *arg1;
        } else {
            int sp10;
            int sp14;
            int temp_v1;

            temp_s0->soundEffectTarget = *arg1;
            temp_s0->soundEffect.parameter = arg1->parameter;
            temp_v1 = temp_s0->soundEffect.kind;

            if (temp_v1 == 0) {
                ((void (*)(int, int*, int*, int))Audio_ActorSoundPanAndVolume)(arg0, &sp10, &sp14, arg0);
                temp_s0->soundEffect.level = sp10;
                temp_s0->soundEffect.flags = sp14;
            } else if (temp_v1 == temp_a2) {
                ((void (*)(int, int*, int*, int))Audio_ActorSoundPanAndVolume)(arg0, &sp10, &sp14, arg0);
                if (temp_s0->soundEffect.level == 0) {
                    temp_s0->soundEffect.level = sp10;
                }
                if (temp_s0->soundEffect.flags == 0) {
                    temp_s0->soundEffect.flags = sp14;
                }
            } else if (temp_v1 == 2) {
                temp_s0->soundEffect.level = 0x80;
                temp_s0->soundEffect.flags = temp_a2;
            }
            if (arg1->level == 0) {
                temp_s0->soundEffect.level = 0;
            }
            if (arg1->flags == 0) {
                temp_s0->soundEffect.flags = 0;
            }
        }
    }
    temp_s0->soundEffect.kind = arg1->kind;
}

// decomp: BATTLE.PRG func_8009FBD8 (src/BATTLE/BATTLE.PRG/30DB0.c:3480)
int Actors_SetActorTravelAnimation(int arg0, int arg1)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 == NULL) {
        return -1;
    }

    if (arg1 == -1) {
        temp_a0->travelAnimation = 0xD;
    } else {
        temp_a0->travelAnimation = arg1;
    }

    return 0;
}

// decomp: BATTLE.PRG func_8009FC20 (src/BATTLE/BATTLE.PRG/30DB0.c:3498)
int Actors_ActorTravelAnimation(int arg0, int* arg1)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 == NULL) {
        return -1;
    }

    *arg1 = temp_a0->travelAnimation;

    return 0;
}

// decomp: BATTLE.PRG func_800A01C8 (src/BATTLE/BATTLE.PRG/30DB0.c:3710)
int Actors_QueueActorAnimation(int arg0, int arg1, int arg2, int arg3)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 == NULL) {
        return -1;
    }

    temp_a0->queuedAnimation = arg1;
    temp_a0->queuedParameterA = arg2;
    temp_a0->queuedParameterB = arg3;

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
int Actors_PlayActorAnimationAtFrame(int arg0, int arg1, int arg2, int arg3)
{
    Actors_ActorRecord* temp_s0;

    int v0 = Actors_PlayActorAnimation(arg0, arg1, 0, arg2);

    if (v0 < 0) {
        return v0;
    }

    if (arg3 == 0) {
        return 0;
    }

    arg3 -= State_tickSpeed / 2;
    temp_s0 = Actors_actorRecords[arg0];

    if (arg3 > 0) {
        temp_s0->framesPerStep = arg3;
        Actors_AdvanceSkeletalAnimation(temp_s0, &temp_s0->pose, 1);
    }

    temp_s0->framesPerStep = 1;

    return 0;
}

// decomp: BATTLE.PRG func_800A0870 (src/BATTLE/BATTLE.PRG/30DB0.c:3948)
void Actors_SetActorTransitionMode(int arg0) { Actors_reservedControlByte = arg0; }

// decomp: BATTLE.PRG func_800A09D8 (src/BATTLE/BATTLE.PRG/30DB0.c:3987)
void Actors_SetActorAnimationLocked(int arg0, int arg1)
{
    if (Actors_actorRecords[arg0] != NULL) {
        Actors_actorRecords[arg0]->animationLocked = arg1;
    }
}

// decomp: BATTLE.PRG func_800A249C (src/BATTLE/BATTLE.PRG/38C1C.c:578)
void Actors_PlayActorTurnAnimation(int arg0, int arg1)
{
    int var_a2;
    int var_a1;
    int v1;
    Actors_ActorRecord* temp_a3 = Actors_actorRecords[arg0];

    if (temp_a3->instance.motionLocked) {
        return;
    }

    var_a1 = arg1 - temp_a3->instance.facing;

    if (var_a1 > 0x800) {
        var_a1 -= 0x1000;
    }

    if (var_a1 < -0x800) {
        var_a1 += 0x1000;
    }

    if (var_a1 == 0) {
        return;
    }

    temp_a3->turnState.mode = 6;

    v1 = temp_a3->animationId;

    if ((v1 == 0x76) || (temp_a3->animationId == 0x75) || temp_a3->instance.supportLayer) {
        return;
    }

    var_a2 = 0x1F;

    if (!(temp_a3->instance.isJumping)) {

        var_a2 = 0x11;

        if (var_a1 < 0) {
            var_a2 = 0x12;
        }
    }

    ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, var_a2, 0, 0xA);
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
void Actors_SetActorBonePoseLock(Actors_ActorRecord* arg0, int arg1, int arg2)
{
    int sp10;
    int i;
    Actors_ShpBone* temp_s2 = &arg0->instance.model->bones[arg1];

    for (i = 0; i < 2; ++i) {
        if (arg2 == 0) {
            if (arg0->lockedBones[i] == arg1) {
                arg0->lockedBones[i] = 0;
                temp_s2->poseLockSlot = 0;
                return;
            }
        } else if (arg0->lockedBones[i] == 0) {
            ((void (*)(int, int, void*, int*))Actors_ActorAnchorMatrix)(arg0->instance.slot, arg1, &arg0->lockedBoneMatrices[i], &sp10);
            temp_s2->poseLockSlot = i + 1;
            arg0->lockedBones[i] = arg1;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_800AC500 (src/BATTLE/BATTLE.PRG/40564.c:2221)
void Actors_ClearActorBonePoseLocks(Actors_ActorRecord* arg0)
{
    int i;

    arg0->lockedBones[0] = 0;
    arg0->lockedBones[1] = 0;

    for (i = 0; i < arg0->instance.boneCount; ++i) {
        arg0->instance.model->bones[i].poseLockSlot = 0;
    }
}

// decomp: BATTLE.PRG func_800ACFA0 (src/BATTLE/BATTLE.PRG/40564.c:2561)
void Actors_DecodeInitialPoseVector(short* arg0, u_char* arg1, int arg2)
{
    int a3;
    a3 = arg1[0];
    a3 <<= 8;
    a3 |= arg1[1];
    a3 *= 2;
    arg0[0] = a3;

    a3 = arg1[2];
    a3 <<= 8;
    a3 |= arg1[3];
    a3 *= 2;
    arg0[1] = a3;

    a3 = arg1[4];
    a3 <<= 8;
    a3 |= arg1[5];
    a3 *= 2;
    arg0[2] = a3;

    arg0[3] = 6;

    if (arg2 != 0xFF) {
        arg0[3] = 0;
    }

    arg0[339] = 0;
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
intptr_t Actors_FindActorAnimationHeader(void* arg0, int arg1, u_short** arg2)
{
    Actors_ActorRecord* actor = arg0;
    Actors_SeqAnimationBank* p;
    int idx;

    *arg2 = NULL;

    if (arg1 == 0) {
        p = actor->ownedSequence;
        idx = 0;
    } else if (arg1 == 0xFF) {
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
    } else if (arg1 == 0xFE) {
        p = actor->sharedSequences[3];
        if (actor->sequenceModelId == 0) {
            return 0;
        }
        idx = actor->sequenceModelId - 1;
        idx = idx * 3 + 31;
        if (idx >= actor->battleAnimationCount) {
            return 0;
        }
    } else if (arg1 >= 0xDC) {
        p = actor->sharedSequences[Actors_sharedSequenceBankIndex];
        idx = arg1 - 0xDC;
    } else if (arg1 >= 0xCA) {
        p = Actors_sequenceWorkBuffer;
        idx = arg1 - 0xCA;
    } else if (arg1 >= 100) {
        p = actor->battleSequence;
        idx = arg1 - 100;
        if (idx >= actor->commonAnimationCount) {
            return 0;
        }
    } else {
        p = actor->commonSequence;
        idx = arg1;
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
        *arg2 = NULL;
    } else {
        *arg2 = &p->headers[(((actor->instance.boneCount * 4) + 10) * idx) >> 1];
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
int Actors_RestartActorAnimationEvents(Actors_ActorRecord* arg0)
{
    u_short* sp10;
    int var_v1;
    void* var_a3;

    Actors_SeqAnimationBank* v0 = (Actors_SeqAnimationBank*)Actors_FindActorAnimationHeader(arg0, arg0->animationId, &sp10);
    u_char* temp_a3;
    if (v0 == NULL || sp10 == NULL || (sp10[2] == 0 && arg0->poseOverrideActive)) {
        Core_Errorf("actors.sequence", "cannot unwind actor=%u animation=%u: unavailable events",
            (unsigned)arg0->instance.slot, (unsigned)arg0->animationId);
        return -1;
    }
    if (sp10[2] == 0) {
        return 0;
    }
    temp_a3 = VS_ONDISC(v0, animationData, u_char*);
    temp_a3 += sp10[2];

    var_v1 = temp_a3[0];

    if (var_v1 == 0xFF) {
        var_a3 = temp_a3 + 2;
        var_v1 = temp_a3[1] + 0xFF;
    } else {
        var_a3 = temp_a3 + 1;
    }

    arg0->blendPose.nextEventFrame = var_v1;
    arg0->blendPose.eventScript = var_a3;

    /* The decoder reads state->unk544 itself; this caller passes an ignored fourth
     * argument. */
    ((void (*)())Actors_RunAnimationEvents)(arg0, &arg0->blendPose, 2, var_a3);
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
void Actors_UpdateActorAnimationBlend(Actors_ActorRecord* arg0)
{
    Actors_AdvanceSkeletalAnimation(arg0, &arg0->pose, 0);

    if (arg0->blendFrame != 0) {
        int v5b2;

        if (arg0->blendFrame == 1) {
            Actors_AlignPoseRotationBranches(arg0->blendPose.boneKeys, arg0->pose.boneKeys, arg0->instance.boneCount);
        }

        v5b2 = arg0->framesPerStep;

        if (v5b2 != 1) {
            int next = arg0->blendFrame + 0xFF;
            arg0->blendFrame = v5b2 + next;

            if (arg0->blendFrame >= arg0->blendFrames) {
                arg0->blendFrame = 0;
                arg0->blendFrames = 0;
                goto tail;
            }
        }

        Actors_BlendActorPose(
            &arg0->blendPose, &arg0->pose, arg0->instance.boneCount, arg0->blendFrame, arg0->blendFrames);

        arg0->blendPose.holdingLastFrame = arg0->pose.holdingLastFrame;
        arg0->blendPose.reachedEnd = arg0->pose.reachedEnd;

        Actors_UpdateMotionAndLookAt(arg0, 0);

        ++arg0->blendFrame;

        if (arg0->blendFrame >= arg0->blendFrames) {
            arg0->blendFrame = 0;
            arg0->blendFrames = 0;
        }
        return;
    }

tail:
    Core_Memory_CopyAlignedWords(&arg0->blendPose, &arg0->pose, sizeof(arg0->blendPose));
    Actors_UpdateMotionAndLookAt(arg0, 0);
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
void Actors_UpdateMotionAndLookAt(Actors_ActorRecord* actor, int arg1)
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

    if (arg1 == 0) {
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
        *(int*)&actor->instance.previousTileX = *(int*)&actor->instance.currentTileX;
        *(Core_IntPair*)&actor->instance.supportOffset = *(Core_IntPair*)&actor->instance.position;
        if (actor->instance.supportLayer == 0) {
            *(int*)&actor->savedPlacement = *(int*)&actor->instance.currentTileX;
        }
        actor->instance.previousFacing = actor->instance.facing;
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

