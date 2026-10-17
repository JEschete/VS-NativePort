// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// Template body: Effects::Templates::TrailRender (decomp src/EFFECT/trails5.h:449). Included once per instance by Effects/Templates/TrailRender.c with that plugin's bindings.
#define TRAIL_SAMPLE_COUNT 5

void VS_TRAIL_RENDER_FUNCTION(Effects_ParticleDefinition* arg0, Effects_ParticleEmitterScratch* arg1,
    Effects_EffectInstance* arg2, Effects_TrailQuadSet* arg3)
{
    Render_EffectGouraudQuadPacket sp10;
    DVECTOR sp50[6];
    int sp78[6];
    int spA0[4];
    CVECTOR spC0;
    int _[6] __attribute__((unused));
    DVECTOR* spF0;
    int i;
    int spF8;
    int spFC;
    Effects_Trail* temp_s1;
    int temp_a1_2;
    int temp_s0;
    int var_s0_2;
    int j;
    Render_EffectGouraudQuadPacket* temp_a3;
    Effects_PackageTexture* temp_s7;
    SVECTOR* temp;
    DVECTOR* temp2;
    int temp_s3;

    Core_Math_InterpolatePairsToShortVector(arg0->vectorRanges[4],
        Effects_SampleEffectCurve(arg0->accelerationCurve, arg3->curveSampleDistance), &arg1->accelerationMin);

    if (arg0->rCurve != 0) {
        j = Effects_SampleEffectCurve(arg0->rCurve, arg3->curveSampleDistance) * 2;
        spC0.r = j <= 255 ? j : 255;
    } else {
        spC0.r = 128;
    }

    if (arg0->gCurve != 0) {
        j = Effects_SampleEffectCurve(arg0->gCurve, arg3->curveSampleDistance) * 2;
        spC0.g = j <= 255 ? j : 255;
    } else {
        spC0.g = 128;
    }

    if (arg0->bCurve != 0) {
        j = Effects_SampleEffectCurve(arg0->bCurve, arg3->curveSampleDistance) * 2;
        spC0.b = j <= 255 ? j : 255;
    } else {
        spC0.b = 128;
    }

    spC0.cd = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;

    if (arg1->flags & 0x20000) {
        Core_Gte_SetRotMatrix(&arg2->transforms[arg0->transformIndex].view);
        Core_Gte_SetTransMatrix(&arg2->transforms[arg0->transformIndex].view);
    } else {
        Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
        Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    }

    Effects_AdvanceParticleTextureAnimation(&arg3->textureAnimation);

    temp_s7 = &Effects_package->textures[arg3->textureAnimation.textureInfo->dataIndex];

    for (i = 0; i < arg3->trailcount; ++i) {

        temp_s1 = &arg3->trails[i];

        temp = temp_s1->splineSamples;
        if (temp_s1->splineSampleCount >= 2) {
            temp_s3 = temp_s1->currentSplineSample;
            for (j = 0; j < temp_s1->splineSampleCount; ++j) {
                Core_Gte_LoadV0(&temp[temp_s3]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&sp50[j]);
                Core_Gte_StoreSz3Otz(&sp78[j]);
                temp_s3 = (temp_s3 + 4) % 5;
            }

            for (j = 0; j < (temp_s1->splineSampleCount - 1); ++j) {
                DVECTOR* p = &sp50[j];
                spA0[j] = Core_Gte_Atan2(p[1].vy - p[0].vy, p[1].vx - p[0].vx);
                spA0[j] -= 0x400;
            }

            temp_s3 = arg1->accelerationMin.vx / 2;
            temp_s0 = (Core_Gte_Cos(spA0[0]) * temp_s3) >> 0xC;
            temp_a1_2 = (Core_Gte_Sin(spA0[0]) * temp_s3) >> 0xC;
            spF0 = sp50;
            sp10.opaque4.x0 = spF0[0].vx + temp_s0;
            sp10.opaque4.y0 = spF0[0].vy + temp_a1_2;
            sp10.opaque4.x2 = spF0[0].vx - temp_s0;
            sp10.opaque4.y2 = spF0[0].vy - temp_a1_2;

            sp10.opaque4.u0 = sp10.opaque4.u2 =
                temp_s7->u + ((temp_s7->w * temp_s1->trailCollapseCounter) / 4);
            spF8 = temp_s7->v;
            spFC = temp_s7->v + temp_s7->h;
            sp10.opaque4.v0 = spF8;
            sp10.opaque4.v2 = spFC;
            sp10.opaque4.r0 =
                (spC0.r * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter])
                >> 7;
            sp10.opaque4.g0 =
                (spC0.g * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter])
                >> 7;
            sp10.opaque4.b0 =
                (spC0.b * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter])
                >> 7;
            sp10.opaque4.code = spC0.cd;

            for (j = 0; j < (temp_s1->splineSampleCount - 2); ++j) {
                var_s0_2 = (spA0[j] + spA0[j + 1]) / 2;

                if (((spA0[j + 1] - spA0[j]) + 0x7FF) >= 0xFFFU) {
                    var_s0_2 += 0x800;
                }

                temp_s0 = (Core_Gte_Cos(var_s0_2) * temp_s3) >> 0xC;
                temp_a1_2 = (Core_Gte_Sin(var_s0_2) * temp_s3) >> 0xC;
                temp2 = &spF0[j];
                sp10.opaque4.x1 = temp2[1].vx + temp_s0;
                sp10.opaque4.y1 = temp2[1].vy + temp_a1_2;
                sp10.opaque4.x3 = temp2[1].vx - temp_s0;
                sp10.opaque4.y3 = temp2[1].vy - temp_a1_2;

                sp10.opaque4.u1 = sp10.opaque4.u3 =
                    temp_s7->u
                    + ((temp_s7->w * ((temp_s1->trailCollapseCounter - j) - 1)) / 4);
                sp10.opaque4.v1 = spF8;
                sp10.opaque4.v3 = spFC;
                sp10.opaque4.r1 =
                    ((spC0.r
                         * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                sp10.opaque4.g1 =
                    ((spC0.g
                         * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                sp10.opaque4.b1 =
                    ((spC0.b
                         * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                sp10.opaque4.p1 = spC0.cd;

                temp_s0 = (sp78[j] + sp78[j + 1]) / 2;

                if ((temp_s0 < 0x800) && (Render_nearClip < temp_s0)) {
                    temp_a3 = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = temp_a3 + 1;
                    temp_a3->wordCount = 0xE;
                    temp_a3->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                    temp_a3->opaque4.tag = 0xE1000000;
                    temp_a3->drawMode = 0xE1000200;
                    *(int*)&temp_a3->tag0 =
                        (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFFFFFF) | 0x0E000000;
                    ((u_long*)Render_frameScratch.orderingTable)[temp_s0] =
                        (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFF000000)
                        | (LegacyGpu_Handle(temp_a3) & 0xFFFFFF);
                    *(int*)&temp_a3->opaque4.r0 = *(int*)&sp10.opaque4.r0;
                    *(int*)&temp_a3->opaque4.r1 = *(int*)&sp10.opaque4.r1;
                    *(int*)&temp_a3->opaque4.r2 = *(int*)&sp10.opaque4.r0;
                    *(int*)&temp_a3->opaque4.r3 = *(int*)&sp10.opaque4.r1;
                    *(int*)&temp_a3->opaque4.x0 = *(int*)&sp10.opaque4.x0;
                    *(int*)&temp_a3->opaque4.x1 = *(int*)&sp10.opaque4.x1;
                    *(int*)&temp_a3->opaque4.x2 = *(int*)&sp10.opaque4.x2;
                    *(int*)&temp_a3->opaque4.x3 = *(int*)&sp10.opaque4.x3;
                    *(short*)&temp_a3->opaque4.u0 = *(short*)&sp10.opaque4.u0;
                    *(short*)&temp_a3->opaque4.u1 = *(short*)&sp10.opaque4.u1;
                    *(short*)&temp_a3->opaque4.u2 = *(short*)&sp10.opaque4.u2;
                    *(short*)&temp_a3->opaque4.u3 = *(short*)&sp10.opaque4.u3;
                    *(short*)&temp_a3->opaque4.tpage = temp_s7->tpage;
                    *(short*)&temp_a3->opaque4.clut = temp_s7->clut;
                }
                *(int*)&sp10.opaque4.x0 = *(int*)&sp10.opaque4.x1;
                *(int*)&sp10.opaque4.x2 = *(int*)&sp10.opaque4.x3;
                *(short*)&sp10.opaque4.u0 = *(short*)&sp10.opaque4.u1;
                *(short*)&sp10.opaque4.u2 = *(short*)&sp10.opaque4.u3;
                *(int*)&sp10.opaque4.r0 = *(int*)&sp10.opaque4.r1;
            }

            temp_s0 = (Core_Gte_Cos(spA0[j]) * temp_s3) >> 0xC;
            temp_a1_2 = (Core_Gte_Sin(spA0[j]) * temp_s3) >> 0xC;
            sp10.opaque4.x1 = ((u_short*)spF0)[j * 2 + 2] + temp_s0;
            sp10.opaque4.y1 = ((u_short*)spF0)[j * 2 + 3] + temp_a1_2;
            sp10.opaque4.x3 = ((u_short*)spF0)[j * 2 + 2] - temp_s0;
            sp10.opaque4.y3 = ((u_short*)spF0)[j * 2 + 3] - temp_a1_2;
            sp10.opaque4.u1 = sp10.opaque4.u3 = temp_s7->u;
            sp10.opaque4.v1 = spF8;
            sp10.opaque4.v3 = spFC;
            sp10.opaque4.r1 = (spC0.r * arg3->sampledTransparencyCurve[0]) >> 7;
            sp10.opaque4.g1 = (spC0.g * arg3->sampledTransparencyCurve[0]) >> 7;
            sp10.opaque4.b1 = (spC0.b * arg3->sampledTransparencyCurve[0]) >> 7;

            temp_s0 = (sp78[j] + sp78[j + 1]) / 2;

            if ((temp_s0 < 0x800) && (Render_nearClip < temp_s0)) {
                temp_a3 = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = temp_a3 + 1;
                temp_a3->wordCount = 0xE;
                temp_a3->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                temp_a3->opaque4.tag = 0xE1000000;
                temp_a3->drawMode = 0xE1000200;
                *(int*)&temp_a3->tag0 =
                    (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)Render_frameScratch.orderingTable)[temp_s0] =
                    (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFF000000)
                    | (LegacyGpu_Handle(temp_a3) & 0xFFFFFF);
                *(int*)&temp_a3->opaque4.r0 = *(int*)&sp10.opaque4.r0;
                *(int*)&temp_a3->opaque4.r1 = *(int*)&sp10.opaque4.r1;
                *(int*)&temp_a3->opaque4.r2 = *(int*)&sp10.opaque4.r0;
                *(int*)&temp_a3->opaque4.r3 = *(int*)&sp10.opaque4.r1;
                *(int*)&temp_a3->opaque4.x0 = *(int*)&sp10.opaque4.x0;
                *(int*)&temp_a3->opaque4.x1 = *(int*)&sp10.opaque4.x1;
                *(int*)&temp_a3->opaque4.x2 = *(int*)&sp10.opaque4.x2;
                *(int*)&temp_a3->opaque4.x3 = *(int*)&sp10.opaque4.x3;
                *(short*)&temp_a3->opaque4.u0 = *(short*)&sp10.opaque4.u0;
                *(short*)&temp_a3->opaque4.u1 = *(short*)&sp10.opaque4.u1;
                *(short*)&temp_a3->opaque4.u2 = *(short*)&sp10.opaque4.u2;
                *(short*)&temp_a3->opaque4.u3 = *(short*)&sp10.opaque4.u3;
                *(short*)&temp_a3->opaque4.tpage = temp_s7->tpage;
                *(short*)&temp_a3->opaque4.clut = temp_s7->clut;
            }
        }
    }
}
#undef TRAIL_SAMPLE_COUNT

