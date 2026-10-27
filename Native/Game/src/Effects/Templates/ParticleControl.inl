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
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleControl (decomp src/EFFECT/particleSetup.h:298). Included once per instance by Effects/Templates/ParticleControl.c with that plugin's bindings.
#ifndef VS_PARTICLE_SETUP_TYPES
#define VS_PARTICLE_SETUP_TYPES
#endif

void VS_PARTICLE_CONTROL_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set)
{
    SVECTOR first_local_control;
    SVECTOR second_local_control;
    int i;
    int spline_span_length;
    int first_control_angle;
    int second_control_angle;
    int first_control_radius;
    int second_control_radius;

    for (i = 0; i < particle_set->count; ++i) {

        SVECTOR* spline_control_points = &particle_set->controlPoints[i * 4];

        emitter_scratch->aimPoint.vx = spline_control_points[3].vx - spline_control_points[0].vx;
        emitter_scratch->aimPoint.vy = spline_control_points[3].vy - spline_control_points[0].vy;
        emitter_scratch->aimPoint.vz = spline_control_points[3].vz - spline_control_points[0].vz;

        spline_span_length = Core_Math_SquareRootInteger((emitter_scratch->aimPoint.vx * emitter_scratch->aimPoint.vx)
                               + (emitter_scratch->aimPoint.vy * emitter_scratch->aimPoint.vy)
                               + (emitter_scratch->aimPoint.vz * emitter_scratch->aimPoint.vz));

        Core_Math_InterpolatePairsToPlaneVector(
            particle_definition->pairRanges[0], Effects_SampleEffectCurve(particle_definition->massCurve, particle_set->age), emitter_scratch->massRange);
        Core_Math_InterpolatePairsToPlaneVector(
            particle_definition->pairRanges[1], Effects_SampleEffectCurve(particle_definition->gravityScaleCurve, particle_set->age), emitter_scratch->gravityScaleRange);
        Core_Math_InterpolatePairsToPlaneVector(
            particle_definition->pairRanges[2], Effects_SampleEffectCurve(particle_definition->speedCurve, particle_set->age), emitter_scratch->speedRange);
        Core_Math_InterpolatePairsToWideVector(
            particle_definition->vectorRanges[1], Effects_SampleEffectCurve(particle_definition->directionJitterCurve, particle_set->age), &emitter_scratch->directionJitter);
        Core_Math_InterpolatePairsToWideVector(
            particle_definition->vectorRanges[0], Effects_SampleEffectCurve(particle_definition->directionCurve, particle_set->age), &emitter_scratch->baseDirection);
        Core_Math_InterpolatePairsToShortVector(
            particle_definition->vectorRanges[2], Effects_SampleEffectCurve(particle_definition->forceCurve, particle_set->age), &emitter_scratch->forceMin);

        if (emitter_scratch->forceMin.vx != 0) {
            int first_longitudinal_factor_256 =
                (emitter_scratch->speedRange[0] + (Core_Random_Rand() % emitter_scratch->forceMin.vx)) - (emitter_scratch->forceMin.vx / 2);
            if (&emitter_scratch->aimPoint) {
                first_local_control.vz = (spline_span_length * first_longitudinal_factor_256) / 256;
            } else {
                first_local_control.vz = (spline_span_length * first_longitudinal_factor_256) / 256;
            }
        } else {
            first_local_control.vz = (spline_span_length * emitter_scratch->speedRange[0]) / 256;
        }

        if (emitter_scratch->forceMin.vy != 0) {
            int second_longitudinal_factor_256 =
                (emitter_scratch->speedRange[1] + (Core_Random_Rand() % emitter_scratch->forceMin.vy)) - (emitter_scratch->forceMin.vy / 2);
            second_local_control.vz = spline_span_length + ((spline_span_length * second_longitudinal_factor_256) / 256);
        } else {
            second_local_control.vz = spline_span_length + ((spline_span_length * emitter_scratch->speedRange[1]) / 256);
        }

        first_control_radius = emitter_scratch->massRange[0];

        if (emitter_scratch->massRange[1] != 0) {
            first_control_radius += Core_Random_Rand() % emitter_scratch->massRange[1];
        }

        second_control_radius = emitter_scratch->gravityScaleRange[0];

        if (emitter_scratch->gravityScaleRange[1] != 0) {
            second_control_radius += Core_Random_Rand() % emitter_scratch->gravityScaleRange[1];
        }

        first_control_angle = emitter_scratch->baseDirection.vx;

        if (emitter_scratch->directionJitter.vx != 0) {
            first_control_angle += (Core_Random_Rand() % (emitter_scratch->directionJitter.vx * 2)) - emitter_scratch->directionJitter.vx;
        }

        second_control_angle = emitter_scratch->baseDirection.vy;

        if (emitter_scratch->directionJitter.vy != 0) {
            second_control_angle += (Core_Random_Rand() % (emitter_scratch->directionJitter.vy * 2)) - emitter_scratch->directionJitter.vy;
        }

        first_local_control.vx = (Core_Gte_Cos(first_control_angle) * first_control_radius) >> 0xC;
        first_local_control.vy = (Core_Gte_Sin(first_control_angle) * first_control_radius) >> 0xC;
        second_local_control.vx = (Core_Gte_Cos(second_control_angle) * second_control_radius) >> 0xC;
        second_local_control.vy = (Core_Gte_Sin(second_control_angle) * second_control_radius) >> 0xC;

        Core_Math_WidenShortVector(spline_control_points, &emitter_scratch->aimPoint);
        Core_Math_WidenShortVector(spline_control_points + 3, &emitter_scratch->spawnPoint);
        Core_Math_BuildLookAtOrientation(&emitter_scratch->aimPoint, &emitter_scratch->spawnPoint, &emitter_scratch->aimMatrix);
        Core_Gte_SetRotMatrix(&emitter_scratch->aimMatrix);
        Core_Gte_SetTransVector(&emitter_scratch->aimPoint);
        Core_Gte_LoadV0(&first_local_control);
        Core_Gte_Rtv0Tr();
        Core_Gte_StoreLvnl(&emitter_scratch->aimPoint);
        Core_Math_NarrowWideVector(&emitter_scratch->aimPoint, spline_control_points + 1);
        Core_Gte_LoadV0(&second_local_control);
        Core_Gte_Rtv0Tr();
        Core_Gte_StoreLvnl(&emitter_scratch->spawnPoint);
        Core_Math_NarrowWideVector(&emitter_scratch->spawnPoint, spline_control_points + 2);
    }
}
#undef VS_PARTICLE_SETUP_TYPES

