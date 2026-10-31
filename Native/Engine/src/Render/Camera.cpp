// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/ScalarMath.h"
#include "Render/Camera.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/UiPrimitives.h"

// decomp: D_800F4910 (D_800F4910@BATTLE/BATTLE.PRG)
MATRIX Render_actorCameraMatrix;

// decomp: vs_main_nearClip (vs_main_nearClip@SLUS_010.40)
int Render_nearClip;

// decomp: D_8005E218 (D_8005E218@SLUS_010.40)
MATRIX Render_cameraBasisMatrix;

// decomp: vs_main_projectionDistance (vs_main_projectionDistance@SLUS_010.40)
int Render_projectionDistance;

// decomp: BATTLE.PRG func_8007ACB0 (src/BATTLE/BATTLE.PRG/146C.c:8157)
void Render_BuildCameraViewMatrix(void)
{
    static const VECTOR Render_kWorldUp = { 0, ONE, 0 };
    VECTOR delta;
    VECTOR right;
    VECTOR forward;
    VECTOR up = Render_kWorldUp;
    SVECTOR negativePosition;
    MATRIX roll;
    int _[8] __attribute__((unused));
    Render_FrameScratch* scratch = &Render_frameScratch;

    setVector(&delta, (scratch->camera.position.vx - scratch->camera.lookAt.vx) / ONE,
        (scratch->camera.lookAt.vy - scratch->camera.position.vy) / ONE,
        (scratch->camera.lookAt.vz - scratch->camera.position.vz) / ONE);

    if (!(delta.vx | delta.vz)) {
        delta.vz = 1;
    }

    Core_Gte_VectorNormal(&delta, &forward);
    Core_Gte_OuterProduct0(&forward, &up, &right);
    Core_Gte_VectorNormal(&right, &right);
    Core_Gte_OuterProduct0(&right, &forward, &up);
    Core_Gte_VectorNormal(&up, &up);
    Render_cameraBasisMatrix.m[0][0] = scratch->viewMatrix.m[0][0] = -right.vx;
    Render_cameraBasisMatrix.m[0][1] = scratch->viewMatrix.m[0][1] = right.vy;
    Render_cameraBasisMatrix.m[0][2] = scratch->viewMatrix.m[0][2] = right.vz;
    Render_cameraBasisMatrix.m[1][0] = scratch->viewMatrix.m[1][0] = -up.vx;
    Render_cameraBasisMatrix.m[1][1] = scratch->viewMatrix.m[1][1] = up.vy;
    Render_cameraBasisMatrix.m[1][2] = scratch->viewMatrix.m[1][2] = up.vz;
    Render_cameraBasisMatrix.m[2][0] = scratch->viewMatrix.m[2][0] = -forward.vx;
    Render_cameraBasisMatrix.m[2][1] = scratch->viewMatrix.m[2][1] = forward.vy;
    Render_cameraBasisMatrix.m[2][2] = scratch->viewMatrix.m[2][2] = forward.vz;
    roll.m[1][1] = roll.m[0][0] = Core_Gte_Cos(scratch->camera.angles.vz);
    roll.m[1][0] = Core_Gte_Sin(scratch->camera.angles.vz);
    roll.m[0][1] = -roll.m[1][0];
    roll.m[2][2] = ONE;
    roll.m[0][2] = roll.m[1][2] = roll.m[2][0] = roll.m[2][1] = 0;
    setVector(&delta, ONE * 9 / 8, ONE, ONE);
    Core_Gte_ScaleMatrix(&roll, &delta);
    Core_Math_MultiplyMatrixIntoRight(&roll, &scratch->viewMatrix);
    setVector(&negativePosition, -scratch->camera.position.vx / ONE,
        -scratch->camera.position.vy / ONE, -scratch->camera.position.vz / ONE);
    Core_Gte_ApplyMatrix(&scratch->viewMatrix, &negativePosition, (VECTOR*)scratch->viewMatrix.t);
    delta.vz = delta.vy = delta.vx = scratch->camera.farClip;
    Core_Gte_ScaleMatrix(&scratch->viewMatrix, &delta);
    scratch->viewMatrix.t[0] = scratch->viewMatrix.t[0] * scratch->camera.farClip / ONE;
    scratch->viewMatrix.t[1] = scratch->viewMatrix.t[1] * scratch->camera.farClip / ONE;
    scratch->viewMatrix.t[2] = scratch->viewMatrix.t[2] * scratch->camera.farClip / ONE;
}

// decomp: BATTLE.PRG vs_battle_setNearClip (src/BATTLE/BATTLE.PRG/146C.c:9041)
void Render_SetCameraNearClip(int near_clip)
{
    Render_nearClip = near_clip;
    Render_SetRoomPolygonNearClip(near_clip);
}

// decomp: BATTLE.PRG vs_battle_setProjectionDistance (src/BATTLE/BATTLE.PRG/146C.c:9047)
void Render_SetCameraProjectionDistance(int projectionDistance)
{
    Render_projectionDistance = projectionDistance;
    Core_Gte_SetGeomScreen(projectionDistance);
}

// decomp: BATTLE.PRG vs_battle_setFarClip (src/BATTLE/BATTLE.PRG/146C.c:9053)
void Render_SetCameraFarClip(int farClip, int preserve_texture_lod)
{
    Render_frameScratch.camera.farClip = farClip;

    if (preserve_texture_lod == 0) {
        if (farClip < 0) {
            farClip += 7;
        }
        farClip >>= 3;
        if (farClip > 0x800) {
            farClip = 0x800;
        } else if (farClip < 0) {
            farClip = 0;
        }
        Render_SetRoomTextureLodDepth(farClip);
    }
}

// decomp: BATTLE.PRG func_800D197C (src/BATTLE/BATTLE.PRG/5BF94.c:6487)
void Render_BuildLookAtViewMatrix(VECTOR* position, VECTOR* target, MATRIX* view)
{
    static const VECTOR Core_kNegativeYAxis = { 0, -ONE, 0 };
    VECTOR up = Core_kNegativeYAxis;
    VECTOR right;
    VECTOR forward;
    VECTOR translation;
    setVector(&forward, target->vx - position->vx, target->vy - position->vy,
        target->vz - position->vz);
    Core_Gte_VectorNormal(&forward, &forward);
    Core_Gte_OuterProduct0(&up, &forward, &right);
    Core_Gte_VectorNormal(&right, &right);
    Core_Gte_OuterProduct0(&right, &forward, &up);
    Core_Gte_VectorNormal(&up, &up);
    view->m[0][0] = -right.vx;
    view->m[0][1] = -right.vy;
    view->m[0][2] = -right.vz;
    view->m[1][0] = up.vx;
    view->m[1][1] = up.vy;
    view->m[1][2] = up.vz;
    view->m[2][0] = forward.vx;
    view->m[2][1] = forward.vy;
    view->m[2][2] = forward.vz;
    Core_Gte_ApplyMatrixLV(view, position, &translation);
    view->t[0] = -translation.vx;
    view->t[1] = -translation.vy;
    view->t[2] = -translation.vz;
}

}  // extern "C"
