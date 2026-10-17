// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"

// decomp: _trig_table (_trig_table@SLUS_010.40)
u_short const Core_kSineTable[1674]; /* rodata: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_800CFB68 (src/BATTLE/BATTLE.PRG/5BF94.c:5805)
int Core_Math_InterpolateSevenBitScalar(int arg0, int arg1, int arg2)
{
    return (((arg1 - arg0) * arg2) >> 7) + arg0;
}

// decomp: BATTLE.PRG func_800CFC8C (src/BATTLE/BATTLE.PRG/5BF94.c:5859)
int Core_Math_InterpolateCosineScalar(int arg0, int arg1, int arg2, int arg3)
{
    return arg0
         + ((int)((arg1 - arg0) * (ONE - Core_Gte_Cos((int)(arg3 * ONE / 2) / arg2)))
             / (ONE * 2));
}

// decomp: BATTLE.PRG vs_battle_lerpVector (src/BATTLE/BATTLE.PRG/5BF94.c:5866)
void Core_Math_InterpolatePairsToWideVector(short* src, int t, VECTOR* vec)
{
    int start0 = src[0];
    int end0 = src[1];
    int start1 = src[2];
    int end1 = src[3];
    int start2 = src[4];
    int end2 = src[5];

    vec->vx = (((end0 - start0) * t) >> 7) + start0;
    vec->vy = (((end1 - start1) * t) >> 7) + start1;
    vec->vz = (((end2 - start2) * t) >> 7) + start2;
}

// decomp: BATTLE.PRG vs_battle_lerpSvector (src/BATTLE/BATTLE.PRG/5BF94.c:5880)
void Core_Math_InterpolatePairsToShortVector(short* src, int t, SVECTOR* vec)
{
    int start0 = src[0];
    int end0 = src[1];
    int start1 = src[2];
    int end1 = src[3];
    int start2 = src[4];
    int end2 = src[5];

    vec->vx = (((end0 - start0) * t) >> 7) + start0;
    vec->vy = (((end1 - start1) * t) >> 7) + start1;
    vec->vz = (((end2 - start2) * t) >> 7) + start2;
}

// decomp: BATTLE.PRG vs_battle_lerp2DVector (src/BATTLE/BATTLE.PRG/5BF94.c:5894)
void Core_Math_InterpolatePairsToPlaneVector(short* src, int t, int* vec)
{
    int start0 = src[0];
    int end0 = src[1];
    int start1 = src[2];
    int end1 = src[3];

    vec[0] = ((((end0 - start0) * t) >> 7) + start0);
    vec[1] = ((((end1 - start1) * t) >> 7) + start1);
}

// decomp: BATTLE.PRG func_800CFE1C (src/BATTLE/BATTLE.PRG/5BF94.c:5905)
int Core_Math_InterpolateShortPair(short* arg0, int arg1)
{
    int new_var = arg0[0];
    return (((arg0[1] - new_var) * arg1) >> 7) + new_var;
}

// decomp: BATTLE.PRG vs_battle_lerpRatio (src/BATTLE/BATTLE.PRG/5BF94.c:6382)
int Core_Math_InterpolateFixedPointRatio(int arg0, int arg1, int arg2, int arg3)
{
    int var_v0 = (arg1 - arg0) * ((arg2 * ONE) / arg3);
    return arg0 + var_v0 / ONE;
}

// decomp: BATTLE.PRG vs_battle_splineInterpolate (src/BATTLE/BATTLE.PRG/5BF94.c:6388)
void Core_Math_InterpolateLagrangeShortVector(
    SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3, int t, SVECTOR* out)
{
    int temp_t3 = -t;
    int temp_t2 = t - ONE;
    int temp_t1 = t + ONE;

    int temp_t0 =
        (((((temp_t3 * temp_t2) >> 0xC) * (t - ONE * 2)) >> 0xC) * (ONE * 2 / 3)) >> 0xE;
    int temp_t3_2 = (((temp_t1 * temp_t2) >> 0xC) * (t - ONE * 2)) >> 0xD;
    int temp_t2_2 = (((temp_t1 * temp_t3) >> 0xC) * (t - ONE * 2)) >> 0xD;
    int temp_v1_2 = (((((temp_t1 * t) >> 0xC) * temp_t2) >> 0xC) * (ONE * 2 / 3)) >> 0xE;

    out->vx = ((p0->vx * temp_t0) + (p1->vx * temp_t3_2) + (p2->vx * temp_t2_2)
                  + (p3->vx * temp_v1_2))
           >> 0xC;
    out->vy = ((p0->vy * temp_t0) + (p1->vy * temp_t3_2) + (p2->vy * temp_t2_2)
                  + (p3->vy * temp_v1_2))
           >> 0xC;
    out->vz = ((p0->vz * temp_t0) + (p1->vz * temp_t3_2) + (p2->vz * temp_t2_2)
                  + (p3->vz * temp_v1_2))
           >> 0xC;
}

// decomp: BATTLE.PRG func_800D155C (src/BATTLE/BATTLE.PRG/5BF94.c:6412)
void Core_Math_InterpolateLagrangeScalar(int arg0, int arg1, int arg2, int arg3, int arg4, int* arg5)
{
    int p0 = ((-arg4 * (arg4 - ONE)) >> 12) * (arg4 - (ONE * 2));
    int p1 = (((arg4 + ONE) * (arg4 - ONE)) >> 12) * (arg4 - (ONE * 2));
    int p2 = (((arg4 + ONE) * -arg4) >> 12) * (arg4 - (ONE * 2));
    int p3 = (((arg4 + ONE) * arg4) >> 12) * (arg4 - ONE);

    *arg5 = ((arg0 * (((p0 >> 12) * (ONE * 2 / 3)) >> 14)) + (arg1 * (p1 >> 13))
                + (arg2 * (p2 >> 13)) + (arg3 * (((p3 >> 12) * (ONE * 2 / 3)) >> 14)))
         >> 12;
}

// decomp: BATTLE.PRG vs_battle_lerp (src/BATTLE/BATTLE.PRG/5BF94.c:6424)
int Core_Math_InterpolateThirteenBitScalar(int arg0, int arg1, int arg2)
{
    int var_v0 = (arg1 - arg0) * arg2;
    return arg0 + (var_v0 / (ONE * 2));
}

// decomp: BATTLE.PRG func_800D169C (src/BATTLE/BATTLE.PRG/5BF94.c:6430)
void Core_Math_InterpolateThirteenBitShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, SVECTOR* arg3)
{
    arg3->vx = Core_Math_InterpolateThirteenBitScalar(arg0->vx, arg1->vx, arg2);
    arg3->vy = Core_Math_InterpolateThirteenBitScalar(arg0->vy, arg1->vy, arg2);
    arg3->vz = Core_Math_InterpolateThirteenBitScalar(arg0->vz, arg1->vz, arg2);
}

// decomp: BATTLE.PRG func_800D1718 (src/BATTLE/BATTLE.PRG/5BF94.c:6437)
void Core_Math_InterpolateCosineShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, int arg3, SVECTOR* arg4)
{
    Core_Math_InterpolateThirteenBitShortVector(arg0, arg1, ONE - Core_Gte_Cos((arg3 * ONE / 2) / arg2), arg4);
}

// decomp: BATTLE.PRG func_800D1778 (src/BATTLE/BATTLE.PRG/5BF94.c:6442)
void Core_Math_InterpolateTimedShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, int arg3, SVECTOR* arg4)
{
    Core_Math_InterpolateThirteenBitShortVector(arg0, arg1, (arg3 * (ONE * 2)) / arg2, arg4);
}

// decomp: BATTLE.PRG func_800D17A8 (src/BATTLE/BATTLE.PRG/5BF94.c:6447)
void Core_Math_InterpolateThirteenBitWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, VECTOR* arg3)
{
    arg3->vx = Core_Math_InterpolateThirteenBitScalar(arg0->vx, arg1->vx, arg2);
    arg3->vy = Core_Math_InterpolateThirteenBitScalar(arg0->vy, arg1->vy, arg2);
    arg3->vz = Core_Math_InterpolateThirteenBitScalar(arg0->vz, arg1->vz, arg2);
}

// decomp: BATTLE.PRG func_800D1824 (src/BATTLE/BATTLE.PRG/5BF94.c:6454)
void Core_Math_InterpolateCosineWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, int arg3, VECTOR* arg4)
{
    Core_Math_InterpolateThirteenBitWideVector(arg0, arg1, ONE - Core_Gte_Cos((arg3 * ONE / 2) / arg2), arg4);
}

// decomp: BATTLE.PRG func_800D1884 (src/BATTLE/BATTLE.PRG/5BF94.c:6459)
void Core_Math_InterpolateTimedWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, int arg3, VECTOR* arg4)
{
    Core_Math_InterpolateThirteenBitWideVector(arg0, arg1, (arg3 * (ONE * 2)) / arg2, arg4);
}

// decomp: BATTLE.PRG func_800D18B4 (src/BATTLE/BATTLE.PRG/5BF94.c:6464)
int Core_Math_InterpolateCosineTimedScalar(int arg0, int arg1, int arg2, int arg3)
{
    return Core_Math_InterpolateThirteenBitScalar(arg0, arg1, ONE - Core_Gte_Cos((arg3 * ONE / 2) / arg2));
}

// decomp: BATTLE.PRG func_800D1904 (src/BATTLE/BATTLE.PRG/5BF94.c:6469)
int Core_Math_InterpolateTimedScalar(int arg0, int arg1, int arg2, int arg3)
{
    return Core_Math_InterpolateThirteenBitScalar(arg0, arg1, (arg3 * (ONE * 2)) / arg2);
}

// decomp: ENDING.PRG func_800688D4 (src/ENDING/ENDING.PRG/D4.c:187)
int Core_Math_SineFromTable(int angle)
{
    int value = (int)Core_kSineTable;
    int quadrant, test, index;

    quadrant = angle & 0x400;
    test = angle & 0x3FF;
    if (quadrant) {
        if (!test) {
            value = 0x1000;
            goto negate;
        }
        angle = 0x800 - angle;
    }
    test = angle & 0x200;
    index = angle & 0x1FF;
    if (test) {
        quadrant = 0x200;
        index = quadrant - index;
        value += 2;
    }
    index <<= 2;
    value += index;
    value = *(u_short*)value;
negate:
    if (angle & 0x800) {
        value = -value;
    }
    return value;
}

// decomp: ENDING.PRG _ease (src/ENDING/ENDING.PRG/D4.c:1293)
int Core_Math_EvaluateEase(short mode, short currentStep, short totalSteps)
{
    switch (mode) {
    case 0:
        return (currentStep * ONE) / totalSteps;
    case 1:
        return Core_Gte_Sin((currentStep * (ONE / 4)) / totalSteps);
    case 2:
        return (Core_Gte_Cos(((currentStep * (ONE / 2)) / totalSteps) + (ONE / 2)) + ONE) >> 1;
    case 3:
        return Core_Gte_Sin(((currentStep * (ONE / 4)) / totalSteps) - (ONE / 4)) + ONE;
    }
}

