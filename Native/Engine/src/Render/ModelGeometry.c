// Hand-maintained (tools/port/owned.csv).
#include "Render/ModelGeometry.h"
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Gimmicks.h"
#include "Render/Camera.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/UiPrimitives.h"

#include <stdint.h>
#include <string.h>


// decomp: BATTLE.PRG func_800B3A68 (src/BATTLE/BATTLE.PRG/4B16C.s:84).
int Render_ProjectModelVertices(void* instance, MATRIX* matrices, int nearOffset, void* modelData)
{
    (void)nearOffset;
    Actors_ModelRenderContext* context = &Actors_modelRenderContext;
    context->projectedVertexCount = 0;
    if (instance == NULL || matrices == NULL || modelData == NULL) {
        return 0;
    }
    const unsigned char* counts = instance;
    const unsigned boneCount = counts[0];
    const unsigned groupCount = counts[1];
    const Actors_ShpModel* model = modelData;
    if (boneCount == 0 || boneCount > 63 || groupCount == 0 ||
        model->groupSection < sizeof *model + boneCount * sizeof(Actors_ShpBone) ||
        model->groupSection > model->vertexSection || model->vertexSection > model->polygonSection ||
        groupCount * 4 > model->vertexSection - model->groupSection ||
        ((model->groupSection | model->vertexSection | model->polygonSection) & 3) != 0) {
        return 0;
    }
    const unsigned short* groups = VS_ONDISC(model, groupSection, const unsigned short*);
    const SVECTOR* vertices = VS_ONDISC(model, vertexSection, const SVECTOR*);
    unsigned previous = 0;
    for (unsigned group = 0; group < groupCount; ++group) {
        const unsigned bone = groups[group * 2];
        const unsigned end = groups[group * 2 + 1];
        if (bone >= boneCount || end < previous || end > 768 ||
            end * sizeof(SVECTOR) > model->polygonSection - model->vertexSection) {
            return 0;
        }
        previous = end;
    }
    unsigned vertex = 0;
    int combinedDepth = 0;
    for (unsigned group = 0; group < groupCount; ++group) {
        const unsigned end = groups[group * 2 + 1];
        const MATRIX* matrix = &matrices[groups[group * 2]];
        Core_Gte_SetRotMatrix(matrix);
        Core_Gte_SetTransMatrix(matrix);
        for (; vertex < end; ++vertex) {
            SVECTOR position = vertices[vertex];
            VECTOR transformed;
            long fog, flags;
            Core_Gte_RotTransPers(&position, &context->projectedVertices[vertex], &fog, &flags);
            Core_Gte_StoreLvnl(&transformed);
            const int depth = transformed.vz < 0 ? 0 : transformed.vz > 65535 ? 65535 : transformed.vz;
            context->vertexDepths[vertex] = depth;
            combinedDepth |= depth;
        }
    }
    context->projectedVertexCount = vertex;
    return combinedDepth;
}

typedef struct ModelHeader {
    unsigned char bones;
    unsigned char groups;
    unsigned short triangles;
    unsigned short quads;
    unsigned short other;
} ModelHeader;

int Render_ValidateModelData(void* instance, const void* data, size_t size)
{
    if (instance == NULL || data == NULL || size < sizeof(Actors_ShpModel)) return 0;
    ModelHeader header;
    memcpy(&header, instance, sizeof header);
    const Actors_ShpModel* model = data;
    if (header.bones > 63 || model->groupSection < sizeof *model + header.bones * sizeof(Actors_ShpBone) ||
        model->groupSection > model->vertexSection || model->vertexSection > model->polygonSection ||
        model->polygonSection > size || header.groups * 4 > model->vertexSection - model->groupSection ||
        ((model->groupSection | model->vertexSection | model->polygonSection) & 3) != 0) return 0;
    const unsigned counts[3] = {header.triangles, header.quads, header.other};
    size_t offset = model->polygonSection;
    const unsigned char* bytes = data;
    const int colored = (((const unsigned char*)instance)[8] & 8) != 0;
    for (int part = 0; part < 3; ++part) {
        for (unsigned face = 0; face < counts[part]; ++face) {
            const unsigned prefix = colored ? 12 : 2;
            if (size - offset < prefix) return 0;
            const unsigned command = bytes[offset + (colored ? 11 : 0)];
            const int corners = command == (colored ? 0x34u : 0x24u) ? 3 : command == (colored ? 0x3Cu : 0x2Cu) ? 4 : 0;
            if (corners == 0 || (part != 2 && corners != part + 3)) return 0;
            const unsigned extent = colored ? (corners == 3 ? 24 : 32) : (corners == 3 ? 16 : 20);
            if (extent > size - offset) return 0;
            offset += extent;
        }
    }
    return 1;
}

static int QueueModelPolygon(Actors_ModelRenderContext* context, const unsigned char* data,
    int corners, int colored, unsigned flags, int bias, unsigned tint, uintptr_t limit)
{
    unsigned short indices[4];
    memcpy(indices, data + (colored ? 0 : 4), (size_t)corners * sizeof indices[0]);
    unsigned depths = 0;
    long screen[4];
    int left = 1024, top = 1024, right = -1024, bottom = -1024;
    for (int corner = 0; corner < corners; ++corner) {
        if ((indices[corner] & 3) != 0 || indices[corner] / 4 >= context->projectedVertexCount) {
            return -1;
        }
        const unsigned vertex = indices[corner] / 4;
        depths += context->vertexDepths[vertex];
        screen[corner] = context->projectedVertices[vertex];
        const int x = (short)screen[corner];
        const int y = (short)((unsigned long)screen[corner] >> 16);
        if (x < left) left = x;
        if (x > right) right = x;
        if (y < top) top = y;
        if (y > bottom) bottom = y;
    }
    if (!colored && corners == 4 && (flags & 3) == 3) {
        int delta[3][3];
        for (int vertex = 0; vertex < 3; ++vertex) {
            delta[vertex][0] = (short)screen[vertex + 1] - (short)screen[0];
            delta[vertex][1] = (short)((unsigned long)screen[vertex + 1] >> 16)
                - (short)((unsigned long)screen[0] >> 16);
            const int difference = context->vertexDepths[indices[vertex + 1] / 4]
                - context->vertexDepths[indices[0] / 4];
            delta[vertex][2] = vertex < 2 ? (short)difference : difference;
        }
        const int64_t volume =
            ((int64_t)delta[0][1] * delta[1][2] - (int64_t)delta[0][2] * delta[1][1]) * delta[2][0]
            + ((int64_t)delta[0][2] * delta[1][0] - (int64_t)delta[0][0] * delta[1][2]) * delta[2][1]
            + ((int64_t)delta[0][0] * delta[1][1] - (int64_t)delta[0][1] * delta[1][0]) * delta[2][2];
        const unsigned normal[2][3] = {{0, 1, 2}, {1, 3, 2}};
        const unsigned alternate[2][3] = {{0, 1, 3}, {0, 3, 2}};
        const unsigned (*order)[3] = (int32_t)(uint32_t)volume > 0 ? normal : alternate;
        int emitted = 0;
        for (int part = 0; part < 2; ++part) {
            unsigned char triangle[16] = {0x24, 16};
            for (int corner = 0; corner < 3; ++corner) {
                const unsigned vertex = order[part][corner];
                memcpy(triangle + 4 + corner * 2, &indices[vertex], sizeof indices[vertex]);
                memcpy(triangle + 10 + ((corner + 1) % 3) * 2, data + 12 + vertex * 2, 2);
            }
            const int result = QueueModelPolygon(context, triangle, 3, 0, 0, -1, tint, limit);
            if (result < 0) return result;
            emitted += result;
        }
        return emitted;
    }
    if (right < 0 || bottom < 0 || left >= 320 || top >= 240 ||
        ((flags & 1) == 0 && Core_Gte_NormalClip(screen[0], screen[1], screen[2]) <= 0)) {
        return 0;
    }
    int depth = (int)(corners == 3 ? (depths * 341u) >> 12 : depths >> 4);
    depth += bias + (int)((flags >> 2) & 3) * 4;
    if (depth < 0 || depth >= 0x7C0 || Render_frameScratch.orderingTable == NULL) {
        return 0;
    }
    const size_t words = colored ? 1 + (size_t)corners * 3 : 2 + (size_t)corners * 2;
    const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
    if (cursor > limit || words * sizeof(u_long) > limit - cursor) {
        return -1;
    }
    u_long* packet = (void*)cursor;
    memset(packet, 0, words * sizeof *packet);
    const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
    const unsigned page = workspace->texturePage | (flags & 0x60);
    for (int corner = 0; corner < corners; ++corner) {
        const unsigned uvCorner = corners == 3 ? (unsigned)(corner + 1) % 3 : (unsigned)corner;
        const unsigned uvOffset = colored ? (corners == 3 ? (uvCorner == 0 ? 6 : 18 + uvCorner * 2)
                                                                         : 24 + uvCorner * 2)
                                          : 4 + corners * 2 + uvCorner * 2;
        unsigned short uv;
        memcpy(&uv, data + uvOffset, sizeof uv);
        const unsigned xyWord = colored ? 2 + corner * 3 : 2 + corner * 2;
        packet[xyWord] = (u_long)screen[corner];
        packet[xyWord + 1] = uv;
        if (colored) {
            unsigned color;
            memcpy(&color, data + 8 + corner * 4, sizeof color);
            packet[xyWord - 1] = color & 0xFFFFFF;
        }
    }
    const unsigned command = (colored ? 0x34u : 0x24u) | (corners == 4 ? 8u : 0u) | ((flags & 0x10) != 0 ? 2u : 0u);
    if (!colored) packet[1] = tint & 0xFFFFFF;
    packet[1] |= command << 24;
    packet[3] |= (unsigned)workspace->palette << 16;
    packet[colored ? 6 : 5] |= page << 16;
    setlen(packet, words - 1);
    AddPrim((u_long*)Render_frameScratch.orderingTable + depth, packet);
    Render_frameScratch.primitiveCursor = packet + words;
    return 1;
}

static void DrawModelPolygons(Actors_ModelRenderContext* context, void* instance, const void* tint,
                              void* modelData, int colored)
{
    if (context == NULL || instance == NULL || modelData == NULL || context->projectedVertexCount == 0) {
        return;
    }
    const uintptr_t base = (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
    const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
    if (base == 0 || cursor < base || cursor - base > 0x20000) {
        return;
    }
    ModelHeader header;
    memcpy(&header, instance, sizeof header);
    const Actors_ShpModel* model = modelData;
    const unsigned char* polygon = VS_ONDISC(model, polygonSection, const unsigned char*);
    unsigned color = 0x808080;
    if (tint != NULL) memcpy(&color, tint, sizeof color);
    const unsigned counts[3] = {header.triangles, header.quads, header.other};
    for (int part = 0; part < 3; ++part) {
        for (unsigned face = 0; face < counts[part]; ++face) {
            const unsigned command = colored ? polygon[11] : polygon[0];
            const int corners = command == (colored ? 0x34u : 0x24u) ? 3 : command == (colored ? 0x3Cu : 0x2Cu) ? 4 : 0;
            if (corners == 0 || (part != 2 && corners != part + 3)) {
                Core_Debugf("render", "Invalid model polygon command 0x%X", command);
                return;
            }
            const unsigned flags = part == 2 ? polygon[colored ? 19 : 2] : 0;
            if (QueueModelPolygon(context, polygon, corners, colored, flags, part == 2 ? -5 : -1,
                                  color, base + 0x20000) < 0) {
                return;
            }
            polygon += colored ? (corners == 3 ? 24 : 32) : (corners == 3 ? 16 : 20);
        }
    }
}

// decomp: BATTLE.PRG func_800B3CF0 (src/BATTLE/BATTLE.PRG/4B16C.s:254).
void Render_DrawModelFlatPolygons(Actors_ModelRenderContext* context, void* instance, CVECTOR* tint, void* modelData)
{
    DrawModelPolygons(context, instance, tint, modelData, 0);
}

// decomp: BATTLE.PRG func_800B4594 (src/BATTLE/BATTLE.PRG/4B16C.s:835).
void Render_DrawModelGouraudPolygons(Actors_ModelRenderContext* context, void* instance, void* tint, void* modelData)
{
    DrawModelPolygons(context, instance, tint, modelData, 1);
}
