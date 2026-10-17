// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Matrix.h"

// decomp: BATTLE.PRG vs_battle_lookAt (src/BATTLE/BATTLE.PRG/146C.c:8209)
void Core_Math_BuildLookAtOrientation(VECTOR* from, VECTOR* to, MATRIX* mat)
{
    static const VECTOR Core_kNegativeYAxis = { 0, -ONE, 0 };
    VECTOR sp10;
    VECTOR sp20;
    VECTOR sp30;
    VECTOR sp40 = Core_kNegativeYAxis;
    int _[2] __attribute__((unused));

    sp10.vx = to->vx - from->vx;
    sp10.vy = to->vy - from->vy;
    sp10.vz = to->vz - from->vz;
    Core_Gte_VectorNormal(&sp10, &sp30);
    Core_Gte_OuterProduct0(&sp30, &sp40, &sp20);
    Core_Gte_VectorNormal(&sp20, &sp20);
    Core_Gte_OuterProduct0(&sp20, &sp30, &sp40);
    Core_Gte_VectorNormal(&sp40, &sp40);
    mat->m[0][0] = sp20.vx;
    mat->m[1][0] = sp20.vy;
    mat->m[2][0] = sp20.vz;
    mat->m[0][1] = sp40.vx;
    mat->m[1][1] = sp40.vy;
    mat->m[2][1] = sp40.vz;
    mat->m[0][2] = sp30.vx;
    mat->m[1][2] = sp30.vy;
    mat->m[2][2] = sp30.vz;
}

// decomp: BATTLE.PRG func_8009874C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Core_Math_BuildEulerRotationMatrix()
{
    VS_UNPORTED("Core::Math::BuildEulerRotationMatrix");
    return 0;
}

// decomp: BATTLE.PRG func_800989B8 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Core_Math_MultiplyRotationMatrixInPlace()
{
    VS_UNPORTED("Core::Math::MultiplyRotationMatrixInPlace");
    return 0;
}

// decomp: BATTLE.PRG func_800B16F4 (src/BATTLE/BATTLE.PRG/48C7C.s:0) [asm-file: not yet ported]
int Core_Math_LookupRotationTrigQuadrant()
{
    VS_UNPORTED("Core::Math::LookupRotationTrigQuadrant");
    return 0;
}

// decomp: BATTLE.PRG func_800B17F0 (src/BATTLE/BATTLE.PRG/48C7C.s:0) [asm-file: not yet ported]
void Core_Math_BuildEulerRotationMatrixQuadrant(SVECTOR*, MATRIX*)
{
    VS_UNPORTED("Core::Math::BuildEulerRotationMatrixQuadrant");
}

// decomp: BATTLE.PRG func_800B196C (src/BATTLE/BATTLE.PRG/48C7C.s:0) [asm-file: not yet ported]
void Core_Math_PremultiplyRotationMatrix(MATRIX*, MATRIX*)
{
    VS_UNPORTED("Core::Math::PremultiplyRotationMatrix");
}

// decomp: BATTLE.PRG _vecToRotMatrix (src/BATTLE/BATTLE.PRG/4A0A8.c:4179)
void Core_Math_DirectionRotationMatrix(VECTOR* arg0, MATRIX* arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;

    sp18.vx = arg0->vx >> 0xC;
    sp18.vy = arg0->vy >> 0xC;
    sp18.vz = arg0->vz >> 0xC;
    sp10.vy = Core_Gte_Atan2(sp18.vx, sp18.vz);
    sp10.vx = Core_Gte_Atan2(sp18.vy, Core_Math_SquareRootInteger((sp18.vx * sp18.vx) + (sp18.vz * sp18.vz)));
    sp10.vz = 0;
    Core_Gte_RotMatrix(&sp10, arg1);
}

// decomp: BATTLE.PRG func_800CFE7C (src/BATTLE/BATTLE.PRG/5BF94.c:5922)
void Core_Math_SetIdentityRotation(MATRIX* arg0)
{
    int* m = (int*)arg0->m;
    m[4] = ONE;
    m[2] = ONE;
    m[0] = ONE;
    m[3] = 0;
    m[1] = 0;
}

// decomp: BATTLE.PRG func_800CFE98 (src/BATTLE/BATTLE.PRG/5BF94.c:5932)
void Core_Math_SetTranslationMatrix(SVECTOR* arg0, MATRIX* arg1)
{
    Core_Math_SetIdentityRotation(arg1);
    arg1->t[0] = arg0->vx;
    arg1->t[1] = arg0->vy;
    arg1->t[2] = arg0->vz;
}

