// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"

// decomp: _trig_table (_trig_table@SLUS_010.40)
u_short Core_kSineTable[1674]; /* contents: module image (ModuleData.cpp) */

// decomp: BATTLE.PRG func_800CFB68 (src/BATTLE/BATTLE.PRG/5BF94.c:5805)
int Core_Math_InterpolateSevenBitScalar(int start, int end, int weight_q7)
{
    return (((end - start) * weight_q7) >> 7) + start;
}

// decomp: BATTLE.PRG func_800CFC8C (src/BATTLE/BATTLE.PRG/5BF94.c:5859)
int Core_Math_InterpolateCosineScalar(int start, int end, int total_steps, int current_step)
{
    return start
         + ((int)((end - start) * (ONE - Core_Gte_Cos((int)(current_step * ONE / 2) / total_steps)))
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
int Core_Math_InterpolateShortPair(short* endpoints, int weight_q7)
{
    int start = endpoints[0];
    return (((endpoints[1] - start) * weight_q7) >> 7) + start;
}

// decomp: BATTLE.PRG vs_battle_lerpRatio (src/BATTLE/BATTLE.PRG/5BF94.c:6382)
int Core_Math_InterpolateFixedPointRatio(int start, int end, int numerator, int denominator)
{
    int scaled_delta = (end - start) * ((numerator * ONE) / denominator);
    return start + scaled_delta / ONE;
}

// decomp: BATTLE.PRG vs_battle_splineInterpolate (src/BATTLE/BATTLE.PRG/5BF94.c:6388)
void Core_Math_InterpolateLagrangeShortVector(
    SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3, int t, SVECTOR* out)
{
    int negative_phase = -t;
    int phase_minus_one = t - ONE;
    int phase_plus_one = t + ONE;

    int weight0_q12 =
        (((((negative_phase * phase_minus_one) >> 0xC) * (t - ONE * 2)) >> 0xC) * (ONE * 2 / 3)) >> 0xE;
    int weight1_q12 = (((phase_plus_one * phase_minus_one) >> 0xC) * (t - ONE * 2)) >> 0xD;
    int weight2_q12 = (((phase_plus_one * negative_phase) >> 0xC) * (t - ONE * 2)) >> 0xD;
    int weight3_q12 = (((((phase_plus_one * t) >> 0xC) * phase_minus_one) >> 0xC) * (ONE * 2 / 3)) >> 0xE;

    out->vx = ((p0->vx * weight0_q12) + (p1->vx * weight1_q12) + (p2->vx * weight2_q12)
                  + (p3->vx * weight3_q12))
           >> 0xC;
    out->vy = ((p0->vy * weight0_q12) + (p1->vy * weight1_q12) + (p2->vy * weight2_q12)
                  + (p3->vy * weight3_q12))
           >> 0xC;
    out->vz = ((p0->vz * weight0_q12) + (p1->vz * weight1_q12) + (p2->vz * weight2_q12)
                  + (p3->vz * weight3_q12))
           >> 0xC;
}

// decomp: BATTLE.PRG func_800D155C (src/BATTLE/BATTLE.PRG/5BF94.c:6412)
void Core_Math_InterpolateLagrangeScalar(int value0, int value1, int value2, int value3, int phase_q12, int* destination)
{
    int p0 = ((-phase_q12 * (phase_q12 - ONE)) >> 12) * (phase_q12 - (ONE * 2));
    int p1 = (((phase_q12 + ONE) * (phase_q12 - ONE)) >> 12) * (phase_q12 - (ONE * 2));
    int p2 = (((phase_q12 + ONE) * -phase_q12) >> 12) * (phase_q12 - (ONE * 2));
    int p3 = (((phase_q12 + ONE) * phase_q12) >> 12) * (phase_q12 - ONE);

    *destination = ((value0 * (((p0 >> 12) * (ONE * 2 / 3)) >> 14)) + (value1 * (p1 >> 13))
                + (value2 * (p2 >> 13)) + (value3 * (((p3 >> 12) * (ONE * 2 / 3)) >> 14)))
         >> 12;
}

// decomp: BATTLE.PRG vs_battle_lerp (src/BATTLE/BATTLE.PRG/5BF94.c:6424)
int Core_Math_InterpolateThirteenBitScalar(int start, int end, int weight_q13)
{
    int scaled_delta = (end - start) * weight_q13;
    return start + (scaled_delta / (ONE * 2));
}

// decomp: BATTLE.PRG func_800D169C (src/BATTLE/BATTLE.PRG/5BF94.c:6430)
void Core_Math_InterpolateThirteenBitShortVector(SVECTOR* start, SVECTOR* end, int weight_q13, SVECTOR* destination)
{
    destination->vx = Core_Math_InterpolateThirteenBitScalar(start->vx, end->vx, weight_q13);
    destination->vy = Core_Math_InterpolateThirteenBitScalar(start->vy, end->vy, weight_q13);
    destination->vz = Core_Math_InterpolateThirteenBitScalar(start->vz, end->vz, weight_q13);
}

// decomp: BATTLE.PRG func_800D1718 (src/BATTLE/BATTLE.PRG/5BF94.c:6437)
void Core_Math_InterpolateCosineShortVector(SVECTOR* start, SVECTOR* end, int total_steps, int current_step, SVECTOR* destination)
{
    Core_Math_InterpolateThirteenBitShortVector(start, end, ONE - Core_Gte_Cos((current_step * ONE / 2) / total_steps), destination);
}

// decomp: BATTLE.PRG func_800D1778 (src/BATTLE/BATTLE.PRG/5BF94.c:6442)
void Core_Math_InterpolateTimedShortVector(SVECTOR* start, SVECTOR* end, int total_steps, int current_step, SVECTOR* destination)
{
    Core_Math_InterpolateThirteenBitShortVector(start, end, (current_step * (ONE * 2)) / total_steps, destination);
}

// decomp: BATTLE.PRG func_800D17A8 (src/BATTLE/BATTLE.PRG/5BF94.c:6447)
void Core_Math_InterpolateThirteenBitWideVector(VECTOR* start, VECTOR* end, int weight_q13, VECTOR* destination)
{
    destination->vx = Core_Math_InterpolateThirteenBitScalar(start->vx, end->vx, weight_q13);
    destination->vy = Core_Math_InterpolateThirteenBitScalar(start->vy, end->vy, weight_q13);
    destination->vz = Core_Math_InterpolateThirteenBitScalar(start->vz, end->vz, weight_q13);
}

// decomp: BATTLE.PRG func_800D1824 (src/BATTLE/BATTLE.PRG/5BF94.c:6454)
void Core_Math_InterpolateCosineWideVector(VECTOR* start, VECTOR* end, int total_steps, int current_step, VECTOR* destination)
{
    Core_Math_InterpolateThirteenBitWideVector(start, end, ONE - Core_Gte_Cos((current_step * ONE / 2) / total_steps), destination);
}

// decomp: BATTLE.PRG func_800D1884 (src/BATTLE/BATTLE.PRG/5BF94.c:6459)
void Core_Math_InterpolateTimedWideVector(VECTOR* start, VECTOR* end, int total_steps, int current_step, VECTOR* destination)
{
    Core_Math_InterpolateThirteenBitWideVector(start, end, (current_step * (ONE * 2)) / total_steps, destination);
}

// decomp: BATTLE.PRG func_800D18B4 (src/BATTLE/BATTLE.PRG/5BF94.c:6464)
int Core_Math_InterpolateCosineTimedScalar(int start, int end, int total_steps, int current_step)
{
    return Core_Math_InterpolateThirteenBitScalar(start, end, ONE - Core_Gte_Cos((current_step * ONE / 2) / total_steps));
}

// decomp: BATTLE.PRG func_800D1904 (src/BATTLE/BATTLE.PRG/5BF94.c:6469)
int Core_Math_InterpolateTimedScalar(int start, int end, int total_steps, int current_step)
{
    return Core_Math_InterpolateThirteenBitScalar(start, end, (current_step * (ONE * 2)) / total_steps);
}

// decomp: ENDING.PRG func_800688D4 (src/ENDING/ENDING.PRG/D4.c:187)
int Core_Math_SineFromTable(int angle)
{
    intptr_t value = (intptr_t)Core_kSineTable;
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

}  // extern "C"
