// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// Template body: Effects::Templates::TrailDispatch (decomp src/EFFECT/trails5.h:677). Included once per instance by Effects/Templates/TrailDispatch.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

int VS_TRAIL_DISPATCH_FUNCTION(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    Effects_PluginEffectState sp18;
    Effects_ParticleDefinition* temp_s3;
    int trail;
    int j;
    Effects_TrailQuadSet* temp_v0;

    Effects_ParticleEmitterScratch* s4 = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_TrailQuadSet* temp_s0 = arg0->rendererState;
    Effects_EffectInstance* temp_s5 = Effects_currentInstance;
    int var_s6 = 1;

    switch (arg1) {
    case 1:
        temp_v0 = Core_Memory_AllocateFromHighEnd(sizeof *temp_v0 + 8);
        arg0->rendererState = temp_v0;
        temp_v0->definitionIndex = arg2 >> 8;
        temp_v0->opaque0 = arg2;
        temp_v0->curveSampleDistance = 0;
        temp_s3 = &Effects_package->block5Data->definitions[temp_v0->definitionIndex];
        trail = temp_s3->intervalRange[0];

        if (trail > 16) {
            trail = 16;
        }

        temp_v0->trails = Core_Memory_AllocateFromHighEnd(trail * sizeof *temp_v0->trails);
        temp_v0->trailcount = trail;
        temp_v0->opaque3 = 0;

        if (temp_s3->transparencyCurve != 0) {

            // bad var reuse, trail = length of curve
            trail = Effects_package->curveBlock->curveSizes[temp_s3->transparencyCurve];

            if (trail > 1) {
                --trail;
            }

            for (j = 0; j < 5; ++j) {
                temp_v0->sampledTransparencyCurve[j] =
                    Effects_package->curves[temp_s3->transparencyCurve][(j * trail) / 4];
            }

        } else {
            for (j = 0; j < 5; ++j) {
                temp_v0->sampledTransparencyCurve[j] = 0x80;
            }
        }

        Effects_ClearParticleTextureState((int*)&temp_v0->textureAnimation);
        Effects_InitializeParticleTextureAnimation(&temp_v0->textureAnimation, temp_s3->textureIndex, temp_s3->animationId);
        break;

    case 2:
        temp_s3 = &Effects_package->block5Data->definitions[temp_s0->definitionIndex];
        s4->flags = (temp_s3->flags);

        if (temp_s0->opaque3 == 0) {
            VS_TRAIL_ORIGIN_FUNCTION(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_TARGET_FUNCTION(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_CONTROL_FUNCTION(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_INIT_FUNCTION(temp_s3, s4, temp_s5, temp_s0);
            temp_s0->opaque3 = 1U;
        }

        VS_TRAIL_UPDATE_FUNCTION(temp_s3, s4, temp_s5, temp_s0);
        VS_TRAIL_RENDER_FUNCTION(temp_s3, s4, temp_s5, temp_s0);

        if (temp_s3->tickEvent != 0) {
            for (trail = 0; trail < temp_s0->trailcount; ++trail) {

                sp18.transparency = 0;

                if (temp_s3->flags & 0x20000) {
                    Core_Gte_SetRotMatrix(&temp_s5->transforms[temp_s3->transformIndex].world);
                    Core_Gte_SetTransMatrix(&temp_s5->transforms[temp_s3->transformIndex].world);
                    s4->vector.vx =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vx;
                    s4->vector.vy =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vy;
                    s4->vector.vz =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vz;

                    Core_Gte_LoadV0(&s4->vector);
                    Core_Gte_Rtv0Tr();
                    Core_Gte_StoreLvnl(&s4->aimPoint);

                    sp18.position.vx = s4->aimPoint.vx * ONE;
                    sp18.position.vy = s4->aimPoint.vy * ONE;
                    sp18.position.vz = s4->aimPoint.vz * ONE;
                } else {
                    sp18.position.vx =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vx
                        * ONE;
                    sp18.position.vy =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vy
                        * ONE;
                    sp18.position.vz =
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vz
                        * ONE;
                }

                Effects_EmitParticleEvent(temp_s5, temp_s3->tickEvent - 1, 0, 0, &sp18);
            }
        }

        ++temp_s0->curveSampleDistance;

        if (temp_s0->opaque0 != 0) {
            --temp_s0->opaque0;
            if (!temp_s0->opaque0) {
                var_s6 = 0;
                Core_Memory_FreeFromHighEnd(temp_s0->trails);
            }
        }
        break;

    case 3:
        temp_s0->opaque0 = 1;
        break;

    case 4:
        var_s6 = 0;
        Core_Memory_FreeFromHighEnd(temp_s0->trails);
        break;
    }

    return var_s6;
}
#undef TRAIL_SAMPLE_COUNT

