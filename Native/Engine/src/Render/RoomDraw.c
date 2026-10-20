// Hand-maintained (tools/port/owned.csv).
#include "Render/RoomDraw.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "Core/Gte.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

enum {
  kMaximumGeometryGroups = 4096,
  kRoomTriangleBytes = 32,
  kRoomQuadBytes = 40,
  kFixedFractionBits = 12,
  kFixedOneSquared = 1 << (2 * kFixedFractionBits),
  kPackedCoordinateBits = sizeof(int16_t) * CHAR_BIT,
  kViewportWidth = 320,
  kViewportHeight = 224,
  kDrawHeight = 240,
  kTriangleCorners = 3,
  kQuadCorners = 4,
  kQuadCommandFlag = 8,
  kVertexPacketWords = 3,
  kLastOrderingSlot = 2047,
  kPrimitiveBufferBytes = 0x20000,
  kRoomBatchBytes = 0x10000,
  kMinimumSubdivisionArea = 512,
  kSkyVertexCount = 34,
  kSkyFaceCount = 64,
  kSkyVertexBytes = 12,
  kSkyFaceBytes = 4,
  kColorMask = 0xFFFFFF,
  kRoomCommandOffset = 15,
  kPacketCommandMask = 0xFE,
  kPacketCommandShift = 3 * CHAR_BIT,
  kPalettePacketWord = 3,
  kPagePacketWord = 6,
  kDefaultNearDepth = 8,
  kDefaultLodDepth = 512,
  kSkipViewUpdateFlag = 0x100,
  kCameraFacingFlag = 0x400,
  kSkipGeometryFlags = 0x1100,
  kAlternatePassShift = 15,
  kProjectedCoordinateBound = 1024,
  kTriangleSubdivisionCount = 7,
  kRecursiveSubdivisionCount = 4,
  kClutRecordOffset = 26,
  kPageRecordOffset = 30,
  kSkyGouraudCommand = 0x30000000,
  kSkyPacketWords = sizeof(POLY_G3) / sizeof(u_long)
};

int Render_ValidateRoomGeometry(const void* data, size_t size) {
  if (data == NULL || size < sizeof(unsigned)) {
    return 0;
  }
  unsigned count;
  memcpy(&count, data, sizeof count);
  const size_t header_end =
      sizeof count + (size_t)count * sizeof(Render_RoomGeometryGroup);
  if (count > kMaximumGeometryGroups || header_end > size) {
    return 0;
  }
  for (unsigned index = 0; index < count; ++index) {
    Render_RoomGeometryGroup group;
    memcpy(&group,
           (const unsigned char*)data + sizeof count + index * sizeof group,
           sizeof group);
    if (group.polygonOffset < header_end || group.polygonOffset > size ||
        size - group.polygonOffset < 2 * sizeof(unsigned)) {
      return 0;
    }
    unsigned counts[2];
    memcpy(counts, (const unsigned char*)data + group.polygonOffset,
           sizeof counts);
    if ((uint64_t)counts[0] * kRoomTriangleBytes +
            (uint64_t)counts[1] * kRoomQuadBytes >
        size - group.polygonOffset - sizeof counts) {
      return 0;
    }
  }
  return 1;
}

static void BuildRoomView(VECTOR* position, VECTOR* angles, MATRIX* output,
                          int inverse_scale) {
  MATRIX camera = Render_frameScratch.viewMatrix;
  if (inverse_scale) {
    const int scale = Render_frameScratch.camera.farClip;
    if (scale <= 0) {
      memset(output, 0, sizeof *output);
      return;
    }
    MATRIX inverse = {0};
    inverse.m[0][0] = inverse.m[1][1] = inverse.m[2][2] =
        (int16_t)(kFixedOneSquared / scale);
    Core_Math_MultiplyMatrixIntoRight(&inverse, &camera);
  }
  SVECTOR rotation = {(int16_t)angles->vx, (int16_t)angles->vy,
                      (int16_t)angles->vz, 0};
  Core_Gte_RotMatrixYXZ(&rotation, output);
  Core_Math_MultiplyMatrixIntoRight(&camera, output);
  const int16_t coordinates[kRoomSpatialDimensions] = {
      (int16_t)position->vx, (int16_t)position->vy, (int16_t)position->vz};
  for (int row = 0; row < kRoomSpatialDimensions; ++row) {
    int64_t translated = 0;
    for (int axis = 0; axis < kRoomSpatialDimensions; ++axis) {
      translated += (int64_t)camera.m[row][axis] * coordinates[axis];
    }
    output->t[row] = (Core_GteWord)((translated >> kFixedFractionBits) +
                                    Render_frameScratch.viewMatrix.t[row]);
  }
}

// decomp: BATTLE.PRG func_800983F8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1265).
void Render_UpdateRoomGroupTransforms(void* data) {
  if (data == NULL) {
    return;
  }
  unsigned count;
  memcpy(&count, data, sizeof count);
  if (count > kMaximumGeometryGroups) {
    return;
  }
  Render_RoomGeometryGroup* groups =
      (void*)((unsigned char*)data + sizeof count);
  const int scale = Render_frameScratch.camera.farClip;
  const int sine = Core_Math_MultiplyFixed(
      Core_Gte_Sin(Render_frameScratch.camera.angles.vz), scale);
  const int cosine = Core_Math_MultiplyFixed(
      Core_Gte_Cos(Render_frameScratch.camera.angles.vz), scale);
  for (unsigned index = 0; index < count; ++index) {
    Render_RoomGeometryGroup* group = &groups[index];
    if ((group->flags & kSkipViewUpdateFlag) != 0) {
      continue;
    }
    VECTOR position = {group->bounds[0][0], group->bounds[1][0],
                       group->bounds[2][0], 0};
    VECTOR angles = {group->angles[0], group->angles[1], group->angles[2], 0};
    BuildRoomView(&position, &angles, &group->view, 0);
    if ((group->flags & kCameraFacingFlag) != 0) {
      memset(group->view.m, 0, sizeof group->view.m);
      group->view.m[0][0] = group->view.m[1][1] = (int16_t)cosine;
      group->view.m[0][1] = (int16_t)-sine;
      group->view.m[1][0] = (int16_t)sine;
      group->view.m[2][2] = (int16_t)scale;
    }
  }
}

// decomp: BATTLE.PRG func_800985AC (src/BATTLE/BATTLE.PRG/2EA3C.s:1383).
void Render_BuildRoomGeometryViewMatrix(VECTOR* position, VECTOR* angles,
                                        MATRIX* output) {
  BuildRoomView(position, angles, output, 0);
}

// decomp: BATTLE.PRG func_80098648 (src/BATTLE/BATTLE.PRG/2EA3C.s:1429).
void Render_BuildScaleCompensatedRoomView(VECTOR* position, VECTOR* angles,
                                          MATRIX* output) {
  BuildRoomView(position, angles, output, 1);
}

// decomp: BATTLE.PRG func_8009723C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
typedef struct RoomVertex {
  SVECTOR position;
  unsigned color;
  uint16_t uv;
} RoomVertex;

static int room_near_depth = kDefaultNearDepth;
static unsigned room_lod_depth = kDefaultLodDepth;

// decomp: BATTLE.PRG func_80097F70 (src/BATTLE/BATTLE.PRG/2EA3C.s).
static RoomVertex Midpoint(const RoomVertex* first, const RoomVertex* second) {
  RoomVertex midpoint = {0};
  midpoint.position.vx =
      (int16_t)(((int)first->position.vx + second->position.vx) >> 1);
  midpoint.position.vy =
      (int16_t)(((int)first->position.vy + second->position.vy) >> 1);
  midpoint.position.vz =
      (int16_t)(((int)first->position.vz + second->position.vz) >> 1);
  for (unsigned channel = 0; channel < kRoomSpatialDimensions; ++channel) {
    const unsigned value =
        ((first->color >> (channel * CHAR_BIT)) & UINT8_MAX) +
        ((second->color >> (channel * CHAR_BIT)) & UINT8_MAX);
    midpoint.color |= (value >> 1) << (channel * CHAR_BIT);
  }
  midpoint.uv =
      (uint16_t)((((first->uv & UINT8_MAX) + (second->uv & UINT8_MAX)) >> 1) |
                 ((((first->uv >> CHAR_BIT) + (second->uv >> CHAR_BIT)) >> 1)
                  << CHAR_BIT));
  return midpoint;
}

typedef struct RoomPolygonMaterial {
  unsigned flags;
  unsigned command;
  unsigned clut;
  unsigned page;
} RoomPolygonMaterial;

typedef struct ProjectedRoomPolygon {
  const RoomVertex* vertices;
  const Core_GteWord* screen;
  int corners;
  Core_GteWord depth;
  RoomPolygonMaterial material;
  int low_detail;
} ProjectedRoomPolygon;

static int WriteRoomPolygon(const ProjectedRoomPolygon* polygon,
                            uintptr_t limit) {
  enum { kLowDetailUvMask = 0x7F7F };
  const size_t words = 1 + (size_t)polygon->corners * kVertexPacketWords;
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (cursor > limit || words * sizeof(u_long) > limit - cursor) return -1;
  u_long* packet = (void*)cursor;
  memset(packet, 0, words * sizeof *packet);
  uint16_t page = (uint16_t)polygon->material.page;
  if (polygon->low_detail) --page;
  for (int index = 0; index < polygon->corners; ++index) {
    const unsigned uv =
        polygon->low_detail
            ? (polygon->vertices[index].uv >> 1) & kLowDetailUvMask
            : polygon->vertices[index].uv;
    const size_t vertex_word = 1 + (size_t)index * kVertexPacketWords;
    packet[vertex_word] = polygon->vertices[index].color;
    packet[vertex_word + 1] = (u_long)polygon->screen[index];
    packet[vertex_word + 2] = uv;
  }
  packet[1] |= (polygon->material.command & kPacketCommandMask)
               << kPacketCommandShift;
  packet[kPalettePacketWord] |= polygon->material.clut << kPackedCoordinateBits;
  packet[kPagePacketWord] |= (unsigned)page << kPackedCoordinateBits;
  setlen(packet, words - 1);
  AddPrim((u_long*)Render_frameScratch.orderingTable + polygon->depth, packet);
  Render_frameScratch.primitiveCursor = packet + words;
  return 1;
}

static int EmitSubdividedPolygon(const RoomVertex* vertices, int corners,
                                 const RoomPolygonMaterial* material,
                                 uintptr_t limit) {
  Core_GteWord screen[kQuadCorners], depth = 0;
  int left = kProjectedCoordinateBound, top = kProjectedCoordinateBound;
  int right = -kProjectedCoordinateBound, bottom = -kProjectedCoordinateBound;
  for (int index = 0; index < corners; ++index) {
    SVECTOR position = vertices[index].position;
    Core_GteWord fog, projection_flags;
    const Core_GteWord current = Core_Gte_RotTransPers(
        &position, &screen[index], &fog, &projection_flags);
    if (current > depth) depth = current;
    const int screen_x = (int16_t)screen[index],
              screen_y =
                  (int16_t)((uint32_t)screen[index] >> kPackedCoordinateBits);
    if (screen_x < left) left = screen_x;
    if (screen_x > right) right = screen_x;
    if (screen_y < top) top = screen_y;
    if (screen_y > bottom) bottom = screen_y;
  }
  if (right < 0 || bottom < 0 || left >= kViewportWidth ||
      top >= kViewportHeight || depth < room_near_depth) {
    return 0;
  }
  depth += (int16_t)(material->flags >> kPackedCoordinateBits);
  if (depth < room_near_depth || depth >= kLastOrderingSlot) return 0;
  ProjectedRoomPolygon polygon = {.vertices = vertices,
                                  .screen = screen,
                                  .corners = corners,
                                  .depth = depth,
                                  .material = *material,
                                  .low_detail = 0};
  if (corners == kTriangleCorners) {
    polygon.material.command &= ~(unsigned)kQuadCommandFlag;
  }
  return WriteRoomPolygon(&polygon, limit);
}

// decomp: BATTLE.PRG func_80097AEC (src/BATTLE/BATTLE.PRG/2EA3C.s:612).
static int SubdivideRoomTriangle(const RoomVertex vertices[kTriangleCorners],
                                 int levels,
                                 const RoomPolygonMaterial* material,
                                 uintptr_t limit) {
  if (levels == 0) {
    return EmitSubdividedPolygon(vertices, kTriangleCorners, material, limit);
  }
  const RoomVertex first = Midpoint(&vertices[0], &vertices[1]);
  const RoomVertex second = Midpoint(&vertices[1], &vertices[2]);
  const RoomVertex third = Midpoint(&vertices[2], &vertices[0]);
  const RoomVertex children[kTriangleSubdivisionCount][kTriangleCorners] = {
      {vertices[0], first, third},       {second, first, vertices[1]},
      {second, vertices[2], third},      {first, second, third},
      {vertices[2], vertices[0], third}, {vertices[2], second, vertices[1]},
      {first, vertices[0], vertices[1]}};
  int emitted = 0;
  for (unsigned child = 0; child < kTriangleSubdivisionCount; ++child) {
    const int result = SubdivideRoomTriangle(
        children[child], child < kRecursiveSubdivisionCount ? levels - 1 : 0,
        material, limit);
    if (result < 0) return result;
    emitted += result;
  }
  return emitted;
}

// decomp: BATTLE.PRG func_80097CF0 (src/BATTLE/BATTLE.PRG/2EA3C.s:752).
static int SubdivideRoomQuad(const RoomVertex vertices[kQuadCorners],
                             int levels, const RoomPolygonMaterial* material,
                             uintptr_t limit) {
  if (levels == 0) {
    return EmitSubdividedPolygon(vertices, kQuadCorners, material, limit);
  }
  const RoomVertex top = Midpoint(&vertices[0], &vertices[1]);
  const RoomVertex right = Midpoint(&vertices[1], &vertices[3]);
  const RoomVertex bottom = Midpoint(&vertices[2], &vertices[3]);
  const RoomVertex left = Midpoint(&vertices[0], &vertices[2]);
  const RoomVertex center = Midpoint(&vertices[1], &vertices[2]);
  const RoomVertex children[kQuadCorners][kQuadCorners] = {
      {vertices[0], top, left, center},
      {vertices[1], top, right, center},
      {vertices[3], bottom, right, center},
      {vertices[2], bottom, left, center}};
  const RoomVertex edges[kQuadCorners][kTriangleCorners] = {
      {vertices[2], bottom, vertices[3]},
      {vertices[2], vertices[0], left},
      {top, vertices[0], vertices[1]},
      {vertices[3], right, vertices[1]}};
  int emitted = 0;
  for (unsigned child = 0; child < kQuadCorners; ++child) {
    const int result =
        SubdivideRoomQuad(children[child], levels - 1, material, limit);
    if (result < 0) return result;
    emitted += result;
  }
  for (unsigned edge = 0; edge < kQuadCorners; ++edge) {
    const int result = SubdivideRoomTriangle(edges[edge], 0, material, limit);
    if (result < 0) return result;
    emitted += result;
  }
  return emitted;
}

static int QueueRoomPolygon(const unsigned char* data, unsigned flags,
                            int corners, int mode, uintptr_t limit) {
  RoomVertex vertices[kQuadCorners] = {0};
  memcpy(&vertices[0].position, data, kRoomSpatialDimensions * sizeof(int16_t));
  const int scale = (flags & 0x800) != 0 ? 1 : 8;
  const unsigned delta_offsets[4] = {0, 6, 9, 32};
  const unsigned color_offsets[4] = {12, 16, 20, 36};
  const unsigned u_offsets[4] = {24, 28, 19, 35};
  const unsigned v_offsets[4] = {25, 29, 23, 39};
  Core_GteWord screen[kQuadCorners];
  Core_GteWord depth = 0;
  Core_GteWord subdivision_depth = 0;
  int left = kProjectedCoordinateBound, top = kProjectedCoordinateBound;
  int right = -kProjectedCoordinateBound, bottom = -kProjectedCoordinateBound;
  for (int index = 0; index < corners; ++index) {
    if (index != 0) {
      const signed char* delta =
          (const signed char*)data + delta_offsets[index];
      vertices[index].position.vx =
          (int16_t)(vertices[0].position.vx + delta[0] * scale);
      vertices[index].position.vy =
          (int16_t)(vertices[0].position.vy + delta[1] * scale);
      vertices[index].position.vz =
          (int16_t)(vertices[0].position.vz + delta[2] * scale);
    }
    const unsigned char* color = data + color_offsets[index];
    vertices[index].color = color[0] | ((unsigned)color[1] << CHAR_BIT) |
                            ((unsigned)color[2] << (2 * CHAR_BIT));
    vertices[index].uv =
        data[u_offsets[index]] | ((uint16_t)data[v_offsets[index]] << CHAR_BIT);
    Core_GteWord fog, projection_flags;
    const Core_GteWord vertex_depth = Core_Gte_RotTransPers(
        &vertices[index].position, &screen[index], &fog, &projection_flags);
    if (vertex_depth > depth) {
      depth = vertex_depth;
    }
    if (index == 2) subdivision_depth = depth;
    const int x = (int16_t)screen[index];
    const int y = (int16_t)((uint32_t)screen[index] >> kPackedCoordinateBits);
    if (x < left) left = x;
    if (x > right) right = x;
    if (y < top) top = y;
    if (y > bottom) bottom = y;
  }
  if (right < 0 || bottom < 0 || left >= kViewportWidth || top >= kDrawHeight ||
      depth < room_near_depth) {
    return 0;
  }
  const Core_GteWord winding =
      Core_Gte_NormalClip(screen[0], screen[1], screen[2]);
  if ((data[kRoomCommandOffset] & 1) == 0 && winding <= 0 &&
      (corners == kTriangleCorners ||
       Core_Gte_NormalClip(screen[1], screen[kQuadCorners - 1], screen[2]) <=
           0)) {
    return 0;
  }
  const int subdivision =
      (int)((flags >> 13) & 3) - (int)((uint32_t)subdivision_depth >> 7);
  Core_GteWord subdivision_area = winding;
  if (corners == kTriangleCorners && subdivision_area < 0) {
    subdivision_area = -subdivision_area;
  }
  if (corners == kQuadCorners && winding < 0 &&
      (data[kRoomCommandOffset] & 1) == 0) {
    subdivision_area =
        Core_Gte_NormalClip(screen[1], screen[kQuadCorners - 1], screen[2]);
  }
  if ((mode & 1) != 0 &&
      (uint32_t)subdivision_area >= kMinimumSubdivisionArea &&
      subdivision > 0) {
    uint16_t clut, page;
    memcpy(&clut, data + kClutRecordOffset, sizeof clut);
    memcpy(&page, data + kPageRecordOffset, sizeof page);
    const RoomPolygonMaterial material = {.flags = flags,
                                          .command = data[kRoomCommandOffset],
                                          .clut = clut,
                                          .page = page};
    return corners == kTriangleCorners
               ? SubdivideRoomTriangle(vertices, subdivision, &material, limit)
               : SubdivideRoomQuad(vertices, subdivision, &material, limit);
  }
  depth += (int16_t)(flags >> kPackedCoordinateBits);
  if (depth < room_near_depth || depth >= kLastOrderingSlot) {
    return 0;
  }
  uint16_t clut, page;
  memcpy(&clut, data + kClutRecordOffset, sizeof clut);
  memcpy(&page, data + kPageRecordOffset, sizeof page);
  const ProjectedRoomPolygon polygon = {
      .vertices = vertices,
      .screen = screen,
      .corners = corners,
      .depth = depth,
      .material = {.flags = flags,
                   .command = data[kRoomCommandOffset],
                   .clut = clut,
                   .page = page},
      .low_detail = (uint32_t)depth >= room_lod_depth};
  return WriteRoomPolygon(&polygon, limit);
}

// decomp: BATTLE.PRG func_8009723C / func_80097388
// (src/BATTLE/BATTLE.PRG/2EA3C.s:10,112).
void Render_DrawRoomGeometry(void* data, int mode) {
  if (data == NULL || Render_frameScratch.orderingTable == NULL) {
    return;
  }
  const uintptr_t base =
      (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  const uintptr_t start = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (base == 0 || start < base || start - base > kPrimitiveBufferBytes) {
    return;
  }
  uintptr_t limit = base + kPrimitiveBufferBytes;
  if (limit - start > kRoomBatchBytes) {
    limit = start + kRoomBatchBytes;
  }
  unsigned count;
  memcpy(&count, data, sizeof count);
  if (count > kMaximumGeometryGroups) {
    return;
  }
  const Render_RoomGeometryGroup* groups =
      (void*)((unsigned char*)data + sizeof count);
  for (unsigned index = 0; index < count; ++index) {
    const Render_RoomGeometryGroup* group = &groups[index];
    const unsigned facing = Render_frameScratch.firstPersonViewFacing;
    if ((group->flags & kSkipGeometryFlags) != 0 ||
        (((group->flags >> kAlternatePassShift) & 1) & (unsigned)mode) != 0 ||
        ((facing >> kPackedCoordinateBits) == 0 &&
         ((~group->flags) & (facing & UINT8_MAX)) != 0)) {
      continue;
    }
    Core_Gte_SetRotMatrix(&group->view);
    Core_Gte_SetTransMatrix(&group->view);
    const unsigned char* polygon =
        (const unsigned char*)data + group->polygonOffset;
    unsigned counts[2];
    memcpy(counts, polygon, sizeof counts);
    polygon += sizeof counts;
    for (int kind = 0; kind < 2; ++kind) {
      for (unsigned face = 0; face < counts[kind]; ++face) {
        if (QueueRoomPolygon(polygon, group->flags,
                             kind == 0 ? kTriangleCorners : kQuadCorners, mode,
                             limit) < 0) {
          return;
        }
        polygon += kind == 0 ? kRoomTriangleBytes : kRoomQuadBytes;
      }
    }
  }
}

// decomp: BATTLE.PRG func_80098160 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_SetRoomPolygonNearClip(int depth) {
  room_near_depth = (int16_t)depth;
}

// decomp: BATTLE.PRG func_80098194 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_SetRoomTextureLodDepth(int depth) {
  room_lod_depth = (unsigned)(int)(int16_t)depth;
}

// decomp: BATTLE.PRG func_8009820C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_DrawSkyDome(void* data) {
  if (data == NULL || Render_frameScratch.orderingTable == NULL) return;
  const uintptr_t base =
      (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  const uintptr_t start = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (base == 0 || start < base || start - base > kPrimitiveBufferBytes) return;
  VECTOR positions[kSkyVertexCount];
  unsigned colors[kSkyVertexCount];
  const unsigned char* bytes = data;
  for (unsigned vertex = 0; vertex < kSkyVertexCount; ++vertex) {
    SVECTOR position;
    Core_GteWord flags;
    memcpy(&position, bytes + (size_t)vertex * kSkyVertexBytes,
           sizeof position);
    memcpy(&colors[vertex],
           bytes + (size_t)vertex * kSkyVertexBytes + sizeof position,
           sizeof colors[vertex]);
    Core_Gte_RotTrans(&position, &positions[vertex], &flags);
  }
  bytes += (size_t)kSkyVertexCount * kSkyVertexBytes;
  for (unsigned face = 0; face < kSkyFaceCount; ++face) {
    const unsigned char* indices = bytes + (size_t)face * kSkyFaceBytes;
    if (indices[0] >= kSkyVertexCount || indices[1] >= kSkyVertexCount ||
        indices[2] >= kSkyVertexCount) {
      return;
    }
    if (positions[indices[0]].vz <= 0 && positions[indices[1]].vz <= 0 &&
        positions[indices[2]].vz <= 0) {
      continue;
    }
    int left = INT32_MAX, right = INT32_MIN, top = INT32_MAX,
        bottom = INT32_MIN;
    Core_GteWord xy[kTriangleCorners];
    for (int corner = 0; corner < kTriangleCorners; ++corner) {
      const VECTOR* position = &positions[indices[corner]];
      const int x = position->vx + kViewportWidth / 2;
      const int y = position->vy + kDrawHeight / 2;
      if (x < left) left = x;
      if (x > right) right = x;
      if (y < top) top = y;
      if (y > bottom) bottom = y;
      xy[corner] = (Core_GteWord)((uint16_t)x | ((unsigned)(uint16_t)y
                                                 << kPackedCoordinateBits));
    }
    if (right < 0 || left >= kViewportWidth || bottom < 0 ||
        top >= kViewportHeight) {
      continue;
    }
    const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
    if (cursor > base + kPrimitiveBufferBytes - sizeof(POLY_G3)) return;
    u_long* packet = (void*)cursor;
    memset(packet, 0, sizeof(POLY_G3));
    for (int corner = 0; corner < kTriangleCorners; ++corner) {
      packet[1 + corner * 2] = colors[indices[corner]] & kColorMask;
      packet[2 + corner * 2] = (u_long)xy[corner];
    }
    packet[1] |= kSkyGouraudCommand;
    setlen(packet, kSkyPacketWords - 1);
    AddPrim((u_long*)Render_frameScratch.orderingTable + kLastOrderingSlot,
            packet);
    Render_frameScratch.primitiveCursor = packet + kSkyPacketWords;
  }
}

// decomp: BATTLE.PRG vs_battle_getGeomOffset (2EA3C.s:2470).
void Render_GetProjectionScreenOffset(Render_ScreenOffset* offset) {
  Core_GteWord x;
  Core_GteWord y;
  Core_Gte_GetGeomOffset(&x, &y);
  offset->x = (u_short)x;
  offset->y = (u_short)y;
}

// decomp: BATTLE.PRG vs_battle_setGeomOffset (2EA3C.s:2482).
void Render_SetProjectionScreenOffset(Render_ScreenOffset* offset) {
  Core_Gte_SetGeomOffset(offset->x, offset->y);
}
