// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailUpdate (decomp src/EFFECT/trails5.h:407). Included once per instance by Effects/Templates/TrailUpdate.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_UPDATE_FUNCTION(Effects_ParticleDefinition* arg0 __attribute__((unused)),
    Effects_ParticleEmitterScratch* arg1 __attribute__((unused)),
    Effects_EffectInstance* arg2 __attribute__((unused)), Effects_TrailQuadSet* arg3)
{
    int i;

    for (i = 0; i < arg3->trailcount; ++i) {

        Effects_Trail* temp_s0 = &arg3->trails[i];

        if (temp_s0->trailCollapseCounter == 0) {
            continue;
        }

        if (arg3->curveSampleDistance >= temp_s0->stepCount) {
            --temp_s0->trailCollapseCounter;
            if (temp_s0->trailCollapseCounter < temp_s0->stepCount) {
                --temp_s0->splineSampleCount;
            }
        } else {
            int currentStep = temp_s0->stepCount - arg3->curveSampleDistance;
            int speed = temp_s0->speed;

            speed += ((0x06000000 - (temp_s0->splineProgress * 2))
                         / (currentStep * currentStep))
                   - ((speed * 2) / currentStep);
            temp_s0->speed = speed;
            temp_s0->splineProgress += speed;
            temp_s0->currentSplineSample = (temp_s0->currentSplineSample + 1) % 5;

            Core_Math_InterpolateLagrangeShortVector(&temp_s0->splinePoints[0],
                &temp_s0->splinePoints[1], &temp_s0->splinePoints[2],
                &temp_s0->splinePoints[3], (temp_s0->splineProgress >> 0xC) - ONE,
                &temp_s0->splineSamples[temp_s0->currentSplineSample]);

            if (temp_s0->splineSampleCount < 5) {
                ++temp_s0->splineSampleCount;
            }
        }
    }
}
#undef TRAIL_SAMPLE_COUNT

