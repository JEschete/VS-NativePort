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

#include "Core/Math/ScalarMath.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TrailCore.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/Templates/TrailsLive.hpp"
#include "Effects/TrailEmitter.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateState.h"
}

namespace {
namespace trails = VS::Effects::Templates;
using trails::RingColor;
using trails::RingProjection;
using trails::RingQuad;
using trails::RingTexture;
using trails::RingUv;
using trails::TrailFault;
using trails::TrailNineRecord;
using trails::TrailScratch;
using VS::Core::Math::ScalarSpan;
using VS::Core::Math::Vec3;
using VS::Effects::TrailPoint;

constexpr std::size_t kNine = trails::kTrailNineHistory;
using NineTransparency = std::array<std::uint8_t, kNine>;

[[nodiscard]] std::uint32_t ScreenOf(std::int32_t x, std::int32_t y) {
  const auto low = static_cast<std::uint16_t>(x);
  const auto high = static_cast<std::uint16_t>(y);
  return low | (static_cast<std::uint32_t>(high) << 16U);
}

// What the nine-sample stages ask of their host: ranges answer their first
// bound, a point projects to its own x and y at depth z, and every quad and
// skipped trail is kept.
class NineSampleRecorder final : public trails::TrailNineHost {
 public:
  static constexpr RingTexture kTexture{
      .clut = 0x4321, .tpage = 0x89, .u = 40, .v = 8, .w = 16, .h = 4};
  NineTransparency fade{9, 18, 27, 36, 45, 54, 63, 72, 81};
  std::vector<std::pair<RingQuad, std::int32_t>> quads;
  std::vector<std::pair<std::size_t, TrailFault>> skips;
  std::size_t range_draws = 0;

  void Submit(const RingQuad& quad, std::int32_t depth) override {
    quads.emplace_back(quad, depth);
  }
  void ReportSkippedTrail(std::size_t index, TrailFault fault) override {
    skips.emplace_back(index, fault);
  }
  [[nodiscard]] NineTransparency Transparency() override { return fade; }
  [[nodiscard]] std::int32_t NearClip() override { return 0; }
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override {
    return kTexture;
  }
  [[nodiscard]] RingProjection Project(TrailPoint point) override {
    return {.screen = ScreenOf(point.x, point.y), .depth = point.z};
  }
  [[nodiscard]] bool BeginProjection(bool /*effect_view*/) override {
    return true;
  }
  [[nodiscard]] std::optional<Vec3> AttractionTranslation() override {
    return std::nullopt;
  }
  [[nodiscard]] Vec3 RequestPosition() override { return {}; }
  [[nodiscard]] std::int32_t RandomRangeExclusive(
      std::int32_t low, std::int32_t /*high*/) override {
    ++range_draws;
    return low;
  }
  [[nodiscard]] std::int32_t Rand() override { return 0; }
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t /*curve*/,
                                         std::int32_t /*distance*/) override {
    return 0;
  }
};

// A nine-sample trail just started over the shared golden spline
// (ParticleRenderTests.cpp) with `steps` steps to cross it.
[[nodiscard]] TrailNineRecord StartedNine(std::uint8_t steps) {
  TrailNineRecord trail;
  trail.points = {TrailPoint{0, 0, 0}, TrailPoint{256, -64, 0},
                  TrailPoint{256, 64, 256}, TrailPoint{512, 0, 256}};
  trail.collapse = trails::kTrailNineFullCollapse;
  trail.sample_count = 1;
  trail.step_count = steps;
  return trail;
}

void UpdateNine(std::span<TrailNineRecord> records, std::int32_t distance,
                NineSampleRecorder& host) {
  TrailScratch scratch;
  trails::TrailNineUpdate(trails::TrailDefinition{}, scratch, records, distance,
                          host);
}

// `trail` updated from distance `first` up to (not including) `last`, with the
// collapse counter and sample count after each update.
[[nodiscard]] std::vector<std::pair<std::uint8_t, std::uint8_t>> UpdatedNine(
    TrailNineRecord& trail, std::int32_t first, std::int32_t last) {
  NineSampleRecorder host;
  std::vector<std::pair<std::uint8_t, std::uint8_t>> counts;
  for (std::int32_t distance = first; distance < last; ++distance) {
    UpdateNine(std::span(&trail, 1), distance, host);
    counts.emplace_back(trail.collapse, trail.sample_count);
  }
  return counts;
}

// A nine-sample trail along y 50 at depth 300: entry i sits at x 20 + 10 i, so
// the newest (the last entry) is at x 100.
[[nodiscard]] TrailNineRecord NineRibbon(std::uint8_t samples) {
  TrailNineRecord trail;
  for (std::size_t index = 0; index < kNine; ++index) {
    trail.samples.at(index) = {static_cast<std::int16_t>(20 + (10 * index)), 50,
                               300};
  }
  trail.current = kNine - 1;
  trail.collapse = trails::kTrailNineFullCollapse;
  trail.sample_count = samples;
  return trail;
}

// Renders with a ribbon eight wide (four either side of the samples).
void DrawNine(std::span<TrailNineRecord> records, NineSampleRecorder& host) {
  trails::TrailDefinition definition;
  definition.acceleration = {ScalarSpan{8, 8}, ScalarSpan{0, 0},
                             ScalarSpan{0, 0}};
  TrailScratch scratch;
  trails::TrailNineRender(definition, scratch, records, 0, host);
}

[[nodiscard]] RingColor Level(std::uint8_t level) {
  return {.r = level, .g = level, .b = level};
}

[[nodiscard]] bool Logged(const std::vector<std::string>& messages,
                          std::string_view text) {
  return std::ranges::any_of(messages, [text](const std::string& message) {
    return message.find(text) != std::string::npos;
  });
}
}  // namespace

TEST_CASE("Nine-sample trail init starts each trail at its full collapse",
          "[effects-trail-nine-templates][effects]") {
  NineSampleRecorder host;
  trails::TrailDefinition definition;
  definition.lifetime = {ScalarSpan{6, 6}, ScalarSpan{0, 0}};
  definition.inner_extent = {3, 3};
  TrailScratch scratch;
  std::array<TrailNineRecord, 2> records{};
  records[1].points.front() = {-5, 7, 9};
  records[1].samples.back() = {4, 4, 4};
  trails::TrailNineInit(definition, scratch, records, 0, host);
  for (const TrailNineRecord& trail : records) {
    CHECK(trail.collapse == 8);
    CHECK(trail.sample_count == 1);
    CHECK(trail.step_count == 6);
    CHECK(trail.samples.front() == trail.points.front());
    CHECK(trail.speed == (3 << 12));
    CHECK(trail.first_step == (0x06000000 / 36) - ((2 * (3 << 12)) / 6));
  }
  CHECK(records[1].samples.back() == TrailPoint{4, 4, 4});
  CHECK(host.range_draws == records.size());
}

TEST_CASE("Nine-sample trail updates wrap their cursor through nine entries",
          "[effects-trail-nine-templates][effects]") {
  NineSampleRecorder host;
  std::array<TrailNineRecord, 1> records{StartedNine(12)};
  std::vector<std::pair<std::uint8_t, std::uint8_t>> cursors;
  for (std::int32_t distance = 0; distance < 12; ++distance) {
    UpdateNine(records, distance, host);
    cursors.emplace_back(records[0].current, records[0].sample_count);
  }
  const std::vector<std::pair<std::uint8_t, std::uint8_t>> expected{
      {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7},
      {7, 8}, {8, 9}, {0, 9}, {1, 9}, {2, 9}, {3, 9}};
  CHECK(cursors == expected);
  CHECK(records[0].progress == 0x03000000);
  // The Lagrange coefficients truncate, so the endpoint lands one short.
  CHECK(records[0].samples.at(3) == TrailPoint{511, 0, 255});
}

TEST_CASE("Nine-sample trails collapse over eight ticks at their target",
          "[effects-trail-nine-templates][effects]") {
  TrailNineRecord trail = StartedNine(12);
  static_cast<void>(UpdatedNine(trail, 0, 12));
  const TrailNineRecord reached = trail;
  const std::vector<std::pair<std::uint8_t, std::uint8_t>> expected{
      {7, 8}, {6, 7}, {5, 6}, {4, 5}, {3, 4},
      {2, 3}, {1, 2}, {0, 1}, {0, 1}, {0, 1}};
  CHECK(UpdatedNine(trail, 12, 22) == expected);
  TrailNineRecord collapsed = reached;
  collapsed.collapse = 0;
  collapsed.sample_count = 1;
  CHECK(trail == collapsed);
}

TEST_CASE("Short nine-sample trails keep their history until the last steps",
          "[effects-trail-nine-templates][effects]") {
  TrailNineRecord trail = StartedNine(2);
  static_cast<void>(UpdatedNine(trail, 0, 2));
  REQUIRE(trail.sample_count == 3);
  const std::vector<std::pair<std::uint8_t, std::uint8_t>> expected{
      {7, 3}, {6, 3}, {5, 3}, {4, 3}, {3, 3}, {2, 3}, {1, 2}, {0, 1}};
  std::vector<std::pair<std::uint8_t, std::uint8_t>> counts;
  for (std::size_t tick = 0; tick < expected.size(); ++tick) {
    const auto step = UpdatedNine(trail, 2, 3);
    counts.push_back(step.front());
  }
  CHECK(counts == expected);
}

TEST_CASE("Nine-sample trail ribbons fade and slide their u in eighths",
          "[effects-trail-nine-templates][effects]") {
  NineSampleRecorder host;
  std::array<TrailNineRecord, 1> records{NineRibbon(9)};
  DrawNine(records, host);
  REQUIRE(host.quads.size() == kNine - 1);
  for (std::size_t quad = 0; quad < host.quads.size(); ++quad) {
    const auto& [drawn, depth] = host.quads[quad];
    const std::size_t newer = kNine - 1 - quad;
    const auto x = static_cast<std::int32_t>(100 - (10 * quad));
    CHECK(drawn.screens == std::array{ScreenOf(x, 54), ScreenOf(x - 10, 54),
                                      ScreenOf(x, 46), ScreenOf(x - 10, 46)});
    const RingColor front = Level(host.fade.at(newer));
    const RingColor back = Level(host.fade.at(newer - 1));
    CHECK(drawn.colors == std::array{front, back, front, back});
    // u moves an eighth of the texture's width per collapse step.
    const auto u = static_cast<std::uint8_t>(40 + (2 * newer));
    const auto next = static_cast<std::uint8_t>(u - 2);
    CHECK(drawn.uvs == std::array{RingUv{u, 8}, RingUv{next, 8}, RingUv{u, 12},
                                  RingUv{next, 12}});
    CHECK(depth == 300);
  }
  CHECK(host.quads.front().first.clut == 0x4321);
}

TEST_CASE("Nine-sample trail render skips histories its arrays cannot hold",
          "[effects-trail-nine-templates][effects]") {
  NineSampleRecorder host;
  std::array<TrailNineRecord, 5> records{NineRibbon(10), NineRibbon(2),
                                         NineRibbon(2), NineRibbon(4),
                                         NineRibbon(3)};
  records[1].current = kNine;
  records[2].collapse = trails::kTrailNineFullCollapse + 1;
  records[3].collapse = 1;
  records[4].collapse = 1;
  DrawNine(records, host);
  const std::vector<std::pair<std::size_t, TrailFault>> expected{
      {0, TrailFault::kHistoryOverflow},
      {1, TrailFault::kCursorOutside},
      {2, TrailFault::kCollapseOutside},
      {3, TrailFault::kFadeUnderflow}};
  CHECK(host.skips == expected);
  // Three samples with one collapse step left still draw both their quads.
  CHECK(host.quads.size() == 2);
}

TEST_CASE("Nine-sample transparency samples its curve at eighths",
          "[effects-trail-nine-templates][effects]") {
  std::array<std::uint8_t, 17> curve{};
  for (std::size_t index = 0; index < curve.size(); ++index) {
    curve.at(index) = static_cast<std::uint8_t>(3 * index);
  }
  NineTransparency expected{};
  for (std::size_t sample = 0; sample < kNine; ++sample) {
    expected.at(sample) = curve.at(2 * sample);
  }
  const auto sampled = VS::Effects::SampleTrailNineTransparency(curve);
  CHECK(sampled.samples == expected);
  CHECK_FALSE(sampled.clamped);
  const auto opaque = VS::Effects::SampleTrailNineTransparency({});
  expected.fill(VS::Effects::kOpaqueTrailTransparency);
  CHECK(opaque.samples == expected);
  CHECK_FALSE(opaque.clamped);
}

TEST_CASE("Nine-sample transparency clamps a one-entry curve's last sample",
          "[effects-trail-nine-templates][effects]") {
  const std::array<std::uint8_t, 1> single{0x33};
  const auto one = VS::Effects::SampleTrailNineTransparency(single);
  NineTransparency expected{};
  expected.fill(0x33);
  CHECK(one.samples == expected);
  CHECK(one.clamped);
  const std::array<std::uint8_t, 2> pair{0x11, 0x22};
  const auto two = VS::Effects::SampleTrailNineTransparency(pair);
  expected.fill(0x11);
  expected.back() = 0x22;
  CHECK(two.samples == expected);
  CHECK_FALSE(two.clamped);
}

TEST_CASE("Nine-sample emitters hold at most eight trails",
          "[effects-trail-nine-templates][effects]") {
  constexpr std::size_t kLimit = VS::Effects::kMaxNineSampleTrailsPerEmitter;
  CHECK(kLimit == 8);
  CHECK(VS::Effects::TrailCountFor(0, kLimit) == 0);
  CHECK(VS::Effects::TrailCountFor(5, kLimit) == 5);
  CHECK(VS::Effects::TrailCountFor(8, kLimit) == 8);
  CHECK(VS::Effects::TrailCountFor(100, kLimit) == 8);
  CHECK_FALSE(VS::Effects::TrailCountFor(-1, kLimit).has_value());
}

namespace {
// For one test: points the effect globals at an empty package, restoring the
// previous one afterwards.
class EmptyPackageScope {
 public:
  EmptyPackageScope() {
    Effects_package = &empty_;
    VS::Effects::ForgetLivePackageExtents();
  }
  EmptyPackageScope(const EmptyPackageScope&) = delete;
  EmptyPackageScope& operator=(const EmptyPackageScope&) = delete;
  EmptyPackageScope(EmptyPackageScope&&) = delete;
  EmptyPackageScope& operator=(EmptyPackageScope&&) = delete;
  ~EmptyPackageScope() {
    Effects_package = saved_;
    VS::Effects::ForgetLivePackageExtents();
  }

 private:
  Effects_EffectPackage* saved_ = Effects_package;
  Effects_EffectPackage empty_{};
};

// A stage that marks every nine-sample trail it is given.
void MarkNine(const trails::TrailDefinition& /*definition*/,
              TrailScratch& /*scratch*/, std::span<TrailNineRecord> records,
              std::int32_t /*distance*/, trails::TrailNineHost& /*host*/) {
  for (TrailNineRecord& trail : records) {
    trail.current = 7;
    trail.samples.back() = {4, 5, 6};
  }
}
}  // namespace

TEST_CASE("Nine-sample live trail entries refuse definitions by overlay",
          "[effects-trail-nine-templates][effects]") {
  const auto [entry, overlay] =
      GENERATE(std::pair{&trails::UpdateLivePlg049TrailNine, "PLG049"},
               std::pair{&trails::UpdateLivePlg050TrailNine, "PLG050"},
               std::pair{&trails::UpdateLivePlg057TrailNine, "PLG057"},
               std::pair{&trails::UpdateLivePlg069TrailNine, "PLG069"},
               std::pair{&trails::UpdateLivePlg135TrailNine, "PLG135"},
               std::pair{&trails::UpdateLivePlg161TrailNine, "PLG161"},
               std::pair{&trails::UpdateLivePlg165TrailNine, "PLG165"},
               std::pair{&trails::UpdateLivePlg167TrailNine, "PLG167"},
               std::pair{&trails::UpdateLivePlg168TrailNine, "PLG168"},
               std::pair{&trails::UpdateLivePlg169TrailNine, "PLG169"},
               std::pair{&trails::UpdateLivePlg232TrailNine, "PLG232"});
  const VS::test_support::ChannelWarnings warnings("effects");
  const EmptyPackageScope package;
  Effects_RendererSlot slot{};
  CHECK(entry(&slot, 1, 0x0102) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(
      Logged(warnings.messages,
             std::string(overlay) + " trail emitter: definition 1 is outside"));
}

TEST_CASE("Nine-sample live trail stages hold at most eight trails",
          "[effects-trail-nine-templates][effects]") {
  const VS::test_support::ChannelWarnings warnings("effects");
  Effects_ParticleDefinition definition{};
  Effects_ParticleEmitterScratch scratch{};
  Effects_EffectInstance instance{};
  std::array<Effects_Trail9, 3> buffer{};
  Effects_Trail9State state{};
  state.trails = buffer.data();
  state.trailCount = 2;
  trails::RunLiveTrailNineStage(&MarkNine, definition, scratch, instance,
                                state);
  CHECK(buffer[0].currentSplineSample == 7);
  CHECK(buffer[1].splineSamples[kNine - 1].vz == 6);
  CHECK(buffer[2].currentSplineSample == 0);
  state.trailCount = static_cast<std::uint8_t>(kNine);
  trails::RunLiveTrailNineStage(&MarkNine, definition, scratch, instance,
                                state);
  CHECK(buffer[2].currentSplineSample == 0);
  CHECK(Logged(warnings.messages, "9 trails in a buffer of 8; stage skipped"));
}
