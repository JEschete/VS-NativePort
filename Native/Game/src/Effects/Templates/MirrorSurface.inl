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

// Template body: Effects::Templates::MirrorSurface (decomp src/EFFECT/mirroredSurfaces.h:34). Included once per instance by Effects/Templates/MirrorSurface.c with that plugin's bindings.
#ifndef VS_MIRROR_SURFACE_TYPES
#define VS_MIRROR_SURFACE_TYPES
#endif
#ifdef VS_MIRROR_SURFACE_REPEAT
#define VS_MIRROR_SURFACE_U_SEGMENTS 4
#define VS_MIRROR_SURFACE_U_INDEX(column) ((column) & 3)
#else
#define VS_MIRROR_SURFACE_U_SEGMENTS 16
#define VS_MIRROR_SURFACE_U_INDEX(column) (column)
#endif

int VS_MIRROR_SURFACE_FUNCTION(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    SVECTOR vertices[9][16];
    int screen[9][16];
    int depths[9][16];
    CVECTOR colors[9];
    int maxFactor;
    u_char u[VS_MIRROR_SURFACE_U_SEGMENTS + 1];
    u_char v[9];
    Effects_EffectInstance* battle;
    int alive;
    int heightStep;
    int row, column;
    int radiusStep;
    int radiusValue, heightValue;
    int sample;
    int depth;
    int curveIndex;
    int initIndex, tempHeight;
    Effects_MirrorSurfaceState* state;
    Effects_ParticleEmitterScratch* scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* effect;
    MATRIX* actorMatrix;
    Render_MirrorSurfacePacket* prim;
    Effects_PackageTexture* texture;

    alive = 1;
    state = renderer_slot->rendererState;
    battle = Effects_currentInstance;
    switch (lifecycle_command) {
    case 1:
        state = Core_Memory_AllocateFromHighEnd(sizeof *state);
        renderer_slot->rendererState = state;
        state->effect = packed_parameters >> 8;
        state->lifetime = packed_parameters;
        state->radiusSpeed = 0;
        state->radius = 0;
        state->heightSpeed = 0;
        state->height = 0;
        state->age = 0;
        effect = &Effects_package->block5Data->definitions[state->effect];
        curveIndex = effect->transparencyCurve;
        if (curveIndex) {
            sample = Effects_package->curveBlock->curveSizes[curveIndex];
            if (sample >= 2) {
                --sample;
            }
            for (initIndex = 0; initIndex < 9; ++initIndex) {
                state->factors[initIndex] =
                    Effects_package->curves[effect->transparencyCurve][sample * initIndex / 8]
                    * 2;
                if (state->factors[initIndex] == 256) {
                    state->factors[initIndex] = 255;
                }
            }
        } else {
            maxFactor = 255;
            for (initIndex = 8; initIndex >= 0; --initIndex) {
                state->factors[initIndex] = maxFactor;
            }
        }
        Effects_ClearParticleTextureState((int*)&state->texture);
        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);
        break;
    case 2: {
        effect = &Effects_package->block5Data->definitions[state->effect];
        scratch->flags = effect->flags;
        actorMatrix = &battle->transforms[effect->transformIndex].world;
        Core_Math_InterpolatePairsToWideVector(effect->positionRange,
            Effects_SampleEffectCurve(effect->positionCurve, state->age), &scratch->position);
        switch (scratch->flags & 7) {
        case 0:
            break;
        case 1:
            Core_Math_AddWideAndShortVectors(&scratch->position, Effects_effectOrigin, &scratch->position);
            break;
        case 2:
            scratch->position.vx += actorMatrix->t[0];
            scratch->position.vy += actorMatrix->t[1];
            scratch->position.vz += actorMatrix->t[2];
            break;
        }
        heightStep = 0;
        Core_Math_InterpolatePairsToWideVector(effect->extentRange,
            Effects_SampleEffectCurve(effect->extentCurve, state->age), &scratch->extent);
        radiusStep = 0;
        sample = Effects_SampleEffectCurve(effect->forceCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[2], sample, &scratch->forceMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[3], sample, &scratch->forceMax);
        row = 0;
        sample = Effects_SampleEffectCurve(effect->accelerationCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[4], sample, &scratch->accelerationMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[5], sample, &scratch->accelerationMax);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[1],
            Effects_SampleEffectCurve(effect->directionJitterCurve, state->age), &scratch->directionJitter);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[2],
            Effects_SampleEffectCurve(effect->speedCurve, state->age), scratch->speedRange);
        state->heightSpeed += scratch->accelerationMin.vy;
        tempHeight = state->height += scratch->forceMin.vy + state->heightSpeed;
        heightValue = tempHeight + scratch->extent.vy;
        state->radiusSpeed += scratch->accelerationMin.vx;
        state->radius += scratch->forceMin.vx + state->radiusSpeed;
        radiusValue = state->radius + scratch->extent.vx;
        for (row = 0; row < 9; ++row) {
            if (row < 5) {
                for (column = 0; column < 16; ++column) {
                    vertices[row][column].vx =
                        scratch->position.vx
                        + ((VS_MIRROR_SURFACE_VERTICES[4 - row][column].vx * radiusValue)
                            >> 12);
                    vertices[row][column].vz =
                        scratch->position.vz
                        + ((VS_MIRROR_SURFACE_VERTICES[4 - row][column].vz * radiusValue)
                            >> 12);
                    vertices[row][column].vy =
                        scratch->position.vy
                        + ((VS_MIRROR_SURFACE_VERTICES[4 - row][column].vy * heightValue)
                            >> 12);
                }
            } else {
                for (column = 0; column < 16; ++column) {
                    vertices[row][column].vx =
                        scratch->position.vx
                        + ((VS_MIRROR_SURFACE_VERTICES[row - 4][column].vx * radiusValue)
                            >> 12);
                    vertices[row][column].vz =
                        scratch->position.vz
                        + ((VS_MIRROR_SURFACE_VERTICES[row - 4][column].vz * radiusValue)
                            >> 12);
                    vertices[row][column].vy =
                        scratch->position.vy
                        + ((-VS_MIRROR_SURFACE_VERTICES[row - 4][column].vy * heightValue)
                            >> 12);
                }
            }
            radiusStep += scratch->accelerationMax.vx;
            radiusValue += radiusStep + scratch->forceMax.vx;
            heightStep += scratch->accelerationMax.vy;
            heightValue += heightStep + scratch->forceMax.vy;
            if (effect->rCurve) {
                colors[row].r = state->factors[row]
                              * Effects_SampleEffectCurve(effect->rCurve, state->age) / 128;
            } else {
                colors[row].r = state->factors[row];
            }
            if (effect->gCurve) {
                colors[row].g = state->factors[row]
                              * Effects_SampleEffectCurve(effect->gCurve, state->age) / 128;
            } else {
                colors[row].g = state->factors[row];
            }
            if (effect->bCurve) {
                colors[row].b = state->factors[row]
                              * Effects_SampleEffectCurve(effect->bCurve, state->age) / 128;
            } else {
                colors[row].b = state->factors[row];
            }
            colors[row].cd = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
        }
        if (scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&battle->transforms[effect->transformIndex].view);
            Core_Gte_SetTransMatrix(&battle->transforms[effect->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }
        for (row = 0; row < 9; ++row) {
            for (column = 0; column < 16; ++column) {
                Core_Gte_LoadV0(&vertices[row][column]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&screen[row][column]);
                Core_Gte_StoreSz3Otz(&depths[row][column]);
            }
        }
        Effects_AdvanceParticleTextureAnimation(&state->texture);
        texture = &Effects_package->textures[state->texture.textureInfo->dataIndex];
        for (sample = 0; sample < VS_MIRROR_SURFACE_U_SEGMENTS + 1; ++sample) {
            u[sample] = texture->u + texture->w * sample / VS_MIRROR_SURFACE_U_SEGMENTS;
        }
        for (sample = 0; sample < 9; ++sample) {
            v[sample] = texture->v + texture->h * sample / 8;
        }
        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 15; ++column) {
                depth = (depths[row][column] + depths[row][column + 1]
                            + depths[row + 1][column] + depths[row + 1][column + 1])
                      / 4;
                if (depth < 0x800 && Render_nearClip < depth) {
                    prim = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = prim + 1;
                    prim->poly.tag = 0xE1000000;
                    prim->drawMode = 0xE1000200;
                    *(int*)&prim->poly.x0 = screen[row][column];
                    *(int*)&prim->poly.x1 = screen[row][column + 1];
                    *(int*)&prim->poly.x2 = screen[row + 1][column];
                    *(int*)&prim->poly.x3 = screen[row + 1][column + 1];
                    *(int*)&prim->poly.r0 = *(int*)&prim->poly.r1 = *(int*)&colors[row];
                    *(int*)&prim->poly.r2 = *(int*)&prim->poly.r3 =
                        *(int*)&colors[row + 1];
                    prim->poly.u0 = prim->poly.u2 = u[VS_MIRROR_SURFACE_U_INDEX(column)];
                    prim->poly.u1 = prim->poly.u3 =
                        u[VS_MIRROR_SURFACE_U_INDEX(column) + 1];
                    prim->poly.v0 = prim->poly.v1 = v[row];
                    prim->poly.v2 = prim->poly.v3 = v[row + 1];
                    prim->poly.tpage = texture->tpage;
                    prim->poly.clut = texture->clut;
                    prim->tag =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0E000000;
                    ((u_long*)Render_frameScratch.orderingTable)[depth] =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                        | (LegacyGpu_Handle(prim) & 0xFFFFFF);
                }
            }
            depth = (depths[row][15] + depths[row][0] + depths[row + 1][15]
                        + depths[row + 1][0])
                  / 4;
            if (depth < 0x800 && Render_nearClip < depth) {
                prim = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = prim + 1;
                prim->poly.tag = 0xE1000000;
                prim->drawMode = 0xE1000200;
                *(int*)&prim->poly.x0 = screen[row][15];
                *(int*)&prim->poly.x1 = screen[row][0];
                *(int*)&prim->poly.x2 = screen[row + 1][15];
                *(int*)&prim->poly.x3 = screen[row + 1][0];
                *(int*)&prim->poly.r0 = *(int*)&prim->poly.r1 = *(int*)&colors[row];
                *(int*)&prim->poly.r2 = *(int*)&prim->poly.r3 = *(int*)&colors[row + 1];
                prim->poly.u0 = prim->poly.u2 = u[VS_MIRROR_SURFACE_U_INDEX(column)];
                prim->poly.u1 = prim->poly.u3 = u[VS_MIRROR_SURFACE_U_INDEX(column) + 1];
                prim->poly.v0 = prim->poly.v1 = v[row];
                prim->poly.v2 = prim->poly.v3 = v[row + 1];
                prim->poly.tpage = texture->tpage;
                prim->poly.clut = texture->clut;
                prim->tag = (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)Render_frameScratch.orderingTable)[depth] =
                    (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                    | (LegacyGpu_Handle(prim) & 0xFFFFFF);
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
    }
    case 3:
        state->lifetime = 1;
        break;
    case 4:
        alive = 0;
        break;
    }
    return alive;
}
#undef VS_MIRROR_SURFACE_TYPES
#undef VS_MIRROR_SURFACE_U_SEGMENTS
#undef VS_MIRROR_SURFACE_U_INDEX

