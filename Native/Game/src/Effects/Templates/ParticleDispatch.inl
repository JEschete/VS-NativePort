// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Memory/Heap.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleDispatch (decomp src/EFFECT/particleDispatch.h:3). Included once per instance by Effects/Templates/ParticleDispatch.c with that plugin's bindings.
int VS_PARTICLE_DISPATCH_FUNCTION(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    Effects_ParticleDefinition* temp_s2;
    int var_a0;
    short var_v0;
    Effects_ParticleEmitterScratch* a2 = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleQuadSet* temp_s1 = arg0->rendererState;
    Effects_EffectInstance* temp_s0 = Effects_currentInstance;
    int s3 = 1;

    switch (arg1) {
    case 1:
#ifdef VS_PARTICLE_UNTEXTURED
        /* This variant uses only the state prefix, without texture animation. */
        temp_s1 = Core_Memory_AllocateFromHighEnd(offsetof(Effects_ParticleQuadSet, textureAnimation));
#else
        temp_s1 = Core_Memory_AllocateFromHighEnd(sizeof(Effects_ParticleQuadSet));
#endif
        arg0->rendererState = temp_s1;
        temp_s1->definitionIndex = arg2 >> 8;
        temp_s1->opaque0 = arg2;
        temp_s1->age = 0;
        temp_s1->opaque8 = 0;
        temp_s2 = &Effects_package->block5Data->definitions[temp_s1->definitionIndex];

        var_v0 = temp_s2->intervalRange[1];
        if (temp_s2->intervalRange[1] < temp_s2->intervalRange[0]) {
            var_v0 = temp_s2->intervalRange[0];
        }

        var_a0 = var_v0;

        if (var_a0 >= 9) {
            var_a0 = 8;
        }

        temp_s1->controlPoints = Core_Memory_AllocateFromHighEnd(var_a0 << 5);
#ifndef VS_PARTICLE_UNTEXTURED
        Effects_ClearParticleTextureState((int*)(&temp_s1->textureAnimation));
        Effects_InitializeParticleTextureAnimation(&temp_s1->textureAnimation, temp_s2->textureIndex, temp_s2->animationId);
#endif
        break;

    case 2:
        temp_s2 = &Effects_package->block5Data->definitions[temp_s1->definitionIndex];
#ifdef VS_PARTICLE_UNTEXTURED
        a2->flags = temp_s2->flags & 0x3FFFFFF;
#else
        a2->flags = temp_s2->flags;
#endif
        temp_s1->count = Core_Math_InterpolateShortPair(
            temp_s2->intervalRange, Effects_SampleEffectCurve(temp_s2->intervalCurve, temp_s1->age));

        if (temp_s1->count >= 9) {
            temp_s1->count = 8;
        }

        if (temp_s1->opaque8 <= 0) {
            VS_PARTICLE_ORIGIN_FUNCTION(temp_s2, a2, temp_s0, temp_s1);
            VS_PARTICLE_TARGET_FUNCTION(temp_s2, a2, temp_s0, temp_s1);
            temp_s1->opaque8 = temp_s2->vectorRanges[5][2];
        }

        temp_s1->opaque8 = (temp_s1->opaque8 - 1);

        VS_PARTICLE_CONTROL_FUNCTION(temp_s2, a2, temp_s0, temp_s1);
        VS_PARTICLE_RENDER_FUNCTION(temp_s2, a2, temp_s0, temp_s1);

        ++temp_s1->age;

        if (temp_s1->opaque0 != 0) {
            --temp_s1->opaque0;
            if (!temp_s1->opaque0) {
                Core_Memory_FreeFromHighEnd(temp_s1->controlPoints);
                s3 = 0;
            }
        }
        break;

    case 3:
        temp_s1->opaque0 = 1;
        break;

    case 4:
        Core_Memory_FreeFromHighEnd(temp_s1->controlPoints);
        s3 = 0;
        break;
    }

    return s3;
}

