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

// Template body: Effects::Templates::ParticleRender (decomp src/EFFECT/particleRender.h:17). Included once per instance by Effects/Templates/ParticleRender.c with that plugin's bindings.
#ifndef VS_PARTICLE_PRIM_TYPE
#define VS_PARTICLE_PRIM_TYPE
#endif

void VS_PARTICLE_RENDER_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_ParticleQuadSet* arg3)
{
    Render_ParticleQuadPacket quad;
    DVECTOR screenPoints[10];
    int depths[10];
    int angles[8];
    CVECTOR color;
    int curveSamples[10];
    DVECTOR* screenStart;
    int particleIndex;
    int topV;
    int bottomV;
    SVECTOR* controlPoints;
    int offsetY;
    int offsetX;
    int angleOrDepth;
    int segment;
    Render_ParticleQuadPacket* prim;
    Effects_PackageTexture* texture;
    DVECTOR* point;
    int width;

    width = Core_Math_InterpolateShortPair(arg0->innerExtentRange, Effects_SampleEffectCurve(arg0->innerExtentCurve, arg3->age));
    if (!width) {
        width = 1;
    }

    Core_Math_InterpolatePairsToShortVector(
        arg0->vectorRanges[4], Effects_SampleEffectCurve(arg0->accelerationCurve, arg3->age), &arg1->accelerationMin);

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

    color.cd = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;

    if (arg1->flags & 0x20000) {
        Core_Gte_SetRotMatrix(&arg2->transforms[arg0->transformIndex].view);
        Core_Gte_SetTransMatrix(&arg2->transforms[arg0->transformIndex].view);
    } else {
        Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
        Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    }

    Effects_AdvanceParticleTextureAnimation(&arg3->textureAnimation);

    texture = &Effects_package->textures[arg3->textureAnimation.textureInfo->dataIndex];

    for (particleIndex = 0; particleIndex < arg3->count; ++particleIndex) {

        controlPoints = &arg3->controlPoints[particleIndex * 4];
        Core_Gte_LoadV0(controlPoints);
        Core_Gte_Rtps();
        Core_Gte_StoreSxy2(&screenPoints[0]);
        Core_Gte_StoreSz3Otz(&depths[0]);
        curveSamples[0] = 0;
        for (segment = 1; segment < 8; ++segment) {
            int sampleJitter = Core_Random_Rand();
            // PLG045 func_800FA760, 0x800FAA74: DIV by zero leaves the dividend in HI.
            if (width != 0) {
                sampleJitter %= width;
            }
            curveSamples[segment] = segment * 0x600 + sampleJitter - width / 2;
            Core_Math_InterpolateLagrangeShortVector(controlPoints, controlPoints + 1,
                controlPoints + 2, controlPoints + 3, curveSamples[segment] - 0x1000,
                &arg1->vector);
            Core_Gte_LoadV0(&arg1->vector);
            Core_Gte_Rtps();
            Core_Gte_StoreSxy2(&screenPoints[segment]);
            Core_Gte_StoreSz3Otz(&depths[segment]);
        }
        Core_Gte_LoadV0(controlPoints + 3);
        Core_Gte_Rtps();
        Core_Gte_StoreSxy2(&screenPoints[segment]);
        Core_Gte_StoreSz3Otz(&depths[segment]);
        curveSamples[segment] = 0x3000;

        for (segment = 0; segment < 8; ++segment) {
            DVECTOR* p = &screenPoints[segment];
            angles[segment] = Core_Gte_Atan2(p[1].vy - p[0].vy, p[1].vx - p[0].vx);
            angles[segment] -= 0x400;
        }

        width = arg1->accelerationMin.vx / 2;
        screenStart = screenPoints;
        if (arg1->accelerationMin.vy) {
            width += (Core_Random_Rand() % arg1->accelerationMin.vy) / 2;
        }
        offsetX = (Core_Gte_Cos(angles[0]) * width) >> 0xC;
        offsetY = (Core_Gte_Sin(angles[0]) * width) >> 0xC;
        quad.opaque4.x0 = screenStart[0].vx + offsetX;
        quad.opaque4.y0 = screenStart[0].vy + offsetY;
        quad.opaque4.x2 = screenStart[0].vx - offsetX;
        quad.opaque4.y2 = screenStart[0].vy - offsetY;

        quad.opaque4.u0 = quad.opaque4.u2 = texture->u;
        topV = texture->v;
        bottomV = topV + texture->h;
        quad.opaque4.v0 = topV;
        quad.opaque4.v2 = bottomV;
        for (segment = 0; segment < 7; ++segment) {
            width = arg1->accelerationMin.vx / 2;
            if (arg1->accelerationMin.vy) {
                width += (Core_Random_Rand() % arg1->accelerationMin.vy) / 2;
            }
            angleOrDepth = (angles[segment] + angles[segment + 1]) / 2;
            offsetX = (Core_Gte_Cos(angleOrDepth) * width) >> 0xC;
            offsetY = (Core_Gte_Sin(angleOrDepth) * width) >> 0xC;
            point = &screenStart[segment];
            quad.opaque4.x1 = point[1].vx + offsetX;
            quad.opaque4.y1 = point[1].vy + offsetY;
            quad.opaque4.x3 = point[1].vx - offsetX;
            quad.opaque4.y3 = point[1].vy - offsetY;

            quad.opaque4.u1 = quad.opaque4.u3 =
                texture->u + ((texture->w * curveSamples[segment + 1]) / 0x3000);
            quad.opaque4.v1 = topV;
            quad.opaque4.v3 = bottomV;
            angleOrDepth = (depths[segment] + depths[segment + 1]) / 2;

            if ((angleOrDepth < 0x800) && (Render_nearClip < angleOrDepth)) {
                switch (arg0->vectorRanges[5][0]) {
                case 0:
                    break;
                case 1:
                    angleOrDepth = 0x7FE;
                    break;
                case 2:
                    angleOrDepth = -15;
                    break;
                case 3:
                    angleOrDepth = -13;
                    break;
                case 4:
                    angleOrDepth = -11;
                    break;
                case 5:
                    if (Render_nearClip < angleOrDepth - 16) {
                        angleOrDepth -= 16;
                    }
                    break;
                }
                prim = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = prim + 1;
                prim->wordCount = 0xE;
                prim->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                prim->opaque4.tag = 0xE1000000;
                prim->drawMode = 0xE1000200;
                *(int*)&prim->tag0 =
                    (((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] =
                    (((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] & 0xFF000000)
                    | (LegacyGpu_Handle(prim) & 0xFFFFFF);
                *(int*)&prim->opaque4.r0 = *(int*)&color;
                *(int*)&prim->opaque4.r1 = *(int*)&color;
                *(int*)&prim->opaque4.r2 = *(int*)&color;
                *(int*)&prim->opaque4.r3 = *(int*)&color;
                *(int*)&prim->opaque4.x0 = *(int*)&quad.opaque4.x0;
                *(int*)&prim->opaque4.x1 = *(int*)&quad.opaque4.x1;
                *(int*)&prim->opaque4.x2 = *(int*)&quad.opaque4.x2;
                *(int*)&prim->opaque4.x3 = *(int*)&quad.opaque4.x3;
                *(short*)&prim->opaque4.u0 = *(short*)&quad.opaque4.u0;
                *(short*)&prim->opaque4.u1 = *(short*)&quad.opaque4.u1;
                *(short*)&prim->opaque4.u2 = *(short*)&quad.opaque4.u2;
                *(short*)&prim->opaque4.u3 = *(short*)&quad.opaque4.u3;
                *(short*)&prim->opaque4.tpage = texture->tpage;
                *(short*)&prim->opaque4.clut = texture->clut;
            }
            *(int*)&quad.opaque4.x0 = *(int*)&quad.opaque4.x1;
            *(int*)&quad.opaque4.x2 = *(int*)&quad.opaque4.x3;
            *(short*)&quad.opaque4.u0 = *(short*)&quad.opaque4.u1;
            *(short*)&quad.opaque4.u2 = *(short*)&quad.opaque4.u3;
        }

        width = arg1->accelerationMin.vx / 2;
        if (arg1->accelerationMin.vy) {
            width += (Core_Random_Rand() % arg1->accelerationMin.vy) / 2;
        }
        offsetX = (Core_Gte_Cos(angles[segment]) * width) >> 0xC;
        offsetY = (Core_Gte_Sin(angles[segment]) * width) >> 0xC;
        quad.opaque4.x1 = ((u_short*)screenStart)[segment * 2 + 2] + offsetX;
        quad.opaque4.y1 = ((u_short*)screenStart)[segment * 2 + 3] + offsetY;
        quad.opaque4.x3 = ((u_short*)screenStart)[segment * 2 + 2] - offsetX;
        quad.opaque4.y3 = ((u_short*)screenStart)[segment * 2 + 3] - offsetY;
        quad.opaque4.u1 = quad.opaque4.u3 = texture->u + texture->w;
        quad.opaque4.v1 = topV;
        quad.opaque4.v3 = bottomV;
        angleOrDepth = (depths[segment] + depths[segment + 1]) / 2;

        if ((angleOrDepth < 0x800) && (Render_nearClip < angleOrDepth)) {
            switch (arg0->vectorRanges[5][0]) {
            case 0:
                break;
            case 1:
                angleOrDepth = 0x7FE;
                break;
            case 2:
                angleOrDepth = -15;
                break;
            case 3:
                angleOrDepth = -13;
                break;
            case 4:
                angleOrDepth = -11;
                break;
            case 5:
                if (Render_nearClip < angleOrDepth - 16) {
                    angleOrDepth -= 16;
                }
                break;
            }
            prim = Render_frameScratch.primitiveCursor;
            Render_frameScratch.primitiveCursor = prim + 1;
            prim->wordCount = 0xE;
            prim->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
            prim->opaque4.tag = 0xE1000000;
            prim->drawMode = 0xE1000200;
            *(int*)&prim->tag0 =
                (((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] & 0xFFFFFF) | 0x0E000000;
            ((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] =
                (((u_long*)Render_frameScratch.orderingTable)[angleOrDepth] & 0xFF000000)
                | (LegacyGpu_Handle(prim) & 0xFFFFFF);
            *(int*)&prim->opaque4.r0 = *(int*)&color;
            *(int*)&prim->opaque4.r1 = *(int*)&color;
            *(int*)&prim->opaque4.r2 = *(int*)&color;
            *(int*)&prim->opaque4.r3 = *(int*)&color;
            *(int*)&prim->opaque4.x0 = *(int*)&quad.opaque4.x0;
            *(int*)&prim->opaque4.x1 = *(int*)&quad.opaque4.x1;
            *(int*)&prim->opaque4.x2 = *(int*)&quad.opaque4.x2;
            *(int*)&prim->opaque4.x3 = *(int*)&quad.opaque4.x3;
            *(short*)&prim->opaque4.u0 = *(short*)&quad.opaque4.u0;
            *(short*)&prim->opaque4.u1 = *(short*)&quad.opaque4.u1;
            *(short*)&prim->opaque4.u2 = *(short*)&quad.opaque4.u2;
            *(short*)&prim->opaque4.u3 = *(short*)&quad.opaque4.u3;
            *(short*)&prim->opaque4.tpage = texture->tpage;
            *(short*)&prim->opaque4.clut = texture->clut;
        }
    }
}
#undef VS_PARTICLE_PRIM_TYPE

