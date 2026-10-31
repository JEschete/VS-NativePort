// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Vector.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleOrigin (decomp src/EFFECT/particleSetup.h:36). Included once per instance by Effects/Templates/ParticleOrigin.cpp with that plugin's bindings.
#ifndef VS_PARTICLE_SETUP_TYPES
#define VS_PARTICLE_SETUP_TYPES
#endif

void VS_PARTICLE_ORIGIN_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set)
{
    int ring_edge_start_x;
    SVECTOR* origin_control_point;
    int next_corner_index;
    int ring_angle_step;
    int temp_s0_2;
    int ring_x;
    int random_angle;
    int edge_interpolation_q12;
    int ring_edge_start_z;
    int i;
    u_short corner_x_bits;
    u_short corner_y_bits;
    u_short corner_z_bits;
    MATRIX* emitter_world_matrix;

    Core_Math_InterpolatePairsToWideVector(
        particle_definition->positionRange, Effects_SampleEffectCurve(particle_definition->positionCurve, particle_set->age), &emitter_scratch->position);
    emitter_world_matrix = &effect_instance->transforms[particle_definition->transformIndex].world;

    switch (emitter_scratch->flags & 7) {
    case 0:
        break;
    case 1:
        Core_Math_AddWideAndShortVectors(&emitter_scratch->position, Effects_effectOrigin, &emitter_scratch->position);
        break;
    case 2:
        emitter_scratch->position.vx += emitter_world_matrix->t[0];
        emitter_scratch->position.vy += emitter_world_matrix->t[1];
        emitter_scratch->position.vz += emitter_world_matrix->t[2];
        break;
    }

    Core_Math_InterpolatePairsToWideVector(
        particle_definition->extentRange, Effects_SampleEffectCurve(particle_definition->extentCurve, particle_set->age), &emitter_scratch->extent);
    Core_Gte_SetRotMatrix(emitter_world_matrix);
    Core_Gte_ClearTranslation();

    switch (emitter_scratch->flags & 0x38) {
    case 0:
        for (i = 0; i < particle_set->count; ++i) {

            origin_control_point = particle_set->controlPoints + (i * 4);

            random_angle = Core_Random_Rand();
            emitter_scratch->sinAngle0 = Core_Gte_Sin(random_angle);
            emitter_scratch->cosAngle0 = Core_Gte_Cos(random_angle);

            random_angle = Core_Random_Rand();
            emitter_scratch->sinAngle1 = Core_Gte_Sin(random_angle);
            emitter_scratch->cosAngle1 = Core_Gte_Cos(random_angle);

            origin_control_point->vx =
                (((emitter_scratch->extent.vx * emitter_scratch->cosAngle0) >> 0xC) * emitter_scratch->cosAngle1) >> 0xC;
            origin_control_point->vy =
                (((emitter_scratch->extent.vy * emitter_scratch->cosAngle0) >> 0xC) * emitter_scratch->sinAngle1) >> 0xC;
            origin_control_point->vz = (emitter_scratch->extent.vz * emitter_scratch->sinAngle0) >> 0xC;

            if (emitter_scratch->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(origin_control_point, &emitter_scratch->aimPoint);
                origin_control_point->vx = emitter_scratch->aimPoint.vx;
                origin_control_point->vy = emitter_scratch->aimPoint.vy;
                origin_control_point->vz = emitter_scratch->aimPoint.vz;
            }

            origin_control_point->vx += emitter_scratch->position.vx;
            origin_control_point->vy += emitter_scratch->position.vy;
            origin_control_point->vz += emitter_scratch->position.vz;
        }
        return;

    case 8:
        for (i = 0; i < particle_set->count; ++i) {

            origin_control_point = particle_set->controlPoints + i * 4;

            switch (emitter_scratch->flags & 0x1800) {
            case 0x0:
            case 0x800:
                switch (Core_Random_Rand() % 6) {
                case 0:
                    origin_control_point->vx = emitter_scratch->extent.vx;
                    goto jump1;

                case 1:
                    origin_control_point->vx = -emitter_scratch->extent.vx;
                jump1:
                    origin_control_point->vy =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vy, -emitter_scratch->extent.vy);
                    origin_control_point->vz =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vz, -emitter_scratch->extent.vz);
                    break;

                case 2:
                    origin_control_point->vy = emitter_scratch->extent.vy;
                    goto jump2;

                case 3:
                    origin_control_point->vy = -emitter_scratch->extent.vy;
                jump2:
                    origin_control_point->vx =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vx, -emitter_scratch->extent.vx);
                    origin_control_point->vz =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vz, -emitter_scratch->extent.vz);
                    break;

                case 4:
                    origin_control_point->vz = emitter_scratch->extent.vz;
                    goto jump3;

                case 5:
                    origin_control_point->vz = -emitter_scratch->extent.vz;
                jump3:
                    origin_control_point->vx =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vx, -emitter_scratch->extent.vx);
                    origin_control_point->vy =
                        Core_Random_RandomRangeExclusive(emitter_scratch->extent.vy, -emitter_scratch->extent.vy);
                    break;
                }
                break;

            case 0x1000:
                next_corner_index = VS_PARTICLE_CORNER_STATE + 1;
                VS_PARTICLE_CORNER_STATE = next_corner_index & 7;

                corner_x_bits = emitter_scratch->extent.vx;
                if (!(VS_PARTICLE_CORNER_STATE & 1)) {
                    corner_x_bits = -corner_x_bits;
                }
                origin_control_point->vx = corner_x_bits;

                corner_y_bits = emitter_scratch->extent.vy;
                if (!(VS_PARTICLE_CORNER_STATE & 2)) {
                    corner_y_bits = -corner_y_bits;
                }
                origin_control_point->vy = corner_y_bits;

                corner_z_bits = emitter_scratch->extent.vz;
                if (!(VS_PARTICLE_CORNER_STATE & 4)) {
                    corner_z_bits = -corner_z_bits;
                }
                origin_control_point->vz = corner_z_bits;

                break;
            }

            if (emitter_scratch->flags & 0x40000) {
                Core_Gte_ApplyRotMatrix(origin_control_point, &emitter_scratch->aimPoint);
                origin_control_point->vx = emitter_scratch->aimPoint.vx;
                origin_control_point->vy = emitter_scratch->aimPoint.vy;
                origin_control_point->vz = emitter_scratch->aimPoint.vz;
            }

            origin_control_point->vx = origin_control_point->vx + emitter_scratch->position.vx;
            origin_control_point->vy = origin_control_point->vy + emitter_scratch->position.vy;
            origin_control_point->vz = origin_control_point->vz + emitter_scratch->position.vz;
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

    ring_angle_step = 0x1000 / emitter_scratch->ringSegments;

    if ((emitter_scratch->flags & 0x1800) != 0x1000) {
        VS_PARTICLE_CORNER_STATE %= emitter_scratch->ringPositions;
        temp_s0_2 = (VS_PARTICLE_CORNER_STATE * ring_angle_step) + 0x200;
        ring_edge_start_x = (Core_Gte_Cos(temp_s0_2) * emitter_scratch->extent.vx) >> 0xC;
        ring_edge_start_z = (Core_Gte_Sin(temp_s0_2) * emitter_scratch->extent.vz) >> 0xC;
    }

    for (i = 0; i < particle_set->count; ++i) {
        origin_control_point = particle_set->controlPoints + (i * 4);
        VS_PARTICLE_CORNER_STATE = (VS_PARTICLE_CORNER_STATE + 1) % emitter_scratch->ringPositions;
        temp_s0_2 = (VS_PARTICLE_CORNER_STATE * ring_angle_step) + 0x200;
        ring_x = (Core_Gte_Cos(temp_s0_2) * emitter_scratch->extent.vx) >> 0xC;
        temp_s0_2 = (Core_Gte_Sin(temp_s0_2) * emitter_scratch->extent.vz) >> 0xC;

        if ((emitter_scratch->flags & 0x1800) == 0x1000) {
            origin_control_point->vx = ring_x;
            origin_control_point->vz = temp_s0_2;
        } else {
            edge_interpolation_q12 = Core_Random_Rand() >> 3;
            origin_control_point->vx = (((ring_x - ring_edge_start_x) * edge_interpolation_q12) >> 0xC) + ring_edge_start_x;
            origin_control_point->vz = (((temp_s0_2 - ring_edge_start_z) * edge_interpolation_q12) >> 0xC) + ring_edge_start_z;
        }

        origin_control_point->vy = 0;

        if (emitter_scratch->flags & 0x40000) {
            Core_Gte_ApplyRotMatrix(origin_control_point, &emitter_scratch->aimPoint);
            origin_control_point->vx = emitter_scratch->aimPoint.vx;
            origin_control_point->vy = emitter_scratch->aimPoint.vy;
            origin_control_point->vz = emitter_scratch->aimPoint.vz;
        }
        origin_control_point->vx += emitter_scratch->position.vx;
        origin_control_point->vy += emitter_scratch->position.vy;
        origin_control_point->vz += emitter_scratch->position.vz;
    }
}
#undef VS_PARTICLE_SETUP_TYPES

}  // extern "C"
