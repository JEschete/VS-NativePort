// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelDrawing.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Core/Math/ScalarMath.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"

// decomp: D_800F3C58 (D_800F3C58@BATTLE/BATTLE.PRG)
Actors_SkeletonPose Actors_skeletonPose;

// decomp: D_800F3C78 (D_800F3C78@BATTLE/BATTLE.PRG)
MATRIX Actors_skeletonRootMatrix;

// decomp: vs_battle_wepModels (vs_battle_wepModels@BATTLE/BATTLE.PRG)
Actors_WeaponModel* Actors_weaponModels[20];

// decomp: D_800F49B8 (D_800F49B8@BATTLE/BATTLE.PRG)
MATRIX Actors_boneMatrixScratch;

// decomp: BATTLE.PRG func_8009E228 (src/BATTLE/BATTLE.PRG/30DB0.c:2525)
int Actors_ActorPitchToPoint(Actors_ActorRecord* actor, SVECTOR* target_point)
{
    SVECTOR root_bone_position;
    int horizontal_distance_work;
    int z_distance_squared;

    if (target_point == NULL) {
        return 0;
    }

    horizontal_distance_work = target_point->vx - actor->instance.position.vx;
    horizontal_distance_work = horizontal_distance_work * horizontal_distance_work;
    z_distance_squared = target_point->vz - actor->instance.position.vz;

    if (horizontal_distance_work < 0) {
        horizontal_distance_work = -horizontal_distance_work;
    }

    z_distance_squared = z_distance_squared * z_distance_squared;

    if (z_distance_squared < 0) {
        z_distance_squared = -z_distance_squared;
    }

    horizontal_distance_work = Core_Math_SquareRootInteger(horizontal_distance_work + z_distance_squared);
    Actors_ActorBonePosition(actor->instance.slot, 0, &root_bone_position, 0);
    return Core_Gte_Atan2(target_point->vy - root_bone_position.vy, horizontal_distance_work);
}

// decomp: BATTLE.PRG func_8009FC60 (src/BATTLE/BATTLE.PRG/30DB0.c:3512)
int Actors_SetActorHeadTracking(int actor_id, int tracking_target, SVECTOR* tracking_vector, int tracking_step)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return -1;
    }

    if (Actors_FindActorBoneTag(actor_id, 2, 3) < 0) {
        return -2;
    }

    actor->headTrackTarget = tracking_target;
    if (tracking_vector == NULL) {
        actor->headTracking.vx = actor->instance.lookPitch;
        actor->headTracking.vy = actor->instance.lookYaw;
    } else {
        actor->headTracking = *tracking_vector;
    }
    if (tracking_step == -1) {
        Actors_ResetActorHeadTrackingSpeed(actor);
    } else {
        actor->headTracking.pad = tracking_step;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009FD38 (src/BATTLE/BATTLE.PRG/30DB0.c:3540)
void Actors_ResetActorHeadTrackingSpeed(Actors_ActorRecord* actor)
{
    actor->headTracking.pad = 30;
    if (actor->collisionRadius > 128) {
        actor->headTracking.pad = 20;
    }
}

// decomp: BATTLE.PRG func_800A1178 (src/BATTLE/BATTLE.PRG/30DB0.c:4200)
int Actors_ActorTaggedBoneMatrix(int actor_id, int bone_tag, MATRIX* matrix_out, u_long* bone_length_out)
{
    int bone_index = Actors_FindActorBoneTag(actor_id, bone_tag, 0);
    if (bone_index < 0) {
        return bone_index;
    }
    Actors_ActorBoneMatrix(actor_id, bone_index, matrix_out, bone_length_out);
    return 0;
}

// decomp: BATTLE.PRG func_800A11D8 (src/BATTLE/BATTLE.PRG/30DB0.c:4210)
void Actors_ActorBoneMatrix(int actor_id, int bone_index, MATRIX* matrix_out, u_long* bone_length_out)
{
    Actors_ActorRecord* model_owner = Actors_actorRecords[actor_id];

    if (model_owner == NULL) {
        model_owner = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actor_id);
        if (model_owner == NULL) {
            return;
        }
    }
    *matrix_out = model_owner->boneMatrices[bone_index];
    *bone_length_out = model_owner->instance.model->bones[bone_index].length;
}

// decomp: BATTLE.PRG func_800A1280 (src/BATTLE/BATTLE.PRG/30DB0.c:4224)
int Actors_ProjectActorTaggedBone(int actor_id, int bone_tag, SVECTOR* screen_position_out, int endpoint)
{
    long packed_screen_position;
    long depth_cue_or_status;
    long projection_flags;
    int resolved_bone_index;
    int projection_result;

    if (bone_tag == 0xFB) {
        short* screen_y_halfword;
        unsigned int screen_y;
        Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

        if (actor == NULL) {
            Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(actor_id);
            if (gimmick == NULL) {
                return -1;
            }
            screen_position_out->vx = gimmick->positionX;
            screen_position_out->vz = gimmick->positionZ;
            screen_position_out->vy = gimmick->positionY - 0xC0;
        } else {
            if (actor->instance.reactionState) {
                return -1;
            }
            screen_position_out->vx = actor->instance.position.vx;
            screen_position_out->vz = actor->instance.position.vz;
            screen_position_out->vy = actor->instance.position.vy - actor->targetPointHeight;
        }

        Core_Gte_SetRotMatrix(&((Render_FrameScratch*)Core_ScratchBase())->viewMatrix);
        Core_Gte_SetTransMatrix(&((Render_FrameScratch*)Core_ScratchBase())->viewMatrix);
        screen_position_out->vz = Core_Gte_RotTransPers(screen_position_out, &packed_screen_position, &depth_cue_or_status, &projection_flags);
        screen_position_out->vx = packed_screen_position;
        screen_y_halfword = &((short*)(&packed_screen_position))[1];
        screen_y = *screen_y_halfword;
        screen_position_out->vy = screen_y;

        return 0;
    }

    resolved_bone_index = Actors_FindActorBoneTag(actor_id, bone_tag, 0);
    depth_cue_or_status = resolved_bone_index;

    if (resolved_bone_index < 0) {
        return resolved_bone_index;
    }

    projection_result = ((int (*)(int actor_id, int bone_index, int* screen_position_out, int endpoint))Actors_ProjectActorBoneEndpoint)(actor_id, resolved_bone_index, (int*)screen_position_out, endpoint);
    depth_cue_or_status = projection_result;
    if (projection_result < 0) {
        return projection_result;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A13EC (src/BATTLE/BATTLE.PRG/30DB0.c:4280)
int Actors_ProjectActorBone(int actor_id, int bone_index, int* screen_position_out, int endpoint)
{
    int projection_result = ((int (*)(int actor_id, int bone_index, int* screen_position_out, int endpoint))Actors_ProjectActorBoneEndpoint)(actor_id, bone_index, screen_position_out, endpoint);

    if (projection_result < 0) {
        return projection_result;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A152C (src/BATTLE/BATTLE.PRG/38C1C.c:103)
int Actors_FindActorBoneTag(int actor_id, int tag_value, int tag_kind)
{
    Actors_ActorRecord* model_owner;
    Actors_ShpModel* model;
    int i;
    int nBones;
    int high_limb_matches;

    model_owner = Actors_actorRecords[actor_id];

    if (model_owner == NULL) {
        model_owner = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actor_id);
        if (model_owner == NULL) {
            return -1;
        }
    }

    nBones = model_owner->instance.boneCount;
    model = model_owner->instance.model;

    do {
        if (tag_value == 0xFF) {
            return 0;
        }
    } while (0);

    for (i = 0; i < nBones; ++i) {
        switch (tag_kind) {
        case 0:
            if (model->bones[i].tag == (tag_value & 0xFF)) {
                return i;
            }
            break;

        case 1:
            high_limb_matches = (model->bones[i].limbIds >> 4) == ((tag_value & 0xFF) + 1);
            if (high_limb_matches) {
                return i;
            }
            break;

        case 2:
            if ((model->bones[i].limbIds & 0xF) == ((tag_value & 0xFF) + 1)) {
                return i;
            }
            break;

        case 3:
            if (model->bones[i].auxiliaryTag == (tag_value & 0xFF)) {
                return i;
            }
            break;
        }
    }

    return -4;
}

// decomp: BATTLE.PRG func_800A1648 (src/BATTLE/BATTLE.PRG/38C1C.c:161)
int Actors_ActorBoneTag(int actor_id, int bone_index, int tag_kind)
{
    Actors_ActorRecord* actor;
    Actors_ShpModel* model;
    u_int v;

    actor = Actors_actorRecords[actor_id];
    if (actor == NULL) {
        actor = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actor_id);
        if (actor == NULL) {
            return -1;
        }
    }
    model = actor->instance.model;
    switch (tag_kind) {
    case 0:
        v = model->bones[bone_index].tag;
        break;
    case 1:
        v = model->bones[bone_index].limbIds >> 4;
        if (v == 0) {
            return -3;
        }
        break;
    case 2:
        v = model->bones[bone_index].limbIds & 0xF;
        if (v == 0) {
            return -3;
        }
        break;
    }
    return v;
}

// decomp: BATTLE.PRG func_800A1720 (src/BATTLE/BATTLE.PRG/38C1C.c:195)
int Actors_ActorAnchorMatrix(int actor_id, int bone_selector, int* matrix_out, int* bone_length_out)
{
    int bone_index_or_status = bone_selector;

    if (bone_selector == 0xFD) {
        bone_index_or_status = 0;
    }

    bone_index_or_status = Actors_ActorBoneWorldMatrix(actor_id, bone_index_or_status, matrix_out, bone_length_out);

    if (bone_index_or_status < 0) {
        return bone_index_or_status;
    }

    if (bone_selector == 0xFD) {
        matrix_out[6] = Actors_actorRecords[actor_id]->instance.position.vy;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A17BC (src/BATTLE/BATTLE.PRG/38C1C.c:216)
int Actors_ActorBoneWorldMatrix(int actor_id, int bone_selector, void* matrix_out, int* bone_length_out)
{
    Actors_ActorRecord* actor;
    Actors_ActorRecord* obj;
    Actors_WeaponModel* model;
    int _[22] __attribute__((unused));

    MATRIX* bone_transform = Actors_ActorBoneWorldTransform(actor_id, bone_selector, &Actors_boneMatrixScratch);
    if (bone_transform == NULL) return -1;
    *(MATRIX*)matrix_out = *bone_transform;

    actor = Actors_actorRecords[actor_id];
    if (actor == NULL) {
        obj = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actor_id);
        if (obj == NULL) {
            return 0;
        }
        *bone_length_out = obj->instance.model->bones[bone_selector].length;
    } else if (bone_selector == 0xFF) {
        *bone_length_out = 0;
    } else if ((bone_selector & 0xF0) == 0x40) {
        model = Actors_weaponModels[actor_id * 2];
        if (model == NULL) {
            return 0;
        }
        *bone_length_out = ((Actors_ShpModel*)model->sectionOffsets)->bones[bone_selector - 0x3F].length;
    } else {
        *bone_length_out = actor->instance.model->bones[bone_selector].length;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246)
int Actors_ActorTaggedBonePosition(int actor_id, int bone_tag, SVECTOR* position_out, int endpoint)
{
    Actors_ActorRecord* model_owner;
    int position_query_value;

    if (bone_tag == 0xFB) {
        model_owner = Actors_actorRecords[actor_id];

        if (model_owner == NULL) {
            model_owner = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actor_id);
            if (model_owner == NULL) {
                return -1;
            }

            *position_out = model_owner->instance.position;
            position_out->vy -= 0xC0;
        } else {
            *position_out = model_owner->instance.position;
            position_out->vy -= model_owner->targetPointHeight;
        }

        return 0;
    }

    if (bone_tag == 0xFD) {
        *position_out = Actors_actorRecords[actor_id]->instance.position;
        return 0;
    }

    if ((bone_tag & 0xF0) == 0x40) {
        position_query_value = bone_tag;
    } else if (bone_tag == 0xFA) {
        model_owner = Actors_actorRecords[actor_id];
        *position_out = model_owner->instance.position;
        position_query_value = model_owner->anchorHeight * model_owner->scale / ONE;
        position_out->vy += position_query_value;
        return 0;
    } else {
        position_query_value = Actors_FindActorBoneTag(actor_id, bone_tag, 0);

        if (position_query_value < 0) {
            return position_query_value;
        }
    }

    position_query_value = Actors_ActorBoneEndpointPosition(actor_id, position_query_value, position_out, endpoint);

    if (position_query_value < 0) {
        return position_query_value;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1AF8 (src/BATTLE/BATTLE.PRG/38C1C.c:300)
int Actors_ActorBonePosition(int actor_id, int bone_index, SVECTOR* position_out, int endpoint)
{
    int query_result = Actors_ActorBoneEndpointPosition(actor_id, bone_index, position_out, endpoint);

    if (query_result < 0) {
        return query_result;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1B28 (src/BATTLE/BATTLE.PRG/38C1C.c:311)
int Actors_ActorHighGroupBonePosition(int actor_id, int limb_index, SVECTOR* position_out, int endpoint)
{
    int bone_index_or_status = Actors_FindActorBoneTag(actor_id, limb_index, 1);

    if (bone_index_or_status < 0) {
        return bone_index_or_status;
    }

    bone_index_or_status = Actors_ActorBoneEndpointPosition(actor_id, bone_index_or_status, position_out, endpoint);

    if (bone_index_or_status < 0) {
        return bone_index_or_status;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1B9C (src/BATTLE/BATTLE.PRG/38C1C.c:328)
int Actors_ActorLowGroupBonePosition(int actor_id, int limb_index, SVECTOR* position_out, int endpoint)
{
    int bone_index_or_status = Actors_FindActorBoneTag(actor_id, limb_index, 2);

    if (bone_index_or_status < 0) {
        return bone_index_or_status;
    }

    bone_index_or_status = Actors_ActorBoneEndpointPosition(actor_id, bone_index_or_status, position_out, endpoint);

    if (bone_index_or_status < 0) {
        return bone_index_or_status;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1C10 (src/BATTLE/BATTLE.PRG/38C1C.c:345)
int Actors_ActorBoneEndpointPosition(int actorId, int bone, SVECTOR* result, int endpoint)
{
    SVECTOR local;
    VECTOR transformed;
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];
    Actors_ShpModel* model;
    MATRIX* matrix;

    if (actor == NULL) {
        actor = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actorId);

        if (actor == NULL) {
            return -1;
        }

    }

    model = actor->instance.model;
    matrix = Actors_ActorBoneWorldTransform(actorId, bone, &Actors_boneMatrixScratch);

    if (endpoint == 0) {
        setVector(result, matrix->t[0], matrix->t[1], matrix->t[2]);
        return 0;
    }

    local.vy = 0;
    local.vz = 0;

    if ((bone & 0xF0) == 0x40) {
        Actors_WeaponModel* weapon = Actors_weaponModels[actorId * 2];

        bone -= 0x3F;

        if (weapon == NULL) {
            return -1;
        }

        model = (Actors_ShpModel*)weapon->sectionOffsets;
    }

    if (endpoint == 1) {
        local.vx = -model->bones[bone].length;
    } else {
        local.vx = -model->bones[bone].length / 2;
    }

    Core_Gte_SetRotMatrix(matrix);
    Core_Gte_SetTransMatrix(matrix);
    Core_Gte_LoadV0(&local);
    Core_Gte_Rtv0Tr();
    Core_Gte_StoreLvnl(&transformed);
    setVector(result, transformed.vx, transformed.vy, transformed.vz);
    return 0;
}

// decomp: BATTLE.PRG func_800A1DE8 (src/BATTLE/BATTLE.PRG/38C1C.c:400)
MATRIX* Actors_ActorBoneWorldTransform(int actorId, int bone, MATRIX* unused)
{
    MATRIX zeroTranslation;
    long flag;
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];

    if (actor == NULL) {
        Actors_ActorRecord* object = (Actors_ActorRecord*)Field_FindGimmickForActorSlot(actorId);

        if (object == NULL) {
            return NULL;
        }

        scratch->matrix = object->boneMatrices[bone];
    } else if ((bone & 0xF0) == 0x40) {
        Actors_WeaponModel* weapon = Actors_weaponModels[actorId * 2];

        if (weapon == NULL) {
            return NULL;
        }

        scratch->matrix = *(weapon->boneWorldMatrices + bone - 0x3F);
    } else if (bone == 0xFF) {
        setVector(&scratch->rootOffset, actor->blendPose.boneKeys[41].vx >> 1,
            actor->blendPose.boneKeys[41].vy >> 1, actor->blendPose.boneKeys[41].vz >> 1);
        setVector(&scratch->rootOffset, scratch->rootOffset.vx * actor->scale / ONE,
            scratch->rootOffset.vy * actor->scale / ONE,
            scratch->rootOffset.vz * actor->scale / ONE);
        Actors_TransformActorRootOffset(actor);
        scratch->matrix.t[0] = scratch->rootOffset.vx;
        scratch->matrix.t[1] = scratch->rootOffset.vy;
        scratch->matrix.t[2] = scratch->rootOffset.vz;
        return &scratch->matrix;
    } else {
        scratch->matrix = actor->boneMatrices[bone];
    }

    Core_Math_MultiplyMatrixIntoRight(&Actors_boneMatrixScratch, &scratch->matrix);
    Core_Gte_SetRotMatrix(&Actors_boneMatrixScratch);
    zeroTranslation.t[0] = 0;
    zeroTranslation.t[1] = 0;
    zeroTranslation.t[2] = 0;
    Core_Gte_SetTransMatrix(&zeroTranslation);
    setVector(&scratch->position, scratch->matrix.t[0] - Actors_boneMatrixScratch.t[0],
        scratch->matrix.t[1] - Actors_boneMatrixScratch.t[1], scratch->matrix.t[2] - Actors_boneMatrixScratch.t[2]);
    Core_Gte_RotTrans(&scratch->position, (VECTOR*)scratch->matrix.t, &flag);
    return &scratch->matrix;
}

// decomp: BATTLE.PRG func_800A4828 (src/BATTLE/BATTLE.PRG/3A1A0.c:1036)
void Actors_BuildActorPoseAtMatrix(int actor_id, MATRIX* camera_matrix)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    int visible = actor->instance.isVisible;
    actor->instance.isVisible = 0;
    actor->framesPerStep = 1;
    Render_PrepareActorCameraMatrices(camera_matrix);
    Actors_PrepareActorRootTransform(actor);
    Actors_DrawActorModel(actor, camera_matrix, 0);
    actor->instance.isVisible = visible;
}

