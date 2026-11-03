// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C++" {
#include "Core/Math/InterpolationLive.hpp"
#include "Core/RandomLive.hpp"
}
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailInit (decomp src/EFFECT/trails5.h:380). Included once per instance by Effects/Templates/TrailInit.cpp with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_INIT_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance __attribute__((unused)), Effects_TrailQuadSet* trail_set)
{
    int speed;
    int i;

    VS::Core::Math::InterpolateLiveSevenBitPlane(particle_definition->pairRanges[4], 0, emitter_scratch->lifetimeRange);

    speed = VS::Core::Math::InterpolateLiveShortPair(particle_definition->innerExtentRange, 0) << 0xC;

    for (i = 0; i < trail_set->trailcount; ++i) {
        int stepCount = VS::Core::LiveRandomRangeExclusive(emitter_scratch->lifetimeRange[0], emitter_scratch->lifetimeRange[1]);
        if (stepCount == 0) {
            stepCount = 1;
        }
        trail_set->trails[i].stepCount = stepCount;
        trail_set->trails[i].splineSampleCount = 1;
        trail_set->trails[i].trailCollapseCounter = 4;
        trail_set->trails[i].currentSplineSample = 0;
        trail_set->trails[i].splineSamples[0] = trail_set->trails[i].splinePoints[0];
        trail_set->trails[i].splineProgress = 0;
        trail_set->trails[i].speed = speed;
        trail_set->trails[i].opaque54 =
            ((0x06000000 / (stepCount * stepCount)) - ((speed * 2) / stepCount));
    }
}
#undef TRAIL_SAMPLE_COUNT

}  // extern "C"
