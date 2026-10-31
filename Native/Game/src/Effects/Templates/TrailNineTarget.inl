// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailNineTarget (decomp src/EFFECT/trails9.h:243). Included once per instance by Effects/Templates/TrailNineTarget.cpp with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 9

void VS_TRAIL_TARGET_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_Trail9State* trail_set)
{
    int i;

    Core_Math_InterpolatePairsToShortVector(particle_definition->vectorRanges[6],
        Effects_SampleEffectCurve(particle_definition->attractionOffsetCurve, trail_set->curveSampleDistance), &emitter_scratch->attractionOffset);
    Core_Math_InterpolatePairsToShortVector(particle_definition->vectorRanges[7],
        Effects_SampleEffectCurve(particle_definition->attractionJitterCurve, trail_set->curveSampleDistance), &emitter_scratch->attractionJitter);

    switch (emitter_scratch->flags & 0x1C0) {
    case 0:
        break;

    case 0xC0:
        applyVector(&emitter_scratch->attractionOffset, effect_instance->transforms[particle_definition->attractionTransform].world.t[0],
            effect_instance->transforms[particle_definition->attractionTransform].world.t[1], effect_instance->transforms[particle_definition->attractionTransform].world.t[2], +=);
        break;

    case 0x80:
        applyVector(&emitter_scratch->attractionOffset, Effects_actionRequest.position.vx, Effects_actionRequest.position.vy,
            Effects_actionRequest.position.vz, +=);
        break;
    }

    for (i = 0; i < trail_set->trailCount; ++i) {
        SVECTOR* v = &trail_set->trails[i].splinePoints[3];
        setVector(v,
            emitter_scratch->attractionOffset.vx + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vx, -emitter_scratch->attractionJitter.vx),
            emitter_scratch->attractionOffset.vy + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vy, -emitter_scratch->attractionJitter.vy),
            emitter_scratch->attractionOffset.vz
                + Core_Random_RandomRangeExclusive(emitter_scratch->attractionJitter.vz, -emitter_scratch->attractionJitter.vz));
    }
}
#undef TRAIL_SAMPLE_COUNT

}  // extern "C"
