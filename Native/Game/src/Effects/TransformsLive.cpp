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
#include "Actors/Location.h"
#include "Actors/SkeletonLive.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/Curves.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/Transforms.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

static_assert(sizeof(VS::Effects::LiveTransformDefinition) ==
              VS::Effects::kTransformDefinitionBytes);
static_assert(
    std::tuple_size_v<decltype(VS::Effects::LiveEffectInstance::transforms)> ==
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

[[nodiscard]] LiveTransformSlot& MotionOf(LiveEffectInstance& instance,
                                          std::size_t index) {
  return std::span(instance.transforms)[index];
}

[[nodiscard]] TransformState StateOf(const LiveTransformSlot& record) {
  return {
      .rotation = {record.rotation[0], record.rotation[1], record.rotation[2]},
      .flags = record.flags,
      .frame = record.frame,
      .position = Math::VectorOf(record.position),
      .actor = record.actor,
      .bone = record.bone,
      .target_actor = record.target_actor,
      .target_bone = record.target_bone,
      .world = Math::TransformOf(record.world),
      .view = Math::TransformOf(record.view),
      .attachment = Math::TransformOf(record.attachment),
      .facing = Math::TransformOf(record.facing),
      .saved = PointOf(record.saved),
      .control0 = PointOf(record.control0),
      .control1 = PointOf(record.control1)};
}

void StoreState(LiveTransformSlot& record, const TransformState& state) {
  std::ranges::copy(state.rotation, record.rotation.begin());
  record.flags = state.flags;
  record.frame = state.frame;
  Math::StoreVector(record.position, state.position);
  record.actor = state.actor;
  record.bone = state.bone;
  record.target_actor = state.target_actor;
  record.target_bone = state.target_bone;
  Math::StoreTransform(record.world, state.world);
  Math::StoreTransform(record.view, state.view);
  Math::StoreTransform(record.attachment, state.attachment);
  Math::StoreTransform(record.facing, state.facing);
  StorePoint(record.saved, state.saved);
  StorePoint(record.control0, state.control0);
  StorePoint(record.control1, state.control1);
}

[[nodiscard]] TransformEndpoint EndpointOf(
    const LiveActionEffectTarget& target) {
  return {.type = target.type,
          .actor = static_cast<std::uint8_t>(target.Actor().actor),
          .part = static_cast<std::uint8_t>(target.Actor().part),
          .position = PointOf(target.position)};
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
    return {.position =
                Math::VectorOf(VS::Render::FrameScratchpad().camera.position),
            .look_at =
                Math::VectorOf(VS::Render::FrameScratchpad().camera.look_at)};
  }

  Math::Transform3 ViewMatrix() override {
    return Math::TransformOf(VS::Render::FrameScratchpad().view_matrix);
  }

  TransformPoint EffectOrigin() override {
    return PointOf(actionRequest.position);
  }

  std::span<const std::int32_t> CurveSamples() override {
    return LiveEffectCurveSamples();
  }

  CurveSample SampleCurve(std::int32_t curve_id,
                          std::int32_t distance) override {
    return SampleLiveEffectCurve(curve_id, distance);
  }

  std::int32_t Random() override { return VS::Core::NextBiosRand(); }

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
    VS::Core::LogWarn("effects",
                      "effect transforms: {} refused: {} ({} refusals)", entry,
                      reason, refused_calls);
  }
}

// The instance's first `count` transform records.
[[nodiscard]] std::span<TransformState> LoadStates(LiveEffectInstance& instance,
                                                   TransformStates& states,
                                                   std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    states.at(i) = StateOf(MotionOf(instance, i));
  }
  return std::span<TransformState>(states).first(count);
}

void StoreStates(LiveEffectInstance& instance,
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
    VS::Core::LogWarn(
        "effects",
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
void LiveInitializeTransformStates(LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("initialize", "no effect instance");
    return;
  }
  TransformDefinitions definitions{};
  const std::size_t count = LiveTransformDefinitions(definitions);
  TransformStates states{};
  const std::span<TransformState> loaded = LoadStates(*instance, states, count);
  const TransformEndpoints endpoints{
      .emit_from = EndpointOf(instance->context.emit_from),
      .emit_to = EndpointOf(instance->context.emit_to)};
  static_cast<void>(InitializeTransformStates(
      std::span<const TransformDefinition>(definitions).first(count), endpoints,
      loaded));
  StoreStates(*instance, loaded);
}

// decomp: BATTLE.PRG func_800D0D08 (src/BATTLE/BATTLE.PRG/5BF94.c:6227)
void LiveUpdateEffectTransforms(LiveEffectInstance* instance) {
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
    VS::Core::LogWarn("effects",
                      "effect transforms: {} transforms skipped a frame for a "
                      "curve past the samples ({} reports)",
                      pass.refused, refused_transforms);
  }
}
}  // namespace VS::Effects
