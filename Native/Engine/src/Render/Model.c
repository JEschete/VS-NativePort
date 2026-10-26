// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Skeleton.h"
#include "Core/Math/ScalarMath.h"
#include "Render/Camera.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Render.h"

// decomp: BATTLE.PRG func_800AECA0 (src/BATTLE/BATTLE.PRG/44F14.c:853)
void Render_PrepareActorCameraMatrices(MATRIX* source)
{
    MATRIX roll;
    Render_ModelScratchpad* scratch = Render_ModelWorkspace();
    Render_FrameScratch* view = (void*)((uintptr_t)Core_ScratchBase() + 0x000);
    int i;

    for (i = 0; i < 3; ++i) {
        scratch->matrix.m[0][i] = Render_cameraBasisMatrix.m[i][0];
        scratch->matrix.m[1][i] = Render_cameraBasisMatrix.m[i][1];
        scratch->matrix.m[2][i] = Render_cameraBasisMatrix.m[i][2];
    }

    roll.m[1][1] = roll.m[0][0] = Core_Gte_Cos(-view->camera.angles.vz);
    roll.m[1][0] = Core_Gte_Sin(-view->camera.angles.vz);
    roll.m[0][1] = -roll.m[1][0];
    roll.m[2][2] = ONE;
    roll.m[0][2] = roll.m[1][2] = roll.m[2][0] = roll.m[2][1] = 0;
    setVector(&scratch->scale, 3640, ONE, ONE);
    Core_Gte_ScaleMatrix(&scratch->matrix, &scratch->scale);
    Core_Math_MultiplyMatrixIntoRight(&scratch->matrix, &roll);
    scratch->matrix = roll;
    i = ONE * ONE / view->camera.farClip;
    setVector(&scratch->scale, i, i, i);
    Core_Gte_ScaleMatrix(&scratch->matrix, &scratch->scale);
    // BUG: copies from the stack slot of the pointer, not from the matrix it points to
    Render_actorCameraMatrix = *source;
    Actors_boneMatrixScratch = scratch->matrix;
    Actors_boneMatrixScratch.t[0] = source->t[0];
    Actors_boneMatrixScratch.t[1] = source->t[1];
    Actors_boneMatrixScratch.t[2] = source->t[2];
}

