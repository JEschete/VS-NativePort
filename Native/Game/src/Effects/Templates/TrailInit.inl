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

void VS_TRAIL_INIT_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2 __attribute__((unused)), Effects_TrailQuadSet* arg3)
{
    int speed;
    int i;

    Core_Math_InterpolatePairsToPlaneVector(arg0->pairRanges[4], 0, arg1->lifetimeRange);

    speed = Core_Math_InterpolateShortPair(arg0->innerExtentRange, 0) << 0xC;

    for (i = 0; i < arg3->trailcount; ++i) {
        int stepCount = Core_Random_RandomRangeExclusive(arg1->lifetimeRange[0], arg1->lifetimeRange[1]);
        if (stepCount == 0) {
            stepCount = 1;
        }
        arg3->trails[i].stepCount = stepCount;
        arg3->trails[i].splineSampleCount = 1;
        arg3->trails[i].trailCollapseCounter = 4;
        arg3->trails[i].currentSplineSample = 0;
        arg3->trails[i].splineSamples[0] = arg3->trails[i].splinePoints[0];
        arg3->trails[i].splineProgress = 0;
        arg3->trails[i].speed = speed;
        arg3->trails[i].opaque54 =
            ((0x06000000 / (stepCount * stepCount)) - ((speed * 2) / stepCount));
    }
}
#undef TRAIL_SAMPLE_COUNT

