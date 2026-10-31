// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailDispatch (decomp src/EFFECT/trails5.h:677). Included once per instance by Effects/Templates/TrailDispatch.cpp with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

int VS_TRAIL_DISPATCH_FUNCTION(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    Effects_PluginEffectState child_particle_context;
    Effects_ParticleDefinition* particle_definition;
    int trail;
    int j;
    Effects_TrailQuadSet* new_trail_set;

    Effects_ParticleEmitterScratch* emitter_scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_TrailQuadSet* trail_set = (Effects_TrailQuadSet *)(renderer_slot->rendererState);
    Effects_EffectInstance* effect_instance = Effects_currentInstance;
    int renderer_alive = 1;

    switch (lifecycle_command) {
    case 1:
        new_trail_set = (Effects_TrailQuadSet *)(Core_Memory_AllocateFromHighEnd(sizeof *new_trail_set + 8));
        renderer_slot->rendererState = new_trail_set;
        new_trail_set->definitionIndex = packed_parameters >> 8;
        new_trail_set->opaque0 = packed_parameters;
        new_trail_set->curveSampleDistance = 0;
        particle_definition = &Effects_package->block5Data->definitions[new_trail_set->definitionIndex];
        trail = particle_definition->intervalRange[0];

        if (trail > 16) {
            trail = 16;
        }

        new_trail_set->trails = (Effects_Trail *)(Core_Memory_AllocateFromHighEnd(trail * sizeof *new_trail_set->trails));
        new_trail_set->trailcount = trail;
        new_trail_set->opaque3 = 0;

        if (particle_definition->transparencyCurve != 0) {

            // bad var reuse, trail = length of curve
            trail = Effects_package->curveBlock->curveSizes[particle_definition->transparencyCurve];

            if (trail > 1) {
                --trail;
            }

            for (j = 0; j < 5; ++j) {
                new_trail_set->sampledTransparencyCurve[j] =
                    Effects_package->curves[particle_definition->transparencyCurve][(j * trail) / 4];
            }

        } else {
            for (j = 0; j < 5; ++j) {
                new_trail_set->sampledTransparencyCurve[j] = 0x80;
            }
        }

        Effects_ClearParticleTextureState((int*)&new_trail_set->textureAnimation);
        Effects_InitializeParticleTextureAnimation(&new_trail_set->textureAnimation, particle_definition->textureIndex, particle_definition->animationId);
        break;

    case 2:
        particle_definition = &Effects_package->block5Data->definitions[trail_set->definitionIndex];
        emitter_scratch->flags = (particle_definition->flags);

        if (trail_set->opaque3 == 0) {
            VS_TRAIL_ORIGIN_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);
            VS_TRAIL_TARGET_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);
            VS_TRAIL_CONTROL_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);
            VS_TRAIL_INIT_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);
            trail_set->opaque3 = 1U;
        }

        VS_TRAIL_UPDATE_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);
        VS_TRAIL_RENDER_FUNCTION(particle_definition, emitter_scratch, effect_instance, trail_set);

        if (particle_definition->tickEvent != 0) {
            for (trail = 0; trail < trail_set->trailcount; ++trail) {

                child_particle_context.transparency = 0;

                if (particle_definition->flags & 0x20000) {
                    Core_Gte_SetRotMatrix(&effect_instance->transforms[particle_definition->transformIndex].world);
                    Core_Gte_SetTransMatrix(&effect_instance->transforms[particle_definition->transformIndex].world);
                    emitter_scratch->vector.vx =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vx;
                    emitter_scratch->vector.vy =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vy;
                    emitter_scratch->vector.vz =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vz;

                    Core_Gte_LoadV0(&emitter_scratch->vector);
                    Core_Gte_Rtv0Tr();
                    Core_Gte_StoreLvnl(&emitter_scratch->aimPoint);

                    child_particle_context.position.vx = emitter_scratch->aimPoint.vx * ONE;
                    child_particle_context.position.vy = emitter_scratch->aimPoint.vy * ONE;
                    child_particle_context.position.vz = emitter_scratch->aimPoint.vz * ONE;
                } else {
                    child_particle_context.position.vx =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vx
                        * ONE;
                    child_particle_context.position.vy =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vy
                        * ONE;
                    child_particle_context.position.vz =
                        trail_set->trails[trail]
                            .splineSamples[trail_set->trails[trail].currentSplineSample]
                            .vz
                        * ONE;
                }

                Effects_EmitParticleEvent(effect_instance, particle_definition->tickEvent - 1, 0, 0, &child_particle_context);
            }
        }

        ++trail_set->curveSampleDistance;

        if (trail_set->opaque0 != 0) {
            --trail_set->opaque0;
            if (!trail_set->opaque0) {
                renderer_alive = 0;
                Core_Memory_FreeFromHighEnd(trail_set->trails);
            }
        }
        break;

    case 3:
        trail_set->opaque0 = 1;
        break;

    case 4:
        renderer_alive = 0;
        Core_Memory_FreeFromHighEnd(trail_set->trails);
        break;
    }

    return renderer_alive;
}
#undef TRAIL_SAMPLE_COUNT

}  // extern "C"
