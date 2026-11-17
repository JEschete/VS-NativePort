#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/Templates/RepeatingRings.hpp"

namespace {
namespace rings = VS::Effects::Templates;
using rings::RingCommand;
using rings::RingState;
using VS::Core::Math::Vec3;
using Instance = bool (*)(RingCommand, std::int32_t,
                          std::span<const rings::RingPaletteEntry>,
                          rings::RingHost&);

template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) throw std::logic_error("REQUIRE did not stop the test");
  return *value;
}

struct Submission {
  rings::RingQuad quad{};
  std::int32_t depth{};
};
struct Event {
  std::uint8_t index{};
  Vec3 position{};
  friend bool operator==(const Event&, const Event&) = default;
};

class RingHost final : public rings::RingHost {
 public:
  std::optional<RingState> state{RingState{}};
  rings::RingTickInputs inputs{};
  std::array<std::int32_t, 33> curves{};
  std::optional<rings::RingTextureSource> texture_source;
  std::optional<rings::RingScratch> scratch;
  std::optional<bool> effect_view;
  std::vector<rings::RingPoint> points;
  std::vector<Submission> quads;
  std::vector<Event> events;
  std::int32_t depth = 100;
  bool refuse_tick = false;
  bool refuse_texture = false;
  bool refuse_create = false;
  RingHost() {
    inputs.definition.extent = {VS::Core::Math::ScalarSpan{100, 100},
                                VS::Core::Math::ScalarSpan{10, 10},
                                VS::Core::Math::ScalarSpan{}};
    inputs.definition.inner_extent = {2048, 2048};
    inputs.near_clip = 32;
  }
  void Create(const RingState& value,
              rings::RingTextureSource source) override {
    if (refuse_create) return;
    state = value;
    texture_source = source;
  }
  [[nodiscard]] std::optional<RingState> State() const override {
    return state;
  }
  void StoreState(const RingState& value) override { state = value; }
  [[nodiscard]] std::optional<rings::RingTickInputs> BeginTick(
      std::uint8_t /*index*/) override {
    if (refuse_tick) return std::nullopt;
    return inputs;
  }
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t /*age*/) override {
    return curves.at(curve);
  }
  [[nodiscard]] std::int32_t Rand() override { return 7; }
  void PublishScratch(const rings::RingScratch& value) override {
    scratch = value;
  }
  void BeginProjection(bool effect) override { effect_view = effect; }
  [[nodiscard]] rings::RingProjection Project(rings::RingPoint point) override {
    points.push_back(point);
    return {static_cast<std::uint32_t>(points.size()), depth};
  }
  [[nodiscard]] std::optional<rings::RingTexture> AdvanceTexture() override {
    if (refuse_texture) return std::nullopt;
    return rings::RingTexture{
        .clut = 0x1234, .tpage = 0x567, .u = 16, .v = 32, .w = 64, .h = 128};
  }
  void Submit(const rings::RingQuad& quad, std::int32_t at) override {
    quads.push_back({quad, at});
  }
  [[nodiscard]] Vec3 TransformByActor(rings::RingPoint point) override {
    return {point.x + 10, point.y - 20, point.z + 30};
  }
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override {
    events.push_back({event, position});
  }
};

using Palette = std::array<rings::RingPaletteEntry, 8>;
const Palette kPalette{rings::RingPaletteEntry{{10, 20, 30}}};

void CheckGeometry(Instance instance, rings::RepeatingRingStyle style) {
  RingHost host;
  REQUIRE(instance(RingCommand::kTick, 0, kPalette, host));
  REQUIRE(host.quads.size() == 32);
  REQUIRE(host.points.size() == 48);
  CHECK(host.points[0] == rings::RingPoint{100, 0, 0});
  const bool transposed = style == rings::RepeatingRingStyle::kTransposed;
  const std::array<std::uint32_t, 4> first =
      transposed ? std::array<std::uint32_t, 4>{1, 17, 2, 18}
                 : std::array<std::uint32_t, 4>{1, 2, 17, 18};
  const std::array<std::uint32_t, 4> seam =
      transposed ? std::array<std::uint32_t, 4>{16, 32, 1, 17}
                 : std::array<std::uint32_t, 4>{16, 1, 32, 17};
  CHECK(host.quads[0].quad.screens == first);
  CHECK(host.quads[15].quad.screens == seam);
  const rings::RingQuadUvs uvs =
      transposed ? rings::RingQuadUvs{{{16, 32}, {48, 32}, {16, 40}, {48, 40}}}
                 : rings::RingQuadUvs{{{16, 32}, {32, 32}, {16, 96}, {32, 96}}};
  CHECK(host.quads[0].quad.uvs == uvs);
  CHECK(host.quads[0].quad.colors[1].r == (transposed ? 20 : 10));
  CHECK(host.quads[0].depth ==
        (style == rings::RepeatingRingStyle::kForeground ? -15 : 100));
}

void CheckLifecycle(Instance instance) {
  RingHost host;
  host.state.reset();
  CHECK(instance(RingCommand::kCreate, 0x12340302, kPalette, host));
  CHECK(host.state == RingState{.lifetime = 2, .definition_index = 3});
  CHECK(host.texture_source == rings::RingTextureSource::kSegmentsAndPositions);
  CHECK(instance(RingCommand::kTick, 0, kPalette, host));
  CHECK(Required(host.state).lifetime == 1);
  CHECK_FALSE(instance(RingCommand::kTick, 0, kPalette, host));
  CHECK(Required(host.state).age == 2);
  CHECK_FALSE(instance(RingCommand::kStop, 0, kPalette, host));
  // The slot runtime, not a renderer callback, owns state destruction.
  CHECK(host.state.has_value());
}

void CheckChildAndExpiry(Instance instance) {
  RingHost host;
  host.inputs.definition.position = {VS::Core::Math::ScalarSpan{11, 11},
                                     VS::Core::Math::ScalarSpan{22, 22},
                                     VS::Core::Math::ScalarSpan{33, 33}};
  host.inputs.definition.flags = rings::kRingEffectView;
  host.inputs.definition.tick_event = 7;
  CHECK(instance(RingCommand::kExpire, 0, kPalette, host));
  CHECK_FALSE(instance(RingCommand::kTick, 0, kPalette, host));
  CHECK(host.events ==
        std::vector<Event>{{6, {21 * 4096, 2 * 4096, 63 * 4096}}});
  CHECK(host.effect_view == true);
  CHECK(Required(host.state).age == 1);
  CHECK(Required(host.scratch).position == Vec3{11, 22, 33});
}

void CheckRefusals(Instance instance) {
  RingHost host;
  SECTION("no bound palette is not replaced with black") {
    CHECK_FALSE(instance(RingCommand::kTick, 0, {}, host));
  }
  SECTION("missing renderer state") {
    host.state.reset();
    CHECK_FALSE(instance(RingCommand::kTick, 0, kPalette, host));
  }
  SECTION("host refuses unbound definition") {
    host.refuse_tick = true;
    CHECK_FALSE(instance(RingCommand::kTick, 0, kPalette, host));
    CHECK(Required(host.state).age == 0);
  }
  CHECK(host.quads.empty());
  CHECK(host.events.empty());
}

void CheckCreateRefusal(Instance instance) {
  RingHost host;
  host.state.reset();
  host.refuse_create = true;
  CHECK(instance(RingCommand::kCreate, 0x0102, kPalette, host));
  CHECK_FALSE(host.state.has_value());
  CHECK(host.quads.empty());
  CHECK(host.events.empty());
}

void CheckDepthAndMissingTexture(Instance instance) {
  SECTION("near and far limits still use projected depth") {
    for (const int depth : {32, 33, 2047, 2048}) {
      RingHost host;
      host.depth = depth;
      REQUIRE(instance(RingCommand::kTick, 0, kPalette, host));
      CHECK(host.quads.size() == (depth == 33 || depth == 2047 ? 32 : 0));
    }
  }
  SECTION("missing texture does not suppress lifetime or child emission") {
    RingHost host;
    host.refuse_texture = true;
    host.inputs.definition.tick_event = 1;
    CHECK(instance(RingCommand::kTick, 0, kPalette, host));
    CHECK(host.quads.empty());
    CHECK(host.events.size() == 1);
    CHECK(Required(host.state).age == 1);
  }
}

void CheckInstance(Instance instance, rings::RepeatingRingStyle style) {
  SECTION("closed geometry UV corners colors and ordering") {
    CheckGeometry(instance, style);
  }
  SECTION("create tick stop lifetime and external cleanup") {
    CheckLifecycle(instance);
  }
  SECTION("expire and transformed child event") {
    CheckChildAndExpiry(instance);
  }
  SECTION("checked host refusal") { CheckRefusals(instance); }
  SECTION("allocation refusal leaves no state for the checked Live wrapper") {
    CheckCreateRefusal(instance);
  }
  SECTION("depth boundaries and missing texture") {
    CheckDepthAndMissingTexture(instance);
  }
}
}  // namespace

TEST_CASE("F01920 PLG052 repeating ring independent lifecycle and geometry",
          "[F01920][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg052::RepeatingRing,
                rings::RepeatingRingStyle::kRepeating);
}
TEST_CASE("F01998 PLG077 repeating ring independent lifecycle and geometry",
          "[F01998][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg077::RepeatingRing,
                rings::RepeatingRingStyle::kRepeating);
}
TEST_CASE("F02209 PLG162 transposed ring independent lifecycle and geometry",
          "[F02209][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg162::TransposedRing,
                rings::RepeatingRingStyle::kTransposed);
}
TEST_CASE("F02289 PLG236 transposed ring independent lifecycle and geometry",
          "[F02289][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg236::TransposedRing,
                rings::RepeatingRingStyle::kTransposed);
}
TEST_CASE("F02323 PLG263 transposed ring independent lifecycle and geometry",
          "[F02323][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg263::TransposedRing,
                rings::RepeatingRingStyle::kTransposed);
}
TEST_CASE("F02397 PLG300 repeating ring independent lifecycle and geometry",
          "[F02397][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg300::RepeatingRing,
                rings::RepeatingRingStyle::kRepeating);
}
TEST_CASE("F02398 PLG300 foreground ring independent lifecycle and geometry",
          "[F02398][emitter-rings][effects]") {
  CheckInstance(&VS::Effects::Plg300::ForegroundRepeatingRing,
                rings::RepeatingRingStyle::kForeground);
}
