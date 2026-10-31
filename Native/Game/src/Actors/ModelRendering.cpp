// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/AppearanceTransition.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelDrawing.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Palette.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/SoundOverrides.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Menu/Inventory/Inventory.h"
#include "ModelLoading.h"
#include "Render/Camera.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelGeometry.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "State/Timers.h"

// decomp: D_800E9308 (D_800E9308@BATTLE/BATTLE.PRG)
int Actors_renderPassCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F2458 (D_800F2458@BATTLE/BATTLE.PRG)
Actors_ModelRenderContext Actors_modelRenderContext;

// decomp: BATTLE.PRG func_800AEAE8 (src/BATTLE/BATTLE.PRG/44F14.c:826)
void Actors_PrepareActorRootTransform(Actors_ActorRecord* actor)
{
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();

    Actors_modelRenderContext.modelScale[0] = 64;
    Actors_modelRenderContext.modelScale[1] = 64;
    Actors_modelRenderContext.modelScale[2] = 64;

    if (actor->instance.scaleX == ONE && actor->instance.scaleY == ONE
        && actor->instance.scaleZ == ONE) {
        setVector(&scratch->rootOffset, actor->blendPose.boneKeys[41].vx >> 1,
            actor->blendPose.boneKeys[41].vy >> 1, actor->blendPose.boneKeys[41].vz >> 1);
    } else {
        setVector(&scratch->rootOffset,
            actor->blendPose.boneKeys[41].vx * actor->instance.scaleX / (ONE * 2),
            actor->blendPose.boneKeys[41].vy * actor->instance.scaleY / (ONE * 2),
            actor->blendPose.boneKeys[41].vz * actor->instance.scaleZ / (ONE * 2));
    }

    setVector(&scratch->rootOffset, scratch->rootOffset.vx * actor->scale / ONE,
        scratch->rootOffset.vy * actor->scale / ONE,
        scratch->rootOffset.vz * actor->scale / ONE);
    Actors_TransformActorRootOffset(actor);
    scratch->angleX = actor->instance.lookYaw;
    scratch->angleY = actor->instance.lookPitch;
}

// decomp: BATTLE.PRG func_800AEF94 (src/BATTLE/BATTLE.PRG/44F14.c:923)
void Actors_UpdateAndDrawRoomActors(MATRIX* camera)
{
    /* The original routine reserves 0x30 bytes of unused local storage. */
    int unused[12];
    Render_ModelScratchpad* objectScratch;
    Render_FrameScratch* frame;
    Actors_ActorRecord** previous;
    Actors_ActorRecord** previousAttached;
    Actors_ActorRecord* actor;
    Actors_ActorRecord* attached;
    Actors_ActorRecord** entries;
    Actors_ActorRecord** weaponActors;
    Field_GimmickInstance* object;
    int packedPosition;
    int weaponActorFlags;
    int i;
    int step;
    int modelHeight;
    u_char attachmentId;
    Actors_WeaponModel** weapons;
    Actors_WeaponModel* weapon;

    Render_PrepareActorCameraMatrices(camera);
    i = 0;
    entries = Actors_actorRecords;
    do {
        actor = entries[i];
        if ((actor != NULL) && (actor->instance.boneCount != 0) && !actor->instance.isSkipped
            && (actor->instance.attachBone != 0xFC)) {
            ((void (*)(void*))Actors_UpdateActorTextureAnimations)(actor);
            if (actor->instance.palettePulse.kind != 0) {
                Actors_UpdateActorPalettePulse(actor, NULL, 0);
            }
            if (actor->instance.actionAnimationState) {
                Actors_UpdateActorAppearanceTransition(actor);
            }
            actor->instance.wasDrawn = 0;
            if (actor->instance.isFrozen) {
                Audio_UpdateActorSoundTransition(actor);
                step = 0;
                if (i > 0) {
                    int texture = actor->modelId;
                    previous = entries;
                    do {
                        ++step;
                        if (*previous != NULL && (*previous)->modelId == texture) {
                            goto texture_ready;
                        }
                        ++previous;
                    } while (step < i);
                }
                ((void (*)(void*))Actors_UpdateActorTextureScrolling)(actor);
            texture_ready:;

                if (actor->routePhase) {
                    Actors_UpdateActorRoute(i, actor);
                }
                if (State_tickSpeed == 4) {
                    actor->framesPerStep *= 2;
                }
                Actors_UpdateActorAnimationBlend(actor);
                actor->framesPerStep = 1;
                actor->framesPerStep = actor->fastAnimation + 1;
            } else {
                Actors_PrepareActorRootTransform(actor);
            }
            if (actor->instance.isAttached) {
                MATRIX* parent_matrix = Actors_ResolveAttachmentParentMatrix(actor);
                if (parent_matrix != NULL) {
                    Actors_DrawActorModel(actor, parent_matrix, 0U);
                }
            } else {
                Actors_DrawActorModel(actor, camera, 0U);
                attachmentId = actor->companionSlot;
                if (attachmentId != 0) {
                    if (attachmentId >= 2U) {
                        attached = entries[actor->companionSlot];
                        ((void (*)(void*))Actors_UpdateActorTextureAnimations)(attached);
                        if (attached->instance.palettePulse.kind != 0) {
                            Actors_UpdateActorPalettePulse(attached, NULL, 0);
                        }
                        if (attached->instance.actionAnimationState) {
                            Actors_UpdateActorAppearanceTransition(attached);
                        }
                        if (actor->instance.isFrozen) {
                            Audio_UpdateActorSoundTransition(attached);
                            step = 0;
                            if (i > 0) {
                                int texture = attached->modelId;
                                previousAttached = entries;
                                do {
                                    ++step;
                                    if (*previousAttached != NULL && (*previousAttached)->modelId == texture) {
                                        goto linked_texture_ready;
                                    }
                                    ++previousAttached;
                                } while (step < i);
                            }
                            ((void (*)(void*))Actors_UpdateActorTextureScrolling)(attached);
                        linked_texture_ready:;
                        }
                    }
                    frame = (Render_FrameScratch *)((void*)((uintptr_t)Core_ScratchBase() + 0x000));
                    frame->orderingTable = (u_char*)frame->orderingTable + actor->companionState.depthBias * 4;
                    actor->instance.position.vx = (u_short)actor->instance.position.vx
                                            + (signed char)(u_char)actor->companionState.offsetX;
                    actor->instance.position.vy = (u_short)actor->instance.position.vy
                                            + (signed char)(u_char)actor->companionState.offsetY;
                    actor->instance.position.vz = (u_short)actor->instance.position.vz
                                            + (signed char)(u_char)actor->companionState.offsetZ;
                    Actors_UpdateMotionAndLookAt(actor, (int)actor->companionSlot);
                    Actors_DrawActorModel(actor, camera, actor->companionSlot);
                    frame->orderingTable = (u_char*)frame->orderingTable - actor->companionState.depthBias * 4;
                    actor->instance.position.vx = (u_short)actor->instance.position.vx
                                            - (signed char)(u_char)actor->companionState.offsetX;
                    actor->instance.position.vy = (u_short)actor->instance.position.vy
                                            - (signed char)(u_char)actor->companionState.offsetY;
                    actor->instance.position.vz = (u_short)actor->instance.position.vz
                                            - (signed char)(u_char)actor->companionState.offsetZ;
                }
            }
            if (((u_short)actor->collisionRadius >= 0x80U) && ((u_char)actor->blendFrame != 0)) {
                actor->instance.wasDrawn = 1;
            }
        }
        i += 1;
    } while (i < 0x11);
    i = 0;
    weaponActors = Actors_actorRecords;
    weapons = Actors_weaponModels;
    do {
        weapon = *weapons;
        if ((weapon != NULL) && weapon->reserved08Bit4 && (weapon->attachmentBone != 0)) {
            actor = weaponActors[weapon->actorId];
            if (actor != NULL) {
                weaponActorFlags = (*(u_int*)((char*)actor + 8));
                if (weaponActorFlags & 1) {
                    goto weapon_next;
                }
                if (!(weaponActorFlags & 2)) {
                    goto weapon_next;
                }
                if (weaponActorFlags & 0x1000) {
                    goto weapon_next;
                }
                Actors_DrawActorWeaponModel(weapon, &actor->boneMatrices[weapon->attachmentBone]);
            }
        }
    weapon_next:;
        i += 1;
        weapons++;
    } while (i < 0x14);
    objectScratch = Render_ModelWorkspace();
    frame = (Render_FrameScratch *)((void*)((uintptr_t)Core_ScratchBase() + 0x000));
    i = 0;
    entries = (Actors_ActorRecord **)((void*)Field_gimmickInstances);
    frame->orderingTable = (u_char*)frame->orderingTable + 0x10;
    do {
        object = (Field_GimmickInstance *)((void*)*entries);
        if ((object != NULL) && (object->boneCount != 0)) {
            if (((i == Actors_pushedGimmickIndex) && ((*(u_char (*))&Actors_isActorUpdateComplete) == 0)) || (object->mode == 0xF7)) {
                Field_UpdateGimmickMotion((Field_GimmickPhysicsView*)object, State_tickSpeed / 2);
            }
            packedPosition = *(int*)&object->positionX;
            objectScratch->rootOffset.vz = (u_short)object->positionZ;
            modelHeight = *(int*)((u_char*)object->model + 0x50);
            *(int*)&objectScratch->rootOffset = packedPosition;
            objectScratch->rootOffset.vy += modelHeight / 2;
            if (object->palettePulse.kind != 0) {
                Actors_UpdateActorPalettePulse(NULL, object, 0);
            }
            if (object->motionState) {
                Field_UpdateGimmickAppearanceTransition(object);
            }
            Actors_DrawGimmickModel(object, camera);
        }
        i += 1;
        entries++;
    } while (i < 0x10);
    (*(u_char (*))&Actors_isActorUpdateComplete) = 0;
    frame->orderingTable = (u_char*)frame->orderingTable - 0x10;
    Actors_renderPassCounter += 1;
}

// decomp: BATTLE.PRG func_800B07DC (src/BATTLE/BATTLE.PRG/44F14.c:1695)
void Actors_TransformActorRootOffset(Actors_ActorRecord* actor)
{
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();

    setVector(
        &scratch->rotation, actor->instance.rotationX, actor->instance.facing, actor->instance.rotationZ);
    Core_Gte_RotMatrixYXZ(&scratch->rotation, &scratch->matrix);
    scratch->matrix.t[0] = 0;
    scratch->matrix.t[1] = 0;
    scratch->matrix.t[2] = 0;
    Core_Gte_SetRotMatrix(&scratch->matrix);
    Core_Gte_SetTransMatrix(&scratch->matrix);
    Core_Gte_LoadV0(&scratch->rootOffset);
    Core_Gte_Rtv0Tr();
    Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
    setVector(&scratch->rootOffset, scratch->matrix.t[0], scratch->matrix.t[1],
        scratch->matrix.t[2]);
    scratch->rootOffset.vx += actor->instance.position.vx;
    scratch->rootOffset.vz += actor->instance.position.vz;
    scratch->rootOffset.vy += actor->instance.position.vy;
}

// decomp: BATTLE.PRG func_800B1A68 (src/BATTLE/BATTLE.PRG/49268.c:48)
void Actors_DrawGimmickModel(Field_GimmickInstance* object, MATRIX* camera)
{
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();
    u_int attachment;
    int value;

    scratch->model = object->model;
    Core_Gte_SetRotMatrix(camera);
    Core_Gte_SetTransMatrix(camera);
    setVector(&scratch->position, scratch->rootOffset.vx, scratch->rootOffset.vy + 16,
        scratch->rootOffset.vz);
    Core_Gte_LoadV0(&scratch->position);
    Core_Gte_Rtv0Tr();
    attachment = object->objectInfo.objectId;

    if (attachment >= 4) {
        MATRIX holderRotation;
        SVECTOR anchors[2];
        Actors_ActorRecord* holder;

        if (object->parent == 0xFF) {
            goto rotate;
        }

        holder = Actors_actorRecords[object->parent];
        Actors_ActorTaggedBonePosition(object->parent, 0xF0, &anchors[0], 1);
        Actors_ActorTaggedBonePosition(object->parent, 0xF1, &anchors[1], 1);
        Core_Gte_SetRotMatrix(camera);
        Core_Gte_SetTransMatrix(camera);
        setVector(&scratch->position, (anchors[0].vx + anchors[1].vx) / 2,
            (anchors[0].vy + anchors[1].vy) / 2, (anchors[0].vz + anchors[1].vz) / 2);
        Core_Gte_LoadV0(&scratch->position);
        Core_Gte_Rtv0Tr();
        setVector(&scratch->rotation, object->rotationOffset.vx + object->rotationX,
            object->rotationOffset.vy + object->rotationY, object->rotationOffset.vz + object->rotationZ);
        Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
        Core_Math_BuildEulerRotationMatrixQuadrant(&scratch->rotation, &scratch->matrix);
        setVector(&scratch->rotation, holder->instance.rotationX, holder->instance.facing,
            holder->instance.rotationZ);
        Core_Math_BuildEulerRotationMatrixQuadrant(&scratch->rotation, &holderRotation);
        Core_Math_PremultiplyRotationMatrix(&holderRotation, &scratch->matrix);
    } else if (attachment < 2) {
        if (object->mode != 0) {
            setVector(&scratch->rotation, object->rotationX, object->rotationY, object->rotationZ);
            Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
            Core_Gte_RotMatrixYXZ(&scratch->rotation, &scratch->matrix);
            Core_Math_PremultiplyRotationMatrix(camera, &scratch->matrix);
            Core_Gte_MulMatrix(&scratch->matrix, &object->matrices[4]);
            goto composed;
        }

        scratch->matrix = object->matrices[4];
        Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
    } else {
    rotate:
        copyVector(&scratch->rotation, &object->rotationOffset);
        Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
        applyVector(&scratch->rotation, object->rotationX, object->rotationY, object->rotationZ, +=);
        *(int*)&scratch->rotation &= 0x0FFF0FFF;
        *(int*)&scratch->rotation.vz &= 0xFFF;
        Core_Math_BuildEulerRotationMatrixQuadrant(&scratch->rotation, &scratch->matrix);
    }

    Core_Math_PremultiplyRotationMatrix(camera, &scratch->matrix);
composed:
    Core_Math_ScaleMatrixColumns(&scratch->matrix, &Actors_roomActorScale);
    Actors_skeletonPose.boneMatrices[0] = scratch->matrix;
    object->matrices[0] = scratch->matrix;

    if (object->scaleX != 64 || object->scaleY != 64 || object->scaleZ != 64) {
        setVector(&scratch->scale, object->scaleX, object->scaleY, object->scaleZ);
        applyVector(&scratch->scale, 64, 64, 64, *=);
        Core_Math_ScaleMatrixColumns(&scratch->matrix, &scratch->scale);
        Actors_skeletonPose.boneMatrices[0] = scratch->matrix;
        object->matrices[0] = scratch->matrix;
    }

    Actors_skeletonRootMatrix = scratch->matrix;
    object->matrices[1] = scratch->matrix;

    if (!object->isVisible
        || !Render_ProjectModelVertices(
            object, &object->matrices[0], -Render_nearClip * 4, object->model)) {
        return;
    }

    scratch->texturePage = GetTPage(0, 0, 64, 256);

    if (object->palettePulse.kind != 0 || object->motionState >= 6) {
        scratch->palette = GetClut(object->slot * 16 + 768, 255);
    } else {
        scratch->palette = GetClut(object->slot * 16 + 768, 237);
    }

    value = object->motionState >= 4 && object->motionState < 9 && object->motionState != 6;

    if (object->motionState >= 12) {
        value = 1;
    }

    if (value) {
        Render_DrawModelGouraudPolygonsWithOverrides(&Actors_modelRenderContext, object, &object->baseColor, scratch->model);
    } else {
        Render_DrawModelGouraudPolygons(&Actors_modelRenderContext, object, &object->baseColor, scratch->model);
    }
}

// decomp: BATTLE.PRG func_800B217C (src/BATTLE/BATTLE.PRG/49268.c:155)
void Actors_DrawActorWeaponModel(Actors_WeaponModel* weapon, MATRIX* source)
{
    CVECTOR color;
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();
    Actors_ActorRecord* actor = Actors_actorRecords[weapon->actorId];
    int value;
    int i;

    scratch->model = (Actors_ShpModel*)weapon->sectionOffsets;
    scratch->matrix = *source;

    if (weapon->reserved13 != 0) {
        int target = weapon->reserved12;
        int current = weapon->reserved11;

        value = target - current;
        value /= weapon->reserved13;
        weapon->reserved11 = current + value;
        --weapon->reserved13;

        if (weapon->reserved11 == 0 && weapon->reserved13 == 0) {
            weapon->reserved08Bit4 = 0;
        }
    }

    if (weapon->reserved11 != 64) {
        value = weapon->reserved11;
        value *= 64;
        setVector(&scratch->scale, value, value, value);
        Core_Math_ScaleMatrixColumns(&scratch->matrix, &scratch->scale);
    }

    Actors_skeletonPose.boneMatrices[0] = scratch->matrix;
    weapon->boneWorldMatrices[0] = scratch->matrix;
    scratch->position.vx = scratch->position.vy = 0;

    for (i = 1; i < weapon->boneCount; ++i) {
        int parentBone = scratch->model->bones[i].parentIndex;

        scratch->parentBoneMatrix = &weapon->boneWorldMatrices[parentBone];
        Core_Gte_SetRotMatrix(scratch->parentBoneMatrix);
        Core_Gte_SetTransMatrix(scratch->parentBoneMatrix);
        setVector(&scratch->position, -scratch->model->bones[parentBone].length, 0, 0);
        Core_Gte_LoadV0(&scratch->position);
        Core_Gte_Rtv0Tr();
        scratch->rotation.vx = weapon->bones[i].vx;
        scratch->rotation.vy = weapon->bones[i].vy;
        scratch->rotation.vz = weapon->bones[i].vz;
        Core_Gte_StoreLvnl((VECTOR *)(scratch->matrix.t));
        *(int*)&scratch->rotation &= 0x0FFF0FFF;
        *(int*)&scratch->rotation.vz &= 0xFFF;
        Core_Math_BuildEulerRotationMatrixQuadrant(&scratch->rotation, &scratch->matrix);
        Core_Math_PremultiplyRotationMatrix(&Actors_skeletonPose.boneMatrices[parentBone], &scratch->matrix);
        Actors_skeletonPose.boneMatrices[i] = scratch->matrix;
        weapon->boneWorldMatrices[i] = scratch->matrix;
    }

    if (!actor->instance.isVisible
        || (actor->instance.position.vy >= 256
            && (actor->instance.slot == 0 || ((short (*)(void))Event_EventPlaybackState)() != 4))) {
        return;
    }

    if (!Render_ProjectModelVertices(
            weapon, &weapon->boneWorldMatrices[0], -Render_nearClip * 4, weapon->sectionOffsets)) {
        return;
    }

    scratch->texturePage = Render_legacyWeaponTextureSlots[weapon->textureSlot].texturePage;
    scratch->palette = Render_legacyWeaponTextureSlots[weapon->paletteSlotOffset].palette;
    value = actor->instance.actionAnimationState >= 4 && actor->instance.actionAnimationState < 9 && actor->instance.actionAnimationState != 6;

    if (actor->instance.actionAnimationState == 15) {
        value = 1;
    }

    if (weapon->reserved08Bit6) {
        color.r = weapon->reserved5C0;
        color.g = weapon->reserved5C0;
        color.b = weapon->reserved5C0;
    } else {
        *(int*)&color = *(int*)&actor->instance.baseColor;
    }

    if (value) {
        Render_DrawModelFlatPolygonsWithOverrides(&Actors_modelRenderContext, weapon, &color, scratch->model);
    } else {
        Render_DrawModelFlatPolygons(&Actors_modelRenderContext, weapon, &color, scratch->model);
    }
}

// decomp: INITBTL.PRG func_800FA7B0 (src/BATTLE/INITBTL.PRG/FB0.c:19)
void Actors_InitializeActorModelResources(void)
{
    VECTOR unit_room_scale;
    int equipment_actor_slot;
    int i;

    Actors_reservedControlByte = 0;

    for (i = 0; i < 16; ++i) {
        Content_legacyActorAssetRequests[i].assetKind = 0;
    }

    for (i = 0; i < 12; ++i) {
        short (*primary_texture_origins)[2];
        Render_legacyActorTextureSlots[i].texturePage =
            GetTPage(0, 0, (*(short (*)[][2])&Actors_textureSlotOrigins)[i][0] << 6, (primary_texture_origins = (*(short (*)[][2])&Actors_textureSlotOrigins))[i][1] << 8);
        Render_legacyActorTextureSlots[i].palette = GetClut(768, i + 228);
        Render_legacyActorTextureSlots[i].alternatePalette = GetClut(768, i + 246);
        Render_legacyActorTextureSlots[i].referenceCount = 0;
    }

    for (; i < 24; ++i) {
        short (*secondary_texture_origins)[2];
        Render_legacyActorTextureSlots[i].texturePage =
            GetTPage(0, 0, (*(short (*)[][2])&Actors_textureSlotOrigins)[i][0] << 6, (secondary_texture_origins = (*(short (*)[][2])&Actors_textureSlotOrigins))[i][1] << 8);
        Render_legacyActorTextureSlots[i].palette = GetClut(768, 229);
        Render_legacyActorTextureSlots[i].alternatePalette = GetClut(768, 247);
        Render_legacyActorTextureSlots[i].referenceCount = 0;
    }

    for (i = 0; i < 17; ++i) {
        Actors_actorRecords[i] = NULL;
    }

    Actors_primarySequenceBuffer = Core_Memory_AllocateFromLowEnd(0x7800);
    Actors_sequenceWorkBuffer = Core_Memory_AllocateFromLowEnd(0x4800);

    for (i = 0; i < 20; ++i) {
        equipment_actor_slot = (i / 2);
        Render_legacyWeaponTextureSlots[i].texturePage = GetTPage(1, 0, equipment_actor_slot * 64, 0);
        Render_legacyWeaponTextureSlots[i].palette = GetClut(((i & 1) * 48) + 928, equipment_actor_slot + 228);
        Render_legacyWeaponTextureSlots[i].referenceCount = 0;
    }

    for (i = 0; i < 20; ++i) {
        Actors_weaponModels[i] = NULL;
    }

    for (i = 0; i < 2; ++i) {
        Content_legacyWeaponModelBuffers[i] =
            (Actors_WeaponModelData *)(Core_Memory_AllocateFromLowEnd(sizeof *Content_legacyWeaponModelBuffers[i]));
    }

    for (i = 0; i < 16; ++i) {
        Field_gimmickInstances[i] = NULL;
    }

    (*(char (*))&Actors_sharedSequenceBankIndex) = 0;
    (*(char (*)[])&Actors_sharedSequenceBankTexturePages)[0] = 0;
    (*(char (*)[])&Actors_sharedSequenceBankTexturePages)[1] = 0;
    (*(char (*)[])&Actors_sharedSequenceBankTexturePages)[2] = 0;
    Actors_sharedSequenceData[0] = NULL;
    Actors_sharedSequenceData[1] = NULL;
    Actors_sharedSequenceData[2] = NULL;
    (*(char (*))&Content_legacySequenceParsePhase) = 0;
    (*(char (*))&Content_legacyEtmLoadPhase) = 0;
    Actors_reservedControlByte = 0;
    (*(char (*))&Content_modelParseResetFlag) = 0;
    Menu_Inventory_equipmentModelLoadStep = 0;
    (*(char (*))&Content_legacyModelLoadPhase) = 0;
    (*(char (*))&Actors_sharedSequenceBankIndex) = 0;
    unit_room_scale.vx = 0x1000;
    unit_room_scale.vy = 0x1000;
    unit_room_scale.vz = 0x1000;
    Actors_SetRoomActorScale(&unit_room_scale);
    (*(char (*))&Actors_pushedGimmickIndex) = 0;
    Actors_isActorUpdateComplete = 0;
}

}  // extern "C"
