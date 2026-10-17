// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Core/Math/Interpolation.h"
#include "Core/Memory/Heap.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
#include "Render/Display.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_800EC2E8 (D_800EC2E8@BATTLE/BATTLE.PRG)
CVECTOR Effects_trailAdditiveColors[5473]; /* data: contents loaded in Step 5 */

// decomp: D_800EC308 (D_800EC308@BATTLE/BATTLE.PRG)
CVECTOR Effects_trailColors[5465]; /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_800CDCBC (src/BATTLE/BATTLE.PRG/5BF94.c:4898)
void Effects_DrawTrailSegment(Effects_BoneTrail* t, int head, int tail)
{
    long xy[4];

    int depth = Core_Gte_RotAverage4(&t->previousFirst, &t->currentFirst, &t->previousSecond,
        &t->currentSecond, &xy[0], &xy[1], &xy[2], &xy[3], &t->projectionDepth,
        &t->projectionFlags);
    Render_TrailPacket* p;

    if (depth < 17 || depth >= 2048) {
        return;
    }

    p = Render_frameScratch.primitiveCursor;
    Render_frameScratch.primitiveCursor = p + 1;
    setlen(p, 14);
    p->code = Render_LegacyPrimitiveCode_PolyGt4;
    p->mode = _get_mode(0, 0, 0);
    p->endMode = _get_mode(0, 1, 0);
    p->xy0 = xy[0];
    p->xy1 = xy[1];
    p->xy2 = xy[2];
    p->xy3 = xy[3];

    if ((*(u_int*)&t->config & 0xF00000) == 0x600000) {
        p->r0 = p->r1 = Effects_trailColors[t->config.color].r * head - 128;
        p->g0 = p->g1 = Effects_trailColors[t->config.color].g * head - 128;
        p->blue0 = p->blue1 = Effects_trailColors[t->config.color].b * head - 128;
        p->r2 = p->r3 = Effects_trailColors[t->config.color].r * tail - 128;
        p->g2 = p->g3 = Effects_trailColors[t->config.color].g * tail - 128;
        p->blue2 = p->blue3 = Effects_trailColors[t->config.color].b * tail - 128;
        Effects_MapTrailFramebufferTexture(p, head, head, 0);
        addPrim(Render_frameScratch.orderingTable + depth * 4, p);
    } else {
        u_char column;
        u_char row;
        u_short uv;

        p->code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
        p->tpage = ((*(u_int*)&t->config & 0xF0000) != 0x70000)
                     ? getTPage(0, 1, 512, 256)
                     : getTPage(0, 2, 512, 256);
        p->clut = getClut(768, 240);
        column = t->config.texture % 3U;
        row = t->config.texture / 3U;
        uv = column * 2560 + row * 40;
        p->uv0 = uv + vs_getUV(104, 72);
        p->uv2 = uv + vs_getUV(135, 72);
        p->uv1 = uv + vs_getUV(104, 79);
        p->uv3 = uv + vs_getUV(135, 79);
        p->r0 = p->r1 = (Effects_trailAdditiveColors[t->config.color].r * head) / 8;
        p->g0 = p->g1 = (Effects_trailAdditiveColors[t->config.color].g * head) / 8;
        p->blue0 = p->blue1 = (Effects_trailAdditiveColors[t->config.color].b * head) / 8;
        p->r2 = p->r3 = (Effects_trailAdditiveColors[t->config.color].r * tail) / 8;
        p->g2 = p->g3 = (Effects_trailAdditiveColors[t->config.color].g * tail) / 8;
        p->blue2 = p->blue3 = (Effects_trailAdditiveColors[t->config.color].b * tail) / 8;
        addPrim(Render_frameScratch.orderingTable + depth * 4, p);
    }
}

// decomp: BATTLE.PRG func_800CE174 (src/BATTLE/BATTLE.PRG/5BF94.c:4958)
int Effects_UpdateBoneTrail(Effects_RendererSlot* node, u_int mode, int config)
{
    Effects_BoneTrail* t = node->rendererState;
    int result = 1;

    switch (mode) {
    case 1:
        t = Core_Memory_AllocateFromHighEnd(sizeof *t);
        node->rendererState = t;
        t->index = 0;
        t->age = 0;
        *(int*)&t->config = config;
        break;
    case 2:
        t->index = (t->index + 1) & 7;
        Actors_ActorBonePosition(t->config.actor, t->config.bone, &t->first[t->index], 0);
        Actors_ActorBonePosition(t->config.actor, t->config.bone, &t->second[t->index], 1);

        if (++t->age >= 4) {
            int segments = (t->age < 9) ? t->age - 3 : 5;
            int phase;
            int span;
            int weight;

            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
            Core_Math_InterpolateLagrangeShortVector(&t->first[t->index],
                &t->first[(t->index - 1) & 7], &t->first[(t->index - 2) & 7],
                &t->first[(t->index - 3) & 7], -ONE, &t->previousFirst);
            Core_Math_InterpolateLagrangeShortVector(&t->second[t->index],
                &t->second[(t->index - 1) & 7], &t->second[(t->index - 2) & 7],
                &t->second[(t->index - 3) & 7], -ONE, &t->previousSecond);
            weight = (segments + 1) * 2;

            for (span = -ONE / 2; span < 0; span += ONE / 2) {
                Core_Math_InterpolateLagrangeShortVector(&t->first[t->index],
                    &t->first[(t->index - 1) & 7], &t->first[(t->index - 2) & 7],
                    &t->first[(t->index - 3) & 7], span, &t->currentFirst);
                Core_Math_InterpolateLagrangeShortVector(&t->second[t->index],
                    &t->second[(t->index - 1) & 7], &t->second[(t->index - 2) & 7],
                    &t->second[(t->index - 3) & 7], span, &t->currentSecond);
                Effects_DrawTrailSegment(t, weight / 2, (weight - 1) / 2);
                --weight;
                t->previousFirst = t->currentFirst;
                t->previousSecond = t->currentSecond;
            }

            for (span = 0; span < segments; ++span) {
                for (phase = 0; phase < ONE; phase += ONE / 2) {
                    Core_Math_InterpolateLagrangeShortVector(&t->first[(t->index - span) & 7],
                        &t->first[(t->index - span - 1) & 7],
                        &t->first[(t->index - span - 2) & 7],
                        &t->first[(t->index - span - 3) & 7], phase, &t->currentFirst);
                    Core_Math_InterpolateLagrangeShortVector(&t->second[(t->index - span) & 7],
                        &t->second[(t->index - span - 1) & 7],
                        &t->second[(t->index - span - 2) & 7],
                        &t->second[(t->index - span - 3) & 7], phase, &t->currentSecond);
                    Effects_DrawTrailSegment(t, weight / 2, (weight - 1) / 2);
                    --weight;
                    t->previousFirst = t->currentFirst;
                    t->previousSecond = t->currentSecond;
                }
            }
        }

        if (t->config.life != 255) {
            --t->config.life;
        }

        if (t->config.life == 0) {
            result = 0;
        }

        break;
    case 3:
        t->config.life = 8;
        break;
    case 4:
        Core_Memory_FreeFromHighEnd(t);
        node->rendererState = NULL;
        result = 0;
        break;
    }

    return result;
}

// decomp: BATTLE.PRG func_800CF7A8 (src/BATTLE/BATTLE.PRG/5BF94.c:5666)
void Effects_SpawnBoneTrail(int arg0, int arg1, int arg2, int arg3)
{
    Effects_ActionEffectRequest request;

    request.effectId = 54;
    request.count = 1;
    request.source.type = 5;
    request.targets[0].type = 5;
    setVector(&request.source.value.position, 0, 0, 0);
    setVector(&request.targets[0].value.position, 0, 0, 0);
    request.otherCount = 0;
    Effects_BeginSecondaryEffects(&request, (arg0 & 0xFF) | ((arg1 & 0xFF) << 8) | ((arg2 & 0xF) << 0x10)
                                | ((arg3 & 0xF) << 0x14) | 0xFF000000);
}

// decomp: BATTLE.PRG func_800CF830 (src/BATTLE/BATTLE.PRG/5BF94.c:5681)
void Effects_FadeBoneTrails(int arg0, int arg1)
{
    Effects_EffectInstance* var_s1 = Effects_secondaryInstances;

    while (var_s1 != NULL) {
        Effects_RendererSlot* var_s0 = var_s1->context.renderer;
        while (var_s0 != NULL) {
            if (var_s0->renderer == Effects_UpdateBoneTrail) {
                var_s0->renderer(var_s0, 3, 0);
            }
            var_s0 = var_s0->next;
        }
        var_s1 = var_s1->next;
    }
}

// decomp: BATTLE.PRG func_800CF92C (src/BATTLE/BATTLE.PRG/5BF94.c:5714)
void Effects_ClampTrailTexturePoint(int arg0, int arg1, int arg2, u_short* arg3)
{
    int var_v1;
    int temp_a1;

    var_v1 = (short)arg2 + arg0;

    if (var_v1 <= 0) {
        var_v1 = 1;
    } else if (var_v1 >= 0xFF) {
        var_v1 = 0xFE;
    }

    temp_a1 = (arg2 >> 16) + arg1;

    if (temp_a1 <= 0) {
        temp_a1 = 1;
    } else if (temp_a1 >= 0xFF) {
        temp_a1 = 0xFE;
    }

    *arg3 = var_v1 | temp_a1 << 8;
}

// decomp: BATTLE.PRG func_800CF988 (src/BATTLE/BATTLE.PRG/5BF94.c:5738)
void Effects_MapTrailFramebufferTexture(Render_TrailPacket* arg0, int arg1, int arg2, int arg3)
{
    int page;
    int offset;
    int left = 320;

    if ((short)arg0->xy0 < left) {
        left = (short)arg0->xy0;
    }

    if ((short)arg0->xy1 < left) {
        left = (short)arg0->xy1;
    }

    if ((short)arg0->xy2 < left) {
        left = (short)arg0->xy2;
    }

    if ((short)arg0->xy3 < left) {
        left = (short)arg0->xy3;
    }

    left &= ~0x3F;
    page = left / 64;
    arg0->tpage = ((Render_frameBuf & 1) ? page : page + 5) | arg3 | 0x100;
    offset = arg1 - left;
    Effects_ClampTrailTexturePoint(offset, arg2, arg0->xy0, &arg0->uv0);
    Effects_ClampTrailTexturePoint(offset, arg2, arg0->xy1, &arg0->uv1);
    Effects_ClampTrailTexturePoint(offset, arg2, arg0->xy2, &arg0->uv2);
    Effects_ClampTrailTexturePoint(offset, arg2, arg0->xy3, &arg0->uv3);
}

