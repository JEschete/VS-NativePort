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
  kFlatIndexOffset = sizeof(uint32_t),
  kFlatTriangleUvOffset =
      kFlatIndexOffset + kTriangleCorners * sizeof(uint16_t),
  kFlatQuadUvOffset = kFlatIndexOffset + kQuadCorners * sizeof(uint16_t),
  kGouraudTriangleBytes = 24,
  kGouraudQuadBytes = 32,
  kColorMask = 0xFFFFFF,
  kPrimitiveBufferBytes = 0x20000,
  kNormalDepthBias = -1,
  kSpecialDepthBias = -5,
  kFlatVertexWords = 2,
  kGouraudVertexWords = 3,
  kGouraudFirstUvOffset = 6,
  kGouraudTriangleTailUvOffset = 20,
  kGouraudQuadUvOffset = 24,
  kBlendModeMask = 0x60,
  kSemiTransparencyFlag = 0x10,
  kAppearanceMaterialMask = 0xF0,
  kAppearanceSubtractiveBlend = 0x40,
  kAppearanceAdditiveBlend = 0x20,
  kAppearanceFadeFirstState = 4,
  kAppearanceFadeEndState = 9,
  kAppearanceOpaqueState = 6,
  kAppearanceBlendBoundary = 7,
  kAppearanceForcedOverride = 15,
  kGouraudSpecialFlagsOffset = 19,
  kFlatSpecialFlagsOffset = 2,
  kQuadCommandFlag = 8,
  kCommandShift = 3 * CHAR_BIT,
  kClutPacketWord = 3,
  kFlatPagePacketWord = 5,
  kGouraudPagePacketWord = 6,
  kPolygonGroupCount = 3,
  kSpecialPolygonGroup = 2,
  kProjectedCoordinateBound = 1024,
  kViewportWidth = 320,
  kViewportHeight = 240,
  kAdaptiveSplitMode = 3,
  kTriangleDepthReciprocal = 341,
  kDepthReciprocalBits = 12,
  kQuadDepthShift = 4,
  kDepthBiasMask = 3,
  kDepthBiasStep = 4,
  kModelOrderingLimit = 0x7C0,
  kLimbPaletteGroups = 8,
  kFirstLimbPaletteBit = 0x80,
  kPulsedClutOffset = 0x480,
  kNeutralTint = 0x808080,
  kWeaponPaletteMatrix = 2,
  kWeaponPaletteColumn = 2
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

static unsigned PolygonBytes(unsigned command) {
  switch (command) {
    case kFlatTriangleCommand:
      return kFlatTriangleBytes;
    case kFlatQuadCommand:
      return kFlatQuadBytes;
    case kGouraudTriangleCommand:
      return kGouraudTriangleBytes;
    case kGouraudQuadCommand:
      return kGouraudQuadBytes;
    default:
      return 0;
  }
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
    const unsigned bone = groups[(size_t)group * kGroupWords];
    const unsigned end = groups[(size_t)group * kGroupWords + 1];
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
    const unsigned end = groups[(size_t)group * kGroupWords + 1];
    const MATRIX* matrix = &matrices[groups[(size_t)group * kGroupWords]];
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
  const unsigned counts[kPolygonGroupCount] = {header.triangles, header.quads,
                                               header.other};
  size_t offset = model->polygonSection;
  const unsigned char* bytes = data;
  const int colored = (((const unsigned char*)instance)[8] & 8) != 0;
  for (int part = 0; part < kPolygonGroupCount; ++part) {
    for (unsigned face = 0; face < counts[part]; ++face) {
      const unsigned prefix = colored ? 12 : 2;
      if (size - offset < prefix) return 0;
      const unsigned command = bytes[offset + (colored ? 11 : 0)];
      const int corners = PolygonCorners(command, colored);
      if (corners == 0 || (part != kSpecialPolygonGroup &&
                           corners != part + kTriangleCorners)) {
        return 0;
      }
      const unsigned extent = PolygonBytes(command);
      if (extent > size - offset) return 0;
      offset += extent;
    }
  }
  return 1;
}

static void BuildAdaptiveTriangles(
    const Actors_ModelRenderContext* context, const unsigned char* data,
    const uint16_t* indices, const Core_GteWord* screen,
    unsigned char triangles[2][kFlatTriangleBytes]) {
  int delta[kTriangleCorners][kTriangleCorners];
  for (int vertex = 0; vertex < kTriangleCorners; ++vertex) {
    delta[vertex][0] = (int16_t)screen[vertex + 1] - (int16_t)screen[0];
    delta[vertex][1] =
        (int16_t)((uint32_t)screen[vertex + 1] >> kCoordinateBits) -
        (int16_t)((uint32_t)screen[0] >> kCoordinateBits);
    const int difference =
        context->vertexDepths[indices[vertex + 1] / sizeof(Core_GteWord)] -
        context->vertexDepths[indices[0] / sizeof(Core_GteWord)];
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
  const unsigned normal[2][kTriangleCorners] = {{0, 1, 2}, {1, 3, 2}};
  const unsigned alternate[2][kTriangleCorners] = {{0, 1, 3}, {0, 3, 2}};
  const unsigned (*order)[kTriangleCorners] =
      (int32_t)(uint32_t)volume > 0 ? normal : alternate;
  for (int part = 0; part < 2; ++part) {
    unsigned char* triangle = triangles[part];
    triangle[0] = kFlatTriangleCommand;
    triangle[1] = kFlatTriangleBytes;
    for (int corner = 0; corner < kTriangleCorners; ++corner) {
      const unsigned vertex = order[part][corner];
      memcpy(triangle + kFlatIndexOffset + (size_t)corner * sizeof(uint16_t),
             &indices[vertex], sizeof indices[vertex]);
      memcpy(triangle + kFlatTriangleUvOffset +
                 (size_t)((corner + 1) % kTriangleCorners) * sizeof(uint16_t),
             data + kFlatQuadUvOffset + (size_t)vertex * sizeof(uint16_t),
             sizeof(uint16_t));
    }
  }
}

typedef struct ProjectedModelPolygon {
  const unsigned char* data;
  const Core_GteWord* screen;
  int corners;
  int colored;
  unsigned flags;
  int depth;
  unsigned tint;
  unsigned palette;
  int uniform_color;
} ProjectedModelPolygon;

static int WriteModelPolygon(const ProjectedModelPolygon* polygon,
                             uintptr_t limit) {
  const int corners = polygon->corners;
  const int colored = polygon->colored;
  const size_t vertex_words = colored ? kGouraudVertexWords : kFlatVertexWords;
  const size_t words = (colored ? 1 : 2) + (size_t)corners * vertex_words;
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (cursor > limit || words * sizeof(u_long) > limit - cursor) return -1;
  u_long* packet = (void*)cursor;
  memset(packet, 0, words * sizeof *packet);
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  const unsigned page =
      workspace->texturePage | (polygon->flags & kBlendModeMask);
  for (int corner = 0; corner < corners; ++corner) {
    const unsigned uv_corner = corners == kTriangleCorners
                                   ? (unsigned)(corner + 1) % kTriangleCorners
                                   : (unsigned)corner;
    const size_t uv_offset =
        colored ? (corners == kTriangleCorners
                       ? (uv_corner == 0
                              ? kGouraudFirstUvOffset
                              : kGouraudTriangleTailUvOffset +
                                    (size_t)(uv_corner - 1) * sizeof(uint16_t))
                       : kGouraudQuadUvOffset +
                             (size_t)uv_corner * sizeof(uint16_t))
                : kFlatIndexOffset +
                      (size_t)(corners + uv_corner) * sizeof(uint16_t);
    uint16_t uv;
    memcpy(&uv, polygon->data + uv_offset, sizeof uv);
    const size_t xy_word = 2 + (size_t)corner * vertex_words;
    packet[xy_word] = (u_long)polygon->screen[corner];
    packet[xy_word + 1] = uv;
    if (colored) {
      unsigned color = polygon->tint;
      if (!polygon->uniform_color) {
        memcpy(&color,
               polygon->data + 2 * sizeof(uint32_t) +
                   (size_t)corner * sizeof(uint32_t),
               sizeof color);
      }
      packet[xy_word - 1] = color & kColorMask;
    }
  }
  const unsigned command =
      (colored ? kGouraudTriangleCommand : kFlatTriangleCommand) |
      (corners == kQuadCorners ? kQuadCommandFlag : 0) |
      ((polygon->flags & kSemiTransparencyFlag) != 0 ? 2u : 0u);
  if (!colored) packet[1] = polygon->tint & kColorMask;
  packet[1] |= command << kCommandShift;
  packet[kClutPacketWord] |= polygon->palette << kCoordinateBits;
  packet[colored ? kGouraudPagePacketWord : kFlatPagePacketWord] |=
      page << kCoordinateBits;
  setlen(packet, words - 1);
  AddPrim((u_long*)Render_frameScratch.orderingTable + polygon->depth, packet);
  Render_frameScratch.primitiveCursor = packet + words;
  return 1;
}

static int QueueModelPolygon(Actors_ModelRenderContext* context,
                             const unsigned char* data, int corners,
                             int colored, unsigned flags, int bias,
                             unsigned tint, unsigned palette, int uniform_color,
                             uintptr_t limit) {
  uint16_t indices[kQuadCorners];
  memcpy(indices, data + (colored ? 0 : sizeof(uint32_t)),
         (size_t)corners * sizeof indices[0]);
  unsigned depths = 0;
  Core_GteWord screen[kQuadCorners];
  int left = kProjectedCoordinateBound, top = kProjectedCoordinateBound;
  int right = -kProjectedCoordinateBound, bottom = -kProjectedCoordinateBound;
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
  if (!colored && corners == kQuadCorners &&
      (flags & kAdaptiveSplitMode) == kAdaptiveSplitMode) {
    unsigned char triangles[2][kFlatTriangleBytes] = {{0}};
    BuildAdaptiveTriangles(context, data, indices, screen, triangles);
    int emitted = 0;
    for (int part = 0; part < 2; ++part) {
      const int result =
          QueueModelPolygon(context, triangles[part], kTriangleCorners, 0,
                            flags & kAppearanceMaterialMask, kNormalDepthBias,
                            tint, palette, uniform_color, limit);
      if (result < 0) return result;
      emitted += result;
    }
    return emitted;
  }
  if (right < 0 || bottom < 0 || left >= kViewportWidth ||
      top >= kViewportHeight ||
      ((flags & 1) == 0 &&
       Core_Gte_NormalClip(screen[0], screen[1], screen[2]) <= 0)) {
    return 0;
  }
  int depth = (int)(corners == kTriangleCorners
                        ? (depths * (unsigned)kTriangleDepthReciprocal) >>
                              kDepthReciprocalBits
                        : depths >> kQuadDepthShift);
  depth += bias + (int)((flags >> 2) & kDepthBiasMask) * kDepthBiasStep;
  if (depth < 0 || depth >= kModelOrderingLimit ||
      Render_frameScratch.orderingTable == NULL) {
    return 0;
  }
  const ProjectedModelPolygon polygon = {.data = data,
                                         .screen = screen,
                                         .corners = corners,
                                         .colored = colored,
                                         .flags = flags,
                                         .depth = depth,
                                         .tint = tint,
                                         .palette = palette,
                                         .uniform_color = uniform_color};
  return WriteModelPolygon(&polygon, limit);
}

// 4B16C.s: func_800B516C / func_800B5C30 select the pulsed CLUT only
// inside the eight per-limb ranges; any remaining faces keep the base CLUT.
static unsigned ModelPolygonPalette(const Actors_ShpModel* model, int part,
                                    unsigned face, unsigned mask,
                                    unsigned base_palette) {
  unsigned end = 0;
  for (int limb = 0; limb < kLimbPaletteGroups; ++limb) {
    uint16_t count;
    const size_t offset =
        ((size_t)part * kLimbPaletteGroups + limb) * sizeof count;
    memcpy(&count, model->header + offset, sizeof count);
    end += count;
    if (face < end) {
      return base_palette + ((mask & (kFirstLimbPaletteBit >> limb)) != 0
                                 ? kPulsedClutOffset
                                 : 0);
    }
  }
  return base_palette;
}

static void DrawModelPolygons(Actors_ModelRenderContext* context,
                              const void* instance, const void* tint,
                              Actors_ShpModel* model_data, int colored,
                              unsigned pulse_mask,
                              unsigned appearance_material) {
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
  unsigned color = kNeutralTint;
  if (tint != NULL) memcpy(&color, tint, sizeof color);
  const unsigned counts[kPolygonGroupCount] = {header.triangles, header.quads,
                                               header.other};
  if (pulse_mask != 0) {
    for (int part = 0; part < kPolygonGroupCount; ++part) {
      unsigned sum = 0;
      for (int limb = 0; limb < kLimbPaletteGroups; ++limb) {
        uint16_t count;
        const size_t offset =
            ((size_t)part * kLimbPaletteGroups + limb) * sizeof count;
        memcpy(&count, model->header + offset, sizeof count);
        sum += count;
      }
      if (sum > counts[part]) {
        Core_Debugf("render", "Invalid model limb polygon ranges");
        return;
      }
    }
  }
  const unsigned base_palette = Render_ModelWorkspace()->palette;
  for (int part = 0; part < kPolygonGroupCount; ++part) {
    for (unsigned face = 0; face < counts[part]; ++face) {
      const unsigned command = colored ? polygon[11] : polygon[0];
      const int corners = PolygonCorners(command, colored);
      if (corners == 0 || (part != kSpecialPolygonGroup &&
                           corners != part + kTriangleCorners)) {
        Core_Debugf("render", "Invalid model polygon command 0x%X", command);
        return;
      }
      unsigned flags = part == kSpecialPolygonGroup
                           ? polygon[colored ? kGouraudSpecialFlagsOffset
                                             : kFlatSpecialFlagsOffset]
                           : 0;
      if (appearance_material != 0) {
        flags = (flags & ~kAppearanceMaterialMask) | appearance_material;
      }
      const unsigned palette =
          pulse_mask == 0 ? base_palette
                          : ModelPolygonPalette(model, part, face, pulse_mask,
                                                base_palette);
      if (QueueModelPolygon(context, polygon, corners, colored, flags,
                            part == kSpecialPolygonGroup ? kSpecialDepthBias
                                                         : kNormalDepthBias,
                            color, palette, appearance_material != 0,
                            base + kPrimitiveBufferBytes) < 0) {
        return;
      }
      polygon += PolygonBytes(command);
    }
  }
}

static unsigned AppearanceMaterial(unsigned state) {
  if (state == 0) return 0;
  return kSemiTransparencyFlag |
         (state < kAppearanceBlendBoundary ? kAppearanceSubtractiveBlend
                                           : kAppearanceAdditiveBlend);
}

// decomp: BATTLE.PRG func_800B3CF0 (src/BATTLE/BATTLE.PRG/4B16C.s:254).
void Render_DrawModelFlatPolygons(Actors_ModelRenderContext* context,
                                  void* instance, CVECTOR* tint,
                                  void* model_data) {
  DrawModelPolygons(context, instance, tint, model_data, 0, 0, 0);
}

// decomp: func_800B516C (src/BATTLE/BATTLE.PRG/4B16C.s:1646).
void Render_DrawModelFlatPolygonsWithOverrides(
    Actors_ModelRenderContext* context, Actors_WeaponModel* weapon,
    CVECTOR* tint, void* model_data) {
  if (context == NULL || weapon == NULL || tint == NULL || model_data == NULL) {
    Core_Debugf("render", "Weapon polygon overrides require model and context");
    return;
  }
  // Original 3A1A0.h places unk20[2].m[0][2] at byte0x64.
  // Preserve that low-byte quirk through the member, not a native byte offset.
  const unsigned mask =
      (uint16_t)weapon->boneWorldMatrices[kWeaponPaletteMatrix]
          .m[0][kWeaponPaletteColumn] &
      UINT8_MAX;
  DrawModelPolygons(context, weapon, tint, model_data, 0, mask,
                    AppearanceMaterial(weapon->ownerMotionState));
}

// decomp: BATTLE.PRG func_800B4594 (src/BATTLE/BATTLE.PRG/4B16C.s:835).
void Render_DrawModelGouraudPolygons(Actors_ModelRenderContext* context,
                                     void* instance, void* tint,
                                     void* model_data) {
  DrawModelPolygons(context, instance, tint, model_data, 1, 0, 0);
}

// decomp: func_800B5C30 (src/BATTLE/BATTLE.PRG/4B16C.s:2387).
void Render_DrawModelGouraudPolygonsWithOverrides(
    Actors_ModelRenderContext* context, Field_GimmickInstance* object,
    int* tint, void* model_data) {
  if (context == NULL || object == NULL || tint == NULL || model_data == NULL) {
    Core_Debugf("render",
                "Gimmick polygon overrides require model and context");
    return;
  }
  DrawModelPolygons(context, object, tint, model_data, 1,
                    object->palettePulse.kind,
                    AppearanceMaterial(object->motionState));
}

void Render_DrawActorPolygons(Actors_ModelRenderContext* context,
                              Actors_ModelInstance* instance,
                              const CVECTOR* tint, Actors_ShpModel* model) {
  if (instance == NULL) {
    Core_Debugf("render", "Missing actor instance for polygon submission");
    return;
  }
  // 800B3680..800B38E0 selects the override renderer; 800B51DC..800B51FC
  // chooses its original subtractive/additive page mode. 800B5DD4..800B5DE0
  // also repeats the supplied tint at each Gouraud vertex.
  const unsigned state = instance->actionAnimationState;
  const unsigned pulse = instance->palettePulse.kind;
  const int override =
      pulse != 0 ||
      (state >= kAppearanceFadeFirstState && state < kAppearanceFadeEndState &&
       state != kAppearanceOpaqueState) ||
      state == kAppearanceForcedOverride;
  const unsigned material = override ? AppearanceMaterial(state) : 0;
  DrawModelPolygons(context, instance, tint, model, instance->uvLayout, pulse,
                    material);
}
