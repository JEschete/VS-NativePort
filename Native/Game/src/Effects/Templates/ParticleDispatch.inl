// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Memory/Heap.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::ParticleDispatch (decomp src/EFFECT/particleDispatch.h:3). Included once per instance by Effects/Templates/ParticleDispatch.cpp with that plugin's bindings.
int VS_PARTICLE_DISPATCH_FUNCTION(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    Effects_ParticleDefinition* particle_definition;
    int control_point_capacity;
    short max_interval_endpoint;
    Effects_ParticleEmitterScratch* emitter_scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleQuadSet* particle_set = (Effects_ParticleQuadSet *)(renderer_slot->rendererState);
    Effects_EffectInstance* effect_instance = Effects_currentInstance;
    int renderer_alive = 1;

    switch (lifecycle_command) {
    case 1:
#ifdef VS_PARTICLE_UNTEXTURED
        /* This variant uses only the state prefix, without texture animation. */
        particle_set = (Effects_ParticleQuadSet *)(Core_Memory_AllocateFromHighEnd(offsetof(Effects_ParticleQuadSet, textureAnimation)));
#else
        particle_set = (Effects_ParticleQuadSet *)(Core_Memory_AllocateFromHighEnd(sizeof(Effects_ParticleQuadSet)));
#endif
        renderer_slot->rendererState = particle_set;
        particle_set->definitionIndex = packed_parameters >> 8;
        particle_set->opaque0 = packed_parameters;
        particle_set->age = 0;
        particle_set->opaque8 = 0;
        particle_definition = &Effects_package->block5Data->definitions[particle_set->definitionIndex];

        max_interval_endpoint = particle_definition->intervalRange[1];
        if (particle_definition->intervalRange[1] < particle_definition->intervalRange[0]) {
            max_interval_endpoint = particle_definition->intervalRange[0];
        }

        control_point_capacity = max_interval_endpoint;

        if (control_point_capacity >= 9) {
            control_point_capacity = 8;
        }

        particle_set->controlPoints = (SVECTOR *)(Core_Memory_AllocateFromHighEnd(control_point_capacity << 5));
#ifndef VS_PARTICLE_UNTEXTURED
        Effects_ClearParticleTextureState((int*)(&particle_set->textureAnimation));
        Effects_InitializeParticleTextureAnimation(&particle_set->textureAnimation, particle_definition->textureIndex, particle_definition->animationId);
#endif
        break;

    case 2:
        particle_definition = &Effects_package->block5Data->definitions[particle_set->definitionIndex];
#ifdef VS_PARTICLE_UNTEXTURED
        emitter_scratch->flags = particle_definition->flags & 0x3FFFFFF;
#else
        emitter_scratch->flags = particle_definition->flags;
#endif
        particle_set->count = Core_Math_InterpolateShortPair(
            particle_definition->intervalRange, Effects_SampleEffectCurve(particle_definition->intervalCurve, particle_set->age));

        if (particle_set->count >= 9) {
            particle_set->count = 8;
        }

        if (particle_set->opaque8 <= 0) {
            VS_PARTICLE_ORIGIN_FUNCTION(particle_definition, emitter_scratch, effect_instance, particle_set);
            VS_PARTICLE_TARGET_FUNCTION(particle_definition, emitter_scratch, effect_instance, particle_set);
            particle_set->opaque8 = particle_definition->vectorRanges[5][2];
        }

        particle_set->opaque8 = (particle_set->opaque8 - 1);

        VS_PARTICLE_CONTROL_FUNCTION(particle_definition, emitter_scratch, effect_instance, particle_set);
        VS_PARTICLE_RENDER_FUNCTION(particle_definition, emitter_scratch, effect_instance, particle_set);

        ++particle_set->age;

        if (particle_set->opaque0 != 0) {
            --particle_set->opaque0;
            if (!particle_set->opaque0) {
                Core_Memory_FreeFromHighEnd(particle_set->controlPoints);
                renderer_alive = 0;
            }
        }
        break;

    case 3:
        particle_set->opaque0 = 1;
        break;

    case 4:
        Core_Memory_FreeFromHighEnd(particle_set->controlPoints);
        renderer_alive = 0;
        break;
    }

    return renderer_alive;
}

}  // extern "C"
