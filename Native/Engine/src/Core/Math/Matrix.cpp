// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"

// decomp: BATTLE.PRG vs_battle_lookAt (src/BATTLE/BATTLE.PRG/146C.c:8209)
void Core_Math_BuildLookAtOrientation(VECTOR* from, VECTOR* to, MATRIX* mat)
{
    static const VECTOR Core_kNegativeYAxis = { 0, -ONE, 0 };
    VECTOR direction;
    VECTOR right_axis;
    VECTOR forward_axis;
    VECTOR up_axis = Core_kNegativeYAxis;
    int _[2] __attribute__((unused));

    direction.vx = to->vx - from->vx;
    direction.vy = to->vy - from->vy;
    direction.vz = to->vz - from->vz;
    Core_Gte_VectorNormal(&direction, &forward_axis);
    Core_Gte_OuterProduct0(&forward_axis, &up_axis, &right_axis);
    Core_Gte_VectorNormal(&right_axis, &right_axis);
    Core_Gte_OuterProduct0(&right_axis, &forward_axis, &up_axis);
    Core_Gte_VectorNormal(&up_axis, &up_axis);
    mat->m[0][0] = right_axis.vx;
    mat->m[1][0] = right_axis.vy;
    mat->m[2][0] = right_axis.vz;
    mat->m[0][1] = up_axis.vx;
    mat->m[1][1] = up_axis.vy;
    mat->m[2][1] = up_axis.vz;
    mat->m[0][2] = forward_axis.vx;
    mat->m[1][2] = forward_axis.vy;
    mat->m[2][2] = forward_axis.vz;
}

// decomp: BATTLE.PRG _vecToRotMatrix (src/BATTLE/BATTLE.PRG/4A0A8.c:4179)
void Core_Math_DirectionRotationMatrix(VECTOR* direction_q12, MATRIX* rotation)
{
    SVECTOR angles;
    SVECTOR integer_direction;

    integer_direction.vx = direction_q12->vx >> 0xC;
    integer_direction.vy = direction_q12->vy >> 0xC;
    integer_direction.vz = direction_q12->vz >> 0xC;
    angles.vy = Core_Gte_Atan2(integer_direction.vx, integer_direction.vz);
    angles.vx = Core_Gte_Atan2(integer_direction.vy, Core_Math_SquareRootInteger((integer_direction.vx * integer_direction.vx) + (integer_direction.vz * integer_direction.vz)));
    angles.vz = 0;
    Core_Gte_RotMatrix(&angles, rotation);
}

// decomp: BATTLE.PRG func_800CFE7C (src/BATTLE/BATTLE.PRG/5BF94.c:5922)
void Core_Math_SetIdentityRotation(MATRIX* matrix)
{
    int* m = (int*)matrix->m;
    m[4] = ONE;
    m[2] = ONE;
    m[0] = ONE;
    m[3] = 0;
    m[1] = 0;
}

// decomp: BATTLE.PRG func_800CFE98 (src/BATTLE/BATTLE.PRG/5BF94.c:5932)
void Core_Math_SetTranslationMatrix(SVECTOR* translation, MATRIX* matrix)
{
    Core_Math_SetIdentityRotation(matrix);
    matrix->t[0] = translation->vx;
    matrix->t[1] = translation->vy;
    matrix->t[2] = translation->vz;
}

}  // extern "C"
