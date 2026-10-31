// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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

// Template body: Effects::Templates::TiledRing (decomp src/EFFECT/tiledRings.h:35). Included once per instance by Effects/Templates/TiledRing.cpp with that plugin's bindings.
int VS_TILED_RING_FUNCTION(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters)
{
    int radius[3];
    int height[3];
    SVECTOR vertices[3][16];
    int screen[3][16];
    int depths[3][16];
    int factors[3];
    CVECTOR colors[3];
    Render_TiledRingTexCoord uv[2][4];
    MATRIX unusedTransform;
    Effects_PluginEffectState child;
    Effects_TiledRingState* state = (Effects_TiledRingState *)(renderer_slot->rendererState);
    Effects_ParticleEmitterScratch* scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* effect;
    Effects_EffectInstance* battle = Effects_currentInstance;
    int alive = 1;
    MATRIX* actorMatrix;
    int fraction;
    int radiusValue;
    int baseAngle;
    CVECTOR* factorBytes;
    int cosAngle;
    int sinAngle;
    int angular_step;
    int maxSpeed;
    int row;
    int oldAngle;
    int column;
    int depth;
    int drawRow;
    int drawColumn;
    Render_TiledRingPacket* prim;
    Effects_PackageTexture* texture;
    switch (lifecycle_command) {
    case 1:
        state = (Effects_TiledRingState *)(Core_Memory_AllocateFromHighEnd(sizeof(*state)));
        renderer_slot->rendererState = state;
        state->lifetime = packed_parameters;
        state->effect = packed_parameters >> 8;
        state->radiusSpeed = 0;
        state->radius = 0;
        state->heightSpeed = 0;
        state->height = 0;
        state->angle = 0;
        state->age = 0;
        effect = &Effects_package->block5Data->definitions[state->effect];
        Effects_ClearParticleTextureState((int*)&state->texture);
        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);
        break;

    case 2:
        effect = &Effects_package->block5Data->definitions[state->effect];
        scratch->flags = effect->flags;
        actorMatrix = &battle->transforms[effect->transformIndex].world;
        effect = &Effects_package->block5Data->definitions[state->effect];
        column = Effects_SampleEffectCurve(effect->innerExtentCurve, state->age);
        fraction = Core_Math_InterpolateShortPair(effect->innerExtentRange, column);
        column = effect->transparencyCurve & 0x7;
        factorBytes = &VS_TILED_RING_COLORS[column];
        factors[0] = factorBytes->r;
        factors[1] = factorBytes->g;
        factors[2] = factorBytes->b;
        Core_Math_InterpolatePairsToWideVector(effect->positionRange,
            Effects_SampleEffectCurve(effect->positionCurve, state->age), &scratch->position);
        switch (scratch->flags & 7) {
        case 0:
            break;

        case 1:
            Core_Math_AddWideAndShortVectors(&scratch->position, Effects_effectOrigin, &scratch->position);
            break;

        case 2:
            applyVector(&scratch->position, actorMatrix->t[0], actorMatrix->t[1],
                actorMatrix->t[2], +=);
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
        oldAngle = state->angle;
        if (scratch->speedRange[0] != scratch->speedRange[1]) {
            if (scratch->speedRange[1] < scratch->speedRange[0]) {
                angular_step = (Core_Random_Rand() % ((scratch->speedRange[0] - scratch->speedRange[1]) + 1))
                      + scratch->speedRange[1];
            } else {
                angular_step = (Core_Random_Rand() % ((scratch->speedRange[1] - scratch->speedRange[0]) + 1))
                      + scratch->speedRange[0];
            }
        } else {
            angular_step = scratch->speedRange[0];
        }
        maxSpeed = oldAngle + angular_step;
        baseAngle = state->angle = maxSpeed;
        for (column = 0; column < 4; ++column) {
            for (row = 0; row < 3; ++row) {
                cosAngle = (radius[row] * Core_Gte_Cos(baseAngle + (column * 256))) >> 12;
                vertices[row][column + 12].vz = vertices[row][column + 8].vx =
                    -(vertices[row][column + 4].vz = vertices[row][column].vx = cosAngle);
                sinAngle = (radius[row] * Core_Gte_Sin(baseAngle + (column * 256))) >> 12;
                vertices[row][column].vz = sinAngle;
                vertices[row][column + 12].vx = sinAngle;
                vertices[row][column + 8].vz = -sinAngle;
                vertices[row][column + 4].vx = -sinAngle;
                vertices[row][column].vy = height[row];
                vertices[row][column + 4].vy = height[row];
                vertices[row][column + 8].vy = height[row];
                vertices[row][column + 12].vy = height[row];
            }
        }

        for (drawRow = 0; drawRow < 3; ++drawRow) {
            for (drawColumn = 0; drawColumn < 16; ++drawColumn) {
                vertices[drawRow][drawColumn].vx += scratch->position.vx;
                vertices[drawRow][drawColumn].vy += scratch->position.vy;
                vertices[drawRow][drawColumn].vz += scratch->position.vz;
            }

            if (effect->rCurve) {
                colors[drawRow].r =
                    (factors[drawRow] * Effects_SampleEffectCurve(effect->rCurve, state->age))
                    / 128;
            } else {
                colors[drawRow].r = factors[drawRow];
            }
            if (effect->gCurve) {
                colors[drawRow].g =
                    (factors[drawRow] * Effects_SampleEffectCurve(effect->gCurve, state->age))
                    / 128;
            } else {
                colors[drawRow].g = factors[drawRow];
            }
            if (effect->bCurve) {
                colors[drawRow].b =
                    (factors[drawRow] * Effects_SampleEffectCurve(effect->bCurve, state->age))
                    / 128;
            } else {
                colors[drawRow].b = factors[drawRow];
            }
            colors[drawRow].cd = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
        }

        if (scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&battle->transforms[effect->transformIndex].view);
            Core_Gte_SetTransMatrix(&battle->transforms[effect->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }
        for (drawRow = 0; drawRow < 3; ++drawRow) {
            for (drawColumn = 0; drawColumn < 16; ++drawColumn) {
                Core_Gte_LoadV0(&vertices[drawRow][drawColumn]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2((long *)(&screen[drawRow][drawColumn]));
                Core_Gte_StoreSz3Otz((long *)(&depths[drawRow][drawColumn]));
            }
        }

        Effects_AdvanceParticleTextureAnimation(&state->texture);
        texture = &Effects_package->textures[state->texture.textureInfo->dataIndex];
        uv[0][0].u = uv[0][2].u = uv[1][0].u = uv[1][2].u = texture->u;
        uv[0][1].u = uv[0][3].u = uv[1][1].u = uv[1][3].u = uv[0][0].u + texture->w;
        uv[0][0].v = uv[0][1].v = texture->v;
        uv[0][2].v = uv[0][3].v = uv[1][0].v = uv[1][1].v =
            uv[0][0].v + ((texture->h * fraction) >> 12);
        uv[1][2].v = uv[1][3].v = texture->v + texture->h;
        for (drawRow = 0; drawRow < 2; ++drawRow) {
            for (drawColumn = 0; drawColumn < 15; ++drawColumn) {
                depth = (depths[drawRow][drawColumn] + depths[drawRow][drawColumn + 1]
                            + depths[drawRow + 1][drawColumn]
                            + depths[drawRow + 1][drawColumn + 1])
                      / 4;
                if (depth < 0x800 && Render_nearClip < depth) {
                    prim = (Render_TiledRingPacket *)(Render_frameScratch.primitiveCursor);
                    Render_frameScratch.primitiveCursor = prim + 1;
                    prim->poly.tag = _get_mode(0, 0, 0);
                    prim->drawMode = _get_mode(0, 1, 0);
                    *(int*)&prim->poly.x0 = screen[drawRow][drawColumn];
                    *(int*)&prim->poly.x1 = screen[drawRow][drawColumn + 1];
                    *(int*)&prim->poly.x2 = screen[drawRow + 1][drawColumn];
                    *(int*)&prim->poly.x3 = screen[drawRow + 1][drawColumn + 1];
                    *(int*)&prim->poly.r0 = *(int*)&prim->poly.r1 =
                        *(int*)&colors[drawRow];
                    *(int*)&prim->poly.r2 = *(int*)&prim->poly.r3 =
                        *(int*)&colors[drawRow + 1];
                    *(u_short*)&prim->poly.u0 = *(u_short*)&uv[drawRow][0];
                    *(u_short*)&prim->poly.u1 = *(u_short*)&uv[drawRow][1];
                    *(u_short*)&prim->poly.u2 = *(u_short*)&uv[drawRow][2];
                    *(u_short*)&prim->poly.u3 = *(u_short*)&uv[drawRow][3];
                    prim->poly.tpage = texture->tpage;
                    prim->poly.clut = texture->clut;
                    prim->tag =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0E000000;
                    ((u_long*)Render_frameScratch.orderingTable)[depth] =
                        (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                        | ((LegacyGpu_Handle(prim)) & 0xFFFFFF);
                }
            }

            depth = (depths[drawRow][15] + depths[drawRow][0] + depths[drawRow + 1][15]
                        + depths[drawRow + 1][0])
                  / 4;
            if (depth < 0x800 && Render_nearClip < depth) {
                prim = (Render_TiledRingPacket *)(Render_frameScratch.primitiveCursor);
                Render_frameScratch.primitiveCursor = prim + 1;
                prim->poly.tag = _get_mode(0, 0, 0);
                prim->drawMode = _get_mode(0, 1, 0);
                *(int*)&prim->poly.x0 = screen[drawRow][15];
                *(int*)&prim->poly.x1 = screen[drawRow][0];
                *(int*)&prim->poly.x2 = screen[drawRow + 1][15];
                *(int*)&prim->poly.x3 = screen[drawRow + 1][0];
                *(int*)&prim->poly.r0 = *(int*)&prim->poly.r1 = *(int*)&colors[drawRow];
                *(int*)&prim->poly.r2 = *(int*)&prim->poly.r3 =
                    *(int*)&colors[drawRow + 1];
                *(u_short*)&prim->poly.u0 = *(u_short*)&uv[drawRow][0];
                *(u_short*)&prim->poly.u1 = *(u_short*)&uv[drawRow][1];
                *(u_short*)&prim->poly.u2 = *(u_short*)&uv[drawRow][2];
                *(u_short*)&prim->poly.u3 = *(u_short*)&uv[drawRow][3];
                prim->poly.tpage = texture->tpage;
                prim->poly.clut = texture->clut;
                prim->tag = (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)Render_frameScratch.orderingTable)[depth] =
                    (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                    | ((LegacyGpu_Handle(prim)) & 0xFFFFFF);
            }
        }

        if (effect->tickEvent) {
            child.transparency = 0;
            if (effect->flags & 0x20000) {
                Core_Gte_SetRotMatrix(actorMatrix);
                Core_Gte_SetTransMatrix(actorMatrix);
                copyVector(&scratch->vector, &scratch->position);
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

    case 3:
        state->lifetime = 1;
        break;

    case 4:
        alive = 0;
        break;
    }

    return alive;
}

}  // extern "C"
