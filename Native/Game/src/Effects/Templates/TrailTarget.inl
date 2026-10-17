// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailTarget (decomp src/EFFECT/trails5.h:243). Included once per instance by Effects/Templates/TrailTarget.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_TARGET_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_TrailQuadSet* arg3)
{
    int i;

    Core_Math_InterpolatePairsToShortVector(arg0->vectorRanges[6],
        Effects_SampleEffectCurve(arg0->attractionOffsetCurve, arg3->curveSampleDistance), &arg1->attractionOffset);
    Core_Math_InterpolatePairsToShortVector(arg0->vectorRanges[7],
        Effects_SampleEffectCurve(arg0->attractionJitterCurve, arg3->curveSampleDistance), &arg1->attractionJitter);

    switch (arg1->flags & 0x1C0) {
    case 0:
        break;

    case 0xC0:
        applyVector(&arg1->attractionOffset, arg2->transforms[arg0->attractionTransform].world.t[0],
            arg2->transforms[arg0->attractionTransform].world.t[1], arg2->transforms[arg0->attractionTransform].world.t[2], +=);
        break;

    case 0x80:
        applyVector(&arg1->attractionOffset, Effects_actionRequest.position.vx, Effects_actionRequest.position.vy,
            Effects_actionRequest.position.vz, +=);
        break;
    }

    for (i = 0; i < arg3->trailcount; ++i) {
        SVECTOR* v = &arg3->trails[i].splinePoints[3];
        setVector(v,
            arg1->attractionOffset.vx + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vx, -arg1->attractionJitter.vx),
            arg1->attractionOffset.vy + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vy, -arg1->attractionJitter.vy),
            arg1->attractionOffset.vz
                + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vz, -arg1->attractionJitter.vz));
    }
}
#undef TRAIL_SAMPLE_COUNT

