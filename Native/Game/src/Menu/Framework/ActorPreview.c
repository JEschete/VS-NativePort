// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDrawing.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/SequencePlayback.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Menu/Framework/ActorPreview.h"
#include "Render/Model.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: MAINMENU.PRG func_800F9A24 (src/MENU/MAINMENU.PRG/224.c:10)
void Menu_Framework_LoadActorPreview(int source_actor_slot)
{
    Menu_Framework_CloneActorPreview(source_actor_slot);
    Menu_Framework_ResetActorPreviewPose();

    Actors_actorRecords[1]->instance.weaponDrawn = 1;

    Actors_PlayActorAnimation(1, 1, 0, 0);
}

// decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:20)
void Menu_Framework_CloneActorPreview(int source_actor_slot)
{
    Content_ActorAssetRequest asset_request = {0};
    Actors_ActorRecord* temp_s0;
    Actors_ActorRecord* source_actor;
    int i;
    Actors_ActorRecord* preview_actor;
    void* cloned_asset_allocation;

    source_actor = Actors_actorRecords[source_actor_slot];
    preview_actor = Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorRecord));
    asset_request.assetKind = 2;
    asset_request.targetSlot = 1;
    asset_request.modelId = source_actor->modelId;
    asset_request.dataAddress = source_actor_slot;
    asset_request.destination = preview_actor;
    asset_request.subKind = 0xFF;
    asset_request.variant = source_actor->instance.variant;
    asset_request.material = 0;

    Content_QueueActorAssetRequest(&asset_request);

    if (source_actor->companionSlot >= 2) {
        temp_s0 = Actors_actorRecords[source_actor->companionSlot];
        cloned_asset_allocation = Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorRecord));
        asset_request.assetKind = 2;
        asset_request.targetSlot = 16;
        asset_request.modelId = temp_s0->modelId;
        asset_request.dataAddress = source_actor->companionSlot;
        asset_request.destination = cloned_asset_allocation;
        asset_request.subKind = 1;
        asset_request.textureSlot = 0xFC;
        asset_request.variant = source_actor->instance.variant;
        asset_request.material = 0;

        Content_QueueActorAssetRequest(&asset_request);
    }

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* temp_s0 =
            Actors_weaponModels[(source_actor->instance.slot * 2) + i];
        if (temp_s0 != 0) {
            cloned_asset_allocation = Core_Memory_AllocateFromLowEnd(sizeof *temp_s0);
            asset_request.assetKind = 4;
            asset_request.targetSlot = i + 2;
            asset_request.modelId = temp_s0->modelId;
            asset_request.dataAddress = source_actor_slot * 2 + i;
            asset_request.destination = cloned_asset_allocation;
            asset_request.subKind = 1;
            asset_request.textureSlot = temp_s0->attachmentSlot;
            asset_request.material = temp_s0->material;
            Content_QueueActorAssetRequest(&asset_request);
        }
    }

    while (Content_UpdateActorAssetQueue() != 0)
        ;

    preview_actor->commonSequence = source_actor->commonSequence;

    if (source_actor->instance.slot == 0) {
        preview_actor->battleSequence = Core_Memory_AllocateFromLowEnd(0x7800);

        Content_CopyWeaponSequence(preview_actor->battleSequence, source_actor->battleSequence);
    } else {
        preview_actor->battleSequence = source_actor->battleSequence;
    }

    preview_actor->sequenceModelId = source_actor->sequenceModelId;
    preview_actor->sequenceFlags = source_actor->sequenceFlags;
    preview_actor->commonAnimationCount = source_actor->commonAnimationCount;
}

// decomp: MAINMENU.PRG func_800F9CB0 (src/MENU/MAINMENU.PRG/224.c:100)
void Menu_Framework_ResetActorPreviewPose(void)
{
    Render_ColorCode preview_color;
    int new_var;
    Menu_Framework_PreviewPoseBytes companion_state_bytes;
    Actors_ActorRecord* preview_actor;
    int i;

    preview_actor = Actors_actorRecords[1];

    for (i = 0; i < 2; ++i) {
        if (Actors_weaponModels[i + 2] != NULL) {
            Actors_weaponModels[i + 2]->reserved08Bit4 = 1;
        }
    }

    ((void (*)())Actors_ResetActorMotionState)(1, i);

    preview_actor->instance.position.vx = 0;
    preview_actor->instance.position.vy = 0;
    preview_actor->instance.position.vz = 0;
    preview_actor->previewPose = 0;
    preview_actor->instance.isSkipped = 0;
    preview_actor->instance.soundEnabled = 0;
    preview_color.red = -1;
    preview_color.green = -1;
    preview_color.blue = -1;

    companion_state_bytes.opaque0Bit0 = 2;
    companion_state_bytes.opaque1Bit0 = 254;
    companion_state_bytes.opaque2Bit0 = 2;
    companion_state_bytes.opaque3Bit0 = 16;

    if (Actors_actorRecords[16] == NULL) {
        Actors_SetActorCompanion(1, 1, (Render_ColorCode*)&companion_state_bytes, &preview_color);
    } else {
        Actors_actorRecords[16]->instance.isSkipped = 0;
    }

    preview_color.red = 0;
    preview_color.green = 0;
    preview_color.blue = 0;

    Actors_SetActorFadeParameters(1, &preview_color);
    Actors_SetActorTint(1, 0);

    preview_actor->previewFlag = 0;
}

// decomp: MAINMENU.PRG func_800F9E0C (src/MENU/MAINMENU.PRG/224.c:149)
void Menu_Framework_ReleaseActorPreview(void)
{
    void* preview_weapon_allocations[2];
    void* preview_actor_allocation;

    preview_actor_allocation = Actors_actorRecords[1];
    preview_weapon_allocations[0] = Actors_weaponModels[2];
    preview_weapon_allocations[1] = Actors_weaponModels[3];

    Content_ReleaseActorModel(1);
    Core_Memory_FreeFromLowEnd(preview_actor_allocation);

    preview_actor_allocation = Actors_actorRecords[16];

    if (preview_actor_allocation != NULL) {
        Content_ReleaseActorModel(0x10);
        Core_Memory_FreeFromLowEnd(preview_actor_allocation);
    }

    if (preview_weapon_allocations[0] != NULL) {
        Core_Memory_FreeFromLowEnd(preview_weapon_allocations[0]);
    }

    if (preview_weapon_allocations[1] != NULL) {
        Core_Memory_FreeFromLowEnd(preview_weapon_allocations[1]);
    }
}

// decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:177)
int Menu_Framework_DrawActorPreview(MATRIX* preview_camera_matrix)
{
    int _[2];
    int i;
    Actors_ActorRecord* preview_actor;
    int companion_ordering_table_advance_bytes;
    int companion_ordering_table_restore_bytes;
    preview_actor = Actors_actorRecords[1];

    if (preview_actor == NULL) {
        return -1;
    }

    if (preview_actor->instance.boneCount == 0) {
        return -1;
    }

    if (preview_actor->instance.isSkipped) {
        return -1;
    }

    Render_PrepareActorCameraMatrices(preview_camera_matrix);

    preview_actor->framesPerStep =
        ((int)(State_tickSpeed + (((u_int)State_tickSpeed) >> 0x1F))) >> 1;

    if (preview_actor->instance.isFrozen) {
        Actors_UpdateActorTextureScrolling(preview_actor);
        Actors_UpdateActorAnimationBlend(preview_actor);
    } else {
        Actors_PrepareActorRootTransform(preview_actor);
    }

    Actors_DrawActorModel(preview_actor, preview_camera_matrix, 0);

    if (preview_actor->companionSlot > 1) {
        Actors_UpdateActorTextureScrolling(Actors_actorRecords[16]);
    }

    if (preview_actor->companionSlot != 0) {
        companion_ordering_table_advance_bytes = (preview_actor->companionState.depthBias * 4) + 0xC0;
        Render_frameScratch.orderingTable += companion_ordering_table_advance_bytes / sizeof(u_long);

        applyVector(&preview_actor->instance.position, preview_actor->companionState.offsetX, preview_actor->companionState.offsetY,
            preview_actor->companionState.offsetZ, +=);

        Actors_UpdateMotionAndLookAt(preview_actor, preview_actor->companionSlot);
        Actors_DrawActorModel(preview_actor, preview_camera_matrix, preview_actor->companionSlot);

        companion_ordering_table_restore_bytes = (preview_actor->companionState.depthBias * 4) + 0xC0;
        Render_frameScratch.orderingTable -= companion_ordering_table_restore_bytes / sizeof(u_long);

        applyVector(&preview_actor->instance.position, preview_actor->companionState.offsetX, preview_actor->companionState.offsetY,
            preview_actor->companionState.offsetZ, -=);
    }

    if (preview_actor->instance.isPreviewModel) {
        return -2;
    }

    for (i = 0; i < 2; ++i) {
        int v = i + 2;
        Actors_WeaponModel* preview_weapon_model = Actors_weaponModels[v];

        if ((preview_weapon_model != 0) && preview_weapon_model->reserved08Bit4) {
            MATRIX* bone = &preview_actor->boneMatrices[preview_weapon_model->attachmentBone];

            bone->t[0] -= preview_actor->companionState.offsetX;
            bone->t[1] -= preview_actor->companionState.offsetY;
            bone->t[2] -= preview_actor->companionState.offsetZ;

            Actors_DrawActorWeaponModel(preview_weapon_model, bone);

            bone->t[0] += preview_actor->companionState.offsetX;
            bone->t[1] += preview_actor->companionState.offsetY;
            bone->t[2] += preview_actor->companionState.offsetZ;
        }
    }

    return 0;
}

// decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:306)
void Menu_Framework_ResetAshleyEquipmentPose(void)
{
    u_short* animation_length_address;
    int i;
    int i_2;
    Actors_ActorRecord* ashley_actor;

    ashley_actor = Actors_actorRecords[0];

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* equipped_weapon_model = Actors_weaponModels[i];

        if (equipped_weapon_model != 0) {
            if (ashley_actor->instance.weaponDrawn) {
                equipped_weapon_model->reserved11 = 0x40;
                equipped_weapon_model->reserved08Bit4 = 1;
            } else {
                equipped_weapon_model->reserved11 = 0;
            }
        }
    }

    Field_RefreshAshleyInteractionTile(ashley_actor);

    i_2 = 1;

    if (ashley_actor->instance.weaponDrawn) {
        i_2 = 101;
    }

    ashley_actor->animationId = i_2;
    ashley_actor->weaponPoseActive = 0;
    ashley_actor->animationHeader = Actors_FindActorAnimationHeader(ashley_actor, i_2, &animation_length_address);
    ashley_actor->animationSet = Actors_lastResolvedSequenceIndex;
    ashley_actor->animationLength = *animation_length_address;

    Actors_InitializeActorAnimationPose(ashley_actor, &ashley_actor->pose);
    Actors_AdvanceSkeletalAnimation(ashley_actor, &ashley_actor->pose, 1);
    Core_Memory_CopyAlignedWords(&ashley_actor->blendPose, &ashley_actor->pose, sizeof ashley_actor->blendPose);

    ashley_actor->pose.frame = 0;
    ashley_actor->queuedAnimation = 0;
    ashley_actor->blendFrames = 0;
    ashley_actor->blendFrame = 0;
}

// decomp: MAINMENU.PRG func_800FA3FC (src/MENU/MAINMENU.PRG/224.c:352)
void Menu_Framework_QueueAshleyWeaponAnimations(int sequence_model_id)
{
    Content_ActorAssetRequest sequence_request;

    Content_ReleaseActorSequence(0, 0);

    sequence_request.assetKind = 7;
    sequence_request.targetSlot = 0;
    sequence_request.modelId = sequence_model_id;
    sequence_request.subKind = 0;

    Content_QueueActorAssetRequest(&sequence_request);
}

