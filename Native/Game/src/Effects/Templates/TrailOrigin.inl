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

void VS_TRAIL_ORIGIN_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_TrailQuadSet* trail_set)
{
    int ring_edge_start_x;
    int ring_edge_start_z;
    MATRIX* matrix;
    SVECTOR* splinePoint;
    int next_corner_index;
    int ring_angle_step;
    int temp_s0_2;
    int ring_x;
    int random_angle;
    int edge_interpolation_q12;
    int i;
    u_short corner_x_bits;
    u_short corner_y_bits;
    u_short corner_z_bits;

    Core_Math_InterpolatePairsToWideVector(particle_definition->positionRange,
        Effects_SampleEffectCurve(particle_definition->positionCurve, trail_set->curveSampleDistance), &emitter_scratch->position);

    matrix = &effect_instance->transforms[particle_definition->transformIndex].world;

    // Determines trail origin:
    switch (emitter_scratch->flags & 7) {
    case 0:
        break;
    // Fixed point D_800F5310
    case 1:
        Core_Math_AddWideAndShortVectors(&emitter_scratch->position, Effects_effectOrigin, &emitter_scratch->position);
        break;
    // Caster
    case 2:
        applyVector(&emitter_scratch->position, matrix->t[0], matrix->t[1], matrix->t[2], +=);
        break;
    }

    Core_Math_InterpolatePairsToWideVector(particle_definition->extentRange,
        Effects_SampleEffectCurve(particle_definition->extentCurve, trail_set->curveSampleDistance), &emitter_scratch->extent);
    Core_Gte_SetRotMatrix(matrix);
    Core_Gte_ClearTranslation();

    switch (emitter_scratch->flags & 0x38) {
    case 0:
        for (i = 0; i < trail_set->trailcount; ++i) {

            splinePoint = &trail_set->trails[i].splinePoints[0];

            random_angle = Core_Random_Rand();
            emitter_scratch->sinAngle0 = Core_Gte_Sin(random_angle);
            emitter_scratch->cosAngle0 = Core_Gte_Cos(random_angle);

            random_angle = Core_Random_Rand();
            emitter_scratch->sinAngle1 = Core_Gte_Sin(random_angle);
            emitter_scratch->cosAngle1 = Core_Gte_Cos(random_angle);

            setVector(splinePoint,
                ((((emitter_scratch->extent.vx * emitter_scratch->cosAngle0) >> 0xC) * emitter_scratch->cosAngle1) >> 0xC),
                ((((emitter_scratch->extent.vy * emitter_scratch->cosAngle0) >> 0xC) * emitter_scratch->sinAngle1) >> 0xC),
                ((emitter_scratch->extent.vz * emitter_scratch->sinAngle0) >> 0xC));

            if (emitter_scratch->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(splinePoint, &emitter_scratch->aimPoint);
                copyVector(splinePoint, &emitter_scratch->aimPoint);
            }

            addVector(splinePoint, &emitter_scratch->position);
        }
        return;

    case 8:
        for (i = 0; i < trail_set->trailcount; ++i) {

            splinePoint = &trail_set->trails[i].splinePoints[0];

            switch (emitter_scratch->flags & 0x1800) {
            case 0x0:
            case 0x800:
                switch (Core_Random_Rand() % 6) {
                case 0:
                    splinePoint->vx = emitter_scratch->extent.vx;
                    if (0) {
                    case 1:
                        splinePoint->vx = -emitter_scratch->extent.vx;
                    }

                    splinePoint->vy =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vy, -emitter_scratch->extent.vy);
                    splinePoint->vz =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vz, -emitter_scratch->extent.vz);
                    break;

                case 2:
                    splinePoint->vy = emitter_scratch->extent.vy;
                    if (0) {
                    case 3:
                        splinePoint->vy = -emitter_scratch->extent.vy;
                    }

                    splinePoint->vx =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vx, -emitter_scratch->extent.vx);
                    splinePoint->vz =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vz, -emitter_scratch->extent.vz);
                    break;

                case 4:
                    splinePoint->vz = emitter_scratch->extent.vz;
                    if (0) {
                    case 5:
                        splinePoint->vz = -emitter_scratch->extent.vz;
                    }

                    splinePoint->vx =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vx, -emitter_scratch->extent.vx);
                    splinePoint->vy =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vy, -emitter_scratch->extent.vy);
                    break;
                }

                break;

            case 0x1000:
                next_corner_index = VS_TRAIL_CORNER_STATE + 1;
                VS_TRAIL_CORNER_STATE = next_corner_index & 7;

                corner_x_bits = emitter_scratch->extent.vx;
                if (!(next_corner_index & 1)) {
                    corner_x_bits = -corner_x_bits;
                }

                splinePoint->vx = corner_x_bits;

                corner_y_bits = emitter_scratch->extent.vy;
                if (!(next_corner_index & 2)) {
                    corner_y_bits = -corner_y_bits;
                }
                splinePoint->vy = corner_y_bits;

                corner_z_bits = emitter_scratch->extent.vz;
                if (!(next_corner_index & 4)) {
                    corner_z_bits = -corner_z_bits;
                }
                splinePoint->vz = corner_z_bits;

                break;
            }

            if (emitter_scratch->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(splinePoint, &emitter_scratch->aimPoint);
                copyVector(splinePoint, &emitter_scratch->aimPoint);
            }

            addVector(splinePoint, &emitter_scratch->position);
        }

        return;

    case 16:
    case 24:
    case 32:
    case 40:
        emitter_scratch->ringSegments = 4;
        emitter_scratch->ringPositions = 4;
        break;

    case 48:
        emitter_scratch->ringSegments = particle_definition->ringSegments;
        if (emitter_scratch->ringSegments == 0) {
            emitter_scratch->ringSegments = 1;
        }
        emitter_scratch->ringPositions = particle_definition->ringPositions;
        if (emitter_scratch->ringPositions == 0) {
            emitter_scratch->ringPositions = emitter_scratch->ringSegments;
        }
        break;

    default:
        return;
    }

    ring_angle_step = ONE / emitter_scratch->ringSegments;

    if ((emitter_scratch->flags & 0x1800) != 0x1000) {
        VS_TRAIL_CORNER_STATE = VS_TRAIL_CORNER_STATE % emitter_scratch->ringPositions;
        temp_s0_2 = (VS_TRAIL_CORNER_STATE * ring_angle_step) + ONE / 8;
        ring_edge_start_x = (Core_Gte_Cos(temp_s0_2) * emitter_scratch->extent.vx) >> 0xC;
        ring_edge_start_z = (Core_Gte_Sin(temp_s0_2) * emitter_scratch->extent.vz) >> 0xC;
    }

    for (i = 0; i < trail_set->trailcount; ++i) {
        splinePoint = &trail_set->trails[i].splinePoints[0];
        VS_TRAIL_CORNER_STATE = (VS_TRAIL_CORNER_STATE + 1) % emitter_scratch->ringPositions;
        temp_s0_2 = (VS_TRAIL_CORNER_STATE * ring_angle_step) + ONE / 8;
        ring_x = (Core_Gte_Cos(temp_s0_2) * emitter_scratch->extent.vx) >> 0xC;
        temp_s0_2 = (Core_Gte_Sin(temp_s0_2) * emitter_scratch->extent.vz) >> 0xC;

        if ((emitter_scratch->flags & 0x1800) == 0x1000) {
            splinePoint->vx = ring_x;
            splinePoint->vz = temp_s0_2;
        } else {
            edge_interpolation_q12 = Core_Random_Rand() >> 3;
            splinePoint->vx = (((ring_x - ring_edge_start_x) * edge_interpolation_q12) >> 0xC) + ring_edge_start_x;
            splinePoint->vz = (((temp_s0_2 - ring_edge_start_z) * edge_interpolation_q12) >> 0xC) + ring_edge_start_z;
        }

        splinePoint->vy = 0;

        if (emitter_scratch->flags & 0x40000) {
            Core_Gte_ApplyRotMatrix(splinePoint, &emitter_scratch->aimPoint);
            copyVector(splinePoint, &emitter_scratch->aimPoint);
        }

        addVector(splinePoint, &emitter_scratch->position);
    }
}
#undef TRAIL_SAMPLE_COUNT

