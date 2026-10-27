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

void VS_TRAIL_RENDER_FUNCTION(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_TrailQuadSet* trail_set)
{
    Render_EffectGouraudQuadPacket strip_packet;
    DVECTOR screen_points[6];
    int projected_depths[6];
    int segment_normal_angles[4];
    CVECTOR sampled_color;
    int _[6] __attribute__((unused));
    DVECTOR* screen_point_base;
    int i;
    int texture_v_start;
    int texture_v_end;
    Effects_Trail* trail;
    int normal_offset_y;
    int temp_s0;
    int joint_normal_angle;
    int j;
    Render_EffectGouraudQuadPacket* output_packet;
    Effects_PackageTexture* texture_frame;
    SVECTOR* temp;
    DVECTOR* temp2;
    int temp_s3;

    Core_Math_InterpolatePairsToShortVector(particle_definition->vectorRanges[4],
        Effects_SampleEffectCurve(particle_definition->accelerationCurve, trail_set->curveSampleDistance), &emitter_scratch->accelerationMin);

    if (particle_definition->rCurve != 0) {
        j = Effects_SampleEffectCurve(particle_definition->rCurve, trail_set->curveSampleDistance) * 2;
        sampled_color.r = j <= 255 ? j : 255;
    } else {
        sampled_color.r = 128;
    }

    if (particle_definition->gCurve != 0) {
        j = Effects_SampleEffectCurve(particle_definition->gCurve, trail_set->curveSampleDistance) * 2;
        sampled_color.g = j <= 255 ? j : 255;
    } else {
        sampled_color.g = 128;
    }

    if (particle_definition->bCurve != 0) {
        j = Effects_SampleEffectCurve(particle_definition->bCurve, trail_set->curveSampleDistance) * 2;
        sampled_color.b = j <= 255 ? j : 255;
    } else {
        sampled_color.b = 128;
    }

    sampled_color.cd = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;

    if (emitter_scratch->flags & 0x20000) {
        Core_Gte_SetRotMatrix(&effect_instance->transforms[particle_definition->transformIndex].view);
        Core_Gte_SetTransMatrix(&effect_instance->transforms[particle_definition->transformIndex].view);
    } else {
        Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
        Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    }

    Effects_AdvanceParticleTextureAnimation(&trail_set->textureAnimation);

    texture_frame = &Effects_package->textures[trail_set->textureAnimation.textureInfo->dataIndex];

    for (i = 0; i < trail_set->trailcount; ++i) {

        trail = &trail_set->trails[i];

        temp = trail->splineSamples;
        if (trail->splineSampleCount >= 2) {
            temp_s3 = trail->currentSplineSample;
            for (j = 0; j < trail->splineSampleCount; ++j) {
                Core_Gte_LoadV0(&temp[temp_s3]);
                Core_Gte_Rtps();
                Core_Gte_StoreSxy2(&screen_points[j]);
                Core_Gte_StoreSz3Otz(&projected_depths[j]);
                temp_s3 = (temp_s3 + 4) % 5;
            }

            for (j = 0; j < (trail->splineSampleCount - 1); ++j) {
                DVECTOR* p = &screen_points[j];
                segment_normal_angles[j] = Core_Gte_Atan2(p[1].vy - p[0].vy, p[1].vx - p[0].vx);
                segment_normal_angles[j] -= 0x400;
            }

            temp_s3 = emitter_scratch->accelerationMin.vx / 2;
            temp_s0 = (Core_Gte_Cos(segment_normal_angles[0]) * temp_s3) >> 0xC;
            normal_offset_y = (Core_Gte_Sin(segment_normal_angles[0]) * temp_s3) >> 0xC;
            screen_point_base = screen_points;
            strip_packet.opaque4.x0 = screen_point_base[0].vx + temp_s0;
            strip_packet.opaque4.y0 = screen_point_base[0].vy + normal_offset_y;
            strip_packet.opaque4.x2 = screen_point_base[0].vx - temp_s0;
            strip_packet.opaque4.y2 = screen_point_base[0].vy - normal_offset_y;

            strip_packet.opaque4.u0 = strip_packet.opaque4.u2 =
                texture_frame->u + ((texture_frame->w * trail->trailCollapseCounter) / 4);
            texture_v_start = texture_frame->v;
            texture_v_end = texture_frame->v + texture_frame->h;
            strip_packet.opaque4.v0 = texture_v_start;
            strip_packet.opaque4.v2 = texture_v_end;
            strip_packet.opaque4.r0 =
                (sampled_color.r * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter])
                >> 7;
            strip_packet.opaque4.g0 =
                (sampled_color.g * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter])
                >> 7;
            strip_packet.opaque4.b0 =
                (sampled_color.b * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter])
                >> 7;
            strip_packet.opaque4.code = sampled_color.cd;

            for (j = 0; j < (trail->splineSampleCount - 2); ++j) {
                joint_normal_angle = (segment_normal_angles[j] + segment_normal_angles[j + 1]) / 2;

                if (((segment_normal_angles[j + 1] - segment_normal_angles[j]) + 0x7FF) >= 0xFFFU) {
                    joint_normal_angle += 0x800;
                }

                temp_s0 = (Core_Gte_Cos(joint_normal_angle) * temp_s3) >> 0xC;
                normal_offset_y = (Core_Gte_Sin(joint_normal_angle) * temp_s3) >> 0xC;
                temp2 = &screen_point_base[j];
                strip_packet.opaque4.x1 = temp2[1].vx + temp_s0;
                strip_packet.opaque4.y1 = temp2[1].vy + normal_offset_y;
                strip_packet.opaque4.x3 = temp2[1].vx - temp_s0;
                strip_packet.opaque4.y3 = temp2[1].vy - normal_offset_y;

                strip_packet.opaque4.u1 = strip_packet.opaque4.u3 =
                    texture_frame->u
                    + ((texture_frame->w * ((trail->trailCollapseCounter - j) - 1)) / 4);
                strip_packet.opaque4.v1 = texture_v_start;
                strip_packet.opaque4.v3 = texture_v_end;
                strip_packet.opaque4.r1 =
                    ((sampled_color.r
                         * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                strip_packet.opaque4.g1 =
                    ((sampled_color.g
                         * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                strip_packet.opaque4.b1 =
                    ((sampled_color.b
                         * trail_set->sampledTransparencyCurve[trail->trailCollapseCounter
                                                          - j - 1])
                        >> 7);
                strip_packet.opaque4.p1 = sampled_color.cd;

                temp_s0 = (projected_depths[j] + projected_depths[j + 1]) / 2;

                if ((temp_s0 < 0x800) && (Render_nearClip < temp_s0)) {
                    output_packet = Render_frameScratch.primitiveCursor;
                    Render_frameScratch.primitiveCursor = output_packet + 1;
                    output_packet->wordCount = 0xE;
                    output_packet->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                    output_packet->opaque4.tag = 0xE1000000;
                    output_packet->drawMode = 0xE1000200;
                    *(int*)&output_packet->tag0 =
                        (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFFFFFF) | 0x0E000000;
                    ((u_long*)Render_frameScratch.orderingTable)[temp_s0] =
                        (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFF000000)
                        | (LegacyGpu_Handle(output_packet) & 0xFFFFFF);
                    *(int*)&output_packet->opaque4.r0 = *(int*)&strip_packet.opaque4.r0;
                    *(int*)&output_packet->opaque4.r1 = *(int*)&strip_packet.opaque4.r1;
                    *(int*)&output_packet->opaque4.r2 = *(int*)&strip_packet.opaque4.r0;
                    *(int*)&output_packet->opaque4.r3 = *(int*)&strip_packet.opaque4.r1;
                    *(int*)&output_packet->opaque4.x0 = *(int*)&strip_packet.opaque4.x0;
                    *(int*)&output_packet->opaque4.x1 = *(int*)&strip_packet.opaque4.x1;
                    *(int*)&output_packet->opaque4.x2 = *(int*)&strip_packet.opaque4.x2;
                    *(int*)&output_packet->opaque4.x3 = *(int*)&strip_packet.opaque4.x3;
                    *(short*)&output_packet->opaque4.u0 = *(short*)&strip_packet.opaque4.u0;
                    *(short*)&output_packet->opaque4.u1 = *(short*)&strip_packet.opaque4.u1;
                    *(short*)&output_packet->opaque4.u2 = *(short*)&strip_packet.opaque4.u2;
                    *(short*)&output_packet->opaque4.u3 = *(short*)&strip_packet.opaque4.u3;
                    *(short*)&output_packet->opaque4.tpage = texture_frame->tpage;
                    *(short*)&output_packet->opaque4.clut = texture_frame->clut;
                }
                *(int*)&strip_packet.opaque4.x0 = *(int*)&strip_packet.opaque4.x1;
                *(int*)&strip_packet.opaque4.x2 = *(int*)&strip_packet.opaque4.x3;
                *(short*)&strip_packet.opaque4.u0 = *(short*)&strip_packet.opaque4.u1;
                *(short*)&strip_packet.opaque4.u2 = *(short*)&strip_packet.opaque4.u3;
                *(int*)&strip_packet.opaque4.r0 = *(int*)&strip_packet.opaque4.r1;
            }

            temp_s0 = (Core_Gte_Cos(segment_normal_angles[j]) * temp_s3) >> 0xC;
            normal_offset_y = (Core_Gte_Sin(segment_normal_angles[j]) * temp_s3) >> 0xC;
            strip_packet.opaque4.x1 = ((u_short*)screen_point_base)[j * 2 + 2] + temp_s0;
            strip_packet.opaque4.y1 = ((u_short*)screen_point_base)[j * 2 + 3] + normal_offset_y;
            strip_packet.opaque4.x3 = ((u_short*)screen_point_base)[j * 2 + 2] - temp_s0;
            strip_packet.opaque4.y3 = ((u_short*)screen_point_base)[j * 2 + 3] - normal_offset_y;
            strip_packet.opaque4.u1 = strip_packet.opaque4.u3 = texture_frame->u;
            strip_packet.opaque4.v1 = texture_v_start;
            strip_packet.opaque4.v3 = texture_v_end;
            strip_packet.opaque4.r1 = (sampled_color.r * trail_set->sampledTransparencyCurve[0]) >> 7;
            strip_packet.opaque4.g1 = (sampled_color.g * trail_set->sampledTransparencyCurve[0]) >> 7;
            strip_packet.opaque4.b1 = (sampled_color.b * trail_set->sampledTransparencyCurve[0]) >> 7;

            temp_s0 = (projected_depths[j] + projected_depths[j + 1]) / 2;

            if ((temp_s0 < 0x800) && (Render_nearClip < temp_s0)) {
                output_packet = Render_frameScratch.primitiveCursor;
                Render_frameScratch.primitiveCursor = output_packet + 1;
                output_packet->wordCount = 0xE;
                output_packet->opaque4.code = Render_LegacyPrimitiveCode_PolyGt4SemiTrans;
                output_packet->opaque4.tag = 0xE1000000;
                output_packet->drawMode = 0xE1000200;
                *(int*)&output_packet->tag0 =
                    (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)Render_frameScratch.orderingTable)[temp_s0] =
                    (((u_long*)Render_frameScratch.orderingTable)[temp_s0] & 0xFF000000)
                    | (LegacyGpu_Handle(output_packet) & 0xFFFFFF);
                *(int*)&output_packet->opaque4.r0 = *(int*)&strip_packet.opaque4.r0;
                *(int*)&output_packet->opaque4.r1 = *(int*)&strip_packet.opaque4.r1;
                *(int*)&output_packet->opaque4.r2 = *(int*)&strip_packet.opaque4.r0;
                *(int*)&output_packet->opaque4.r3 = *(int*)&strip_packet.opaque4.r1;
                *(int*)&output_packet->opaque4.x0 = *(int*)&strip_packet.opaque4.x0;
                *(int*)&output_packet->opaque4.x1 = *(int*)&strip_packet.opaque4.x1;
                *(int*)&output_packet->opaque4.x2 = *(int*)&strip_packet.opaque4.x2;
                *(int*)&output_packet->opaque4.x3 = *(int*)&strip_packet.opaque4.x3;
                *(short*)&output_packet->opaque4.u0 = *(short*)&strip_packet.opaque4.u0;
                *(short*)&output_packet->opaque4.u1 = *(short*)&strip_packet.opaque4.u1;
                *(short*)&output_packet->opaque4.u2 = *(short*)&strip_packet.opaque4.u2;
                *(short*)&output_packet->opaque4.u3 = *(short*)&strip_packet.opaque4.u3;
                *(short*)&output_packet->opaque4.tpage = texture_frame->tpage;
                *(short*)&output_packet->opaque4.clut = texture_frame->clut;
            }
        }
    }
}
#undef TRAIL_SAMPLE_COUNT

