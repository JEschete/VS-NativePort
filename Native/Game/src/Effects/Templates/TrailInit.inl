// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailInit (decomp src/EFFECT/trails5.h:380). Included once per instance by Effects/Templates/TrailInit.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_INIT_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance __attribute__((unused)), Effects_TrailQuadSet* trail_set)
{
    int speed;
    int i;

    Core_Math_InterpolatePairsToPlaneVector(particle_definition->pairRanges[4], 0, emitter_scratch->lifetimeRange);

    speed = Core_Math_InterpolateShortPair(particle_definition->innerExtentRange, 0) << 0xC;

    for (i = 0; i < trail_set->trailcount; ++i) {
        int stepCount = Core_Random_RandomRangeExclusive(emitter_scratch->lifetimeRange[0], emitter_scratch->lifetimeRange[1]);
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

