#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/TrailEmitter.hpp"

namespace {
using VS::Effects::TrailEmitterCommand;
using VS::Effects::TrailEmitterCounters;
using VS::Effects::TrailEmitterPlan;
using VS::Effects::TrailOriginRequest;
using VS::Effects::TrailOriginScratch;
using VS::Effects::TrailPoint;
using VS::Effects::TrailVector;

constexpr std::int32_t kOne = 4096;

class FakeServices final : public VS::Effects::TrailOriginServices {
 public:
  std::int32_t rand_value = 0;
  std::int32_t sin_value = 0;
  std::int32_t cos_value = kOne;
  std::vector<std::int32_t> angles;
  std::vector<std::pair<std::int32_t, std::int32_t>> ranges;
  int rotations = 0;

  std::int32_t Rand() override { return rand_value; }
  std::int32_t RandomRangeExclusive(std::int32_t a, std::int32_t b) override {
    ranges.emplace_back(a, b);
    return a / 2;
  }
  std::int32_t Sin(std::int32_t angle) override {
    angles.push_back(angle);
    return sin_value;
  }
  std::int32_t Cos(std::int32_t angle) override {
    angles.push_back(angle);
    return cos_value;
  }
  TrailVector Rotate(TrailPoint point) override {
    ++rotations;
    return {point.y, point.z, point.x};
  }
};

TrailOriginRequest BoxCorners(std::uint32_t anchor) {
  return {.flags = VS::Effects::kTrailShapeBox |
                   VS::Effects::kTrailPlacementCorners | anchor,
          .position = {1, 2, 3},
          .extent = {10, 20, 30},
          .effect_origin = {100, 200, 300},
          .caster_translation = {-7, -8, -9}};
}
}  // namespace

TEST_CASE("Trail emitter plans packed lifetimes, definitions and counts",
          "[effects][trail-emitter]") {
  const TrailEmitterPlan plan = VS::Effects::PlanTrailEmitter(0x1234);
  CHECK(plan.lifetime == 0x34);
  CHECK(plan.definition_index == 0x12);
  CHECK(VS::Effects::TrailCountFor(0) == std::optional<std::uint8_t>{0});
  CHECK(VS::Effects::TrailCountFor(9) == std::optional<std::uint8_t>{9});
  CHECK(VS::Effects::TrailCountFor(20) == std::optional<std::uint8_t>{16});
  CHECK_FALSE(VS::Effects::TrailCountFor(-1).has_value());
}

TEST_CASE("Trail transparency samples five points and bounds one-entry curves",
          "[effects][trail-emitter]") {
  const std::array<std::uint8_t, 5> five{10, 20, 30, 40, 50};
  auto sampled = VS::Effects::SampleTrailTransparency(five);
  CHECK(sampled.samples == five);
  CHECK_FALSE(sampled.clamped);

  const std::array<std::uint8_t, 2> two{1, 2};
  sampled = VS::Effects::SampleTrailTransparency(two);
  CHECK(sampled.samples == VS::Effects::TrailTransparency{1, 1, 1, 1, 2});
  CHECK_FALSE(sampled.clamped);

  const std::array<std::uint8_t, 1> one{7};
  sampled = VS::Effects::SampleTrailTransparency(one);
  CHECK(sampled.samples == VS::Effects::TrailTransparency{7, 7, 7, 7, 7});
  CHECK(sampled.clamped);

  sampled = VS::Effects::SampleTrailTransparency({});
  CHECK(sampled.samples ==
        VS::Effects::TrailTransparency{0x80, 0x80, 0x80, 0x80, 0x80});
}

TEST_CASE("Trail event positions are Q12 with 32-bit wrapping",
          "[effects][trail-emitter]") {
  const TrailVector position =
      VS::Effects::TrailEventPosition({1, -2, 0x80000});
  CHECK(position.x == kOne);
  CHECK(position.y == -2 * kOne);
  CHECK(position.z == std::numeric_limits<std::int32_t>::min());
}

TEST_CASE("Trail origins cycle box corners in each anchor space",
          "[effects][trail-emitter]") {
  const std::array<std::pair<std::uint32_t, TrailVector>, 3> anchors{
      {{0, {1, 2, 3}},
       {VS::Effects::kTrailAnchorEffectOrigin, {101, 202, 303}},
       {VS::Effects::kTrailAnchorCaster, {-6, -6, -6}}}};
  for (const auto& [anchor, position] : anchors) {
    FakeServices services;
    TrailOriginScratch scratch;
    std::array<TrailPoint, 3> points{};
    std::int32_t corner = 6;
    VS::Effects::GenerateTrailOrigins(BoxCorners(anchor), points, corner,
                                      scratch, services);
    CHECK(scratch.position == position);
    CHECK(scratch.extent == TrailVector{10, 20, 30});
    CHECK(corner == 1);
    const auto at = [&](int x, int y, int z) {
      return TrailPoint{static_cast<std::int16_t>(position.x + x),
                        static_cast<std::int16_t>(position.y + y),
                        static_cast<std::int16_t>(position.z + z)};
    };
    CHECK(points[0] == at(10, 20, 30));
    CHECK(points[1] == at(-10, -20, -30));
    CHECK(points[2] == at(10, -20, -30));
  }
}

TEST_CASE("Trail origins keep, rotate and wrap box points",
          "[effects][trail-emitter]") {
  FakeServices services;
  TrailOriginScratch scratch;
  std::int32_t corner = 0;
  std::array<TrailPoint, 1> points{{{5, 6, 7}}};
  TrailOriginRequest request = BoxCorners(0);
  request.flags = VS::Effects::kTrailShapeBox |
                  VS::Effects::kTrailPlacementNone |
                  VS::Effects::kTrailRotateOrigins;
  request.position = {0x7FFF, 0, 0};
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(services.rotations == 1);
  CHECK(scratch.aim_point == TrailVector{6, 7, 5});
  CHECK(points[0] ==
        TrailPoint{std::numeric_limits<std::int16_t>::min() + 5, 7, 5});
  CHECK(corner == 0);

  request.flags = VS::Effects::kTrailShapeBox;
  request.position = {};
  services.rand_value = 3;  // the negative Y face
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(points[0] == TrailPoint{5, -20, 15});
  CHECK(services.ranges == std::vector<std::pair<std::int32_t, std::int32_t>>{
                               {10, -10}, {30, -30}});
}

TEST_CASE("Trail origins sample spheres", "[effects][trail-emitter]") {
  FakeServices services;
  TrailOriginScratch scratch;
  std::int32_t corner = 0;
  std::array<TrailPoint, 2> points{};
  const TrailOriginRequest request{.flags = VS::Effects::kTrailShapeSphere,
                                   .extent = {100, 200, 300}};
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(points[0] == TrailPoint{100, 0, 0});
  CHECK(scratch.cos_angle0 == kOne);
  CHECK(scratch.sin_angle1 == 0);
}

TEST_CASE("Trail origins step exactly around ring corners",
          "[effects][trail-emitter]") {
  FakeServices services;
  TrailOriginScratch scratch;
  std::int32_t corner = 3;
  std::array<TrailPoint, 2> points{};
  TrailOriginRequest request{.flags = VS::Effects::kTrailShapeRing |
                                      VS::Effects::kTrailPlacementCorners,
                             .extent = {100, 200, 300}};
  request.ring_segments = 4;
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(scratch.ring_segments == 4);
  CHECK(scratch.ring_positions == 4);
  CHECK(corner == 1);
  CHECK(points[0] == TrailPoint{100, 0, 0});
  CHECK(services.angles.front() == kOne / 8);
}

TEST_CASE("Trail origins blend between ring positions",
          "[effects][trail-emitter]") {
  FakeServices services;
  TrailOriginScratch scratch;
  std::int32_t corner = 5;
  std::array<TrailPoint, 2> points{};
  TrailOriginRequest request{.flags = VS::Effects::kTrailShapeRing,
                             .extent = {100, 200, 300}};
  request.ring_segments = 4;
  request.ring_positions = 2;
  services.rand_value = kOne * 4;  // half way after the >> 3
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(corner == 1);
  CHECK(points[1] == TrailPoint{100, 0, 0});
}

TEST_CASE("Trail origins leave points alone for shapes without an origin",
          "[effects][trail-emitter]") {
  FakeServices services;
  TrailOriginScratch scratch;
  scratch.ring_segments = 7;
  std::int32_t corner = 0;
  std::array<TrailPoint, 2> points{TrailPoint{1, 2, 3}};
  const TrailOriginRequest request{.flags = VS::Effects::kTrailShapeMask,
                                   .extent = {100, 200, 300}};
  VS::Effects::GenerateTrailOrigins(request, points, corner, scratch, services);
  CHECK(points[0] == TrailPoint{1, 2, 3});
  CHECK(scratch.ring_segments == 7);
}

namespace {
class FakeHost final : public VS::Effects::TrailEmitterHost {
 public:
  bool has_state = false;
  bool accept_tick = true;
  TrailEmitterCounters counters;
  std::optional<TrailEmitterPlan> plan;
  std::vector<std::string> calls;

  void Create(TrailEmitterPlan value) override {
    calls.emplace_back("create");
    plan = value;
    has_state = true;
    counters = {.lifetime = value.lifetime};
  }
  bool HasState() const override { return has_state; }
  TrailEmitterCounters Counters() const override { return counters; }
  void StoreCounters(const TrailEmitterCounters& value) override {
    counters = value;
  }
  bool BeginTick() override {
    calls.emplace_back("begin");
    return accept_tick;
  }
  void InitializeTrails() override { calls.emplace_back("initialize"); }
  void DrawTrails() override { calls.emplace_back("draw"); }
  void ReleaseTrails() override { calls.emplace_back("release"); }
};

bool Run(FakeHost& host, TrailEmitterCommand command, std::int32_t packed = 0) {
  return VS::Effects::UpdateTrailEmitter(command, packed, host);
}
}  // namespace

TEST_CASE("Trail emitter creation plans the packed definition",
          "[effects][trail-emitter]") {
  FakeHost host;
  CHECK(Run(host, TrailEmitterCommand::kCreate, 0x0302));
  CHECK(host.plan.value_or(TrailEmitterPlan{}).definition_index == 3);
  CHECK(host.has_state);
  CHECK(host.calls == std::vector<std::string>{"create"});
}

TEST_CASE("Trail emitter lifetimes initialize once and release at expiry",
          "[effects][trail-emitter]") {
  FakeHost host;
  CHECK(Run(host, TrailEmitterCommand::kCreate, 0x0302));
  CHECK(Run(host, TrailEmitterCommand::kTick));
  CHECK(host.counters.initialized);
  CHECK(host.counters.curve_sample_distance == 1);
  CHECK(host.counters.lifetime == 1);
  CHECK_FALSE(Run(host, TrailEmitterCommand::kTick));
  CHECK(host.counters.curve_sample_distance == 2);
  CHECK(host.calls == std::vector<std::string>{"create", "begin", "initialize",
                                               "draw", "begin", "draw",
                                               "release"});
}

TEST_CASE("Trail emitter commands without state do nothing",
          "[effects][trail-emitter]") {
  FakeHost host;
  CHECK(Run(host, TrailEmitterCommand::kTick) == false);
  CHECK(Run(host, TrailEmitterCommand::kExpire));
  CHECK(Run(host, TrailEmitterCommand::kStop) == false);
  CHECK(host.calls.empty());
}

TEST_CASE("Trail emitter expiry releases on the next tick",
          "[effects][trail-emitter]") {
  FakeHost host;
  CHECK(Run(host, TrailEmitterCommand::kCreate, 0));
  for (int tick = 0; tick < 3; ++tick) {
    CHECK(Run(host, TrailEmitterCommand::kTick));
  }
  CHECK(host.counters.lifetime == 0);
  CHECK(Run(host, TrailEmitterCommand::kExpire));
  CHECK(host.counters.lifetime == 1);
  CHECK_FALSE(Run(host, TrailEmitterCommand::kTick));
  CHECK(host.calls.back() == "release");
}

TEST_CASE("Trail emitter refused ticks and stops detach the slot",
          "[effects][trail-emitter]") {
  FakeHost host;
  CHECK(Run(host, TrailEmitterCommand::kCreate, 0));
  host.calls.clear();
  host.accept_tick = false;
  CHECK_FALSE(Run(host, TrailEmitterCommand::kTick));
  CHECK(host.calls == std::vector<std::string>{"begin", "release"});
  host.calls.clear();
  CHECK_FALSE(Run(host, TrailEmitterCommand::kStop));
  CHECK(host.calls == std::vector<std::string>{"release"});
  // Renderer slots also receive commands the emitter does not handle.
  constexpr auto kUnhandled = std::uint32_t{9};
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(Run(host, static_cast<TrailEmitterCommand>(kUnhandled)));
}
