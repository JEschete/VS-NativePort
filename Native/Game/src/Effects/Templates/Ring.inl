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

// Template body: Effects::Templates::Ring (decomp src/EFFECT/rings.h:26). Included once per instance by Effects/Templates/Ring.c with that plugin's bindings.
#ifndef VS_RING_TYPES
#define VS_RING_TYPES
#endif

int VS_RING_FUNCTION(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    int radius[3];
    int height[3];
    SVECTOR vertices[3][9];
    int screen[3][9];
    int depths[3][9];
    int factors[3];
    CVECTOR colors[3];
    u_char u[9];
    u_char v[3];
    Effects_PluginEffectState child;
    Effects_EffectInstance* battle;
    int alive;
    MATRIX* actorMatrix;
    int fraction;
    int radiusValue;
    int baseAngle;
    u_char* factorBytes;
    int baseAngleCopy;
    int angle;
    int angleA, angleB;
    int cosAngle;
    int sinAngle;
    int speed;
    int maxSpeed;
    int row;
    intptr_t column;
    int depth;
    int drawRow;
    int drawColumn;
    int* factor;
    u_char* colorStart;
    u_char* color;
    Render_RingPacket* prim;
    Effects_PackageTexture* texture;
    Effects_ParticleDefinition* effect;
    Effects_RingState* state;
    Effects_ParticleEmitterScratch* scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);

    alive = 1;
    state = arg0->rendererState;
    battle = Effects_currentInstance;
    switch (arg1) {
    case 1:
        state = Core_Memory_AllocateFromHighEnd(sizeof *state);
        arg0->rendererState = state;
        state->effect = arg2 >> 8;
        state->lifetime = arg2;
        state->radiusSpeed = 0;
        state->radius = 0;
        state->heightSpeed = 0;
        state->height = 0;
        state->angle = 0;
        state->age = 0;
        effect = &Effects_package->block5Data->definitions[state->effect];
        Effects_ClearParticleTextureState((int*)&state->texture);
        Effects_InitializeParticleTextureAnimation(&state->texture, effect->textureIndex, effect->animationId);
        break;
    case 2: {
        effect = &Effects_package->block5Data->definitions[state->effect];
        scratch->flags = effect->flags;
        actorMatrix = &battle->transforms[effect->transformIndex].world;
        effect = &Effects_package->block5Data->definitions[state->effect];
        fraction = Core_Math_InterpolateShortPair(
            effect->innerExtentRange, Effects_SampleEffectCurve(effect->innerExtentCurve, state->age));
        column = ((u_char*)effect)[7];
        factorBytes = (u_char*)VS_RING_COLORS + (column & 0x1C);
        factors[0] = factorBytes[0];
        factors[1] = factorBytes[1];
        factors[2] = factorBytes[2];
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
        Core_Math_InterpolatePairsToWideVector(effect->extentRange,
            Effects_SampleEffectCurve(effect->extentCurve, state->age), &scratch->extent);
        column = Effects_SampleEffectCurve(effect->forceCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[2], column, &scratch->forceMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[3], column, &scratch->forceMax);
        column = Effects_SampleEffectCurve(effect->accelerationCurve, state->age);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[4], column, &scratch->accelerationMin);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[5], column, &scratch->accelerationMax);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[0],
            Effects_SampleEffectCurve(effect->directionCurve, state->age), &scratch->baseDirection);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[2],
            Effects_SampleEffectCurve(effect->speedCurve, state->age), scratch->speedRange);
        state->radiusSpeed += scratch->accelerationMin.vx;
        radiusValue = state->radius += scratch->forceMin.vx + state->radiusSpeed;
        height[0] = 0;
        radius[0] = radiusValue + scratch->extent.vx;
        state->heightSpeed += scratch->accelerationMin.vy;
        state->height += scratch->forceMin.vy + state->heightSpeed;
        radius[2] = state->height + scratch->extent.vy;
        column = Core_Gte_Sin(scratch->baseDirection.vx);
        height[2] = (radius[2] * column) >> 12;
        radius[1] = (radius[2] * fraction) >> 12;
        height[1] = (radius[1] * column) >> 12;
        row = Core_Gte_Cos(scratch->baseDirection.vx);
        radius[1] = ((radius[1] * row) >> 12) + radius[0];
        radius[2] = ((radius[2] * row) >> 12) + radius[0];
        do {
            speed = scratch->speedRange[0];
            maxSpeed = scratch->speedRange[1];
        } while (0);
        row = state->angle;
        if (speed != maxSpeed) {
            if (maxSpeed < speed) {
                speed = Core_Random_Rand() % (scratch->speedRange[0] - scratch->speedRange[1] + 1)
                      + scratch->speedRange[1];
            } else {
                speed = Core_Random_Rand() % (scratch->speedRange[1] - scratch->speedRange[0] + 1)
                      + scratch->speedRange[0];
            }
        }
        maxSpeed = row + speed;
        baseAngle = state->angle = maxSpeed;
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[1],
            Effects_SampleEffectCurve(effect->directionJitterCurve, state->age), &scratch->directionJitter);
        column = 0;
        do {
            baseAngleCopy = baseAngle;
            angleA = baseAngleCopy + (scratch->directionJitter.vx * column) / 8;
            angleB = -scratch->directionJitter.vy / 2 + ((angle = scratch->directionJitter.vy) * column) / 8;
            angle = angleA + angleB;
            cosAngle = Core_Gte_Cos(angle);
            sinAngle = Core_Gte_Sin(angle);
            for (row = 0; row < 3; ++row) {
                vertices[row][column].vx =
                    scratch->position.vx + ((radius[row] * cosAngle) >> 12);
                vertices[row][column].vz =
                    scratch->position.vz + ((radius[row] * sinAngle) >> 12);
                vertices[row][column].vy = height[row] + scratch->position.vy;
            }
            ++column;
        } while (column < 9);
        colorStart = (u_char*)colors;
        color = colorStart;
        factor = &factors[0];
        do {
            if (effect->rCurve) {
                maxSpeed =
                    *factor * Effects_SampleEffectCurve(effect->rCurve, state->age) / 128;
            } else {
                maxSpeed = (u_char)*factor;
            }
            color[0] = maxSpeed;
            if (effect->gCurve) {
                maxSpeed =
                    *factor * Effects_SampleEffectCurve(effect->gCurve, state->age) / 128;
            } else {
                maxSpeed = (u_char)*factor;
            }
            color[1] = maxSpeed;
            if (effect->bCurve) {
                maxSpeed =
                    *factor * Effects_SampleEffectCurve(effect->bCurve, state->age) / 128;
            } else {
                maxSpeed = (u_char)*factor;
            }
            color[2] = maxSpeed;
            {
                *(color++ + 3) = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                color += 3;
            }
            ++factor;
        } while ((column = (intptr_t)color) < (intptr_t)(colorStart + 12));
        if (scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&battle->transforms[effect->transformIndex].view);
            Core_Gte_SetTransMatrix(&battle->transforms[effect->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }
        for (drawRow = 0; drawRow < 3; ++drawRow) {
            for (drawColumn = 0; drawColumn < 9; ++drawColumn) {
                Core_Gte_LoadV0(&vertices[drawRow][drawColumn]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&screen[drawRow][drawColumn]);
                Core_Gte_StoreSz3Otz(&depths[drawRow][drawColumn]);
            }
        }
        Effects_AdvanceParticleTextureAnimation(&state->texture);
        texture = &Effects_package->textures[state->texture.textureInfo->dataIndex];
#ifdef VS_RING_TRANSPOSED
        for (column = 0; column < 9; ++column) {
            u[column] = texture->v + texture->h * column / 8;
        }
        v[0] = texture->u;
        v[2] = v[0] + texture->w;
        v[1] = texture->u + ((texture->w * fraction) >> 12);
#else
        for (column = 0; column < 9; ++column) {
            u[column] = texture->u + texture->w * column / 8;
        }
        v[0] = texture->v;
        v[2] = v[0] + texture->h;
        v[1] = texture->v + ((texture->h * fraction) >> 12);
#endif
        for (drawRow = 0; drawRow < 2; ++drawRow) {
            for (drawColumn = 0; drawColumn < 8; ++drawColumn) {
                depth = (depths[drawRow][drawColumn] + depths[drawRow][drawColumn + 1]
                            + depths[drawRow + 1][drawColumn]
                            + depths[drawRow + 1][drawColumn + 1])
                      / 4;
                if (depth < 0x800 && Render_nearClip < depth) {
                    prim = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = prim + 1;
                    prim->poly.tag = 0xE1000000;
                    prim->drawMode = 0xE1000200;
                    *(int*)&prim->poly.x0 = screen[drawRow][drawColumn];
#ifdef VS_RING_TRANSPOSED
                    *(int*)&prim->poly.x2 = screen[drawRow][drawColumn + 1];
                    *(int*)&prim->poly.x1 = screen[drawRow + 1][drawColumn];
#else
                    *(int*)&prim->poly.x1 = screen[drawRow][drawColumn + 1];
                    *(int*)&prim->poly.x2 = screen[drawRow + 1][drawColumn];
#endif
                    *(int*)&prim->poly.x3 = screen[drawRow + 1][drawColumn + 1];
#ifdef VS_RING_TRANSPOSED
                    *(int*)&prim->poly.r0 = *(int*)&prim->poly.r2 =
                        *(int*)&colors[drawRow];
                    *(int*)&prim->poly.r1 = *(int*)&prim->poly.r3 =
                        *(int*)&colors[drawRow + 1];
                    prim->poly.u0 = prim->poly.u2 = v[drawRow];
                    prim->poly.u1 = prim->poly.u3 = v[drawRow + 1];
                    prim->poly.v0 = prim->poly.v1 = u[drawColumn];
                    prim->poly.v2 = prim->poly.v3 = u[drawColumn + 1];
#else
                    *(int*)&prim->poly.r0 = *(int*)&prim->poly.r1 =
                        *(int*)&colors[drawRow];
                    *(int*)&prim->poly.r2 = *(int*)&prim->poly.r3 =
                        *(int*)&colors[drawRow + 1];
                    prim->poly.u0 = prim->poly.u2 = u[drawColumn];
                    prim->poly.u1 = prim->poly.u3 = u[drawColumn + 1];
                    prim->poly.v0 = prim->poly.v1 = v[drawRow];
                    prim->poly.v2 = prim->poly.v3 = v[drawRow + 1];
#endif
                    prim->poly.tpage = texture->tpage;
                    prim->poly.clut = texture->clut;
                    prim->tag =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0E000000;
                    ((u_long*)Render_frameScratch.orderingTable)[depth] =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                        | (LegacyGpu_Handle(prim) & 0xFFFFFF);
                }
            }
        }
        if (effect->tickEvent) {
            child.transparency = 0;
            if (effect->flags & 0x20000) {
                Core_Gte_SetRotMatrix(actorMatrix);
                Core_Gte_SetTransMatrix(actorMatrix);
                setVector(&scratch->vector, scratch->position.vx, scratch->position.vy,
                    scratch->position.vz);
                Core_Gte_LoadV0(&scratch->vector);
                Core_Gte_Rtv0Tr();
                Core_Gte_StoreLvnl(&scratch->aimPoint);
                setVector(&child.position, scratch->aimPoint.vx * ONE, scratch->aimPoint.vy * ONE,
                    scratch->aimPoint.vz * ONE);
            } else {
                setVector(&child.position, scratch->position.vx * ONE, scratch->position.vy * ONE,
                    scratch->position.vz * ONE);
            }
            Effects_EmitParticleEvent(battle, effect->tickEvent - 1, 0, 0, &child);
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
#undef VS_RING_TYPES

