// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailControl (decomp src/EFFECT/trails5.h:278). Included once per instance by Effects/Templates/TrailControl.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_CONTROL_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_TrailQuadSet* arg3)
{
    SVECTOR sp10;
    SVECTOR sp18;
    int i;
    int temp_s0;
    int var_s0;
    int var_s1;
    int var_s3;
    int var_s4;

    for (i = 0; i < arg3->trailcount; ++i) {

        SVECTOR* temp_s5 = arg3->trails[i].splinePoints;

        arg1->aimPoint.vx = temp_s5[3].vx - temp_s5[0].vx;
        arg1->aimPoint.vy = temp_s5[3].vy - temp_s5[0].vy;
        arg1->aimPoint.vz = temp_s5[3].vz - temp_s5[0].vz;

        temp_s0 = Core_Math_SquareRootInteger((arg1->aimPoint.vx * arg1->aimPoint.vx)
                               + (arg1->aimPoint.vy * arg1->aimPoint.vy)
                               + (arg1->aimPoint.vz * arg1->aimPoint.vz));

        Core_Math_InterpolatePairsToPlaneVector(arg0->pairRanges[0],
            Effects_SampleEffectCurve(arg0->massCurve, arg3->curveSampleDistance), arg1->massRange);
        Core_Math_InterpolatePairsToPlaneVector(arg0->pairRanges[1],
            Effects_SampleEffectCurve(arg0->gravityScaleCurve, arg3->curveSampleDistance), arg1->gravityScaleRange);
        Core_Math_InterpolatePairsToPlaneVector(arg0->pairRanges[2],
            Effects_SampleEffectCurve(arg0->speedCurve, arg3->curveSampleDistance), arg1->speedRange);
        Core_Math_InterpolatePairsToWideVector(arg0->vectorRanges[1],
            Effects_SampleEffectCurve(arg0->directionJitterCurve, arg3->curveSampleDistance), &arg1->directionJitter);
        Core_Math_InterpolatePairsToWideVector(arg0->vectorRanges[0],
            Effects_SampleEffectCurve(arg0->directionCurve, arg3->curveSampleDistance), &arg1->baseDirection);
        Core_Math_InterpolatePairsToShortVector(arg0->vectorRanges[2],
            Effects_SampleEffectCurve(arg0->forceCurve, arg3->curveSampleDistance), &arg1->forceMin);

        if (arg1->forceMin.vx != 0) {
            int var_v0_2 =
                (arg1->speedRange[0] + (Core_Random_Rand() % arg1->forceMin.vx)) - (arg1->forceMin.vx / 2);
            if (&arg1->aimPoint) {
                sp10.vz = (temp_s0 * var_v0_2) / 256;
            } else {
                sp10.vz = (temp_s0 * var_v0_2) / 256;
            }
        } else {
            sp10.vz = (temp_s0 * arg1->speedRange[0]) / 256;
        }

        if (arg1->forceMin.vy != 0) {
            int var_v0_3 =
                (arg1->speedRange[1] + (Core_Random_Rand() % arg1->forceMin.vy)) - (arg1->forceMin.vy / 2);
            sp18.vz = temp_s0 + ((temp_s0 * var_v0_3) / 256);
        } else {
            sp18.vz = temp_s0 + ((temp_s0 * arg1->speedRange[1]) / 256);
        }

        var_s3 = arg1->massRange[0];

        if (arg1->massRange[1] != 0) {
            var_s3 += Core_Random_Rand() % arg1->massRange[1];
        }

        var_s4 = arg1->gravityScaleRange[0];

        if (arg1->gravityScaleRange[1] != 0) {
            var_s4 += Core_Random_Rand() % arg1->gravityScaleRange[1];
        }

        var_s0 = arg1->baseDirection.vx;

        if (arg1->directionJitter.vx != 0) {
            var_s0 += (Core_Random_Rand() % (arg1->directionJitter.vx * 2)) - arg1->directionJitter.vx;
        }

        var_s1 = arg1->baseDirection.vy;

        if (arg1->directionJitter.vy != 0) {
            var_s1 += (Core_Random_Rand() % (arg1->directionJitter.vy * 2)) - arg1->directionJitter.vy;
        }

        sp10.vx = (Core_Gte_Cos(var_s0) * var_s3) >> 0xC;
        sp10.vy = (Core_Gte_Sin(var_s0) * var_s3) >> 0xC;
        sp18.vx = (Core_Gte_Cos(var_s1) * var_s4) >> 0xC;
        sp18.vy = (Core_Gte_Sin(var_s1) * var_s4) >> 0xC;

        Core_Math_WidenShortVector(temp_s5, &arg1->aimPoint);
        Core_Math_WidenShortVector(temp_s5 + 3, &arg1->spawnPoint);
        Core_Math_BuildLookAtOrientation(&arg1->aimPoint, &arg1->spawnPoint, &arg1->aimMatrix);
        Core_Gte_SetRotMatrix(&arg1->aimMatrix);
        Core_Gte_SetTransVector(&arg1->aimPoint);
        Core_Gte_LoadV0(&sp10);
        Core_Gte_Rtv0Tr();
        Core_Gte_StoreLvnl(&arg1->aimPoint);
        Core_Math_NarrowWideVector(&arg1->aimPoint, temp_s5 + 1);
        Core_Gte_LoadV0(&sp18);
        Core_Gte_Rtv0Tr();
        Core_Gte_StoreLvnl(&arg1->spawnPoint);
        Core_Math_NarrowWideVector(&arg1->spawnPoint, temp_s5 + 2);
    }
}
#undef TRAIL_SAMPLE_COUNT

