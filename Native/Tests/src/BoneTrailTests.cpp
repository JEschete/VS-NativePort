#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ModelDrawing.hpp"
#include "Effects/BoneTrail.hpp"
#include "Effects/BoneTrailLive.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/RingCore.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/LegacyPackets.hpp"

namespace {
namespace fx = VS::Effects;
using VS::Actors::BoneEndpoint;
using VS::Effects::RendererCommand;
using VS::Effects::Templates::RingColor;
using VS::Effects::Templates::RingQuad;
using VS::Effects::Templates::RingUv;

[[nodiscard]] constexpr std::uint32_t Point(std::int16_t x, std::int16_t y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

[[nodiscard]] fx::TrailPalettes TestPalettes() {
  fx::TrailPalettes palettes{};
  for (std::size_t index = 0; index < palettes.additive.size(); ++index) {
    const auto base = static_cast<std::uint8_t>(10 * (index + 1));
    palettes.additive.at(index) = {.r = base,
                                   .g = static_cast<std::uint8_t>(base + 1),
                                   .b = static_cast<std::uint8_t>(base + 2)};
  }
  for (std::size_t index = 0; index < palettes.distortion.size(); ++index) {
    const auto base = static_cast<std::uint8_t>(100 + index);
    palettes.distortion.at(index) = {.r = base,
                                     .g = static_cast<std::uint8_t>(base + 20),
                                     .b = static_cast<std::uint8_t>(base + 40)};
  }
  return palettes;
}

struct SubmittedQuad {
  RingQuad quad;
  std::uint8_t code{};
  std::int32_t depth{};
};

class TestSegmentHost final : public fx::TrailSegmentHost {
 public:
  fx::TrailProjection projection{};
  fx::TrailPalettes palettes = TestPalettes();
  std::uint32_t draw_buffer = 0;
  std::vector<std::array<fx::TrailPoint, fx::kTrailCorners>> projected;
  std::vector<SubmittedQuad> submitted;

  fx::TrailProjection Project(
      const std::array<fx::TrailPoint, fx::kTrailCorners>& corners) override {
    projected.push_back(corners);
    return projection;
  }
  const fx::TrailPalettes& Palettes() override { return palettes; }
  std::uint32_t DrawBuffer() override { return draw_buffer; }
  void Submit(const RingQuad& quad, std::uint8_t code,
              std::int32_t depth) override {
    submitted.push_back({.quad = quad, .code = code, .depth = depth});
  }
};

[[nodiscard]] fx::BoneTrail TrailOf(std::uint8_t color, std::uint8_t texture) {
  fx::BoneTrail trail{};
  trail.config = {.actor = 1,
                  .bone = 2,
                  .color = color,
                  .texture = texture,
                  .life = fx::kEndlessTrailLife};
  trail.previous = {.base = {.x = 1, .y = 2, .z = 3},
                    .tip = {.x = 4, .y = 5, .z = 6}};
  trail.current = {.base = {.x = 7, .y = 8, .z = 9},
                   .tip = {.x = 10, .y = 11, .z = 12}};
  return trail;
}

struct DrawnSegment {
  fx::TrailEdge previous;
  fx::TrailEdge current;
  fx::TrailSegmentWeights weights;
};

struct BoneSample {
  std::uint8_t actor{};
  std::uint8_t bone{};
  BoneEndpoint endpoint{};
};

// A renderer slot's host whose bones move 64 units along x per sample.
class TestTrailHost final : public fx::BoneTrailHost {
 public:
  std::optional<fx::BoneTrail> trail;
  bool room = true;
  std::int16_t samples = 0;
  std::vector<BoneSample> sampled;
  std::vector<DrawnSegment> drawn;
  std::vector<std::pair<RendererCommand, fx::BoneTrailRefusal>> refusals;
  int views = 0;
  int releases = 0;

  fx::BoneTrail* Trail() override { return trail ? &*trail : nullptr; }
  fx::BoneTrail* Allocate() override {
    if (!room) {
      return nullptr;
    }
    trail.emplace();
    return &*trail;
  }
  void Release() override {
    ++releases;
    trail.reset();
  }
  void SampleBone(std::uint8_t actor, std::uint8_t bone, BoneEndpoint endpoint,
                  fx::TrailPoint& point) override {
    sampled.push_back({.actor = actor, .bone = bone, .endpoint = endpoint});
    if (endpoint == BoneEndpoint::kBase) {
      ++samples;
    }
    point = {.x = static_cast<std::int16_t>(samples * 64),
             .y = static_cast<std::int16_t>(endpoint == BoneEndpoint::kTip ? 100
                                                                           : 0),
             .z = 7};
  }
  void LoadView() override { ++views; }
  void DrawSegment(const fx::BoneTrail& drawn_trail,
                   fx::TrailSegmentWeights weights) override {
    drawn.push_back({.previous = drawn_trail.previous,
                     .current = drawn_trail.current,
                     .weights = weights});
  }
  void Refuse(RendererCommand command, fx::BoneTrailRefusal refusal) override {
    refusals.emplace_back(command, refusal);
  }
};

[[nodiscard]] const fx::BoneTrail& ReadTrail(const TestTrailHost& host) {
  if (!host.trail.has_value()) {
    throw std::bad_optional_access();
  }
  return host.trail.value();
}

[[nodiscard]] bool Send(RendererCommand command, std::uint32_t parameter,
                        TestTrailHost& host) {
  return fx::UpdateBoneTrail(command, parameter, host);
}

// Ticks a started trail `count` times; the last tick's result.
bool Tick(TestTrailHost& host, int count) {
  bool alive = true;
  for (int tick = 0; tick < count; ++tick) {
    alive = Send(RendererCommand::kUpdate, 0, host);
  }
  return alive;
}

int other_renderer_calls = 0;

int OtherRenderer(Effects_RendererSlot* /*slot*/, u_int /*command*/,
                  int /*parameter*/) {
  ++other_renderer_calls;
  return 1;
}

// Secondary instances and slots in chain order: instance 0 runs slots 0 and
// 1, instance 1 runs slot 2.
struct TestChain {
  std::array<Effects_EffectInstance, 2> instances{};
  std::array<Effects_RendererSlot, 3> slots{};
  std::array<fx::BoneTrail, 3> trails{};

  TestChain() {
    instances[0].next = &instances[1];
    instances[0].context.renderer = &slots[0];
    instances[1].context.renderer = &slots[2];
    slots[0].next = &slots[1];
    for (std::size_t index = 0; index < slots.size(); ++index) {
      trails.at(index).config.life = fx::kEndlessTrailLife;
      slots.at(index).renderer = &fx::LiveUpdateBoneTrail;
      slots.at(index).rendererState = &trails.at(index);
    }
  }
};

class TestChains final : public fx::BoneTrailChains {
 public:
  TestChain chain;
  std::vector<Effects_RendererSlot*> faded;

  Effects_EffectInstance* FirstInstance() override {
    return &chain.instances[0];
  }
  Effects_EffectInstance* NextInstance(
      Effects_EffectInstance& instance) override {
    return instance.next;
  }
  Effects_RendererSlot* FirstRenderer(
      Effects_EffectInstance& instance) override {
    return instance.context.renderer;
  }
  Effects_RendererSlot* NextRenderer(Effects_RendererSlot& slot) override {
    return slot.next;
  }
  bool RunsBoneTrail(Effects_RendererSlot& slot) override {
    return slot.renderer == &fx::LiveUpdateBoneTrail;
  }
  void Fade(Effects_RendererSlot& slot) override {
    faded.push_back(&slot);
    // The fade may unlink the rest of the chain; the walk reads it after.
    slot.next = nullptr;
  }
};
}  // namespace

TEST_CASE("A trail configuration packs actor, bone, colour, texture and life",
          "[bone-trail][F01418]") {
  constexpr std::uint32_t kPacked = 0xFFA7'2C34;
  constexpr fx::BoneTrailConfig kConfig = fx::BoneTrailConfig::Unpack(kPacked);
  STATIC_CHECK(kConfig == fx::BoneTrailConfig{.actor = 0x34,
                                              .bone = 0x2C,
                                              .color = 7,
                                              .texture = 0xA,
                                              .life = 0xFF});
  STATIC_CHECK(kConfig.Pack() == kPacked);
}

TEST_CASE("Additive colours past the table read the distortion colours",
          "[bone-trail][F01417]") {
  const fx::TrailPalettes palettes = TestPalettes();
  using fx::TrailPalette;
  CHECK(fx::TrailPaletteColor(palettes, TrailPalette::kAdditive, 7) ==
        palettes.additive[7]);
  CHECK(fx::TrailPaletteColor(palettes, TrailPalette::kAdditive, 8) ==
        palettes.distortion[0]);
  CHECK(fx::TrailPaletteColor(palettes, TrailPalette::kAdditive, 14) ==
        palettes.distortion[6]);
  CHECK_FALSE(fx::TrailPaletteColor(palettes, TrailPalette::kAdditive, 15));
  CHECK(fx::TrailPaletteColor(palettes, TrailPalette::kDistortion, 6) ==
        palettes.distortion[6]);
  CHECK_FALSE(fx::TrailPaletteColor(palettes, TrailPalette::kDistortion, 7));
}

TEST_CASE("A segment is drawn only at average depths 17 to 2047",
          "[bone-trail][F01417]") {
  const fx::BoneTrail trail = TrailOf(1, 0);
  for (const auto& [depth, drawn] :
       std::to_array<std::pair<std::int32_t, bool>>(
           {{16, false}, {17, true}, {0x7FF, true}, {0x800, false}})) {
    TestSegmentHost host;
    host.projection.depth = depth;
    CHECK(fx::DrawTrailSegment(trail, {.base = 2, .tip = 1}, host) ==
          (drawn ? fx::TrailSegmentDraw::kDrawn
                 : fx::TrailSegmentDraw::kOutsideDepth));
    CHECK(host.submitted.size() == (drawn ? 1U : 0U));
  }
}

TEST_CASE("A segment projects the older and newer edges' base then tip",
          "[bone-trail][F01417]") {
  const fx::BoneTrail trail = TrailOf(1, 0);
  TestSegmentHost host;
  host.projection.depth = 100;
  static_cast<void>(fx::DrawTrailSegment(trail, {.base = 2, .tip = 1}, host));
  REQUIRE(host.projected.size() == 1);
  CHECK(host.projected[0] == std::array{trail.previous.base, trail.current.base,
                                        trail.previous.tip, trail.current.tip});
}

TEST_CASE("A textured segment shades its texture by the weights over 8",
          "[bone-trail][F01417]") {
  TestSegmentHost host;
  host.projection = {
      .screens = {Point(10, 20), Point(30, 40), Point(50, 60), Point(70, 80)},
      .depth = 300};
  const fx::BoneTrail trail = TrailOf(2, 0);
  REQUIRE(fx::DrawTrailSegment(trail, {.base = 5, .tip = 3}, host) ==
          fx::TrailSegmentDraw::kDrawn);
  REQUIRE(host.submitted.size() == 1);
  const SubmittedQuad& drawn = host.submitted[0];
  CHECK(drawn.code == fx::kTrailQuadCode);
  CHECK(drawn.depth == 300);
  CHECK(drawn.quad.screens == host.projection.screens);
  CHECK(drawn.quad.clut == 0x3C30);
  CHECK(drawn.quad.tpage == 0x38);
  // Colour 2 is (30, 31, 32): the base corners take 5/8, the tips 3/8.
  const RingColor base{.r = 18, .g = 19, .b = 20};
  const RingColor tip{.r = 11, .g = 11, .b = 12};
  CHECK(drawn.quad.colors == std::array{base, base, tip, tip});
  CHECK(drawn.quad.uvs == std::array<RingUv, 4>{{{.u = 0x68, .v = 0x48},
                                                 {.u = 0x68, .v = 0x4F},
                                                 {.u = 0x87, .v = 0x48},
                                                 {.u = 0x87, .v = 0x4F}}});
}

TEST_CASE("Texture cells carry their u into v and colour 7 subtracts",
          "[bone-trail][F01417]") {
  TestSegmentHost host;
  host.projection.depth = 300;
  // Texture 14: row 2, column 4, at 0x4868 + 0x1400 + 160 = 0x5D08.
  REQUIRE(fx::DrawTrailSegment(TrailOf(7, 14), {.base = 1, .tip = 0}, host) ==
          fx::TrailSegmentDraw::kDrawn);
  REQUIRE(host.submitted.size() == 1);
  CHECK(host.submitted[0].quad.tpage == 0x58);
  CHECK(host.submitted[0].quad.uvs ==
        std::array<RingUv, 4>{{{.u = 0x08, .v = 0x5D},
                               {.u = 0x08, .v = 0x64},
                               {.u = 0x27, .v = 0x5D},
                               {.u = 0x27, .v = 0x64}}});
  // Additive colour 9 reads distortion colour 1.
  host.submitted.clear();
  REQUIRE(fx::DrawTrailSegment(TrailOf(9, 0), {.base = 8, .tip = 8}, host) ==
          fx::TrailSegmentDraw::kDrawn);
  CHECK(host.submitted.at(0).quad.colors[0] ==
        RingColor{.r = 101, .g = 121, .b = 141});
}

TEST_CASE("A distortion segment maps the frame on display behind it",
          "[bone-trail][F01417][F01452]") {
  TestSegmentHost host;
  host.projection = {.screens = {Point(130, 20), Point(70, 40), Point(200, 250),
                                 Point(90, -4)},
                     .depth = 64};
  host.draw_buffer = 0;
  const fx::BoneTrail trail = TrailOf(1, fx::kDistortionTrailTexture);
  REQUIRE(fx::DrawTrailSegment(trail, {.base = 2, .tip = 1}, host) ==
          fx::TrailSegmentDraw::kDrawn);
  REQUIRE(host.submitted.size() == 1);
  const RingQuad& quad = host.submitted[0].quad;
  CHECK(host.submitted[0].code == fx::kDistortionQuadCode);
  // Distortion colour 1 is (101, 121, 141): times the weight, less 128,
  // wrapping in a byte.
  const RingColor base{.r = 74, .g = 114, .b = 154};
  const RingColor tip{.r = 229, .g = 249, .b = 13};
  CHECK(quad.colors == std::array{base, base, tip, tip});
  // The leftmost corner (70) is in page 1; buffer 0 draws, so buffer 1's
  // page 6 is on display. u and v move by the base weight, less the page.
  CHECK(quad.tpage == 0x106);
  CHECK(quad.uvs == std::array<RingUv, 4>{{{.u = 68, .v = 22},
                                           {.u = 8, .v = 42},
                                           {.u = 138, .v = 252},
                                           {.u = 28, .v = 1}}});
  CHECK(quad.clut == 0);
}

TEST_CASE("Unknown colours draw nothing", "[bone-trail][F01417]") {
  TestSegmentHost host;
  host.projection.depth = 100;
  CHECK(fx::DrawTrailSegment(TrailOf(15, 0), {.base = 1, .tip = 1}, host) ==
        fx::TrailSegmentDraw::kUnknownColor);
  CHECK(fx::DrawTrailSegment(TrailOf(7, fx::kDistortionTrailTexture),
                             {.base = 1, .tip = 1},
                             host) == fx::TrailSegmentDraw::kUnknownColor);
  CHECK(host.submitted.empty());
}

TEST_CASE("A texel is clamped to 1..254 on each axis", "[bone-trail][F01451]") {
  CHECK(fx::ClampTrailTexturePoint(3, -2, Point(100, 50)) ==
        RingUv{.u = 103, .v = 48});
  CHECK(fx::ClampTrailTexturePoint(0, 0, Point(-5, 300)) ==
        RingUv{.u = 1, .v = 254});
  CHECK(fx::ClampTrailTexturePoint(-100, 10, Point(100, -20)) ==
        RingUv{.u = 1, .v = 1});

  u_short uv = 0xFFFF;
  fx::LiveClampTrailTexturePoint(3, -2, static_cast<int>(Point(100, 50)), &uv);
  CHECK(uv == (103 | (48 << 8)));
  fx::LiveClampTrailTexturePoint(0, 0, 0, nullptr);
}

TEST_CASE("The framebuffer page holds the leftmost corner up to x 320",
          "[bone-trail][F01452]") {
  RingQuad quad{
      .screens = {Point(400, 0), Point(500, 0), Point(330, 0), Point(900, 0)}};
  fx::MapTrailFramebufferTexture(quad, 0, 0, 0x20, 1);
  CHECK(quad.tpage == (5 | 0x20 | 0x100));
  fx::MapTrailFramebufferTexture(quad, 0, 0, 0x20, 0);
  CHECK(quad.tpage == (10 | 0x20 | 0x100));
  // A corner left of the screen selects page -1, as the original did.
  quad.screens[1] = Point(-10, 0);
  fx::MapTrailFramebufferTexture(quad, 0, 0, 0, 1);
  CHECK(quad.tpage == 0xFFFF);
  CHECK(quad.uvs[1] == RingUv{.u = 54, .v = 1});
  fx::MapTrailFramebufferTexture(quad, 0, 0, 0, 0);
  CHECK(quad.tpage == 0x104);
}

TEST_CASE("The live map writes only the packet's page and texels",
          "[bone-trail][F01452]") {
  const int saved_buffer = VS::Render::frameBuf;
  VS::Render::TrailPacket packet{};
  packet.xy0 = static_cast<std::int32_t>(Point(130, 20));
  packet.xy1 = static_cast<std::int32_t>(Point(70, 40));
  packet.xy2 = static_cast<std::int32_t>(Point(200, 250));
  packet.xy3 = static_cast<std::int32_t>(Point(90, -4));
  packet.palette = 0x1234;
  packet.reserved2a = 0x5678;
  VS::Render::frameBuf = 3;
  fx::LiveMapTrailFramebufferTexture(&packet, 2, 2, 0x40);
  VS::Render::frameBuf = saved_buffer;
  CHECK(packet.texture_page == (1 | 0x40 | 0x100));
  CHECK(packet.uv0 == (68 | (22 << 8)));
  CHECK(packet.uv1 == (8 | (42 << 8)));
  CHECK(packet.uv2 == (138 | (252 << 8)));
  CHECK(packet.uv3 == (28 | (1 << 8)));
  CHECK(packet.palette == 0x1234);
  CHECK(packet.reserved2a == 0x5678);
  fx::LiveMapTrailFramebufferTexture(nullptr, 0, 0, 0);
}

TEST_CASE("A started trail unpacks its parameter and a full heap refuses",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  CHECK(Send(RendererCommand::kStart, 0x0812'0503, host));
  REQUIRE(host.trail);
  const fx::BoneTrail& trail = ReadTrail(host);
  CHECK(trail.config ==
        fx::BoneTrailConfig{
            .actor = 3, .bone = 5, .color = 2, .texture = 1, .life = 8});
  CHECK(trail.newest == 0);
  CHECK(trail.age == 0);

  TestTrailHost full;
  full.room = false;
  CHECK_FALSE(Send(RendererCommand::kStart, 0, full));
  CHECK(full.refusals == std::vector{std::pair{RendererCommand::kStart,
                                               fx::BoneTrailRefusal::kNoRoom}});
}

TEST_CASE("Unknown commands keep the trail and a stop frees it",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  // The fixed uint32_t underlying type permits unknown renderer commands.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(Send(static_cast<RendererCommand>(0), 0, host));
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(Send(static_cast<RendererCommand>(5), 0, host));
  CHECK(host.trail);
  CHECK_FALSE(Send(RendererCommand::kStop, 0, host));
  CHECK_FALSE(host.trail);
  CHECK(host.releases == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE("Ticks and fades without a trail are refused",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  CHECK_FALSE(Send(RendererCommand::kUpdate, 0, host));
  CHECK_FALSE(Send(RendererCommand::kFade, 0, host));
  CHECK(host.refusals ==
        std::vector{
            std::pair{RendererCommand::kUpdate, fx::BoneTrailRefusal::kNoTrail},
            std::pair{RendererCommand::kFade, fx::BoneTrailRefusal::kNoTrail}});
  CHECK(host.sampled.empty());
}

TEST_CASE("A tick samples the bone's base then tip", "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0904, host));
  CHECK(Tick(host, 1));
  REQUIRE(host.sampled.size() == 2);
  CHECK(host.sampled[0].actor == 4);
  CHECK(host.sampled[0].bone == 9);
  CHECK(host.sampled[0].endpoint == BoneEndpoint::kBase);
  CHECK(host.sampled[1].endpoint == BoneEndpoint::kTip);
}

TEST_CASE("Bone samples wrap the eight-entry trail history",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0904, host));
  CHECK(Tick(host, 9));
  REQUIRE(host.trail);
  const fx::BoneTrail& trail = ReadTrail(host);
  CHECK(trail.newest == 1);
  CHECK(trail.age == 9);
  CHECK(host.sampled.size() == 18);
  // The ninth sample replaced the first.
  CHECK(trail.history[1].base.x == 9 * 64);
  CHECK(trail.history[1].tip.y == 100);
  CHECK(trail.history[0].base.x == 8 * 64);
}

TEST_CASE("Trails wait for four samples before drawing",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  REQUIRE(Tick(host, 3));
  CHECK(host.drawn.empty());
  CHECK(host.views == 0);
}

TEST_CASE("The fourth trail sample draws three fading segments",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  REQUIRE(Tick(host, 3));
  REQUIRE(Tick(host, 1));
  CHECK(host.views == 1);
  REQUIRE(host.drawn.size() == 3);
  CHECK(host.drawn[0].weights == fx::TrailSegmentWeights{.base = 2, .tip = 1});
  CHECK(host.drawn[1].weights == fx::TrailSegmentWeights{.base = 1, .tip = 1});
  CHECK(host.drawn[2].weights == fx::TrailSegmentWeights{.base = 1, .tip = 0});
}

TEST_CASE("Trail segments join the Q12 Lagrange half samples",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  REQUIRE(Tick(host, 4));
  REQUIRE(host.drawn.size() == 3);
  // Samples run 64 units apart: the first segment runs from the newest
  // (sample 4) to halfway to the one before, then by half samples. The Q12
  // Lagrange weights at the newest and half-span phases round down one unit.
  CHECK(host.drawn[0].previous.base.x == (4 * 64) - 1);
  CHECK(host.drawn[0].current.base.x == (4 * 64) - 32 - 1);
  CHECK(host.drawn[1].current.base.x == 3 * 64);
  CHECK(host.drawn[2].current.base.x == (3 * 64) - 32);
  CHECK(host.drawn[2].current.tip.y == 100);
  for (std::size_t segment = 1; segment < host.drawn.size(); ++segment) {
    CHECK(host.drawn[segment].previous == host.drawn[segment - 1].current);
  }
}

TEST_CASE("Trails draw at most five history spans", "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  REQUIRE(Tick(host, 4));
  for (const std::size_t expected : {5U, 7U, 9U, 11U, 11U}) {
    host.drawn.clear();
    REQUIRE(Tick(host, 1));
    REQUIRE(host.drawn.size() == expected);
  }
  CHECK(host.drawn.front().weights ==
        fx::TrailSegmentWeights{.base = 6, .tip = 5});
  CHECK(host.drawn.back().weights ==
        fx::TrailSegmentWeights{.base = 1, .tip = 0});
}

TEST_CASE("An expired trail wraps from zero to endless if ticked again",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0x0200'0000, host));
  REQUIRE(host.trail);
  const fx::BoneTrail& trail = ReadTrail(host);
  CHECK(Tick(host, 1));
  CHECK_FALSE(Tick(host, 1));
  CHECK(trail.config.life == 0);
  // The renderer would be detached; another tick wraps to 255, as before.
  CHECK(Tick(host, 1));
  CHECK(trail.config.life == 0xFF);
  CHECK(Tick(host, 3));
  CHECK(trail.config.life == 0xFF);
}

TEST_CASE("A fade gives an endless trail eight more ticks",
          "[bone-trail][F01418]") {
  TestTrailHost host;
  REQUIRE(Send(RendererCommand::kStart, 0xFF00'0000, host));
  REQUIRE(host.trail);
  const fx::BoneTrail& trail = ReadTrail(host);
  CHECK(Send(RendererCommand::kFade, 0, host));
  CHECK(trail.config.life == fx::kFadingTrailLife);
  CHECK(Tick(host, 7));
  CHECK_FALSE(Tick(host, 1));
  CHECK(trail.config.life == 0);
}

TEST_CASE("Trail requests pack an endless effect 54 at the origin",
          "[bone-trail][F01447][F00056]") {
  CHECK(fx::MakeBoneTrailRequest(0x1234, 0x1FF, 0x17, 0x2A) ==
        fx::BoneTrailRequest{.effect_id = 54,
                             .target_count = 1,
                             .other_count = 0,
                             .source_type = 5,
                             .target_type = 5,
                             .parameter = 0xFFA7'FF34});
  CHECK(fx::MakeWeaponTrailRequest(-1, 0x102) ==
        fx::BoneTrailRequest{.effect_id = 54,
                             .target_count = 1,
                             .other_count = 0,
                             .source_type = 5,
                             .target_type = 5,
                             .parameter = 0xFF00'02FF});
}

TEST_CASE("A fade walks every instance's renderers after each fade",
          "[bone-trail][F01448][F00057]") {
  TestChains chains;
  chains.chain.slots[1].renderer = &OtherRenderer;
  fx::FadeBoneTrails(chains);
  // Slot 0's fade unlinked slot 1; instance 1 still followed.
  CHECK(chains.faded ==
        std::vector{&chains.chain.slots[0], &chains.chain.slots[2]});
  chains.faded.clear();
  chains.chain.slots[0].renderer = &OtherRenderer;
  fx::FadeWeaponTrails(chains);
  CHECK(chains.faded == std::vector{&chains.chain.slots[2]});
}

TEST_CASE("The live fade commands only the secondary bone trails",
          "[bone-trail][F01448][F00057][F01418]") {
  TestChain chain;
  chain.slots[1].renderer = &OtherRenderer;
  Effects_EffectInstance* const saved = Effects_secondaryInstances;
  Effects_secondaryInstances = chain.instances.data();
  other_renderer_calls = 0;
  fx::LiveFadeBoneTrails();
  CHECK(chain.trails[0].config.life == fx::kFadingTrailLife);
  CHECK(chain.trails[1].config.life == fx::kEndlessTrailLife);
  CHECK(chain.trails[2].config.life == fx::kFadingTrailLife);
  chain.trails[2].config.life = 1;
  fx::LiveFadeWeaponTrails();
  CHECK(chain.trails[2].config.life == fx::kFadingTrailLife);
  Effects_secondaryInstances = saved;
  CHECK(other_renderer_calls == 0);
}

TEST_CASE("The live renderer refuses a missing slot or trail",
          "[bone-trail][F01418]") {
  CHECK(fx::LiveUpdateBoneTrail(nullptr, 2, 0) == 0);
  Effects_RendererSlot slot{};
  slot.renderer = &fx::LiveUpdateBoneTrail;
  CHECK(fx::LiveUpdateBoneTrail(&slot, 2, 0) == 0);
  CHECK(fx::LiveUpdateBoneTrail(&slot, 3, 0) == 0);
  CHECK(fx::LiveUpdateBoneTrail(&slot, 0, 0) == 1);
  CHECK(fx::LiveUpdateBoneTrail(&slot, 4, 0) == 0);
  CHECK(slot.rendererState == nullptr);
  fx::LiveDrawTrailSegment(nullptr, 1, 1);
}
