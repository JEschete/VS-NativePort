// Hand-maintained (tools/port/owned.csv).
#include "Render/RoomDraw.h"

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

int Render_ValidateRoomGeometry(const void* data, size_t size) {
  if (data == NULL || size < sizeof(unsigned)) {
    return 0;
  }
  unsigned count;
  memcpy(&count, data, sizeof count);
  const size_t header_end =
      sizeof count + (size_t)count * sizeof(Render_RoomGeometryGroup);
  if (count > 4096 || header_end > size) {
    return 0;
  }
  for (unsigned index = 0; index < count; ++index) {
    Render_RoomGeometryGroup group;
    memcpy(&group,
           (const unsigned char*)data + sizeof count + index * sizeof group,
           sizeof group);
    if (group.polygonOffset < header_end || group.polygonOffset > size ||
        size - group.polygonOffset < 8) {
      return 0;
    }
    unsigned counts[2];
    memcpy(counts, (const unsigned char*)data + group.polygonOffset,
           sizeof counts);
    if ((uint64_t)counts[0] * 32 + (uint64_t)counts[1] * 40 >
        size - group.polygonOffset - 8) {
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
        (int16_t)(0x1000000 / scale);
    Core_Math_MultiplyMatrixIntoRight(&inverse, &camera);
  }
  SVECTOR rotation = {(int16_t)angles->vx, (int16_t)angles->vy,
                      (int16_t)angles->vz, 0};
  Core_Gte_RotMatrixYXZ(&rotation, output);
  Core_Math_MultiplyMatrixIntoRight(&camera, output);
  const int16_t coordinates[3] = {(int16_t)position->vx, (int16_t)position->vy,
                                  (int16_t)position->vz};
  for (int row = 0; row < 3; ++row) {
    int64_t translated = 0;
    for (int axis = 0; axis < 3; ++axis) {
      translated += (int64_t)camera.m[row][axis] * coordinates[axis];
    }
    output->t[row] = (Core_GteWord)((translated >> 12) +
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
  if (count > 4096) {
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
    if ((group->flags & 0x100) != 0) {
      continue;
    }
    VECTOR position = {group->bounds[0][0], group->bounds[1][0],
                       group->bounds[2][0], 0};
    VECTOR angles = {group->angles[0], group->angles[1], group->angles[2], 0};
    BuildRoomView(&position, &angles, &group->view, 0);
    if ((group->flags & 0x400) != 0) {
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

static int roomNearDepth = 8;
static unsigned roomLodDepth = 512;

enum {
  kViewportWidth = 320,
  kViewportHeight = 224,
  kTriangleCorners = 3,
  kQuadCorners = 4,
  kQuadCommandFlag = 8
};

// decomp: BATTLE.PRG func_80097F70 (src/BATTLE/BATTLE.PRG/2EA3C.s).
static RoomVertex Midpoint(const RoomVertex* first, const RoomVertex* second) {
  RoomVertex midpoint = {0};
  midpoint.position.vx =
      (int16_t)(((int)first->position.vx + second->position.vx) >> 1);
  midpoint.position.vy =
      (int16_t)(((int)first->position.vy + second->position.vy) >> 1);
  midpoint.position.vz =
      (int16_t)(((int)first->position.vz + second->position.vz) >> 1);
  for (unsigned channel = 0; channel < 3; ++channel) {
    const unsigned value = ((first->color >> (channel * 8)) & 255) +
                           ((second->color >> (channel * 8)) & 255);
    midpoint.color |= (value >> 1) << (channel * 8);
  }
  midpoint.uv =
      (uint16_t)((((first->uv & 255) + (second->uv & 255)) >> 1) |
                 ((((first->uv >> 8) + (second->uv >> 8)) >> 1) << 8));
  return midpoint;
}

static int EmitSubdividedPolygon(const RoomVertex* vertices, int corners,
                                 unsigned flags, unsigned command,
                                 unsigned clut, unsigned page,
                                 uintptr_t limit) {
  Core_GteWord screen[4], depth = 0;
  int left = 1024, top = 1024, right = -1024, bottom = -1024;
  for (int index = 0; index < corners; ++index) {
    SVECTOR position = vertices[index].position;
    Core_GteWord fog, projection_flags;
    const Core_GteWord current = Core_Gte_RotTransPers(
        &position, &screen[index], &fog, &projection_flags);
    if (current > depth) depth = current;
    const int screen_x = (int16_t)screen[index],
              screen_y = (int16_t)((uint32_t)screen[index] >> 16);
    if (screen_x < left) left = screen_x;
    if (screen_x > right) right = screen_x;
    if (screen_y < top) top = screen_y;
    if (screen_y > bottom) bottom = screen_y;
  }
  if (right < 0 || bottom < 0 || left >= kViewportWidth ||
      top >= kViewportHeight || depth < roomNearDepth) {
    return 0;
  }
  depth += (int16_t)(flags >> 16);
  if (depth < roomNearDepth || depth >= 2047) return 0;
  const size_t bytes = (1 + (size_t)corners * 3) * sizeof(u_long);
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (cursor > limit || bytes > limit - cursor) return -1;
  u_long* packet = (void*)cursor;
  memset(packet, 0, bytes);
  for (int index = 0; index < corners; ++index) {
    packet[1 + index * 3] = vertices[index].color;
    packet[2 + index * 3] = (u_long)screen[index];
    packet[3 + index * 3] = vertices[index].uv;
  }
  packet[1] |= (command & 0xFE) << 24;
  packet[3] |= clut << 16;
  packet[6] |= page << 16;
  setlen(packet, corners * 3);
  AddPrim((u_long*)Render_frameScratch.orderingTable + depth, packet);
  Render_frameScratch.primitiveCursor = (unsigned char*)packet + bytes;
  return 1;
}

// decomp: BATTLE.PRG func_80097AEC (src/BATTLE/BATTLE.PRG/2EA3C.s:612).
static int SubdivideRoomTriangle(const RoomVertex vertices[3], int levels,
                                 unsigned flags, unsigned command,
                                 unsigned clut, unsigned page,
                                 uintptr_t limit) {
  if (levels == 0) {
    return EmitSubdividedPolygon(vertices, kTriangleCorners, flags,
                                 command & ~(unsigned)kQuadCommandFlag, clut,
                                 page, limit);
  }
  const RoomVertex first = Midpoint(&vertices[0], &vertices[1]);
  const RoomVertex second = Midpoint(&vertices[1], &vertices[2]);
  const RoomVertex third = Midpoint(&vertices[2], &vertices[0]);
  const RoomVertex children[7][3] = {
      {vertices[0], first, third},       {second, first, vertices[1]},
      {second, vertices[2], third},      {first, second, third},
      {vertices[2], vertices[0], third}, {vertices[2], second, vertices[1]},
      {first, vertices[0], vertices[1]}};
  int emitted = 0;
  for (unsigned child = 0; child < 7; ++child) {
    const int result =
        SubdivideRoomTriangle(children[child], child < 4 ? levels - 1 : 0,
                              flags, command, clut, page, limit);
    if (result < 0) return result;
    emitted += result;
  }
  return emitted;
}

// decomp: BATTLE.PRG func_80097CF0 (src/BATTLE/BATTLE.PRG/2EA3C.s:752).
static int SubdivideRoomQuad(const RoomVertex vertices[4], int levels,
                             unsigned flags, unsigned command, unsigned clut,
                             unsigned page, uintptr_t limit) {
  if (levels == 0) {
    return EmitSubdividedPolygon(vertices, kQuadCorners, flags, command, clut,
                                 page, limit);
  }
  const RoomVertex top = Midpoint(&vertices[0], &vertices[1]);
  const RoomVertex right = Midpoint(&vertices[1], &vertices[3]);
  const RoomVertex bottom = Midpoint(&vertices[2], &vertices[3]);
  const RoomVertex left = Midpoint(&vertices[0], &vertices[2]);
  const RoomVertex center = Midpoint(&vertices[1], &vertices[2]);
  const RoomVertex children[4][4] = {{vertices[0], top, left, center},
                                     {vertices[1], top, right, center},
                                     {vertices[3], bottom, right, center},
                                     {vertices[2], bottom, left, center}};
  const RoomVertex edges[4][3] = {{vertices[2], bottom, vertices[3]},
                                  {vertices[2], vertices[0], left},
                                  {top, vertices[0], vertices[1]},
                                  {vertices[3], right, vertices[1]}};
  int emitted = 0;
  for (unsigned child = 0; child < 4; ++child) {
    const int result = SubdivideRoomQuad(children[child], levels - 1, flags,
                                         command, clut, page, limit);
    if (result < 0) return result;
    emitted += result;
  }
  for (unsigned edge = 0; edge < 4; ++edge) {
    const int result = SubdivideRoomTriangle(edges[edge], 0, flags, command,
                                             clut, page, limit);
    if (result < 0) return result;
    emitted += result;
  }
  return emitted;
}

static int QueueRoomPolygon(const unsigned char* data, unsigned flags,
                            int corners, int mode, uintptr_t limit) {
  RoomVertex vertices[4] = {0};
  memcpy(&vertices[0].position, data, 6);
  const int scale = (flags & 0x800) != 0 ? 1 : 8;
  const unsigned delta_offsets[4] = {0, 6, 9, 32};
  const unsigned color_offsets[4] = {12, 16, 20, 36};
  const unsigned u_offsets[4] = {24, 28, 19, 35};
  const unsigned v_offsets[4] = {25, 29, 23, 39};
  Core_GteWord screen[4];
  Core_GteWord depth = 0;
  Core_GteWord subdivision_depth = 0;
  int left = 1024, top = 1024, right = -1024, bottom = -1024;
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
    vertices[index].color =
        color[0] | ((unsigned)color[1] << 8) | ((unsigned)color[2] << 16);
    vertices[index].uv =
        data[u_offsets[index]] | ((uint16_t)data[v_offsets[index]] << 8);
    Core_GteWord fog, projection_flags;
    const Core_GteWord vertex_depth = Core_Gte_RotTransPers(
        &vertices[index].position, &screen[index], &fog, &projection_flags);
    if (vertex_depth > depth) {
      depth = vertex_depth;
    }
    if (index == 2) subdivision_depth = depth;
    const int x = (int16_t)screen[index];
    const int y = (int16_t)((uint32_t)screen[index] >> 16);
    if (x < left) left = x;
    if (x > right) right = x;
    if (y < top) top = y;
    if (y > bottom) bottom = y;
  }
  if (right < 0 || bottom < 0 || left >= 320 || top >= 240 ||
      depth < roomNearDepth) {
    return 0;
  }
  const Core_GteWord winding =
      Core_Gte_NormalClip(screen[0], screen[1], screen[2]);
  if ((data[15] & 1) == 0 && winding <= 0 &&
      (corners == 3 ||
       Core_Gte_NormalClip(screen[1], screen[3], screen[2]) <= 0)) {
    return 0;
  }
  const int subdivision =
      (int)((flags >> 13) & 3) - (int)((uint32_t)subdivision_depth >> 7);
  Core_GteWord subdivision_area = winding;
  if (corners == 3 && subdivision_area < 0)
    subdivision_area = -subdivision_area;
  if (corners == 4 && winding < 0 && (data[15] & 1) == 0) {
    subdivision_area = Core_Gte_NormalClip(screen[1], screen[3], screen[2]);
  }
  if ((mode & 1) != 0 && (uint32_t)subdivision_area >= 512 && subdivision > 0) {
    uint16_t clut, page;
    memcpy(&clut, data + 26, sizeof clut);
    memcpy(&page, data + 30, sizeof page);
    return corners == 3 ? SubdivideRoomTriangle(vertices, subdivision, flags,
                                                data[15], clut, page, limit)
                        : SubdivideRoomQuad(vertices, subdivision, flags,
                                            data[15], clut, page, limit);
  }
  depth += (int16_t)(flags >> 16);
  if (depth < roomNearDepth || depth >= 2047) {
    return 0;
  }
  const size_t bytes = (1 + (size_t)corners * 3) * sizeof(u_long);
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (cursor > limit || bytes > limit - cursor) {
    return -1;
  }
  u_long* packet = (void*)cursor;
  memset(packet, 0, bytes);
  uint16_t clut, page;
  memcpy(&clut, data + 26, sizeof clut);
  memcpy(&page, data + 30, sizeof page);
  const int low_detail = (uint32_t)depth >= roomLodDepth;
  if (low_detail) {
    --page;
  }
  for (int index = 0; index < corners; ++index) {
    const unsigned uv =
        low_detail ? (vertices[index].uv >> 1) & 0x7F7F : vertices[index].uv;
    packet[1 + index * 3] = vertices[index].color;
    packet[2 + index * 3] = (u_long)screen[index];
    packet[3 + index * 3] = uv;
  }
  packet[1] |= (unsigned)(data[15] & 0xFE) << 24;
  packet[3] |= (unsigned)clut << 16;
  packet[6] |= (unsigned)page << 16;
  setlen(packet, corners * 3);
  AddPrim((u_long*)Render_frameScratch.orderingTable + depth, packet);
  Render_frameScratch.primitiveCursor = (unsigned char*)packet + bytes;
  return 1;
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
  if (base == 0 || start < base || start - base > 0x20000) {
    return;
  }
  uintptr_t limit = base + 0x20000;
  if (limit - start > 0x10000) {
    limit = start + 0x10000;
  }
  unsigned count;
  memcpy(&count, data, sizeof count);
  if (count > 4096) {
    return;
  }
  const Render_RoomGeometryGroup* groups =
      (void*)((unsigned char*)data + sizeof count);
  for (unsigned index = 0; index < count; ++index) {
    const Render_RoomGeometryGroup* group = &groups[index];
    const unsigned facing = Render_frameScratch.firstPersonViewFacing;
    if ((group->flags & 0x1100) != 0 ||
        (((group->flags >> 15) & 1) & (unsigned)mode) != 0 ||
        ((facing >> 16) == 0 && ((~group->flags) & (facing & 255)) != 0)) {
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
        if (QueueRoomPolygon(polygon, group->flags, kind == 0 ? 3 : 4, mode,
                             limit) < 0) {
          return;
        }
        polygon += kind == 0 ? 32 : 40;
      }
    }
  }
}

// decomp: BATTLE.PRG func_80098160 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_SetRoomPolygonNearClip(int depth) {
  roomNearDepth = (int16_t)depth;
}

// decomp: BATTLE.PRG func_80098194 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_SetRoomTextureLodDepth(int depth) {
  roomLodDepth = (unsigned)(int)(int16_t)depth;
}

// decomp: BATTLE.PRG func_8009820C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file:
// not yet ported]
void Render_DrawSkyDome(void* data) {
  if (data == NULL || Render_frameScratch.orderingTable == NULL) return;
  const uintptr_t base =
      (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  const uintptr_t start = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (base == 0 || start < base || start - base > 0x20000) return;
  VECTOR positions[34];
  unsigned colors[34];
  const unsigned char* bytes = data;
  for (unsigned vertex = 0; vertex < 34; ++vertex) {
    SVECTOR position;
    Core_GteWord flags;
    memcpy(&position, bytes + vertex * 12, sizeof position);
    memcpy(&colors[vertex], bytes + vertex * 12 + 8, sizeof colors[vertex]);
    Core_Gte_RotTrans(&position, &positions[vertex], &flags);
  }
  bytes += 34 * 12;
  for (unsigned face = 0; face < 64; ++face) {
    const unsigned char* indices = bytes + face * 4;
    if (indices[0] >= 34 || indices[1] >= 34 || indices[2] >= 34) return;
    if (positions[indices[0]].vz <= 0 && positions[indices[1]].vz <= 0 &&
        positions[indices[2]].vz <= 0) {
      continue;
    }
    int left = INT32_MAX, right = INT32_MIN, top = INT32_MAX,
        bottom = INT32_MIN;
    Core_GteWord xy[3];
    for (int corner = 0; corner < 3; ++corner) {
      const VECTOR* position = &positions[indices[corner]];
      const int x = position->vx + 160;
      const int y = position->vy + 120;
      if (x < left) left = x;
      if (x > right) right = x;
      if (y < top) top = y;
      if (y > bottom) bottom = y;
      xy[corner] = (Core_GteWord)((uint16_t)x | ((unsigned)(uint16_t)y << 16));
    }
    if (right < 0 || left >= 320 || bottom < 0 || top >= 224) continue;
    const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
    if (cursor > base + 0x20000 - sizeof(POLY_G3)) return;
    u_long* packet = (void*)cursor;
    memset(packet, 0, sizeof(POLY_G3));
    for (int corner = 0; corner < 3; ++corner) {
      packet[1 + corner * 2] = colors[indices[corner]] & 0xFFFFFF;
      packet[2 + corner * 2] = (u_long)xy[corner];
    }
    packet[1] |= 0x30000000;
    setlen(packet, 6);
    AddPrim((u_long*)Render_frameScratch.orderingTable + 2047, packet);
    Render_frameScratch.primitiveCursor = packet + 7;
  }
}
