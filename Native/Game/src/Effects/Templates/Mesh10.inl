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

// Template body: Effects::Templates::Mesh10 (decomp src/EFFECT/meshes10.h:29). Included once per instance by Effects/Templates/Mesh10.c with that plugin's bindings.
#ifndef VS_MESH10_TYPES
#define VS_MESH10_TYPES
#endif

int VS_MESH10_FUNCTION(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    SVECTOR vertices[10];
    int screen[10];
    int depths[10];
    CVECTOR colors[2];
    Render_Mesh10TexCoord uv[6];
    Effects_PluginEffectState child;
    Effects_Mesh10State* state;
    Effects_ParticleEmitterScratch* scratch = (void*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* effect;
    Effects_EffectInstance* battle;
    int alive;
    MATRIX* actorMatrix;
    int color, index, depth;
    Effects_PackageTexture* texture;
    u_char* face;
    Render_Mesh10Packet* prim;
    alive = 1;
    state = arg0->rendererState;
    battle = Effects_currentInstance;
    switch (arg1) {
    case 1:
        state = Core_Memory_AllocateFromHighEnd(sizeof *state);
        arg0->rendererState = state;
        state->effect = arg2 >> 8;
        state->lifetime = arg2;
        *(int*)&state->rotation.vx = 0;
        *(int*)&state->rotation.vz = 0;
        state->age = 0;
        effect = &Effects_package->block5Data->definitions[state->effect];
        Effects_ClearParticleTextureState((int*)&state->texture);
        Effects_InitializeParticleTextureAnimation(&state->texture, effect->textureIndex, effect->animationId);
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
        Core_Math_InterpolatePairsToWideVector(effect->extentRange,
            Effects_SampleEffectCurve(effect->extentCurve, state->age), &scratch->extent);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[7],
            Effects_SampleEffectCurve(effect->attractionJitterCurve, state->age), &scratch->attractionJitter);
        if (effect->rCurve) {
            index = Effects_SampleEffectCurve(effect->rCurve, state->age) * 2;
            color = index;
            if (color >= 256) {
                color = 255;
            }
            colors[0].r = color;
        } else {
            colors[0].r = 128;
        }
        if (effect->gCurve) {
            index = Effects_SampleEffectCurve(effect->gCurve, state->age) * 2;
            color = index;
            if (color >= 256) {
                color = 255;
            }
            colors[0].g = color;
        } else {
            colors[0].g = 128;
        }
        if (effect->bCurve) {
            index = Effects_SampleEffectCurve(effect->bCurve, state->age) * 2;
            color = index;
            if (color >= 256) {
                color = 255;
            }
            colors[0].b = color;
        } else {
            colors[0].b = 128;
        }
        if (effect->vectorRanges[2][0]) {
            index = Effects_SampleEffectCurve(effect->vectorRanges[2][0], state->age) * 2;
            color = index;
            if (index >= 256) {
                color = 255;
            }
            colors[1].r = color;
        } else {
            colors[1].r = 128;
        }
        if (effect->vectorRanges[2][2]) {
            index = Effects_SampleEffectCurve(effect->vectorRanges[2][2], state->age) * 2;
            color = index;
            if (index >= 256) {
                color = 255;
            }
            colors[1].g = color;
        } else {
            colors[1].g = 128;
        }
        if (effect->vectorRanges[2][4]) {
            index = Effects_SampleEffectCurve(effect->vectorRanges[2][4], state->age) * 2;
            color = index;
            if (index >= 256) {
                color = 255;
            }
            colors[1].b = color;
        } else {
            colors[1].b = 128;
        }
        do {
            colors[0].cd = 0x36;
            colors[1].cd = 0x36;
            for (index = 0; index < 4; ++index) {
                vertices[index].vx =
                    (VS_MESH10_VERTICES[index].vx * scratch->extent.vx) >> 12;
                vertices[index].vy =
                    (VS_MESH10_VERTICES[index].vy * scratch->extent.vy) >> 12;
                vertices[index].vz =
                    (VS_MESH10_VERTICES[index].vz * scratch->extent.vz) >> 12;
            }
        } while (0);
        for (index = 4; index < 10; ++index) {
            vertices[index].vx =
                (VS_MESH10_VERTICES[index].vx * scratch->attractionJitter.vx) >> 12;
            vertices[index].vy =
                (VS_MESH10_VERTICES[index].vy * scratch->attractionJitter.vy) >> 12;
            vertices[index].vz =
                (VS_MESH10_VERTICES[index].vz * scratch->attractionJitter.vz) >> 12;
        }
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[0],
            Effects_SampleEffectCurve(effect->directionCurve, state->age), &scratch->baseDirection);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[1],
            Effects_SampleEffectCurve(effect->directionJitterCurve, state->age), &scratch->directionJitter);
        scratch->vector.vx = (state->rotation.vx += scratch->directionJitter.vx) + scratch->baseDirection.vx;
        scratch->vector.vy = (state->rotation.vy += scratch->directionJitter.vy) + scratch->baseDirection.vy;
        scratch->vector.vz = (state->rotation.vz += scratch->directionJitter.vz) + scratch->baseDirection.vz;
        Core_Gte_RotMatrix(&scratch->vector, &scratch->aimMatrix);
        scratch->aimMatrix.t[0] = scratch->position.vx;
        scratch->aimMatrix.t[1] = scratch->position.vy;
        scratch->aimMatrix.t[2] = scratch->position.vz;
        Core_Gte_SetRotMatrix(&scratch->aimMatrix);
        Core_Gte_SetTransMatrix(&scratch->aimMatrix);
        for (index = 0; index < 10; ++index) {
            Core_Gte_LoadV0(&vertices[index]);
            Core_Gte_Rtv0Tr();
            Core_Gte_StoreIr(&vertices[index]);
        }
        if (scratch->flags & 0x20000) {
            Core_Gte_SetRotMatrix(&battle->transforms[effect->transformIndex].view);
            Core_Gte_SetTransMatrix(&battle->transforms[effect->transformIndex].view);
        } else {
            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
        }
        for (index = 0; index < 10; ++index) {
            Core_Gte_LoadV0(&vertices[index]);
            Core_Gte_Rtps();
            Core_Gte_StoreSxy2(&screen[index]);
            Core_Gte_StoreSz3Otz(&depths[index]);
        }
        Effects_AdvanceParticleTextureAnimation(&state->texture);
        texture = &Effects_package->textures[state->texture.textureInfo->dataIndex];
        uv[1].u = texture->u + texture->w / 4;
        uv[2].u = texture->u + texture->w * 3 / 4;
        uv[0].u = uv[4].u = texture->u + texture->w / 2;
        uv[3].u = texture->u;
        uv[5].u = uv[3].u + texture->w;
        uv[1].v = uv[2].v = texture->v + texture->h / 2;
        uv[0].v = texture->v;
        uv[3].v = uv[4].v = uv[5].v = uv[0].v + texture->h;
        for (index = 0; index < 16; ++index) {
            depth = (depths[VS_MESH10_FACES[index][0]] + depths[VS_MESH10_FACES[index][1]]
                        + depths[VS_MESH10_FACES[index][2]])
                  / 3;
            if (depth < 0x800 && Render_nearClip < depth) {
                prim = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = prim + 1;
                prim->poly.tag = 0xE1000000;
                prim->drawMode = 0xE1000200;
                *(int*)&prim->poly.x0 = screen[VS_MESH10_FACES[index][0]];
                *(int*)&prim->poly.x1 = screen[VS_MESH10_FACES[index][1]];
                *(int*)&prim->poly.x2 = screen[VS_MESH10_FACES[index][2]];
                *(int*)&prim->poly.r0 =
                    *(int*)&colors[VS_MESH10_VERTICES[VS_MESH10_FACES[index][0]].pad];
                *(int*)&prim->poly.r1 =
                    *(int*)&colors[VS_MESH10_VERTICES[VS_MESH10_FACES[index][1]].pad];
                *(int*)&prim->poly.r2 =
                    *(int*)&colors[VS_MESH10_VERTICES[VS_MESH10_FACES[index][2]].pad];
                *(u_short*)&prim->poly.u0 =
                    *(u_short*)((u_char*)uv + VS_MESH10_FACES[index][3]);
                *(u_short*)&prim->poly.u1 =
                    *(u_short*)((u_char*)uv + VS_MESH10_FACES[index][4]);
                *(u_short*)&prim->poly.u2 =
                    *(u_short*)((u_char*)uv + VS_MESH10_FACES[index][5]);
                prim->poly.tpage = texture->tpage;
                prim->poly.clut = texture->clut;
                prim->tag = (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFFFFFF) | 0x0B000000;
                ((u_long*)Render_frameScratch.orderingTable)[depth] =
                    (((u_long*)Render_frameScratch.orderingTable)[depth] & 0xFF000000)
                    | ((u_long)prim & 0xFFFFFF);
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
#undef VS_MESH10_TYPES

