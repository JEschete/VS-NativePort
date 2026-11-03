// Hand-maintained (tools/port/owned.csv).
// Live binding for the SHP model packets (ModelGeometry.cpp).
#include "Render/ModelGeometryLive.hpp"

#include <algorithm>
#include <bit>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>

#include "Core/Math/ScalarMath.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRenderingLiveState.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/WeaponModel.h"
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

extern unsigned int
    Content_battleShadowVertices[VS::Render::kShadowVertexCount];
extern unsigned int Content_battleShadowFaces[VS::Render::kShadowFaceCount];
}

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr const char* kChannel = "render";
// The frame primitive buffers are low-end allocations of this size
// (Field/RoomLoading.cpp, Ending/Credits/StaffRoll.cpp).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
constexpr int kShadowSupportMode = 3;
constexpr std::size_t kWeaponPaletteMatrix = 2;
constexpr std::size_t kWeaponPaletteColumn = 2;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(sizeof(ModelCounts) == sizeof(std::uint64_t) &&
              std::is_trivially_copyable_v<ModelCounts>);
static_assert(std::is_same_v<
              decltype(Actors_ModelRenderContext::projectedVertices[0]), int&>);

struct ModelExtent {
  std::uintptr_t begin = 0;
  std::size_t bytes = 0;
};

std::vector<ModelExtent>& Extents() {
  static std::vector<ModelExtent> extents;
  return extents;
}

std::uint64_t unrecorded_models = 0;

// The instance's eight-byte bone, group and polygon count prefix.
template <typename Instance>
[[nodiscard]] ModelCounts CountsOf(const Instance& instance) {
  static_assert(sizeof(Instance) >= sizeof(ModelCounts));
  ModelCounts counts;
  std::memcpy(&counts, &instance, sizeof counts);
  return counts;
}

[[nodiscard]] Math::Vec3 TranslationOf(const MATRIX& matrix) {
  return {static_cast<std::int32_t>(matrix.t[0]),
          static_cast<std::int32_t>(matrix.t[1]),
          static_cast<std::int32_t>(matrix.t[2])};
}

[[nodiscard]] MATRIX MatrixFrom(const ModelTransform& transform) {
  MATRIX matrix{};
  Math::StoreMatrix(matrix, transform.rotation);
  matrix.t[0] = transform.translation.x;
  matrix.t[1] = transform.translation.y;
  matrix.t[2] = transform.translation.z;
  return matrix;
}

class LiveModelTransformer final : public ModelTransformer {
 public:
  explicit LiveModelTransformer(std::span<const MATRIX> matrices = {})
      : matrices_(matrices) {}

  void LoadBone(std::size_t bone) override {
    const MATRIX& matrix = matrices_[bone];
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  void LoadTransform(const ModelTransform& transform) override {
    const MATRIX matrix = MatrixFrom(transform);
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  Math::Vec3 Transform(const ModelVertex& vertex) override {
    SVECTOR input = {vertex.x, vertex.y, vertex.z, 0};
    VECTOR output{};
    Core_GteWord flags = 0;
    Core_Gte_RotTrans(&input, &output, &flags);
    return Math::VectorOf(output);
  }
  ProjectedPoint Project(const ModelVertex& vertex) override {
    SVECTOR input = {vertex.x, vertex.y, vertex.z, 0};
    Core_GteWord screen = 0;
    Core_GteWord fog = 0;
    Core_GteWord flags = 0;
    VECTOR transformed{};
    Core_Gte_RotTransPers(&input, &screen, &fog, &flags);
    Core_Gte_StoreLvnl(&transformed);
    return {static_cast<std::int32_t>(screen),
            static_cast<std::int32_t>(transformed.vz)};
  }

 private:
  std::span<const MATRIX> matrices_;
};

// The end of the current frame's primitive buffer, or nothing when the
// cursor is not inside it.
[[nodiscard]] std::uintptr_t PrimitiveLimit() {
  const auto base = reinterpret_cast<std::uintptr_t>(
      Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
  const auto cursor =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
  return base != 0 && cursor >= base && cursor - base <= kPrimitiveBufferBytes
             ? base + kPrimitiveBufferBytes
             : 0;
}

class LivePacketQueue final : public ModelPacketQueue {
 public:
  explicit LivePacketQueue(std::uintptr_t limit) : limit_(limit) {}

  bool Emit(std::span<const std::uint32_t> packet,
            std::uint32_t depth) override {
    auto* ordering = static_cast<u_long*>(Render_frameScratch.orderingTable);
    if (ordering == nullptr) {
      return true;
    }
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    if (packet.empty() || cursor > limit_ ||
        packet.size_bytes() > limit_ - cursor) {
      return false;
    }
    auto* slot = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
    std::ranges::copy(packet, slot);
    setlen(slot, packet.size() - 1);
    AddPrim(ordering + depth, slot);
    Render_frameScratch.primitiveCursor = slot + packet.size();
    return true;
  }

 private:
  std::uintptr_t limit_;
};

class LiveShadowGround final : public ShadowGround {
 public:
  explicit LiveShadowGround(SVECTOR& position) : position_(position) {}

  std::int32_t SupportHeight(std::int32_t dx, std::int32_t dz) override {
    return Field_SampleActorSupportHeight(&position_, dx, dz,
                                          kShadowSupportMode);
  }
  [[nodiscard]] bool OnPlatform() const override {
    return static_cast<std::int8_t>(Actors_supportingPlatformLayer) > 0;
  }

 private:
  SVECTOR& position_;
};

[[nodiscard]] ModelSurface WorkspaceSurface() {
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  return {workspace->palette, workspace->texturePage};
}

[[nodiscard]] std::uint32_t TintOf(const CVECTOR& tint) {
  std::uint32_t color = 0;
  std::memcpy(&color, &tint, sizeof color);
  return color;
}

[[nodiscard]] ModelProjectionView ProjectedVertices() {
  const Actors_ModelRenderContext& context = Actors_modelRenderContext;
  const std::size_t count = std::min<std::size_t>(
      context.projectedVertexCount, std::size(context.projectedVertices));
  return {std::span<const std::int32_t>(context.projectedVertices).first(count),
          std::span<const std::int32_t>(context.vertexDepths).first(count)};
}

[[nodiscard]] std::span<const std::byte> ActorModelBytes(
    const Actors_ActorRecord& geometry) {
  if (geometry.instance.model == nullptr) {
    return {};
  }
  return {reinterpret_cast<const std::byte*>(geometry.instance.model),
          geometry.modelDataSize};
}

// The recorded bytes of a gimmick or weapon model; logs (at powers of two)
// a model no loader recorded.
[[nodiscard]] std::span<const std::byte> RecordedModelBytes(const void* model) {
  const std::span<const std::byte> bytes = LiveModelBytes(model);
  if (bytes.empty() && model != nullptr &&
      std::has_single_bit(++unrecorded_models)) {
    Core_Errorf(kChannel,
                "model %p has no recorded extent (%" PRIu64 " refused)", model,
                unrecorded_models);
  }
  return bytes;
}

bool ProjectLive(const ModelCounts& counts, std::span<const std::byte> model,
                 std::span<const MATRIX> matrices) {
  Actors_ModelRenderContext& context = Actors_modelRenderContext;
  context.projectedVertexCount = 0;
  LiveModelTransformer transformer(matrices);
  const ModelProjectionResult result =
      ProjectModelVertices(counts, model, matrices.size(), transformer,
                           std::span<std::int32_t>(context.projectedVertices),
                           std::span<std::int32_t>(context.vertexDepths));
  context.projectedVertexCount = result.vertex_count;
  return result.combined_depth != 0;
}

void ReportDraw(ModelDrawStatus status) {
  switch (status) {
    case ModelDrawStatus::kInvalidLimbRanges:
      Core_Debugf(kChannel, "Invalid model limb polygon ranges");
      return;
    case ModelDrawStatus::kInvalidPolygon:
      Core_Debugf(kChannel, "Invalid model polygon command");
      return;
    case ModelDrawStatus::kTruncatedModel:
      Core_Debugf(kChannel, "Model polygons run past the model data");
      return;
    case ModelDrawStatus::kDrawn:
    case ModelDrawStatus::kNoVertices:
    case ModelDrawStatus::kInvalidVertexIndex:
    case ModelDrawStatus::kBufferFull:
      return;
  }
}

enum class PolygonKind : std::uint8_t {
  kFlat,
  kGouraud,
  kFlatOverrides,
  kGouraudOverrides
};

void DrawLive(PolygonKind kind, const ModelCounts& counts,
              std::span<const std::byte> model, std::uint32_t tint,
              const ModelOverrides& overrides) {
  const std::uintptr_t limit = PrimitiveLimit();
  if (model.empty() || limit == 0) {
    return;
  }
  LivePacketQueue queue(limit);
  const ModelPolygonSource source{counts, model, ProjectedVertices()};
  const ModelSurface surface = WorkspaceSurface();
  switch (kind) {
    case PolygonKind::kFlat:
      ReportDraw(DrawModelFlatPolygons(source, tint, surface, queue));
      return;
    case PolygonKind::kGouraud:
      ReportDraw(DrawModelGouraudPolygons(source, tint, surface, queue));
      return;
    case PolygonKind::kFlatOverrides:
      ReportDraw(DrawModelFlatPolygonsWithOverrides(source, tint, surface,
                                                    overrides, queue));
      return;
    case PolygonKind::kGouraudOverrides:
      ReportDraw(DrawModelGouraudPolygonsWithOverrides(source, tint, surface,
                                                       overrides, queue));
      return;
  }
}

[[nodiscard]] ModelOverrides WeaponOverrides(const Actors_WeaponModel& weapon) {
  // Original 3A1A0.h places unk20[2].m[0][2] at byte 0x64; the low byte of
  // that matrix element is the pulse mask.
  return {static_cast<std::uint32_t>(
              static_cast<std::uint16_t>(
                  weapon.boneWorldMatrices[kWeaponPaletteMatrix]
                      .m[0][kWeaponPaletteColumn]) &
              UINT8_MAX),
          weapon.ownerMotionState};
}

[[nodiscard]] ModelOverrides GimmickOverrides(
    const Field_GimmickInstance& object) {
  return {object.palettePulse.kind, object.motionState};
}

[[nodiscard]] std::uint32_t GimmickTint(const Field_GimmickInstance& object) {
  return static_cast<std::uint32_t>(object.baseColor);
}
}  // namespace

void RecordLiveModelExtent(const void* model, std::size_t bytes) {
  if (model == nullptr) {
    return;
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(model);
  const std::uintptr_t end = begin + bytes;
  std::erase_if(Extents(), [&](const ModelExtent& known) {
    return known.begin == begin ||
           (known.begin < end && begin < known.begin + known.bytes);
  });
  Extents().push_back({begin, bytes});
}

void RecordLiveModelCopy(const void* destination, const void* source,
                         std::size_t capacity) {
  const std::span<const std::byte> copied = LiveModelBytes(source);
  if (copied.empty()) {
    ForgetLiveModelExtent(destination);
    Core_Errorf(kChannel,
                "model copy %p came from %p, which has no recorded extent",
                destination, source);
    return;
  }
  RecordLiveModelExtent(destination, std::min(copied.size(), capacity));
}

void ForgetLiveModelExtent(const void* model) {
  const auto begin = reinterpret_cast<std::uintptr_t>(model);
  std::erase_if(Extents(),
                [&](const ModelExtent& known) { return known.begin == begin; });
}

std::span<const std::byte> LiveModelBytes(const void* model) {
  const auto begin = reinterpret_cast<std::uintptr_t>(model);
  const auto found = std::ranges::find(Extents(), begin, &ModelExtent::begin);
  if (model == nullptr || found == Extents().end()) {
    return {};
  }
  return {static_cast<const std::byte*>(model), found->bytes};
}

bool ValidateLiveActorModel(const Actors_ActorRecord& geometry) {
  return ValidateModelData(CountsOf(geometry.instance),
                           geometry.instance.uvLayout != 0,
                           ActorModelBytes(geometry));
}

bool ProjectLiveActorModel(const Actors_ActorRecord& geometry,
                           std::span<const MATRIX> matrices) {
  return ProjectLive(CountsOf(geometry.instance), ActorModelBytes(geometry),
                     matrices);
}

void DrawLiveActorPolygons(const Actors_ActorRecord& geometry,
                           const CVECTOR& tint) {
  const std::uintptr_t limit = PrimitiveLimit();
  const std::span<const std::byte> model = ActorModelBytes(geometry);
  if (model.empty() || limit == 0) {
    return;
  }
  LivePacketQueue queue(limit);
  const Actors_ModelInstance& instance = geometry.instance;
  ReportDraw(DrawActorModelPolygons(
      {CountsOf(instance), model, ProjectedVertices()}, instance.uvLayout != 0,
      TintOf(tint), WorkspaceSurface(),
      {instance.palettePulse.kind, instance.actionAnimationState}, queue));
}

void DrawLiveActorGouraudPolygons(const Actors_ActorRecord& geometry,
                                  const CVECTOR& tint) {
  DrawLive(PolygonKind::kGouraud, CountsOf(geometry.instance),
           ActorModelBytes(geometry), TintOf(tint), {});
}

void DrawLiveActorFlatPolygons(const Actors_ActorRecord& geometry,
                               const CVECTOR& tint) {
  DrawLive(PolygonKind::kFlat, CountsOf(geometry.instance),
           ActorModelBytes(geometry), TintOf(tint), {});
}

void DrawLiveActorGroundShadow(Actors_ActorRecord& actor, const MATRIX& camera,
                               const SVECTOR& root_offset) {
  const std::uintptr_t limit = PrimitiveLimit();
  if (limit == 0 || Render_frameScratch.orderingTable == nullptr) {
    Core_Debugf(kChannel,
                "Actor shadow requires actor, view and packet buffers");
    std::abort();
  }
  const Actors_ModelInstance& instance = actor.instance;
  const ActorShadowSource source{
      .height = instance.position.vy,
      .radius = actor.shadowRadius,
      .rotation_x = instance.rotationX,
      .facing = instance.facing,
      .rotation_z = instance.rotationZ,
      .scale_x = instance.scaleX,
      .scale_y = instance.scaleY,
      .scale_z = instance.scaleZ,
      .blend_level = actor.shadowBlend.level,
      .radius_shrink_rate = actor.shadowBlend.radiusShrinkRate,
      .radius_expand_rate = actor.shadowBlend.radiusExpandRate,
      .blend_color = static_cast<std::uint32_t>(actor.shadowBlend.reserved0),
      .fade_red = static_cast<std::int8_t>(actor.fadeRed)};
  LiveShadowGround ground(actor.instance.position);
  LiveModelTransformer transformer;
  LivePacketQueue queue(limit);
  const ActorShadowResult result = DrawActorGroundShadow(
      source, {Math::MatrixOf(camera), TranslationOf(camera)},
      {root_offset.vx, root_offset.vy, root_offset.vz},
      {Content_battleShadowVertices, Content_battleShadowFaces}, ground,
      transformer, queue);
  if (result.status == ActorShadowStatus::kInvalidMesh) {
    Core_Debugf(kChannel, "Invalid actor shadow vertex offset");
    std::abort();
  }
  actor.soundPosition = {result.anchor[0], result.anchor[1], result.anchor[2],
                         0};
  if (result.status == ActorShadowStatus::kBufferFull) {
    Core_Debugf(kChannel, "Actor shadow primitive buffer exhausted");
  }
}

bool ProjectLiveGimmickModel(const Field_GimmickInstance& object) {
  return ProjectLive(CountsOf(object), RecordedModelBytes(object.model),
                     object.matrices);
}

void DrawLiveGimmickGouraudPolygons(const Field_GimmickInstance& object) {
  DrawLive(PolygonKind::kGouraud, CountsOf(object),
           LiveModelBytes(object.model), GimmickTint(object), {});
}

void DrawLiveGimmickGouraudPolygonsWithOverrides(
    const Field_GimmickInstance& object) {
  DrawLive(PolygonKind::kGouraudOverrides, CountsOf(object),
           LiveModelBytes(object.model), GimmickTint(object),
           GimmickOverrides(object));
}

bool ProjectLiveWeaponModel(const Actors_WeaponModel& weapon) {
  return ProjectLive(CountsOf(weapon),
                     RecordedModelBytes(weapon.sectionOffsets),
                     weapon.boneWorldMatrices);
}

void DrawLiveWeaponFlatPolygons(const Actors_WeaponModel& weapon,
                                const CVECTOR& tint) {
  DrawLive(PolygonKind::kFlat, CountsOf(weapon),
           LiveModelBytes(weapon.sectionOffsets), TintOf(tint), {});
}

void DrawLiveWeaponFlatPolygonsWithOverrides(const Actors_WeaponModel& weapon,
                                             const CVECTOR& tint) {
  DrawLive(PolygonKind::kFlatOverrides, CountsOf(weapon),
           LiveModelBytes(weapon.sectionOffsets), TintOf(tint),
           WeaponOverrides(weapon));
}
}  // namespace VS::Render
