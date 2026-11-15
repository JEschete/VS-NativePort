#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Effects/Camera.hpp"
#include "Effects/Plugins/Plg128Camera.hpp"

namespace {
namespace p = VS::Effects::Plg128;
using VS::Core::Math::Mat3;
using VS::Core::Math::Position3;
using VS::Core::Math::ScalarSpan;
using VS::Core::Math::Vec3;
using VS::Effects::EffectCameraPose;

constexpr std::int32_t kOne = 4096;
constexpr std::int32_t kQuarterTurn = 1024;
constexpr std::int32_t kIntMax = std::numeric_limits<std::int32_t>::max();
constexpr std::int32_t kIntMin = std::numeric_limits<std::int32_t>::min();
constexpr std::uint8_t kTransformCount = 16;

constexpr Mat3 kIdentity{{{{kOne, 0, 0}, {0, kOne, 0}, {0, 0, kOne}}}};

[[nodiscard]] p::VectorRange Constant(std::int32_t x, std::int32_t y,
                                      std::int32_t z) {
  return {ScalarSpan{x, x}, ScalarSpan{y, y}, ScalarSpan{z, z}};
}

struct LookAtCall {
  Vec3 from;
  Vec3 to;

  friend bool operator==(const LookAtCall&, const LookAtCall&) = default;
};

struct CurveCall {
  std::uint8_t curve;
  std::int32_t age;

  friend bool operator==(const CurveCall&, const CurveCall&) = default;
};

class FakeHost final : public p::CameraEffectHost {
 public:
  std::optional<p::CameraDefinition> definition;
  Position3 origin{};
  Vec3 translations[kTransformCount]{};
  std::int16_t facings[kTransformCount]{};
  Mat3 look_at = kIdentity;
  EffectCameraPose captured{};
  std::vector<std::uint8_t> definition_queries;
  std::vector<CurveCall> curves;
  std::vector<LookAtCall> look_ats;
  std::vector<std::int16_t> motions;
  std::vector<std::int32_t> transitions;
  std::vector<std::pair<EffectCameraPose, EffectCameraPose>> published;
  int captures = 0;

  std::optional<p::CameraDefinition> Definition(std::uint8_t index) override {
    definition_queries.push_back(index);
    return definition;
  }
  std::int32_t SampleCurve(std::uint8_t curve, std::int32_t age) override {
    curves.push_back({curve, age});
    return curve;
  }
  Position3 EffectOrigin() override { return origin; }
  std::optional<Vec3> TransformTranslation(std::uint8_t index) override {
    if (index >= kTransformCount) {
      return std::nullopt;
    }
    return translations[index];
  }
  std::optional<std::int16_t> TransformFacing(std::uint8_t index) override {
    if (index >= kTransformCount) {
      return std::nullopt;
    }
    return facings[index];
  }
  Mat3 LookAt(Vec3 from, Vec3 to) override {
    look_ats.push_back({from, to});
    return look_at;
  }
  void CaptureCamera(EffectCameraPose& pose) override {
    ++captures;
    pose = captured;
  }
  void BeginCameraMotion(std::int16_t duration) override {
    motions.push_back(duration);
  }
  void SetTransitionMode(std::int32_t mode) override {
    transitions.push_back(mode);
  }
  void PublishCameraMotionEnds(const EffectCameraPose& start,
                               const EffectCameraPose& end) override {
    published.emplace_back(start, end);
  }
};

// Curves 1-6, constant ranges: position (100, 200, 300), extent (10, 20, 30),
// direction (1024, 0, 0), speed (50, 60), attraction offset (7, 8, 9), no
// jitter and an inner extent of 33.
[[nodiscard]] p::CameraDefinition SampleDefinition(std::uint32_t flags) {
  return {.flags = flags,
          .position_curve = 1,
          .direction_curve = 2,
          .speed_curve = 3,
          .attraction_offset_curve = 4,
          .attraction_jitter_curve = 5,
          .inner_extent_curve = 6,
          .position_range = Constant(100, 200, 300),
          .extent_range = Constant(10, 20, 30),
          .direction_range = Constant(kQuarterTurn, 0, 0),
          .speed_range = {ScalarSpan{50, 50}, ScalarSpan{60, 60}},
          .attraction_offset_range = Constant(7, 8, 9),
          .attraction_jitter_range = Constant(0, 0, 0),
          .inner_extent_range = ScalarSpan{33, 33},
          .transform_index = 2,
          .attraction_transform = 3};
}

[[nodiscard]] p::CameraEffectState TickState(std::int16_t mode) {
  p::CameraEffectState state;
  state.lifetime = 5;
  state.definition = 9;
  state.mode = mode;
  state.age = 4;
  state.start_camera.roll = 11;
  state.end_camera.roll = 12;
  return state;
}

std::int32_t Tick(FakeHost& host, p::CameraEffectState& state,
                  p::CameraScratch& scratch) {
  return p::UpdateEffectCamera(host, state, scratch,
                               std::to_underlying(p::CameraCommand::kTick), 0);
}

// ---- F02064 BuildCameraOffsetTransform -------------------------------------

TEST_CASE("F02064 looks from the target towards the position",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.look_at.m[0][1] = 77;
  p::CameraEffectState state;
  state.position = {1, 2, 3};
  state.target = {-4, -5, -6};
  p::BuildCameraOffsetTransform(host, state, {7, 8, 9});
  CHECK(host.look_ats == std::vector<LookAtCall>{{{-4, -5, -6}, {1, 2, 3}}});
  CHECK(state.view.rotation == host.look_at);
  CHECK(state.view.translation == Vec3{7, 8, 9});
}

TEST_CASE("F02064 replaces only the view", "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraEffectState state = TickState(0);
  state.near_clip = 44;
  state.view.translation = {kIntMin, 0, kIntMax};
  p::CameraEffectState expected = state;
  p::BuildCameraOffsetTransform(host, state, {kIntMax, -1, kIntMin});
  expected.view = {kIdentity, {kIntMax, -1, kIntMin}};
  CHECK(state == expected);
}

// ---- F02065 ResolveCameraEndpoints -----------------------------------------

TEST_CASE("F02065 moves the position by the effect origin in words",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kPositionFromOrigin;
  scratch.position = {kIntMax, 10, -10};
  scratch.attraction_offset = {-1, 2, -3};
  p::ResolveCameraEndpoints(state, scratch, {.effect_origin = {1, -32768, 5}});
  CHECK(scratch.position == Vec3{kIntMin, -32758, -5});
  CHECK(state.position == scratch.position);
  CHECK(state.target == Vec3{-1, 2, -3});
}

TEST_CASE("F02065 moves the position by the transform translation",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kPositionFromTransform | 0x38;
  scratch.position = {kIntMin, 1, 2};
  p::ResolveCameraEndpoints(
      state, scratch,
      {.effect_origin = {50, 50, 50}, .transform_translation = {-1, 70000, 3}});
  CHECK(state.position == Vec3{kIntMax, 70001, 5});
}

TEST_CASE("F02065 ignores the other position sources",
          "[effects][plg128][camera]") {
  for (std::uint32_t source : {0U, 3U, 4U, 7U}) {
    p::CameraEffectState state;
    p::CameraScratch scratch;
    scratch.flags = source;
    scratch.position = {4, 5, 6};
    p::ResolveCameraEndpoints(
        state, scratch,
        {.effect_origin = {1, 1, 1}, .transform_translation = {9, 9, 9}});
    CHECK(state.position == Vec3{4, 5, 6});
  }
}

TEST_CASE("F02065 adds the attraction transform in halfwords",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kTargetFromTransform;
  scratch.attraction_offset = {32767, -32768, 100};
  p::ResolveCameraEndpoints(state, scratch,
                            {.attraction_translation = {1, -1, 0x10005}});
  CHECK(scratch.attraction_offset == Position3{-32768, 32767, 105});
  CHECK(state.target == Vec3{-32768, 32767, 105});
}

TEST_CASE("F02065 adds the effect origin to the target in halfwords",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kTargetFromOrigin;
  scratch.attraction_offset = {-32768, 0, 1};
  p::ResolveCameraEndpoints(
      state, scratch,
      {.effect_origin = {-1, -2, 3}, .attraction_translation = {9, 9, 9}});
  CHECK(state.target == Vec3{32767, -2, 4});
}

TEST_CASE("F02065 keeps the offset for the other target sources",
          "[effects][plg128][camera]") {
  for (std::uint32_t source : {0x0U, 0x40U, 0x100U, 0x140U, 0x1C0U}) {
    p::CameraEffectState state;
    p::CameraScratch scratch;
    scratch.flags = source;
    scratch.attraction_offset = {-7, 8, -9};
    p::ResolveCameraEndpoints(
        state, scratch,
        {.effect_origin = {1, 1, 1}, .attraction_translation = {2, 2, 2}});
    CHECK(state.target == Vec3{-7, 8, -9});
  }
}

// ---- F02066 ResolveActorFacingCamera ---------------------------------------

TEST_CASE("F02066 stands the jitter behind the source along facing zero",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.position = {10, 20, 30};
  scratch.attraction_jitter = {100, 7, 7};
  p::ResolveActorFacingCamera(state, scratch, {});
  CHECK(state.target == Vec3{10, 20, 30});
  CHECK(state.position == Vec3{10, 20, -70});
}

TEST_CASE("F02066 turns the offset with the transform facing",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.position = {10, 20, 30};
  scratch.attraction_jitter = {-100, 0, 0};
  p::ResolveActorFacingCamera(state, scratch, {.facing = kQuarterTurn});
  CHECK(state.position == Vec3{110, 20, 30});
  p::ResolveActorFacingCamera(state, scratch, {.facing = 2 * kQuarterTurn});
  CHECK(state.position == Vec3{10, 20, -70});
}

TEST_CASE("F02066 resolves the position source before facing",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kPositionFromTransform;
  scratch.position = {kIntMax, 0, 0};
  scratch.attraction_jitter = {-1, 0, 0};
  p::ResolveActorFacingCamera(
      state, scratch,
      {.transform_translation = {1, 2, 3}, .facing = kQuarterTurn});
  CHECK(state.target == Vec3{kIntMin, 2, 3});
  CHECK(state.position == Vec3{kIntMin + 1, 2, 3});
}

TEST_CASE("F02066 steps a positive jitter back against the facing",
          "[effects][plg128][camera]") {
  p::CameraEffectState state;
  p::CameraScratch scratch;
  scratch.flags = p::kPositionFromOrigin;
  scratch.attraction_jitter = {1, 0, 0};
  p::ResolveActorFacingCamera(
      state, scratch, {.effect_origin = {5, 6, 7}, .facing = kQuarterTurn});
  CHECK(state.target == Vec3{5, 6, 7});
  CHECK(state.position == Vec3{4, 6, 7});
}

// ---- F02067 StoreCameraEndpoints -------------------------------------------

TEST_CASE("F02067 stores the endpoints with 12 fractional bits",
          "[effects][plg128][camera]") {
  p::CameraEffectState state = TickState(0);
  state.position = {1, -1, 0x80000};
  state.target = {-0x80000, 0x7FFFF, 3};
  p::CameraEffectState expected = state;
  p::StoreCameraEndpoints(state);
  expected.end_camera.position = {kOne, -kOne, kIntMin};
  expected.end_camera.look_at = {kIntMin, kIntMax - (kOne - 1), 3 * kOne};
  CHECK(state == expected);
}

// ---- F02068 UpdateEffectCamera ---------------------------------------------

TEST_CASE("F02068 creation unpacks the request and captures both ends",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.captured.roll = 99;
  p::CameraEffectState state;
  state.age = 40;
  p::CameraScratch scratch;
  CHECK(p::UpdateEffectCamera(host, state, scratch, 1, 0x00010305) == 1);
  CHECK(state.definition == 3);
  CHECK(state.lifetime == 5);
  CHECK(state.mode == 1);
  CHECK(state.age == 0);
  CHECK(host.captures == 2);
  CHECK(state.start_camera == host.captured);
  CHECK(state.end_camera == host.captured);
  CHECK(host.motions == std::vector<std::int16_t>{5});
}

TEST_CASE("F02068 selects the cosine transition only for endpoint modes",
          "[effects][plg128][camera]") {
  for (std::uint32_t mode : {0U, 1U}) {
    FakeHost host;
    p::CameraEffectState state;
    p::CameraScratch scratch;
    CHECK(p::UpdateEffectCamera(host, state, scratch, 1,
                                static_cast<std::int32_t>(mode << 16)) == 1);
    CHECK(host.transitions == std::vector<std::int32_t>{2});
  }
  for (std::uint32_t mode : {2U, 0xFFFFU, 0x8000U}) {
    FakeHost host;
    p::CameraEffectState state;
    p::CameraScratch scratch;
    CHECK(p::UpdateEffectCamera(host, state, scratch, 1,
                                static_cast<std::int32_t>(mode << 16)) == 1);
    CHECK(host.transitions.empty());
  }
}

TEST_CASE("F02068 creation keeps the sign of the packed mode",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraEffectState state;
  p::CameraScratch scratch;
  CHECK(p::UpdateEffectCamera(host, state, scratch, 1,
                              static_cast<std::int32_t>(0xFFFEFF00U)) == 1);
  CHECK(state.mode == -2);
  CHECK(state.definition == 0xFF);
  CHECK(state.lifetime == 0);
  CHECK(host.motions == std::vector<std::int16_t>{0});
}

TEST_CASE("F02068 release, stop and unknown commands",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  const p::CameraEffectState before = state;
  CHECK(p::UpdateEffectCamera(host, state, scratch, 4, 0) == 0);
  CHECK(p::UpdateEffectCamera(host, state, scratch, 0, 0) == 1);
  CHECK(p::UpdateEffectCamera(host, state, scratch, 5, 0) == 1);
  CHECK(state == before);
  CHECK(p::UpdateEffectCamera(host, state, scratch, 3, 0) == 1);
  CHECK(state.lifetime == 1);
  CHECK(host.definition_queries.empty());
}

TEST_CASE("F02068 tick samples the definition curves in order",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(0xFC000000U);
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(host.definition_queries == std::vector<std::uint8_t>{9});
  CHECK(scratch.flags == 0);
  CHECK(host.curves ==
        std::vector<CurveCall>{
            {1, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}});
  CHECK(scratch.speed == std::array<std::int32_t, 2>{50, 60});
  CHECK(state.near_clip == 33);
}

TEST_CASE("F02068 tick orbits a sphere and publishes the end pose",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(0);
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(host.look_ats == std::vector<LookAtCall>{{{7, 8, 9}, {100, 200, 300}}});
  // sin(1024) is positive, so the elevation sine is negated.
  CHECK(scratch.sin_angle0 == -kOne);
  CHECK(scratch.vector == Position3{0, 0, -30});
  CHECK(state.position == Vec3{100, 200, 270});
  CHECK(state.end_camera.position == Vec3{100 * kOne, 200 * kOne, 270 * kOne});
  CHECK(state.end_camera.look_at == Vec3{7 * kOne, 8 * kOne, 9 * kOne});
  CHECK(host.published ==
        std::vector<std::pair<EffectCameraPose, EffectCameraPose>>{
            {state.start_camera, state.end_camera}});
}

TEST_CASE("F02068 tick sets the end clips and advances the age",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(0);
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(state.end_camera.near_clip == 33);
  CHECK(state.end_camera.far_clip == 50);
  CHECK(state.end_camera.roll == 12);
  CHECK(state.age == 5);
  CHECK(state.lifetime == 4);
}

TEST_CASE("F02068 tick keeps the disc sine and the extent depth",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraDefinition definition = SampleDefinition(p::kOffsetShapeDisc);
  definition.extent_range = Constant(10, 20, 0x12345);
  host.definition = definition;
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  scratch.sin_angle1 = 77;
  scratch.cos_angle1 = 88;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(scratch.sin_angle0 == kOne);
  CHECK(scratch.cos_angle0 == 0);
  CHECK(scratch.vector == Position3{0, 20, 0x2345});
  CHECK(scratch.sin_angle1 == 77);
  CHECK(scratch.cos_angle1 == 88);
  CHECK(state.position == Vec3{100, 220, 300 + 0x2345});
}

TEST_CASE("F02068 tick keeps the last view for other offset shapes",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(0x08);
  p::CameraEffectState state = TickState(0);
  state.view = {kIdentity, {1000, 2000, 3000}};
  p::CameraScratch scratch;
  scratch.vector = {1, 2, 3};
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(host.look_ats.empty());
  CHECK(scratch.vector == Position3{1, 2, 3});
  CHECK(state.position == Vec3{1001, 2002, 3003});
  CHECK(state.end_camera.position ==
        Vec3{1001 * kOne, 2002 * kOne, 3003 * kOne});
}

TEST_CASE("F02068 tick in actor-facing mode aims at the source",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraDefinition definition =
      SampleDefinition(p::kPositionFromTransform | p::kOffsetShapeDisc);
  definition.direction_range = Constant(0, 0, 0);
  definition.extent_range = Constant(0, 0, 0);
  definition.attraction_jitter_range = Constant(10, 0, 0);
  host.definition = definition;
  host.translations[2] = {1, 2, 3};
  host.facings[2] = kQuarterTurn;
  p::CameraEffectState state = TickState(1);
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(state.target == Vec3{101, 202, 303});
  CHECK(host.look_ats ==
        std::vector<LookAtCall>{{{101, 202, 303}, {91, 202, 303}}});
  CHECK(state.position == Vec3{91, 202, 303});
  CHECK(state.end_camera.look_at == Vec3{101 * kOne, 202 * kOne, 303 * kOne});
}

TEST_CASE("F02068 tick in other modes keeps the endpoints",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(p::kOffsetShapeDisc);
  p::CameraEffectState state = TickState(2);
  state.position = {1, 2, 3};
  state.target = {4, 5, 6};
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(state.target == Vec3{4, 5, 6});
  CHECK(host.look_ats == std::vector<LookAtCall>{{{4, 5, 6}, {1, 2, 3}}});
  CHECK(state.position == Vec3{1, 22, 3 + 30});
  CHECK(state.end_camera.position == Vec3{});
  CHECK(state.end_camera.far_clip == 50);
}

TEST_CASE("F02068 tick lifetime one ends and zero wraps",
          "[effects][plg128][camera]") {
  FakeHost host;
  host.definition = SampleDefinition(0);
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  state.lifetime = 1;
  CHECK(Tick(host, state, scratch) == 0);
  CHECK(state.lifetime == 0);
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(state.lifetime == 255);
  state.age = kIntMax;
  CHECK(Tick(host, state, scratch) == 1);
  CHECK(state.age == kIntMin);
}

TEST_CASE("F02068 tick refuses a missing definition without writing",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  scratch.flags = 0x1234;
  const p::CameraEffectState before = state;
  const p::CameraScratch scratch_before = scratch;
  CHECK(Tick(host, state, scratch) == 0);
  CHECK(state == before);
  CHECK(scratch == scratch_before);
  CHECK(host.curves.empty());
  CHECK(host.published.empty());
}

TEST_CASE("F02068 tick refuses out-of-range transforms without writing",
          "[effects][plg128][camera]") {
  struct Case {
    std::uint32_t flags;
    std::int16_t mode;
    std::uint8_t transform;
    std::uint8_t attraction;
  };
  for (const Case& item :
       {Case{p::kPositionFromTransform, 0, kTransformCount, 0},
        Case{p::kTargetFromTransform, 0, 0, 0xFF},
        Case{p::kPositionFromOrigin, 1, kTransformCount, 0}}) {
    FakeHost host;
    p::CameraDefinition definition = SampleDefinition(item.flags);
    definition.transform_index = item.transform;
    definition.attraction_transform = item.attraction;
    host.definition = definition;
    p::CameraEffectState state = TickState(item.mode);
    p::CameraScratch scratch;
    const p::CameraEffectState before = state;
    CHECK(Tick(host, state, scratch) == 0);
    CHECK(state == before);
    CHECK(scratch == p::CameraScratch{});
    CHECK(host.published.empty());
  }
}

TEST_CASE("F02068 tick reads no unselected transform",
          "[effects][plg128][camera]") {
  FakeHost host;
  p::CameraDefinition definition = SampleDefinition(p::kTargetFromTransform);
  definition.transform_index = 0xFF;
  host.definition = definition;
  p::CameraEffectState state = TickState(0);
  p::CameraScratch scratch;
  CHECK(Tick(host, state, scratch) == 1);
  definition = SampleDefinition(p::kTargetFromTransform);
  definition.attraction_transform = 0xFF;
  host.definition = definition;
  state.mode = 2;
  CHECK(Tick(host, state, scratch) == 1);
}
}  // namespace
