// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// Template body: Effects::Templates::Grid (decomp src/EFFECT/grids.h:23). Included once per instance by Effects/Templates/Grid.c with that plugin's bindings.
int VS_GRID_FUNCTION(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    int x[9];
    int z[9];
    SVECTOR vertices[9][9];
    int screen[9][9];
    int depths[9][9];
    CVECTOR color;
    u_char u[9];
    u_char v[9];
    Effects_EffectInstance* battle;
    int alive;
    int depthMode;
    int xPhase;
    int zPhase;
    int row;
    int column;
    int xSpacing;
    int zSpacing;
    int acceleration;
    int zAcceleration;
    int uvBase;
    int angleX;
    int angleZ;
    int sample;
    int scroll;
    int depth;
    Effects_GridState* state;
    Effects_ParticleEmitterScratch* scratch = (void*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* effect;
    MATRIX* matrix;
    Render_GridPacket* prim;
    alive = 1;
    state = renderer_slot->rendererState;
    battle = Effects_currentInstance;
    switch (lifecycle_command) {
    case 1:
        state = renderer_slot->rendererState = Core_Memory_AllocateFromHighEnd(sizeof(*state));
        state->effect = packed_parameters >> 8;
        state->lifetime = packed_parameters;
        state->age = 0;
        state->xSpacing = state->xSpeed = 0;
        state->zSpacing = state->zSpeed = 0;
        state->xPhase = state->zPhase = 0;
        effect = &Effects_package->block5Data->definitions[state->effect];
        state->tpage =
            ((effect->ringSegments & 7) | ((((u_int*)effect)[1] >> 21) & 0xE0)) + 0x18;
        state->clut = ((effect->ringPositions & 7) << 6) | 0x3C30;
        state->u = ((u_char*)effect)[0xAC];
        state->v = ((u_char*)effect)[0xB0];
        state->uScroll = state->vScroll = 0;
        break;
    case 2:
        effect = &Effects_package->block5Data->definitions[state->effect];
        scratch->flags = effect->flags;
        matrix = &battle->transforms[effect->transformIndex].world;
        Core_Math_InterpolatePairsToWideVector(effect->positionRange,
            Effects_SampleEffectCurve(effect->positionCurve, state->age), &scratch->position);
        switch (scratch->flags & 7) {
        case 0:
            break;
        case 1:
            Core_Math_AddWideAndShortVectors(&scratch->position, Effects_effectOrigin, &scratch->position);
            break;
        case 2:
            scratch->position.vx += matrix->t[0];
            scratch->position.vy += matrix->t[1];
            scratch->position.vz += matrix->t[2];
            break;
        }
        Core_Math_InterpolatePairsToWideVector(effect->extentRange,
            Effects_SampleEffectCurve(effect->extentCurve, state->age), &scratch->extent);
        sample = Effects_SampleEffectCurve(effect->forceCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[2], sample, &scratch->forceMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[3], sample, &scratch->forceMax);
        sample = Effects_SampleEffectCurve(effect->accelerationCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[4], sample, &scratch->accelerationMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[5], sample, &scratch->accelerationMax);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[0],
            Effects_SampleEffectCurve(effect->directionCurve, state->age), &scratch->baseDirection);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[1],
            Effects_SampleEffectCurve(effect->directionJitterCurve, state->age), &scratch->directionJitter);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[2],
            Effects_SampleEffectCurve(effect->speedCurve, state->age), scratch->speedRange);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[0],
            Effects_SampleEffectCurve(effect->massCurve, state->age), scratch->massRange);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[1],
            Effects_SampleEffectCurve(effect->gravityScaleCurve, state->age), scratch->gravityScaleRange);
        depthMode = Core_Math_InterpolateShortPair(
            effect->innerExtentRange, Effects_SampleEffectCurve(effect->innerExtentCurve, state->age));
        state->xSpeed += scratch->accelerationMin.vx;
        state->xSpacing += scratch->forceMin.vx + state->xSpeed;
        xSpacing = scratch->extent.vx + state->xSpacing;
        state->zSpeed += scratch->accelerationMin.vy;
        state->zSpacing += scratch->forceMin.vy + state->zSpeed;
        zSpacing = scratch->extent.vy + state->zSpacing;
        xPhase = state->xPhase += scratch->directionJitter.vx;
        zPhase = state->zPhase += scratch->directionJitter.vy;
        column = acceleration = 0;
        for (sample = 0; sample < 5; ++sample) {
            x[4 - sample] = scratch->position.vx - column;
            x[4 + sample] = column + scratch->position.vx;
            column += xSpacing;
            xSpacing += acceleration + scratch->forceMax.vx;
            acceleration += scratch->accelerationMax.vx;
        }
        row = zAcceleration = 0;
        for (sample = 0; sample < 5; ++sample) {
            z[4 - sample] = scratch->position.vz - row;
            z[4 + sample] = row + scratch->position.vz;
            row += zSpacing;
            zSpacing += zAcceleration + scratch->forceMax.vy;
            zAcceleration += scratch->accelerationMax.vy;
        }
        for (row = 0; row < 9; ++row) {
            angleX = xPhase;
            angleZ = zPhase;
            xPhase += scratch->speedRange[0];
            zPhase += scratch->speedRange[1];
            for (column = 0; column < 9; ++column) {
                vertices[row][column].vx =
                    x[column] + ((Core_Gte_Sin(angleX) * scratch->baseDirection.vx) >> 12);
                vertices[row][column].vz =
                    z[row] + ((Core_Gte_Cos(angleZ) * scratch->baseDirection.vy) >> 12);
                vertices[row][column].vy = scratch->position.vy;
                angleX += scratch->directionJitter.vx;
                angleZ += scratch->directionJitter.vy;
            }
        }
        if (effect->rCurve) {
            sample = Effects_SampleEffectCurve(effect->rCurve, state->age) * 2;
            if (sample < 256) {
                color.r = sample;
            } else {
                color.r = 255;
            }
        } else {
            color.r = 128;
        }
        if (effect->gCurve) {
            sample = Effects_SampleEffectCurve(effect->gCurve, state->age) * 2;
            if (sample < 256) {
                color.g = sample;
            } else {
                color.g = 255;
            }
        } else {
            color.g = 128;
        }
        if (effect->bCurve) {
            sample = Effects_SampleEffectCurve(effect->bCurve, state->age) * 2;
            if (sample < 256) {
                color.b = sample;
            } else {
                color.b = 255;
            }
        } else {
            color.b = 128;
        }
        color.cd = 0x2E;
        scroll = state->uScroll;
        state->uScroll = (scroll + scratch->gravityScaleRange[0]) % effect->pairRanges[0][0];
        uvBase = state->u + scroll;
        for (sample = 0; sample < 9; ++sample) {
            u[sample] = uvBase + effect->pairRanges[0][0] * sample / 8;
        }
        scroll = state->vScroll;
        state->vScroll = (scroll + scratch->gravityScaleRange[1]) % effect->pairRanges[0][2];
        uvBase = state->v + scroll;
        for (sample = 0; sample < 9; ++sample) {
            v[sample] = uvBase + effect->pairRanges[0][2] * sample / 8;
        }
        if (scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&battle->transforms[effect->transformIndex].view);
            Core_Gte_SetTransMatrix(&battle->transforms[effect->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }
        for (row = 0; row < 9; ++row) {
            for (column = 0; column < 9; ++column) {
                Core_Gte_LoadV0(&vertices[row][column]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&screen[row][column]);
                Core_Gte_StoreSz3Otz(&depths[row][column]);
            }
        }
        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column) {
                depth = (depths[row][column] + depths[row][column + 1]
                            + depths[row + 1][column] + depths[row + 1][column + 1])
                          / 4
                      + scratch->extent.vz;
                if (depth < 0x800 && Render_nearClip < depth) {
                    switch (depthMode) {
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
                        if (depth - 16 > Render_nearClip) {
                            depth -= 16;
                        }
                        break;
                    }
                    prim = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = prim + 1;
                    prim->poly.tag = 0xE1000000;
                    prim->drawMode = 0xE1000200;
                    *(int*)&prim->poly.x0 = screen[row][column];
                    *(int*)&prim->poly.x1 = screen[row][column + 1];
                    *(int*)&prim->poly.x2 = screen[row + 1][column];
                    *(int*)&prim->poly.x3 = screen[row + 1][column + 1];
                    *(int*)&prim->poly.r0 = *(int*)&color;
                    prim->poly.u0 = prim->poly.u2 = u[column];
                    prim->poly.u1 = prim->poly.u3 = u[column + 1];
                    prim->poly.v0 = prim->poly.v1 = v[row];
                    prim->poly.v2 = prim->poly.v3 = v[row + 1];
                    prim->poly.tpage = state->tpage;
                    prim->poly.clut = state->clut;
                    prim->tag =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0B000000;
                    ((u_long*)Render_frameScratch.orderingTable)[depth] =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                        | (LegacyGpu_Handle(prim) & 0xFFFFFF);
                }
            }
        }
        ++state->age;
        if (state->lifetime) {
            --state->lifetime;
            if (!state->lifetime) {
                alive = 0;
            }
        }
        break;
    case 3:
        state->lifetime = 1;
        break;
    case 4:
        alive = 0;
        break;
    }
    return alive;
}

