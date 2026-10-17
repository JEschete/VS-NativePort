// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg005.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: PLG005.BIN vs_effect_analyze (src/EFFECT/PLG005.BIN/18.c:35)
int Effects_Plg005_UpdateAnalyzeGrid(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    int gridX[9];
    int gridZ[9];
    SVECTOR vertices[9][9];
    int screenXY[9][9];
    int depths[9][9];
    CVECTOR color;
    u_char uCoords[9];
    u_char vCoords[9];
    Effects_EffectInstance* sp5A0;
    int retVal;
    int depthMode;
    int phaseX;
    int phaseZ;
    Effects_ParticleEmitterScratch* s6;
    Effects_ParticleDefinition* s7;
    Effects_Plg005_AnalyzeGridQuad* fp;
    int j, i, k;
    int var_a0, var_a2, var_t0;
    int var_a1, var_a3;
    int angleX, angleZ;
    int temp;
    Render_EffectFlatQuadPacket* prim;
    MATRIX* m;

    s6 = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    retVal = 1;
    fp = arg0->rendererState;
    sp5A0 = Effects_currentInstance;

    switch (arg1) {
    case 1:
        fp = Core_Memory_AllocateFromHighEnd(sizeof *fp);
        arg0->rendererState = fp;
        fp->opaque1 = arg2 >> 8;
        fp->opaque0 = arg2;
        fp->curveSampleDistance = 0;
        fp->opaqueC = 0;
        fp->opaqueA = 0;
        fp->opaque10 = 0;
        fp->opaqueE = 0;
        fp->opaque14 = 0;
        fp->opaque12 = 0;
        s7 = &Effects_package->block5Data->definitions[fp->opaque1];
        fp->tpage = ((s7->ringSegments & 7) | ((s7->transparencyCurve & 7) << 5)) + 0x18;
        fp->clut = ((s7->ringPositions & 7) << 6) | 0x3C30;
        fp->uBase = s7->pairRanges[3][0];
        fp->vBase = s7->pairRanges[3][2];
        fp->vScroll = 0;
        fp->uScroll = 0;
        break;

    case 2:
        s7 = &Effects_package->block5Data->definitions[fp->opaque1];
        s6->flags = s7->flags;
        m = &sp5A0->transforms[s7->transformIndex].world;
        Core_Math_InterpolatePairsToWideVector(s7->positionRange,
            Effects_SampleEffectCurve(s7->positionCurve, fp->curveSampleDistance), &s6->position);
        switch (s6->flags & 7) {
        case 0:
            break;
        case 1:
            Core_Math_AddWideAndShortVectors(&s6->position, Effects_effectOrigin, &s6->position);
            break;
        case 2:
            applyVector(&s6->position, m->t[0], m->t[1], m->t[2], +=);
            break;
        }
        // bad var reuse: j and i accumulate the grid offsets below
        j = 0;
        k = 0;
        Core_Math_InterpolatePairsToWideVector(s7->extentRange,
            Effects_SampleEffectCurve(s7->extentCurve, fp->curveSampleDistance), &s6->extent);
        temp = Effects_SampleEffectCurve(s7->forceCurve, fp->curveSampleDistance);
        Core_Math_InterpolatePairsToShortVector(&s7->vectorRanges[2][0], temp, &s6->forceMin);
        Core_Math_InterpolatePairsToShortVector(&s7->vectorRanges[3][0], temp, &s6->forceMax);
        temp = Effects_SampleEffectCurve(s7->accelerationCurve, fp->curveSampleDistance);
        Core_Math_InterpolatePairsToShortVector(&s7->vectorRanges[4][0], temp, &s6->accelerationMin);
        Core_Math_InterpolatePairsToShortVector(&s7->vectorRanges[5][0], temp, &s6->accelerationMax);

        Core_Math_InterpolatePairsToWideVector(&s7->vectorRanges[0][0],
            Effects_SampleEffectCurve(s7->directionCurve, fp->curveSampleDistance), &s6->baseDirection);
        Core_Math_InterpolatePairsToWideVector(&s7->vectorRanges[1][0],
            Effects_SampleEffectCurve(s7->directionJitterCurve, fp->curveSampleDistance), &s6->directionJitter);
        Core_Math_InterpolatePairsToPlaneVector(&s7->pairRanges[2][0],
            Effects_SampleEffectCurve(s7->speedCurve, fp->curveSampleDistance), s6->speedRange);
        Core_Math_InterpolatePairsToPlaneVector(&s7->pairRanges[0][0],
            Effects_SampleEffectCurve(s7->massCurve, fp->curveSampleDistance), s6->massRange);
        Core_Math_InterpolatePairsToPlaneVector(&s7->pairRanges[1][0],
            Effects_SampleEffectCurve(s7->gravityScaleCurve, fp->curveSampleDistance), s6->gravityScaleRange);

        depthMode = Core_Math_InterpolateShortPair(
            s7->innerExtentRange, Effects_SampleEffectCurve(s7->innerExtentCurve, fp->curveSampleDistance));

        fp->opaqueC += s6->accelerationMin.vx;
        fp->opaqueA += s6->forceMin.vx + fp->opaqueC;
        var_a2 = s6->extent.vx + fp->opaqueA;
        fp->opaque10 += s6->accelerationMin.vy;
        fp->opaqueE += s6->forceMin.vy + fp->opaque10;
        var_a1 = s6->extent.vy + fp->opaqueE;
        fp->opaque12 += s6->directionJitter.vx;
        phaseX = fp->opaque12;
        fp->opaque14 += s6->directionJitter.vy;
        phaseZ = fp->opaque14;

        var_t0 = 0;
        for (; k < 5; k++) {
            gridX[4 - k] = s6->position.vx - j;
            gridX[4 + k] = j + s6->position.vx;
            j += var_a2;
            var_a2 += var_t0 + s6->forceMax.vx;
            var_t0 += s6->accelerationMax.vx;
        }

        i = var_a3 = 0;
        for (k = 0; k < 5; k++) {
            gridZ[4 - k] = s6->position.vz - i;
            gridZ[4 + k] = i + s6->position.vz;
            i += var_a1;
            var_a1 += var_a3 + s6->forceMax.vy;
            var_a3 += s6->accelerationMax.vy;
        }

        for (i = 0; i < 9; i++) {
            angleX = phaseX;
            angleZ = phaseZ;
            phaseX += s6->speedRange[0];
            phaseZ += s6->speedRange[1];
            for (j = 0; j < 9; j++) {
                vertices[i][j].vx = gridX[j] + ((Core_Gte_Sin(angleX) * s6->baseDirection.vx) >> 12);
                vertices[i][j].vz = gridZ[i] + ((Core_Gte_Cos(angleZ) * s6->baseDirection.vy) >> 12);
                vertices[i][j].vy = s6->position.vy;
                angleX += s6->directionJitter.vx;
                angleZ += s6->directionJitter.vy;
            }
        }

        if (s7->rCurve != 0) {
            temp = Effects_SampleEffectCurve(s7->rCurve, fp->curveSampleDistance) * 2;
            color.r = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.r = 0x80;
        }

        if (s7->gCurve != 0) {
            temp = Effects_SampleEffectCurve(s7->gCurve, fp->curveSampleDistance) * 2;
            color.g = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.g = 0x80;
        }

        if (s7->bCurve != 0) {
            temp = Effects_SampleEffectCurve(s7->bCurve, fp->curveSampleDistance) * 2;
            color.b = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.b = 0x80;
        }

        color.cd = Render_LegacyPrimitiveCode_PolyFt4 | Render_LegacyPrimitiveCode_SemiTrans;

        var_a0 = fp->uBase + fp->uScroll;
        fp->uScroll = (fp->uScroll + s6->gravityScaleRange[0]) % s7->pairRanges[0][0];
        for (k = 0; k < 9; k++) {
            uCoords[k] = var_a0 + (s7->pairRanges[0][0] * k) / 8;
        }

        var_a0 = fp->vBase + fp->vScroll;
        fp->vScroll = (fp->vScroll + s6->gravityScaleRange[1]) % s7->pairRanges[0][2];
        for (k = 0; k < 9; k++) {
            vCoords[k] = var_a0 + (s7->pairRanges[0][2] * k) / 8;
        }

        if (s6->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&sp5A0->transforms[s7->transformIndex].view);
            Core_Gte_SetTransMatrix(&sp5A0->transforms[s7->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }

        for (i = 0; i < 9; i++) {
            for (j = 0; j < 9; j++) {
                Core_Gte_LoadV0(&vertices[i][j]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&screenXY[i][j]);
                Core_Gte_StoreSz3Otz(&depths[i][j]);
            }
        }

        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                int z = (depths[i][j] + depths[i][j + 1] + depths[i + 1][j]
                            + depths[i + 1][j + 1])
                          / 4
                      + s6->extent.vz;
                if (z < 0x800 && Render_nearClip < z) {
                    switch (depthMode) {
                    case 0:
                    default:
                        break;
                    case 1:
                        z = 0x7FE;
                        break;
                    case 2:
                        z = -0xF;
                        break;
                    case 3:
                        z = -0xD;
                        break;
                    case 4:
                        z = -0xB;
                        break;
                    case 5:
                        if (z - 0x10 > Render_nearClip) {
                            z -= 0x10;
                        }
                        break;
                    }
                    prim = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = prim + 1;
                    prim->opaque4.tag = _get_mode(0, 0, 0);
                    prim->drawMode = _get_mode(0, 1, 0);
                    *(int*)&prim->opaque4.x0 = screenXY[i][j];
                    *(int*)&prim->opaque4.x1 = screenXY[i][j + 1];
                    *(int*)&prim->opaque4.x2 = screenXY[i + 1][j];
                    *(int*)&prim->opaque4.x3 = screenXY[i + 1][j + 1];
                    *(int*)&prim->opaque4.r0 = *(int*)&color;
                    prim->opaque4.u0 = prim->opaque4.u2 = uCoords[j];
                    prim->opaque4.u1 = prim->opaque4.u3 = uCoords[j + 1];
                    prim->opaque4.v0 = prim->opaque4.v1 = vCoords[i];
                    prim->opaque4.v2 = prim->opaque4.v3 = vCoords[i + 1];
                    prim->opaque4.tpage = fp->tpage;
                    prim->opaque4.clut = fp->clut;
                    *(int*)&prim->tag0 =
                        (((u_long*)Render_frameScratch.orderingTable)[z] & 0xFFFFFF) | 0x0B000000;
                    ((u_long*)Render_frameScratch.orderingTable)[z] =
                        (((u_long*)Render_frameScratch.orderingTable)[z] & 0xFF000000)
                        | ((u_long)prim & 0xFFFFFF);
                }
            }
        }

        fp->curveSampleDistance++;
        if (fp->opaque0 != 0) {
            fp->opaque0--;
            if (fp->opaque0 == 0) {
                retVal = 0;
            }
        }
        break;

    case 3:
        fp->opaque0 = 1;
        break;

    case 4:
        retVal = 0;
        break;
    }
    return retVal;
}

