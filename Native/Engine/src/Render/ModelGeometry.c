// Hand-maintained (tools/port/owned.csv).
#include "Render/ModelGeometry.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Core/Gte.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

enum {
  kSupportedBoneLimit = 63,
  kGroupWords = 2,
  kGroupBytes = kGroupWords * sizeof(uint16_t),
  kTriangleCorners = 3,
  kQuadCorners = 4,
  kCoordinateBits = sizeof(int16_t) * CHAR_BIT,
  kSectionAlignmentMask = sizeof(uint32_t) - 1,
  kFlatTriangleCommand = 0x24,
  kFlatQuadCommand = 0x2C,
  kGouraudTriangleCommand = 0x34,
  kGouraudQuadCommand = 0x3C,
  kFlatTriangleBytes = 16,
  kFlatQuadBytes = 20,
  kGouraudTriangleBytes = 24,
  kGouraudQuadBytes = 32,
  kColorMask = 0xFFFFFF,
  kPrimitiveBufferBytes = 0x20000
};

static int PolygonCorners(unsigned command, int colored) {
  if (command ==
      (unsigned)(colored ? kGouraudTriangleCommand : kFlatTriangleCommand)) {
    return kTriangleCorners;
  }
  if (command == (unsigned)(colored ? kGouraudQuadCommand : kFlatQuadCommand)) {
    return kQuadCorners;
  }
  return 0;
}

static unsigned PolygonBytes(int corners, int colored) {
  if (colored) {
    return corners == kTriangleCorners ? kGouraudTriangleBytes
                                       : kGouraudQuadBytes;
  }
  return corners == kTriangleCorners ? kFlatTriangleBytes : kFlatQuadBytes;
}

// decomp: BATTLE.PRG func_800B3A68 (src/BATTLE/BATTLE.PRG/4B16C.s:84).
int Render_ProjectModelVertices(void* instance, MATRIX* matrices,
                                int near_offset, void* model_data) {
  (void)near_offset;
  Actors_ModelRenderContext* context = &Actors_modelRenderContext;
  context->projectedVertexCount = 0;
  if (instance == NULL || matrices == NULL || model_data == NULL) {
    return 0;
  }
  const unsigned char* counts = instance;
  const unsigned bone_count = counts[0];
  const unsigned group_count = counts[1];
  const Actors_ShpModel* model = model_data;
  if (bone_count == 0 || bone_count > kSupportedBoneLimit || group_count == 0 ||
      model->groupSection <
          sizeof *model + bone_count * sizeof(Actors_ShpBone) ||
      model->groupSection > model->vertexSection ||
      model->vertexSection > model->polygonSection ||
      group_count * kGroupBytes > model->vertexSection - model->groupSection ||
      ((model->groupSection | model->vertexSection | model->polygonSection) &
       kSectionAlignmentMask) != 0) {
    return 0;
  }
  const uint16_t* groups = VS_ONDISC(model, groupSection, const uint16_t*);
  const SVECTOR* vertices = VS_ONDISC(model, vertexSection, const SVECTOR*);
  unsigned previous = 0;
  for (unsigned group = 0; group < group_count; ++group) {
    const unsigned bone = groups[group * 2];
    const unsigned end = groups[group * 2 + 1];
    if (bone >= bone_count || end < previous ||
        end > sizeof context->projectedVertices /
                  sizeof context->projectedVertices[0] ||
        end * sizeof(SVECTOR) > model->polygonSection - model->vertexSection) {
      return 0;
    }
    previous = end;
  }
  unsigned vertex = 0;
  int combined_depth = 0;
  for (unsigned group = 0; group < group_count; ++group) {
    const unsigned end = groups[group * 2 + 1];
    const MATRIX* matrix = &matrices[groups[group * 2]];
    Core_Gte_SetRotMatrix(matrix);
    Core_Gte_SetTransMatrix(matrix);
    for (; vertex < end; ++vertex) {
      SVECTOR position = vertices[vertex];
      VECTOR transformed;
      Core_GteWord fog, flags;
      Core_Gte_RotTransPers(&position, &context->projectedVertices[vertex],
                            &fog, &flags);
      Core_Gte_StoreLvnl(&transformed);
      const int depth = transformed.vz < 0            ? 0
                        : transformed.vz > UINT16_MAX ? UINT16_MAX
                                                      : transformed.vz;
      context->vertexDepths[vertex] = depth;
      combined_depth |= depth;
    }
  }
  context->projectedVertexCount = vertex;
  return combined_depth;
}

typedef struct ModelHeader {
  unsigned char bones;
  unsigned char groups;
  uint16_t triangles;
  uint16_t quads;
  uint16_t other;
} ModelHeader;

int Render_ValidateModelData(void* instance, const void* data, size_t size) {
  if (instance == NULL || data == NULL || size < sizeof(Actors_ShpModel)) {
    return 0;
  }
  ModelHeader header;
  memcpy(&header, instance, sizeof header);
  const Actors_ShpModel* model = data;
  if (header.bones > kSupportedBoneLimit ||
      model->groupSection <
          sizeof *model + header.bones * sizeof(Actors_ShpBone) ||
      model->groupSection > model->vertexSection ||
      model->vertexSection > model->polygonSection ||
      model->polygonSection > size ||
      header.groups * kGroupBytes >
          model->vertexSection - model->groupSection ||
      ((model->groupSection | model->vertexSection | model->polygonSection) &
       kSectionAlignmentMask) != 0) {
    return 0;
  }
  const unsigned counts[3] = {header.triangles, header.quads, header.other};
  size_t offset = model->polygonSection;
  const unsigned char* bytes = data;
  const int colored = (((const unsigned char*)instance)[8] & 8) != 0;
  for (int part = 0; part < 3; ++part) {
    for (unsigned face = 0; face < counts[part]; ++face) {
      const unsigned prefix = colored ? 12 : 2;
      if (size - offset < prefix) return 0;
      const unsigned command = bytes[offset + (colored ? 11 : 0)];
      const int corners = PolygonCorners(command, colored);
      if (corners == 0 || (part != 2 && corners != part + 3)) return 0;
      const unsigned extent = PolygonBytes(corners, colored);
      if (extent > size - offset) return 0;
      offset += extent;
    }
  }
  return 1;
}

static int QueueModelPolygon(Actors_ModelRenderContext* context,
                             const unsigned char* data, int corners,
                             int colored, unsigned flags, int bias,
                             unsigned tint, uintptr_t limit) {
  uint16_t indices[kQuadCorners];
  memcpy(indices, data + (colored ? 0 : sizeof(uint32_t)),
         (size_t)corners * sizeof indices[0]);
  unsigned depths = 0;
  Core_GteWord screen[kQuadCorners];
  int left = 1024, top = 1024, right = -1024, bottom = -1024;
  for (int corner = 0; corner < corners; ++corner) {
    if ((indices[corner] & kSectionAlignmentMask) != 0 ||
        indices[corner] / sizeof(Core_GteWord) >=
            context->projectedVertexCount) {
      return -1;
    }
    const unsigned vertex = indices[corner] / sizeof(Core_GteWord);
    depths += context->vertexDepths[vertex];
    screen[corner] = context->projectedVertices[vertex];
    const int x = (int16_t)screen[corner];
    const int y = (int16_t)((uint32_t)screen[corner] >> kCoordinateBits);
    if (x < left) left = x;
    if (x > right) right = x;
    if (y < top) top = y;
    if (y > bottom) bottom = y;
  }
  if (!colored && corners == 4 && (flags & 3) == 3) {
    int delta[3][3];
    for (int vertex = 0; vertex < 3; ++vertex) {
      delta[vertex][0] = (int16_t)screen[vertex + 1] - (int16_t)screen[0];
      delta[vertex][1] = (int16_t)((uint32_t)screen[vertex + 1] >> 16) -
                         (int16_t)((uint32_t)screen[0] >> 16);
      const int difference = context->vertexDepths[indices[vertex + 1] / 4] -
                             context->vertexDepths[indices[0] / 4];
      delta[vertex][2] = vertex < 2 ? (int16_t)difference : difference;
    }
    const int64_t volume = ((int64_t)delta[0][1] * delta[1][2] -
                            (int64_t)delta[0][2] * delta[1][1]) *
                               delta[2][0] +
                           ((int64_t)delta[0][2] * delta[1][0] -
                            (int64_t)delta[0][0] * delta[1][2]) *
                               delta[2][1] +
                           ((int64_t)delta[0][0] * delta[1][1] -
                            (int64_t)delta[0][1] * delta[1][0]) *
                               delta[2][2];
    const unsigned normal[2][3] = {{0, 1, 2}, {1, 3, 2}};
    const unsigned alternate[2][3] = {{0, 1, 3}, {0, 3, 2}};
    const unsigned (*order)[3] =
        (int32_t)(uint32_t)volume > 0 ? normal : alternate;
    int emitted = 0;
    for (int part = 0; part < 2; ++part) {
      unsigned char triangle[kFlatTriangleBytes] = {kFlatTriangleCommand,
                                                    kFlatTriangleBytes};
      for (int corner = 0; corner < 3; ++corner) {
        const unsigned vertex = order[part][corner];
        memcpy(triangle + 4 + corner * 2, &indices[vertex],
               sizeof indices[vertex]);
        memcpy(triangle + 10 + ((corner + 1) % 3) * 2, data + 12 + vertex * 2,
               2);
      }
      const int result =
          QueueModelPolygon(context, triangle, 3, 0, 0, -1, tint, limit);
      if (result < 0) return result;
      emitted += result;
    }
    return emitted;
  }
  if (right < 0 || bottom < 0 || left >= 320 || top >= 240 ||
      ((flags & 1) == 0 &&
       Core_Gte_NormalClip(screen[0], screen[1], screen[2]) <= 0)) {
    return 0;
  }
  int depth = (int)(corners == 3 ? (depths * 341u) >> 12 : depths >> 4);
  depth += bias + (int)((flags >> 2) & 3) * 4;
  if (depth < 0 || depth >= 0x7C0 ||
      Render_frameScratch.orderingTable == NULL) {
    return 0;
  }
  const size_t words =
      colored ? 1 + (size_t)corners * 3 : 2 + (size_t)corners * 2;
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (cursor > limit || words * sizeof(u_long) > limit - cursor) {
    return -1;
  }
  u_long* packet = (void*)cursor;
  memset(packet, 0, words * sizeof *packet);
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  const unsigned page = workspace->texturePage | (flags & 0x60);
  for (int corner = 0; corner < corners; ++corner) {
    const unsigned uv_corner =
        corners == 3 ? (unsigned)(corner + 1) % 3 : (unsigned)corner;
    const unsigned uv_offset =
        colored ? (corners == 3 ? (uv_corner == 0 ? 6 : 18 + uv_corner * 2)
                                : 24 + uv_corner * 2)
                : 4 + corners * 2 + uv_corner * 2;
    uint16_t uv;
    memcpy(&uv, data + uv_offset, sizeof uv);
    const unsigned xy_word = colored ? 2 + corner * 3 : 2 + corner * 2;
    packet[xy_word] = (u_long)screen[corner];
    packet[xy_word + 1] = uv;
    if (colored) {
      unsigned color;
      memcpy(&color, data + 8 + corner * 4, sizeof color);
      packet[xy_word - 1] = color & kColorMask;
    }
  }
  const unsigned command = (colored ? 0x34u : 0x24u) |
                           (corners == 4 ? 8u : 0u) |
                           ((flags & 0x10) != 0 ? 2u : 0u);
  if (!colored) packet[1] = tint & kColorMask;
  packet[1] |= command << 24;
  packet[3] |= (unsigned)workspace->palette << 16;
  packet[colored ? 6 : 5] |= page << 16;
  setlen(packet, words - 1);
  AddPrim((u_long*)Render_frameScratch.orderingTable + depth, packet);
  Render_frameScratch.primitiveCursor = packet + words;
  return 1;
}

static void DrawModelPolygons(Actors_ModelRenderContext* context,
                              Actors_ModelInstance* instance,
                              const CVECTOR* tint, Actors_ShpModel* model_data,
                              int colored) {
  if (context == NULL || instance == NULL || model_data == NULL ||
      context->projectedVertexCount == 0) {
    return;
  }
  const uintptr_t base =
      (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (base == 0 || cursor < base || cursor - base > kPrimitiveBufferBytes) {
    return;
  }
  ModelHeader header;
  memcpy(&header, instance, sizeof header);
  const Actors_ShpModel* model = model_data;
  const unsigned char* polygon =
      VS_ONDISC(model, polygonSection, const unsigned char*);
  unsigned color = 0x808080;
  if (tint != NULL) memcpy(&color, tint, sizeof color);
  const unsigned counts[3] = {header.triangles, header.quads, header.other};
  for (int part = 0; part < 3; ++part) {
    for (unsigned face = 0; face < counts[part]; ++face) {
      const unsigned command = colored ? polygon[11] : polygon[0];
      const int corners = PolygonCorners(command, colored);
      if (corners == 0 || (part != 2 && corners != part + 3)) {
        Core_Debugf("render", "Invalid model polygon command 0x%X", command);
        return;
      }
      const unsigned flags = part == 2 ? polygon[colored ? 19 : 2] : 0;
      if (QueueModelPolygon(context, polygon, corners, colored, flags,
                            part == 2 ? -5 : -1, color,
                            base + kPrimitiveBufferBytes) < 0) {
        return;
      }
      polygon += PolygonBytes(corners, colored);
    }
  }
}

// decomp: BATTLE.PRG func_800B3CF0 (src/BATTLE/BATTLE.PRG/4B16C.s:254).
void Render_DrawModelFlatPolygons(Actors_ModelRenderContext* context,
                                  void* instance, CVECTOR* tint,
                                  void* model_data) {
  DrawModelPolygons(context, instance, tint, model_data, 0);
}

// decomp: BATTLE.PRG func_800B4594 (src/BATTLE/BATTLE.PRG/4B16C.s:835).
void Render_DrawModelGouraudPolygons(Actors_ModelRenderContext* context,
                                     void* instance, void* tint,
                                     void* model_data) {
  DrawModelPolygons(context, instance, tint, model_data, 1);
}
