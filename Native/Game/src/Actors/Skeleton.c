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
int Actors_ActorPitchToPoint(Actors_ActorRecord* arg0, SVECTOR* arg1)
{
    SVECTOR sp10;
    int var_s0;
    int var_v1;

    if (arg1 == NULL) {
        return 0;
    }

    var_s0 = arg1->vx - arg0->instance.position.vx;
    var_s0 = var_s0 * var_s0;
    var_v1 = arg1->vz - arg0->instance.position.vz;

    if (var_s0 < 0) {
        var_s0 = -var_s0;
    }

    var_v1 = var_v1 * var_v1;

    if (var_v1 < 0) {
        var_v1 = -var_v1;
    }

    var_s0 = Core_Math_SquareRootInteger(var_s0 + var_v1);
    Actors_ActorBonePosition(arg0->instance.slot, 0, &sp10, 0);
    return Core_Gte_Atan2(arg1->vy - sp10.vy, var_s0);
}

// decomp: BATTLE.PRG func_8009FC60 (src/BATTLE/BATTLE.PRG/30DB0.c:3512)
int Actors_SetActorHeadTracking(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0 == NULL) {
        return -1;
    }

    if (Actors_FindActorBoneTag(arg0, 2, 3) < 0) {
        return -2;
    }

    temp_s0->headTrackTarget = arg1;
    if (arg2 == NULL) {
        temp_s0->headTracking.vx = temp_s0->instance.lookPitch;
        temp_s0->headTracking.vy = temp_s0->instance.lookYaw;
    } else {
        temp_s0->headTracking = *arg2;
    }
    if (arg3 == -1) {
        Actors_ResetActorHeadTrackingSpeed(temp_s0);
    } else {
        temp_s0->headTracking.pad = arg3;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009FD38 (src/BATTLE/BATTLE.PRG/30DB0.c:3540)
void Actors_ResetActorHeadTrackingSpeed(Actors_ActorRecord* arg0)
{
    arg0->headTracking.pad = 30;
    if (arg0->collisionRadius > 128) {
        arg0->headTracking.pad = 20;
    }
}

// decomp: BATTLE.PRG func_800A1178 (src/BATTLE/BATTLE.PRG/30DB0.c:4200)
int Actors_ActorTaggedBoneMatrix(int arg0, int arg1, MATRIX* arg2, u_long* arg3)
{
    int temp_v0 = Actors_FindActorBoneTag(arg0, arg1, 0);
    if (temp_v0 < 0) {
        return temp_v0;
    }
    Actors_ActorBoneMatrix(arg0, temp_v0, arg2, arg3);
    return 0;
}

// decomp: BATTLE.PRG func_800A11D8 (src/BATTLE/BATTLE.PRG/30DB0.c:4210)
void Actors_ActorBoneMatrix(int arg0, int arg1, MATRIX* arg2, u_long* arg3)
{
    Actors_ActorRecord* var_t0 = Actors_actorRecords[arg0];

    if (var_t0 == NULL) {
        var_t0 = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
        if (var_t0 == NULL) {
            return;
        }
    }
    *arg2 = var_t0->boneMatrices[arg1];
    *arg3 = var_t0->instance.model->bones[arg1].length;
}

// decomp: BATTLE.PRG func_800A1280 (src/BATTLE/BATTLE.PRG/30DB0.c:4224)
int Actors_ProjectActorTaggedBone(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    long sp10;
    long sp14;
    long sp18;
    int var_v0;
    int v1;

    if (arg1 == 0xFB) {
        short* new_var;
        unsigned int new_var2;
        Actors_ActorRecord* temp_a1 = Actors_actorRecords[arg0];

        if (temp_a1 == NULL) {
            Field_GimmickInstance* temp_v1 = Field_gimmickInstances[arg0];
            if (temp_v1 == NULL) {
                return -1;
            }
            arg2->vx = temp_v1->positionX;
            arg2->vz = temp_v1->positionZ;
            arg2->vy = temp_v1->positionY - 0xC0;
        } else {
            if (temp_a1->instance.reactionState) {
                return -1;
            }
            arg2->vx = temp_a1->instance.position.vx;
            arg2->vz = temp_a1->instance.position.vz;
            arg2->vy = temp_a1->instance.position.vy - temp_a1->targetPointHeight;
        }

        Core_Gte_SetRotMatrix(&((Render_FrameScratch*)Core_ScratchBase())->viewMatrix);
        Core_Gte_SetTransMatrix(&((Render_FrameScratch*)Core_ScratchBase())->viewMatrix);
        arg2->vz = Core_Gte_RotTransPers(arg2, &sp10, &sp14, &sp18);
        arg2->vx = sp10;
        new_var = &((short*)(&sp10))[1];
        new_var2 = *new_var;
        arg2->vy = new_var2;

        return 0;
    }

    var_v0 = Actors_FindActorBoneTag(arg0, arg1, 0);
    sp14 = var_v0;

    if (var_v0 < 0) {
        return var_v0;
    }

    v1 = ((int (*)(int arg0, int arg1, int* arg2, int arg3))Actors_ProjectActorBoneEndpoint)(arg0, var_v0, (int*)arg2, arg3);
    sp14 = v1;
    if (v1 < 0) {
        return v1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A13EC (src/BATTLE/BATTLE.PRG/30DB0.c:4280)
int Actors_ProjectActorBone(int arg0, int arg1, int* arg2, int arg3)
{
    int var_v0 = ((int (*)(int arg0, int arg1, int* arg2, int arg3))Actors_ProjectActorBoneEndpoint)(arg0, arg1, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A152C (src/BATTLE/BATTLE.PRG/38C1C.c:103)
int Actors_FindActorBoneTag(int arg0, int arg1, int arg2)
{
    Actors_ActorRecord* var_v0_2;
    Actors_ShpModel* var_v1;
    int i;
    int nBones;
    int new_var;

    var_v0_2 = Actors_actorRecords[arg0];

    if (var_v0_2 == NULL) {
        var_v0_2 = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
        if (var_v0_2 == NULL) {
            return -1;
        }
    }

    nBones = var_v0_2->instance.boneCount;
    var_v1 = var_v0_2->instance.model;

    do {
        if (arg1 == 0xFF) {
            return 0;
        }
    } while (0);

    for (i = 0; i < nBones; ++i) {
        switch (arg2) {
        case 0:
            if (var_v1->bones[i].tag == (arg1 & 0xFF)) {
                return i;
            }
            break;

        case 1:
            new_var = (var_v1->bones[i].limbIds >> 4) == ((arg1 & 0xFF) + 1);
            if (new_var) {
                return i;
            }
            break;

        case 2:
            if ((var_v1->bones[i].limbIds & 0xF) == ((arg1 & 0xFF) + 1)) {
                return i;
            }
            break;

        case 3:
            if (var_v1->bones[i].auxiliaryTag == (arg1 & 0xFF)) {
                return i;
            }
            break;
        }
    }

    return -4;
}

// decomp: BATTLE.PRG func_800A1648 (src/BATTLE/BATTLE.PRG/38C1C.c:161)
int Actors_ActorBoneTag(int arg0, int arg1, int arg2)
{
    Actors_ActorRecord* actor;
    Actors_ShpModel* model;
    u_int v;

    actor = Actors_actorRecords[arg0];
    if (actor == NULL) {
        actor = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
        if (actor == NULL) {
            return -1;
        }
    }
    model = actor->instance.model;
    switch (arg2) {
    case 0:
        v = model->bones[arg1].tag;
        break;
    case 1:
        v = model->bones[arg1].limbIds >> 4;
        if (v == 0) {
            return -3;
        }
        break;
    case 2:
        v = model->bones[arg1].limbIds & 0xF;
        if (v == 0) {
            return -3;
        }
        break;
    }
    return v;
}

// decomp: BATTLE.PRG func_800A1720 (src/BATTLE/BATTLE.PRG/38C1C.c:195)
int Actors_ActorAnchorMatrix(int arg0, int arg1, int* arg2, int* arg3)
{
    int var_v0 = arg1;

    if (arg1 == 0xFD) {
        var_v0 = 0;
    }

    var_v0 = Actors_ActorBoneWorldMatrix(arg0, var_v0, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    if (arg1 == 0xFD) {
        arg2[6] = Actors_actorRecords[arg0]->instance.position.vy;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A17BC (src/BATTLE/BATTLE.PRG/38C1C.c:216)
int Actors_ActorBoneWorldMatrix(int arg0, int arg1, void* arg2, int* arg3)
{
    Actors_ActorRecord* actor;
    Actors_ActorRecord* obj;
    Actors_WeaponModel* model;
    int _[22] __attribute__((unused));

    *(MATRIX*)arg2 = *Actors_ActorBoneWorldTransform(arg0, arg1, &Actors_boneMatrixScratch);

    actor = Actors_actorRecords[arg0];
    if (actor == NULL) {
        obj = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
        if (obj == NULL) {
            return 0;
        }
        *arg3 = obj->instance.model->bones[arg1].length;
    } else if (arg1 == 0xFF) {
        *arg3 = 0;
    } else if ((arg1 & 0xF0) == 0x40) {
        model = Actors_weaponModels[arg0 * 2];
        if (model == NULL) {
            return 0;
        }
        *arg3 = ((Actors_ShpModel*)model->sectionOffsets)->bones[arg1 - 0x3F].length;
    } else {
        *arg3 = actor->instance.model->bones[arg1].length;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246)
int Actors_ActorTaggedBonePosition(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    Actors_ActorRecord* var_a2;
    int var_v0;

    if (arg1 == 0xFB) {
        var_a2 = Actors_actorRecords[arg0];

        if (var_a2 == NULL) {
            var_a2 = (Actors_ActorRecord*)Field_gimmickInstances[arg0];
            if (var_a2 == NULL) {
                return -1;
            }

            *arg2 = var_a2->instance.position;
            arg2->vy -= 0xC0;
        } else {
            *arg2 = var_a2->instance.position;
            arg2->vy -= var_a2->targetPointHeight;
        }

        return 0;
    }

    if (arg1 == 0xFD) {
        *arg2 = Actors_actorRecords[arg0]->instance.position;
        return 0;
    }

    if ((arg1 & 0xF0) == 0x40) {
        var_v0 = arg1;
    } else if (arg1 == 0xFA) {
        var_a2 = Actors_actorRecords[arg0];
        *arg2 = var_a2->instance.position;
        var_v0 = var_a2->anchorHeight * var_a2->scale / ONE;
        arg2->vy += var_v0;
        return 0;
    } else {
        var_v0 = Actors_FindActorBoneTag(arg0, arg1, 0);

        if (var_v0 < 0) {
            return var_v0;
        }
    }

    var_v0 = Actors_ActorBoneEndpointPosition(arg0, var_v0, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1AF8 (src/BATTLE/BATTLE.PRG/38C1C.c:300)
int Actors_ActorBonePosition(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    int var_v0 = Actors_ActorBoneEndpointPosition(arg0, arg1, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1B28 (src/BATTLE/BATTLE.PRG/38C1C.c:311)
int Actors_ActorHighGroupBonePosition(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    int var_v0 = Actors_FindActorBoneTag(arg0, arg1, 1);

    if (var_v0 < 0) {
        return var_v0;
    }

    var_v0 = Actors_ActorBoneEndpointPosition(arg0, var_v0, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1B9C (src/BATTLE/BATTLE.PRG/38C1C.c:328)
int Actors_ActorLowGroupBonePosition(int arg0, int arg1, SVECTOR* arg2, int arg3)
{
    int var_v0 = Actors_FindActorBoneTag(arg0, arg1, 2);

    if (var_v0 < 0) {
        return var_v0;
    }

    var_v0 = Actors_ActorBoneEndpointPosition(arg0, var_v0, arg2, arg3);

    if (var_v0 < 0) {
        return var_v0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A1C10 (src/BATTLE/BATTLE.PRG/38C1C.c:345)
int Actors_ActorBoneEndpointPosition(int actorId, int bone, SVECTOR* result, int endpoint)
{
    SVECTOR local;
    VECTOR transformed;
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];
    Actors_ShpModel* model = actor->instance.model;
    MATRIX* matrix;

    if (actor == NULL) {
        actor = (Actors_ActorRecord*)Field_gimmickInstances[actorId];

        if (actor == NULL) {
            return -1;
        }

        model = actor->instance.model;
    }

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
        Actors_ActorRecord* object = (Actors_ActorRecord*)Field_gimmickInstances[actorId];

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
void Actors_BuildActorPoseAtMatrix(int arg0, MATRIX* arg1)
{
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];
    int visible = temp_s1->instance.isVisible;
    temp_s1->instance.isVisible = 0;
    temp_s1->framesPerStep = 1;
    Render_PrepareActorCameraMatrices(arg1);
    Actors_PrepareActorRootTransform(temp_s1);
    Actors_DrawActorModel(temp_s1, arg1, 0);
    temp_s1->instance.isVisible = visible;
}

