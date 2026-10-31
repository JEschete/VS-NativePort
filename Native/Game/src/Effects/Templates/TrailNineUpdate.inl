// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailNineUpdate (decomp src/EFFECT/trails9.h:414). Included once per instance by Effects/Templates/TrailNineUpdate.cpp with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 9

void VS_TRAIL_UPDATE_FUNCTION(Effects_ParticleDefinition* particle_definition __attribute__((unused)),
    Effects_ParticleEmitterScratch* emitter_scratch __attribute__((unused)),
    Effects_EffectInstance* effect_instance __attribute__((unused)), Effects_Trail9State* trail_set)
{
    int i;

    for (i = 0; i < trail_set->trailCount; ++i) {

        Effects_Trail9* trail = &trail_set->trails[i];

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
            trail->currentSplineSample = (trail->currentSplineSample + 1) % 9;

            Core_Math_InterpolateLagrangeShortVector(&trail->splinePoints[0],
                &trail->splinePoints[1], &trail->splinePoints[2],
                &trail->splinePoints[3], (trail->splineProgress >> 0xC) - ONE,
                &trail->splineSamples[trail->currentSplineSample]);

            if (trail->splineSampleCount < 9) {
                ++trail->splineSampleCount;
            }
        }
    }
}
#undef TRAIL_SAMPLE_COUNT

}  // extern "C"
