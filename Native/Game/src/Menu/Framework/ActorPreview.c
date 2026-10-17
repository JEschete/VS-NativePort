// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Battle/StatusEffects.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Menu/Framework/ActorPreview.h"
#include "Render/Model.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: MAINMENU.PRG func_800F9A24 (src/MENU/MAINMENU.PRG/224.c:10)
void Menu_Framework_LoadActorPreview(int arg0)
{
    Menu_Framework_CloneActorPreview(arg0);
    Menu_Framework_ResetActorPreviewPose();

    Actors_actorRecords[1]->instance.weaponDrawn = 1;

    Actors_PlayActorAnimation(1, 1, 0, 0);
}

// decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:20)
void Menu_Framework_CloneActorPreview(int arg0)
{
    Content_ActorAssetRequest sp10;
    Actors_ActorRecord* temp_s0;
    Actors_ActorRecord* temp_s2;
    int i;
    Actors_ActorRecord* temp_s3;
    void* temp_v0;

    temp_s2 = Actors_actorRecords[arg0];
    temp_s3 = Core_Memory_AllocateFromLowEnd(0x1900);
    sp10.assetKind = 2;
    sp10.targetSlot = 1;
    sp10.modelId = temp_s2->modelId;
    sp10.dataAddress = arg0;
    sp10.destination = temp_s3;
    sp10.subKind = 0xFF;
    sp10.variant = temp_s2->instance.variant;
    sp10.material = 0;

    Content_QueueActorAssetRequest(&sp10);

    if (temp_s2->companionSlot >= 2) {
        temp_s0 = Actors_actorRecords[temp_s2->companionSlot];
        temp_v0 = Core_Memory_AllocateFromLowEnd(0x1900);
        sp10.assetKind = 2;
        sp10.targetSlot = 16;
        sp10.modelId = temp_s0->modelId;
        sp10.dataAddress = temp_s2->companionSlot;
        sp10.destination = temp_v0;
        sp10.subKind = 1;
        sp10.textureSlot = 0xFC;
        sp10.variant = temp_s2->instance.variant;
        sp10.material = 0;

        Content_QueueActorAssetRequest(&sp10);
    }

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* temp_s0 =
            Actors_weaponModels[(temp_s2->instance.slot * 2) + i];
        if (temp_s0 != 0) {
            temp_v0 = Core_Memory_AllocateFromLowEnd(sizeof *temp_s0);
            sp10.assetKind = 4;
            sp10.targetSlot = i + 2;
            sp10.modelId = temp_s0->modelId;
            sp10.dataAddress = arg0 * 2 + i;
            sp10.destination = temp_v0;
            sp10.subKind = 1;
            sp10.textureSlot = temp_s0->attachmentSlot;
            sp10.material = temp_s0->material;
            Content_QueueActorAssetRequest(&sp10);
        }
    }

    while (Content_UpdateActorAssetQueue() != 0)
        ;

    temp_s3->commonSequence = temp_s2->commonSequence;

    if (temp_s2->instance.slot == 0) {
        temp_s3->battleSequence = Core_Memory_AllocateFromLowEnd(0x7800);

        Content_CopyRelocatedWeaponSequence(temp_s3->battleSequence, temp_s2->battleSequence);
    } else {
        temp_s3->battleSequence = temp_s2->battleSequence;
    }

    temp_s3->sequenceModelId = temp_s2->sequenceModelId;
    temp_s3->sequenceFlags = temp_s2->sequenceFlags;
    temp_s3->commonAnimationCount = temp_s2->commonAnimationCount;
}

// decomp: MAINMENU.PRG func_800F9CB0 (src/MENU/MAINMENU.PRG/224.c:100)
void Menu_Framework_ResetActorPreviewPose(void)
{
    Render_ColorCode sp10;
    int new_var;
    Menu_Framework_PreviewPoseBytes sp18;
    Actors_ActorRecord* temp_s0;
    int i;

    temp_s0 = Actors_actorRecords[1];

    for (i = 0; i < 2; ++i) {
        if (Battle_unusedGlobal1[i] != NULL) {
            Battle_unusedGlobal1[i]->opaque8 |= 0x10;
        }
    }

    ((void (*)())Actors_ResetActorMotionState)(1, i);

    temp_s0->instance.position.vx = 0;
    temp_s0->instance.position.vy = 0;
    temp_s0->instance.position.vz = 0;
    temp_s0->previewPose = 0;
    temp_s0->instance.isSkipped = 0;
    temp_s0->instance.soundEnabled = 0;
    sp10.red = -1;
    sp10.green = -1;
    sp10.blue = -1;

    sp18.opaque0Bit0 = 2;
    sp18.opaque1Bit0 = 254;
    sp18.opaque2Bit0 = 2;
    sp18.opaque3Bit0 = 16;

    if (Actors_actorRecords[16] == NULL) {
        Actors_SetActorCompanion(1, 1, (Render_ColorCode*)&sp18, &sp10);
    } else {
        Actors_actorRecords[16]->instance.isSkipped = 0;
    }

    sp10.red = 0;
    sp10.green = 0;
    sp10.blue = 0;

    Actors_SetActorFadeParameters(1, &sp10);
    Actors_SetActorTint(1, 0);

    temp_s0->previewFlag = 0;
}

// decomp: MAINMENU.PRG func_800F9E0C (src/MENU/MAINMENU.PRG/224.c:149)
void Menu_Framework_ReleaseActorPreview(void)
{
    void* sp10[2];
    void* temp_s1;

    temp_s1 = Actors_actorRecords[1];
    sp10[0] = Actors_weaponModels[2];
    sp10[1] = Actors_weaponModels[3];

    Content_ReleaseActorModel(1);
    Core_Memory_FreeFromLowEnd(temp_s1);

    temp_s1 = Actors_actorRecords[16];

    if (temp_s1 != NULL) {
        Content_ReleaseActorModel(0x10);
        Core_Memory_FreeFromLowEnd(temp_s1);
    }

    if (sp10[0] != NULL) {
        Core_Memory_FreeFromLowEnd(sp10[0]);
    }

    if (sp10[1] != NULL) {
        Core_Memory_FreeFromLowEnd(sp10[1]);
    }
}

// decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:177)
int Menu_Framework_DrawActorPreview(MATRIX* arg0)
{
    int _[2];
    int i;
    Actors_ActorRecord* temp_s1;
    int new_var2;
    int new_var3;
    temp_s1 = Actors_actorRecords[1];

    if (temp_s1 == NULL) {
        return -1;
    }

    if (temp_s1->instance.boneCount == 0) {
        return -1;
    }

    if (temp_s1->instance.isSkipped) {
        return -1;
    }

    Render_PrepareActorCameraMatrices(arg0);

    temp_s1->framesPerStep =
        ((int)(State_tickSpeed + (((u_int)State_tickSpeed) >> 0x1F))) >> 1;

    if (temp_s1->instance.isFrozen) {
        Actors_UpdateActorTextureScrolling(temp_s1);
        Actors_UpdateActorAnimationBlend(temp_s1);
    } else {
        Actors_PrepareActorRootTransform(temp_s1);
    }

    Actors_DrawActorModel(temp_s1, arg0, 0);

    if (temp_s1->companionSlot > 1) {
        Actors_UpdateActorTextureScrolling(Actors_actorRecords[16]);
    }

    if (temp_s1->companionSlot != 0) {
        int* p = (int*)((uintptr_t)Core_ScratchBase() + 0x000);
        new_var2 = (temp_s1->companionState.depthBias * 4) + 0xC0;
        p[1] += new_var2;

        applyVector(&temp_s1->instance.position, temp_s1->companionState.offsetX, temp_s1->companionState.offsetY,
            temp_s1->companionState.offsetZ, +=);

        Actors_UpdateMotionAndLookAt(temp_s1, temp_s1->companionSlot);
        Actors_DrawActorModel(temp_s1, arg0, temp_s1->companionSlot);

        p = (int*)((uintptr_t)Core_ScratchBase() + 0x000);
        new_var3 = (temp_s1->companionState.depthBias * 4) + 0xC0;
        p[1] -= new_var3;

        applyVector(&temp_s1->instance.position, temp_s1->companionState.offsetX, temp_s1->companionState.offsetY,
            temp_s1->companionState.offsetZ, -=);
    }

    if (temp_s1->instance.isPreviewModel) {
        return -2;
    }

    for (i = 0; i < 2; ++i) {
        int v = i + 2;
        Actors_WeaponModel* temp_a0 = Actors_weaponModels[v];

        if ((temp_a0 != 0) && temp_a0->reserved08Bit4) {
            MATRIX* bone = &temp_s1->boneMatrices[temp_a0->attachmentBone];

            bone->t[0] -= temp_s1->companionState.offsetX;
            bone->t[1] -= temp_s1->companionState.offsetY;
            bone->t[2] -= temp_s1->companionState.offsetZ;

            Actors_DrawActorWeaponModel(temp_a0, bone);

            bone->t[0] += temp_s1->companionState.offsetX;
            bone->t[1] += temp_s1->companionState.offsetY;
            bone->t[2] += temp_s1->companionState.offsetZ;
        }
    }

    return 0;
}

// decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:306)
void Menu_Framework_ResetAshleyEquipmentPose(void)
{
    u_short* sp10;
    int i;
    int i_2;
    Actors_ActorRecord* temp_s1;

    temp_s1 = Actors_actorRecords[0];

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* var_a0 = Actors_weaponModels[i];

        if (var_a0 != 0) {
            if (temp_s1->instance.weaponDrawn) {
                var_a0->reserved11 = 0x40;
                var_a0->reserved08Bit4 = 1;
            } else {
                var_a0->reserved11 = 0;
            }
        }
    }

    Field_RefreshAshleyInteractionTile(temp_s1);

    i_2 = 1;

    if (temp_s1->instance.weaponDrawn) {
        i_2 = 101;
    }

    temp_s1->animationId = i_2;
    temp_s1->weaponPoseActive = 0;
    temp_s1->animationHeader = Actors_FindActorAnimationHeader(temp_s1, i_2, &sp10);
    temp_s1->animationSet = Actors_lastResolvedSequenceIndex;
    temp_s1->animationLength = *sp10;

    Actors_InitializeActorAnimationPose(temp_s1, &temp_s1->pose);
    Actors_AdvanceSkeletalAnimation(temp_s1, &temp_s1->pose, 1);
    Core_Memory_CopyAlignedWords(&temp_s1->blendPose, &temp_s1->pose, sizeof temp_s1->blendPose);

    temp_s1->pose.frame = 0;
    temp_s1->queuedAnimation = 0;
    temp_s1->blendFrames = 0;
    temp_s1->blendFrame = 0;
}

// decomp: MAINMENU.PRG func_800FA3FC (src/MENU/MAINMENU.PRG/224.c:352)
void Menu_Framework_QueueAshleyWeaponAnimations(int arg0)
{
    Content_ActorAssetRequest sp10;

    Content_ReleaseActorSequence(0, 0);

    sp10.assetKind = 7;
    sp10.targetSlot = 0;
    sp10.modelId = arg0;
    sp10.subKind = 0;

    Content_QueueActorAssetRequest(&sp10);
}

