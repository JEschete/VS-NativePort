// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailUpdate (decomp src/EFFECT/trails5.h:407). Included once per instance by Effects/Templates/TrailUpdate.cpp with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_UPDATE_FUNCTION(Effects_ParticleDefinition* particle_definition __attribute__((unused)),
    Effects_ParticleEmitterScratch* emitter_scratch __attribute__((unused)),
    Effects_EffectInstance* effect_instance __attribute__((unused)), Effects_TrailQuadSet* trail_set)
{
    int i;

    for (i = 0; i < trail_set->trailcount; ++i) {

        Effects_Trail* trail = &trail_set->trails[i];

        if (trail->trailCollapseCounter == 0) {
            continue;
        }

        if (trail_set->curveSampleDistance >= trail->stepCount) {
            --trail->trailCollapseCounter;
            if (trail->trailCollapseCounter < trail->stepCount) {
                --trail->splineSampleCount;
            }
        } else {
            int currentStep = trail->stepCount - trail_set->curveSampleDistance;
            int speed = trail->speed;

            speed += ((0x06000000 - (trail->splineProgress * 2))
                         / (currentStep * currentStep))
                   - ((speed * 2) / currentStep);
            trail->speed = speed;
            trail->splineProgress += speed;
            trail->currentSplineSample = (trail->currentSplineSample + 1) % 5;

            Core_Math_InterpolateLagrangeShortVector(&trail->splinePoints[0],
                &trail->splinePoints[1], &trail->splinePoints[2],
                &trail->splinePoints[3], (trail->splineProgress >> 0xC) - ONE,
                &trail->splineSamples[trail->currentSplineSample]);

            if (trail->splineSampleCount < 5) {
                ++trail->splineSampleCount;
            }
        }
    }
}
#undef TRAIL_SAMPLE_COUNT

}  // extern "C"
