// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleTarget (decomp src/EFFECT/particleSetup.h:262). Included once per instance by Effects/Templates/ParticleTarget.cpp with that plugin's bindings.
#ifndef VS_PARTICLE_SETUP_TYPES
#define VS_PARTICLE_SETUP_TYPES
#endif

void VS_PARTICLE_TARGET_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set)
{
    int i;

    Core_Math_InterpolatePairsToShortVector(
        particle_definition->vectorRanges[6], Effects_SampleEffectCurve(particle_definition->attractionOffsetCurve, particle_set->age), &emitter_scratch->attractionOffset);
    Core_Math_InterpolatePairsToShortVector(
        particle_definition->vectorRanges[7], Effects_SampleEffectCurve(particle_definition->attractionJitterCurve, particle_set->age), &emitter_scratch->attractionJitter);

    switch (emitter_scratch->flags & 0x1C0) {
    case 0xC0:
        emitter_scratch->attractionOffset.vx += effect_instance->transforms[particle_definition->attractionTransform].world.t[0];
        emitter_scratch->attractionOffset.vy += effect_instance->transforms[particle_definition->attractionTransform].world.t[1];
        emitter_scratch->attractionOffset.vz += effect_instance->transforms[particle_definition->attractionTransform].world.t[2];
        break;
    case 0x80:
        emitter_scratch->attractionOffset.vx += Effects_actionRequest.position.vx;
        emitter_scratch->attractionOffset.vy += Effects_actionRequest.position.vy;
        emitter_scratch->attractionOffset.vz += Effects_actionRequest.position.vz;
        break;
    case 0:
        break;
    }

    for (i = 0; i < particle_set->count; ++i) {
        SVECTOR* target_control_point = &particle_set->controlPoints[i * 4 + 3];
        target_control_point->vx =
            emitter_scratch->attractionOffset.vx + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vx, -emitter_scratch->attractionJitter.vx);
        target_control_point->vy =
            emitter_scratch->attractionOffset.vy + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vy, -emitter_scratch->attractionJitter.vy);
        target_control_point->vz =
            emitter_scratch->attractionOffset.vz + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vz, -emitter_scratch->attractionJitter.vz);
    }
}
#undef VS_PARTICLE_SETUP_TYPES

}  // extern "C"
