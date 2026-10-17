// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleTarget (decomp src/EFFECT/particleSetup.h:262). Included once per instance by Effects/Templates/ParticleTarget.c with that plugin's bindings.
#ifndef VS_PARTICLE_SETUP_TYPES
#define VS_PARTICLE_SETUP_TYPES
#endif

void VS_PARTICLE_TARGET_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_ParticleQuadSet* arg3)
{
    int i;

    Core_Math_InterpolatePairsToShortVector(
        arg0->vectorRanges[6], Effects_SampleEffectCurve(arg0->attractionOffsetCurve, arg3->age), &arg1->attractionOffset);
    Core_Math_InterpolatePairsToShortVector(
        arg0->vectorRanges[7], Effects_SampleEffectCurve(arg0->attractionJitterCurve, arg3->age), &arg1->attractionJitter);

    switch (arg1->flags & 0x1C0) {
    case 0xC0:
        arg1->attractionOffset.vx += arg2->transforms[arg0->attractionTransform].world.t[0];
        arg1->attractionOffset.vy += arg2->transforms[arg0->attractionTransform].world.t[1];
        arg1->attractionOffset.vz += arg2->transforms[arg0->attractionTransform].world.t[2];
        break;
    case 0x80:
        arg1->attractionOffset.vx += Effects_actionRequest.position.vx;
        arg1->attractionOffset.vy += Effects_actionRequest.position.vy;
        arg1->attractionOffset.vz += Effects_actionRequest.position.vz;
        break;
    case 0:
        break;
    }

    for (i = 0; i < arg3->count; ++i) {
        SVECTOR* temp_s0 = &arg3->controlPoints[i * 4 + 3];
        temp_s0->vx =
            arg1->attractionOffset.vx + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vx, -arg1->attractionJitter.vx);
        temp_s0->vy =
            arg1->attractionOffset.vy + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vy, -arg1->attractionJitter.vy);
        temp_s0->vz =
            arg1->attractionOffset.vz + Core_Random_RandomRangeExclusive(arg1->attractionJitter.vz, -arg1->attractionJitter.vz);
    }
}
#undef VS_PARTICLE_SETUP_TYPES

