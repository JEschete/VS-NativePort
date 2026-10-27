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
int Effects_Plg005_UpdateAnalyzeGrid(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    int gridX[9];
    int gridZ[9];
    SVECTOR vertices[9][9];
    int screenXY[9][9];
    int depths[9][9];
    CVECTOR color;
    u_char uCoords[9];
    u_char vCoords[9];
    Effects_EffectInstance* effect_instance;
    int retVal;
    int depthMode;
    int phaseX;
    int phaseZ;
    Effects_ParticleEmitterScratch* emitter_scratch;
    Effects_ParticleDefinition* particle_definition;
    Effects_Plg005_AnalyzeGridQuad* fp;
    int j, i, k;
    int texture_axis_origin, x_spacing, x_spacing_increment;
    int z_spacing, z_spacing_increment;
    int angleX, angleZ;
    int temp;
    Render_EffectFlatQuadPacket* prim;
    MATRIX* m;

    emitter_scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    retVal = 1;
    fp = renderer_slot->rendererState;
    effect_instance = Effects_currentInstance;

    switch (lifecycle_command) {
    case 1:
        fp = Core_Memory_AllocateFromHighEnd(sizeof *fp);
        renderer_slot->rendererState = fp;
        fp->opaque1 = packed_parameters >> 8;
        fp->opaque0 = packed_parameters;
        fp->curveSampleDistance = 0;
        fp->opaqueC = 0;
        fp->opaqueA = 0;
        fp->opaque10 = 0;
        fp->opaqueE = 0;
        fp->opaque14 = 0;
        fp->opaque12 = 0;
        particle_definition = &Effects_package->block5Data->definitions[fp->opaque1];
        fp->tpage = ((particle_definition->ringSegments & 7) | ((particle_definition->transparencyCurve & 7) << 5)) + 0x18;
        fp->clut = ((particle_definition->ringPositions & 7) << 6) | 0x3C30;
        fp->uBase = particle_definition->pairRanges[3][0];
        fp->vBase = particle_definition->pairRanges[3][2];
        fp->vScroll = 0;
        fp->uScroll = 0;
        break;

    case 2:
        particle_definition = &Effects_package->block5Data->definitions[fp->opaque1];
        emitter_scratch->flags = particle_definition->flags;
        m = &effect_instance->transforms[particle_definition->transformIndex].world;
        Core_Math_InterpolatePairsToWideVector(particle_definition->positionRange,
            Effects_SampleEffectCurve(particle_definition->positionCurve, fp->curveSampleDistance), &emitter_scratch->position);
        switch (emitter_scratch->flags & 7) {
        case 0:
            break;
        case 1:
            Core_Math_AddWideAndShortVectors(&emitter_scratch->position, Effects_effectOrigin, &emitter_scratch->position);
            break;
        case 2:
            applyVector(&emitter_scratch->position, m->t[0], m->t[1], m->t[2], +=);
            break;
        }
        // bad var reuse: j and i accumulate the grid offsets below
        j = 0;
        k = 0;
        Core_Math_InterpolatePairsToWideVector(particle_definition->extentRange,
            Effects_SampleEffectCurve(particle_definition->extentCurve, fp->curveSampleDistance), &emitter_scratch->extent);
        temp = Effects_SampleEffectCurve(particle_definition->forceCurve, fp->curveSampleDistance);
        Core_Math_InterpolatePairsToShortVector(&particle_definition->vectorRanges[2][0], temp, &emitter_scratch->forceMin);
        Core_Math_InterpolatePairsToShortVector(&particle_definition->vectorRanges[3][0], temp, &emitter_scratch->forceMax);
        temp = Effects_SampleEffectCurve(particle_definition->accelerationCurve, fp->curveSampleDistance);
        Core_Math_InterpolatePairsToShortVector(&particle_definition->vectorRanges[4][0], temp, &emitter_scratch->accelerationMin);
        Core_Math_InterpolatePairsToShortVector(&particle_definition->vectorRanges[5][0], temp, &emitter_scratch->accelerationMax);

        Core_Math_InterpolatePairsToWideVector(&particle_definition->vectorRanges[0][0],
            Effects_SampleEffectCurve(particle_definition->directionCurve, fp->curveSampleDistance), &emitter_scratch->baseDirection);
        Core_Math_InterpolatePairsToWideVector(&particle_definition->vectorRanges[1][0],
            Effects_SampleEffectCurve(particle_definition->directionJitterCurve, fp->curveSampleDistance), &emitter_scratch->directionJitter);
        Core_Math_InterpolatePairsToPlaneVector(&particle_definition->pairRanges[2][0],
            Effects_SampleEffectCurve(particle_definition->speedCurve, fp->curveSampleDistance), emitter_scratch->speedRange);
        Core_Math_InterpolatePairsToPlaneVector(&particle_definition->pairRanges[0][0],
            Effects_SampleEffectCurve(particle_definition->massCurve, fp->curveSampleDistance), emitter_scratch->massRange);
        Core_Math_InterpolatePairsToPlaneVector(&particle_definition->pairRanges[1][0],
            Effects_SampleEffectCurve(particle_definition->gravityScaleCurve, fp->curveSampleDistance), emitter_scratch->gravityScaleRange);

        depthMode = Core_Math_InterpolateShortPair(
            particle_definition->innerExtentRange, Effects_SampleEffectCurve(particle_definition->innerExtentCurve, fp->curveSampleDistance));

        fp->opaqueC += emitter_scratch->accelerationMin.vx;
        fp->opaqueA += emitter_scratch->forceMin.vx + fp->opaqueC;
        x_spacing = emitter_scratch->extent.vx + fp->opaqueA;
        fp->opaque10 += emitter_scratch->accelerationMin.vy;
        fp->opaqueE += emitter_scratch->forceMin.vy + fp->opaque10;
        z_spacing = emitter_scratch->extent.vy + fp->opaqueE;
        fp->opaque12 += emitter_scratch->directionJitter.vx;
        phaseX = fp->opaque12;
        fp->opaque14 += emitter_scratch->directionJitter.vy;
        phaseZ = fp->opaque14;

        x_spacing_increment = 0;
        for (; k < 5; k++) {
            gridX[4 - k] = emitter_scratch->position.vx - j;
            gridX[4 + k] = j + emitter_scratch->position.vx;
            j += x_spacing;
            x_spacing += x_spacing_increment + emitter_scratch->forceMax.vx;
            x_spacing_increment += emitter_scratch->accelerationMax.vx;
        }

        i = z_spacing_increment = 0;
        for (k = 0; k < 5; k++) {
            gridZ[4 - k] = emitter_scratch->position.vz - i;
            gridZ[4 + k] = i + emitter_scratch->position.vz;
            i += z_spacing;
            z_spacing += z_spacing_increment + emitter_scratch->forceMax.vy;
            z_spacing_increment += emitter_scratch->accelerationMax.vy;
        }

        for (i = 0; i < 9; i++) {
            angleX = phaseX;
            angleZ = phaseZ;
            phaseX += emitter_scratch->speedRange[0];
            phaseZ += emitter_scratch->speedRange[1];
            for (j = 0; j < 9; j++) {
                vertices[i][j].vx = gridX[j] + ((Core_Gte_Sin(angleX) * emitter_scratch->baseDirection.vx) >> 12);
                vertices[i][j].vz = gridZ[i] + ((Core_Gte_Cos(angleZ) * emitter_scratch->baseDirection.vy) >> 12);
                vertices[i][j].vy = emitter_scratch->position.vy;
                angleX += emitter_scratch->directionJitter.vx;
                angleZ += emitter_scratch->directionJitter.vy;
            }
        }

        if (particle_definition->rCurve != 0) {
            temp = Effects_SampleEffectCurve(particle_definition->rCurve, fp->curveSampleDistance) * 2;
            color.r = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.r = 0x80;
        }

        if (particle_definition->gCurve != 0) {
            temp = Effects_SampleEffectCurve(particle_definition->gCurve, fp->curveSampleDistance) * 2;
            color.g = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.g = 0x80;
        }

        if (particle_definition->bCurve != 0) {
            temp = Effects_SampleEffectCurve(particle_definition->bCurve, fp->curveSampleDistance) * 2;
            color.b = temp >= 0x100 ? 0xFF : temp;
        } else {
            color.b = 0x80;
        }

        color.cd = Render_LegacyPrimitiveCode_PolyFt4 | Render_LegacyPrimitiveCode_SemiTrans;

        texture_axis_origin = fp->uBase + fp->uScroll;
        fp->uScroll = (fp->uScroll + emitter_scratch->gravityScaleRange[0]) % particle_definition->pairRanges[0][0];
        for (k = 0; k < 9; k++) {
            uCoords[k] = texture_axis_origin + (particle_definition->pairRanges[0][0] * k) / 8;
        }

        texture_axis_origin = fp->vBase + fp->vScroll;
        fp->vScroll = (fp->vScroll + emitter_scratch->gravityScaleRange[1]) % particle_definition->pairRanges[0][2];
        for (k = 0; k < 9; k++) {
            vCoords[k] = texture_axis_origin + (particle_definition->pairRanges[0][2] * k) / 8;
        }

        if (emitter_scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&effect_instance->transforms[particle_definition->transformIndex].view);
            Core_Gte_SetTransMatrix(&effect_instance->transforms[particle_definition->transformIndex].view);
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
                      + emitter_scratch->extent.vz;
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
                        | (LegacyGpu_Handle(prim) & 0xFFFFFF);
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

