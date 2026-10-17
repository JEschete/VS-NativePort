// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Vector.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleOrigin (decomp src/EFFECT/particleSetup.h:36). Included once per instance by Effects/Templates/ParticleOrigin.c with that plugin's bindings.
#ifndef VS_PARTICLE_SETUP_TYPES
#define VS_PARTICLE_SETUP_TYPES
#endif

void VS_PARTICLE_ORIGIN_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_ParticleQuadSet* arg3)
{
    int sp10;
    SVECTOR* temp_s1;
    int temp_a1;
    int temp_lo;
    int temp_s0_2;
    int temp_s3;
    int temp_v0;
    int temp_v0_4;
    int var_fp;
    int i;
    u_short var_a0_2;
    u_short var_v1;
    u_short var_v1_2;
    MATRIX* temp_s0;

    Core_Math_InterpolatePairsToWideVector(
        arg0->positionRange, Effects_SampleEffectCurve(arg0->positionCurve, arg3->age), &arg1->position);
    temp_s0 = &arg2->transforms[arg0->transformIndex].world;

    switch (arg1->flags & 7) {
    case 0:
        break;
    case 1:
        Core_Math_AddWideAndShortVectors(&arg1->position, Effects_effectOrigin, &arg1->position);
        break;
    case 2:
        arg1->position.vx += temp_s0->t[0];
        arg1->position.vy += temp_s0->t[1];
        arg1->position.vz += temp_s0->t[2];
        break;
    }

    Core_Math_InterpolatePairsToWideVector(
        arg0->extentRange, Effects_SampleEffectCurve(arg0->extentCurve, arg3->age), &arg1->extent);
    Core_Gte_SetRotMatrix(temp_s0);
    Core_Gte_ClearTranslation();

    switch (arg1->flags & 0x38) {
    case 0:
        for (i = 0; i < arg3->count; ++i) {

            temp_s1 = arg3->controlPoints + (i * 4);

            temp_v0 = Core_Random_Rand();
            arg1->sinAngle0 = Core_Gte_Sin(temp_v0);
            arg1->cosAngle0 = Core_Gte_Cos(temp_v0);

            temp_v0 = Core_Random_Rand();
            arg1->sinAngle1 = Core_Gte_Sin(temp_v0);
            arg1->cosAngle1 = Core_Gte_Cos(temp_v0);

            temp_s1->vx =
                (((arg1->extent.vx * arg1->cosAngle0) >> 0xC) * arg1->cosAngle1) >> 0xC;
            temp_s1->vy =
                (((arg1->extent.vy * arg1->cosAngle0) >> 0xC) * arg1->sinAngle1) >> 0xC;
            temp_s1->vz = (arg1->extent.vz * arg1->sinAngle0) >> 0xC;

            if (arg1->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(temp_s1, &arg1->aimPoint);
                temp_s1->vx = arg1->aimPoint.vx;
                temp_s1->vy = arg1->aimPoint.vy;
                temp_s1->vz = arg1->aimPoint.vz;
            }

            temp_s1->vx += arg1->position.vx;
            temp_s1->vy += arg1->position.vy;
            temp_s1->vz += arg1->position.vz;
        }
        return;

    case 8:
        for (i = 0; i < arg3->count; ++i) {

            temp_s1 = arg3->controlPoints + i * 4;

            switch (arg1->flags & 0x1800) {
            case 0x0:
            case 0x800:
                switch (Core_Random_Rand() % 6) {
                case 0:
                    temp_s1->vx = arg1->extent.vx;
                    goto jump1;

                case 1:
                    temp_s1->vx = -arg1->extent.vx;
                jump1:
                    temp_s1->vy =
                        Core_Random_RandomRangeExclusive(arg1->extent.vy, -arg1->extent.vy);
                    temp_s1->vz =
                        Core_Random_RandomRangeExclusive(arg1->extent.vz, -arg1->extent.vz);
                    break;

                case 2:
                    temp_s1->vy = arg1->extent.vy;
                    goto jump2;

                case 3:
                    temp_s1->vy = -arg1->extent.vy;
                jump2:
                    temp_s1->vx =
                        Core_Random_RandomRangeExclusive(arg1->extent.vx, -arg1->extent.vx);
                    temp_s1->vz =
                        Core_Random_RandomRangeExclusive(arg1->extent.vz, -arg1->extent.vz);
                    break;

                case 4:
                    temp_s1->vz = arg1->extent.vz;
                    goto jump3;

                case 5:
                    temp_s1->vz = -arg1->extent.vz;
                jump3:
                    temp_s1->vx =
                        Core_Random_RandomRangeExclusive(arg1->extent.vx, -arg1->extent.vx);
                    temp_s1->vy =
                        Core_Random_RandomRangeExclusive(arg1->extent.vy, -arg1->extent.vy);
                    break;
                }
                break;

            case 0x1000:
                temp_a1 = VS_PARTICLE_CORNER_STATE + 1;
                VS_PARTICLE_CORNER_STATE = temp_a1 & 7;

                var_a0_2 = arg1->extent.vx;
                if (!(VS_PARTICLE_CORNER_STATE & 1)) {
                    var_a0_2 = -var_a0_2;
                }
                temp_s1->vx = var_a0_2;

                var_v1 = arg1->extent.vy;
                if (!(VS_PARTICLE_CORNER_STATE & 2)) {
                    var_v1 = -var_v1;
                }
                temp_s1->vy = var_v1;

                var_v1_2 = arg1->extent.vz;
                if (!(VS_PARTICLE_CORNER_STATE & 4)) {
                    var_v1_2 = -var_v1_2;
                }
                temp_s1->vz = var_v1_2;

                break;
            }

            if (arg1->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(temp_s1, &arg1->aimPoint);
                temp_s1->vx = arg1->aimPoint.vx;
                temp_s1->vy = arg1->aimPoint.vy;
                temp_s1->vz = arg1->aimPoint.vz;
            }

            temp_s1->vx = temp_s1->vx + arg1->position.vx;
            temp_s1->vy = temp_s1->vy + arg1->position.vy;
            temp_s1->vz = temp_s1->vz + arg1->position.vz;
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

    temp_lo = 0x1000 / arg1->ringSegments;

    if ((arg1->flags & 0x1800) != 0x1000) {
        VS_PARTICLE_CORNER_STATE %= arg1->ringPositions;
        temp_s0_2 = (VS_PARTICLE_CORNER_STATE * temp_lo) + 0x200;
        sp10 = (Core_Gte_Cos(temp_s0_2) * arg1->extent.vx) >> 0xC;
        var_fp = (Core_Gte_Sin(temp_s0_2) * arg1->extent.vz) >> 0xC;
    }

    for (i = 0; i < arg3->count; ++i) {
        temp_s1 = arg3->controlPoints + (i * 4);
        VS_PARTICLE_CORNER_STATE = (VS_PARTICLE_CORNER_STATE + 1) % arg1->ringPositions;
        temp_s0_2 = (VS_PARTICLE_CORNER_STATE * temp_lo) + 0x200;
        temp_s3 = (Core_Gte_Cos(temp_s0_2) * arg1->extent.vx) >> 0xC;
        temp_s0_2 = (Core_Gte_Sin(temp_s0_2) * arg1->extent.vz) >> 0xC;

        if ((arg1->flags & 0x1800) == 0x1000) {
            temp_s1->vx = temp_s3;
            temp_s1->vz = temp_s0_2;
        } else {
            temp_v0_4 = Core_Random_Rand() >> 3;
            temp_s1->vx = (((temp_s3 - sp10) * temp_v0_4) >> 0xC) + sp10;
            temp_s1->vz = (((temp_s0_2 - var_fp) * temp_v0_4) >> 0xC) + var_fp;
        }

        temp_s1->vy = 0;

        if (arg1->flags & 0x40000) {
            Core_Gte_ApplyRotMatrix(temp_s1, &arg1->aimPoint);
            temp_s1->vx = arg1->aimPoint.vx;
            temp_s1->vy = arg1->aimPoint.vy;
            temp_s1->vz = arg1->aimPoint.vz;
        }
        temp_s1->vx += arg1->position.vx;
        temp_s1->vy += arg1->position.vy;
        temp_s1->vz += arg1->position.vz;
    }
}
#undef VS_PARTICLE_SETUP_TYPES

