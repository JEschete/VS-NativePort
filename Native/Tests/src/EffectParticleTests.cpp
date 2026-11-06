#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Log.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Random.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/ParticleControl.hpp"
#include "Effects/Templates/ParticleCore.hpp"
#include "Effects/Templates/ParticleDispatch.hpp"
#include "Effects/Templates/ParticleLineRender.hpp"
#include "Effects/Templates/ParticleOrigin.hpp"
#include "Effects/Templates/ParticleRender.hpp"
#include "Effects/Templates/ParticleTarget.hpp"
#include "Effects/Templates/ParticlesLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
}

namespace {
namespace fx = VS::Effects;
namespace math = VS::Core::Math;
namespace particles = VS::Effects::Templates;
using math::Vec2;
using math::Vec3;
using particles::kParticleEmissionCounters;
using particles::kParticleProjections;
using particles::kParticleSegments;
using particles::kParticleSetCapacity;
using particles::ParticleAnchor;
using particles::ParticleColor;
using particles::ParticleCommand;
using particles::ParticleControlSet;
using particles::ParticleDispatchOptions;
using particles::ParticleEmissionCounter;
using particles::ParticleLine;
using particles::ParticlePlacement;
using particles::ParticlePoint;
using particles::ParticleProjection;
using particles::ParticleQuad;
using particles::ParticleSetDefinition;
using particles::ParticleSetFrame;
using particles::ParticleSetHost;
using particles::ParticleSetPoints;
using particles::ParticleSetScratch;
using particles::ParticleSetState;
using particles::ParticleShape;
using particles::ParticleStage;
using particles::ParticleTexture;
using particles::ParticleTexturing;
using particles::ParticleUv;

constexpr std::uint32_t kSeed = 1234;
// The fake host's screens: each ribbon point 16 pixels right of the last,
// all on row 100, at depth 100.
constexpr std::uint32_t kScreenStep = 16;
constexpr std::int32_t kScreenRow = 100;
constexpr std::int32_t kDepth = 100;

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

[[nodiscard]] constexpr std::uint32_t Flags(auto value) noexcept {
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] constexpr std::size_t Index(
    ParticleEmissionCounter counter) noexcept {
  return static_cast<std::size_t>(counter);
}

// A screen word: x in the low halfword, y in the high one.
[[nodiscard]] constexpr std::uint32_t Screen(std::int32_t x,
                                             std::int32_t y) noexcept {
  return (static_cast<std::uint32_t>(x) & 0xFFFFU) |
         (static_cast<std::uint32_t>(y) << 16U);
}

// The fake host's screen for ribbon point `point`.
[[nodiscard]] constexpr std::uint32_t PointScreen(std::size_t point) noexcept {
  return Screen(static_cast<std::int32_t>(point * kScreenStep), kScreenRow);
}

struct QuadSubmission {
  ParticleQuad quad;
  std::int32_t depth{};
};

struct LineSubmission {
  ParticleLine line;
  std::int32_t depth{};
};

struct RangeDraw {
  std::int32_t first{};
  std::int32_t second{};

  friend bool operator==(const RangeDraw&, const RangeDraw&) = default;
};

class FakeParticleHost final : public ParticleSetHost {
 public:
  std::optional<ParticleSetDefinition> definition = ParticleSetDefinition{};
  std::vector<std::uint8_t> definition_requests;
  std::optional<ParticleSetState> state;
  std::optional<std::int32_t> created_capacity;
  std::optional<ParticleTexturing> created_texturing;
  std::size_t releases = 0;
  particles::ParticleTickInputs inputs{};
  bool refuse_tick = false;
  std::vector<std::uint8_t> ticked_definitions;
  ParticleSetPoints points{.capacity = kParticleSetCapacity};
  ParticleSetScratch scratch{};
  std::optional<ParticleSetScratch> published;
  std::array<std::int32_t, 256> curve_values{};
  std::vector<std::pair<std::uint8_t, std::int32_t>> samples;
  // Scripted draws come first, then the BIOS generator's sequence.
  std::vector<std::int32_t> scripted_rands;
  VS::Core::Rand15 random{kSeed};
  std::size_t rand_draws = 0;
  std::vector<RangeDraw> range_draws;
  std::array<std::int32_t, kParticleEmissionCounters> counters{};
  std::vector<ParticleEmissionCounter> stored_counters;
  std::optional<bool> effect_view;
  std::vector<ParticlePoint> projected;
  std::int32_t depth = kDepth;
  std::optional<ParticleTexture> texture = ParticleTexture{
      .clut = 0x1234, .tpage = 0x567, .u = 16, .v = 32, .w = 8, .h = 8};
  std::size_t texture_advances = 0;
  std::vector<QuadSubmission> quads;
  std::vector<LineSubmission> lines;
  std::string stages;
  std::vector<std::int16_t> staged_counts;

  [[nodiscard]] std::optional<ParticleSetDefinition> Definition(
      std::uint8_t index) override {
    definition_requests.push_back(index);
    return definition;
  }
  void Create(const ParticleSetState& value, std::int32_t capacity,
              ParticleTexturing texturing) override {
    state = value;
    created_capacity = capacity;
    created_texturing = texturing;
  }
  [[nodiscard]] std::optional<ParticleSetState> State() const override {
    return state;
  }
  void StoreState(const ParticleSetState& value) override { state = value; }
  void Release() override { ++releases; }
  [[nodiscard]] std::optional<particles::ParticleTickInputs> BeginTick(
      std::uint8_t definition_index) override {
    ticked_definitions.push_back(definition_index);
    if (refuse_tick) {
      return std::nullopt;
    }
    return inputs;
  }
  [[nodiscard]] ParticleSetPoints LoadPoints() const override { return points; }
  void StorePoints(const ParticleSetPoints& value) override { points = value; }
  [[nodiscard]] ParticleSetScratch LoadScratch() const override {
    return scratch;
  }
  void PublishScratch(const ParticleSetScratch& value) override {
    published = value;
  }
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override {
    samples.emplace_back(curve, distance);
    return curve_values.at(curve);
  }
  [[nodiscard]] std::int32_t Rand() override {
    ++rand_draws;
    if (!scripted_rands.empty()) {
      const std::int32_t value = scripted_rands.front();
      scripted_rands.erase(scripted_rands.begin());
      return value;
    }
    return random.Next();
  }
  // The lower bound, so the tests can predict every draw.
  [[nodiscard]] std::int32_t RandomRange(std::int32_t first,
                                         std::int32_t second) override {
    range_draws.push_back({first, second});
    return std::min(first, second);
  }
  [[nodiscard]] std::int32_t EmissionCounter(
      ParticleEmissionCounter counter) const override {
    return counters.at(Index(counter));
  }
  void StoreEmissionCounter(ParticleEmissionCounter counter,
                            std::int32_t value) override {
    counters.at(Index(counter)) = value;
    stored_counters.push_back(counter);
  }
  void BeginProjection(bool view) override { effect_view = view; }
  [[nodiscard]] ParticleProjection Project(ParticlePoint point) override {
    const std::size_t index = projected.size() % kParticleProjections;
    projected.push_back(point);
    return {.screen = PointScreen(index), .depth = depth};
  }
  [[nodiscard]] std::optional<ParticleTexture> AdvanceTexture() override {
    ++texture_advances;
    return texture;
  }
  void SubmitQuad(const ParticleQuad& quad, std::int32_t at) override {
    quads.push_back({quad, at});
  }
  void SubmitLine(const ParticleLine& line, std::int32_t at) override {
    lines.push_back({line, at});
  }
};

FakeParticleHost& Fake(ParticleSetHost& host) {
  return dynamic_cast<FakeParticleHost&>(host);
}

// The stages a ParticleSetFrame-recording dispatcher binds: each notes its
// run, and the control and render stages leave a mark the host must store.
constexpr ParticlePoint kStageMark{7, 8, 9};

void RecordOrigin(ParticleSetFrame& frame, ParticleSetHost& host) {
  Fake(host).stages += 'o';
  Fake(host).staged_counts.push_back(frame.state.count);
}

void RecordTarget(ParticleSetFrame& /*frame*/, ParticleSetHost& host) {
  Fake(host).stages += 't';
}

void RecordControl(ParticleSetFrame& frame, ParticleSetHost& host) {
  Fake(host).stages += 'c';
  frame.points.sets.front()[1] = kStageMark;
}

void RecordRender(ParticleSetFrame& frame, ParticleSetHost& host) {
  Fake(host).stages += 'r';
  frame.scratch.vector = kStageMark;
}

constexpr ParticleDispatchOptions kRecordingOptions{
    .origin = &RecordOrigin,
    .target = &RecordTarget,
    .control = &RecordControl,
    .render = &RecordRender,
    .texturing = ParticleTexturing::kUntextured,
};

bool Dispatch(ParticleCommand command, std::int32_t packed,
              FakeParticleHost& host) {
  return particles::ParticleDispatch(kRecordingOptions, command, packed, host);
}

using DispatchFunction = bool(ParticleCommand, std::int32_t, ParticleSetHost&);

// The stages and texturing each dispatch instance binds and the emission
// cursor its origin steps, as the reference plugin sources define them
// before including particleDispatch.h (#290: the generated instances once
// leaked option flags between overlays).
struct ReferenceBinding {
  DispatchFunction* dispatch;
  const ParticleDispatchOptions* options;
  ParticleStage origin;
  ParticleStage target;
  ParticleStage control;
  ParticleStage render;
  ParticleTexturing texturing;
  ParticleEmissionCounter counter;
  const char* reference;
};

const std::array<ReferenceBinding, 22> kReferenceBindings{{
    {&fx::Plg045::ParticleDispatch, &fx::Plg045::kParticleDispatchOptions,
     &fx::Plg045::ParticleOrigin, &fx::Plg045::ParticleTarget,
     &fx::Plg045::ParticleControl, &fx::Plg045::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg045,
     "PLG045.BIN/110.c:11"},
    {&fx::Plg057::ParticleDispatch, &fx::Plg057::kParticleDispatchOptions,
     &fx::Plg057::ParticleOrigin, &fx::Plg057::ParticleTarget,
     &fx::Plg057::ParticleControl, &fx::Plg057::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg057,
     "PLG057.BIN/2D0.c:13"},
    {&fx::Plg065::ParticleDispatch, &fx::Plg065::kParticleDispatchOptions,
     &fx::Plg065::ParticleOrigin, &fx::Plg065::ParticleTarget,
     &fx::Plg065::ParticleControl, &fx::Plg065::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg065,
     "PLG065.BIN/F8.c:42"},
    {&fx::Plg082::ParticleDispatch, &fx::Plg082::kParticleDispatchOptions,
     &fx::Plg082::ParticleOrigin, &fx::Plg082::ParticleTarget,
     &fx::Plg082::ParticleControl, &fx::Plg082::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg082,
     "PLG082.BIN/128.c:19"},
    {&fx::Plg087::ParticleDispatch, &fx::Plg087::kParticleDispatchOptions,
     &fx::Plg087::ParticleOrigin, &fx::Plg087::ParticleTarget,
     &fx::Plg087::ParticleControl, &fx::Plg087::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg087,
     "PLG087.BIN/1F0.c:54"},
    {&fx::Plg114::ParticleDispatch, &fx::Plg114::kParticleDispatchOptions,
     &fx::Plg114::ParticleOrigin, &fx::Plg114::ParticleTarget,
     &fx::Plg114::ParticleControl, &fx::Plg114::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg114,
     "PLG114.BIN/208.c:13"},
    {&fx::Plg114::ParticleDispatch2, &fx::Plg114::kParticleDispatch2Options,
     &fx::Plg114::ParticleOrigin2, &fx::Plg114::ParticleTarget2,
     &fx::Plg114::ParticleControl2, &fx::Plg114::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg114Second,
     "PLG114.BIN/208.c:37"},
    {&fx::Plg146::ParticleDispatch, &fx::Plg146::kParticleDispatchOptions,
     &fx::Plg146::ParticleOrigin, &fx::Plg146::ParticleTarget,
     &fx::Plg146::ParticleControl, &fx::Plg146::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg146,
     "PLG146.BIN/1D8.c:33"},
    {&fx::Plg147::ParticleDispatch, &fx::Plg147::kParticleDispatchOptions,
     &fx::Plg147::ParticleOrigin, &fx::Plg147::ParticleTarget,
     &fx::Plg147::ParticleControl, &fx::Plg147::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg147,
     "PLG147.BIN/F8.c:29"},
    {&fx::Plg152::ParticleDispatch, &fx::Plg152::kParticleDispatchOptions,
     &fx::Plg152::ParticleOrigin, &fx::Plg152::ParticleTarget,
     &fx::Plg152::ParticleControl, &fx::Plg152::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg152,
     "PLG152.BIN/128.c:22"},
    {&fx::Plg153::ParticleDispatch, &fx::Plg153::kParticleDispatchOptions,
     &fx::Plg153::ParticleOrigin, &fx::Plg153::ParticleTarget,
     &fx::Plg153::ParticleControl, &fx::Plg153::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg153,
     "PLG153.BIN/110.c:23"},
    {&fx::Plg156::ParticleDispatch, &fx::Plg156::kParticleDispatchOptions,
     &fx::Plg156::ParticleOrigin, &fx::Plg156::ParticleTarget,
     &fx::Plg156::ParticleControl, &fx::Plg156::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg156,
     "PLG156.BIN/128.c:13"},
    {&fx::Plg168::ParticleDispatch, &fx::Plg168::kParticleDispatchOptions,
     &fx::Plg168::ParticleOrigin, &fx::Plg168::ParticleTarget,
     &fx::Plg168::ParticleControl, &fx::Plg168::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg168,
     "PLG168.BIN/1F0.c:35"},
    {&fx::Plg224::ParticleDispatch, &fx::Plg224::kParticleDispatchOptions,
     &fx::Plg224::ParticleOrigin, &fx::Plg224::ParticleTarget,
     &fx::Plg224::ParticleControl, &fx::Plg224::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg224,
     "PLG224.BIN/F8.c:25"},
    {&fx::Plg255::ParticleDispatch, &fx::Plg255::kParticleDispatchOptions,
     &fx::Plg255::ParticleOrigin, &fx::Plg255::ParticleTarget,
     &fx::Plg255::ParticleControl, &fx::Plg255::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg255,
     "PLG255.BIN/1D8.c:35"},
    {&fx::Plg263::ParticleDispatch, &fx::Plg263::kParticleDispatchOptions,
     &fx::Plg263::ParticleOrigin, &fx::Plg263::ParticleTarget,
     &fx::Plg263::ParticleControl, &fx::Plg263::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg263,
     "PLG263.BIN/110.c:355"},
    {&fx::Plg265::ParticleDispatch, &fx::Plg265::kParticleDispatchOptions,
     &fx::Plg265::ParticleOrigin, &fx::Plg265::ParticleTarget,
     &fx::Plg265::ParticleControl, &fx::Plg265::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg265,
     "PLG265.BIN/2E8.c:29"},
    {&fx::Plg265::ParticleDispatch2, &fx::Plg265::kParticleDispatch2Options,
     &fx::Plg265::ParticleOrigin2, &fx::Plg265::ParticleTarget2,
     &fx::Plg265::ParticleControl2, &fx::Plg265::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg265Second,
     "PLG265.BIN/2E8.c:51"},
    {&fx::Plg266::ParticleDispatch, &fx::Plg266::kParticleDispatchOptions,
     &fx::Plg266::ParticleOrigin, &fx::Plg266::ParticleTarget,
     &fx::Plg266::ParticleControl, &fx::Plg266::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg266,
     "PLG266.BIN/2E8.c:13"},
    {&fx::Plg266::ParticleDispatch2, &fx::Plg266::kParticleDispatch2Options,
     &fx::Plg266::ParticleOrigin2, &fx::Plg266::ParticleTarget2,
     &fx::Plg266::ParticleControl2, &fx::Plg266::ParticleLineRender,
     ParticleTexturing::kUntextured, ParticleEmissionCounter::kPlg266Second,
     "PLG266.BIN/2E8.c:37"},
    {&fx::Plg300::ParticleDispatch, &fx::Plg300::kParticleDispatchOptions,
     &fx::Plg300::ParticleOrigin, &fx::Plg300::ParticleTarget,
     &fx::Plg300::ParticleControl, &fx::Plg300::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg300,
     "PLG300.BIN/110.c:23"},
    {&fx::Plg301::ParticleDispatch, &fx::Plg301::kParticleDispatchOptions,
     &fx::Plg301::ParticleOrigin, &fx::Plg301::ParticleTarget,
     &fx::Plg301::ParticleControl, &fx::Plg301::ParticleRender,
     ParticleTexturing::kTextured, ParticleEmissionCounter::kPlg301,
     "PLG301.BIN/110.c:19"},
}};

// A tick that places one ribbon from a box corner to a target 400 along z,
// so the origin steps its emission cursor once, and draws it.
particles::ParticleTickInputs CornerTick() {
  particles::ParticleTickInputs inputs{};
  inputs.definition.flags =
      Flags(ParticleShape::kBox) | Flags(ParticlePlacement::kCorners);
  inputs.definition.interval = {1, 1};
  inputs.definition.target_offset = {{{0, 0}, {0, 0}, {400, 400}}};
  return inputs;
}
}  // namespace

TEST_CASE("Particle dispatch options bind the reference plugins' stages",
          "[effects-particles][effects]") {
  for (const ReferenceBinding& binding : kReferenceBindings) {
    INFO(binding.reference);
    CHECK(binding.options->origin == binding.origin);
    CHECK(binding.options->target == binding.target);
    CHECK(binding.options->control == binding.control);
    CHECK(binding.options->render == binding.render);
    CHECK(binding.options->texturing == binding.texturing);
  }
}

TEST_CASE("Particle dispatch instances run their reference bindings",
          "[effects-particles][effects]") {
  for (const ReferenceBinding& binding : kReferenceBindings) {
    INFO(binding.reference);
    FakeParticleHost host;
    Required(host.definition).interval = {1, 1};
    CHECK(binding.dispatch(ParticleCommand::kCreate, 5, host));
    CHECK(host.created_texturing == binding.texturing);
    host.inputs = CornerTick();
    CHECK(binding.dispatch(ParticleCommand::kTick, 0, host));
    CHECK(host.stored_counters == std::vector{binding.counter});
    CHECK(host.counters.at(Index(binding.counter)) == 1);
    const bool textured = binding.texturing == ParticleTexturing::kTextured;
    CHECK(host.quads.size() == (textured ? kParticleSegments : 0));
    CHECK(host.lines.size() == (textured ? 0 : kParticleSegments));
  }
}

TEST_CASE("Particle set plans and capacities keep the original's field widths",
          "[effects-particles][effects]") {
  using particles::ParticleSetPlan;
  CHECK(particles::PlanParticleSet(0x1234) == ParticleSetPlan{0x34, 0x12});
  CHECK(particles::PlanParticleSet(-1) == ParticleSetPlan{0xFF, 0xFF});
  CHECK(particles::ParticleSetCapacity({3, 5}) == 5);
  CHECK(particles::ParticleSetCapacity({9, 2}) == 8);
  CHECK(particles::ParticleSetCapacity({-4, -2}) == -2);
  CHECK(particles::ParticleSetCapacity({0x10003, 0}) == 3);
  CHECK(particles::ParticleSetCapacity({0x18000, 0}) == -0x8000);
}

TEST_CASE("Particle ribbon counts stay within the set's points",
          "[effects-particles][effects]") {
  CHECK(particles::ParticleSetCount(0x10005) == 5);
  CHECK(particles::ParticleSetCount(12) == 8);
  CHECK(particles::ParticleSetCount(-3) == -3);
  ParticleSetFrame frame{};
  frame.points.capacity = 3;
  frame.state.count = -1;
  CHECK(particles::ParticleRibbons(frame) == 0);
  frame.state.count = 5;
  CHECK(particles::ParticleRibbons(frame) == 3);
  frame.points.capacity = kParticleSetCapacity;
  CHECK(particles::ParticleRibbons(frame) == 5);
}

TEST_CASE("Particle remainders and halves keep the R3000 division rules",
          "[effects-particles][effects]") {
  constexpr std::int32_t kMinimum = std::numeric_limits<std::int32_t>::min();
  CHECK(particles::ParticleRemainder(7, 0) == 7);
  CHECK(particles::ParticleRemainder(kMinimum, -1) == 0);
  CHECK(particles::ParticleRemainder(-7, 3) == -1);
  CHECK(particles::ParticleRemainder(7, -3) == 1);
  CHECK(particles::ParticleHalf(-3) == -1);
  CHECK(particles::ParticleHalf(5) == 2);
  CHECK(particles::ParticleHalf(kMinimum) == kMinimum / 2);
}

TEST_CASE("Particle Q12 products and points keep the original's widths",
          "[effects-particles][effects]") {
  CHECK(particles::ParticleFixed(0x1000, 300) == 300);
  CHECK(particles::ParticleFixed(-0x800, 3) == -2);
  CHECK(particles::ParticleFixed(0x10000, 0x10000) == 0);
  CHECK(particles::ParticlePointAt({0x18000, -1, 0x12345}) ==
        ParticlePoint{-0x8000, -1, 0x2345});
}

namespace {
struct DepthCase {
  std::int32_t first{};
  std::int32_t second{};
  std::int16_t bias{};
  std::int32_t near_clip{};
  std::optional<std::int32_t> expected;
};

const std::array<DepthCase, 12> kDepthCases{{
    {100, 102, 0, 10, 101},
    {0x7FF, 0x801, 0, 10, std::nullopt},
    {10, 11, 0, 10, std::nullopt},
    {100, 100, 1, 10, 0x7FE},
    {100, 100, 2, 10, -15},
    {100, 100, 3, 10, -13},
    {100, 100, 4, 10, -11},
    {100, 100, 5, 10, 84},
    {20, 20, 5, 10, 20},
    {100, 100, 6, 10, 100},
    {100, 100, -1, 10, 100},
    {-3, 0, 0, -10, -1},
}};
}  // namespace

TEST_CASE("Particle segment depths apply each bias inside the clip range",
          "[effects-particles][effects]") {
  for (const DepthCase& depth : kDepthCases) {
    CAPTURE(depth.first, depth.second, depth.bias, depth.near_clip);
    CHECK(particles::ParticleSegmentDepth(depth.first, depth.second, depth.bias,
                                          depth.near_clip) == depth.expected);
  }
}

TEST_CASE("Particle colours and jitter sample their curves at the set's age",
          "[effects-particles][effects]") {
  CHECK(particles::ParticleColorChannel(10) == 20);
  CHECK(particles::ParticleColorChannel(127) == 254);
  CHECK(particles::ParticleColorChannel(200) == 255);
  CHECK(particles::ParticleColorChannel(-1) == 254);
  FakeParticleHost host;
  host.curve_values[3] = 40;
  host.curve_values[4] = -1;
  ParticleSetFrame frame{};
  frame.state.age = 9;
  frame.inputs.definition.curves.color = {0, 3, 4};
  CHECK(particles::SampleParticleColor(frame.inputs.definition, 9, host) ==
        ParticleColor{128, 80, 254});
  CHECK(host.samples ==
        std::vector<std::pair<std::uint8_t, std::int32_t>>{{3, 9}, {4, 9}});
  frame.inputs.definition.inner_extent = {0, 128};
  frame.inputs.definition.curves.inner_extent = 5;
  CHECK(particles::SampleRibbonJitter(frame, host) == 1);
  host.curve_values[5] = 64;
  CHECK(particles::SampleRibbonJitter(frame, host) == 64);
}

TEST_CASE("Particle packets encode quads and lines between draw-mode words",
          "[effects-particles][effects]") {
  const ParticleQuad quad{
      .screens = {0x00100020, 0x00300040, 0x00500060, 0x00700080},
      .uvs = {ParticleUv{1, 2}, ParticleUv{3, 4}, ParticleUv{5, 6},
              ParticleUv{7, 8}},
      .color = {0x11, 0x22, 0x33},
      .clut = 0x1234,
      .tpage = 0x567};
  std::array<std::uint32_t, particles::kParticleQuadWords> packet{};
  packet.fill(0xAAAA0000U);
  particles::EncodeParticleQuadPacket(quad, 0xFF123456U, packet);
  CHECK(packet == std::array<std::uint32_t, particles::kParticleQuadWords>{
                      0x0E123456, 0xE1000000, 0x3E332211, 0x00100020,
                      0x12340201, 0x3E332211, 0x00300040, 0x05670403,
                      0x3E332211, 0x00500060, 0xAAAA0605, 0x3E332211,
                      0x00700080, 0xAAAA0807, 0xE1000200});
  const ParticleLine line{
      .screens = {0x10, 0x20}, .color = {1, 2, 3}, .blend = 3};
  CHECK(particles::EncodeParticleLinePacket(line, 0xABCDEF01U) ==
        std::array<std::uint32_t, particles::kParticleLineWords>{
            0x05CDEF01, 0xE1000060, 0x42030201, 0x10, 0x20, 0xE1000200});
}

TEST_CASE("Particle dispatch creates sets only for definitions it holds",
          "[effects-particles][effects]") {
  FakeParticleHost refused;
  refused.definition.reset();
  CHECK(Dispatch(ParticleCommand::kCreate, 0x0305, refused));
  CHECK(refused.definition_requests == std::vector<std::uint8_t>{3});
  CHECK_FALSE(refused.state.has_value());
  CHECK_FALSE(refused.created_capacity.has_value());
  FakeParticleHost host;
  Required(host.definition).interval = {3, 12};
  ParticleDispatchOptions textured = kRecordingOptions;
  textured.texturing = ParticleTexturing::kTextured;
  CHECK(particles::ParticleDispatch(textured, ParticleCommand::kCreate, 0x0205,
                                    host));
  CHECK(host.state == ParticleSetState{.lifetime = 5, .definition_index = 2});
  CHECK(host.created_capacity == 8);
  CHECK(host.created_texturing == ParticleTexturing::kTextured);
  CHECK(host.stages.empty());
}

namespace {
// A set of age 4 whose definition sets every flag, waits two ticks between
// placements and samples five ribbons.
void PrepareCooldownSet(FakeParticleHost& host) {
  host.state = ParticleSetState{.definition_index = 1, .age = 4};
  host.inputs.definition.flags = 0xFFFFFFFFU;
  host.inputs.definition.cooldown = 2;
  host.inputs.definition.interval = {5, 5};
}
}  // namespace

TEST_CASE("Particle dispatch ticks place points only when the cooldown ends",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  PrepareCooldownSet(host);
  CHECK(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(host.stages == "otcr");
  CHECK(host.staged_counts == std::vector<std::int16_t>{5});
  CHECK(host.state ==
        ParticleSetState{
            .definition_index = 1, .count = 5, .cooldown = 1, .age = 5});
  CHECK(Required(host.published).flags == 0x3FFFFFFU);
  CHECK(Required(host.published).vector == kStageMark);
  CHECK(host.points.sets.front()[1] == kStageMark);
}

TEST_CASE("Particle dispatch ticks within the cooldown only move the points",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  PrepareCooldownSet(host);
  CHECK(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(host.stages == "otcrcrotcr");
  CHECK(host.ticked_definitions == std::vector<std::uint8_t>{1, 1, 1});
  CHECK(Required(host.state).age == 7);
  CHECK(Required(host.state).lifetime == 0);
  CHECK(host.releases == 0);
}

TEST_CASE("Particle dispatch releases a set when its lifetime ends",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.state = ParticleSetState{.lifetime = 2};
  CHECK(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(Required(host.state).lifetime == 1);
  CHECK(host.releases == 0);
  CHECK_FALSE(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(Required(host.state).lifetime == 0);
  CHECK(Required(host.state).age == 2);
  CHECK(host.releases == 1);
}

TEST_CASE("Particle dispatch keeps an endless set until it expires",
          "[effects-particles][effects]") {
  FakeParticleHost endless;
  endless.state = ParticleSetState{};
  CHECK(Dispatch(ParticleCommand::kTick, 0, endless));
  CHECK(Dispatch(ParticleCommand::kExpire, 0, endless));
  CHECK(Required(endless.state).lifetime == 1);
  CHECK_FALSE(Dispatch(ParticleCommand::kTick, 0, endless));
  CHECK(endless.releases == 1);
}

TEST_CASE("Particle dispatch commands without a set stop or keep the slot",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  CHECK_FALSE(Dispatch(ParticleCommand::kTick, 0, host));
  CHECK(host.ticked_definitions.empty());
  CHECK(Dispatch(ParticleCommand::kExpire, 0, host));
  CHECK_FALSE(host.state.has_value());
  // An unknown renderer command, which the original's switch ignores.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(Dispatch(static_cast<ParticleCommand>(9), 0, host));
  CHECK_FALSE(Dispatch(ParticleCommand::kStop, 0, host));
  CHECK(host.releases == 1);
}

TEST_CASE("Particle dispatch stops a set whose tick the host refuses",
          "[effects-particles][effects]") {
  FakeParticleHost refused;
  refused.state = ParticleSetState{.lifetime = 3, .definition_index = 4};
  refused.refuse_tick = true;
  CHECK_FALSE(Dispatch(ParticleCommand::kTick, 0, refused));
  CHECK(refused.ticked_definitions == std::vector<std::uint8_t>{4});
  CHECK(refused.releases == 1);
  CHECK(refused.stages.empty());
  CHECK(Required(refused.state).lifetime == 3);
  CHECK_FALSE(refused.published.has_value());
}

namespace {
constexpr Vec3 kEffectOrigin{1000, 2000, 3000};
constexpr Vec3 kWorldTranslation{-5, -6, -7};
constexpr std::int32_t kQuarterTurn = 0x400;

// Two ribbons whose origins sample (10, 20, 30) within a 400 x 200 x 100
// extent.
ParticleSetFrame OriginFrame(std::uint32_t flags) {
  ParticleSetFrame frame{};
  ParticleSetDefinition& definition = frame.inputs.definition;
  definition.position = {{{10, 10}, {20, 20}, {30, 30}}};
  definition.extent = {{{400, 400}, {200, 200}, {100, 100}}};
  frame.inputs.effect_origin = kEffectOrigin;
  frame.inputs.world.translation = kWorldTranslation;
  frame.scratch.flags = flags;
  frame.state.count = 2;
  frame.points.capacity = kParticleSetCapacity;
  return frame;
}

[[nodiscard]] ParticlePoint Moved(ParticlePoint point, Vec3 by) {
  return particles::ParticlePointAt(
      {point.x + by.x, point.y + by.y, point.z + by.z});
}

constexpr Vec3 kOriginPosition{10, 20, 30};

// The ring position `index` of `segments` round, an eighth turn on, scaled
// by OriginFrame's extent.
[[nodiscard]] ParticlePoint RingOffset(std::int32_t index,
                                       std::int32_t segments) {
  const std::int32_t angle = (index * (0x1000 / segments)) + 0x200;
  return {static_cast<std::int16_t>((math::Cosine(angle) * 400) >> 12), 0,
          static_cast<std::int16_t>((math::Sine(angle) * 100) >> 12)};
}

std::array<ParticlePoint, 2> Origins(const ParticleSetFrame& frame) {
  return {frame.points.sets[0][0], frame.points.sets[1][0]};
}
}  // namespace

TEST_CASE("Particle origins place sphere points on the extent's ellipsoid",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.scripted_rands = {0, 0, kQuarterTurn, kQuarterTurn};
  ParticleSetFrame frame = OriginFrame(Flags(ParticleAnchor::kEffectOrigin));
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg045, frame, host);
  CHECK(frame.scratch.position == Vec3{1010, 2020, 3030});
  CHECK(frame.scratch.extent == Vec3{400, 200, 100});
  CHECK(Origins(frame) ==
        std::array<ParticlePoint, 2>{ParticlePoint{1410, 2020, 3030},
                                     ParticlePoint{1010, 2020, 3130}});
  CHECK(frame.points.sets[2][0] == ParticlePoint{});
  CHECK(frame.scratch.sine == std::array<std::int32_t, 2>{4096, 4096});
  CHECK(frame.scratch.cosine == std::array<std::int32_t, 2>{0, 0});
  CHECK(host.rand_draws == 4);
  CHECK(host.stored_counters.empty());
  ParticleSetFrame anchored = OriginFrame(Flags(ParticleAnchor::kTransform));
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg045, anchored, host);
  CHECK(anchored.scratch.position == Vec3{5, 14, 23});
}

TEST_CASE("Particle origins place box points on random faces",
          "[effects-particles][effects]") {
  const auto placement =
      GENERATE(ParticlePlacement::kRandom, ParticlePlacement::kRandomAlternate);
  FakeParticleHost host;
  host.counters.at(Index(ParticleEmissionCounter::kPlg147)) = 5;
  host.scripted_rands = {0, 9, 4};
  ParticleSetFrame frame =
      OriginFrame(Flags(ParticleShape::kBox) | Flags(placement));
  frame.state.count = 3;
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg147, frame, host);
  CHECK(frame.points.sets[0][0] == ParticlePoint{410, -180, -70});
  CHECK(frame.points.sets[1][0] == ParticlePoint{-390, -180, -70});
  CHECK(frame.points.sets[2][0] == ParticlePoint{-390, -180, 130});
  CHECK(host.range_draws == std::vector<RangeDraw>{{200, -200},
                                                   {100, -100},
                                                   {400, -400},
                                                   {100, -100},
                                                   {400, -400},
                                                   {200, -200}});
  CHECK(host.counters.at(Index(ParticleEmissionCounter::kPlg147)) == 5);
  CHECK(host.stored_counters == std::vector{ParticleEmissionCounter::kPlg147});
}

TEST_CASE("Particle origins step box corners through the overlay's cursor",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.counters.at(Index(ParticleEmissionCounter::kPlg224)) = 6;
  ParticleSetFrame frame = OriginFrame(Flags(ParticleShape::kBox) |
                                       Flags(ParticlePlacement::kCorners));
  frame.state.count = 3;
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg224, frame, host);
  CHECK(frame.points.sets[0][0] == ParticlePoint{410, 220, 130});
  CHECK(frame.points.sets[1][0] == ParticlePoint{-390, -180, -70});
  CHECK(frame.points.sets[2][0] == ParticlePoint{410, -180, -70});
  CHECK(host.counters.at(Index(ParticleEmissionCounter::kPlg224)) == 1);
  CHECK(host.stored_counters == std::vector{ParticleEmissionCounter::kPlg224});
  CHECK(host.rand_draws == 0);
}

TEST_CASE("Particle origins offset fixed box points from the set's position",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.counters.at(Index(ParticleEmissionCounter::kPlg224)) = 6;
  ParticleSetFrame fixed = OriginFrame(Flags(ParticleShape::kBox) |
                                       Flags(ParticlePlacement::kFixed));
  fixed.points.sets[0][0] = {1, 2, 3};
  fixed.points.sets[1][0] = {4, 5, 6};
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg224, fixed, host);
  CHECK(Origins(fixed) ==
        std::array<ParticlePoint, 2>{ParticlePoint{11, 22, 33},
                                     ParticlePoint{14, 25, 36}});
  CHECK(host.counters.at(Index(ParticleEmissionCounter::kPlg224)) == 6);
  CHECK(host.stored_counters == std::vector{ParticleEmissionCounter::kPlg224});
  CHECK(host.rand_draws == 0);
}

TEST_CASE("Particle origins turn offsets by the world rotation when asked",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.counters.at(Index(ParticleEmissionCounter::kPlg156)) = 6;
  ParticleSetFrame frame = OriginFrame(
      Flags(ParticleAnchor::kTransform) | Flags(ParticleShape::kBox) |
      Flags(ParticlePlacement::kCorners) | particles::kParticleRotateOffsets);
  frame.state.count = 1;
  for (std::size_t axis = 0; axis < particles::kParticleAxes; ++axis) {
    frame.inputs.world.rotation.m.at(axis).at(axis) = -4096;
  }
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg156, frame, host);
  CHECK(frame.scratch.aim_point == Vec3{-400, -200, -100});
  CHECK(frame.points.sets[0][0] == ParticlePoint{-395, -186, -77});
}

TEST_CASE("Particle origins place ring corners on successive ring positions",
          "[effects-particles][effects]") {
  using enum ParticleEmissionCounter;
  FakeParticleHost host;
  host.counters.at(Index(kPlg065)) = 1;
  ParticleSetFrame four = OriginFrame(Flags(ParticleShape::kRingA) |
                                      Flags(ParticlePlacement::kCorners));
  particles::ParticleOrigin(kPlg065, four, host);
  CHECK(Origins(four) ==
        std::array<ParticlePoint, 2>{Moved(RingOffset(2, 4), kOriginPosition),
                                     Moved(RingOffset(3, 4), kOriginPosition)});
  CHECK(host.counters.at(Index(kPlg065)) == 3);
  CHECK(four.scratch.ring_segments == 4);
  CHECK(four.scratch.ring_positions == 4);
  CHECK(host.rand_draws == 0);
}

TEST_CASE("Particle origins place a custom ring's points on its one position",
          "[effects-particles][effects]") {
  using enum ParticleEmissionCounter;
  FakeParticleHost host;
  host.counters.at(Index(kPlg265Second)) = 5;
  ParticleSetFrame single = OriginFrame(Flags(ParticleShape::kCustomRing));
  particles::ParticleOrigin(kPlg265Second, single, host);
  const ParticlePoint only = Moved(RingOffset(0, 1), kOriginPosition);
  CHECK(Origins(single) == std::array<ParticlePoint, 2>{only, only});
  CHECK(single.scratch.ring_segments == 1);
  CHECK(single.scratch.ring_positions == 1);
  CHECK(host.counters.at(Index(kPlg265Second)) == 0);
  CHECK(host.rand_draws == 2);
}

TEST_CASE("Particle origins weigh ring points between the cursor's positions",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.counters.at(Index(ParticleEmissionCounter::kPlg266Second)) = 4;
  host.scripted_rands = {0x4000};
  ParticleSetFrame frame = OriginFrame(Flags(ParticleShape::kCustomRing));
  frame.inputs.definition.ring_segments = 8;
  frame.inputs.definition.ring_positions = 3;
  frame.state.count = 1;
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg266Second, frame,
                            host);
  const ParticlePoint start = RingOffset(1, 8);
  const ParticlePoint end = RingOffset(2, 8);
  const ParticlePoint halfway{
      static_cast<std::int16_t>(start.x + (((end.x - start.x) * 0x800) >> 12)),
      0,
      static_cast<std::int16_t>(start.z + (((end.z - start.z) * 0x800) >> 12))};
  CHECK(frame.points.sets[0][0] == Moved(halfway, kOriginPosition));
  CHECK(host.counters.at(Index(ParticleEmissionCounter::kPlg266Second)) == 2);
  ParticleSetFrame unused = OriginFrame(particles::kParticleShapeMask);
  particles::ParticleOrigin(ParticleEmissionCounter::kPlg266Second, unused,
                            host);
  CHECK(Origins(unused) == std::array<ParticlePoint, 2>{});
  CHECK(host.stored_counters.size() == 1);
  CHECK(host.rand_draws == 1);
}

namespace {
struct TargetCase {
  std::uint32_t flags{};
  ParticlePoint expected;
};
}  // namespace

TEST_CASE("Particle targets anchor and jitter every ribbon's last point",
          "[effects-particles][effects]") {
  const TargetCase target = GENERATE(values<TargetCase>({
      {0, {95, 200, 293}},
      {0x40, {95, 200, 293}},
      {0x80, {1095, 2200, 3293}},
      {0xC0, {45, 140, 223}},
  }));
  CAPTURE(target.flags);
  FakeParticleHost host;
  ParticleSetFrame frame = OriginFrame(target.flags);
  frame.inputs.definition.target_offset = {
      {{100, 100}, {200, 200}, {300, 300}}};
  frame.inputs.definition.target_jitter = {{{5, 5}, {0, 0}, {7, 7}}};
  frame.inputs.target_translation = {-50, -60, -70};
  particles::ParticleTarget(frame, host);
  CHECK(frame.points.sets[0].back() == target.expected);
  CHECK(frame.points.sets[1].back() == target.expected);
  CHECK(frame.points.sets[2].back() == ParticlePoint{});
  CHECK(frame.scratch.target_jitter == ParticlePoint{5, 0, 7});
  CHECK(Moved(frame.scratch.target_offset, {-5, 0, -7}) == target.expected);
  CHECK(host.range_draws.size() == 6);
  CHECK(host.range_draws.front() == RangeDraw{5, -5});
}

namespace {
// One ribbon running 400 along z from (10, 20, 30). The original's table
// square root measures that span as 399 and its look-at normalizes the
// forward axis to 4098, so the inner depths land a little short of exact.
ParticleSetFrame ControlFrame() {
  ParticleSetFrame frame{};
  frame.state.count = 1;
  frame.points.capacity = 1;
  frame.points.sets[0] = {ParticlePoint{10, 20, 30}, ParticlePoint{},
                          ParticlePoint{}, ParticlePoint{10, 20, 430}};
  frame.inputs.definition.speed = {{{0x80, 0x80}, {0, 0}}};
  return frame;
}
}  // namespace

TEST_CASE("Particle controls place the inner points along the ribbon",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  ParticleSetFrame frame = ControlFrame();
  particles::ParticleControl(frame, host);
  CHECK(frame.points.sets[0][1] == ParticlePoint{10, 20, 229});
  CHECK(frame.points.sets[0][2] == ParticlePoint{10, 20, 429});
  CHECK(frame.scratch.speed == Vec2{0x80, 0});
  CHECK(frame.scratch.spawn_point == Vec3{10, 20, 429});
  CHECK(host.rand_draws == 0);
}

TEST_CASE("Particle control draws follow the original's speed radius order",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  host.scripted_rands = {3, 0, 0, 0, 0, 0};
  ParticleSetFrame frame = ControlFrame();
  ParticleSetDefinition& definition = frame.inputs.definition;
  definition.speed_jitter = {{{4, 4}, {6, 6}, {0, 0}}};
  definition.first_radius = {{{0, 0}, {3, 3}}};
  definition.second_radius = {{{0, 0}, {5, 5}}};
  definition.direction_jitter = {{{2, 2}, {3, 3}, {0, 0}}};
  particles::ParticleControl(frame, host);
  CHECK(frame.points.sets[0][1] == ParticlePoint{10, 20, 231});
  CHECK(frame.points.sets[0][2] == ParticlePoint{10, 20, 425});
  CHECK(host.rand_draws == 6);
  CHECK(frame.scratch.speed_jitter == ParticlePoint{4, 6, 0});
  CHECK(frame.scratch.first_radius == Vec2{0, 3});
  CHECK(frame.scratch.second_radius == Vec2{0, 5});
  CHECK(frame.scratch.direction_jitter == Vec3{2, 3, 0});
}

namespace {
ParticleSetFrame RibbonFrame(std::int16_t count) {
  ParticleSetFrame frame{};
  frame.state.count = count;
  frame.points.capacity = 3;
  return frame;
}

struct RibbonCase {
  std::int32_t initial_width{};
  std::int32_t half_width{};
  std::int16_t count{};
};

// The first ribbon's phases jitter within the inner extent (1 for 0), later
// ribbons' within the previous ribbon's last half width; a zero jitter keeps
// the whole draw, as the R3000 divide leaves it.
void CheckRibbonSamples(FakeParticleHost& host, const RibbonCase& ribbon_case) {
  constexpr std::int32_t kCurveEnd = 0x3000;
  constexpr std::int32_t kSegments = kParticleSegments;
  REQUIRE(host.quads.size() ==
          static_cast<std::size_t>(ribbon_case.count) * kParticleSegments);
  const ParticleTexture texture = Required(host.texture);
  VS::Core::Rand15 expected(kSeed);
  for (std::int32_t ribbon = 0; ribbon < ribbon_case.count; ++ribbon) {
    const std::int32_t first =
        ribbon_case.initial_width == 0 ? 1 : ribbon_case.initial_width;
    const std::int32_t width = ribbon == 0 ? first : ribbon_case.half_width;
    std::int32_t previous = 0;
    for (std::int32_t segment = 0; segment < kSegments; ++segment) {
      std::int32_t sample = kCurveEnd;
      if (segment + 1 < kSegments) {
        const std::int32_t random = expected.Next();
        sample = ((segment + 1) * 0x600) +
                 (width == 0 ? random : random % width) - (width / 2);
      }
      const std::size_t index =
          (static_cast<std::size_t>(ribbon) * kParticleSegments) +
          static_cast<std::size_t>(segment);
      const ParticleQuad& quad = host.quads.at(index).quad;
      CHECK(quad.uvs[0].u == texture.u + (texture.w * previous / kCurveEnd));
      CHECK(quad.uvs[1].u == texture.u + (texture.w * sample / kCurveEnd));
      previous = sample;
    }
  }
  CHECK(host.random.Next() == expected.Next());
  CHECK(host.texture_advances == 1);
}

void CheckRibbonQuad(const QuadSubmission& submission, std::size_t segment) {
  INFO("segment " << segment);
  const ParticleQuad& quad = submission.quad;
  const auto trail = static_cast<std::int32_t>(segment * kScreenStep);
  const auto lead = static_cast<std::int32_t>((segment + 1) * kScreenStep);
  CHECK(quad.screens == std::array{Screen(trail, kScreenRow - 4),
                                   Screen(lead, kScreenRow - 4),
                                   Screen(trail, kScreenRow + 4),
                                   Screen(lead, kScreenRow + 4)});
  const auto trail_u = static_cast<std::uint8_t>(16 + segment);
  const auto lead_u = static_cast<std::uint8_t>(trail_u + 1);
  CHECK(quad.uvs == std::array{ParticleUv{trail_u, 32}, ParticleUv{lead_u, 32},
                               ParticleUv{trail_u, 40},
                               ParticleUv{lead_u, 40}});
  CHECK(quad.color == ParticleColor{128, 128, 128});
  CHECK(quad.clut == 0x1234);
  CHECK(quad.tpage == 0x567);
  CHECK(submission.depth == kDepth);
}
}  // namespace

TEST_CASE("Particle ribbons preserve the original zero-width remainder and RNG",
          "[effects-particles][effects]") {
  const ParticleStage render =
      GENERATE(&fx::Plg045::ParticleRender, &fx::Plg057::ParticleRender,
               &fx::Plg082::ParticleRender, &fx::Plg114::ParticleRender,
               &fx::Plg152::ParticleRender, &fx::Plg153::ParticleRender,
               &fx::Plg156::ParticleRender, &fx::Plg263::ParticleRender,
               &fx::Plg265::ParticleRender, &fx::Plg266::ParticleRender,
               &fx::Plg300::ParticleRender, &fx::Plg301::ParticleRender);
  const std::int32_t initial_width = GENERATE(0, 16, -16);
  const std::int32_t half_width = GENERATE(0, 6);
  const std::int16_t count = GENERATE(as<std::int16_t>{}, 2, 1, 0, 3);
  CAPTURE(initial_width, half_width, count);
  FakeParticleHost host;
  ParticleSetFrame frame = RibbonFrame(count);
  frame.inputs.definition.inner_extent = {initial_width, initial_width};
  frame.inputs.definition.width = {
      {{half_width * 2, half_width * 2}, {0, 0}, {0, 0}}};
  render(frame, host);
  CheckRibbonSamples(host, {initial_width, half_width, count});
}

TEST_CASE("Particle ribbon quads share edges across each projected point",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  ParticleSetFrame frame = RibbonFrame(1);
  frame.inputs.definition.width = {{{8, 8}, {0, 0}, {0, 0}}};
  particles::ParticleRender(frame, host);
  REQUIRE(host.quads.size() == kParticleSegments);
  CHECK(frame.scratch.width == ParticlePoint{8, 0, 0});
  CHECK(host.effect_view == false);
  CHECK(host.projected.size() == kParticleProjections);
  CHECK(host.rand_draws == kParticleSegments - 1);
  for (std::size_t segment = 0; segment < kParticleSegments; ++segment) {
    CheckRibbonQuad(host.quads[segment], segment);
  }
  FakeParticleHost back;
  ParticleSetFrame biased = RibbonFrame(1);
  biased.inputs.definition.depth_bias = 1;
  particles::ParticleRender(biased, back);
  REQUIRE(back.quads.size() == kParticleSegments);
  CHECK(back.quads.back().depth == 0x7FE);
}

TEST_CASE("Particle ribbons draw nothing for an untextured set",
          "[effects-particles][effects]") {
  FakeParticleHost host;
  ParticleSetFrame frame = RibbonFrame(1);
  frame.scratch.flags = particles::kParticleEffectView;
  frame.texturing = ParticleTexturing::kUntextured;
  frame.inputs.definition.width = {{{8, 8}, {6, 6}, {0, 0}}};
  particles::ParticleRender(frame, host);
  CHECK(host.effect_view == true);
  CHECK(frame.scratch.width == ParticlePoint{8, 6, 0});
  CHECK(host.texture_advances == 0);
  CHECK(host.projected.empty());
}

TEST_CASE("Particle ribbons advance but draw nothing without a texture",
          "[effects-particles][effects]") {
  FakeParticleHost bare;
  bare.texture.reset();
  ParticleSetFrame textured = RibbonFrame(1);
  particles::ParticleRender(textured, bare);
  CHECK(bare.texture_advances == 1);
  CHECK(bare.projected.empty());
  CHECK(bare.quads.empty());
}

TEST_CASE("Particle ribbons drop segments at or behind the near clip",
          "[effects-particles][effects]") {
  FakeParticleHost clipped;
  ParticleSetFrame near = RibbonFrame(1);
  near.inputs.near_clip = kDepth;
  particles::ParticleRender(near, clipped);
  CHECK(clipped.projected.size() == kParticleProjections);
  CHECK(clipped.quads.empty());
}

TEST_CASE(
    "Particle line ribbons draw eight lines per ribbon in the set's blend",
    "[effects-particles][effects]") {
  const ParticleStage render = GENERATE(
      &fx::Plg065::ParticleLineRender, &fx::Plg087::ParticleLineRender,
      &fx::Plg114::ParticleLineRender, &fx::Plg146::ParticleLineRender,
      &fx::Plg147::ParticleLineRender, &fx::Plg168::ParticleLineRender,
      &fx::Plg224::ParticleLineRender, &fx::Plg255::ParticleLineRender,
      &fx::Plg265::ParticleLineRender, &fx::Plg266::ParticleLineRender);
  FakeParticleHost host;
  ParticleSetFrame frame = RibbonFrame(2);
  frame.texturing = ParticleTexturing::kUntextured;
  frame.inputs.definition.blend = 2;
  render(frame, host);
  REQUIRE(host.lines.size() == 2 * kParticleSegments);
  CHECK(host.texture_advances == 0);
  CHECK(host.rand_draws == 2 * (kParticleSegments - 1));
  for (std::size_t index = 0; index < host.lines.size(); ++index) {
    const std::size_t segment = index % kParticleSegments;
    const LineSubmission& submission = host.lines[index];
    CHECK(submission.line.screens ==
          std::array{PointScreen(segment), PointScreen(segment + 1)});
    CHECK(submission.line.color == ParticleColor{128, 128, 128});
    CHECK(submission.line.blend == 2);
    CHECK(submission.depth == kDepth);
  }
}

namespace {
constexpr ParticleControlSet kCurvePoints{
    ParticlePoint{0, 0, 0},
    ParticlePoint{100, 0, 0},
    ParticlePoint{200, 50, 0},
    ParticlePoint{300, 0, 0},
};
}  // namespace

TEST_CASE("Particle line ribbons jitter every ribbon within the inner extent",
          "[effects-particles][effects]") {
  CHECK(particles::ParticleRibbonPoint(kCurvePoints, 0) == kCurvePoints[1]);
  CHECK(particles::ParticleRibbonPoint(kCurvePoints, 0x1000) ==
        kCurvePoints[2]);
  FakeParticleHost host;
  ParticleSetFrame frame = RibbonFrame(2);
  frame.inputs.definition.inner_extent = {6, 6};
  frame.points.sets[0] = kCurvePoints;
  frame.points.sets[1] = kCurvePoints;
  particles::ParticleLineRender(frame, host);
  REQUIRE(host.projected.size() == 2 * kParticleProjections);
  VS::Core::Rand15 expected(kSeed);
  for (std::size_t ribbon = 0; ribbon < 2; ++ribbon) {
    const std::size_t base = ribbon * kParticleProjections;
    CHECK(host.projected[base] == kCurvePoints.front());
    for (std::size_t segment = 1; segment < kParticleSegments; ++segment) {
      const std::int32_t phase = (static_cast<std::int32_t>(segment) * 0x600) -
                                 0x1000 + (expected.Next() % 6) - 3;
      CHECK(host.projected[base + segment] ==
            particles::ParticleRibbonPoint(kCurvePoints, phase));
    }
    CHECK(host.projected[base + kParticleSegments] == kCurvePoints.back());
  }
  CHECK(frame.scratch.vector == host.projected[(2 * kParticleProjections) - 2]);
}

namespace {
// Points the effect globals at an empty package and a bare effect instance
// for one test and collects the effects channel's warnings.
struct LiveParticleScope {
  Effects_EffectPackage package{};
  Effects_EffectInstance instance{};
  Effects_EffectPackage* saved_package = Effects_package;
  Effects_EffectInstance* saved_instance = Effects_currentInstance;
  std::vector<std::string> warnings;
  VS::Core::LogObserver previous;

  LiveParticleScope()
      : previous(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.channel == "effects") {
                warnings.emplace_back(record.message);
              }
            })) {
    Effects_package = &package;
    Effects_currentInstance = &instance;
    VS::Effects::ForgetLivePackageExtents();
  }
  LiveParticleScope(const LiveParticleScope&) = delete;
  LiveParticleScope& operator=(const LiveParticleScope&) = delete;
  LiveParticleScope(LiveParticleScope&&) = delete;
  LiveParticleScope& operator=(LiveParticleScope&&) = delete;
  ~LiveParticleScope() {
    VS::Core::SetLogObserver(std::move(previous));
    Effects_package = saved_package;
    Effects_currentInstance = saved_instance;
  }

  [[nodiscard]] bool Warned(const std::string& text) const {
    return std::ranges::any_of(warnings, [&text](const std::string& warning) {
      return warning.find(text) != std::string::npos;
    });
  }
};
}  // namespace

TEST_CASE("Live particle entries refuse definitions the package lacks",
          "[effects-particles][effects]") {
  const LiveParticleScope scope;
  Effects_RendererSlot slot{};
  CHECK(particles::UpdateLivePlg045ParticleDispatch(&slot, 1, 0x0105) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(scope.Warned("definition 1 is outside"));
}

TEST_CASE("Live particle entries stop sets their package no longer holds",
          "[effects-particles][effects]") {
  const LiveParticleScope scope;
  Effects_ParticleQuadSet live{};
  live.definitionIndex = 1;
  live.opaque0 = 3;
  Effects_RendererSlot slot{};
  slot.rendererState = &live;
  CHECK(particles::UpdateLivePlg065ParticleDispatch(&slot, 2, 0) == 0);
  CHECK(scope.Warned("no longer holds definition 1"));
  CHECK(particles::UpdateLivePlg065ParticleDispatch(&slot, 3, 0) == 1);
  CHECK(live.opaque0 == 1);
  Effects_currentInstance = nullptr;
  CHECK(particles::UpdateLivePlg152ParticleDispatch(&slot, 2, 0) == 0);
  CHECK(scope.Warned("no current effect instance"));
  CHECK(live.age == 0);
}

TEST_CASE("Live particle entries answer commands without a set state",
          "[effects-particles][effects]") {
  const LiveParticleScope scope;
  Effects_RendererSlot slot{};
  CHECK(particles::UpdateLivePlg114ParticleDispatch2(&slot, 3, 0) == 1);
  CHECK(particles::UpdateLivePlg265ParticleDispatch(&slot, 9, 0) == 1);
  CHECK(particles::UpdateLivePlg300ParticleDispatch(&slot, 2, 0) == 0);
  CHECK(particles::UpdateLivePlg266ParticleDispatch2(&slot, 4, 0) == 0);
  CHECK(particles::UpdateLivePlg301ParticleDispatch(nullptr, 2, 0) == 0);
  CHECK(slot.rendererState == nullptr);
}
