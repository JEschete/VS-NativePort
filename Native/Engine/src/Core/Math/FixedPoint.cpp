// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"

// decomp: D_80040A14 (D_80040A14@SLUS_010.40)
u_char Core_Math_kArcTangentTable[1296]; /* contents: module image (ModuleData.cpp) */

// decomp: BATTLE.PRG vs_battle_clamp (src/BATTLE/BATTLE.PRG/2842C.c:390)
short Core_Math_ClampSignedShort(short value, int bound_a, int bound_b)
{
    short lower;
    short upper;

    if ((bound_a << 0x10) < (bound_b << 0x10)) {
        lower = bound_a;
        upper = bound_b;
    } else {
        lower = bound_b;
        upper = bound_a;
    }
    if (value < lower) {
        return lower;
    }
    if (upper < value) {
        return upper;
    }
    return value;
}

// decomp: BATTLE.PRG _vectorFixedPointMult (src/BATTLE/BATTLE.PRG/4A0A8.c:4118)
VECTOR* Core_Math_ScaleFixedVector(VECTOR* destination, VECTOR* source, int scale_q12)
{
    setVector(destination, Core_Math_MultiplyFixed(source->vx, scale_q12), Core_Math_MultiplyFixed(source->vy, scale_q12),
        Core_Math_MultiplyFixed(source->vz, scale_q12));
    return destination;
}

// decomp: BATTLE.PRG _vectorMagnitude (src/BATTLE/BATTLE.PRG/4A0A8.c:4131)
int Core_Math_FixedVectorMagnitude(VECTOR* source)
{
    VECTOR components;
    int x_squared;
    int y_squared;
    int z_squared;

    Core_Math_CopyVector(&components, source);
    x_squared = (components.vx >> 0xC) * (components.vx >> 0xC);
    y_squared = (components.vy >> 0xC) * (components.vy >> 0xC);
    z_squared = (components.vz >> 0xC) * (components.vz >> 0xC);
    components.vx = components.vx >> 0xC;
    components.vy = components.vy >> 0xC;
    components.vz = components.vz >> 0xC;
    return Core_Math_SquareRootInteger(x_squared + y_squared + z_squared) * ONE;
}

// decomp: BATTLE.PRG _sVectorToFixedPointVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4155)
VECTOR* Core_Math_ShortVectorToFixed(VECTOR* destination, SVECTOR* source)
{
    destination->vx = source->vx * ONE;
    destination->vy = source->vy * ONE;
    destination->vz = source->vz * ONE;
    return destination;
}

// decomp: BATTLE.PRG _toNormalIntegerVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4193)
void Core_Math_NormalizeFixedVector(VECTOR* source, VECTOR* destination)
{
    VECTOR integer_vector;

    setVector(&integer_vector, source->vx >> 0xC, source->vy >> 0xC, source->vz >> 0xC);
    Core_Gte_VectorNormal(&integer_vector, destination);
}

// decomp: BATTLE.PRG vs_battle_toBCD (src/BATTLE/BATTLE.PRG/5BF94.c:4494)
int Core_Math_SplitDecimalDigit(int value) { return (value % 10) | ((value / 10) * 16); }

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

}  // extern "C"
