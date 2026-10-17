// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// Template body: Effects::Templates::ParticleLineRender (decomp src/EFFECT/particleLineRender.h:13). Included once per instance by Effects/Templates/ParticleLineRender.c with that plugin's bindings.
#ifndef VS_PARTICLE_LINE_TYPE
#define VS_PARTICLE_LINE_TYPE
#endif

void VS_PARTICLE_RENDER_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_ParticleQuadSet* arg3)
{
    int screenPoints[10];
    int depths[10];
    CVECTOR color;
    int blendMode = (((u_int*)arg0)[1] >> 26) & 3;
    int particleIndex;
    int segment;
    int renderSegment;
    int width;
    int depth;
    int screenOffset;
    int* endpoint;
    u_int endpointStep;
    SVECTOR* controlPoints;
    Render_ParticleLinePacket* prim;
    width = Core_Math_InterpolateShortPair(arg0->innerExtentRange, Effects_SampleEffectCurve(arg0->innerExtentCurve, arg3->age));
    if (!width) {
        width = 1;
    }

    if (arg0->rCurve != 0) {
        segment = Effects_SampleEffectCurve(arg0->rCurve, arg3->age) * 2;
        color.r = segment <= 255 ? segment : 255;
    } else {
        color.r = 128;
    }

    if (arg0->gCurve != 0) {
        segment = Effects_SampleEffectCurve(arg0->gCurve, arg3->age) * 2;
        color.g = segment <= 255 ? segment : 255;
    } else {
        color.g = 128;
    }

    if (arg0->bCurve != 0) {
        segment = Effects_SampleEffectCurve(arg0->bCurve, arg3->age) * 2;
        color.b = segment <= 255 ? segment : 255;
    } else {
        color.b = 128;
    }

    color.cd = 0x42;

    if (arg1->flags & 0x20000) {
        Core_Gte_SetRotMatrix(&arg2->transforms[arg0->transformIndex].view);
        Core_Gte_SetTransMatrix(&arg2->transforms[arg0->transformIndex].view);
    } else {
        Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
        Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    }

    for (particleIndex = 0; particleIndex < arg3->count; ++particleIndex) {
        controlPoints = &arg3->controlPoints[particleIndex * 4];
        Core_Gte_LoadV0(controlPoints);
        Core_Gte_Rtps();
        Core_Gte_StoreSxy2(&screenPoints[0]);
        Core_Gte_StoreSz3Otz(&depths[0]);
        for (segment = 1; segment < 8; ++segment) {
            Core_Math_InterpolateLagrangeShortVector(controlPoints, controlPoints + 1,
                controlPoints + 2, controlPoints + 3,
                (segment * 0x600) - 0x1000 + Core_Random_Rand() % width - width / 2, &arg1->vector);
            Core_Gte_LoadV0(&arg1->vector);
            Core_Gte_Rtps();
            Core_Gte_StoreSxy2(&screenPoints[segment]);
            Core_Gte_StoreSz3Otz(&depths[segment]);
        }
        Core_Gte_LoadV0(controlPoints + 3);
        Core_Gte_Rtps();
        Core_Gte_StoreSxy2(&screenPoints[segment]);
        Core_Gte_StoreSz3Otz(&depths[segment]);
        for (renderSegment = 0; renderSegment < 8; ++renderSegment) {
            depth = (depths[renderSegment] + depths[renderSegment + 1]) / 2;
            if (depth < 0x800 && Render_nearClip < depth) {
                switch (arg0->vectorRanges[5][0]) {
                case 0:
                    break;
                case 1:
                    depth = 0x7FE;
                    break;
                case 2:
                    depth = -15;
                    break;
                case 3:
                    depth = -13;
                    break;
                case 4:
                    depth = -11;
                    break;
                case 5:
                    if (Render_nearClip < depth - 16) {
                        depth -= 16;
                    }
                    break;
                }
                prim = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = prim + 1;
                prim->wordCount = 5;
                prim->line.code = 0x42;
                prim->line.tag = 0xE1000000 | (blendMode << 5);
                prim->drawMode = 0xE1000200;
                *(int*)&prim->tag0 =
                    (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x05000000;
                endpointStep = 1;
                ((u_long*)Render_frameScratch.orderingTable)[depth] =
                    (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                    | (LegacyGpu_Handle(prim) & 0xFFFFFF);
                *(int*)&prim->line.r0 = *(int*)&color;
                screenOffset = renderSegment * sizeof(screenPoints[0]);
                *(int*)&prim->line.x0 = *(int*)((char*)screenPoints + screenOffset);
                endpoint = &screenPoints[renderSegment + endpointStep];
                *(int*)&prim->line.x1 = *endpoint;
                Effects_StepParticleEndpointTowardStart(prim);
            }
        }
    }
}
#undef VS_PARTICLE_LINE_TYPE

