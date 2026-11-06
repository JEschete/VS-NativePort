#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Camera.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/Curves.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Transforms.hpp"
#include "Effects/TransformsLive.hpp"

extern "C" {
#include "Effects/Package.h"
#include "Effects/Runtime.h"
}

namespace {
namespace Effects = VS::Effects;
namespace Math = VS::Core::Math;

// {u32 size | type << 16, payload} blocks, then a zero terminator.
struct Block {
  std::uint32_t type{};
  std::vector<std::uint8_t> payload;
};

std::vector<std::byte> BuildPackage(std::initializer_list<Block> blocks,
                                    bool terminate = true) {
  std::vector<std::byte> package;
  const auto put_word = [&package](std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
      package.push_back(static_cast<std::byte>((word >> (i * 8)) & 0xFFU));
    }
  };
  for (const Block& block : blocks) {
    put_word(static_cast<std::uint32_t>(block.payload.size() + 4) |
             (block.type << 16));
    for (const std::uint8_t value : block.payload) {
      package.push_back(static_cast<std::byte>(value));
    }
  }
  if (terminate) put_word(0);
  return package;
}

std::vector<std::uint8_t> Bytes(std::span<const std::byte> view) {
  std::vector<std::uint8_t> bytes;
  for (const std::byte value : view) {
    bytes.push_back(std::to_integer<std::uint8_t>(value));
  }
  return bytes;
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

void PutHalfword(std::span<std::byte> bytes, std::size_t at,
                 std::uint16_t value) {
  bytes[at] = static_cast<std::byte>(value & 0xFFU);
  bytes[at + 1] = static_cast<std::byte>(value >> 8);
}

// ---------------------------------------------------------------- curves --

// Curve 1 is {10, 20, 30}; 2 is unbound, 3 is empty and 4 is {10}.
struct CurveFixture {
  std::array<std::uint8_t, 3> first{10, 20, 30};
  std::array<std::span<const std::uint8_t>, 4> curves{
      std::span<const std::uint8_t>(first), std::span<const std::uint8_t>{},
      std::span<const std::uint8_t>(first).first(0),
      std::span<const std::uint8_t>(first).first(1)};

  [[nodiscard]] Effects::EffectCurveBlock Block() const {
    return {.header_count = 5, .curves = curves};
  }
};

// One segmented curve: {step 4, value 10}, {step 0, value 30}.
using SegmentedBytes = std::array<std::uint8_t, 11>;
constexpr SegmentedBytes kSegmentedTable{1, 0, 0, 0, 6, 0, 1, 4, 10, 0, 30};

// ---------------------------------------------------------------- camera --

class FakeCameraHost final : public Effects::EffectCameraHost {
 public:
  Effects::FieldCameraSnapshot field{};
  std::vector<Effects::AppliedEffectCamera> applied;
  Effects::CurveSample curve{};
  Effects::CurveSample segmented{};
  std::int32_t random = 0;
  int random_calls = 0;

  Effects::FieldCameraSnapshot CurrentCamera() override { return field; }
  void ApplyCamera(const Effects::AppliedEffectCamera& camera) override {
    applied.push_back(camera);
  }
  Effects::CurveSample SampleCurve(std::int32_t /*curve_id*/,
                                   std::int32_t /*distance*/) override {
    return curve;
  }
  Effects::CurveSample SampleSegmentedCurve(
      std::int32_t /*curve_index*/, std::int32_t /*distance*/) override {
    return segmented;
  }
  std::int32_t Random() override {
    ++random_calls;
    return random;
  }
};

// ------------------------------------------------------------ transforms --

constexpr std::uint16_t kAnimatedLocal = 0x1;
constexpr std::uint16_t kAttached = 0x2;
constexpr std::uint16_t kKeepEndpointBone = 0x4;
constexpr std::uint16_t kAttachToReceiver = 0x10;
constexpr std::uint16_t kTargeted = 0x80;
constexpr std::uint16_t kTargetEmitter = 0x400;
constexpr std::uint16_t kLinearMotion = 0x1000;
constexpr std::uint16_t kFrameAdvances = 0x4;
constexpr std::uint16_t kFromSavedTarget = 0x80;

class FakeTransformHost final : public Effects::TransformHost {
 public:
  std::vector<std::int32_t> samples = std::vector<std::int32_t>(8, 0);
  std::optional<std::int16_t> facing = std::int16_t{0x400};
  Math::Transform3 anchor{.rotation = Math::IdentityRotation(),
                          .translation = {.x = 1, .y = 2, .z = 3}};
  Effects::TransformPoint bone{.x = 7, .y = 8, .z = 9, .pad = 0};
  Effects::TransformPoint origin{.x = 100, .y = 200, .z = 300, .pad = 0};
  Effects::TransformCameraView camera{};
  Effects::CurveSample curve{};
  std::vector<std::int32_t> curve_ids;
  int anchor_calls = 0;

  void ActorAnchorMatrix(std::uint8_t /*actor*/, std::uint8_t /*bone*/,
                         Math::Transform3& attachment) override {
    ++anchor_calls;
    attachment = anchor;
  }
  std::optional<std::int16_t> ActorFacing(std::uint8_t /*actor*/) override {
    return facing;
  }
  void ActorBonePosition(std::uint8_t /*actor*/, std::uint8_t /*bone*/,
                         Effects::TransformPoint& position) override {
    position.x = bone.x;
    position.y = bone.y;
    position.z = bone.z;
  }
  Effects::TransformCameraView FrameCamera() override { return camera; }
  Math::Transform3 ViewMatrix() override {
    return {.rotation = Math::IdentityRotation(), .translation = {}};
  }
  Effects::TransformPoint EffectOrigin() override { return origin; }
  std::span<const std::int32_t> CurveSamples() override { return samples; }
  Effects::CurveSample SampleCurve(std::int32_t curve_id,
                                   std::int32_t /*distance*/) override {
    curve_ids.push_back(curve_id);
    return curve;
  }
  std::int32_t Random() override { return 0; }
  Math::Vec3 RotateTranslate(const Math::Transform3& transform,
                             Math::Vec3 point) override {
    return {.x = transform.translation.x + point.x,
            .y = transform.translation.y + point.y,
            .z = transform.translation.z + point.z};
  }
};

// ------------------------------------------------------------- resources --

// The ids name only kNone; the fakes hand out opaque values for the rest.
constexpr Effects::EffectBufferId BufferId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Effects::EffectBufferId>(id);
}

constexpr Effects::EffectPackageId PackageId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Effects::EffectPackageId>(id);
}

constexpr Effects::DiscReadId ReadId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Effects::DiscReadId>(id);
}

class FakeResourceHost final : public Effects::EffectResourceHost {
 public:
  Effects::EffectDiscLayout layout{.package_base_lba = 1000,
                                   .purge_overlay = {.lba = 50, .size = 0x800},
                                   .texture_staging_bytes = 0x8000};
  std::vector<Effects::DiscRead> requested;
  std::vector<std::pair<Effects::DiscReadId, Effects::EffectBufferId>> enqueued;
  bool queue_full = false;
  int refused_completions = 0;
  bool loaded = false;
  std::vector<Effects::DiscReadId> released;
  std::vector<std::size_t> allocations;
  std::vector<Effects::EffectBufferId> freed;
  int overlay_acquired = 0;
  Effects::EffectBuffer overlay{.id = BufferId(0x500), .capacity = 0x10000};
  std::vector<std::uint32_t> reserved;
  std::vector<std::uint32_t> retired;
  std::vector<std::pair<Effects::VramRect, Effects::EffectBufferId>> uploads;
  std::uintptr_t next_id = 0x100;

  Effects::EffectDiscLayout DiscLayout() override { return layout; }
  Effects::DiscReadId AllocateRead(Effects::DiscRead read) override {
    requested.push_back(read);
    if (queue_full) return Effects::DiscReadId::kNone;
    return ReadId(next_id++);
  }
  void EnqueueRead(Effects::DiscReadId read,
                   Effects::EffectBufferId destination) override {
    enqueued.emplace_back(read, destination);
  }
  Effects::DiscReadId CompleteRefusedRead(
      Effects::DiscRead /*read*/,
      Effects::EffectBufferId /*destination*/) override {
    ++refused_completions;
    return ReadId(0x900);
  }
  bool ReadLoaded(Effects::DiscReadId /*read*/) override { return loaded; }
  void ReleaseRead(Effects::DiscReadId read) override {
    released.push_back(read);
  }
  Effects::EffectBuffer AllocateBuffer(std::size_t bytes) override {
    allocations.push_back(bytes);
    return {.id = BufferId(next_id++), .capacity = bytes};
  }
  void FreeBuffer(Effects::EffectBufferId buffer) override {
    freed.push_back(buffer);
  }
  void AcquirePurgeOverlayWorkspace() override { ++overlay_acquired; }
  Effects::EffectBuffer PurgeOverlayBuffer() override { return overlay; }
  void ReserveTexturePage(std::uint32_t page) override {
    reserved.push_back(page);
  }
  void RetireTexturePage(std::uint32_t page) override {
    retired.push_back(page);
  }
  void UploadImage(const Effects::VramRect& area,
                   Effects::EffectBufferId pixels) override {
    uploads.emplace_back(area, pixels);
  }
};

class FakeLifecycleHost final : public Effects::EffectLifecycleHost {
 public:
  int resets = 0;
  std::vector<Effects::EffectPackageId> empty_bindings;
  std::vector<std::int32_t> pools;
  int cleared = 0;

  void ResetPackageRequests() override { ++resets; }
  void BindEmptyPackage(Effects::EffectPackageId package) override {
    empty_bindings.push_back(package);
  }
  void AcquireParticlePool(std::int32_t count) override {
    pools.push_back(count);
  }
  void ClearReloadedBlocks() override { ++cleared; }
};

class FakePackageHost final : public Effects::EffectPackageHost {
 public:
  std::vector<std::size_t> texture_tables;
  std::vector<std::size_t> sound_banks;
  std::vector<std::size_t> auxiliary;

  void BindTextureTables(std::span<std::byte> block) override {
    texture_tables.push_back(block.size());
  }
  void BindSoundBank(std::span<std::byte> block) override {
    sound_banks.push_back(block.size());
  }
  std::int32_t BindAuxiliarySound(std::span<std::byte> block) override {
    auxiliary.push_back(block.size());
    return 77;
  }
};

class FakeForceHost final : public Effects::ParticleForceHost {
 public:
  std::vector<Math::Vec3> gravity;
  std::vector<std::int32_t> drag;

  void SetParticleGravity(Math::Vec3 value) override {
    gravity.push_back(value);
  }
  void SetParticleDrag(std::int32_t value) override { drag.push_back(value); }
};

class FakePluginHost final : public Effects::PluginUpdateHost {
 public:
  std::vector<bool> present;
  std::vector<std::size_t> calls;

  std::size_t EntryCount() override { return present.size(); }
  bool HasPluginEntry(std::size_t entry) override { return present[entry]; }
  std::int32_t UpdatePlugin(std::size_t entry) override {
    calls.push_back(entry);
    return static_cast<std::int32_t>(entry * 10);
  }
};

// The current package bound to the action package record for one test;
// the package pointers, the action record and the curve samples are
// restored afterwards.
struct LivePackageScope {
  Effects_EffectPackage saved_action = Effects_actionPackage;
  Effects_EffectPackage* saved_package = Effects_package;
  Effects_EffectPackage* saved_saved = Effects_savedPackage;
  std::vector<std::int32_t> saved_samples;

  LivePackageScope() {
    const auto samples = Effects::LiveEffectCurveSamples();
    saved_samples.assign(samples.begin(), samples.end());
    Effects_actionPackage = {};
    Effects_package = &Effects_actionPackage;
  }
  LivePackageScope(const LivePackageScope&) = delete;
  LivePackageScope& operator=(const LivePackageScope&) = delete;
  LivePackageScope(LivePackageScope&&) = delete;
  LivePackageScope& operator=(LivePackageScope&&) = delete;
  ~LivePackageScope() {
    Effects_package = &Effects_actionPackage;
    Effects::ForgetLivePackageExtents();
    Effects_actionPackage = saved_action;
    Effects_package = saved_package;
    Effects_savedPackage = saved_saved;
    std::ranges::copy(saved_samples, Effects::LiveEffectCurveSamples().begin());
  }
};
}  // namespace

TEST_CASE("Effect curves sample by id",
          "[effects][effects-resources-transforms]") {
  const CurveFixture fixture;
  const Effects::EffectCurveBlock block = fixture.Block();
  using Status = Effects::CurveSampleStatus;
  CHECK(Effects::SampleEffectCurve(block, 1, 4).value == 20);
  CHECK(Effects::SampleEffectCurve(block, 1, 4).status == Status::kSampled);
  CHECK(Effects::SampleEffectCurve(block, 4, 9).value == 10);
  const auto negative = Effects::SampleEffectCurve(block, 1, -1);
  CHECK(negative.status == Status::kNegativeDistance);
  CHECK(negative.value == 0);
  CHECK(Effects::IsCurveRefusal(Status::kUnboundCurve));
  CHECK_FALSE(Effects::IsCurveRefusal(Status::kNoCurve));
}

TEST_CASE("Effect curves refuse what the block lacks",
          "[effects][effects-resources-transforms]") {
  const CurveFixture fixture;
  const Effects::EffectCurveBlock block = fixture.Block();
  using Status = Effects::CurveSampleStatus;
  CHECK(Effects::SampleEffectCurve(block, 0, 1).status == Status::kNoCurve);
  CHECK(Effects::SampleEffectCurve(block, 6, 1).status == Status::kNoCurve);
  CHECK(Effects::SampleEffectCurve(block, -1, 1).status ==
        Status::kNegativeCurve);
  CHECK(Effects::SampleEffectCurve(block, 2, 1).status ==
        Status::kUnboundCurve);
  CHECK(Effects::SampleEffectCurve(block, 5, 1).status ==
        Status::kUnboundCurve);
  CHECK(Effects::SampleEffectCurve(block, 3, 1).status == Status::kEmptyCurve);
}

TEST_CASE("Effect curve sweeps write only the curves the block holds",
          "[effects][effects-resources-transforms]") {
  const CurveFixture fixture;
  const Effects::EffectCurveBlock block = fixture.Block();
  std::array<std::int32_t, 5> samples{-1, -1, -1, -1, -1};
  const auto sweep = Effects::SampleAllEffectCurves(block, 2, samples);
  CHECK(sweep.written == 2);
  CHECK(sweep.refused == 3);
  CHECK(samples == std::array<std::int32_t, 5>{-1, 30, -1, -1, 10});

  // Slots past the sample table are refused rather than written.
  std::array<std::int32_t, 2> short_samples{};
  const auto clipped = Effects::SampleAllEffectCurves(block, 0, short_samples);
  CHECK(clipped.written == 1);
  CHECK(clipped.refused == 4);
  CHECK(short_samples[1] == 10);
}

TEST_CASE("Segmented effect curves interpolate their steps",
          "[effects][effects-resources-transforms]") {
  const std::span<const std::uint8_t> table = kSegmentedTable;
  CHECK(Effects::SampleSegmentedEffectCurve(table, 0, 0).value == 10);
  CHECK(Effects::SampleSegmentedEffectCurve(table, 0, 2).value == 20);
  CHECK(Effects::SampleSegmentedEffectCurve(table, 0, 4).value == 30);
  CHECK(Effects::SampleSegmentedEffectCurve(table, 0, 5).value == -1);
  CHECK(Effects::SegmentedEffectCurveDuration(table, 0).value == 5);
}

TEST_CASE("Segmented effect curves refuse what the table lacks",
          "[effects][effects-resources-transforms]") {
  using Status = Effects::CurveSampleStatus;
  const std::span<const std::uint8_t> table = kSegmentedTable;
  CHECK(Effects::SampleSegmentedEffectCurve(table, 1, 0).status ==
        Status::kNoCurve);
  CHECK(Effects::SegmentedEffectCurveDuration(table, 1).status ==
        Status::kNoCurve);
  CHECK(Effects::SampleSegmentedEffectCurve(table, -1, 0).status ==
        Status::kNegativeCurve);
  CHECK(Effects::SampleSegmentedEffectCurve(table.first(3), 0, 0).status ==
        Status::kNoBlock);
}

TEST_CASE("Segmented effect curves refuse steps that leave the table",
          "[effects][effects-resources-transforms]") {
  using Status = Effects::CurveSampleStatus;
  const std::span<const std::uint8_t> table = kSegmentedTable;
  // A curve whose steps run off the table is refused, not read past.
  CHECK(Effects::SampleSegmentedEffectCurve(table.first(9), 0, 9).status ==
        Status::kLeavesBlock);
  CHECK(Effects::SegmentedEffectCurveDuration(table.first(9), 0).status ==
        Status::kLeavesBlock);
  auto bad_offset = kSegmentedTable;
  bad_offset[4] = 0x40;
  CHECK(Effects::SampleSegmentedEffectCurve(bad_offset, 0, 0).status ==
        Status::kLeavesBlock);
}

namespace {
// The mask 0x15 starts channels 0 and 2; bit 4 is past the four channels.
Effects::CameraShakeChannel StartedChannel() {
  return {.phase = 0,
          .shape_curve = 3,
          .curve_step = false,
          .random_step = false,
          .step_curve = 7,
          .random_step_curve = 10,
          .amplitude = 0x80,
          .angle = 3};
}

void StartShakeChannels(Effects::EffectCameraState& state,
                        FakeCameraHost& host) {
  host.random = 23;
  Effects::StartCameraShakeChannels(state, host, 0x15,
                                    {.shape_curve = 0x23,
                                     .amplitude = 0x180,
                                     .step_uses_curve = 2,
                                     .step_curve_or_value = 7,
                                     .random_step_uses_curve = 0,
                                     .random_step_curve_or_value = 10});
}
}  // namespace

TEST_CASE("Effect camera shake channels start by mask",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host;
  StartShakeChannels(state, host);
  CHECK(state.channels[0] == StartedChannel());
  CHECK(state.channels[1] == Effects::CameraShakeChannel{});
  CHECK(state.channels[2] == StartedChannel());
  CHECK(state.enabled_channels == 0x5);
}

TEST_CASE("Effect camera shake phase steps sample their curves",
          "[effects][effects-resources-transforms]") {
  FakeCameraHost host;
  host.random = 23;
  // A zero random limit draws nothing.
  CHECK(Effects::RandomCameraShakePhaseStep(Effects::CameraShakeChannel{},
                                            host) == 0);
  CHECK(host.random_calls == 0);
  host.curve = {.value = 300};
  const Effects::CameraShakeChannel sampled{.curve_step = true,
                                            .random_step = true};
  CHECK(Effects::CameraShakePhaseStep(sampled, host) == 44);
  CHECK(Effects::RandomCameraShakePhaseStep(sampled, host) == 23);
}

TEST_CASE("Effect camera shake channels stop, enable and disable by mask",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host;
  StartShakeChannels(state, host);
  Effects::StopCameraShakeChannels(state, 0x4);
  CHECK(state.channels[2].phase == 0xFF);
  CHECK(state.enabled_channels == 0x1);
  Effects::EnableCameraShakeChannels(state, 0xA);
  CHECK(state.enabled_channels == 0xB);
  Effects::DisableCameraShakeChannels(state, 0x2);
  CHECK(state.enabled_channels == 0x9);
  Effects::DisableCameraShakeChannels(state, 0);
  CHECK(state.enabled_channels == 0);
}

TEST_CASE("Effect camera motion resets every shake channel",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host;
  StartShakeChannels(state, host);
  state.motion_frame = 3;
  state.motion_duration = 9;
  state.transition_mode = 2;
  state.enabled_channels = 0xF;
  Effects::ResetEffectCameraMotion(state);
  CHECK(state.enabled_channels == 0);
  CHECK(state.transition_mode == 0);
  CHECK(state.motion_frame == 0);
  CHECK(state.motion_duration == 0);
  for (const auto& channel : state.channels) CHECK(channel.phase == 0xFF);
}

TEST_CASE("Effect camera shake follows its shape curve until it ends",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host;
  const Effects::CameraShakeChannel channel{
      .phase = 0, .step_curve = 7, .amplitude = 0x80, .angle = 32};
  state.channels[0] = channel;
  state.channels[3] = channel;
  state.enabled_channels = 0x9;
  host.segmented = {.value = 64};
  Effects::UpdateCameraShakeChannels(state, host);
  // A quarter turn at full shape and half amplitude.
  CHECK(state.camera.shake_lateral == 0x20000);
  CHECK(state.camera.shake_roll == 0x20);
  CHECK(state.channels[0].angle == 39);
  CHECK(state.channels[0].phase == 1);

  // The curve's end stops a channel; a refused curve stops it as well.
  host.segmented = {.value = -1};
  state.enabled_channels = 0x1;
  Effects::UpdateCameraShakeChannels(state, host);
  CHECK(state.channels[0].phase == 0xFF);
  CHECK(state.enabled_channels == 0);
  host.segmented = {.value = 0,
                    .status = Effects::CurveSampleStatus::kLeavesBlock};
  state.enabled_channels = 0x8;
  Effects::UpdateCameraShakeChannels(state, host);
  CHECK(state.channels[3].phase == 0xFF);
  CHECK(state.enabled_channels == 0);
}

TEST_CASE("Effect camera transitions hold, then finish and stop applying",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host;
  state.motion_ends[0] = {.position = {.x = 0, .y = 0, .z = 10 << 12},
                          .look_at = {},
                          .shake_lateral = 99,
                          .roll = 0x8000 + 100,
                          .near_clip = 5,
                          .projection_distance = 256,
                          .far_clip = 900};
  state.motion_duration = 2;
  state.transition_mode =
      static_cast<std::uint32_t>(Effects::CameraTransition::kHold);
  for (int frame = 0; frame < 4; ++frame) {
    Effects::UpdateEffectCameraMotion(state, host);
  }
  REQUIRE(host.applied.size() == 3);
  const Effects::AppliedEffectCamera expected{
      .position = {.x = 0, .y = 0, .z = 10 << 12},
      .look_at = {},
      .roll = 100,
      .near_clip = 5,
      .projection_distance = 256,
      .far_clip = 900,
      .texture_lod_hint = 0};
  CHECK(host.applied[0] == expected);
  CHECK(host.applied[2] == expected);
  CHECK(state.camera.shake_lateral == 0);
  CHECK(state.transition_mode == 0);
  CHECK(state.motion_duration == 0);
  CHECK(state.motion_frame == 0);
}

namespace {
FakeCameraHost FieldCameraHost() {
  FakeCameraHost host;
  host.field = {.position = {.x = 1, .y = 2, .z = 3},
                .look_at = {.x = 4, .y = 5, .z = 6},
                .roll = 10,
                .near_clip = 20,
                .projection_distance = 30,
                .far_clip = 40};
  return host;
}
}  // namespace

TEST_CASE("Effect camera capture keeps the shake and offsets the roll",
          "[effects][effects-resources-transforms]") {
  FakeCameraHost host = FieldCameraHost();
  Effects::EffectCameraPose pose{.shake_lateral = 77};
  Effects::CaptureEffectCamera(host, pose);
  CHECK(pose.shake_lateral == 77);
  CHECK(pose.roll == 0x8000 + 10);
  CHECK(pose.position == Math::Vec3{.x = 1, .y = 2, .z = 3});
}

TEST_CASE("Effect camera focus eases from the captured camera",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host = FieldCameraHost();
  Effects::FocusCameraOnEffectTransform(state, host, {.x = 7, .y = 8, .z = 9},
                                        0);
  CHECK(state.motion_ends[0].look_at == Math::Vec3{.x = 4, .y = 5, .z = 6});
  CHECK(state.motion_ends[1].look_at ==
        Math::Vec3{.x = 7 << 12, .y = 8 << 12, .z = 9 << 12});
  CHECK(state.motion_ends[1].far_clip == 40);
  CHECK(state.motion_duration == 1);
  CHECK(state.motion_frame == 0);
  CHECK(state.transition_mode == 2);
}

TEST_CASE("Effect camera restore eases back to the saved action camera",
          "[effects][effects-resources-transforms]") {
  Effects::EffectCameraState state{};
  FakeCameraHost host = FieldCameraHost();
  Effects::FocusCameraOnEffectTransform(state, host, {.x = 7, .y = 8, .z = 9},
                                        0);
  state.saved_action_camera.roll = 1234;
  Effects::RestoreActionEffectCamera(state, host, 5);
  CHECK(state.motion_ends[1] == state.saved_action_camera);
  CHECK(state.motion_duration == 5);
}

namespace {
// Two block-10 records and a partial third; the first sets every field.
std::vector<std::byte> TransformDefinitionBlock() {
  std::vector<std::byte> block((2 * Effects::kTransformDefinitionBytes) + 10);
  PutHalfword(block, 0x00, 0x1234);
  block[0x02] = std::byte{5};
  block[0x03] = std::byte{6};
  block[0x04] = std::byte{1};
  block[0x05] = std::byte{2};
  block[0x06] = std::byte{3};
  block[0x07] = std::byte{4};
  PutHalfword(block, 0x08, 0x0102);
  PutHalfword(block, 0x0A, 0x0A0B);
  PutHalfword(block, 0x0C, 0x0304);
  block[0x0E] = std::byte{9};
  block[0x0F] = std::byte{0x73};
  PutHalfword(block, 0x10, 0x0506);
  PutHalfword(block, 0x12, 0x0708);
  PutHalfword(block, 0x14, 0xFFFB);
  PutHalfword(block, 0x16, 100);
  PutHalfword(block, 0x2C, 4096);
  PutHalfword(block, 0x36, 0x8000);
  PutHalfword(block, Effects::kTransformDefinitionBytes, 0xBEEF);
  return block;
}

std::array<Effects::TransformDefinition, 4> DecodedTransformDefinitions() {
  std::array<Effects::TransformDefinition, 4> definitions{};
  REQUIRE(Effects::ReadTransformDefinitions(TransformDefinitionBlock(),
                                            definitions) == 2);
  return definitions;
}
}  // namespace

TEST_CASE("Transform definitions decode the attachment and curve ids",
          "[effects][effects-resources-transforms]") {
  const auto definitions = DecodedTransformDefinitions();
  const auto& first = definitions[0];
  CHECK(first.flags == 0x1234);
  CHECK(first.attach_actor == 5);
  CHECK(first.attach_bone == 6);
  CHECK(first.translation_curve == 1);
  CHECK(first.curve == 2);
  CHECK(first.rotation_curve == 3);
  CHECK(first.scale_curve == 4);
}

TEST_CASE("Transform definitions decode the rotation, mode and radius",
          "[effects][effects-resources-transforms]") {
  const auto definitions = DecodedTransformDefinitions();
  const auto& first = definitions[0];
  CHECK(first.rotation_x == 0x0102);
  CHECK(first.mode == 0x0A0B);
  CHECK(first.rotation_y == 0x0304);
  CHECK(first.limit == 9);
  CHECK(first.radius == 3);
  CHECK(first.radius_jitter == 7);
  CHECK(first.rotation_z == 0x0506);
}

TEST_CASE("Transform definitions decode the ranges and whole records",
          "[effects][effects-resources-transforms]") {
  const auto definitions = DecodedTransformDefinitions();
  const auto& first = definitions[0];
  CHECK(first.shape == 0x0708);
  CHECK(first.rotation_range[0] == Math::ScalarSpan{.start = -5, .end = 100});
  CHECK(first.scale_range[0].start == 4096);
  CHECK(first.scale_range[2].end == -0x8000);
  CHECK(definitions[1].flags == 0xBEEF);
  CHECK(definitions[2] == Effects::TransformDefinition{});
}

TEST_CASE("Transform definitions read only whole records that fit",
          "[effects][effects-resources-transforms]") {
  const auto block = TransformDefinitionBlock();
  std::array<Effects::TransformDefinition, 1> one{};
  CHECK(Effects::ReadTransformDefinitions(block, one) == 1);
  std::array<Effects::TransformDefinition, 4> definitions{};
  CHECK(Effects::ReadTransformDefinitions(
            std::span(block).first(Effects::kTransformDefinitionBytes - 1),
            definitions) == 0);
}

namespace {
// A state the initialisation must overwrite.
Effects::TransformState DirtyTransformState() {
  Effects::TransformState state{};
  state.flags = 0xF;
  state.frame = 9;
  state.position = {.x = 1, .y = 1, .z = 1};
  return state;
}

// Two definitions initialised over three dirty states.
std::array<Effects::TransformState, 3> InitializedTransformStates() {
  std::array<Effects::TransformDefinition, 2> definitions{};
  definitions[0] = {.flags = static_cast<std::uint16_t>(
                        kAttached | kAttachToReceiver | kKeepEndpointBone |
                        kTargeted | kTargetEmitter),
                    .attach_bone = 9,
                    .rotation_x = 0xFFFF,
                    .mode = 0x0500,
                    .rotation_y = 2,
                    .rotation_z = 3};
  definitions[1] = {.flags = static_cast<std::uint16_t>(kAttached | kTargeted),
                    .attach_actor = 0x12,
                    .attach_bone = 2,
                    .mode = 0x0311};
  const Effects::TransformEndpoints endpoints{
      .emit_from = {.type = 1,
                    .position = {.x = 10, .y = 20, .z = 30, .pad = 0}},
      .emit_to = {.type = Effects::kActorPartEndpoint, .actor = 3, .part = 7}};
  std::array<Effects::TransformState, 3> states{};
  states.fill(DirtyTransformState());
  REQUIRE(Effects::InitializeTransformStates(definitions, endpoints, states) ==
          2);
  return states;
}
}  // namespace

TEST_CASE("Transform states reset and take the definition's rotation",
          "[effects][effects-resources-transforms]") {
  const auto states = InitializedTransformStates();
  CHECK(states[0].rotation == std::array<std::int16_t, 3>{-1, 2, 3});
  CHECK(states[0].flags == 0);
  CHECK(states[0].frame == 0);
  CHECK(states[0].position == Math::Vec3{});
  CHECK(states[0].facing.rotation == Math::IdentityRotation());
}

TEST_CASE("Transform states attach to the receiver and target the emitter",
          "[effects][effects-resources-transforms]") {
  const auto states = InitializedTransformStates();
  CHECK(states[0].actor == 3);
  CHECK(states[0].bone == 7);
  CHECK(states[0].target_actor == Effects::kNoAttachment);
  CHECK(states[0].target_bone == 5);
  CHECK(states[0].saved ==
        Effects::TransformPoint{.x = 10, .y = 20, .z = 30, .pad = 0});
}

TEST_CASE("Transform states resolve the origin and camera-offset ids",
          "[effects][effects-resources-transforms]") {
  const auto states = InitializedTransformStates();
  CHECK(states[1].actor == Effects::kOriginAttachment);
  CHECK(states[1].bone == 2);
  CHECK(states[1].target_actor == Effects::kCameraOffsetAttachment);
  CHECK(states[1].target_bone == 3);
  CHECK(states[2] == DirtyTransformState());
}

TEST_CASE("Animated local transforms refuse curve ids past the samples",
          "[effects][effects-resources-transforms]") {
  std::array<std::int16_t, 3> rotation{1, 2, 3};
  Effects::TransformDefinition definition{};
  auto still = Effects::BuildAnimatedLocalTransform(
      definition, rotation, std::span<const std::int32_t>{});
  const auto& still_transform = Required(still);
  CHECK(still_transform.rotation == Math::IdentityRotation());
  CHECK(still_transform.translation == Math::Vec3{});

  definition.flags = kAnimatedLocal;
  definition.rotation_curve = 1;
  definition.translation_curve = 2;
  definition.scale_curve = 0;
  definition.rotation_range[0] = {.start = 0, .end = 100};
  definition.translation_range[0] = {.start = 0, .end = 256};
  for (auto& scale : definition.scale_range) scale = {4096, 4096};
  const std::array<std::int32_t, 3> samples{0, 128, 64};
  auto animated =
      Effects::BuildAnimatedLocalTransform(definition, rotation, samples);
  const auto& animated_transform = Required(animated);
  CHECK(rotation == std::array<std::int16_t, 3>{101, 2, 3});
  CHECK(animated_transform.translation == Math::Vec3{.x = 128, .y = 0, .z = 0});

  definition.scale_curve = 3;
  CHECK_FALSE(
      Effects::BuildAnimatedLocalTransform(definition, rotation, samples)
          .has_value());
  CHECK(rotation == std::array<std::int16_t, 3>{101, 2, 3});
}

namespace {
// A targeted linear motion over three steps from a saved target.
Effects::TransformDefinition TargetedLinearMotion() {
  return {.flags = static_cast<std::uint16_t>(kTargeted | kLinearMotion),
          .curve = 4,
          .limit = 3};
}

Effects::TransformState SavedTargetState(std::int16_t frame) {
  Effects::TransformState state{};
  state.saved = {.x = 400, .y = 800, .z = -400, .pad = 0};
  state.frame = frame;
  return state;
}
}  // namespace

TEST_CASE("Transform motion moves along the target line",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  Effects::TransformDefinition definition = TargetedLinearMotion();
  Effects::TransformState state = SavedTargetState(2);
  using Status = Effects::TransformStatus;
  CHECK(Effects::InterpolateTransformPosition(definition, state, host) ==
        Status::kApplied);
  CHECK(state.position == Math::Vec3{.x = 200, .y = 400, .z = -200});

  definition.mode = kFromSavedTarget;
  state.frame = 1;
  CHECK(Effects::InterpolateTransformPosition(definition, state, host) ==
        Status::kApplied);
  CHECK(state.position == Math::Vec3{.x = 300, .y = 600, .z = -300});
}

TEST_CASE("Transform motion eases along the definition's curve",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  Effects::TransformDefinition definition = TargetedLinearMotion();
  Effects::TransformState state = SavedTargetState(1);
  // Curve easing samples the definition's curve (1-based) at the frame.
  definition.mode = 2 << 5;
  host.curve = {.value = 64};
  CHECK(Effects::InterpolateTransformPosition(definition, state, host) ==
        Effects::TransformStatus::kApplied);
  CHECK(state.position == Math::Vec3{.x = 200, .y = 400, .z = -200});
  CHECK(host.curve_ids == std::vector<std::int32_t>{5});
}

TEST_CASE("Transform motion keeps the position for an unknown easing",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  Effects::TransformDefinition definition = TargetedLinearMotion();
  Effects::TransformState state = SavedTargetState(1);
  state.position = {.x = 200, .y = 400, .z = -200};
  using Status = Effects::TransformStatus;
  // Easing 3 has no interpolation: the position is kept.
  definition.mode = 3 << 5;
  CHECK(Effects::InterpolateTransformPosition(definition, state, host) ==
        Status::kUnknownEasing);
  CHECK(Effects::InterpolateTransformSpline(definition, state, host) ==
        Status::kUnknownEasing);
  CHECK(state.position == Math::Vec3{.x = 200, .y = 400, .z = -200});
}

TEST_CASE("Transform motion frames advance while the limit allows",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  const Effects::TransformDefinition definition = TargetedLinearMotion();
  Effects::TransformState state = SavedTargetState(3);
  // The frame advances while the state asks and the limit allows.
  state.flags = kFrameAdvances;
  CHECK(Effects::AdvanceTransformMotion(definition, state, host) ==
        Effects::TransformStatus::kApplied);
  CHECK(state.frame == 4);
  static_cast<void>(Effects::AdvanceTransformMotion(definition, state, host));
  CHECK(state.frame == 4);
}

TEST_CASE("Transform attachments resolve actors through their anchors",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  using Status = Effects::TransformStatus;
  Effects::TransformState state{};
  state.actor = 2;
  CHECK(Effects::ResolveTransformAttachment(state, host) == Status::kApplied);
  CHECK(host.anchor_calls == 1);
  CHECK(state.attachment == host.anchor);
  CHECK(state.saved.pad == 0x400);

  host.facing.reset();
  state.saved.pad = 7;
  CHECK(Effects::ResolveTransformAttachment(state, host) ==
        Status::kNoPlacement);
  CHECK(state.saved.pad == 7);
}

TEST_CASE("Transform attachments resolve the origin and the camera",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  using Status = Effects::TransformStatus;
  Effects::TransformState state{};
  state.actor = Effects::kOriginAttachment;
  CHECK(Effects::ResolveTransformAttachment(state, host) == Status::kApplied);
  CHECK(state.attachment.rotation == Math::IdentityRotation());
  CHECK(state.attachment.translation ==
        Math::Vec3{.x = 100, .y = 200, .z = 300});

  host.camera = {.position = {.x = 5 << 12, .y = 0, .z = 100 << 12},
                 .look_at = {.x = 5 << 12, .y = 0, .z = 0}};
  state.actor = Effects::kCameraAttachment;
  CHECK(Effects::ResolveTransformAttachment(state, host) == Status::kApplied);
  CHECK(state.attachment.translation == Math::Vec3{.x = 5, .y = 0, .z = 0});
}

TEST_CASE("Transform attachments keep the attachment for other ids",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  Effects::TransformState state{};
  state.attachment = host.anchor;
  // Other ids past the actors leave the attachment as it was.
  const Math::Transform3 before = state.attachment;
  state.actor = 0x13;
  CHECK(Effects::ResolveTransformAttachment(state, host) ==
        Effects::TransformStatus::kApplied);
  CHECK(state.attachment == before);
  CHECK(Effects::BuildEffectOriginAttachment(host).translation ==
        Math::Vec3{.x = 100, .y = 200, .z = 300});
}

TEST_CASE("Effect transform updates skip a frame for a curve past the samples",
          "[effects][effects-resources-transforms]") {
  FakeTransformHost host;
  std::array<Effects::TransformDefinition, 2> definitions{};
  definitions[0].flags = kAnimatedLocal;
  definitions[0].scale_curve = 20;
  std::array<Effects::TransformState, 2> states{};
  states[0].frame = 6;
  states[1].world.translation = {.x = 9, .y = 9, .z = 9};
  const Effects::TransformState refused = states[0];
  const auto pass = Effects::UpdateEffectTransforms(definitions, states, host);
  CHECK(pass.applied == 1);
  CHECK(pass.refused == 1);
  CHECK(states[0] == refused);
  CHECK(states[1].world.rotation == Math::IdentityRotation());
  CHECK(states[1].world.translation == Math::Vec3{});
  CHECK(states[1].view == states[1].world);
  CHECK(Effects::UpdateEffectTransforms(
            std::span<const Effects::TransformDefinition>{}, states, host)
            .applied == 0);
}

TEST_CASE("Effect resource contexts select, save and restore packages",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceContext context{.action = PackageId(1),
                                         .secondary = PackageId(2)};
  Effects::SelectEffectResourceContext(context, 7);
  CHECK(context.current == PackageId(1));
  Effects::SaveEffectResourceContext(context);
  Effects::SelectEffectResourceContext(context, 0);
  CHECK(context.current == PackageId(2));
  Effects::RestoreEffectResourceContext(context);
  CHECK(context.current == PackageId(1));
}

TEST_CASE("Effect resource initialisation clears reads, buffers and pages",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceContext context{.action = PackageId(1),
                                         .secondary = PackageId(2)};
  Effects::EffectResourceState state{
      .read = ReadId(4),
      .package_buffer = {.id = BufferId(5), .capacity = 8},
      .texture_buffer = {.id = BufferId(6), .capacity = 8},
      .purge_overlay_loaded = true,
      .loaded_texture_pages = 1U << 24};
  FakeLifecycleHost host;
  Effects::InitializeEffectResources(state, context, host);
  CHECK(state.read == Effects::DiscReadId::kNone);
  CHECK(state.package_buffer == Effects::EffectBuffer{});
  CHECK(state.texture_buffer == Effects::EffectBuffer{});
  CHECK_FALSE(state.purge_overlay_loaded);
  CHECK(state.loaded_texture_pages == 0);
}

TEST_CASE("Effect resource initialisation binds the secondary package",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceContext context{.action = PackageId(1),
                                         .secondary = PackageId(2)};
  Effects::SelectEffectResourceContext(context, 7);
  Effects::EffectResourceState state{};
  FakeLifecycleHost host;
  Effects::InitializeEffectResources(state, context, host);
  CHECK(host.resets == 1);
  CHECK(context.current == PackageId(2));
  CHECK(host.empty_bindings == std::vector{PackageId(2)});
}

namespace {
Effects::EffectPackageBlocks BlocksWithParameters(
    std::span<std::byte> parameters) {
  Effects::EffectPackageBlocks blocks{};
  blocks.parameters = parameters;
  return blocks;
}

// Blocks 2, 4, 7, 12 and 13 bound over a previous package's parameters.
struct KnownBlockWalk {
  std::vector<std::byte> package = BuildPackage({
      {.type = 2, .payload = {2, 0, 0, 0, 8, 0, 9, 0, 0xAA, 0xBB, 0, 0}},
      {.type = 4, .payload = {2, 0, 6, 0, 2, 3, 10, 11, 20, 21, 22, 0}},
      {.type = 7, .payload = {1, 2, 3, 4}},
      {.type = 12, .payload = {5, 6, 7, 8}},
      {.type = 13, .payload = {9, 9, 9, 9, 9, 9, 9, 9}},
  });
  std::array<std::byte, 4> previous{};
  Effects::EffectPackageBlocks blocks = BlocksWithParameters(previous);
  FakePackageHost host;
  Effects::PackageWalk walk =
      Effects::BindEffectPackageBlocks(package, blocks, host);

  KnownBlockWalk() = default;
  KnownBlockWalk(const KnownBlockWalk&) = delete;
  KnownBlockWalk& operator=(const KnownBlockWalk&) = delete;
  KnownBlockWalk(KnownBlockWalk&&) = delete;
  KnownBlockWalk& operator=(KnownBlockWalk&&) = delete;
  ~KnownBlockWalk() = default;
};
}  // namespace

TEST_CASE("Effect package walks bind known blocks until the terminator",
          "[effects][effects-resources-transforms]") {
  const KnownBlockWalk fixture;
  const auto& walk = fixture.walk;
  using Type = Effects::PackageBlockType;
  CHECK(walk.status == Effects::PackageWalkStatus::kEnded);
  CHECK(walk.blocks == 5);
  CHECK(walk.bound_types == (Effects::PackageBlockBit(Type::kAnimationTable) |
                             Effects::PackageBlockBit(Type::kCurves) |
                             Effects::PackageBlockBit(Type::kSoundBank) |
                             Effects::PackageBlockBit(Type::kAuxiliarySound)));
  CHECK(walk.curves.header_count == 2);
  CHECK(walk.curves.bound == 2);
  CHECK(walk.curves.refused == 0);
}

TEST_CASE("Effect package walks bind each curve's bytes",
          "[effects][effects-resources-transforms]") {
  const KnownBlockWalk fixture;
  CHECK(Bytes(fixture.blocks.curves[0]) == std::vector<std::uint8_t>{10, 11});
  CHECK(Bytes(fixture.blocks.curves[1]) ==
        std::vector<std::uint8_t>{20, 21, 22});
  CHECK(fixture.blocks.animation_count == 2);
}

TEST_CASE("Effect package walks bind the particle animations",
          "[effects][effects-resources-transforms]") {
  const KnownBlockWalk fixture;
  auto first = Effects::ParticleAnimationData(fixture.blocks, 0);
  CHECK(Bytes(Required(first)) == std::vector<std::uint8_t>{0xAA, 0xBB, 0, 0});
  auto second = Effects::ParticleAnimationData(fixture.blocks, 1);
  CHECK(Required(second).size() == 3);
  CHECK_FALSE(Effects::ParticleAnimationData(fixture.blocks, 2).has_value());
  CHECK_FALSE(Effects::ParticleAnimationData(fixture.blocks, -1).has_value());
}

TEST_CASE("Effect package walks bind sounds and keep views the package lacks",
          "[effects][effects-resources-transforms]") {
  const KnownBlockWalk fixture;
  CHECK(fixture.host.sound_banks == std::vector<std::size_t>{4});
  CHECK(fixture.host.auxiliary == std::vector<std::size_t>{8});
  CHECK(fixture.blocks.auxiliary_sound_handle == 77);
  // A block the package lacks keeps the previous package's view.
  CHECK(fixture.blocks.parameters.data() == fixture.previous.data());
}

TEST_CASE("Effect package walks refuse short, overlong and open packages",
          "[effects][effects-resources-transforms]") {
  Effects::EffectPackageBlocks blocks{};
  FakePackageHost host;
  using Status = Effects::PackageWalkStatus;
  auto short_block = BuildPackage({});
  short_block[0] = std::byte{2};
  short_block[2] = std::byte{3};
  CHECK(Effects::BindEffectPackageBlocks(short_block, blocks, host).status ==
        Status::kShortBlock);
  auto leaving = BuildPackage({{.type = 3, .payload = {1, 2, 3, 4}}});
  leaving[0] = std::byte{0x20};
  CHECK(Effects::BindEffectPackageBlocks(leaving, blocks, host).status ==
        Status::kLeavesPackage);
  auto unterminated =
      BuildPackage({{.type = 3, .payload = {1, 2, 3, 4}}}, false);
  CHECK(Effects::BindEffectPackageBlocks(unterminated, blocks, host).status ==
        Status::kNoTerminator);
  CHECK(Effects::BindEffectPackageBlocks({}, blocks, host).status ==
        Status::kNoTerminator);
}

TEST_CASE("Effect curve binding refuses curves past the block",
          "[effects][effects-resources-transforms]") {
  Effects::EffectPackageBlocks blocks{};
  std::array<std::byte, 10> clipped{};
  const std::array<std::uint8_t, 10> clipped_bytes{3, 0, 7, 0, 2,
                                                   2, 9, 1, 2, 3};
  std::ranges::transform(clipped_bytes, clipped.begin(),
                         [](std::uint8_t value) { return std::byte{value}; });
  const auto binding = Effects::BindEffectCurves(blocks, clipped);
  CHECK(binding.header_count == 3);
  CHECK(binding.bound == 1);
  CHECK(binding.refused == 2);
  CHECK(Bytes(blocks.curves[0]) == std::vector<std::uint8_t>{1, 2});
  CHECK(blocks.curves[1].empty());
  CHECK(blocks.curves[2].empty());
}

TEST_CASE("Effect curve binding refuses curves past its capacity",
          "[effects][effects-resources-transforms]") {
  Effects::EffectPackageBlocks blocks{};
  std::vector<std::byte> crowded(44);
  crowded[0] = std::byte{40};
  crowded[2] = std::byte{44};
  const auto capped = Effects::BindEffectCurves(blocks, crowded);
  CHECK(capped.header_count == 40);
  CHECK(capped.bound == 32);
  CHECK(capped.refused == 8);

  std::array<std::byte, 3> tiny{};
  CHECK(Effects::BindEffectCurves(blocks, tiny).refused == 1);
}

namespace {
constexpr std::array<std::int16_t, 6> kGravity{-100, 100, 0, 200, 50, 50};

// Gravity on curve 1, drag {0, 128} on curve 2, 30 frames, two transforms
// and timeline ids 1-4.
std::vector<std::byte> PackageParameterBytes() {
  std::vector<std::byte> parameters(Effects::kPackageParametersBytes);
  for (std::size_t i = 0; i < kGravity.size(); ++i) {
    PutHalfword(parameters, i * 2, static_cast<std::uint16_t>(kGravity[i]));
  }
  PutHalfword(parameters, 0xC, 0);
  PutHalfword(parameters, 0xE, 128);
  parameters[0x12] = std::byte{1};
  parameters[0x13] = std::byte{2};
  parameters[0x14] = std::byte{30};
  parameters[0x15] = std::byte{2};
  for (std::size_t i = 0; i < 4; ++i) {
    parameters[0x16 + i] = static_cast<std::byte>(i + 1);
  }
  return parameters;
}
}  // namespace

TEST_CASE("Package parameters read the ranges, counts and timelines",
          "[effects][effects-resources-transforms]") {
  const auto parameters = PackageParameterBytes();
  auto read = Effects::ReadPackageParameters(parameters);
  const auto& full = Required(read);
  CHECK(full.gravity_range == kGravity);
  CHECK(full.drag_range == std::array<std::int16_t, 2>{0, 128});
  CHECK(full.frame_count == 30);
  CHECK(full.transform_count == 2);
  CHECK(full.timeline_ids == std::array<std::uint8_t, 4>{1, 2, 3, 4});
}

TEST_CASE("Package parameters accept a block 9 trimmed after two timelines",
          "[effects][effects-resources-transforms]") {
  const auto parameters = PackageParameterBytes();
  // Sixteen shipped packages end block 9 after two timeline ids.
  auto trimmed = Effects::ReadPackageParameters(
      std::span(parameters)
          .first(Effects::kPackageParametersRequiredBytes + 2));
  CHECK(Required(trimmed).timeline_ids ==
        std::array<std::uint8_t, 4>{1, 2, 0, 0});
  CHECK_FALSE(Effects::ReadPackageParameters(
                  std::span(parameters)
                      .first(Effects::kPackageParametersRequiredBytes - 1))
                  .has_value());
}

TEST_CASE("Package parameters drive the particle gravity and drag",
          "[effects][effects-resources-transforms]") {
  const auto parameters = PackageParameterBytes();
  FakeForceHost host;
  using Status = Effects::ParticleForceStatus;
  const std::array<std::int32_t, 3> samples{0, 64, 128};
  CHECK(Effects::UpdateParticleGravityAndDrag(parameters, samples, host) ==
        Status::kApplied);
  CHECK(host.gravity == std::vector{Math::Vec3{.x = 0, .y = 100, .z = 50}});
  CHECK(host.drag == std::vector<std::int32_t>{128});
  CHECK(Effects::UpdateParticleGravityAndDrag(parameters,
                                              std::span(samples).first(2),
                                              host) == Status::kCurveIndex);
  CHECK(Effects::UpdateParticleGravityAndDrag({}, samples, host) ==
        Status::kNoParameters);
  CHECK(host.gravity.size() == 1);
}

namespace {
constexpr Effects::DiscRead kResourceRead{.lba = 5, .size = 0x80};

Effects::EffectBuffer ReadDestination() {
  return {.id = BufferId(0x10), .capacity = 0x100};
}
}  // namespace

TEST_CASE("Effect resource reads issue once into their destination",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  const auto poll = Effects::PollEffectResourceRead(state, host, kResourceRead,
                                                    ReadDestination());
  CHECK(poll.pending);
  CHECK(poll.refusal == Effects::ResourceRefusal::kNone);
  CHECK(host.requested == std::vector{kResourceRead});
  REQUIRE(host.enqueued.size() == 1);
  CHECK(host.enqueued[0].second == BufferId(0x10));
}

TEST_CASE("Effect resource reads report pending until loaded",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  const auto destination = ReadDestination();
  static_cast<void>(
      Effects::PollEffectResourceRead(state, host, kResourceRead, destination));
  const auto issued = state.read;
  CHECK(issued != Effects::DiscReadId::kNone);
  CHECK(Effects::PollEffectResourceRead(state, host, kResourceRead, destination)
            .pending);
  CHECK(host.requested.size() == 1);
  host.loaded = true;
  const auto poll =
      Effects::PollEffectResourceRead(state, host, kResourceRead, destination);
  CHECK_FALSE(poll.pending);
  CHECK(host.released == std::vector{issued});
  CHECK(state.read == Effects::DiscReadId::kNone);
}

TEST_CASE("Effect resource reads refuse a missing buffer or oversized read",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  const auto destination = ReadDestination();
  using Refusal = Effects::ResourceRefusal;
  auto poll = Effects::PollEffectResourceRead(state, host, kResourceRead, {});
  CHECK_FALSE(poll.pending);
  CHECK(poll.refusal == Refusal::kNoBuffer);
  poll = Effects::PollEffectResourceRead(state, host, {.lba = 5, .size = 0x101},
                                         destination);
  CHECK(poll.refusal == Refusal::kReadTooLarge);
  poll = Effects::PollEffectResourceRead(state, host, {.lba = 5, .size = -1},
                                         destination);
  CHECK(poll.refusal == Refusal::kReadTooLarge);
  CHECK(host.requested.empty());
}

TEST_CASE("Effect resource reads complete a read the queue refuses",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.queue_full = true;
  CHECK(Effects::PollEffectResourceRead(state, host, kResourceRead,
                                        ReadDestination())
            .pending);
  CHECK(host.refused_completions == 1);
  CHECK(state.read == ReadId(0x900));
}

TEST_CASE("Effect packages load into a buffer",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  Effects::ConfigureEffectPackageLoad(state, {.lba = 3, .size = 0x1800});
  CHECK(Effects::LoadEffectPackage(state, host).pending);
  CHECK(host.allocations == std::vector<std::size_t>{0x1800});
  CHECK(host.requested ==
        std::vector{Effects::DiscRead{.lba = 1003, .size = 0x1800}});
  CHECK(state.package_buffer.id != Effects::EffectBufferId::kNone);
  host.loaded = true;
  CHECK_FALSE(Effects::LoadEffectPackage(state, host).pending);
  CHECK(host.allocations.size() == 1);
}

TEST_CASE("Effect packages release their pages and buffer",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  Effects::ConfigureEffectPackageLoad(state, {.lba = 3, .size = 0x1800});
  static_cast<void>(Effects::LoadEffectPackage(state, host));
  host.loaded = true;
  static_cast<void>(Effects::LoadEffectPackage(state, host));
  const auto buffer = state.package_buffer.id;
  state.loaded_texture_pages = (1U << 24) | (1U << 30) | (1U << 5);
  Effects::ReleaseEffectPackageResources(state, host);
  CHECK(host.retired == std::vector<std::uint32_t>{24, 30});
  CHECK(state.loaded_texture_pages == (1U << 5));
  CHECK(host.freed == std::vector{buffer});
  CHECK(state.package_buffer == Effects::EffectBuffer{});
}

TEST_CASE("Effect purge overlays load once",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  CHECK(Effects::LoadEffectPurgeOverlay(state, host).pending);
  CHECK_FALSE(Effects::LoadEffectPurgeOverlay(state, host).pending);
  CHECK(host.overlay_acquired == 1);
  CHECK(host.requested.back() == host.layout.purge_overlay);
}

namespace {
// A CLUT for slot 2, then a texture page for slot 3 (page 27).
void ConfigureClutAndPage(Effects::EffectResourceState& state) {
  Effects::SetEffectTextureAssetCount(state, 2);
  REQUIRE(Effects::ConfigureEffectTextureAsset(
      state, {.file = {.lba = 1, .size = 0x200}, .slot = 2, .is_clut = true},
      0));
  REQUIRE(Effects::ConfigureEffectTextureAsset(
      state, {.file = {.lba = 4, .size = 0x8000}, .slot = 3, .is_clut = false},
      1));
}

// The four polls that load both assets.
void LoadClutAndPage(Effects::EffectResourceState& state,
                     FakeResourceHost& host) {
  ConfigureClutAndPage(state);
  for (int poll = 0; poll < 4; ++poll) {
    static_cast<void>(Effects::UpdateEffectTextureLoading(state, host));
  }
}
}  // namespace

TEST_CASE("Effect textures load the CLUT, then each texture page",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  ConfigureClutAndPage(state);
  CHECK_FALSE(Effects::ConfigureEffectTextureAsset(state, {}, 8));
  for (int poll = 0; poll < 4; ++poll) {
    const auto result = Effects::UpdateEffectTextureLoading(state, host);
    CHECK(result.pending);
    CHECK(result.refusal == Effects::ResourceRefusal::kNone);
  }
  CHECK(Effects::EffectTextureLoadState(state) ==
        Effects::TextureLoadState::kLoaded);
  CHECK(host.uploads.size() == 2);
}

TEST_CASE("Effect textures upload the CLUT row and reserve the page",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  LoadClutAndPage(state, host);
  REQUIRE(host.uploads.size() == 2);
  CHECK(host.uploads[0].first ==
        Effects::VramRect{.x = 768, .y = 242, .w = 256, .h = 3});
  CHECK(host.uploads[1].first ==
        Effects::VramRect{.x = 704, .y = 256, .w = 64, .h = 256});
  CHECK(host.reserved == std::vector<std::uint32_t>{27});
  CHECK(state.loaded_texture_pages == (1U << 27));
  CHECK(host.requested ==
        std::vector{Effects::DiscRead{.lba = 1001, .size = 0x200},
                    Effects::DiscRead{.lba = 1004, .size = 0x8000}});
  CHECK(host.allocations == std::vector<std::size_t>{0x8000});
}

TEST_CASE("Effect textures free the staging buffer after the last page",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  LoadClutAndPage(state, host);
  const auto staging = state.texture_buffer.id;
  CHECK_FALSE(Effects::UpdateEffectTextureLoading(state, host).pending);
  CHECK(host.freed == std::vector{staging});
  CHECK(state.texture_buffer == Effects::EffectBuffer{});
}

TEST_CASE("Effect textures refuse and skip a slot past pages 24-31",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  Effects::SetEffectTextureAssetCount(state, 1);
  REQUIRE(
      Effects::ConfigureEffectTextureAsset(state, {.file = {}, .slot = 8}, 0));
  CHECK(Effects::UpdateEffectTextureLoading(state, host).pending);
  const auto refused = Effects::UpdateEffectTextureLoading(state, host);
  CHECK(refused.refusal == Effects::ResourceRefusal::kTextureSlot);
  CHECK(Effects::EffectTextureLoadState(state) ==
        Effects::TextureLoadState::kLoaded);
  CHECK(host.uploads.empty());
}

TEST_CASE("Effect textures refuse an asset index or state they lack",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  host.loaded = true;
  using Load = Effects::TextureLoadState;
  Effects::SetEffectTextureLoadState(state, Load::kLoading);
  state.texture_index = 8;
  CHECK(Effects::UpdateEffectTextureLoading(state, host).refusal ==
        Effects::ResourceRefusal::kAssetIndex);
  CHECK(Effects::EffectTextureLoadState(state) == Load::kLoaded);
  // A state the original's switch has no case for.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Effects::SetEffectTextureLoadState(state, static_cast<Load>(7));
  const auto unknown = Effects::UpdateEffectTextureLoading(state, host);
  CHECK_FALSE(unknown.pending);
  CHECK(unknown.refusal == Effects::ResourceRefusal::kUnknownState);
}

TEST_CASE("Effect plugins load into the purge overlay slot",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  Effects::ConfigureEffectPluginLoad(state, {.lba = 9, .size = 0x400}, 2);
  Effects::SetEffectPluginLoadState(state,
                                    Effects::PluginLoadState::kAcquireOverlay);
  CHECK(Effects::UpdateEffectPluginLoading(state, host).pending);
  CHECK(host.overlay_acquired == 1);
  CHECK(state.purge_overlay_loaded);
  CHECK(host.requested ==
        std::vector{Effects::DiscRead{.lba = 1009, .size = 0x400}});
  REQUIRE(host.enqueued.size() == 1);
  CHECK(host.enqueued[0].second == host.overlay.id);
}

TEST_CASE("Effect plugins finish loading once and refuse unknown states",
          "[effects][effects-resources-transforms]") {
  Effects::EffectResourceState state{};
  FakeResourceHost host;
  using Plugin = Effects::PluginLoadState;
  Effects::ConfigureEffectPluginLoad(state, {.lba = 9, .size = 0x400}, 2);
  Effects::SetEffectPluginLoadState(state, Plugin::kAcquireOverlay);
  static_cast<void>(Effects::UpdateEffectPluginLoading(state, host));
  host.loaded = true;
  CHECK_FALSE(Effects::UpdateEffectPluginLoading(state, host).pending);
  CHECK(state.plugin_state == Plugin::kLoaded);
  CHECK_FALSE(Effects::UpdateEffectPluginLoading(state, host).pending);
  CHECK(host.requested.size() == 1);
  CHECK(state.plugin_entry_count == 2);

  // A state the original's switch has no case for.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Effects::SetEffectPluginLoadState(state, static_cast<Plugin>(9));
  CHECK(Effects::UpdateEffectPluginLoading(state, host).refusal ==
        Effects::ResourceRefusal::kUnknownState);
}

namespace {
using Extent = Effects::EffectFileExtent;

// Three effects: 0 has two FBTs and two plugin entries, 1 has no textures
// and one entry, 2 has neither entries nor FBTs.
struct PurgeTableFixture {
  std::array<std::uint8_t, 3> uses{1, 0, 1};
  std::array<Extent, 3> cluts{Extent{10, 0x200}, Extent{11, 0x200},
                              Extent{12, 0x200}};
  std::array<std::uint8_t, 3> clut_slots{0, 1, 2};
  std::array<std::uint8_t, 3> counts{2, 1, 0};
  std::array<Extent, 3> textures{Extent{20, 0x8000}, Extent{21, 0x8000},
                                 Extent{22, 0x8000}};
  std::array<std::uint8_t, 3> texture_slots{0, 1, 2};
  std::array<Extent, 3> packages{Extent{30, 0x1000}, Extent{31, 0x2000},
                                 Extent{32, 0x3000}};
  std::array<Extent, 2> plugins{Extent{40, 0x800}, Extent{41, 0x800}};
  std::array<std::uint16_t, 4> entries{0, 2, 3, 3};
  Effects::EffectPurgeTables tables{.uses_textures = uses,
                                    .clut_files = cluts,
                                    .clut_slots = clut_slots,
                                    .texture_counts = counts,
                                    .texture_files = textures,
                                    .texture_slots = texture_slots,
                                    .package_files = packages,
                                    .plugin_files = plugins,
                                    .entry_point_indices = entries,
                                    .entry_point_count = 3};
  Effects::EffectResourceState state{};
  Effects::EffectRendererTable renderers{};
  FakeLifecycleHost host;

  // Configuration resets a finished texture load to idle.
  PurgeTableFixture() {
    state.texture_state = Effects::TextureLoadState::kLoaded;
  }
  PurgeTableFixture(const PurgeTableFixture&) = delete;
  PurgeTableFixture& operator=(const PurgeTableFixture&) = delete;
  PurgeTableFixture(PurgeTableFixture&&) = delete;
  PurgeTableFixture& operator=(PurgeTableFixture&&) = delete;
  ~PurgeTableFixture() = default;

  Effects::EffectPurgeConfiguration Configure(std::int32_t effect_id) {
    return Effects::ConfigureEffectResources(state, renderers, tables,
                                             effect_id, host);
  }

  // Effect 0, then effects 1 and 2 reconfigured, as the refusals follow.
  void ConfigureBeforeRefusedEffect() {
    static_cast<void>(Configure(0));
    static_cast<void>(Effects::ConfigureTextureLoads(state, tables, 1));
    Effects::EffectRendererTable untouched{};
    static_cast<void>(
        Effects::ConfigurePluginLoad(state, untouched, tables, 2));
  }
};
}  // namespace

TEST_CASE("Effect purge tables configure textures, plugins and the package",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  using Refusal = Effects::PurgeTableRefusal;
  const auto configuration = fixture.Configure(0);
  CHECK(configuration.textures == Refusal::kNone);
  CHECK(configuration.plugins == Refusal::kNone);
  CHECK(configuration.package == Refusal::kNone);
  CHECK(fixture.state.package_file == fixture.packages[0]);
  CHECK(fixture.host.pools == std::vector<std::int32_t>{255});
  CHECK(fixture.host.cleared == 1);
}

TEST_CASE("Effect purge tables list the CLUT, then the effect's FBTs",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  static_cast<void>(fixture.Configure(0));
  const auto& textures = fixture.state.textures;
  CHECK(fixture.state.texture_state == Effects::TextureLoadState::kIdle);
  CHECK(textures.asset_count == 3);
  CHECK(textures.assets[0] == Effects::TextureAsset{.file = fixture.cluts[0],
                                                    .slot = 0,
                                                    .is_clut = true});
  CHECK(textures.assets[1] ==
        Effects::TextureAsset{.file = fixture.textures[0], .slot = 0});
  CHECK(textures.assets[2] ==
        Effects::TextureAsset{.file = fixture.textures[1], .slot = 1});
}

TEST_CASE("Effect purge tables load the plugin file and its entries",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  static_cast<void>(fixture.Configure(0));
  const auto& state = fixture.state;
  CHECK(state.plugin_file == fixture.plugins[0]);
  CHECK(state.plugin_entry_count == 2);
  CHECK(state.plugin_state == Effects::PluginLoadState::kAcquireOverlay);
  CHECK(fixture.renderers[0] == static_cast<Effects::PluginEntry>(0));
  CHECK(fixture.renderers[1] == static_cast<Effects::PluginEntry>(1));
  CHECK_FALSE(fixture.renderers[2].has_value());
}

TEST_CASE("Effect purge tables drop the last FBT of an effect without textures",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  static_cast<void>(fixture.Configure(0));
  // An effect without textures loads its CLUT and drops its last FBT.
  CHECK(Effects::ConfigureTextureLoads(fixture.state, fixture.tables, 1) ==
        Effects::PurgeTableRefusal::kNone);
  CHECK(fixture.state.textures.asset_count == 1);
  CHECK(fixture.state.textures.assets[1] ==
        Effects::TextureAsset{.file = fixture.textures[2], .slot = 2});
}

TEST_CASE("Effect purge tables need no plugin file for an effect without one",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  static_cast<void>(fixture.Configure(0));
  // An effect without plugins needs no plugin file.
  Effects::EffectRendererTable untouched{};
  CHECK(Effects::ConfigurePluginLoad(fixture.state, untouched, fixture.tables,
                                     2) == Effects::PurgeTableRefusal::kNone);
  CHECK(fixture.state.plugin_entry_count == 0);
  CHECK(fixture.state.plugin_state == Effects::PluginLoadState::kIdle);
  CHECK(untouched == Effects::EffectRendererTable{});
}

TEST_CASE("Effect purge tables refuse an effect id past the tables",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  using Refusal = Effects::PurgeTableRefusal;
  fixture.ConfigureBeforeRefusedEffect();
  const auto configuration = fixture.Configure(3);
  CHECK(configuration.textures == Refusal::kEffectId);
  CHECK(configuration.plugins == Refusal::kEffectId);
  CHECK(configuration.package == Refusal::kEffectId);
  CHECK(fixture.state.textures.asset_count == 0);
  CHECK(fixture.state.package_file == Extent{});
  CHECK(fixture.state.plugin_state == Effects::PluginLoadState::kIdle);
}

TEST_CASE("Effect purge tables refuse plugin and texture ranges past the files",
          "[effects][effects-resources-transforms]") {
  PurgeTableFixture fixture;
  using Refusal = Effects::PurgeTableRefusal;
  fixture.ConfigureBeforeRefusedEffect();
  static_cast<void>(fixture.Configure(3));
  fixture.tables.entry_point_count = 2;
  CHECK(Effects::ConfigurePluginLoad(fixture.state, fixture.renderers,
                                     fixture.tables,
                                     1) == Refusal::kPluginRange);
  CHECK(fixture.state.plugin_file == Extent{});
  fixture.tables.texture_files = std::span(fixture.textures).first(2);
  CHECK(Effects::ConfigureTextureLoads(fixture.state, fixture.tables, 1) ==
        Refusal::kTextureRange);
  CHECK(fixture.state.textures.asset_count == 0);
}

TEST_CASE("Effect renderer and plugin tables refuse entries past their ends",
          "[effects][effects-resources-transforms]") {
  Effects::EffectRendererTable renderers{};
  const auto entry = static_cast<Effects::PluginEntry>(12);
  CHECK(Effects::SetEffectRenderer(renderers, entry, 7));
  CHECK(renderers[7] == entry);
  CHECK_FALSE(Effects::SetEffectRenderer(renderers, entry, 8));
  CHECK_FALSE(Effects::SetEffectRenderer(renderers, entry, -1));

  FakePluginHost host;
  host.present = {true, false, true, true};
  const auto sweep = Effects::InvokeAllPluginUpdates(host);
  CHECK(sweep.updated == 3);
  CHECK(sweep.result == 30);
  CHECK(host.calls == std::vector<std::size_t>{0, 2, 3});

  CHECK_FALSE(Effects::LiveSetEffectRenderer(
      static_cast<Effects::PluginEntry>(0xFFFF), 0));
  CHECK_FALSE(
      Effects::LiveSetEffectRenderer(static_cast<Effects::PluginEntry>(0), 8));
}

namespace {
// Curve 1 is {7, 8, 9}; segmented curve 0 is {step 4, 10}, {step 0, 30}.
std::vector<std::byte> LiveTestPackage() {
  return BuildPackage({
      {.type = 4, .payload = {1, 0, 5, 0, 3, 7, 8, 9, 0, 0, 0, 0}},
      {.type = 9, .payload = std::vector<std::uint8_t>(0x1C, 0)},
      {.type = 11, .payload = {1, 0, 0, 0, 6, 0, 1, 4, 10, 0, 30, 0}},
  });
}
}  // namespace

TEST_CASE("Live effect packages bind typed views beside the legacy pointers",
          "[effects][effects-resources-transforms]") {
  LivePackageScope scope;
  auto package = LiveTestPackage();
  auto walk = Effects::BindLiveEffectPackage(package);
  CHECK(Required(walk).status == Effects::PackageWalkStatus::kEnded);
  REQUIRE(Effects_actionPackage.curveBlock != nullptr);
  CHECK(Effects_actionPackage.curves[0][0] == 7);
  CHECK(Effects::LiveSampleEffectCurve(1, 4) == 8);
  CHECK(Effects::LiveSampleEffectCurve(2, 0) == 0);
  Effects::LiveSampleAllEffectCurves(2);
  CHECK(Effects_curveSamples[1] == 9);
}

TEST_CASE("Live effect packages bind segmented curves and block views",
          "[effects][effects-resources-transforms]") {
  LivePackageScope scope;
  auto package = LiveTestPackage();
  auto walk = Effects::BindLiveEffectPackage(package);
  static_cast<void>(Required(walk));
  CHECK(Effects::LiveSegmentedEffectCurveDuration(0) == 5);
  CHECK(Effects::LiveSampleSegmentedEffectCurve(0, 2) == 20);
  CHECK(Effects::LiveBoundPackageBlock(Effects::PackageBlockType::kParameters)
            .size() == 0x1C);
  CHECK(Effects::LiveBoundPackageBlock(Effects::PackageBlockType::kTimelines)
            .empty());
}

TEST_CASE("Live effect package views follow the pointers the walk bound",
          "[effects][effects-resources-transforms]") {
  LivePackageScope scope;
  auto package = LiveTestPackage();
  auto walk = Effects::BindLiveEffectPackage(package);
  static_cast<void>(Required(walk));

  // The pointer a typed reader trusts must still be the one the walk bound.
  std::array<u_long, 2> other_curves{1, 2};
  Effects_actionPackage.curveBlock =
      reinterpret_cast<Effects_CurveBlock*>(other_curves.data());
  CHECK(Effects::LiveSampleEffectCurve(1, 0) == 0);
  Effects_actionPackage.block9Data = nullptr;
  CHECK(Effects::LiveBoundPackageBlock(Effects::PackageBlockType::kParameters)
            .empty());

  // Only the package buffer and the empty package walk through the C entry.
  Effects::LiveBindEffectPackageBlocks(package.data());
  CHECK(Effects_actionPackage.block9Data == nullptr);

  Effects_package = nullptr;
  CHECK_FALSE(Effects::BindLiveEffectPackage(package).has_value());
}

TEST_CASE("Live effect resource contexts move between the package records",
          "[effects][effects-resources-transforms]") {
  LivePackageScope scope;
  Effects::LiveSelectEffectResourceContext(1);
  CHECK(Effects_package == &Effects_actionPackage);
  Effects::LiveSaveEffectResourceContext();
  CHECK(Effects_savedPackage == &Effects_actionPackage);
  Effects::LiveSelectEffectResourceContext(0);
  CHECK(Effects_package == &Effects_secondaryPackage);
  Effects::LiveRestoreEffectResourceContext();
  CHECK(Effects_package == &Effects_actionPackage);
}

TEST_CASE("Live effect camera and transforms refuse a missing instance",
          "[effects][effects-resources-transforms]") {
  const Effects::EffectCameraState saved = Effects::LoadLiveEffectCameraState();
  Effects::LiveFocusCameraOnEffectTransform(nullptr, 0, 4);
  static Effects_EffectInstance storage{};
  storage = {};
  Effects_EffectInstance* const instance = &storage;
  Effects::LiveFocusCameraOnEffectTransform(instance, 16, 4);
  CHECK(Effects::LoadLiveEffectCameraState() == saved);
  Effects::LiveCaptureEffectCamera(nullptr);
  Effects::LiveInitializeTransformStates(nullptr);
  Effects::LiveUpdateEffectTransforms(nullptr);

  instance->transforms[3].world.t[0] = 1;
  instance->transforms[3].world.t[1] = 2;
  instance->transforms[3].world.t[2] = 3;
  Effects::LiveFocusCameraOnEffectTransform(instance, 3, 0);
  const Effects::EffectCameraState focused =
      Effects::LoadLiveEffectCameraState();
  CHECK(focused.motion_ends[1].look_at ==
        Math::Vec3{.x = 1 << 12, .y = 2 << 12, .z = 3 << 12});
  CHECK(focused.motion_duration == 1);
  CHECK(focused.transition_mode == 2);
  Effects::StoreLiveEffectCameraState(saved);
  CHECK(Effects::LoadLiveEffectCameraState() == saved);
}

TEST_CASE("Promoted effect resources and transforms leave no module binding",
          "[effects][effects-resources-transforms]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // No module image stores these functions' addresses; callers use the
  // typed API through the Live entries. The four EFFPURGE.BIN functions
  // never had a module entry.
  constexpr std::array<std::uint32_t, 62> kRetired{
      0x800CFEF0u, 0x800D01E4u, 0x800D037Cu, 0x800D0548u, 0x800D05F4u,
      0x800D0984u, 0x800D0B08u, 0x800D0B30u, 0x800D0C60u, 0x800D0D08u,
      0x800D1104u, 0x800D118Cu, 0x800D11F4u, 0x800D12D8u, 0x800D1930u,
      0x800D1B18u, 0x800D1B80u, 0x800D1DE4u, 0x800D1DFCu, 0x800D1E20u,
      0x800D1E2Cu, 0x800D1E78u, 0x800D1EF0u, 0x800D1FECu, 0x800D206Cu,
      0x800D21C0u, 0x800D236Cu, 0x800D2518u, 0x800D2560u, 0x800D66FCu,
      0x800D67C4u, 0x800D7814u, 0x800D7890u, 0x800D78B8u, 0x800D78CCu,
      0x800D78E0u, 0x800D78F0u, 0x800D7980u, 0x800D7990u, 0x800D79E4u,
      0x800D79F4u, 0x800D7A04u, 0x800D7A74u, 0x800D7A90u, 0x800D7AB4u,
      0x800D7AC4u, 0x800D7AECu, 0x800D7B24u, 0x800D7BA4u, 0x800D7BF8u,
      0x800D7C5Cu, 0x800D7CFCu, 0x800D7EF4u, 0x800D7FB4u, 0x800D7FC8u,
      0x800D7FE4u, 0x800D7FFCu, 0x800D8008u, 0x800D8038u, 0x800D8044u,
      0x800D8054u, 0x800D8060u};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
