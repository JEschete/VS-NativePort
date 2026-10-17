// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Vector.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailOrigin (decomp src/EFFECT/trails5.h:27). Included once per instance by Effects/Templates/TrailOrigin.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_ORIGIN_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_TrailQuadSet* arg3)
{
    int sp10;
    int sp14;
    MATRIX* matrix;
    SVECTOR* splinePoint;
    int temp_a1;
    int temp_lo;
    int temp_s0_2;
    int temp_s3;
    int temp_v0;
    int temp_v0_4;
    int i;
    u_short var_a0;
    u_short var_v1;
    u_short var_v1_2;

    Core_Math_InterpolatePairsToWideVector(arg0->positionRange,
        Effects_SampleEffectCurve(arg0->positionCurve, arg3->curveSampleDistance), &arg1->position);

    matrix = &arg2->transforms[arg0->transformIndex].world;

    // Determines trail origin:
    switch (arg1->flags & 7) {
    case 0:
        break;
    // Fixed point D_800F5310
    case 1:
        Core_Math_AddWideAndShortVectors(&arg1->position, Effects_effectOrigin, &arg1->position);
        break;
    // Caster
    case 2:
        applyVector(&arg1->position, matrix->t[0], matrix->t[1], matrix->t[2], +=);
        break;
    }

    Core_Math_InterpolatePairsToWideVector(arg0->extentRange,
        Effects_SampleEffectCurve(arg0->extentCurve, arg3->curveSampleDistance), &arg1->extent);
    Core_Gte_SetRotMatrix(matrix);
    Core_Gte_ClearTranslation();

    switch (arg1->flags & 0x38) {
    case 0:
        for (i = 0; i < arg3->trailcount; ++i) {

            splinePoint = &arg3->trails[i].splinePoints[0];

            temp_v0 = Core_Random_Rand();
            arg1->sinAngle0 = Core_Gte_Sin(temp_v0);
            arg1->cosAngle0 = Core_Gte_Cos(temp_v0);

            temp_v0 = Core_Random_Rand();
            arg1->sinAngle1 = Core_Gte_Sin(temp_v0);
            arg1->cosAngle1 = Core_Gte_Cos(temp_v0);

            setVector(splinePoint,
                ((((arg1->extent.vx * arg1->cosAngle0) >> 0xC) * arg1->cosAngle1) >> 0xC),
                ((((arg1->extent.vy * arg1->cosAngle0) >> 0xC) * arg1->sinAngle1) >> 0xC),
                ((arg1->extent.vz * arg1->sinAngle0) >> 0xC));

            if (arg1->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(splinePoint, &arg1->aimPoint);
                copyVector(splinePoint, &arg1->aimPoint);
            }

            addVector(splinePoint, &arg1->position);
        }
        return;

    case 8:
        for (i = 0; i < arg3->trailcount; ++i) {

            splinePoint = &arg3->trails[i].splinePoints[0];

            switch (arg1->flags & 0x1800) {
            case 0x0:
            case 0x800:
                switch (Core_Random_Rand() % 6) {
                case 0:
                    splinePoint->vx = arg1->extent.vx;
                    if (0) {
                    case 1:
                        splinePoint->vx = -arg1->extent.vx;
                    }

                    splinePoint->vy =
                        Core_Random_RandomRangeExclusive(arg1->extent.vy, -arg1->extent.vy);
                    splinePoint->vz =
                        Core_Random_RandomRangeExclusive(arg1->extent.vz, -arg1->extent.vz);
                    break;

                case 2:
                    splinePoint->vy = arg1->extent.vy;
                    if (0) {
                    case 3:
                        splinePoint->vy = -arg1->extent.vy;
                    }

                    splinePoint->vx =
                        Core_Random_RandomRangeExclusive(arg1->extent.vx, -arg1->extent.vx);
                    splinePoint->vz =
                        Core_Random_RandomRangeExclusive(arg1->extent.vz, -arg1->extent.vz);
                    break;

                case 4:
                    splinePoint->vz = arg1->extent.vz;
                    if (0) {
                    case 5:
                        splinePoint->vz = -arg1->extent.vz;
                    }

                    splinePoint->vx =
                        Core_Random_RandomRangeExclusive(arg1->extent.vx, -arg1->extent.vx);
                    splinePoint->vy =
                        Core_Random_RandomRangeExclusive(arg1->extent.vy, -arg1->extent.vy);
                    break;
                }

                break;

            case 0x1000:
                temp_a1 = VS_TRAIL_CORNER_STATE + 1;
                VS_TRAIL_CORNER_STATE = temp_a1 & 7;

                var_a0 = arg1->extent.vx;
                if (!(temp_a1 & 1)) {
                    var_a0 = -var_a0;
                }

                splinePoint->vx = var_a0;

                var_v1 = arg1->extent.vy;
                if (!(temp_a1 & 2)) {
                    var_v1 = -var_v1;
                }
                splinePoint->vy = var_v1;

                var_v1_2 = arg1->extent.vz;
                if (!(temp_a1 & 4)) {
                    var_v1_2 = -var_v1_2;
                }
                splinePoint->vz = var_v1_2;

                break;
            }

            if (arg1->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(splinePoint, &arg1->aimPoint);
                copyVector(splinePoint, &arg1->aimPoint);
            }

            addVector(splinePoint, &arg1->position);
        }

        return;

    case 16:
    case 24:
    case 32:
    case 40:
        arg1->ringSegments = 4;
        arg1->ringPositions = 4;
        break;

    case 48:
        arg1->ringSegments = arg0->ringSegments;
        if (arg1->ringSegments == 0) {
            arg1->ringSegments = 1;
        }
        arg1->ringPositions = arg0->ringPositions;
        if (arg1->ringPositions == 0) {
            arg1->ringPositions = arg1->ringSegments;
        }
        break;

    default:
        return;
    }

    temp_lo = ONE / arg1->ringSegments;

    if ((arg1->flags & 0x1800) != 0x1000) {
        VS_TRAIL_CORNER_STATE = VS_TRAIL_CORNER_STATE % arg1->ringPositions;
        temp_s0_2 = (VS_TRAIL_CORNER_STATE * temp_lo) + ONE / 8;
        sp10 = (Core_Gte_Cos(temp_s0_2) * arg1->extent.vx) >> 0xC;
        sp14 = (Core_Gte_Sin(temp_s0_2) * arg1->extent.vz) >> 0xC;
    }

    for (i = 0; i < arg3->trailcount; ++i) {
        splinePoint = &arg3->trails[i].splinePoints[0];
        VS_TRAIL_CORNER_STATE = (VS_TRAIL_CORNER_STATE + 1) % arg1->ringPositions;
        temp_s0_2 = (VS_TRAIL_CORNER_STATE * temp_lo) + ONE / 8;
        temp_s3 = (Core_Gte_Cos(temp_s0_2) * arg1->extent.vx) >> 0xC;
        temp_s0_2 = (Core_Gte_Sin(temp_s0_2) * arg1->extent.vz) >> 0xC;

        if ((arg1->flags & 0x1800) == 0x1000) {
            splinePoint->vx = temp_s3;
            splinePoint->vz = temp_s0_2;
        } else {
            temp_v0_4 = Core_Random_Rand() >> 3;
            splinePoint->vx = (((temp_s3 - sp10) * temp_v0_4) >> 0xC) + sp10;
            splinePoint->vz = (((temp_s0_2 - sp14) * temp_v0_4) >> 0xC) + sp14;
        }

        splinePoint->vy = 0;

        if (arg1->flags & 0x40000) {
            Core_Gte_ApplyRotMatrix(splinePoint, &arg1->aimPoint);
            copyVector(splinePoint, &arg1->aimPoint);
        }

        addVector(splinePoint, &arg1->position);
    }
}
#undef TRAIL_SAMPLE_COUNT

