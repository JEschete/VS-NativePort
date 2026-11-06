#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Log.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailCore.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/Templates/TrailsLive.hpp"
#include "Effects/TrailEmitter.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Trails.h"
}

namespace {
namespace trails = VS::Effects::Templates;
using trails::RingColor;
using trails::RingProjection;
using trails::RingQuad;
using trails::RingTexture;
using trails::RingUv;
using trails::TrailFault;
using trails::TrailRecord;
using trails::TrailScratch;
using VS::Core::Math::ScalarSpan;
using VS::Core::Math::Vec3;
using VS::Effects::TrailPoint;
using VS::Effects::TrailTransparency;

constexpr std::int32_t kTravel = 0x03000000;

struct Submission {
  RingQuad quad;
  std::int32_t depth{};
};

struct Skip {
  std::size_t index{};
  TrailFault fault{};
};

class FakeTrailHost final : public trails::TrailHost {
 public:
  std::array<std::int32_t, 256> curve_values{};
  std::int32_t rand_value = 0;
  std::size_t rand_calls = 0;
  std::vector<std::pair<std::int32_t, std::int32_t>> ranges;
  Vec3 request{};
  std::optional<Vec3> attraction;
  bool refuse_projection = false;
  std::optional<bool> effect_view;
  std::vector<TrailPoint> projected;
  std::size_t texture_advances = 0;
  std::optional<RingTexture> texture = RingTexture{
      .clut = 0x1234, .tpage = 0x567, .u = 16, .v = 32, .w = 8, .h = 8};
  TrailTransparency transparency{10, 20, 30, 40, 50};
  std::int32_t near_clip = 0;
  std::vector<Submission> submissions;
  std::vector<Skip> skipped;

  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t /*distance*/) override {
    return curve_values.at(curve);
  }
  [[nodiscard]] std::int32_t Rand() override {
    ++rand_calls;
    return rand_value;
  }
  // Answers the first bound and records both.
  [[nodiscard]] std::int32_t RandomRangeExclusive(std::int32_t a,
                                                  std::int32_t b) override {
    ranges.emplace_back(a, b);
    return a;
  }
  [[nodiscard]] Vec3 RequestPosition() override { return request; }
  [[nodiscard]] std::optional<Vec3> AttractionTranslation() override {
    return attraction;
  }
  [[nodiscard]] bool BeginProjection(bool view) override {
    effect_view = view;
    return !refuse_projection;
  }
  // A point projects to its own x and y at depth z.
  [[nodiscard]] RingProjection Project(TrailPoint point) override {
    projected.push_back(point);
    return {.screen = Screen(point.x, point.y), .depth = point.z};
  }
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override {
    ++texture_advances;
    return texture;
  }
  [[nodiscard]] TrailTransparency Transparency() override {
    return transparency;
  }
  [[nodiscard]] std::int32_t NearClip() override { return near_clip; }
  void Submit(const RingQuad& quad, std::int32_t depth) override {
    submissions.push_back({quad, depth});
  }
  void ReportSkippedTrail(std::size_t index, TrailFault fault) override {
    skipped.push_back({index, fault});
  }

  [[nodiscard]] static std::uint32_t Screen(std::int32_t x, std::int32_t y) {
    return static_cast<std::uint16_t>(x) |
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16U);
  }
};

[[nodiscard]] trails::TrailVectorRange Fixed(Vec3 value) {
  return {ScalarSpan{value.x, value.x}, ScalarSpan{value.y, value.y},
          ScalarSpan{value.z, value.z}};
}

[[nodiscard]] trails::TrailPlaneRange FixedPlane(std::int32_t first,
                                                 std::int32_t second) {
  return {ScalarSpan{first, first}, ScalarSpan{second, second}};
}

// The record a trail has after init with `steps` and no starting speed.
[[nodiscard]] TrailRecord Started(std::uint8_t steps) {
  TrailRecord trail;
  trail.step_count = steps;
  trail.sample_count = 1;
  trail.collapse = trails::kTrailFullCollapse;
  return trail;
}
}  // namespace

TEST_CASE("Trail targets offset the sampled target by their anchor",
          "[effects-trail-templates][effects]") {
  const auto [flags, offset] =
      GENERATE(std::pair{0U, Vec3{0, 0, 0}}, std::pair{0x40U, Vec3{0, 0, 0}},
               std::pair{0x80U, Vec3{400, -500, 600}},
               std::pair{0xC0U, Vec3{700, -800, 900}});
  FakeTrailHost host;
  host.request = {400, -500, 600};
  host.attraction = Vec3{700, -800, 900};
  trails::TrailDefinition definition;
  definition.attraction_offset = Fixed({10, -20, 30});
  TrailScratch scratch;
  scratch.flags = flags;
  std::array<TrailRecord, 2> records{};
  trails::TrailTarget(definition, scratch, records, 0, host);
  const TrailPoint expected{static_cast<std::int16_t>(offset.x + 10),
                            static_cast<std::int16_t>(offset.y - 20),
                            static_cast<std::int16_t>(offset.z + 30)};
  for (const TrailRecord& trail : records) {
    CHECK(trail.points.back() == expected);
    CHECK(trail.points.front() == TrailPoint{});
  }
  CHECK(scratch.attraction_offset == trails::TrailWide(expected));
  CHECK(host.ranges.size() == 6);
}

TEST_CASE("Trail targets keep the sampled offset without the transform",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  trails::TrailDefinition definition;
  definition.attraction_offset = Fixed({10, -20, 30});
  definition.attraction_jitter = Fixed({5, 6, 7});
  TrailScratch scratch;
  scratch.flags = trails::kTrailAttractionTransform;
  std::array<TrailRecord, 1> records{};
  trails::TrailTarget(definition, scratch, records, 0, host);
  CHECK(records[0].points.back() == TrailPoint{15, -14, 37});
  CHECK(scratch.attraction_jitter == Vec3{5, 6, 7});
  const std::vector<std::pair<std::int32_t, std::int32_t>> expected{
      {5, -5}, {6, -6}, {7, -7}};
  CHECK(host.ranges == expected);
}

TEST_CASE("Trail control points sit their shares along the spline",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  host.rand_value = 25;
  const bool spread = GENERATE(false, true);
  trails::TrailDefinition definition;
  definition.speed = FixedPlane(64, -64);
  if (spread) {
    definition.force = Fixed({20, 0, 0});
  }
  TrailScratch scratch;
  std::array<TrailRecord, 1> records{};
  TrailRecord& trail = records[0];
  trail.points.front() = {10, 20, 30};
  trail.points.back() = {10, 20, 1054};
  trails::TrailControl(definition, scratch, records, 0, host);
  // 25 % 20 - 10 moves the first share from 64 to 59 of 256.
  const auto first = static_cast<std::int16_t>(spread ? 266 : 286);
  CHECK(trail.points[1] == TrailPoint{10, 20, first});
  CHECK(trail.points[2] == TrailPoint{10, 20, 798});
  CHECK(trail.points.front() == TrailPoint{10, 20, 30});
  CHECK(trail.points.back() == TrailPoint{10, 20, 1054});
  CHECK(scratch.aim_point == Vec3{10, 20, first});
  CHECK(scratch.spawn_point == Vec3{10, 20, 798});
  CHECK(host.rand_calls == (spread ? 1 : 0));
}

TEST_CASE("Trail init starts every trail at its origin",
          "[effects-trail-templates][effects]") {
  const auto [lifetime, steps] =
      GENERATE(std::pair{0, 1}, std::pair{1, 1}, std::pair{12, 12},
               std::pair{255, 255}, std::pair{256, 256});
  FakeTrailHost host;
  trails::TrailDefinition definition;
  definition.lifetime = FixedPlane(lifetime, 0);
  definition.inner_extent = {2, 2};
  TrailScratch scratch;
  std::array<TrailRecord, 2> records{};
  records[0].points.front() = {10, 20, 30};
  records[0].samples.back() = {1, 1, 1};
  std::array<TrailRecord, 2> expected = records;
  for (TrailRecord& trail : expected) {
    // The step count keeps the drawn count's low byte; the first step divides
    // by the whole count.
    trail.step_count = static_cast<std::uint8_t>(steps);
    trail.sample_count = 1;
    trail.collapse = trails::kTrailFullCollapse;
    trail.samples.front() = trail.points.front();
    trail.speed = 8192;
    trail.first_step = (0x06000000 / (steps * steps)) - (16384 / steps);
  }
  trails::TrailInit(definition, scratch, records, 99, host);
  CHECK(records == expected);
  CHECK(scratch.lifetime == VS::Core::Math::Vec2{lifetime, 0});
  CHECK(host.ranges.size() == records.size());
}

TEST_CASE("Trail speed steps pull toward the target against the speed",
          "[effects-trail-templates][effects]") {
  TrailRecord trail;
  CHECK(trails::TrailSpeedStep(trail, 1) == 0x06000000);
  CHECK(trails::TrailSpeedStep(trail, 4) == 0x06000000 / 16);
  trail.progress = kTravel;
  trail.speed = 100;
  CHECK(trails::TrailSpeedStep(trail, 4) == -50);
  trail.speed = -7;
  CHECK(trails::TrailSpeedStep(trail, 2) == 7);
  trail.progress = 0x7FFFFFFF;
  trail.speed = 0;
  // The doubled distance wraps in a word as the original's did.
  CHECK(trails::TrailSpeedStep(trail, 1) == 0x06000002);
}

namespace {
// The reference trail of the shared trail goldens (ParticleRenderTests.cpp),
// updated through all twelve of its steps.
[[nodiscard]] TrailRecord ReachedTrail(trails::TrailHost& host) {
  const trails::TrailDefinition definition;
  TrailScratch scratch;
  std::array<TrailRecord, 1> records{Started(12)};
  records[0].points = {TrailPoint{0, 0, 0}, TrailPoint{256, -64, 0},
                       TrailPoint{256, 64, 256}, TrailPoint{512, 0, 256}};
  for (std::int32_t distance = 0; distance < 12; ++distance) {
    trails::TrailUpdate(definition, scratch, records, distance, host);
  }
  return records[0];
}
}  // namespace

TEST_CASE("Trail updates reach the target in their step count",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  const TrailRecord trail = ReachedTrail(host);
  CHECK(trail.progress == kTravel);
  CHECK(trail.current == 12 % trails::kTrailHistory);
  CHECK(trail.sample_count == trails::kTrailHistory);
  // The Lagrange coefficients truncate, so the endpoint lands one short.
  CHECK(trail.samples.at(trail.current) == TrailPoint{511, 0, 255});
  CHECK(host.rand_calls == 0);
}

TEST_CASE("Trail updates collapse a trail that has reached its target",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  const trails::TrailDefinition definition;
  TrailScratch scratch;
  std::array<TrailRecord, 1> records{ReachedTrail(host)};
  const TrailRecord reached = records[0];
  std::vector<std::pair<std::uint8_t, std::uint8_t>> counts;
  for (std::int32_t distance = 12; distance < 18; ++distance) {
    trails::TrailUpdate(definition, scratch, records, distance, host);
    counts.emplace_back(records[0].collapse, records[0].sample_count);
  }
  const std::vector<std::pair<std::uint8_t, std::uint8_t>> expected{
      {3, 4}, {2, 3}, {1, 2}, {0, 1}, {0, 1}, {0, 1}};
  CHECK(counts == expected);
  TrailRecord collapsed = reached;
  collapsed.collapse = 0;
  collapsed.sample_count = 1;
  CHECK(records[0] == collapsed);
}

TEST_CASE("Short trails keep their history until the collapse passes its steps",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  const trails::TrailDefinition definition;
  TrailScratch scratch;
  std::array<TrailRecord, 1> records{Started(2)};
  TrailRecord& trail = records[0];
  trails::TrailUpdate(definition, scratch, records, 0, host);
  trails::TrailUpdate(definition, scratch, records, 1, host);
  REQUIRE(trail.sample_count == 3);
  const std::array<std::uint8_t, 4> counts{3, 3, 2, 1};
  for (const std::uint8_t count : counts) {
    trails::TrailUpdate(definition, scratch, records, 2, host);
    CHECK(trail.sample_count == count);
  }
  CHECK(trail.collapse == 0);
}

namespace {
// A trail along the x axis at y 50 and depth `depth`, newest sample at x 100
// and each older one 10 to its left.
[[nodiscard]] TrailRecord Ribbon(std::uint8_t samples, std::int16_t depth) {
  TrailRecord trail;
  trail.sample_count = samples;
  trail.collapse = trails::kTrailFullCollapse;
  trail.current = trails::kTrailHistory - 1;
  for (std::size_t age = 0; age < trails::kTrailHistory; ++age) {
    trail.samples.at(trails::kTrailHistory - 1 - age) = {
        static_cast<std::int16_t>(100 - (10 * age)), 50, depth};
  }
  return trail;
}

[[nodiscard]] trails::TrailDefinition WideRibbon() {
  trails::TrailDefinition definition;
  definition.acceleration = Fixed({8, 0, 0});
  return definition;
}

void RenderRibbon(std::span<TrailRecord> records, FakeTrailHost& host) {
  TrailScratch scratch;
  trails::TrailRender(WideRibbon(), scratch, records, 0, host);
}

[[nodiscard]] RingColor Gray(std::uint8_t level) {
  return {.r = level, .g = level, .b = level};
}
}  // namespace

TEST_CASE("Trail ribbons fade across their history in vertical quads",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  std::array<TrailRecord, 1> records{Ribbon(5, 300)};
  RenderRibbon(records, host);
  REQUIRE(host.submissions.size() == 4);
  CHECK(host.projected.size() == 5);
  for (std::size_t quad = 0; quad < host.submissions.size(); ++quad) {
    const auto& [submitted, depth] = host.submissions[quad];
    const auto from = static_cast<std::int32_t>(100 - (10 * quad));
    const std::array screens{
        FakeTrailHost::Screen(from, 54), FakeTrailHost::Screen(from - 10, 54),
        FakeTrailHost::Screen(from, 46), FakeTrailHost::Screen(from - 10, 46)};
    CHECK(submitted.screens == screens);
    const std::uint8_t newer = host.transparency.at(4 - quad);
    const std::uint8_t older = host.transparency.at(3 - quad);
    CHECK(submitted.colors ==
          std::array{Gray(newer), Gray(older), Gray(newer), Gray(older)});
    const auto u = static_cast<std::uint8_t>(16 + (2 * (4 - quad)));
    const auto next = static_cast<std::uint8_t>(u - 2);
    CHECK(submitted.uvs == std::array{RingUv{u, 32}, RingUv{next, 32},
                                      RingUv{u, 40}, RingUv{next, 40}});
    CHECK(submitted.clut == 0x1234);
    CHECK(submitted.tpage == 0x567);
    CHECK(depth == 300);
  }
}

TEST_CASE("Two-sample trails draw one quad from the full fade to none",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  std::array<TrailRecord, 2> records{Ribbon(2, 300), Ribbon(1, 300)};
  RenderRibbon(records, host);
  REQUIRE(host.submissions.size() == 1);
  const RingQuad& quad = host.submissions.front().quad;
  CHECK(quad.colors == std::array{Gray(50), Gray(10), Gray(50), Gray(10)});
  CHECK(quad.uvs == std::array{RingUv{24, 32}, RingUv{16, 32}, RingUv{24, 40},
                               RingUv{16, 40}});
  CHECK(host.projected.size() == 2);
  CHECK(host.skipped.empty());
}

TEST_CASE("Trail quads outside the depth window are not submitted",
          "[effects-trail-templates][effects]") {
  const auto [depth, drawn] =
      GENERATE(std::pair{100, false}, std::pair{101, true},
               std::pair{0x7FF, true}, std::pair{0x800, false});
  FakeTrailHost host;
  host.near_clip = 100;
  std::array<TrailRecord, 1> records{
      Ribbon(2, static_cast<std::int16_t>(depth))};
  RenderRibbon(records, host);
  CHECK(host.submissions.size() == (drawn ? 1 : 0));
}

TEST_CASE("Trail render skips histories its arrays cannot hold",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  std::array<TrailRecord, 5> records{Ribbon(6, 300), Ribbon(2, 300),
                                     Ribbon(2, 300), Ribbon(4, 300),
                                     Ribbon(3, 300)};
  records[1].current = trails::kTrailHistory;
  records[2].collapse = trails::kTrailFullCollapse + 1;
  records[3].collapse = 1;
  records[4].collapse = 1;
  RenderRibbon(records, host);
  REQUIRE(host.skipped.size() == 4);
  const std::array faults{
      TrailFault::kHistoryOverflow, TrailFault::kCursorOutside,
      TrailFault::kCollapseOutside, TrailFault::kFadeUnderflow};
  for (std::size_t index = 0; index < faults.size(); ++index) {
    CHECK(host.skipped[index].index == index);
    CHECK(host.skipped[index].fault == faults.at(index));
  }
  // Three samples with one collapse step left still have their inner edges.
  CHECK(host.submissions.size() == 2);
  CHECK(host.projected.size() == 3);
}

TEST_CASE("Trail render draws nothing without a projection or a texture",
          "[effects-trail-templates][effects]") {
  const bool effect_view = GENERATE(false, true);
  const bool refuse_projection = GENERATE(false, true);
  FakeTrailHost host;
  host.refuse_projection = refuse_projection;
  host.texture.reset();
  std::array<TrailRecord, 1> records{Ribbon(5, 300)};
  TrailScratch scratch;
  scratch.flags = effect_view ? trails::kTrailEffectView : 0U;
  trails::TrailRender(WideRibbon(), scratch, records, 0, host);
  CHECK(host.effect_view == effect_view);
  CHECK(host.texture_advances == (refuse_projection ? 0 : 1));
  CHECK(host.projected.empty());
  CHECK(host.submissions.empty());
  CHECK(scratch.acceleration_min == Vec3{8, 0, 0});
}

TEST_CASE("Trail colour curves double into saturating channels",
          "[effects-trail-templates][effects]") {
  FakeTrailHost host;
  host.curve_values[1] = 0x40;
  host.curve_values[2] = 0x90;
  host.transparency.fill(0x80);
  trails::TrailDefinition definition = WideRibbon();
  definition.curves.color = {1, 2, 0};
  TrailScratch scratch;
  std::array<TrailRecord, 1> records{Ribbon(2, 300)};
  trails::TrailRender(definition, scratch, records, 0, host);
  REQUIRE(host.submissions.size() == 1);
  CHECK(host.submissions.front().quad.colors.front() ==
        RingColor{.r = 0x80, .g = 0xFF, .b = 0x80});
}

namespace {
// For one test: points the effect globals at an empty package and keeps the
// effects channel's messages, restoring both afterwards.
class LiveTrailScope {
 public:
  LiveTrailScope()
      : previous_observer_(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.channel == "effects") {
                messages_.emplace_back(record.message);
              }
            })) {
    Effects_package = &empty_package_;
    VS::Effects::ForgetLivePackageExtents();
  }
  LiveTrailScope(const LiveTrailScope&) = delete;
  LiveTrailScope& operator=(const LiveTrailScope&) = delete;
  LiveTrailScope(LiveTrailScope&&) = delete;
  LiveTrailScope& operator=(LiveTrailScope&&) = delete;
  ~LiveTrailScope() {
    Effects_package = saved_package_;
    VS::Core::SetLogObserver(std::move(previous_observer_));
  }

  [[nodiscard]] bool Logged(std::string_view text) const {
    return std::ranges::any_of(messages_, [text](const std::string& message) {
      return message.find(text) != std::string::npos;
    });
  }

 private:
  std::vector<std::string> messages_;
  VS::Core::LogObserver previous_observer_;
  Effects_EffectPackage* saved_package_ = Effects_package;
  Effects_EffectPackage empty_package_{};
};

// A stage that marks every trail it is given.
void MarkTrails(const trails::TrailDefinition& /*definition*/,
                TrailScratch& /*scratch*/, std::span<TrailRecord> records,
                std::int32_t /*distance*/, trails::TrailHost& /*host*/) {
  for (TrailRecord& trail : records) {
    trail.step_count = 99;
    trail.points.back() = {1, 2, 3};
  }
}
}  // namespace

TEST_CASE("Live trail entries refuse definitions and name their overlay",
          "[effects-trail-templates][effects]") {
  const auto [entry, overlay] =
      GENERATE(std::pair{&trails::UpdateLivePlg057Trail, "PLG057"},
               std::pair{&trails::UpdateLivePlg291Trail, "PLG291"});
  const LiveTrailScope scope;
  Effects_RendererSlot slot{};
  CHECK(entry(&slot, 1, 0x0102) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(scope.Logged(std::string(overlay) +
                     " trail emitter: definition 1 is outside"));
}

TEST_CASE("Live trail stages run over the emitter's trail buffer",
          "[effects-trail-templates][effects]") {
  Effects_ParticleDefinition definition{};
  Effects_ParticleEmitterScratch scratch{};
  Effects_EffectInstance instance{};
  std::array<Effects_Trail, 3> buffer{};
  Effects_TrailQuadSet state{};
  state.trails = buffer.data();
  state.trailcount = 2;
  trails::RunLiveTrailStage(&MarkTrails, definition, scratch, instance, state);
  CHECK(buffer[0].stepCount == 99);
  CHECK(buffer[1].splinePoints[3].vz == 3);
  CHECK(buffer[2].stepCount == 0);
}

TEST_CASE("Live trail stages skip a buffer that cannot hold its count",
          "[effects-trail-templates][effects]") {
  const LiveTrailScope scope;
  Effects_ParticleDefinition definition{};
  Effects_ParticleEmitterScratch scratch{};
  Effects_EffectInstance instance{};
  std::array<Effects_Trail, 1> buffer{};
  Effects_TrailQuadSet state{};
  state.trails = buffer.data();
  state.trailcount = 17;
  trails::RunLiveTrailStage(&MarkTrails, definition, scratch, instance, state);
  state.trails = nullptr;
  state.trailcount = 1;
  trails::RunLiveTrailStage(&MarkTrails, definition, scratch, instance, state);
  CHECK(buffer[0].stepCount == 0);
  CHECK(scope.Logged("17 trails in a buffer of 16; stage skipped"));
  CHECK(scope.Logged("1 trails in a buffer of 0; stage skipped"));
}
