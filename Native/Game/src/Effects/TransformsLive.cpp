// Hand-maintained (tools/port/owned.csv).
#include "Effects/TransformsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorStateLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Curves.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Transforms.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/Location.h"
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// func_800D0548_arg1 (5BF94.c:6076) views each Effects_TransformSlot.
namespace {
constexpr std::size_t kMotionFlagsAt = 0x06;
constexpr std::size_t kMotionFrameAt = 0x0E;
constexpr std::size_t kMotionPositionAt = 0x1C;
constexpr std::size_t kMotionAttachmentAt = 0x78;
constexpr std::size_t kMotionFacingAt = 0x98;
constexpr std::size_t kMotionSavedAt = 0xB8;
constexpr std::size_t kMotionControlAt = 0xC8;
}  // namespace

static_assert(sizeof(Effects_TransformMotionView) ==
              sizeof(Effects_TransformSlot));
static_assert(offsetof(Effects_TransformMotionView, flags) == kMotionFlagsAt);
static_assert(offsetof(Effects_TransformMotionView, frame) == kMotionFrameAt);
static_assert(offsetof(Effects_TransformMotionView, position) ==
              kMotionPositionAt);
static_assert(offsetof(Effects_TransformMotionView, actor) ==
              offsetof(Effects_TransformSlot, actor));
static_assert(offsetof(Effects_TransformMotionView, world) ==
              offsetof(Effects_TransformSlot, world));
static_assert(offsetof(Effects_TransformMotionView, view) ==
              offsetof(Effects_TransformSlot, view));
static_assert(offsetof(Effects_TransformMotionView, attachment) ==
              kMotionAttachmentAt);
static_assert(offsetof(Effects_TransformMotionView, facing) == kMotionFacingAt);
static_assert(offsetof(Effects_TransformMotionView, saved) == kMotionSavedAt);
static_assert(offsetof(Effects_TransformMotionView, control1) ==
              kMotionControlAt);
static_assert(sizeof(Effects_TransformDefinition) ==
              VS::Effects::kTransformDefinitionBytes);
static_assert(std::extent_v<decltype(Effects_EffectInstance::transforms)> ==
              VS::Effects::kEffectTransformSlots);

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;

template <typename T>
using PerSlot = std::array<T, kEffectTransformSlots>;
using TransformDefinitions = PerSlot<TransformDefinition>;
using TransformStates = PerSlot<TransformState>;

std::uint64_t refused_calls = 0;
std::uint64_t clamped_counts = 0;
std::uint64_t refused_transforms = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] TransformPoint PointOf(const SVECTOR& point) {
  return {.x = point.vx, .y = point.vy, .z = point.vz, .pad = point.pad};
}

void StorePoint(SVECTOR& record, TransformPoint point) {
  record.vx = point.x;
  record.vy = point.y;
  record.vz = point.z;
  record.pad = point.pad;
}

[[nodiscard]] Effects_TransformMotionView& MotionOf(
    Effects_EffectInstance& instance, std::size_t index) {
  return *reinterpret_cast<Effects_TransformMotionView*>(
      &instance.transforms[index]);
}

[[nodiscard]] TransformState StateOf(
    const Effects_TransformMotionView& record) {
  return {
      .rotation = {record.rotation[0], record.rotation[1], record.rotation[2]},
      .flags = record.flags,
      .frame = record.frame,
      .position = Math::VectorOf(record.position),
      .actor = record.actor,
      .bone = record.bone,
      .target_actor = record.targetActor,
      .target_bone = record.targetBone,
      .world = Math::TransformOf(record.world),
      .view = Math::TransformOf(record.view),
      .attachment = Math::TransformOf(record.attachment),
      .facing = Math::TransformOf(record.facing),
      .saved = PointOf(record.saved),
      .control0 = PointOf(record.control0),
      .control1 = PointOf(record.control1)};
}

void StoreState(Effects_TransformMotionView& record,
                const TransformState& state) {
  std::ranges::copy(state.rotation, std::begin(record.rotation));
  record.flags = state.flags;
  record.frame = state.frame;
  Math::StoreVector(record.position, state.position);
  record.actor = state.actor;
  record.bone = state.bone;
  record.targetActor = state.target_actor;
  record.targetBone = state.target_bone;
  Math::StoreTransform(record.world, state.world);
  Math::StoreTransform(record.view, state.view);
  Math::StoreTransform(record.attachment, state.attachment);
  Math::StoreTransform(record.facing, state.facing);
  StorePoint(record.saved, state.saved);
  StorePoint(record.control0, state.control0);
  StorePoint(record.control1, state.control1);
}

[[nodiscard]] TransformEndpoint EndpointOf(
    const Effects_ActionEffectTarget& target) {
  return {.type = target.type,
          .actor = static_cast<std::uint8_t>(target.value.actor.actor),
          .part = static_cast<std::uint8_t>(target.value.actor.part),
          .position = PointOf(target.value.position)};
}

class LiveTransformHostImpl final : public TransformHost {
 public:
  void ActorAnchorMatrix(std::uint8_t actor, std::uint8_t bone,
                         Math::Transform3& attachment) override {
    MATRIX matrix{};
    Math::StoreTransform(matrix, attachment);
    int length = 0;
    static_cast<void>(
        Actors::LiveActorAnchorMatrix(actor, bone, &matrix, &length));
    attachment = Math::TransformOf(matrix);
  }

  std::optional<std::int16_t> ActorFacing(std::uint8_t actor) override {
    const auto placement = Actors::LiveActorPlacementView(actor);
    if (!placement) return std::nullopt;
    return placement->position.w;
  }

  void ActorBonePosition(std::uint8_t actor, std::uint8_t bone,
                         TransformPoint& position) override {
    SVECTOR point{};
    StorePoint(point, position);
    static_cast<void>(Actors::LiveActorBonePosition(actor, bone, &point, 0));
    position = PointOf(point);
  }

  TransformCameraView FrameCamera() override {
    return {.position = Math::VectorOf(Render_frameScratch.camera.position),
            .look_at = Math::VectorOf(Render_frameScratch.camera.lookAt)};
  }

  Math::Transform3 ViewMatrix() override {
    return Math::TransformOf(Render_frameScratch.viewMatrix);
  }

  TransformPoint EffectOrigin() override {
    return PointOf(*Effects_effectOrigin);
  }

  std::span<const std::int32_t> CurveSamples() override {
    return LiveEffectCurveSamples();
  }

  CurveSample SampleCurve(std::int32_t curve_id,
                          std::int32_t distance) override {
    return SampleLiveEffectCurve(curve_id, distance);
  }

  std::int32_t Random() override { return Core_Random_Rand(); }

  Math::Vec3 RotateTranslate(const Math::Transform3& transform,
                             Math::Vec3 point) override {
    MATRIX matrix{};
    Math::StoreTransform(matrix, transform);
    SVECTOR input = {static_cast<std::int16_t>(point.x),
                     static_cast<std::int16_t>(point.y),
                     static_cast<std::int16_t>(point.z), 0};
    VECTOR output{};
    Core_GteWord flags = 0;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
    Core_Gte_RotTrans(&input, &output, &flags);
    return Math::VectorOf(output);
  }
};

void ReportRefusal(const char* entry, const char* reason) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN("effects", "effect transforms: {} refused: {} ({} refusals)",
                entry, reason, refused_calls);
  }
}

// The instance's first `count` transform records.
[[nodiscard]] std::span<TransformState> LoadStates(
    Effects_EffectInstance& instance, TransformStates& states,
    std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    states.at(i) = StateOf(MotionOf(instance, i));
  }
  return std::span<TransformState>(states).first(count);
}

void StoreStates(Effects_EffectInstance& instance,
                 std::span<const TransformState> states) {
  for (std::size_t i = 0; i < states.size(); ++i) {
    StoreState(MotionOf(instance, i), states[i]);
  }
}
}  // namespace

std::size_t LiveTransformDefinitions(
    std::span<TransformDefinition, kEffectTransformSlots> out) {
  const std::optional<PackageParameters> parameters = ReadPackageParameters(
      LiveBoundPackageBlock(PackageBlockType::kParameters));
  if (!parameters) return 0;
  const std::size_t counted = parameters->transform_count;
  const std::size_t decoded = ReadTransformDefinitions(
      LiveBoundPackageBlock(PackageBlockType::kTransformDefinitions),
      out.first(std::min(counted, out.size())));
  if (decoded < counted && ShouldReport(clamped_counts)) {
    VS_LOG_WARN("effects",
                "effect transforms: block 9 counts {} transforms but {} are "
                "bound ({} clamps)",
                counted, decoded, clamped_counts);
  }
  return decoded;
}

TransformHost& LiveTransformHost() {
  static LiveTransformHostImpl host;
  return host;
}

// decomp: BATTLE.PRG func_800CFEF0 (src/BATTLE/BATTLE.PRG/5BF94.c:5940)
void LiveInitializeTransformStates(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("initialize", "no effect instance");
    return;
  }
  TransformDefinitions definitions{};
  const std::size_t count = LiveTransformDefinitions(definitions);
  TransformStates states{};
  const std::span<TransformState> loaded = LoadStates(*instance, states, count);
  const TransformEndpoints endpoints{
      .emit_from = EndpointOf(instance->context.emitFrom),
      .emit_to = EndpointOf(instance->context.emitTo)};
  static_cast<void>(InitializeTransformStates(
      std::span<const TransformDefinition>(definitions).first(count), endpoints,
      loaded));
  StoreStates(*instance, loaded);
}

// decomp: BATTLE.PRG func_800D0D08 (src/BATTLE/BATTLE.PRG/5BF94.c:6227)
void LiveUpdateEffectTransforms(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("update", "no effect instance");
    return;
  }
  TransformDefinitions definitions{};
  const std::size_t count = LiveTransformDefinitions(definitions);
  TransformStates states{};
  const std::span<TransformState> loaded = LoadStates(*instance, states, count);
  const TransformPass pass = UpdateEffectTransforms(
      std::span<const TransformDefinition>(definitions).first(count), loaded,
      LiveTransformHost());
  StoreStates(*instance, loaded);
  if (pass.refused != 0 && ShouldReport(refused_transforms)) {
    VS_LOG_WARN("effects",
                "effect transforms: {} transforms skipped a frame for a "
                "curve past the samples ({} reports)",
                pass.refused, refused_transforms);
  }
}
}  // namespace VS::Effects
