// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"

// decomp: D_80040A14 (D_80040A14@SLUS_010.40)
u_char Core_Math_kArcTangentTable[1296]; /* contents: module image (ModuleData.c) */

// decomp: BATTLE.PRG vs_battle_clamp (src/BATTLE/BATTLE.PRG/2842C.c:390)
short Core_Math_ClampSignedShort(short arg0, int arg1, int arg2)
{
    short var_t0;
    short var_v1;

    if ((arg1 << 0x10) < (arg2 << 0x10)) {
        var_t0 = arg1;
        var_v1 = arg2;
    } else {
        var_t0 = arg2;
        var_v1 = arg1;
    }
    if (arg0 < var_t0) {
        return var_t0;
    }
    if (var_v1 < arg0) {
        return var_v1;
    }
    return arg0;
}

// decomp: BATTLE.PRG _vectorFixedPointMult (src/BATTLE/BATTLE.PRG/4A0A8.c:4118)
VECTOR* Core_Math_ScaleFixedVector(VECTOR* arg0, VECTOR* arg1, int arg2)
{
    setVector(arg0, Core_Math_MultiplyFixed(arg1->vx, arg2), Core_Math_MultiplyFixed(arg1->vy, arg2),
        Core_Math_MultiplyFixed(arg1->vz, arg2));
    return arg0;
}

// decomp: BATTLE.PRG _vectorMagnitude (src/BATTLE/BATTLE.PRG/4A0A8.c:4131)
int Core_Math_FixedVectorMagnitude(VECTOR* arg0)
{
    VECTOR sp10;
    int temp_lo;
    int temp_lo_2;
    int temp_v0;

    Core_Math_CopyVector(&sp10, arg0);
    temp_lo = (sp10.vx >> 0xC) * (sp10.vx >> 0xC);
    temp_lo_2 = (sp10.vy >> 0xC) * (sp10.vy >> 0xC);
    temp_v0 = (sp10.vz >> 0xC) * (sp10.vz >> 0xC);
    sp10.vx = sp10.vx >> 0xC;
    sp10.vy = sp10.vy >> 0xC;
    sp10.vz = sp10.vz >> 0xC;
    return Core_Math_SquareRootInteger(temp_lo + temp_lo_2 + temp_v0) * ONE;
}

// decomp: BATTLE.PRG _sVectorToFixedPointVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4155)
VECTOR* Core_Math_ShortVectorToFixed(VECTOR* arg0, SVECTOR* arg1)
{
    arg0->vx = arg1->vx * ONE;
    arg0->vy = arg1->vy * ONE;
    arg0->vz = arg1->vz * ONE;
    return arg0;
}

// decomp: BATTLE.PRG _toNormalIntegerVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4193)
void Core_Math_NormalizeFixedVector(VECTOR* arg0, VECTOR* arg1)
{
    VECTOR sp10;

    setVector(&sp10, arg0->vx >> 0xC, arg0->vy >> 0xC, arg0->vz >> 0xC);
    Core_Gte_VectorNormal(&sp10, arg1);
}

// decomp: BATTLE.PRG vs_battle_toBCD (src/BATTLE/BATTLE.PRG/5BF94.c:4494)
int Core_Math_SplitDecimalDigit(int value) { return (value % 10) | ((value / 10) * 16); }

// decomp: SLUS_010.40 RotMatrixYXZ_gte (src/SLUS_010.40/31724.s:0) [asm-file: not yet ported]
int Core_Math_BuildRotationYxz()
{
    VS_UNPORTED("Core::Math::BuildRotationYxz");
    return 0;
}

// decomp: SLUS_010.40 RotMatrix_gte (src/SLUS_010.40/31724.s:0) [asm-file: not yet ported]
int Core_Math_BuildRotationXyz()
{
    VS_UNPORTED("Core::Math::BuildRotationXyz");
    return 0;
}

// decomp: SLUS_010.40 func_80041104 (src/SLUS_010.40/31724.s:0) [asm-file: not yet ported]
int Core_Math_LookupSineCosinePair()
{
    VS_UNPORTED("Core::Math::LookupSineCosinePair");
    return 0;
}

// decomp: SLUS_010.40 rcos (src/SLUS_010.40/31724.s:0) [asm-file: not yet ported]
int Core_Math_Cosine()
{
    VS_UNPORTED("Core::Math::Cosine");
    return 0;
}

// decomp: SLUS_010.40 rsin (src/SLUS_010.40/31724.s:0) [asm-file: not yet ported]
int Core_Math_Sine()
{
    VS_UNPORTED("Core::Math::Sine");
    return 0;
}

// decomp: SLUS_010.40 ratan2 (src/SLUS_010.40/31B7C.c:6)
long Core_Math_ArcTangent2(long y, long x)
{
    long c = 0;
    long ay, ax, t;

    t = y | x;
    if (t != 0) {
        ax = x < 0 ? -x : x;
        ay = y < 0 ? -y : y;
        t = 512;
        if (ax < ay) {
            c = ax << 9;
            c /= ay;
            c = Core_Math_kArcTangentTable[c];
            c = t - c;
        } else {
            c = ay << 9;
            c /= ax;
            c = Core_Math_kArcTangentTable[c];
        }
        t = 1024;
        if (ax != x) {
            c = t - c;
        }
        c <<= 1;
        if (ay != y) {
            c = -c;
        }
    }
    return c;
}

// decomp: SLUS_010.40 InvSquareRoot (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_InverseSquareRootParts()
{
    VS_UNPORTED("Core::Math::InverseSquareRootParts");
    return 0;
}

// decomp: SLUS_010.40 MulMatrix (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_MultiplyMatrixIntoLeft()
{
    VS_UNPORTED("Core::Math::MultiplyMatrixIntoLeft");
    return 0;
}

// decomp: SLUS_010.40 OuterProduct0 (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_CrossProductFixed()
{
    VS_UNPORTED("Core::Math::CrossProductFixed");
    return 0;
}

// decomp: SLUS_010.40 Square0 (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_SquareVectorComponents()
{
    VS_UNPORTED("Core::Math::SquareVectorComponents");
    return 0;
}

// decomp: SLUS_010.40 SquareRoot12 (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_SquareRootFixed()
{
    VS_UNPORTED("Core::Math::SquareRootFixed");
    return 0;
}

// decomp: SLUS_010.40 VectorNormal (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_NormalizeVector()
{
    VS_UNPORTED("Core::Math::NormalizeVector");
    return 0;
}

// decomp: SLUS_010.40 VectorNormalS (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_NormalizeVectorToShort()
{
    VS_UNPORTED("Core::Math::NormalizeVectorToShort");
    return 0;
}

// decomp: SLUS_010.40 VectorNormalSS (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_NormalizeShortVector()
{
    VS_UNPORTED("Core::Math::NormalizeShortVector");
    return 0;
}

// decomp: SLUS_010.40 func_80041AF4 (src/SLUS_010.40/32154.s:0) [asm-file: not yet ported]
int Core_Math_NormalizeVectorRegisters()
{
    VS_UNPORTED("Core::Math::NormalizeVectorRegisters");
    return 0;
}

